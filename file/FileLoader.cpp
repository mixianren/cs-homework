/* ============================================================================
 *  file/FileLoader.cpp  --  文本数据文件加载器实现
 * ----------------------------------------------------------------------------
 *  声明见 file/FileLoader.h。
 *
 *  读取方式：一次读一行到固定长度的字符数组，不依赖 std::string 或
 *  std::getline，保持"不用标准类库"的约束。
 * ==========================================================================*/

#include "file/FileLoader.h"
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cctype>

/* ==========================================================================
 *  LineParser
 * ==========================================================================*/

void LineParser::trim(char* s)
{
    if (s == 0) return;

    int len = (int)std::strlen(s);

    /* 去掉尾部空白 */
    while (len > 0 && (s[len - 1] == ' '  || s[len - 1] == '\t' ||
                       s[len - 1] == '\r' || s[len - 1] == '\n'))
    {
        s[len - 1] = '\0';
        len--;
    }

    /* 去掉首部空白：把后面的内容整体前移 */
    int start = 0;
    while (s[start] == ' ' || s[start] == '\t') start++;

    if (start > 0)
    {
        int i = 0;
        while (s[start + i] != '\0')
        {
            s[i] = s[start + i];
            i++;
        }
        s[i] = '\0';
    }
}

int LineParser::compare(const char* a, const char* b)
{
    if (a == 0 || b == 0) return (a == b) ? 0 : 1;

    int i = 0;
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i]) return (a[i] < b[i]) ? -1 : 1;
        i++;
    }
    if (a[i] == b[i]) return 0;
    return (a[i] == '\0') ? -1 : 1;
}

int LineParser::toInt(const char* s, int dflt)
{
    if (s == 0) return dflt;

    int i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;

    int sign = 1;
    if (s[i] == '-') { sign = -1; i++; }
    else if (s[i] == '+') { i++; }

    if (s[i] < '0' || s[i] > '9') return dflt;

    long value = 0;
    while (s[i] >= '0' && s[i] <= '9')
    {
        value = value * 10 + (s[i] - '0');
        i++;
    }
    return (int)(sign * value);
}

double LineParser::toDouble(const char* s, double dflt)
{
    if (s == 0) return dflt;

    int i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;

    int sign = 1;
    if (s[i] == '-') { sign = -1; i++; }
    else if (s[i] == '+') { i++; }

    bool anyDigit = false;
    double value = 0.0;

    while (s[i] >= '0' && s[i] <= '9')
    {
        value = value * 10.0 + (s[i] - '0');
        i++;
        anyDigit = true;
    }

    if (s[i] == '.')
    {
        i++;
        double scale = 0.1;
        while (s[i] >= '0' && s[i] <= '9')
        {
            value += (s[i] - '0') * scale;
            scale *= 0.1;
            i++;
            anyDigit = true;
        }
    }

    if (!anyDigit) return dflt;
    return sign * value;
}

