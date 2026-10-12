/* ============================================================================
 *  util/Console.cpp  --  控制台输入输出工具实现
 * ----------------------------------------------------------------------------
 *  声明见 util/Console.h。
 * ==========================================================================*/

#include "util/Console.h"
#include <cstdio>
#include <cstring>

/* ==========================================================================
 *  UTF-8 码点解码
 * ==========================================================================*/
int utf8Decode(const char* s, int pos, unsigned int* cpOut)
{
    if (s == 0) { if (cpOut) *cpOut = 0; return 0; }

    unsigned char c0 = (unsigned char)s[pos];
    if (c0 == 0) { if (cpOut) *cpOut = 0; return 0; }

    /* 单字节：ASCII */
    if (c0 < 0x80) { if (cpOut) *cpOut = c0; return 1; }

    /* 两字节：110xxxxx 10xxxxxx */
    if ((c0 & 0xE0) == 0xC0)
    {
        unsigned char c1 = (unsigned char)s[pos + 1];
        if ((c1 & 0xC0) != 0x80) { if (cpOut) *cpOut = c0; return 1; }
        if (cpOut) *cpOut = ((unsigned int)(c0 & 0x1F) << 6) | (c1 & 0x3F);
        return 2;
    }

    /* 三字节：1110xxxx 10xxxxxx 10xxxxxx —— 常用汉字在这一档 */
    if ((c0 & 0xF0) == 0xE0)
    {
        unsigned char c1 = (unsigned char)s[pos + 1];
        unsigned char c2 = (unsigned char)s[pos + 2];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80)
        {
            if (cpOut) *cpOut = c0;
            return 1;
        }
        if (cpOut)
            *cpOut = ((unsigned int)(c0 & 0x0F) << 12)
                   | ((unsigned int)(c1 & 0x3F) << 6)
                   | (c2 & 0x3F);
        return 3;
    }

    /* 四字节：11110xxx 10xxxxxx 10xxxxxx 10xxxxxx —— 生僻字 */
    if ((c0 & 0xF8) == 0xF0)
    {
        unsigned char c1 = (unsigned char)s[pos + 1];
        unsigned char c2 = (unsigned char)s[pos + 2];
        unsigned char c3 = (unsigned char)s[pos + 3];
        if ((c1 & 0xC0) != 0x80 || (c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80)
        {
            if (cpOut) *cpOut = c0;
            return 1;
        }
        if (cpOut)
            *cpOut = ((unsigned int)(c0 & 0x07) << 18)
                   | ((unsigned int)(c1 & 0x3F) << 12)
                   | ((unsigned int)(c2 & 0x3F) << 6)
                   | (c3 & 0x3F);
        return 4;
    }

    /* 非法首字节（如单独的续接字节） */
    if (cpOut) *cpOut = c0;
    return 1;
}

/* ==========================================================================
 *  显示宽度
 * ==========================================================================*/
int displayWidth(const char* s)
{
    if (s == 0) return 0;

    int width = 0;
    int pos   = 0;
    while (true)
    {
        unsigned int cp = 0;
        int adv = utf8Decode(s, pos, &cp);
        if (adv == 0) break;
        width += (cp < 0x80) ? 1 : 2;
        pos += adv;
    }
    return width;
}

/* ==========================================================================
 *  按宽度打印字段
 * ==========================================================================*/
void printField(const char* s, int width)
{
    if (s == 0) s = "";

    int used = displayWidth(s);
    if (used <= width)
    {
        std::printf("%s", s);
        for (int i = used; i < width; i++) std::printf(" ");
        return;
    }

    /* 超宽：逐码点输出，最后追加截断标记。
       【注意】省略号 "…" 本身占 2 个显示列，不是 1 个。所以能留给正文的
       宽度是 width - 2。这里最初写成了 -1，结果是截断后总比目标宽 1 列，
       表格边框依然会歪 —— 而且因为只歪 1 列，肉眼很容易放过。自检里
       "截断后宽度不得超过目标"这条断言把它抓了出来。
       宽度小到放不下 2 列的省略号时，退化成 ASCII 的 "."（占 1 列），
       这样"输出永不超过 width"这个保证在任何宽度下都成立。 */
    const bool useEllipsis = (width >= 2);
    const int  markerWidth = useEllipsis ? 2 : 1;

    int budget  = width - markerWidth;
    if (budget < 0) budget = 0;

    int pos     = 0;
    int printed = 0;

    while (true)
    {
        unsigned int cp = 0;
        int adv = utf8Decode(s, pos, &cp);
        if (adv == 0) break;

        int cw = (cp < 0x80) ? 1 : 2;
        if (printed + cw > budget) break;

        for (int b = 0; b < adv; b++) std::printf("%c", s[pos + b]);
        printed += cw;
        pos += adv;
    }

    std::printf("%s", useEllipsis ? "…" : ".");

    /* 补齐到目标宽度 */
    for (int i = printed + markerWidth; i < width; i++) std::printf(" ");
}

/* ==========================================================================
 *  分隔线
 * ==========================================================================*/
void printSeparator(const int* widths, int count)
{
    if (widths == 0 || count <= 0) return;

    std::printf("  +");
    for (int c = 0; c < count; c++)
    {
        for (int i = 0; i < widths[c] + 2; i++) std::printf("-");
        std::printf("+");
    }
    std::printf("\n");
}

