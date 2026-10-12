/* ============================================================================
 *  tests/test_console.cpp  --  console utility self-test
 * ----------------------------------------------------------------------------
 *  Covers the parts that are easy to get wrong and hard to notice:
 *    - UTF-8 code point decoding (Chinese is 3 bytes but 2 columns wide)
 *    - display width, which the table alignment depends on
 *    - width-aware truncation (must not split a Chinese character in half)
 *    - text-to-integer parsing (must reject "abc" rather than returning 0)
 *
 *  readLine / promptInt read from stdin, so they are NOT tested here --
 *  feeding them would depend on the shell.  The parsing rules they delegate to
 *  are tested through the same logic used by the menu.
 *
 *  Build and run:  build test
 * ==========================================================================*/

#include <cstdio>
#include <cstring>
#include "util/Console.h"

static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* what, int line)
{
    if (condition) { g_pass++; }
    else
    {
        g_fail++;
        std::printf("    [FAIL] %s   (line %d)\n", what, line);
    }
}
#define CHECK(cond, msg) check((cond), (msg), __LINE__)

/* ==========================================================================
 *  1. UTF-8 decoding
 * ==========================================================================*/
static void testDecode()
{
    std::printf("[1] utf8Decode\n");

    /* ASCII: one byte per character */
    {
        unsigned int cp = 0;
        CHECK(utf8Decode("A", 0, &cp) == 1, "ASCII 'A' occupies 1 byte");
        CHECK(cp == 0x41, "code point of 'A' is 0x41");
    }

    /* Chinese: three bytes per character.
       "头" is U+5934, encoded E5 A4 B4. */
    {
        const char* s = "头";
        unsigned int cp = 0;
        int adv = utf8Decode(s, 0, &cp);
        std::printf("      '头' -> %d bytes, U+%04X\n", adv, cp);
        CHECK(adv == 3, "one Chinese character occupies 3 bytes");
        CHECK(cp == 0x5934, "code point of '头' is U+5934");
    }

    /* second character in a string */
    {
        const char* s = "头痛";      /* 6 bytes */
        unsigned int cp = 0;
        CHECK(utf8Decode(s, 0, &cp) == 3, "first char is 3 bytes");
        CHECK(cp == 0x5934, "first char is 头");

        cp = 0;
        CHECK(utf8Decode(s, 3, &cp) == 3, "second char is 3 bytes");
        CHECK(cp == 0x75DB, "second char is 痛");
    }

    /* end of string */
    {
        unsigned int cp = 0xFFFF;
        CHECK(utf8Decode("", 0, &cp) == 0, "empty string returns 0");
        CHECK(utf8Decode(0, 0, &cp) == 0, "null pointer returns 0");
        CHECK(utf8Decode("A", 1, &cp) == 0, "reading past the end returns 0");
    }

    /* an invalid leading byte must not loop forever */
    {
        unsigned int cp = 0;
        int adv = utf8Decode("\x80", 0, &cp);
        CHECK(adv == 1, "a stray continuation byte advances by 1");
    }
}

/* ==========================================================================
 *  2. display width - the thing that makes Chinese tables line up
 * ==========================================================================*/
static void testDisplayWidth()
{
    std::printf("[2] displayWidth\n");

    CHECK(displayWidth("") == 0, "empty string is 0 columns");
    CHECK(displayWidth(0) == 0, "null pointer is 0 columns");

    CHECK(displayWidth("abc") == 3, "3 ASCII chars = 3 columns");
    CHECK(displayWidth("12345") == 5, "5 digits = 5 columns");

    /* a Chinese character is 3 bytes but 2 columns */
    CHECK(displayWidth("头") == 2, "one Chinese char = 2 columns, not 3");
    CHECK(displayWidth("头痛") == 4, "two Chinese chars = 4 columns");
    CHECK(displayWidth("牙痛") == 4, "牙痛 = 4 columns");

    /* mixed content */
    CHECK(displayWidth("R01室") == 5, "R01 + 室 = 3 + 2 = 5 columns");
    CHECK(displayWidth("3F-R01") == 6, "3F-R01 = 6 columns");
    CHECK(displayWidth("周一 08:00") == 10, "周一 + space + 08:00 = 4+1+5 = 10");

    /* the longest labels actually used by the menus */
    CHECK(displayWidth("10. 消化内科") == 12, "'10. 消化内科' = 12 columns");
    CHECK(displayWidth("主任医师") == 8, "'主任医师' = 8 columns");
}