int LineParser::split(const char* line, char fields[][MAX_FIELD_LEN],
                      int maxFields, char delim)
{
    if (line == 0 || fields == 0 || maxFields <= 0) return 0;

    /* 跳过前导空白 */
    int p = 0;
    while (line[p] == ' ' || line[p] == '\t') p++;

    /* 空行或以 '#' 开头的整行注释：视为没有数据 */
    if (line[p] == '\0' || line[p] == '#' ||
        line[p] == '\r' || line[p] == '\n')
        return 0;

    int  count       = 0;
    int  fieldPos    = 0;
    bool inQuote     = false;
    bool fieldExists = false;   /* 当前字段是否已经有内容 */
    bool afterDelim  = false;   /* 上一个字符是否是分隔符 */

    for (int i = p; line[i] != '\0'; i++)
    {
        char c = line[i];

        if (c == '\r' || c == '\n') break;              /* 行尾 */
        if (c == '"') { inQuote = !inQuote; fieldExists = true; continue; }

        /* 不在引号内的 '#' 是行内注释：到此处为止，本行数据结束。
           必须把 afterDelim 清掉，否则 "1|2|# 注释" 会被误判成
           "尾随分隔符划出的空字段"，多算一个字段出来。 */
        if (c == '#' && !inQuote) { afterDelim = false; break; }

        if (c == delim && !inQuote)
        {
            fields[count][fieldPos] = '\0';
            trim(fields[count]);
            count++;
            fieldExists = false;
            afterDelim  = true;
            fieldPos    = 0;

            if (count >= maxFields) return count;       /* 字段过多，截断 */
        }
        else
        {
            if (fieldPos < MAX_FIELD_LEN - 1)
                fields[count][fieldPos++] = c;
            if (c != ' ' && c != '\t') fieldExists = true;
            afterDelim = false;
        }
    }

    /* 收尾：把最后一个字段写出来。
       计入的条件是"它有内容"或"它是由尾随分隔符明确划出的空字段"。
       例："a|b"      -> 2 个字段
           "a|b|"     -> 3 个字段（尾随分隔符产生一个空字段）
           "a|b|#注释" -> 2 个字段（注释不算字段） */
    fields[count][fieldPos] = '\0';
    trim(fields[count]);

    if (fieldExists || afterDelim || count == 0)
        count++;

    return count;
}

int LineParser::readLines(const char* path, char lines[][MAX_FIELD_LEN],
                          int maxLines)
{
    if (path == 0 || lines == 0 || maxLines <= 0) return -1;

    std::FILE* fp = std::fopen(path, "rb");
    if (fp == 0) return -1;

    int  count    = 0;
    int  pos      = 0;
    int  c;

    /* 手工按字节读取并切行，这样能统一处理 CRLF / LF / CR 三种换行，
       也不受行长缓冲区上限的影响（超长行会被截断而不是越界）。 */
    while ((c = std::fgetc(fp)) != EOF)
    {
        if (c == '\n')
        {
            lines[count][pos] = '\0';
            count++;
            pos = 0;
            if (count >= maxLines) break;
        }
        else if (c == '\r')
        {
            lines[count][pos] = '\0';
            count++;
            pos = 0;
            if (count >= maxLines) break;

            int next = std::fgetc(fp);
            if (next != '\n' && next != EOF) std::ungetc(next, fp);
        }
        else
        {
            if (pos < MAX_FIELD_LEN - 1) lines[count][pos++] = (char)c;
        }
    }

    /* 末行可能没有换行符 */
    if (pos > 0 && count < maxLines)
    {
        lines[count][pos] = '\0';
        count++;
    }

    std::fclose(fp);
    return count;
}

/* ==========================================================================
 *  LoadReport
 * ==========================================================================*/

LoadReport::LoadReport()
{
    reset();
}

void LoadReport::reset()
{
    filesRead    = 0;
    filesMissing = 0;
    rowsLoaded   = 0;
    rowsSkipped  = 0;
    warnings     = 0;
    m_warningCount = 0;
    for (int i = 0; i < MAX_WARNINGS; i++) m_warnings[i][0] = '\0';
}

void LoadReport::warn(const char* fmt, ...)
{
    warnings++;

    if (m_warningCount >= MAX_WARNINGS) return;   /* 只保留前若干条 */

    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(m_warnings[m_warningCount], WARN_TEXT_LEN, fmt, ap);
    va_end(ap);

    m_warningCount++;
}

const char* LoadReport::warningAt(int i) const
{
    if (i < 0 || i >= m_warningCount) return "";
    return m_warnings[i];
}

void LoadReport::print() const
{
    std::printf("----------------------------------------------------------------------\n");
    std::printf("  数据加载报告\n");
    std::printf("----------------------------------------------------------------------\n");
    std::printf("  文件：成功 %d 个，缺失 %d 个\n", filesRead, filesMissing);
    std::printf("  数据行：成功 %d 行，跳过 %d 行\n", rowsLoaded, rowsSkipped);
    std::printf("  告警：%d 条\n", warnings);

    for (int i = 0; i < m_warningCount; i++)
        std::printf("    - %s\n", m_warnings[i]);

    if (warnings > m_warningCount)
        std::printf("    ...（其余 %d 条已省略）\n", warnings - m_warningCount);
}

