#ifndef HMS_FILELOADER_H
#define HMS_FILELOADER_H

/* ============================================================================
 *  file/FileLoader.h  --  文本数据文件加载器
 * ----------------------------------------------------------------------------
 *  对应项目框架.md 3.4 节与需求分析.md 第七章。
 *
 *  题目基本要求第一条："测试数据通过文本文件读入"。
 *  本类负责把 files/ 目录下的 7 个文本文件解析进 DataStore。
 *
 *  【统一的文件格式约定】
 *      * 编码 UTF-8，一行一条记录
 *      * 字段之间用 '|' 分隔
 *      * 以 '#' 开头的整行是注释，空行忽略
 *      * 字段两端空白自动去掉
 *
 *  【加载顺序（有依赖，不能打乱）】
 *      1. symptoms.txt       症状字典   —— 后续关联表要按名字查下标
 *      2. departments.txt    科室       —— 医生与关联表要按名字查编号
 *      3. doctors.txt        医生       —— 排班与预约要按编号查医生
 *      4. schedules.txt      排班       —— 建立排班网格
 *      5. patients.txt       患者       —— 预约要按编号查患者
 *      6. appointments.txt   预约       —— 依赖患者与医生
 *      7. symptom_dept.txt   症状-科室关联 —— 依赖症状与科室
 *
 *  【容错原则】
 *      坏数据不中断程序：单行解析失败只跳过该行并记一条告警，
 *      文件缺失只记一条告警。这样演示时不会因为一个字段写错就整个跑不起来。
 *      但致命问题（科室表或医生表为空）会让 loadAll 返回 false。
 * ==========================================================================*/

#include "data/DataStore.h"

/* ==========================================================================
 *  加载报告
 * ==========================================================================*/
class LoadReport
{
public:
    LoadReport();

    int filesRead;          /* 成功读取的文件数 */
    int filesMissing;       /* 缺失的文件数 */
    int rowsLoaded;         /* 成功解析的数据行总数 */
    int rowsSkipped;        /* 被跳过的行数（注释、空行、格式错误） */
    int warnings;           /* 告警条数 */

    /* 记录一条告警。最多保留 40 条，避免刷屏。 */
    void warn(const char* fmt, ...);

    /* 取第 i 条告警，越界返回空串 */
    const char* warningAt(int i) const;

    /* 打印完整报告 */
    void print() const;

    void reset();

private:
    static const int MAX_WARNINGS = 40;
    static const int WARN_TEXT_LEN = 200;
    char m_warnings[MAX_WARNINGS][WARN_TEXT_LEN];
    int  m_warningCount;
};

/* ==========================================================================
 *  文件加载器
 * ==========================================================================*/
class FileLoader
{
public:
    /* dataDir 例如 "files"；留空则用 "." */
    explicit FileLoader(const char* dataDir = "files");

    /* 加载全部文件。
       返回 true 表示关键数据已就绪，可以继续运行；
       返回 false 表示存在致命问题（科室表或医生表为空）。 */
    bool loadAll(DataStore* store);

    const LoadReport& report() const { return m_report; }

    /* 单项加载（便于单独测试某个文件）。
       返回成功解析的行数，文件缺失返回 -1。 */
    int loadSymptoms(DataStore* store);
    int loadDepartments(DataStore* store);
    int loadDoctors(DataStore* store);
    int loadSchedules(DataStore* store);
    int loadPatients(DataStore* store);
    int loadSymptomDept(DataStore* store);

private:
    char        m_dataDir[256];
    LoadReport  m_report;

    /* 组合出完整路径 */
    void makePath(char* out, int cap, const char* fileName) const;

    /* 两遍加载的辅助：先数行数用于预留容量 */
    static int countDataLines(const char* path);
};

/* ==========================================================================
 *  行解析工具
 *  独立成一组静态函数，方便单独测试切分逻辑
 * ==========================================================================*/
class LineParser
{
public:
    static const int MAX_FIELDS = 16;      /* 一行最多 16 个字段（最宽的是医生 8 列） */

    /* 单个字段的最大字节数。
       128 字节约 42 个汉字，足够容纳"位置描述""科室说明"这类内容。 */
    static const int MAX_FIELD_LEN = 128;

    /* 一次最多读入的行数。
       【注意】这个上限是为了控制栈占用：行缓冲区是二维数组，
       256 行 × 128 字节 = 32 KB，已接近单帧可承受的范围。
       本工程最大的数据文件是 schedules.txt（90 行数据 + 1 行注释），
       256 行有足够余量。若以后数据文件超过该行数，需要改成动态分配。 */
    static const int MAX_LINES = 256;

    /* 把一行按 '|' 切分。
       跳过 '#' 注释与空行（此时返回 0）。
       字段两端空白去掉；支持用双引号包裹含 '|' 的字段。
       返回字段个数。 */
    static int split(const char* line, char fields[][MAX_FIELD_LEN],
                     int maxFields, char delim = '|');

    /* 读取文件全部行到内存。返回行数，文件不存在返回 -1。
       调用方提供二维缓冲区，容量至少 maxLines 行。 */
    static int readLines(const char* path, char lines[][MAX_FIELD_LEN],
                         int maxLines);

    /* 字符串转整数；空串或非法输入返回 dflt */
    static int    toInt(const char* s, int dflt);
    /* 字符串转浮点；空串或非法输入返回 dflt */
    static double toDouble(const char* s, double dflt);
    /* 去掉首尾空白（原地修改） */
    static void trim(char* s);
    /* 字符串比较，相等返回 0 */
    static int compare(const char* a, const char* b);
};

#endif /* HMS_FILELOADER_H */
