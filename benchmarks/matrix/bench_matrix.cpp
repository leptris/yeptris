// benchmarks/matrix/bench_matrix.cpp — the performance matrix (TODO.impl/18).
//
// Deterministic corpora (seeded), same-process reference runs: every
// number is a RATIO against libyaml on THIS machine (machine-relative
// reporting — the leptris convention; never absolute cross-machine
// claims). Output: JSON + Markdown.
//
// Usage: bench_matrix [out-dir] [--quick|--full] [--seed N]
#include <dirent.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <yeptris.h>

#if defined(YEP_BENCH_LIBYAML)
#include <yaml.h>
#endif

namespace {

typedef std::chrono::steady_clock clk;

struct Corpus {
    std::string name;
    std::string data;
};

/* ---- 18B measures: allocations/parse + memory/input ratio --------
 * Via the engine+DOM pair with a counting allocator (the public parse
 * entry hard-wires the system allocator; the pair below is the same
 * path minus the encoding front-end, which allocates nothing on the
 * borrow path). */
extern "C" {
#include "dom/dom.h"
#include "memory/allocator.h"
#include "parse/engine.h"
}

typedef struct {
    size_t allocs;
    size_t bytes; /* cumulative allocation volume (churn) */
    size_t live;  /* outstanding bytes right now */
    size_t peak;  /* high-water of live */
} alloc_count;

/* Size-prefixed allocations: free() reads the size back to keep the
 * live-bytes ledger exact — deterministic peak, no RSS guesswork. */
static void* count_alloc(void* ctx, size_t size) {
    alloc_count* c = (alloc_count*)ctx;
    c->allocs++;
    c->bytes += size;
    c->live += size;
    if (c->live > c->peak) {
        c->peak = c->live;
    }
    size_t* p = (size_t*)malloc(size + sizeof(size_t));
    if (p != NULL) {
        *p = size;
    }
    return p != NULL ? (void*)(p + 1) : NULL;
}

static void count_free(void* ctx, void* ptr) {
    alloc_count* c = (alloc_count*)ctx;
    if (ptr != NULL) {
        size_t* p = (size_t*)ptr - 1;
        c->live -= *p;
        free(p);
    }
}

typedef struct {
    double allocs_per_mb;
    double churn_ratio;    /* cumulative allocation bytes / input bytes */
    double peak_rss_ratio; /* child-process peak RSS / input bytes */
} mem_stats;

static mem_stats measure_mem(const Corpus& c) {
    alloc_count cnt = {0, 0};
    yep_allocator counter = {count_alloc, count_free, &cnt};
    yep_engine* eng = yep_engine_create(&counter);
    yep_dom* dom = yep_dom_create(&counter);
    mem_stats ms = {0, 0, 0};
    if (eng != NULL && dom != NULL) {
        /* the same sizing seam as parse_impl: the measure must
         * exercise the product's reserve path, not a bare engine */
        yep_dom_prepare(dom, c.data.data(), c.data.size());
        yep_sink sink = {yep_dom_on_event, dom};
        int rc = yep_engine_run(eng, c.data.data(), c.data.size(), &sink);
        if (rc == 0 && dom->ncount > 0) {
            ms.allocs_per_mb = (double)cnt.allocs / ((double)c.data.size() / 1e6);
            ms.churn_ratio = (double)cnt.bytes / (double)c.data.size();
            ms.peak_rss_ratio = (double)cnt.peak / (double)c.data.size();
        }
    }
    yep_dom_destroy(dom);
    yep_engine_destroy(eng);
    return ms;
}

std::string fmt(double v) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return buf;
}

/* ---- deterministic corpus generators (seeded) ---- */

struct Rng {
    unsigned long s;
    explicit Rng(unsigned long seed) : s(seed ? seed : 1) {}
    unsigned long next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    unsigned long pick(unsigned long n) {
        return next() % (n ? n : 1);
    }
};

const char* k_words[] = {"alpha",  "beta",    "gamma",  "delta",   "epsilon", "zeta",   "eta",
                         "theta",  "iota",    "kappa",  "value",   "item",    "record", "node",
                         "entry",  "118-222", "0x1A2B", "3.14159", "true",    "null",   "name",
                         "status", "level",   "count",  "ratio"};
const int k_nwords = 25;

std::string word(Rng& r) {
    return k_words[r.pick(k_nwords)];
}

std::string sentence(Rng& r, int words) {
    std::string s = word(r);
    for (int i = 1; i < words; i++) {
        s += " ";
        s += word(r);
    }
    return s;
}