/* ==========================================================================
 *  FileLoader
 * ==========================================================================*/

FileLoader::FileLoader(const char* dataDir)
{
    if (dataDir == 0 || dataDir[0] == '\0') dataDir = "files";
    std::snprintf(m_dataDir, sizeof(m_dataDir), "%s", dataDir);
}

void FileLoader::makePath(char* out, int cap, const char* fileName) const
{
    std::snprintf(out, (size_t)cap, "%s/%s", m_dataDir, fileName);
}

int FileLoader::countDataLines(const char* path)
{
    std::FILE* fp = std::fopen(path, "rb");
    if (fp == 0) return -1;

    int count = 0;
    int c;
    int firstChar = 1;
    bool lineHasContent = false;

    while ((c = std::fgetc(fp)) != EOF)
    {
        if (c == '\n')
        {
            if (lineHasContent) count++;
            lineHasContent = false;
            firstChar = 1;
        }
        else if (c == '\r')
        {
            if (lineHasContent) count++;
            lineHasContent = false;
            firstChar = 1;
        }
        else
        {
            if (firstChar)
            {
                firstChar = 0;
                /* 以 '#' 开头的行是注释，不算数据行 */
                if (c != '#' && c != ' ' && c != '\t') lineHasContent = true;
            }
        }
    }
    if (lineHasContent) count++;

    std::fclose(fp);
    return count;
}

/* ------------------------------------------------------------------ 症状 */
int FileLoader::loadSymptoms(DataStore* store)
{
    if (store == 0) return -1;

    char path[512];
    makePath(path, sizeof(path), "symptoms.txt");

    char lines[LineParser::MAX_LINES][LineParser::MAX_FIELD_LEN];
    int lineCount = LineParser::readLines(path, lines, LineParser::MAX_LINES);
    if (lineCount < 0)
    {
        m_report.filesMissing++;
        m_report.warn("symptoms.txt 缺失，智能导诊将无法识别症状");
        return -1;
    }
    m_report.filesRead++;

    int loaded = 0, skipped = 0;
    for (int i = 0; i < lineCount; i++)
    {
        char fields[LineParser::MAX_FIELDS][LineParser::MAX_FIELD_LEN];
        int n = LineParser::split(lines[i], fields, LineParser::MAX_FIELDS);
        if (n == 0) { continue; }                     /* 注释或空行 */

        if (n < 2 || fields[0][0] == '\0')
        {
            skipped++;
            m_report.warn("symptoms.txt 第 %d 行字段不足，已跳过", i + 1);
            continue;
        }

        Symptom s;
        s.init();
        s.index = loaded;                             /* 下标 = 出现顺序 */
        std::snprintf(s.name, NAME_LEN, "%s", fields[0]);
        s.bodyPart = LineParser::toInt(fields[1], 0);

        store->addSymptom(s);
        loaded++;
    }

    m_report.rowsLoaded += loaded;
    m_report.rowsSkipped += skipped;
    return loaded;
}

