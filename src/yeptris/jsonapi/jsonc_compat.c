/* jsonc_compat.c — json-c symbols over the yeptris core (TODO.impl/21).
 *
 * A thin read-path shim: parse via the strict JSON mode, query via the
 * DOM API, output via the JSON writer. The root object owns the
 * YeptrisDocument; children borrow it.
 */

#include <stdlib.h>
#include <string.h>

#include "../../include/yeptris/dom.h"
#include "../../include/yeptris/json.h"
#include "../../include/yeptris/jsonc_compat.h"

struct json_object {
    YeptrisDocument doc; /* owned only when owns_doc */
    YeptrisNode node;    /* NULL for the root placeholder */
    int owns_doc;
    char* json_out; /* cached to_json_string result */
    /* the root registers every child wrapper it hands out; children
     * live until the root is put (json-c lifetime: borrowed refs die
     * with the root) */
    struct json_object** children;
    size_t nchild;
    size_t capchild;
};

static json_object* child_of(json_object* root, YeptrisNode node) {
    if (root == NULL || node == NULL) {
        return NULL;
    }
    if (root->nchild == root->capchild) {
        size_t cap = root->capchild ? root->capchild * 2 : 8;
        struct json_object** grown = realloc(root->children, cap * sizeof(*grown));
        if (grown == NULL) {
            return NULL;
        }
        root->children = grown;
        root->capchild = cap;
    }
    json_object* child = calloc(1, sizeof(*child));
    if (child == NULL) {
        return NULL;
    }
    child->doc = root->doc;
    child->node = node;
    root->children[root->nchild++] = child;
    return child;
}

json_object* json_tokener_parse(const char* str) {
    if (str == NULL) {
        return NULL;
    }
    return json_tokener_parse_ex(str, (int)strlen(str));
}

json_object* json_tokener_parse_ex(const char* buf, int len) {
    if (buf == NULL || len < 0) {
        return NULL;
    }
    /* json-c tolerates trailing NUL: honor the explicit length only */
    YeptrisStatus st = YEPTRIS_OK;
    YeptrisDocument doc = yeptris_parse_json(buf, (size_t)len, &st);
    if (doc == NULL) {
        return NULL;
    }
    if (yeptris_document_count(doc) == 0) {
        yeptris_document_free(doc);
        return NULL;
    }
    json_object* obj = calloc(1, sizeof(*obj));
    if (obj == NULL) {
        yeptris_document_free(doc);
        return NULL;
    }
    obj->doc = doc;
    obj->node = yeptris_document_root(doc, 0);
    obj->owns_doc = 1;
    return obj;
}

void json_tokener_free(json_object* obj) {
    json_object_put(obj);
}

int json_object_put(json_object* obj) {
    if (obj == NULL) {
        return 0;
    }
    if (obj->owns_doc) {
        free(obj->json_out);
        for (size_t i = 0; i < obj->nchild; i++) {
            free(obj->children[i]->json_out);
            free(obj->children[i]);
        }
        free(obj->children);
        yeptris_document_free(obj->doc);
        free(obj);
    }
    /* children are owned by their root */
    return 1;
}

json_object* json_object_get(json_object* obj) {
    return obj;
}

static int kind_of(const json_object* obj) {
    if (obj == NULL || obj->node == NULL) {
        return -1;
    }
    return (int)yeptris_node_kind(obj->node);
}

json_type json_object_get_type(const json_object* obj) {
    if (obj == NULL || obj->node == NULL) {
        return json_type_null;
    }
    switch (kind_of(obj)) {
    case YEPTRIS_NODE_MAPPING:
        return json_type_object;
    case YEPTRIS_NODE_SEQUENCE:
        return json_type_array;
    case YEPTRIS_NODE_ALIAS:
        return json_type_null;
    default:
        break;
    }
    /* Typed accessors respect implicitness: a QUOTED "12" in strict
     * JSON is a string (the accessors reject it), a bare 12 is int —
     * the discrimination comes free from the resolver */
    int64_t i;
    double d;
    int b;
    if (yeptris_node_int(obj->node, &i) == YEPTRIS_OK) {
        return json_type_int;
    }
    if (yeptris_node_float(obj->node, &d) == YEPTRIS_OK) {
        return json_type_double;
    }
    if (yeptris_node_bool(obj->node, &b) == YEPTRIS_OK) {
        return json_type_boolean;
    }
    size_t len = 0;
    const char* v = yeptris_node_value(obj->node, &len);
    if (len == 4 && v != NULL && memcmp(v, "null", 4) == 0) {
        return json_type_null;
    }
    return json_type_string;
}