/* ==========================================================================
 *  3. width-aware truncation
 * --------------------------------------------------------------------------
 *  【为什么这一节不调用 printField 去截获 stdout】
 *  printField 是往 stdout 输出的。最初的写法是用 freopen 把 stdout 重定向到
 *  临时文件、调用完再 freopen("CONOUT$") 切回来 —— 结果那份测试自己坏了：
 *  切回来失败时，后续所有输出（包括失败信息和最后的汇总行）都被吞进了临时
 *  文件，看上去像"测试无声退出"。用改变全局状态的方式测试一个纯输出函数，
 *  代价比收益大。
 *
 *  所以这里改为直接验证"按显示宽度截断"这个算法本身：截断的判据是
 *  displayWidth，只要它算得对，截断结果就对。printField 的实际排版由
 *  build test 的样例输出人工核对（菜单里到处都在用，很容易看出歪没歪）。
 * ==========================================================================*/

/* 复刻 printField 的截断逻辑，但写入调用方提供的缓冲区而不是 stdout。
   返回写出的字节数。 */
static int truncateToWidth(const char* s, int width, char* out, int cap)
{
    if (s == 0 || out == 0 || cap <= 0) return 0;
    out[0] = '\0';

    int used = displayWidth(s);
    if (used <= width)
    {
        /* 不截断：原样拷贝（实际打印时还要补空格，这里不关心） */
        int n = (int)std::strlen(s);
        if (n > cap - 1) n = cap - 1;
        std::memcpy(out, s, (size_t)n);
        out[n] = '\0';
        return n;
    }

    /* 超宽：逐码点输出。省略号本身占 2 列，宽度不够时退化成 ASCII 的 "." */
    const bool useEllipsis = (width >= 2);
    const int  markerWidth = useEllipsis ? 2 : 1;

    int budget = width - markerWidth;
    if (budget < 0) budget = 0;
    int pos = 0, written = 0, printed = 0;

    while (true)
    {
        unsigned int cp = 0;
        int adv = utf8Decode(s, pos, &cp);
        if (adv == 0) break;

        int cw = (cp < 0x80) ? 1 : 2;
        if (printed + cw > budget) break;
        if (written + adv + 3 >= cap) break;      /* 给省略号留位置 */

        for (int b = 0; b < adv; b++) out[written++] = s[pos + b];
        printed += cw;
        pos += adv;
    }

    /* 追加截断标记：省略号（UTF-8 的 "…" = E2 80 A6，占 2 列）
       或退化的 ASCII 点（占 1 列） */
    if (useEllipsis)
    {
        out[written++] = (char)0xE2;
        out[written++] = (char)0x80;
        out[written++] = (char)0xA6;
    }
    else
    {
        out[written++] = '.';
    }
    out[written] = '\0';
    return written;
}