/* ------------------------------------------------------------------ 科室 */
int FileLoader::loadDepartments(DataStore* store)
{
    if (store == 0) return -1;

    char path[512];
    makePath(path, sizeof(path), "departments.txt");

    char lines[LineParser::MAX_LINES][LineParser::MAX_FIELD_LEN];
    int lineCount = LineParser::readLines(path, lines, LineParser::MAX_LINES);
    if (lineCount < 0)
    {
        m_report.filesMissing++;
        m_report.warn("departments.txt 缺失，程序无法运行");
        return -1;
    }
    m_report.filesRead++;

    int loaded = 0, skipped = 0;
    for (int i = 0; i < lineCount; i++)
    {
        char fields[LineParser::MAX_FIELDS][LineParser::MAX_FIELD_LEN];
        int n = LineParser::split(lines[i], fields, LineParser::MAX_FIELDS);
        if (n == 0) continue;

        if (n < 2 || fields[0][0] == '\0' || fields[1][0] == '\0')
        {
            skipped++;
            m_report.warn("departments.txt 第 %d 行缺少编号或名称，已跳过", i + 1);
            continue;
        }

        Department d;
        d.init();
        d.id = LineParser::toInt(fields[0], -1);
        std::snprintf(d.name, NAME_LEN, "%s", fields[1]);
        if (n > 2) d.floor = LineParser::toInt(fields[2], 1);
        if (n > 3) std::snprintf(d.room, ROOM_LEN, "%s", fields[3]);
        if (n > 4) std::snprintf(d.location, TEXT_LEN, "%s", fields[4]);
        /* 第 6 列是"科室说明"（一句话介绍该科室）。
           Department 里没有单独字段保存它，界面上也用不到，因此忽略。 */

        if (d.id < 0)
        {
            skipped++;
            m_report.warn("departments.txt 第 %d 行科室编号非法，已跳过", i + 1);
            continue;
        }
        if (store->findDepartment(d.id) != 0)
        {
            skipped++;
            m_report.warn("departments.txt 第 %d 行科室编号 %d 重复，已跳过", i + 1, d.id);
            continue;
        }

        store->addDepartment(d);
        loaded++;
    }

    m_report.rowsLoaded += loaded;
    m_report.rowsSkipped += skipped;
    return loaded;
}

/* ------------------------------------------------------------------ 医生 */
int FileLoader::loadDoctors(DataStore* store)
{
    if (store == 0) return -1;

    char path[512];
    makePath(path, sizeof(path), "doctors.txt");

    char lines[LineParser::MAX_LINES][LineParser::MAX_FIELD_LEN];
    int lineCount = LineParser::readLines(path, lines, LineParser::MAX_LINES);
    if (lineCount < 0)
    {
        m_report.filesMissing++;
        m_report.warn("doctors.txt 缺失，程序无法运行");
        return -1;
    }
    m_report.filesRead++;

    int loaded = 0, skipped = 0;
    for (int i = 0; i < lineCount; i++)
    {
        char fields[LineParser::MAX_FIELDS][LineParser::MAX_FIELD_LEN];
        int n = LineParser::split(lines[i], fields, LineParser::MAX_FIELDS);
        if (n == 0) continue;

        if (n < 4 || fields[0][0] == '\0' || fields[1][0] == '\0')
        {
            skipped++;
            m_report.warn("doctors.txt 第 %d 行字段不足，已跳过", i + 1);
            continue;
        }

        Doctor d;
        d.init();
        d.id = LineParser::toInt(fields[0], -1);
        std::snprintf(d.name, NAME_LEN, "%s", fields[1]);

        /* 第 3 列写的是科室名称，要按名字查出科室编号。
           也允许直接写编号，两种写法都支持。 */
        int deptId = store->departmentIdByName(fields[2]);
        if (deptId < 0) deptId = LineParser::toInt(fields[2], -1);
        d.deptId = deptId;

        d.title = LineParser::toInt(fields[3], TITLE_ATTENDING);
        if (n > 4) std::snprintf(d.goodAt, TEXT_LEN, "%s", fields[4]);
        if (n > 5) d.rating = LineParser::toDouble(fields[5], 0.0);
        if (n > 7) std::snprintf(d.roomId, ROOM_LEN, "%s", fields[7]);

        /* ---- 逐项校验 ---- */
        if (d.id < 0)
        {
            skipped++;
            m_report.warn("doctors.txt 第 %d 行医生编号非法，已跳过", i + 1);
            continue;
        }
        if (store->findDoctor(d.id) != 0)
        {
            skipped++;
            m_report.warn("doctors.txt 第 %d 行医生编号 %d 重复，已跳过", i + 1, d.id);
            continue;
        }
        if (d.deptId < 0 || store->findDepartment(d.deptId) == 0)
        {
            skipped++;
            m_report.warn("doctors.txt 第 %d 行医生 %s 的科室 '%s' 不存在，已跳过",
                          i + 1, d.name, fields[2]);
            continue;
        }
        if (d.title < TITLE_RESIDENT || d.title > TITLE_CHIEF)
        {
            m_report.warn("doctors.txt 第 %d 行医生 %s 职称 %d 非法，按主治医师处理",
                          i + 1, d.name, d.title);
            d.title = TITLE_ATTENDING;
        }
        if (d.rating < 0.0) d.rating = 0.0;
        if (d.rating > 5.0) d.rating = 5.0;

        store->addDoctor(d);
        loaded++;
    }

    m_report.rowsLoaded += loaded;
    m_report.rowsSkipped += skipped;
    return loaded;
}