void gen_block(std::string* out, Rng& r, int entries) {
    for (int i = 0; i < entries; i++) {
        out->append(word(r) + "_" + std::to_string(i % 977) + ":\n");
        out->append("  id: " + std::to_string(i) + "\n");
        out->append("  name: " + sentence(r, 3) + "\n");
        out->append("  score: " + fmt(r.next() % 100000 / 100.0) + "\n");
        out->append("  active: " + std::string(r.pick(2) ? "true" : "false") + "\n");
        out->append("  tags:\n    - " + word(r) + "\n    - " + word(r) + "\n");
        if (r.pick(3) == 0) {
            out->append("  meta:\n    origin: " + word(r) +
                        "\n    weight: " + std::to_string(r.pick(100)) + "\n");
        }
    }
}

void gen_flow(std::string* out, Rng& r, int entries) {
    for (int i = 0; i < entries; i++) {
        out->append("- { \"id\": " + std::to_string(i) + ", \"name\": \"" + word(r) +
                    "\", \"vals\": [" + std::to_string(r.pick(1000)) + ", " +
                    std::to_string(r.pick(1000)) + ", " + std::to_string(r.pick(1000)) +
                    "], \"ok\": " + std::string(r.pick(2) ? "true" : "false") + " }\n");
    }
}

void gen_scalar(std::string* out, Rng& r, int entries) {
    for (int i = 0; i < entries; i++) {
        out->append("k" + std::to_string(i) + ": " + sentence(r, 12) + "\n");
        if (r.pick(4) == 0) {
            out->append("q" + std::to_string(i) + ": \"" + sentence(r, 8) + "\"\n");
        }
        if (r.pick(8) == 0) {
            out->append("b" + std::to_string(i) + ": |\n  " + sentence(r, 4) + "\n  " +
                        sentence(r, 4) + "\n");
        }
    }
}

void gen_anchor(std::string* out, Rng& r, int entries) {
    out->append("defaults: &def" + std::to_string(0) + "\n  a: 1\n  b: 2\n  c: 3\n");
    for (int i = 0; i < entries; i++) {
        out->append("item" + std::to_string(i) + ":\n  <<: *def" + std::to_string(0) + "\n");
        out->append("  x: &x" + std::to_string(i) + " " + word(r) + "\n");
        if (r.pick(3) == 0) {
            out->append("  y: *x" + std::to_string(i) + "\n");
        }
    }
}

void gen_deep(std::string* out, Rng& r, int depth, int branches) {
    for (int b = 0; b < branches; b++) {
        for (int d = 0; d < depth; d++) {
            out->append(std::string(d * 2 + 2, ' ') + "l" + std::to_string(d) + ":\n");
        }
        out->append(std::string(depth * 2 + 4, ' ') + "leaf: " + word(r) + "\n");
    }
}

void gen_wide(std::string* out, Rng& r, int keys) {
    for (int i = 0; i < keys; i++) {
        out->append("key_" + std::to_string(i) + ": " + std::to_string(r.pick(1 << 20)) + "\n");
    }
}

/* Classifies a body's opening: leading %directive lines (which must
 * precede any --- marker) and whether the body carries its own marker
 * after them. */
static void rw_opening(const std::string& body, bool* directive_led, bool* has_marker) {
    *directive_led = false;
    *has_marker = false;
    size_t i = 0;
    int seen_directive = 0;
    while (i < body.size()) {
        size_t eol = body.find('\n', i);
        if (eol == std::string::npos) {
            eol = body.size();
        }
        size_t b = body.find_first_not_of(" \t", i);
        if (b == std::string::npos || b >= eol) {
            i = eol + 1; /* blank line */
            continue;
        }
        if (body[b] == '%') {
            seen_directive = 1;
            *directive_led = true;
        } else if (eol - b >= 3 && body.compare(b, 3, "---") == 0 &&
                   (b + 3 == eol || body[b + 3] == ' ' || body[b + 3] == '\t')) {
            /* "---word1" is a plain scalar, not a marker */
            *has_marker = true;
            return;
        } else {
            return; /* content: nothing later can change the opening */
        }
        i = eol + 1;
    }
    (void)seen_directive;
}

#if defined(YEP_BENCH_LIBYAML)
/* The race needs common ground: keep only snippets the reference also
 * parses (yeptris-only conformance wins like 2JQS stay in the
 * conformance suite, where the comparison belongs). */