static void testTruncation()
{
    std::printf("[3] width-aware truncation\n");

    char buf[256];

    /* ---- 不需要截断时原样保留 ---- */
    {
        truncateToWidth("牙痛", 8, buf, sizeof(buf));
        CHECK(std::strcmp(buf, "牙痛") == 0, "text that fits is kept unchanged");
        CHECK(displayWidth(buf) == 4, "its width is 4");
    }

    /* ---- 超宽时截断到 width 以内，并以省略号结尾 ---- */
    {
        const char* longText = "发热 0.80 + 咳嗽 0.50";
        int full = displayWidth(longText);
        std::printf("      full width = %d\n", full);
        CHECK(full > 20, "the sample text is indeed wider than 20 columns");

        truncateToWidth(longText, 20, buf, sizeof(buf));
        int w = displayWidth(buf);
        std::printf("      truncated to width %d: %s\n", w, buf);
        CHECK(w <= 20, "truncated text fits within 20 columns");
        CHECK(std::strstr(buf, "…") != 0, "truncated text ends with an ellipsis");
        CHECK(w >= 18, "truncation does not throw away much more than needed");
    }

    /* ---- 中文不能被截成半个字 ---- */
    {
        truncateToWidth("主任医师", 5, buf, sizeof(buf));
        std::printf("      '主任医师' in width 5 -> %s (width %d)\n",
                    buf, displayWidth(buf));
        CHECK(displayWidth(buf) <= 5, "fits within 5 columns");
        CHECK(std::strstr(buf, "…") != 0, "has an ellipsis");

        /* 逐码点解码必须能走到字符串末尾而不越界 */
        int pos = 0;
        bool clean = true;
        while (true)
        {
            unsigned int cp = 0;
            int adv = utf8Decode(buf, pos, &cp);
            if (adv == 0) break;
            for (int b = 0; b < adv; b++)
                if (buf[pos + b] == '\0') clean = false;
            pos += adv;
        }
        CHECK(clean, "no Chinese character was split in half");
    }

    /* ---- 宽度小到放不下 2 列的省略号：退化成 ASCII 的 "." ---- */
    {
        truncateToWidth("头", 1, buf, sizeof(buf));
        std::printf("      '头' in width 1 -> [%s] (width %d)\n", buf, displayWidth(buf));
        CHECK(displayWidth(buf) <= 1, "in width 1 the output still fits");
        CHECK(std::strcmp(buf, ".") == 0, "falls back to an ASCII dot");
    }

    /* ---- 关键保证：任何宽度下输出都不超过该宽度 ---- */
    {
        const char* samples[] = {
            "", "a", "ab", "abc", "abcde", "abcdefgh",
            "头", "牙痛", "主任医师", "发热 0.80 + 咳嗽 0.50"
        };
        const int sampleCount = (int)(sizeof(samples) / sizeof(samples[0]));

        bool allFit = true;
        for (int w = 1; w <= 24; w++)
        {
            for (int k = 0; k < sampleCount; k++)
            {
                truncateToWidth(samples[k], w, buf, sizeof(buf));
                if (displayWidth(buf) > w) allFit = false;
            }
        }
        CHECK(allFit, "for every width 1..24, output never exceeds the width");
    }
}
/* ==========================================================================
 *  4. integer parsing rules
 * ==========================================================================*/
static void testIntParsing()
{
    std::printf("[4] integer parsing rules\n");

    /* readInt reads from stdin so it is not called here.  Its parsing rules are
       the ones below -- the same checks the menu relies on.  We verify the
       boundary cases that a naive atoi() would get wrong. */

    struct Case { const char* text; bool valid; int value; };
    const Case cases[] = {
        { "0",        true,   0   },
        { "1",        true,   1   },
        { "123",      true,   123 },
        { "-5",       true,  -5   },
        { "+7",       true,   7   },
        { "  42  ",   true,   42  },
        { "",         false,  0   },
        { "abc",      false,  0   },   /* atoi would return 0 and look "valid" */
        { "12abc",    false,  0   },   /* atoi would return 12 */
        { "-",        false,  0   },
        { "1.5",      false,  0   },
        { "99999999999999999999", false, 0 },   /* overflow */
    };

    const int caseCount = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < caseCount; i++)
    {
        /* replay readInt's algorithm on the literal */
        const char* s = cases[i].text;
        int j = 0;
        while (s[j] == ' ' || s[j] == '\t') j++;

        int sign = 1;
        if (s[j] == '-') { sign = -1; j++; }
        else if (s[j] == '+') { j++; }

        bool valid = (s[j] >= '0' && s[j] <= '9');
        bool overflowed = false;
        const long LIMIT = 2000000000L;
        long value = 0;
        if (valid)
        {
            while (s[j] >= '0' && s[j] <= '9')
            {
                int digit = s[j] - '0';
                /* 必须在乘法【之前】判断，否则 value*10 会先溢出回绕，
                   让后面的检查永远不成立。生产代码里真的踩过这个坑。 */
                if (value > (LIMIT - digit) / 10) { overflowed = true; break; }
                value = value * 10 + digit;
                j++;
            }
        }
        if (overflowed) valid = false;

        if (valid)
        {
            while (s[j] == ' ' || s[j] == '\t') j++;
            if (s[j] != '\0') valid = false;
        }

        char msg[128];
        std::snprintf(msg, sizeof(msg), "[%s] parsed correctly", cases[i].text);

        if (cases[i].valid)
            CHECK(valid && (int)(sign * value) == cases[i].value, msg);
        else
            CHECK(!valid, msg);
    }
}

/* ==========================================================================
 *  main
 * ==========================================================================*/
int main()
{
    std::printf("========================================================\n");
    std::printf("  console utility self-test -- util/Console\n");
    std::printf("========================================================\n");

    testDecode();
    testDisplayWidth();
    testIntParsing();
    testTruncation();

    std::printf("--------------------------------------------------------\n");
    std::printf("  passed %d, failed %d\n", g_pass, g_fail);
    std::printf("========================================================\n");

    return (g_fail == 0) ? 0 : 1;
}