/* ------------------------------------------------------------------ 排班 */
int FileLoader::loadSchedules(DataStore* store)
{
    if (store == 0) return -1;

    char path[512];
    makePath(path, sizeof(path), "schedules.txt");

    char lines[LineParser::MAX_LINES][LineParser::MAX_FIELD_LEN];
    int lineCount = LineParser::readLines(path, lines, LineParser::MAX_LINES);
    if (lineCount < 0)
    {
        m_report.filesMissing++;
        m_report.warn("schedules.txt 缺失，将无法预约（没有号源）");
        return -1;
    }
    m_report.filesRead++;

    int loaded = 0, skipped = 0;
    for (int i = 0; i < lineCount; i++)
    {
        char fields[LineParser::MAX_FIELDS][LineParser::MAX_FIELD_LEN];
        int n = LineParser::split(lines[i], fields, LineParser::MAX_FIELDS);
        if (n == 0) continue;

        if (n < 5)
        {
            skipped++;
            m_report.warn("schedules.txt 第 %d 行字段不足，已跳过", i + 1);
            continue;
        }

        Schedule s;
        s.init();
        s.doctorId = LineParser::toInt(fields[0], -1);
        s.day      = LineParser::toInt(fields[1], -1);
        s.slot     = LineParser::toInt(fields[2], -1);
        s.capacity = LineParser::toInt(fields[3], 0);
        s.booked   = LineParser::toInt(fields[4], 0);

        /* ---- 校验 ---- */
        if (store->findDoctor(s.doctorId) == 0)
        {
            skipped++;
            m_report.warn("schedules.txt 第 %d 行医生 %d 不存在，已跳过", i + 1, s.doctorId);
            continue;
        }
        if (s.day < 0 || s.day > 6)
        {
            skipped++;
            m_report.warn("schedules.txt 第 %d 行星期 %d 越界（应为 0..6），已跳过",
                          i + 1, s.day);
            continue;
        }
        if (s.slot < 1 || s.slot > SLOT_COUNT)
        {
            skipped++;
            m_report.warn("schedules.txt 第 %d 行时段 %d 越界（应为 1..%d），已跳过",
                          i + 1, s.slot, SLOT_COUNT);
            continue;
        }
        if (s.capacity < 0) s.capacity = 0;
        if (s.booked < 0) s.booked = 0;
        if (s.booked > s.capacity)
        {
            m_report.warn("schedules.txt 第 %d 行已预约 %d 超过上限 %d，已修正为 %d",
                          i + 1, s.booked, s.capacity, s.capacity);
            s.booked = s.capacity;
        }
        if (store->findScheduleByKey(s.doctorId, s.day, s.slot) != 0)
        {
            skipped++;
            m_report.warn("schedules.txt 第 %d 行排班重复（医生 %d %s%s），已跳过",
                          i + 1, s.doctorId, weekdayName(s.day), slotName(s.slot));
            continue;
        }

        store->addSchedule(s);
        loaded++;
    }

    m_report.rowsLoaded += loaded;
    m_report.rowsSkipped += skipped;
    return loaded;
}