int json_object_is_type(const json_object* obj, json_type type) {
    return json_object_get_type(obj) == type;
}

const char* json_object_get_string(json_object* obj) {
    if (obj == NULL || obj->node == NULL) {
        return NULL;
    }
    size_t len = 0;
    const char* v = yeptris_node_value(obj->node, &len);
    if (v == NULL) {
        return NULL;
    }
    char* out = malloc(len + 1);
    if (out == NULL) {
        return NULL;
    }
    memcpy(out, v, len);
    out[len] = '\0';
    /* attach to the object's slot: freed on the NEXT call or at put —
     * json-c returns owned storage; we approximate with a per-object
     * cache (the last getString result) */
    free(obj->json_out);
    obj->json_out = out;
    return out;
}

int32_t json_object_get_int(const json_object* obj) {
    int64_t v = json_object_get_int64(obj);
    return (int32_t)v;
}

int64_t json_object_get_int64(const json_object* obj) {
    int64_t v = 0;
    if (obj != NULL && obj->node != NULL) {
        yeptris_node_int(obj->node, &v);
    }
    return v;
}

double json_object_get_double(const json_object* obj) {
    double v = 0.0;
    if (obj != NULL && obj->node != NULL) {
        yeptris_node_float(obj->node, &v);
    }
    return v;
}

int json_object_get_boolean(const json_object* obj) {
    int v = 0;
    if (obj != NULL && obj->node != NULL) {
        yeptris_node_bool(obj->node, &v);
    }
    return v;
}

int json_object_object_get_ex(const json_object* obj, const char* key, json_object** value) {
    if (obj == NULL || key == NULL || kind_of(obj) != YEPTRIS_NODE_MAPPING) {
        if (value != NULL) {
            *value = NULL;
        }
        return 0;
    }
    YeptrisNode n = yeptris_node_map_get(obj->node, key, strlen(key));
    if (n == NULL) {
        if (value != NULL) {
            *value = NULL;
        }
        return 0;
    }
    if (value != NULL) {
        json_object* child = child_of((json_object*)obj, n);
        *value = child;
        return child != NULL;
    }
    return 1;
}

size_t json_object_object_length(const json_object* obj) {
    if (obj == NULL || kind_of(obj) != YEPTRIS_NODE_MAPPING) {
        return 0;
    }
    return yeptris_node_map_count(obj->node);
}

size_t json_object_array_length(const json_object* obj) {
    if (obj == NULL || kind_of(obj) != YEPTRIS_NODE_SEQUENCE) {
        return 0;
    }
    return yeptris_node_seq_count(obj->node);
}

json_object* json_object_array_get_idx(const json_object* obj, size_t idx) {
    if (obj == NULL || kind_of(obj) != YEPTRIS_NODE_SEQUENCE) {
        return NULL;
    }
    if (idx >= yeptris_node_seq_count(obj->node)) {
        return NULL;
    }
    return child_of((json_object*)obj, yeptris_node_seq_at(obj->node, idx));
}

const char* json_object_to_json_string_ext(json_object* obj, int flags) {
    (void)flags; /* spacing variants arrive with the pretty printer */
    if (obj == NULL) {
        return NULL;
    }
    if (obj->json_out != NULL && obj->owns_doc) {
        free(obj->json_out);
    }
    /* children serialize their SUBTREE: emit from the node's document
     * requires a whole-document writer; v1 supports roots only */
    if (obj->node == NULL || !obj->owns_doc) {
        return NULL;
    }
    size_t len = 0;
    obj->json_out = yeptris_serialize_json(obj->doc, &len);
    return obj->json_out;
}

const char* json_object_to_json_string(json_object* obj) {
    return json_object_to_json_string_ext(obj, JSON_C_TO_STRING_PLAIN);
}