void printRule(int width)
{
    if (width < 1) width = 1;
    for (int i = 0; i < width; i++) std::printf("-");
    std::printf("\n");
}

/* ==========================================================================
 *  读一行
 * ==========================================================================*/
bool readLine(char* buf, int cap)
{
    if (buf == 0 || cap <= 0) return false;

    buf[0] = '\0';
    if (std::fgets(buf, cap, stdin) == 0) return false;   /* EOF */

    /* 去掉行尾的 \n 与 \r */
    int len = (int)std::strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
    {
        buf[len - 1] = '\0';
        len--;
    }

    /* 如果这一行太长没读完，把剩余部分丢弃，避免污染下一次输入 */
    if (len == cap - 1 && buf[len - 1] != '\n')
    {
        int c;
        while ((c = std::getchar()) != '\n' && c != EOF) { }
    }

    return true;
}

/* ==========================================================================
 *  读整数
 * --------------------------------------------------------------------------
 *  不用 std::atoi：它无法区分"输入了 0"和"输入了 abc"—— 两者都返回 0。
 *  这里手动解析并检查是否整串都是数字。
 * ==========================================================================*/
bool readInt(int* out)
{
    char buf[64];
    if (!readLine(buf, sizeof(buf))) return false;

    /* 跳过前导空白 */
    int i = 0;
    while (buf[i] == ' ' || buf[i] == '\t') i++;

    /* 符号 */
    int sign = 1;
    if (buf[i] == '-') { sign = -1; i++; }
    else if (buf[i] == '+') { i++; }

    /* 至少要有一位数字 */
    if (buf[i] < '0' || buf[i] > '9') return false;

    /* ---------------- 解析数字并检测溢出 ----------------
       这里有一个容易写错的地方：**必须在乘法之前判断，而不是乘完再判断**。

       反例（曾经真的写错过）：先 value = value*10 + d，再判断
       value > 2000000000L。Windows/MinGW 上 long 只有 32 位，输入
       "99999999999999999999" 时 value*10 会溢出并【回绕】成一个小正数，
       于是那个判断永远不成立 —— 溢出检测形同虚设，最终把一个垃圾值
       当成合法输入收下。（实测该输入得到 value = 1661992959。）

       正确做法：先看 value 是否已经大于 (上限 - 本位数字) / 10，
       若是则下一步乘 10 必定溢出，直接拒绝。 */
    const long LIMIT = 2000000000L;

    long value = 0;
    while (buf[i] >= '0' && buf[i] <= '9')
    {
        int digit = buf[i] - '0';

        if (value > (LIMIT - digit) / 10) return false;   /* 乘之前先挡 */

        value = value * 10 + digit;
        i++;
    }

    /* 数字后面只允许有空白 */
    while (buf[i] == ' ' || buf[i] == '\t') i++;
    if (buf[i] != '\0') return false;

    if (out != 0) *out = (int)(sign * value);
    return true;
}

bool promptInt(const char* prompt, int* out)
{
    std::printf("%s", (prompt != 0) ? prompt : "");
    std::fflush(stdout);

    if (!readInt(out))
    {
        std::printf("  输入无效，请输入一个整数。\n");
        return false;
    }
    return true;
}

bool promptIntInRange(const char* prompt, int lo, int hi, int* out)
{
    int value = 0;
    if (!promptInt(prompt, &value)) return false;

    if (value < lo || value > hi)
    {
        std::printf("  输入超出范围，请输入 %d ~ %d 之间的整数。\n", lo, hi);
        return false;
    }

    if (out != 0) *out = value;
    return true;
}

bool promptLine(const char* prompt, char* buf, int cap)
{
    std::printf("%s", (prompt != 0) ? prompt : "");
    std::fflush(stdout);
    return readLine(buf, cap);
}

bool promptYesNo(const char* prompt, bool* out)
{
    char buf[32];
    std::printf("%s", (prompt != 0) ? prompt : "");
    std::fflush(stdout);

    if (!readLine(buf, sizeof(buf))) return false;

    if (buf[0] == 'y' || buf[0] == 'Y') { if (out) *out = true;  return true; }
    if (buf[0] == 'n' || buf[0] == 'N') { if (out) *out = false; return true; }

    std::printf("  请输入 y 或 n。\n");
    return false;
}

void pause()
{
    std::printf("\n  按回车继续...");
    std::fflush(stdout);

    char buf[16];
    std::fgets(buf, sizeof(buf), stdin);
}

/* ==========================================================================
 *  标题框
 * ==========================================================================*/
void printTitleBox(const char* title)
{
    if (title == 0) title = "";

    const int INNER = 46;
    int textWidth = displayWidth(title);
    int padLeft   = (INNER - textWidth) / 2;
    if (padLeft < 0) padLeft = 0;

    std::printf("\n  +");
    for (int i = 0; i < INNER + 2; i++) std::printf("=");
    std::printf("+\n");

    std::printf("  | ");
    for (int i = 0; i < padLeft; i++) std::printf(" ");
    std::printf("%s", title);
    int printed = padLeft + textWidth;
    for (int i = printed; i < INNER; i++) std::printf(" ");
    std::printf(" |\n");

    std::printf("  +");
    for (int i = 0; i < INNER + 2; i++) std::printf("=");
    std::printf("+\n");
}