/* ------------------------------------------------------------------ 患者 */
int FileLoader::loadPatients(DataStore* store)
{
    if (store == 0) return -1;

    char path[512];
    makePath(path, sizeof(path), "patients.txt");

    char lines[LineParser::MAX_LINES][LineParser::MAX_FIELD_LEN];
    int lineCount = LineParser::readLines(path, lines, LineParser::MAX_LINES);
    if (lineCount < 0)
    {
        m_report.filesMissing++;
        m_report.warn("patients.txt 缺失，将无法按患者查询预约");
        return -1;
    }
    m_report.filesRead++;

    int loaded = 0, skipped = 0;
    for (int i = 0; i < lineCount; i++)
    {
        char fields[LineParser::MAX_FIELDS][LineParser::MAX_FIELD_LEN];
        int n = LineParser::split(lines[i], fields, LineParser::MAX_FIELDS);
        if (n == 0) continue;

        if (n < 2 || fields[0][0] == '\0')
        {
            skipped++;
            m_report.warn("patients.txt 第 %d 行字段不足，已跳过", i + 1);
            continue;
        }

        Patient p;
        p.init();
        p.id = LineParser::toInt(fields[0], -1);
        std::snprintf(p.name, NAME_LEN, "%s", fields[1]);
        if (n > 2) std::snprintf(p.phone, PHONE_LEN, "%s", fields[2]);

        if (p.id < 0)
        {
            skipped++;
            m_report.warn("patients.txt 第 %d 行患者编号非法，已跳过", i + 1);
            continue;
        }
        if (store->findPatient(p.id) != 0)
        {
            skipped++;
            m_report.warn("patients.txt 第 %d 行患者编号 %d 重复，已跳过", i + 1, p.id);
            continue;
        }

        store->addPatient(p);
        loaded++;
    }

    m_report.rowsLoaded += loaded;
    m_report.rowsSkipped += skipped;
    return loaded;
}

/* -------------------------------------------------------- 症状-科室关联 */
int FileLoader::loadSymptomDept(DataStore* store)
{
    if (store == 0) return -1;

    char path[512];
    makePath(path, sizeof(path), "symptom_dept.txt");

    char lines[LineParser::MAX_LINES][LineParser::MAX_FIELD_LEN];
    int lineCount = LineParser::readLines(path, lines, LineParser::MAX_LINES);
    if (lineCount < 0)
    {
        m_report.filesMissing++;
        m_report.warn("symptom_dept.txt 缺失，智能导诊将无法给出推荐结果");
        return -1;
    }
    m_report.filesRead++;

    int edges = 0, skipped = 0;
    for (int i = 0; i < lineCount; i++)
    {
        char fields[LineParser::MAX_FIELDS][LineParser::MAX_FIELD_LEN];
        int n = LineParser::split(lines[i], fields, LineParser::MAX_FIELDS);
        if (n == 0) continue;

        if (n < 2)
        {
            skipped++;
            m_report.warn("symptom_dept.txt 第 %d 行字段不足，已跳过", i + 1);
            continue;
        }

        /* 第 1 列是症状名 */
        int symptomIndex = store->symptomIndexByName(fields[0]);
        if (symptomIndex < 0)
        {
            skipped++;
            m_report.warn("symptom_dept.txt 第 %d 行症状 '%s' 不在症状字典中，已跳过",
                          i + 1, fields[0]);
            continue;
        }

        /* 后面各列形如 "科室编号:权重"，一个症状可以关联多个科室 */
        for (int k = 1; k < n; k++)
        {
            if (fields[k][0] == '\0') continue;

            /* 找到冒号，切开"科室编号"与"权重" */
            int colon = -1;
            for (int c = 0; fields[k][c] != '\0'; c++)
            {
                if (fields[k][c] == ':') { colon = c; break; }
            }
            if (colon <= 0)
            {
                skipped++;
                m_report.warn("symptom_dept.txt 第 %d 行 '%s' 格式应为 科室:权重，已跳过",
                              i + 1, fields[k]);
                continue;
            }

            char deptPart[64];
            char weightPart[64];
            int ci = 0;
            for (int c = 0; c < colon && ci < 63; c++) deptPart[ci++] = fields[k][c];
            deptPart[ci] = '\0';
            ci = 0;
            for (int c = colon + 1; fields[k][c] != '\0' && ci < 63; c++)
                weightPart[ci++] = fields[k][c];
            weightPart[ci] = '\0';

            LineParser::trim(deptPart);
            LineParser::trim(weightPart);

            /* 科室可以写编号，也可以写名称，两种都支持 */
            int deptId = store->departmentIdByName(deptPart);
            if (deptId < 0) deptId = LineParser::toInt(deptPart, -1);

            if (deptId < 0 || store->findDepartment(deptId) == 0)
            {
                skipped++;
                m_report.warn("symptom_dept.txt 第 %d 行科室 '%s' 不存在，已跳过",
                              i + 1, deptPart);
                continue;
            }

            double weight = LineParser::toDouble(weightPart, -1.0);
            if (weight <= 0.0 || weight > 1.0)
            {
                skipped++;
                m_report.warn("symptom_dept.txt 第 %d 行权重 '%s' 应在 (0,1]，已跳过",
                              i + 1, weightPart);
                continue;
            }

            store->addSymptomDept(symptomIndex, deptId, weight);
            edges++;
        }
    }

    m_report.rowsLoaded += (lineCount - skipped > 0 ? lineCount - skipped : 0);
    m_report.rowsSkipped += skipped;
    return edges;
}

