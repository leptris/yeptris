/* test_chartype.cpp — golden tests: every table entry re-derived from the
 * YAML spec character productions (TODO.impl/02 Phase B). A typo in the
 * table (or in this test) fails loudly.
 */

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "common/chartype.h"

namespace {

bool in(unsigned char c, const char* set) {
    /* None of our sets contain NUL; strchr would otherwise "find" the
     * terminator for c == 0. */
    if (c == 0 || set == nullptr) {
        return false;
    }
    return strchr(set, c) != nullptr;
}

uint16_t expected_flags(unsigned char c) {
    uint16_t f = 0;

    /* Whitespace and line breaks (composites are stored pre-computed). */
    if (c == ' ')
        f |= YEP_CT_SPACE;
    if (c == '\t')
        f |= YEP_CT_TAB;
    if (c == '\n')
        f |= YEP_CT_LF;
    if (c == '\r')
        f |= YEP_CT_CR;
    if (c == ' ' || c == '\t')
        f |= YEP_CT_BLANK;
    if (c == '\n' || c == '\r')
        f |= YEP_CT_LBREAK;

    /* Alphanumerics. */
    if (c >= '0' && c <= '9')
        f |= YEP_CT_DIGIT | YEP_CT_HEXDIGIT;
    if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
        f |= YEP_CT_HEXDIGIT;
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))
        f |= YEP_CT_ALPHA;

    /* c-indicator set, YAML 1.2 spec 5.5: - ? : , [ ] { } # & * ! | > ' " % @ ` */
    static const char* indicators = "-?:,[]{}#&*!|>'\"%@`";
    if (in(c, indicators))
        f |= YEP_CT_INDICATOR;

    /* Flow indicators, YAML 1.2 spec 5.5 flow indicators: , [ ] { } */
    static const char* flow = ",[]{}";
    if (in(c, flow))
        f |= YEP_CT_FLOW_IND;

    /* Byte-level printable: tab | LF | CR | 0x20..0x7E (spec 5.1, ASCII). */
    if (c == 0x09 || c == 0x0A || c == 0x0D || (c >= 0x20 && c <= 0x7E))
        f |= YEP_CT_PRINTABLE;

    /* High bytes: UTF-8 sequence territory, encoding layer decides. */
    if (c >= 0x80)
        f |= YEP_CT_HIGH;

    return f;
}

} // namespace

TEST(Chartype, GoldenTableMatchesSpec) {
    for (int i = 0; i < 256; i++) {
        unsigned char c = (unsigned char)i;
        uint16_t want = expected_flags(c);
        EXPECT_EQ(yep_chartype_table[c], want) << "byte 0x" << std::hex << i << " ('"
                                               << (c >= 0x20 && c < 0x7F ? (char)c : '?') << "')";
    }
}

TEST(Chartype, CompositesAgreeWithSingletons) {
    for (int i = 0; i < 256; i++) {
        unsigned char c = (unsigned char)i;
        EXPECT_EQ(yep_ct_any(c, YEP_CT_BLANK),
                  yep_ct_is(c, YEP_CT_SPACE) || yep_ct_is(c, YEP_CT_TAB));
        EXPECT_EQ(yep_ct_any(c, YEP_CT_LBREAK), yep_ct_is(c, YEP_CT_LF) || yep_ct_is(c, YEP_CT_CR));
    }
}

TEST(Chartype, IndicatorSetIsExact) {
    static const char* indicators = "-?:,[]{}#&*!|>'\"%@`";
    for (int i = 0; i < 256; i++) {
        unsigned char c = (unsigned char)i;
        bool want = in(c, indicators);
        EXPECT_EQ(yep_ct_is(c, YEP_CT_INDICATOR), want) << "byte 0x" << i;
        if (yep_ct_is(c, YEP_CT_FLOW_IND)) {
            EXPECT_TRUE(in(c, ",[]{}")) << "flow indicator outside the flow set: 0x" << i;
        }
    }
}

TEST(Chartype, NsCharExcludesWhitespaceAndControls) {
    EXPECT_FALSE(yep_ct_is_ns(' '));
    EXPECT_FALSE(yep_ct_is_ns('\t'));
    EXPECT_FALSE(yep_ct_is_ns('\n'));
    EXPECT_FALSE(yep_ct_is_ns('\0'));
    EXPECT_FALSE(yep_ct_is_ns(0x7F)); /* DEL */
    EXPECT_TRUE(yep_ct_is_ns('a'));
    EXPECT_TRUE(yep_ct_is_ns('-'));
    EXPECT_TRUE(yep_ct_is_ns(0xC3)); /* high byte: deferred to encoding */
}