static bool rw_libyaml_ok(const std::string& doc) {
    yaml_parser_t p;
    yaml_event_t ev;
    if (!yaml_parser_initialize(&p)) {
        return false;
    }
    yaml_parser_set_input_string(&p, (const unsigned char*)doc.data(), doc.size());
    int done = 0, ok = 1;
    while (!done) {
        if (!yaml_parser_parse(&p, &ev)) {
            ok = 0;
            break;
        }
        done = (ev.type == YAML_STREAM_END_EVENT);
        yaml_event_delete(&ev);
    }
    yaml_parser_delete(&p);
    return ok != 0;
}
#endif

/* Real-world: the committed differential snapshots (deterministic).
 * Each snapshot becomes its own document: error-case fixtures (which
 * fail to parse by design) are dropped by probing the framed snippet. */
void gen_realworld(std::string* out, const std::string& snapshot_dir) {
    std::vector<std::string> files;
    DIR* d = opendir(snapshot_dir.c_str());
    if (d == NULL) {
        return;
    }
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        size_t n = strlen(ent->d_name);
        if (n > 3 && strcmp(ent->d_name + n - 3, ".in") == 0) {
            files.push_back(snapshot_dir + "/" + ent->d_name);
        }
    }
    closedir(d);
    std::sort(files.begin(), files.end());
    size_t budget = 4u << 20;
    for (const std::string& f : files) {
        std::string body;
        FILE* fp = fopen(f.c_str(), "rb");
        if (fp == NULL) {
            continue;
        }
        char buf[65536];
        size_t got;
        while ((got = fread(buf, 1, sizeof(buf), fp)) > 0) {
            body.append(buf, got);
        }
        fclose(fp);
        bool directive_led, has_marker;
        rw_opening(body, &directive_led, &has_marker);
        std::string doc;
        if (directive_led) {
            /* directives belong to their own document: terminate the
             * previous one (repeated ... markers are legal and skipped),
             * then guarantee a --- after the directive block */
            if (!out->empty()) {
                doc += "...\n";
            }
            doc += body;
            if (!has_marker) {
                doc += "---\n";
            }
        } else if (has_marker) {
            doc = body;
        } else {
            doc = "---\n" + body;
        }
        YeptrisStatus st = YEPTRIS_OK;
        YeptrisDocument probe = yeptris_parse(doc.data(), doc.size(), &st);
        int keep = (probe != NULL);
        yeptris_document_free(probe);
#if defined(YEP_BENCH_LIBYAML)
        if (keep) {
            keep = rw_libyaml_ok(doc);
        }
#endif
        if (!keep) {
            continue;
        }
        if (out->size() + doc.size() + 1 > budget) {
            break;
        }
        *out += doc;
        out->push_back('\n');
    }
}

/* ---- measurement ---- */

double ms_of(clk::time_point a, clk::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

struct Result {
    std::string name;
    double mb_s = 0; /* 0 = unsupported/failed */
    double ms = 0;
    size_t bytes = 0;
    Result() = default;
    Result(std::string n, double m, double milliseconds, size_t b)
        : name(std::move(n)), mb_s(m), ms(milliseconds), bytes(b) {}
};

Result bench_dom(const Corpus& c, int iters) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument probe = yeptris_parse(c.data.data(), c.data.size(), &st);
    if (probe == NULL && st != YEPTRIS_OK) {
        yeptris_document_free(probe);
        return {c.name + " (DOM)", 0, 0, c.data.size()}; /* parse fails */
    }
    yeptris_document_free(probe);
    double best_ms = 1e9;
    for (int i = 0; i < iters; i++) {
        auto t0 = clk::now();
        YeptrisDocument d = yeptris_parse(c.data.data(), c.data.size(), &st);
        auto t1 = clk::now();
        yeptris_document_free(d);
        double m = ms_of(t0, t1);
        if (m < best_ms) {
            best_ms = m;
        }
    }
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    return {c.name + " (DOM)", best_ms > 0 ? mb * 1000.0 / best_ms : 0, best_ms, c.data.size()};
}

Result bench_pull(const Corpus& c, int iters) {
    YeptrisStatus probe_st = YEPTRIS_OK;
    YeptrisDocument probe = yeptris_parse(c.data.data(), c.data.size(), &probe_st);
    int parses = !(probe == NULL && probe_st != YEPTRIS_OK);
    yeptris_document_free(probe);
    if (!parses) {
        return {c.name + " (pull)", 0, 0, c.data.size()};
    }
    double best_ms = 1e9;
    for (int i = 0; i < iters; i++) {
        auto t0 = clk::now();
        YeptrisPullParser p = yeptris_pull_new(c.data.data(), c.data.size());
        const YeptrisEvent* e;
        while ((e = yeptris_pull_next(p)) != NULL) {}
        auto t1 = clk::now();
        yeptris_pull_free(p);
        double m = ms_of(t0, t1);
        if (m < best_ms) {
            best_ms = m;
        }
    }
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    return {c.name + " (pull)", best_ms > 0 ? mb * 1000.0 / best_ms : 0, best_ms, c.data.size()};
}