/* ------------------------------------------------------------------ 总入口 */
bool FileLoader::loadAll(DataStore* store)
{
    if (store == 0) return false;

    m_report.reset();
    store->clear();

    /* 先按预期规模预留容量，让加载过程中顺序表不再扩容。
       数不准也没关系，只是少一次扩容。 */
    char path[512];
    makePath(path, sizeof(path), "departments.txt");
    int deptHint   = countDataLines(path);
    makePath(path, sizeof(path), "doctors.txt");
    int doctorHint = countDataLines(path);
    makePath(path, sizeof(path), "symptoms.txt");
    int symptomHint = countDataLines(path);
    makePath(path, sizeof(path), "schedules.txt");
    int schedHint  = countDataLines(path);
    makePath(path, sizeof(path), "patients.txt");
    int patHint    = countDataLines(path);

    store->reserveFor(deptHint   > 0 ? deptHint   : 16,
                      doctorHint > 0 ? doctorHint : 48,
                      symptomHint > 0 ? symptomHint : 64,
                      schedHint  > 0 ? schedHint  : 128,
                      patHint    > 0 ? patHint    : 16,
                      0);

    /* 加载顺序有依赖：症状 -> 科室 -> 医生 -> 排班 -> 患者 -> 预约 -> 关联 */
    int symptomCount = loadSymptoms(store);
    int deptCount    = loadDepartments(store);
    int doctorCount  = loadDoctors(store);
    loadSchedules(store);
    loadPatients(store);
    int edgeCount    = loadSymptomDept(store);

    std::printf("----------------------------------------------------------------------\n");
    std::printf("  读取完毕：症状 %d 条，科室 %d 个，医生 %d 人，关联边 %d 条\n",
                symptomCount  > 0 ? symptomCount  : 0,
                deptCount     > 0 ? deptCount     : 0,
                doctorCount   > 0 ? doctorCount   : 0,
                edgeCount     > 0 ? edgeCount     : 0);

    /* 收尾检查：科室表或医生表为空则不能继续 */
    char problems[1024];
    bool ok = store->finishLoading(problems, sizeof(problems));

    if (!ok)
    {
        std::printf("  致命问题：%s", problems);
        return false;
    }
    if (problems[0] != '\0')
    {
        std::printf("  数据完整性提示：\n");
        std::printf("    %s", problems);
    }

    return true;
}
