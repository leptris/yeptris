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
#include <yeptris/cbor.h> /* TODO.cbor/07: the CBOR tier */
#include <yeptris/json.h> /* yeptris_parse_json: the strict-JSON direct build */
#include <yeptris/tape.h> /* yeptris_parse_json_tape: the fused walk-to-tape */

#if defined(YEP_BENCH_LIBYAML)
#include <yaml.h>
#endif

#if defined(YEP_BENCH_RYML)
#include <ryml.hpp>
#include <ryml_std.hpp> /* c4::to_substr(std::string&) adapters */
#endif

#if defined(YEP_BENCH_SIMDJSON)
#include <simdjson.h> /* the JSON-field reference (TODO.restructure/81) */
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
#include "common/cpu.h"
#include "common/simd_text.h"
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
    alloc_count cnt = {0, 0, 0, 0};
    yep_allocator counter = {count_alloc, count_free, &cnt};
    yep_engine* eng = yep_engine_create(&counter);
    yep_dom* dom = yep_dom_create(&counter);
    mem_stats ms = {0, 0, 0};
    if (eng != NULL && dom != NULL) {
        /* the same sizing seam as parse_impl: the measure must
         * exercise the product's reserve path, not a bare engine */
        yep_text_stats st;
        yep_text_active()->scan_stats(c.data.data(), c.data.size(), &st);
        yep_dom_prepare(dom, &st);
        yep_sink sink = {.on_event = yep_dom_on_event,
                         .ctx = dom,
                         .on_flow_build = NULL,
                         .on_flow_commit = NULL,
                         .on_flow_rollback = NULL,
                         .on_block_pair = NULL,
                         .on_block_open = NULL,
                         .on_block_item = NULL};
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

/* One large STRICT-JSON document (TODO.restructure/81): the same
 * object mix as flow-json but as a single JSON array — the corpus for
 * the JSON-field comparison (yeptris_parse_json's direct build vs the
 * dedicated JSON parsers). */
void gen_json_doc(std::string* out, Rng& r, int entries) {
    out->append("[");
    for (int i = 0; i < entries; i++) {
        out->append((i ? ", " : "") + std::string("{\"id\": ") + std::to_string(i) +
                    ", \"name\": \"" + word(r) + "\", \"vals\": [" + std::to_string(r.pick(1000)) +
                    ", " + std::to_string(r.pick(1000)) + ", " + std::to_string(r.pick(1000)) +
                    "], \"ok\": " + (r.pick(2) ? "true" : "false") + " }");
    }
    out->append("]\n");
}

/* serialbench's medium.json shape (issue #342): rows of small maps
 * with a nested profile map — the many-token, map-heavy JSON the DOM
 * route loses hardest on; the attribution corpus for the DOM-build
 * lane. */
void gen_json_users(std::string* out, Rng& r, int entries) {
    (void)r;
    out->append("{\"users\":[");
    for (int i = 0; i < entries; i++) {
        out->append(
            (i ? "," : "") + std::string("{\"id\":") + std::to_string(i) + ",\"name\":\"user " +
            std::to_string(i) + "\",\"email\":\"user" + std::to_string(i) +
            "@example.com\",\"active\":" + ((i & 1) ? "true" : "false") +
            ",\"score\":" + std::to_string(i) +
            ".5,\"tags\":[\"a\",\"b\",\"c\"],\"profile\":{\"age\":" + std::to_string(i + 20) +
            ",\"theme\":\"dark\"}}");
    }
    out->append("]}\n");
}

/* One giant ONE-LINE flow collection: the shape that hid the
 * jx_advance_line quadratic (a single long line rescanned per token)
 * — the many-small-collections shape could never catch it. */
void gen_flow_single(std::string* out, Rng& r, int entries) {
    out->append("[");
    for (int i = 0; i < entries; i++) {
        out->append((i ? ", " : "") + std::string("{ \"id\": ") + std::to_string(i) +
                    ", \"name\": \"" + word(r) + "\", \"ok\": " + (r.pick(2) ? "true" : "false") +
                    " }");
    }
    out->append(" ]\n");
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

/* #352: the emit kernel split — the fresh-allocation route callers get
 * from yeptris_serialize vs the caller-buffer route (serialize_into,
 * sized once, reused), interleaved medians. The ryml reference lives
 * in serialbench's CI table (the issue); this isolates what we own:
 * the allocation share vs the kernel itself. */
struct EmitSplit {
    double alloc_mb;
    double into_mb;
};
EmitSplit emit_split(const Corpus& c, int rounds) {
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse(c.data.data(), c.data.size(), &st);
    if (doc == NULL) {
        return {0, 0};
    }
    size_t need = yeptris_serialize_into(doc, NULL, 0);
    std::vector<char> buf(need + 1);
    std::vector<double> a_ms, i_ms;
    double mb = (double)need / (1024.0 * 1024.0);
    for (int r = 0; r < rounds; r++) {
        size_t len = 0;
        auto t0 = clk::now();
        char* s = yeptris_serialize(doc, &len);
        auto t1 = clk::now();
        yeptris_free(s);
        a_ms.push_back(ms_of(t0, t1));
        t0 = clk::now();
        yeptris_serialize_into(doc, buf.data(), buf.size());
        t1 = clk::now();
        i_ms.push_back(ms_of(t0, t1));
    }
    yeptris_document_free(doc);
    std::sort(a_ms.begin(), a_ms.end());
    std::sort(i_ms.begin(), i_ms.end());
    double am = a_ms[a_ms.size() / 2];
    double im = i_ms[i_ms.size() / 2];
    return {am > 0 ? mb * 1000.0 / am : 0, im > 0 ? mb * 1000.0 / im : 0};
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

#if defined(YEP_BENCH_RYML)
/* rapidyaml (the mission's field benchmark): in-place parse to its
 * tree. The per-iteration buffer copy sits OUTSIDE the timed region
 * (ryml mutates its input; the corpus must be reset fairly). */
/* ryml's default error callback ABORTS (their contract); a parse
 * failure on a corpus must be an n/a row, not a dead bench. Their
 * own test suite throws across the same frames. */
struct RymlParseFailure {};

Result bench_ryml(const Corpus& c, int iters) {
    ryml::Callbacks cb = ryml::get_callbacks();
    cb.m_error_basic = [](ryml::csubstr, ryml::ErrorDataBasic const&, void*) {
        throw RymlParseFailure();
    };
    cb.m_error_parse = [](ryml::csubstr, ryml::ErrorDataParse const&, void*) {
        throw RymlParseFailure();
    };
    cb.m_error_visit = [](ryml::csubstr, ryml::ErrorDataVisit const&, void*) {
        throw RymlParseFailure();
    };
    ryml::set_callbacks(cb);
    double best_ms = 1e9;
    std::string scratch;
    scratch.resize(c.data.size());
    ryml::Tree tree; /* reused across iterations: their benchmark's */
    try {
        for (int i = 0; i < iters; i++) { /* steady-state shape (bm_parse) */
            memcpy(&scratch[0], c.data.data(), c.data.size());
            auto t0 = clk::now();
            ryml::parse_in_place(ryml::csubstr{}, ryml::to_substr(scratch), &tree);
            auto t1 = clk::now();
            if (tree.size() <= 1) {
                break;
            }
            double m = ms_of(t0, t1);
            if (m < best_ms) {
                best_ms = m;
            }
        }
    } catch (RymlParseFailure&) {
        ryml::reset_callbacks();
        return {c.name + " (ryml)", 0, 0, c.data.size()}; /* rejected: n/a */
    }
    ryml::reset_callbacks();
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    return {c.name + " (ryml)", best_ms < 1e9 ? mb * 1000.0 / best_ms : 0, best_ms, c.data.size()};
}

/* Interleaved head-to-head: one yeptris parse then one ryml parse per
 * round, same machine state for both, median of per-round ratios.
 * Separate-phase best-of rides thermal/cache phase bias (and runners
 * are bimodal); the interleaved median is the campaign referee. */
double h2h_ratio(const Corpus& c, int rounds, double* yep_mb) {
    ryml::Callbacks cb = ryml::get_callbacks();
    cb.m_error_basic = [](ryml::csubstr, ryml::ErrorDataBasic const&, void*) {
        throw RymlParseFailure();
    };
    cb.m_error_parse = [](ryml::csubstr, ryml::ErrorDataParse const&, void*) {
        throw RymlParseFailure();
    };
    cb.m_error_visit = [](ryml::csubstr, ryml::ErrorDataVisit const&, void*) {
        throw RymlParseFailure();
    };
    ryml::set_callbacks(cb);
    std::string scratch;
    scratch.resize(c.data.size());
    ryml::Tree tree;
    std::vector<double> ratios;
    double best_yep = 1e9;
    YeptrisStatus st = YEPTRIS_OK;
    try {
        for (int i = 0; i < rounds; i++) {
            /* Order alternates per round: whoever parses second rides the
             * core the first just warmed (turbo/cache state) — a fixed
             * order is a systematic bias that scales with ambient clock
             * (the referee swung ±0.2 across identical builds). */
            double ty, tr;
            if (i & 1) {
                memcpy(&scratch[0], c.data.data(), c.data.size());
                auto b0 = clk::now();
                ryml::parse_in_place(ryml::csubstr{}, ryml::to_substr(scratch), &tree);
                auto b1 = clk::now();
                if (tree.size() <= 1) {
                    break; /* rejected corpus: no h2h */
                }
                auto a0 = clk::now();
                YeptrisDocument d = yeptris_parse(c.data.data(), c.data.size(), &st);
                auto a1 = clk::now();
                yeptris_document_free(d);
                ty = ms_of(a0, a1);
                tr = ms_of(b0, b1);
            } else {
                auto a0 = clk::now();
                YeptrisDocument d = yeptris_parse(c.data.data(), c.data.size(), &st);
                auto a1 = clk::now();
                yeptris_document_free(d);
                memcpy(&scratch[0], c.data.data(), c.data.size());
                auto b0 = clk::now();
                ryml::parse_in_place(ryml::csubstr{}, ryml::to_substr(scratch), &tree);
                auto b1 = clk::now();
                if (tree.size() <= 1) {
                    break; /* rejected corpus: no h2h */
                }
                ty = ms_of(a0, a1);
                tr = ms_of(b0, b1);
            }
            if (ty < best_yep) {
                best_yep = ty;
            }
            ratios.push_back(tr / ty); /* >1: yeptris faster */
        }
    } catch (RymlParseFailure&) {
        ryml::reset_callbacks();
        return 0;
    }
    ryml::reset_callbacks();
    if (ratios.empty()) {
        return 0;
    }
    std::sort(ratios.begin(), ratios.end());
    double med = ratios[ratios.size() / 2];
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    *yep_mb = best_yep < 1e9 ? mb * 1000.0 / best_yep : 0;
    return med;
}
#endif

#if defined(YEP_BENCH_SIMDJSON)
/* The JSON-field referee (TODO.restructure/81): yeptris_parse_json's
 * direct DOM build vs simdjson's DOM parse over one strict-JSON
 * document. Same discipline as the ryml referee: interleaved,
 * order-alternating, median of per-round ratios. */
Result bench_simdjson(const Corpus& c, int iters) {
    simdjson::dom::parser parser;
    double best_ms = 1e9;
    for (int i = 0; i < iters; i++) {
        auto t0 = clk::now();
        simdjson::dom::element doc = parser.parse(c.data.data(), c.data.size());
        auto t1 = clk::now();
        (void)doc;
        double ms = ms_of(t0, t1);
        if (ms < best_ms) {
            best_ms = ms;
        }
    }
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    return {c.name + " (simdjson)", best_ms < 1e9 ? mb * 1000.0 / best_ms : 0, best_ms,
            c.data.size()};
}

/* ---- CBOR vs JSON on the same DOM (TODO.cbor/07, C tier) --------
 * The corpus's JSON is encoded to CBOR once (canonical), then both
 * decoders/encoders run interleaved over their own byte forms. Ratios
 * are per-shape: decode CBOR/parse JSON, encode CBOR/emit JSON
 * (canonical), and the encoded-size ratio. */
struct CborTier {
    double dec_ratio; /* >1: CBOR decode faster than JSON parse */
    double enc_ratio;
    double size_ratio; /* cbor bytes / json bytes */
    double dec_mb, enc_mb;
    size_t json_len, cbor_len;
};

CborTier cbor_vs_json(const Corpus& c, int rounds) {
    CborTier t = {0, 0, 0, 0, 0, 0, 0};
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument seed = yeptris_parse_json(c.data.data(), c.data.size(), &st);
    if (seed == NULL || st != YEPTRIS_OK) {
        yeptris_document_free(seed);
        return t; /* not strict JSON: no CBOR tier for this shape */
    }
    size_t clen = 0;
    unsigned char* cbor = (unsigned char*)yeptris_cbor_encode(seed, YEPTRIS_CBOR_CANONICAL, &clen);
    yeptris_document_free(seed);
    if (cbor == NULL) {
        return t;
    }
    /* documents held OUTSIDE the loops: the encoders are timed alone */
    YeptrisDocument jdoc = yeptris_parse_json(c.data.data(), c.data.size(), &st);
    YeptrisDocument cdoc = yeptris_cbor_decode(cbor, clen, 0, &st);
    if (jdoc == NULL || cdoc == NULL) {
        yeptris_document_free(jdoc);
        yeptris_document_free(cdoc);
        free(cbor);
        return t;
    }
    double best_jp = 1e9, best_cd = 1e9, best_je = 1e9, best_ce = 1e9;
    double mb_c = (double)clen / (1024.0 * 1024.0);
    for (int i = 0; i < rounds; i++) {
        auto a0 = clk::now();
        YeptrisDocument d = yeptris_parse_json(c.data.data(), c.data.size(), &st);
        auto a1 = clk::now();
        yeptris_document_free(d);
        double jp = ms_of(a0, a1);
        if (jp < best_jp)
            best_jp = jp;

        auto b0 = clk::now();
        YeptrisDocument dc = yeptris_cbor_decode(cbor, clen, 0, &st);
        auto b1 = clk::now();
        yeptris_document_free(dc);
        double cd = ms_of(b0, b1);
        if (cd < best_cd)
            best_cd = cd;

        size_t jlen = 0;
        auto e0 = clk::now();
        char* jout = yeptris_serialize_ex(jdoc, NULL, &jlen);
        auto e1 = clk::now();
        free(jout);
        double je = ms_of(e0, e1);
        if (je < best_je)
            best_je = je;

        size_t olen = 0;
        auto f0 = clk::now();
        unsigned char* cout =
            (unsigned char*)yeptris_cbor_encode(cdoc, YEPTRIS_CBOR_CANONICAL, &olen);
        auto f1 = clk::now();
        free(cout);
        double ce = ms_of(f0, f1);
        if (ce < best_ce)
            best_ce = ce;
    }
    yeptris_document_free(jdoc);
    yeptris_document_free(cdoc);
    free(cbor);
    t.dec_ratio = best_jp / best_cd;
    t.enc_ratio = best_je / best_ce;
    t.size_ratio = (double)clen / (double)c.data.size();
    t.dec_mb = best_cd < 1e9 ? mb_c * 1000.0 / best_cd : 0;
    t.enc_mb = best_ce < 1e9 ? mb_c * 1000.0 / best_ce : 0;
    t.json_len = c.data.size();
    t.cbor_len = clen;
    return t;
}

struct H2hJson {
    double dom_ratio;
    double tape_ratio;
    double lnt_ratio;
    double dom_mb;
    double tape_mb;
    double lnt_mb;
};

/* Both yeptris routes ride the same interleaved rounds: parse_json's
 * direct DOM build and parse_json_tape's fused walk-to-tape (item 85),
 * each ratioed against simdjson DOM per round, median of ratios. */
H2hJson h2h_vs_simdjson(const Corpus& c, int rounds) {
    simdjson::dom::parser parser;
    std::vector<double> dom_ratios, tape_ratios;
    double best_yep = 1e9;
    double best_tape = 1e9;
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument probe = yeptris_parse_json(c.data.data(), c.data.size(), &st);
    if (probe == NULL) {
        yeptris_document_free(probe);
        return {0, 0, 0, 0, 0, 0}; /* not strict JSON: no referee */
    }
    yeptris_document_free(probe);
    std::vector<double> lnt_ratios;
    double best_lnt = 1e9;
    for (int i = 0; i < rounds; i++) {
        double ty, tp, tr, tl;
        if (i & 1) {
            auto b0 = clk::now();
            simdjson::dom::element doc = parser.parse(c.data.data(), c.data.size());
            auto b1 = clk::now();
            (void)doc;
            auto a0 = clk::now();
            YeptrisDocument d = yeptris_parse_json(c.data.data(), c.data.size(), &st);
            auto a1 = clk::now();
            yeptris_document_free(d);
            auto p0 = clk::now();
            yeptris_json_tape tape;
            yeptris_parse_json_tape(c.data.data(), c.data.size(), &tape);
            auto p1 = clk::now();
            yeptris_tape_free(&tape);
            auto l0 = clk::now();
            yeptris_json_tape lt;
            yeptris_parse_json_tape_lenient(c.data.data(), c.data.size(), &lt);
            auto l1 = clk::now();
            yeptris_tape_free(&lt);
            ty = ms_of(a0, a1);
            tp = ms_of(p0, p1);
            tr = ms_of(b0, b1);
            tl = ms_of(l0, l1);
        } else {
            auto l0 = clk::now();
            yeptris_json_tape lt;
            yeptris_parse_json_tape_lenient(c.data.data(), c.data.size(), &lt);
            auto l1 = clk::now();
            yeptris_tape_free(&lt);
            auto p0 = clk::now();
            yeptris_json_tape tape;
            yeptris_parse_json_tape(c.data.data(), c.data.size(), &tape);
            auto p1 = clk::now();
            yeptris_tape_free(&tape);
            auto a0 = clk::now();
            YeptrisDocument d = yeptris_parse_json(c.data.data(), c.data.size(), &st);
            auto a1 = clk::now();
            yeptris_document_free(d);
            auto b0 = clk::now();
            simdjson::dom::element doc = parser.parse(c.data.data(), c.data.size());
            auto b1 = clk::now();
            (void)doc;
            ty = ms_of(a0, a1);
            tp = ms_of(p0, p1);
            tr = ms_of(b0, b1);
            tl = ms_of(l0, l1);
        }
        if (ty < best_yep) {
            best_yep = ty;
        }
        if (tp < best_tape) {
            best_tape = tp;
        }
        if (tl < best_lnt) {
            best_lnt = tl;
        }
        dom_ratios.push_back(tr / ty); /* >1: yeptris faster */
        tape_ratios.push_back(tr / tp);
        lnt_ratios.push_back(tr / tl);
    }
    std::sort(dom_ratios.begin(), dom_ratios.end());
    std::sort(tape_ratios.begin(), tape_ratios.end());
    std::sort(lnt_ratios.begin(), lnt_ratios.end());
    double mb = (double)c.data.size() / (1024.0 * 1024.0);
    return {dom_ratios[dom_ratios.size() / 2],
            tape_ratios[tape_ratios.size() / 2],
            lnt_ratios[lnt_ratios.size() / 2],
            best_yep < 1e9 ? mb * 1000.0 / best_yep : 0,
            best_tape < 1e9 ? mb * 1000.0 / best_tape : 0,
            best_lnt < 1e9 ? mb * 1000.0 / best_lnt : 0};
}
#endif

} // namespace

static std::string g_kernels_line; /* rides the markdown artifact (80) */

int main(int argc, char** argv) {
    const char* out_dir = "bench-out";
    int full = 0;
    const char* one_shape = NULL; /* --shape NAME: DOM loop only (profiling) */
    unsigned long seed = 0xC0FFEE;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--full") == 0) {
            full = 1;
        } else if (strcmp(argv[i], "--quick") == 0) {
            full = 0;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            seed = (unsigned long)strtoul(argv[++i], NULL, 0);
        } else if (strcmp(argv[i], "--shape") == 0 && i + 1 < argc) {
            one_shape = argv[++i];
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
        gen_flow_single(&s, r, entries / 4);
        corpora.push_back({"flow-single", s});
    }
    {
        std::string s;
        gen_json_doc(&s, r, entries);
        corpora.push_back({"json-doc", s});
    }
    {
        std::string s;
        gen_json_users(&s, r, entries);
        corpora.push_back({"json-users", s});
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

    /* The artifact states which kernel table this run actually used —
     * a mis-dispatched run (scalar on an AVX2 box) must be visible in
     * the artifact, not inferred from symbol names after the fact. */
    {
        const char* impl = "scalar";
#if defined(__x86_64__) || defined(_M_X64)
        yep_cpu_features cpu = yep_cpu_detect();
        impl = cpu.avx2 ? "avx2" : (cpu.avx ? "avx(only)" : "scalar(sse2)");
#elif defined(__aarch64__)
        impl = "neon";
#endif
        printf("kernels: %s\n", impl);
        g_kernels_line = std::string("kernels: ") + impl + "\n";
    }

    /* --shape NAME: DOM parse loop on the one corpus, ~30s (sample
     * target for the perf/profile workflows — one hot path per run) */
    if (one_shape != NULL) {
        for (const Corpus& c : corpora) {
            if (c.name != one_shape) {
                continue;
            }
            Result r = bench_dom(c, 400);
            printf("%s: DOM %.2f MB/s (%.2f ms)\n", c.name.c_str(), r.mb_s, r.ms);
            return 0;
        }
        fprintf(stderr, "unknown shape: %s\n", one_shape);
        return 2;
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
#if defined(YEP_BENCH_RYML)
        results.push_back(bench_ryml(c, iters));
#endif
#if defined(YEP_BENCH_SIMDJSON)
        if (c.name == "json-doc") {
            results.push_back(bench_simdjson(c, iters));
        }
#endif
    }

    /* The artifact carries the referee tables too: stdout is not
     * captured by CI, the markdown file is (TODO.restructure/80). */
    std::string md_h2h;

#if defined(YEP_BENCH_RYML)
    /* Interleaved head-to-head vs ryml (the campaign referee, item 48). */
    printf("\n# head-to-head vs rapidyaml (interleaved, median of rounds)\n\n"
           "| shape | yeptris DOM MB/s | vs ryml |\n|---|---|---|\n");
    md_h2h += "\n# head-to-head vs rapidyaml (interleaved, median of rounds)\n\n"
              "| shape | yeptris DOM MB/s | vs ryml |\n|---|---|---|\n";
    for (const Corpus& c : corpora) {
        double yep_mb = 0;
        double med = h2h_ratio(c, full ? 9 : 5, &yep_mb);
        if (med == 0) {
            printf("| %s | n/a | n/a |\n", c.name.c_str());
            continue;
        }
        printf("| %s | %.2f | %.2fx |\n", c.name.c_str(), yep_mb, med);
        char row[160];
        snprintf(row, sizeof(row), "| %s | %.2f | %.2fx |\n", c.name.c_str(), yep_mb, med);
        md_h2h += row;
    }
    printf("\n");
#endif

#if defined(YEP_BENCH_SIMDJSON)
    /* The JSON-field referee (TODO.restructure/81; the tape leg is 85). */
    printf("\n# CBOR vs JSON on the same DOM (TODO.cbor/07, canonical, best-of)\n\n"
           "| shape | decode vs JSON parse | encode vs JSON emit | cbor/json size | CBOR decode "
           "MB/s |\n"
           "|---|---|---|---|---|\n");
    for (const Corpus& c : corpora) {
        if (c.name != "json-doc" && c.name != "flow-single" && c.name != "scalar-heavy") {
            continue; /* the tier rides JSON-class shapes */
        }
        CborTier t = cbor_vs_json(c, full ? 9 : 5);
        if (t.cbor_len == 0) {
            continue;
        }
        printf("| %s | %.2fx | %.2fx | %.2f | %.1f |\n", c.name.c_str(), t.dec_ratio, t.enc_ratio,
               t.size_ratio, t.dec_mb);
    }
    printf("\n# head-to-head vs simdjson DOM (JSON shapes, interleaved, median of rounds)\n\n"
           "| route | MB/s | vs simdjson |\n|---|---|---|\n");
    md_h2h += "\n# head-to-head vs simdjson DOM (JSON shapes, interleaved, median of rounds)\n\n"
              "| route | MB/s | vs simdjson |\n|---|---|---|\n";
    for (const Corpus& c : corpora) {
        if (c.name != "json-doc" && c.name != "json-users") {
            continue; /* the JSON-field shapes: the DOM-build lane's table */
        }
        H2hJson h = h2h_vs_simdjson(c, full ? 9 : 5);
        printf("| parse_json DOM | %.2f | %.2fx |\n", h.dom_mb, h.dom_ratio);
        printf("| parse_json_tape | %.2f | %.2fx |\n", h.tape_mb, h.tape_ratio);
        printf("| tape_lenient | %.2f | %.2fx |\n", h.lnt_mb, h.lnt_ratio);
        char row[96];
        snprintf(row, sizeof(row), "| parse_json DOM | %.2f | %.2fx |\n", h.dom_mb, h.dom_ratio);
        md_h2h += row;
        snprintf(row, sizeof(row), "| parse_json_tape | %.2f | %.2fx |\n", h.tape_mb, h.tape_ratio);
        md_h2h += row;
        snprintf(row, sizeof(row), "| tape_lenient | %.2f | %.2fx |\n", h.lnt_mb, h.lnt_ratio);
        md_h2h += row;
    }
    printf("\n");
    printf("%s", md_h2h.c_str()); /* the tail dump for console runs */
#endif

    /* #352's referee table: the emit kernel split. */
    printf("\n# emit kernel split: serialize vs serialize_into (#352, median of rounds)\n\n"
           "| shape | serialize MB/s | into-buffer MB/s | alloc share |\n|---|---|---|---|\n");
    md_h2h += "\n# emit kernel split: serialize vs serialize_into (#352, median of rounds)\n\n"
              "| shape | serialize MB/s | into-buffer MB/s | alloc share |\n|---|---|---|---|\n";
    for (const Corpus& c : corpora) {
        if (c.name != "block-heavy" && c.name != "flow-json" && c.name != "scalar-heavy" &&
            c.name != "json-users") {
            continue;
        }
        EmitSplit e = emit_split(c, full ? 9 : 5);
        double share = (e.into_mb > 0) ? 1.0 - e.alloc_mb / e.into_mb : 0;
        char row[96];
        snprintf(row, sizeof(row), "| %s | %.2f | %.2f | %.0f%% |\n", c.name.c_str(), e.alloc_mb,
                 e.into_mb, share * 100.0);
        printf("%s", row);
        md_h2h += row;
    }

    /* Markdown + JSON */
    std::string md = g_kernels_line +
                     "\n# yeptris benchmark matrix\n\nMachine-relative: MB/s on this "
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
    md += md_h2h; /* the referee tables ride the artifact (item 80) */

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