Result bench_recorder(const Corpus& c, int iters) {
    YeptrisStatus probe_st = YEPTRIS_OK;
    YeptrisDocument probe = yeptris_parse(c.data.data(), c.data.size(), &probe_st);
    int parses = !(probe == NULL && probe_st != YEPTRIS_OK);
    yeptris_document_free(probe);
    if (!parses) {
        return {c.name + " (recorder)", 0, 0, c.data.size()};
    }
    double best_ms = 1e9;
    for (int i = 0; i < iters; i++) {
        auto t0 = clk::now();
        YeptrisRecorder r = yeptris_recorder_new();
        yeptris_recorder_feed(r, c.data.data(), c.data.size(), 1);
        size_t n = 0;
        yeptris_recorder_records(r, &n);
        auto t1 = clk::now();
        yeptris_recorder_free(r);
        double m = ms_of(t0, t1);
        if (m < best_ms) {
            best_ms = m;
        }
    }
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    return {c.name + " (recorder)", best_ms > 0 ? mb * 1000.0 / best_ms : 0, best_ms,
            c.data.size()};
}

Result bench_emit(const Corpus& c, int iters) {
    YeptrisStatus st;
    YeptrisDocument d = yeptris_parse(c.data.data(), c.data.size(), &st);
    if (d == NULL) {
        return {c.name + " (emit)", 0, 0, c.data.size()}; /* parse fails */
    }
    size_t len = 0;
    char* s = yeptris_serialize(d, &len);
    double best_ms = 1e9;
    for (int i = 0; i < iters; i++) {
        auto t0 = clk::now();
        char* s2 = yeptris_serialize(d, &len);
        auto t1 = clk::now();
        free(s2);
        double m = ms_of(t0, t1);
        if (m < best_ms) {
            best_ms = m;
        }
    }
    free(s);
    yeptris_document_free(d);
    double mb = (double)len / (1024.0 * 1024.0);
    return {c.name + " (emit)", best_ms > 0 ? mb * 1000.0 / best_ms : 0, best_ms, len};
}

#if defined(YEP_BENCH_LIBYAML)
Result bench_libyaml(const Corpus& c, int iters) {
    double best_ms = 1e9;
    for (int i = 0; i < iters; i++) {
        auto t0 = clk::now();
        yaml_parser_t p;
        yaml_event_t ev;
        if (yaml_parser_initialize(&p)) {
            yaml_parser_set_input_string(&p, (const unsigned char*)c.data.data(), c.data.size());
            int done = 0;
            int bad = 0;
            while (!done) {
                if (!yaml_parser_parse(&p, &ev)) {
                    bad = 1;
                    break;
                }
                done = (ev.type == YAML_STREAM_END_EVENT);
                yaml_event_delete(&ev);
            }
            yaml_parser_delete(&p);
            if (bad) {
                return {c.name + " (libyaml)", 0, 0, c.data.size()};
            }
        }
        auto t1 = clk::now();
        double m = ms_of(t0, t1);
        if (m < best_ms) {
            best_ms = m;
        }
    }
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    return {c.name + " (libyaml)", best_ms > 0 ? mb * 1000.0 / best_ms : 0, best_ms, c.data.size()};
}
#endif

} // namespace

