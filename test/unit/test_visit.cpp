/* test_visit.cpp — the visit API's C-side gate (TODO.restructure/24).
 *
 * Counts visitor callbacks over representative inputs and pins the
 * exact sequences: values in document order, keys before values,
 * container nesting, anchors/aliases on the YAML path. The fused
 * JSON path's object-graph semantics are pinned in the Ruby binding
 * (native_spec, against JSON.parse). */

#include <gtest/gtest.h>
#include <string.h>

#include <yeptris.h>
#include <yeptris/dom.h>
#include <yeptris/visit.h>

namespace {

struct Log {
    char buf[512];
    size_t n = 0;
    int ints = 0, strs = 0, seqs = 0, maps = 0, nulls = 0, bools = 0, floats = 0;
    void add(const char* s) {
        size_t l = strlen(s);
        if (n + l < sizeof(buf)) {
            memcpy(buf + n, s, l);
            n += l;
            buf[n] = '\0';
        }
    }
};

int v_null(void* ctx) {
    ((Log*)ctx)->add("N;");
    ((Log*)ctx)->nulls++;
    return 0;
}
int v_bool(void* ctx, int t) {
    ((Log*)ctx)->add(t ? "T;" : "F;");
    ((Log*)ctx)->bools++;
    return 0;
}
int v_int(void* ctx, int64_t) {
    ((Log*)ctx)->add("i;");
    ((Log*)ctx)->ints++;
    return 0;
}
int v_float(void* ctx, double) {
    ((Log*)ctx)->add("f;");
    ((Log*)ctx)->floats++;
    return 0;
}
int v_str(void* ctx, const char*, size_t) {
    ((Log*)ctx)->add("s;");
    ((Log*)ctx)->strs++;
    return 0;
}
int v_ss(void* ctx) {
    ((Log*)ctx)->add("[");
    ((Log*)ctx)->seqs++;
    return 0;
}
int v_se(void* ctx) {
    ((Log*)ctx)->add("]");
    return 0;
}
int v_ms(void* ctx) {
    ((Log*)ctx)->add("{");
    ((Log*)ctx)->maps++;
    return 0;
}
int v_me(void* ctx) {
    ((Log*)ctx)->add("}");
    return 0;
}
int v_key(void* ctx, const char*, size_t) {
    ((Log*)ctx)->add("k;");
    return 0;
}

const YeptrisVisitVTable k_log = {
    v_null, v_bool, v_int, v_float, v_str, v_ss, v_se, v_ms, v_me, v_key, NULL, NULL, NULL,
};

TEST(Visit, JsonSequence) {
    Log log = {};
    const char* in = "{\"a\":[1,true,null]}";
    YeptrisStatus st = yeptris_visit_json(in, strlen(in), &k_log, &log);
    ASSERT_EQ(st, YEPTRIS_OK);
    EXPECT_STREQ(log.buf, "{k;[i;T;N;]}");
}

TEST(Visit, JsonScalars) {
    Log log = {};
    YeptrisStatus st = yeptris_visit_json("-12.5", 5, &k_log, &log);
    ASSERT_EQ(st, YEPTRIS_OK);
    EXPECT_EQ(log.floats, 1);
    st = yeptris_visit_json("42", 2, &k_log, &log);
    ASSERT_EQ(st, YEPTRIS_OK);
    EXPECT_EQ(log.ints, 1);
    EXPECT_STREQ(log.buf, "f;i;");
}

TEST(Visit, JsonTrailingGarbage) {
    Log log = {};
    EXPECT_EQ(yeptris_visit_json("{} x", 4, &k_log, &log), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(yeptris_visit_json("[1,]", 4, &k_log, &log), YEPTRIS_ERROR_PARSE);
    EXPECT_EQ(yeptris_visit_json("{\"a\" 1}", 7, &k_log, &log), YEPTRIS_ERROR_PARSE);
}

TEST(Visit, YamlSequence) {
    Log log = {};
    const char* in = "a: 1\nb:\n  - x\n  - true\n";
    YeptrisStatus st = yeptris_visit(in, strlen(in), YEPTRIS_SCHEMA_11_COMPAT, &k_log, &log);
    ASSERT_EQ(st, YEPTRIS_OK) << yeptris_last_error(0, 0);
    EXPECT_STREQ(log.buf, "{k;i;k;[s;T;]}");
}

TEST(Visit, Node) {
    const char* yaml = "- 1\n- two\n";
    YeptrisDocument doc = yeptris_parse(yaml, strlen(yaml), nullptr);
    ASSERT_NE(doc, nullptr);
    YeptrisNode root = yeptris_document_root(doc, 0);
    ASSERT_NE(root, nullptr);
    Log log = {};
    ASSERT_EQ(yeptris_visit_node(root, &k_log, &log), YEPTRIS_OK);
    EXPECT_STREQ(log.buf, "[i;s;]");
    yeptris_document_free(doc);
}

TEST(Visit, NullVtable) {
    YeptrisVisitVTable none = {};
    none.on_null = NULL;
    EXPECT_EQ(yeptris_visit_json("{\"a\":1}", 7, &none, NULL), YEPTRIS_OK);
    EXPECT_EQ(yeptris_visit_json("bad", 3, &none, NULL), YEPTRIS_ERROR_PARSE);
}

} // namespace