int main(int argc, char** argv) {
    const char* out_dir = "bench-out";
    int full = 0;
    unsigned long seed = 0xC0FFEE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--full") == 0) {
            full = 1;
        } else if (strcmp(argv[i], "--quick") == 0) {
            full = 0;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (unsigned long)strtoul(argv[++i], NULL, 0);
        } else {
            out_dir = argv[i];
        }
    }
    int entries = full ? 200000 : 40000; /* ~2–8 MB per shape */
    Rng r(seed);

    std::vector<Corpus> corpora;
    {
        std::string s;
        gen_block(&s, r, entries);
        corpora.push_back({"block-heavy", s});
    }
    {
        std::string s;
        gen_flow(&s, r, entries);
        corpora.push_back({"flow-json", s});
    }
    {
        std::string s;
        gen_scalar(&s, r, entries);
        corpora.push_back({"scalar-heavy", s});
    }
    {
        std::string s;
        gen_anchor(&s, r, entries);
        corpora.push_back({"anchor-heavy", s});
    }
    {
        std::string s;
        gen_deep(&s, r, full ? 80 : 40, entries / 200 + 1);
        corpora.push_back({"deep-nesting", s});
    }
    {
        std::string s;
        gen_wide(&s, r, full ? 1000000 : 200000);
        corpora.push_back({"wide-mapping", s});
    }
    {
        std::string s;
        gen_realworld(&s, YEP_BENCH_SNAPSHOT_DIR);
        if (!s.empty()) {
            corpora.push_back({"realworld-suite", s});
        }
    }

    /* YEP_BENCH_DUMP_DIR: persist corpora so any ledger number is
     * reproducible byte-for-byte. */
    if (const char* dump = getenv("YEP_BENCH_DUMP_DIR")) {
        for (const Corpus& c : corpora) {
            std::string path = std::string(dump) + "/" + c.name + ".yaml";
            FILE* fp = fopen(path.c_str(), "wb");
            if (fp != NULL) {
                fwrite(c.data.data(), 1, c.data.size(), fp);
                fclose(fp);
            }
        }
    }

    printf("18B measures (parse path)\n\n");
    printf("| shape | allocs/MB | alloc churn/input | peak heap/input |\n|---|---|---|---|\n");
    for (const Corpus& c : corpora) {
        mem_stats ms = measure_mem(c);
        printf("| %s | %.0f | %.2fx | %.2fx |\n", c.name.c_str(), ms.allocs_per_mb, ms.churn_ratio,
               ms.peak_rss_ratio);
    }
    printf("\n");

    std::vector<Result> results;
    for (const Corpus& c : corpora) {
        int iters = full ? 5 : 3;
        results.push_back(bench_dom(c, iters));
        results.push_back(bench_pull(c, iters));
        results.push_back(bench_recorder(c, iters));
        results.push_back(bench_emit(c, iters));
#if defined(YEP_BENCH_LIBYAML)
        results.push_back(bench_libyaml(c, iters));
#endif
    }

    /* Markdown + JSON */
    std::string md = "# yeptris benchmark matrix\n\nMachine-relative: MB/s on this "
                     "run; ratio vs the same-run libyaml parse.\n\n"
                     "| shape | measure | MB/s | ms | vs libyaml |\n|---|---|---|---|---|\n";
    std::string js = "[\n";
    for (const Result& res : results) {
        double ref = 0;
#if defined(YEP_BENCH_LIBYAML)
        for (const Result& x : results) {
            if (x.name == res.name.substr(0, res.name.find(" (")) + " (libyaml)") {
                ref = x.mb_s;
                break;
            }
        }
#endif
        char ratio[32] = "n/a";
        if (ref > 0 && res.mb_s > 0) {
            snprintf(ratio, sizeof(ratio), "%.2fx", res.mb_s / ref);
        }
        md += "| " + res.name.substr(0, res.name.find(" (")) + " | " +
              res.name.substr(res.name.find("(") + 1, res.name.size() - res.name.find("(") - 2) +
              " | " + (res.mb_s > 0 ? fmt(res.mb_s) : "n/a") + " | " +
              (res.ms > 0 && res.ms < 1e8 ? fmt(res.ms) : "n/a") + " | " + ratio + " |\n";
        js += "  {\"name\": \"" + res.name +
              "\", \"mb_s\": " + (res.mb_s > 0 ? fmt(res.mb_s) : std::string("0")) +
              ", \"ms\": " + (res.ms > 0 && res.ms < 1e8 ? fmt(res.ms) : std::string("0")) +
              ", \"bytes\": " + std::to_string(res.bytes) + "},\n";
    }
    if (js.size() > 2) {
        js[js.size() - 2] = '\n';
        js[js.size() - 1] = '\0';
        js.resize(js.size() - 1);
    }
    js += "]\n";

    char path[512];
    snprintf(path, sizeof(path), "%s/bench-matrix.md", out_dir);
    FILE* f = fopen(path, "w");
    if (f != NULL) {
        fwrite(md.data(), 1, md.size(), f);
        fclose(f);
    }
    snprintf(path, sizeof(path), "%s/bench-matrix.json", out_dir);
    f = fopen(path, "w");
    if (f != NULL) {
        fwrite(js.data(), 1, js.size(), f);
        fclose(f);
    }
    printf("%s", md.c_str());
    return 0;
}
