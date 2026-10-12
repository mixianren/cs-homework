/* ============================================================================
 *  tools/gen_data.cpp  --  test data generator
 * ----------------------------------------------------------------------------
 *  Data Structures Course Design - Problem 1
 *
 *  Purpose
 *  -------
 *  Writes the seven text files in files/ that the main program reads through
 *  file/FileLoader. Keeping the generator in C++ (instead of a scripting
 *  language) means:
 *      * the whole project stays in one language, matching the assignment
 *        requirement to implement everything in an OOP language;
 *      * you can re-run it yourself with the g++ you already have;
 *      * data can be regenerated in seconds if the requirements change
 *        during the defence.
 *
 *  Design rules
 *  ------------
 *  1. Fully deterministic: no random numbers, so re-running always produces
 *     byte-identical output and demo results are reproducible.
 *
 *  2. Size sits on the assignment minimum: 10 departments, 30 doctors.
 *
 *  3. Symptom-to-department weights follow one important rule, otherwise the
 *     triage results come out unreasonable:
 *         broad departments (internal medicine, surgery) get LOW weights
 *         (0.15 - 0.30), narrow specialists get HIGH weights (0.80 - 0.95).
 *     Because triage simply sums the weights of the patient''s symptoms per
 *     department, a broad department tagged with many common symptoms would
 *     otherwise outscore the correct specialist. This fix lives entirely in
 *     the data, not in the code.
 *
 *  Build and run
 *  -------------
 *      g++ -std=c++17 -Wall -I. tools\gen_data.cpp -o bin\gen_data.exe
 *      bin\gen_data.exe              -> writes into .\files
 *      bin\gen_data.exe some\dir     -> writes into the given directory
 *
 *  Note on line endings
 *  --------------------
 *  Files are opened in BINARY mode ("wb") on purpose. In text mode ("w")
 *  Windows silently rewrites every '\n' into '\r\n', so the output would
 *  differ between Windows and Linux and would not be byte-stable. Binary
 *  mode gives exactly the bytes written here, which keeps regenerated data
 *  reproducible and makes the files readable on any platform.
 * ==========================================================================*/

#include <cstdio>
#include <cstring>
#include <cstdarg>      /* va_list / va_start / va_end，供 reportError 变参使用 */
#include <cstdlib>      /* std::system，用于创建输出目录 */

/* ==========================================================================
 *  Compile-time sizes
 * ==========================================================================*/
const int MAX_DEPT      = 32;
const int MAX_DOCTOR    = 128;
const int MAX_SYMPTOM   = 128;
const int MAX_EDGE      = 512;
const int MAX_PATIENT   = 128;
const int MAX_SCHEDULE  = 512;
const int MAX_APPT      = 128;

const int DOCTORS_PER_DEPT = 3;

/* ==========================================================================
 *  1. Departments
 *     Floor layout: 1F public services and dermatology
 *                   2F pediatrics, eye, dental, ENT
 *                   3F internal medicine family
 *                   4F orthopedics and neurology
 * ==========================================================================*/
struct DeptDef
{
    int  id;
    const char* name;
    int  floor;
    const char* block;      /* room block id, e.g. 3F-N01 */
    const char* location;
    const char* desc;
};

const DeptDef g_depts[] = {
    { 1,  "内科",     3, "3F-N01", "门诊楼3层东区", "以药物治疗为主，诊治呼吸、消化、心血管等非手术疾病" },
    { 2,  "外科",     3, "3F-N02", "门诊楼3层东区", "以手术为主要治疗手段，处理创伤、感染、肿块等" },
    { 3,  "儿科",     2, "2F-N01", "门诊楼2层南区", "诊治0-14岁儿童各类疾病" },
    { 4,  "骨科",     4, "4F-N01", "门诊楼4层西区", "诊治骨骼、关节、肌肉、脊柱等运动系统疾病" },
    { 5,  "眼科",     2, "2F-N02", "门诊楼2层北区", "诊治视力、眼压、眼底及眼部炎症等疾病" },
    { 6,  "口腔科",   2, "2F-N03", "门诊楼2层北区", "诊治牙体牙髓、牙周、口腔黏膜疾病" },
    { 7,  "皮肤科",   1, "1F-N01", "门诊楼1层西区", "诊治皮肤病及皮肤附属器官疾病" },
    { 8,  "耳鼻喉科", 2, "2F-N04", "门诊楼2层北区", "诊治耳、鼻、咽喉疾病" },
    { 9,  "神经内科", 4, "4F-N02", "门诊楼4层西区", "诊治头痛、眩晕、脑血管病等神经系统疾病" },
    { 10, "消化内科", 3, "3F-N03", "门诊楼3层西区", "诊治食管、胃、肠、肝、胆、胰疾病" },
};
const int DEPT_COUNT = (int)(sizeof(g_depts) / sizeof(g_depts[0]));

/* ==========================================================================
 *  2. Symptoms
 *     body part code: 1 head/neck  2 chest/abdomen  3 limbs  4 whole body
 *                     5 skin       6 facial organs   7 mental/neuro
 * ==========================================================================*/
struct SymptomDef
{
    const char* name;
    int part;
};

const SymptomDef g_symptoms[] = {
    /* whole body */
    { "发热", 4 }, { "乏力", 4 }, { "盗汗", 4 }, { "食欲不振", 4 }, { "水肿", 4 },
    /* head and neck */
    { "头痛", 1 }, { "头晕", 1 }, { "眩晕", 1 }, { "颈部疼痛", 1 }, { "咽喉疼痛", 1 },
    /* chest and abdomen */
    { "咳嗽", 2 }, { "咳痰", 2 }, { "气促", 2 }, { "胸闷", 2 }, { "胸痛", 2 }, { "心悸", 2 },
    { "腹痛", 2 }, { "腹胀", 2 }, { "腹泻", 2 }, { "便秘", 2 }, { "恶心", 2 }, { "呕吐", 2 },
    { "反酸", 2 }, { "烧心", 2 }, { "黑便", 2 }, { "黄疸", 2 },
    /* limbs */
    { "腰部疼痛", 3 }, { "关节疼痛", 3 }, { "骨折外伤", 3 }, { "四肢麻木", 3 },
    { "下肢无力", 3 }, { "颈部肿块", 3 }, { "运动损伤", 3 },
    /* skin */
    { "皮疹", 5 }, { "瘙痒", 5 }, { "荨麻疹", 5 }, { "痤疮", 5 }, { "脱发", 5 }, { "皮肤溃疡", 5 },
    /* facial organs */
    { "视力下降", 6 }, { "视物模糊", 6 }, { "眼红", 6 }, { "眼干涩", 6 }, { "耳鸣", 6 },
    { "听力下降", 6 }, { "鼻塞", 6 }, { "流鼻涕", 6 }, { "鼻出血", 6 }, { "牙痛", 6 },
    { "牙龈出血", 6 }, { "口腔溃疡", 6 },
    /* mental and neuro */
    { "失眠", 7 }, { "记忆力减退", 7 }, { "肢体抽搐", 7 }, { "面瘫", 7 }, { "晕厥", 7 },
    { "小儿哭闹不止", 7 }, { "喂养困难", 7 }, { "黄疸不退", 7 },
};
const int SYMPTOM_COUNT = (int)(sizeof(g_symptoms) / sizeof(g_symptoms[0]));

/* ==========================================================================
 *  3. Symptom-to-department edges (the weighted bipartite graph)
 *     weight meaning: 0.80-0.95 highly specific
 *                     0.50-0.79 strong hint
 *                     0.30-0.49 moderate hint
 *                     0.15-0.29 weak hint, used for broad departments on purpose
 * ==========================================================================*/
struct Edge { const char* symptom; int dept; double weight; };

const Edge g_edges[] = {
    /* ---- whole-body symptoms: spread weakly over several departments ---- */
    { "发热",     3,  0.80 }, { "发热",     1,  0.25 }, { "发热",     2,  0.20 },
    { "乏力",     1,  0.30 }, { "乏力",     10, 0.20 },
    { "盗汗",     1,  0.45 }, { "盗汗",     2,  0.20 },
    { "食欲不振", 10, 0.50 }, { "食欲不振", 1,  0.20 },
    { "水肿",     1,  0.35 },

    /* ---- head and neck ---- */
    { "头痛",     9,  0.85 }, { "头痛",     1,  0.25 },
    { "头晕",     9,  0.80 }, { "头晕",     1,  0.25 },
    { "眩晕",     9,  0.80 }, { "眩晕",     8,  0.30 },
    { "颈部疼痛", 4,  0.75 }, { "颈部疼痛", 9,  0.25 },
    { "咽喉疼痛", 8,  0.85 }, { "咽喉疼痛", 1,  0.20 },

    /* ---- chest and abdomen ---- */
    { "咳嗽",     1,  0.70 }, { "咳嗽",     3,  0.50 },
    { "咳痰",     1,  0.70 },
    { "气促",     1,  0.65 },
    { "胸闷",     1,  0.60 }, { "胸闷",     2,  0.15 },
    { "胸痛",     1,  0.55 }, { "胸痛",     2,  0.20 },
    { "心悸",     1,  0.65 },
    { "腹痛",     10, 0.55 }, { "腹痛",     2,  0.40 }, { "腹痛",     3,  0.45 },
    { "腹胀",     10, 0.70 },
    { "腹泻",     10, 0.65 }, { "腹泻",     3,  0.50 },
    { "便秘",     10, 0.60 },
    { "恶心",     10, 0.55 }, { "恶心",     1,  0.20 },
    { "呕吐",     10, 0.55 }, { "呕吐",     3,  0.45 },
    { "反酸",     10, 0.85 },
    { "烧心",     10, 0.85 },
    { "黑便",     10, 0.80 }, { "黑便",     2,  0.30 },
    { "黄疸",     10, 0.60 }, { "黄疸",     2,  0.35 },

    /* ---- limbs ---- */
    { "腰部疼痛", 4,  0.80 },
    { "关节疼痛", 4,  0.75 },
    { "骨折外伤", 4,  0.85 }, { "骨折外伤", 2,  0.50 },
    { "四肢麻木", 9,  0.60 }, { "四肢麻木", 4,  0.45 },
    { "下肢无力", 9,  0.60 }, { "下肢无力", 4,  0.45 },
    { "颈部肿块", 2,  0.70 }, { "颈部肿块", 8,  0.40 },
    { "运动损伤", 4,  0.85 },

    /* ---- skin: narrow specialty, high weights ---- */
    { "皮疹",     7,  0.85 }, { "皮疹",     3,  0.30 },
    { "瘙痒",     7,  0.85 },
    { "荨麻疹",   7,  0.90 },
    { "痤疮",     7,  0.90 },
    { "脱发",     7,  0.85 },
    { "皮肤溃疡", 7,  0.65 }, { "皮肤溃疡", 2,  0.40 },

    /* ---- facial organs ---- */
    { "视力下降", 5,  0.90 },
    { "视物模糊", 5,  0.85 },
    { "眼红",     5,  0.85 },
    { "眼干涩",   5,  0.80 },
    { "耳鸣",     8,  0.80 },
    { "听力下降", 8,  0.85 },
    { "鼻塞",     8,  0.80 }, { "鼻塞",     1,  0.20 },
    { "流鼻涕",   8,  0.80 },
    { "鼻出血",   8,  0.75 },
    { "牙痛",     6,  0.90 },
    { "牙龈出血", 6,  0.85 },
    { "口腔溃疡", 6,  0.80 },

    /* ---- mental and neuro ---- */
    { "失眠",       9, 0.70 },
    { "记忆力减退", 9, 0.80 },
    { "肢体抽搐",   9, 0.80 }, { "肢体抽搐", 3,  0.40 },
    { "面瘫",       9, 0.85 },
    { "晕厥",       9, 0.70 }, { "晕厥",     1,  0.30 },

    /* ---- pediatric only: very specific ---- */
    { "小儿哭闹不止", 3, 0.85 },
    { "喂养困难",     3, 0.85 },
    { "黄疸不退",     3, 0.85 },
};
const int EDGE_COUNT = (int)(sizeof(g_edges) / sizeof(g_edges[0]));

/* ==========================================================================
 *  4. Doctors: three per department
 *     title: 1 resident  2 attending  3 associate chief  4 chief
 * ==========================================================================*/
struct DoctorDef
{
    const char* name;
    int         title;
    const char* goodAt;
    double      rating;
    int         reviews;
};

const DoctorDef g_doctors[DEPT_COUNT][DOCTORS_PER_DEPT] = {
    /* 内科 */ { { "张建国", 4, "高血压,冠心病,心律失常",       4.8, 126 },
                 { "李慧敏", 3, "慢性支气管炎,咳嗽,哮喘",       4.6,  84 },
                 { "王浩然", 2, "胃炎,消化不良,发热",           4.3,  52 } },
    /* 外科 */ { { "陈立群", 4, "胆囊结石,疝气,腹部外伤",       4.7, 118 },
                 { "刘卫东", 3, "甲状腺结节,乳腺肿块,体表肿物", 4.5,  76 },
                 { "赵明哲", 2, "阑尾炎,痔疮,外伤清创",         4.2,  48 } },
    /* 儿科 */ { { "孙雅琴", 4, "小儿发热,小儿咳嗽,腹泻",       4.9, 142 },
                 { "周晓彤", 3, "新生儿黄疸,小儿湿疹,营养不良", 4.6,  91 },
                 { "吴俊杰", 2, "小儿哮喘,手足口病,疫苗接种",   4.4,  57 } },
    /* 骨科 */ { { "郑国强", 4, "骨折,关节置换,腰椎间盘突出",   4.7, 110 },
                 { "冯建军", 3, "颈椎病,膝关节疼痛,运动损伤",   4.5,  79 },
                 { "许文博", 2, "骨质疏松,腱鞘炎,足踝损伤",     4.2,  45 } },
    /* 眼科 */ { { "何秀兰", 4, "白内障,青光眼,眼底病变",       4.8, 105 },
                 { "林静怡", 3, "近视矫正,干眼症,结膜炎",       4.5,  72 },
                 { "黄志远", 2, "眼外伤,泪道阻塞,斜视弱视",     4.3,  41 } },
    /* 口腔 */ { { "马文彬", 4, "牙体牙髓病,根管治疗,牙周炎",   4.7,  98 },
                 { "朱丽娟", 3, "口腔正畸,牙齿美白,儿童龋齿",   4.6,  83 },
                 { "高鹏飞", 2, "阻生齿拔除,口腔黏膜病",         4.4,  50 } },
    /* 皮肤 */ { { "徐美华", 4, "湿疹,银屑病,荨麻疹",           4.6,  95 },
                 { "曹雪梅", 3, "痤疮,皮肤过敏,真菌感染",       4.5,  77 },
                 { "邓子豪", 2, "白癜风,脱发,疣病",             4.2,  39 } },
    /* 耳鼻喉 */ { { "宋佳明", 4, "中耳炎,耳鸣,听力下降",       4.7, 101 },
                   { "唐雨欣", 3, "过敏性鼻炎,鼻窦炎,鼻出血",   4.6,  88 },
                   { "韩志强", 2, "咽喉炎,声带息肉,扁桃体炎",   4.3,  54 } },
    /* 神经内科 */ { { "范德明", 4, "头痛,眩晕,脑血管病",       4.8, 132 },
                     { "彭雪君", 3, "偏头痛,面神经麻痹,失眠",   4.6,  86 },
                     { "袁子涵", 2, "周围神经病,记忆力减退",     4.4,  49 } },
    /* 消化内科 */ { { "邵振华", 4, "胃溃疡,反酸,肝硬化",       4.7, 114 },
                     { "汪丽云", 3, "肠易激综合征,慢性胃炎,便秘", 4.5,  81 },
                     { "龚天宇", 2, "脂肪肝,胆结石,消化道出血",   4.3,  53 } },
};

/* Floor of each department, used to build room ids.
   Rooms are numbered consecutively across all departments:
   department k owns room numbers (k-1)*3+1 .. (k-1)*3+3.
   A plain prefix scheme such as "3F-R1" would make department 1 room 1
   ("3F-R11") clash with department 10's rooms, so numbers are used instead. */
const int g_deptFloor[DEPT_COUNT] = { 3, 3, 2, 4, 2, 2, 1, 2, 4, 3 };

/* ==========================================================================
 *  5. Schedule template
 *     (weekday, slot, capacity)   weekday 0=Mon..6=Sun, slot 1=morning 2=afternoon
 *     Chief physicians get fewer slots, attending physicians get more.
 * ==========================================================================*/
struct SchedSlot { int day; int slot; int capacity; };

const SchedSlot g_schedChief[]     = { { 0, 1, 15 }, { 2, 1, 15 }, { 4, 1, 12 } };
const SchedSlot g_schedAssociate[] = { { 0, 2, 20 }, { 1, 1, 25 }, { 3, 1, 25 } };
const SchedSlot g_schedAttend[]    = { { 1, 2, 30 }, { 2, 1, 30 }, { 4, 2, 30 } };

/* ==========================================================================
 *  6. Patients
 * ==========================================================================*/
struct PatientDef { int id; const char* name; const char* phone; };

const PatientDef g_patients[] = {
    { 1,  "陈志远", "13800001001" }, { 2,  "李梦琪", "13800001002" },
    { 3,  "王海涛", "13800001003" }, { 4,  "张雨薇", "13800001004" },
    { 5,  "刘建华", "13800001005" }, { 6,  "孙丽娜", "13800001006" },
    { 7,  "周文博", "13800001007" }, { 8,  "吴晓芳", "13800001008" },
    { 9,  "郑浩然", "13800001009" }, { 10, "冯雅静", "13800001010" },
};
const int PATIENT_COUNT = (int)(sizeof(g_patients) / sizeof(g_patients[0]));

/* ==========================================================================
 *  Helpers
 * ==========================================================================*/

FILE* g_err = 0;            /* error reporting goes to stderr */
int   g_errorCount = 0;
int   g_warnCount  = 0;

void reportError(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::fprintf(stderr, "  [ERROR] ");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
    va_end(ap);
    g_errorCount++;
}

void reportWarn(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::fprintf(stderr, "  [warn]  ");
    std::vfprintf(stderr, fmt, ap);
    std::fprintf(stderr, "\n");
    va_end(ap);
    g_warnCount++;
}

/* Build a path "<dir>/<name>" without overflowing the buffer */
void joinPath(char* out, int cap, const char* dir, const char* name)
{
    std::snprintf(out, (size_t)cap, "%s/%s", dir, name);
}

/* Floor and room number of a doctor, derived from department and position */
void makeRoomId(char* out, int cap, int deptIndex, int pos)
{
    int floor = g_deptFloor[deptIndex];
    int number = deptIndex * DOCTORS_PER_DEPT + pos + 1;
    std::snprintf(out, (size_t)cap, "%dF-R%02d", floor, number);
}

/* Look up a department index by id; -1 when absent */
int deptIndexById(int id)
{
    for (int i = 0; i < DEPT_COUNT; i++)
        if (g_depts[i].id == id) return i;
    return -1;
}

/* Doctor id of the pos-th (0-based) doctor of a given department index */
int doctorIdOf(int deptIndex, int pos)
{
    return deptIndex * DOCTORS_PER_DEPT + pos + 1;
}

/* ==========================================================================
 *  Output helpers
 * ==========================================================================*/

void writeHeader(FILE* fp, const char* text)
{
    std::fprintf(fp, "# %s\n", text);
}

/* ==========================================================================
 *  File writers
 * ==========================================================================*/

void writeDepartments(const char* dir)
{
    char path[512];
    joinPath(path, sizeof(path), dir, "departments.txt");
    FILE* fp = std::fopen(path, "wb");
    if (fp == 0) { reportError("cannot write %s", path); return; }

    writeHeader(fp, "科室信息  格式：科室编号|科室名称|楼层|房间号|位置描述|科室说明");
    for (int i = 0; i < DEPT_COUNT; i++)
    {
        const DeptDef& d = g_depts[i];
        std::fprintf(fp, "%d|%s|%d|%s|%s|%s\n",
                     d.id, d.name, d.floor, d.block, d.location, d.desc);
    }
    std::fclose(fp);
    std::printf("  %-18s %4d 行\n", "departments.txt", DEPT_COUNT);
}

void writeDoctors(const char* dir)
{
    char path[512];
    joinPath(path, sizeof(path), dir, "doctors.txt");
    FILE* fp = std::fopen(path, "wb");
    if (fp == 0) { reportError("cannot write %s", path); return; }

    writeHeader(fp, "医生信息  格式：医生编号|姓名|所属科室|职称(1住院2主治3副主任4主任)|擅长领域|评分|评价人数|诊室编号");

    int rows = 0;
    for (int di = 0; di < DEPT_COUNT; di++)
    {
        for (int p = 0; p < DOCTORS_PER_DEPT; p++)
        {
            const DoctorDef& doc = g_doctors[di][p];
            char room[32];
            makeRoomId(room, sizeof(room), di, p);
            std::fprintf(fp, "%d|%s|%s|%d|%s|%.1f|%d|%s\n",
                         doctorIdOf(di, p), doc.name, g_depts[di].name,
                         doc.title, doc.goodAt, doc.rating, doc.reviews, room);
            rows++;
        }
    }
    std::fclose(fp);
    std::printf("  %-18s %4d 行\n", "doctors.txt", rows);
}

void writeSymptoms(const char* dir)
{
    char path[512];
    joinPath(path, sizeof(path), dir, "symptoms.txt");
    FILE* fp = std::fopen(path, "wb");
    if (fp == 0) { reportError("cannot write %s", path); return; }

    writeHeader(fp, "症状字典  格式：症状名|部位码(1头颈 2胸腹 3四肢 4全身 5皮肤 6五官 7精神神经)");
    for (int i = 0; i < SYMPTOM_COUNT; i++)
        std::fprintf(fp, "%s|%d\n", g_symptoms[i].name, g_symptoms[i].part);
    std::fclose(fp);
    std::printf("  %-18s %4d 行\n", "symptoms.txt", SYMPTOM_COUNT);
}

void writeSymptomDept(const char* dir)
{
    char path[512];
    joinPath(path, sizeof(path), dir, "symptom_dept.txt");
    FILE* fp = std::fopen(path, "wb");
    if (fp == 0) { reportError("cannot write %s", path); return; }

    writeHeader(fp, "症状-科室关联（加权二部图的边表）  格式：症状名|科室编号:关联强度|科室编号:关联强度|...");

    /* Edges are grouped by symptom so each line lists all departments of one
       symptom, which is exactly the shape the loader expects. */
    int lines = 0;
    for (int s = 0; s < SYMPTOM_COUNT; s++)
    {
        const char* sym = g_symptoms[s].name;
        bool first = true;
        for (int e = 0; e < EDGE_COUNT; e++)
        {
            if (std::strcmp(g_edges[e].symptom, sym) != 0) continue;
            if (first) { std::fprintf(fp, "%s", sym); first = false; }
            std::fprintf(fp, "|%d:%.2f", g_edges[e].dept, g_edges[e].weight);
        }
        if (!first) { std::fprintf(fp, "\n"); lines++; }
    }
    std::fclose(fp);
    std::printf("  %-18s %4d 行\n", "symptom_dept.txt", lines);
}

void writeSchedules(const char* dir)
{
    char path[512];
    joinPath(path, sizeof(path), dir, "schedules.txt");
    FILE* fp = std::fopen(path, "wb");
    if (fp == 0) { reportError("cannot write %s", path); return; }

    writeHeader(fp, "排班信息  格式：医生编号|星期(0周一..6周日)|时段(1上午2下午)|最大接诊数|已预约数");

    int rows = 0;
    for (int di = 0; di < DEPT_COUNT; di++)
    {
        for (int p = 0; p < DOCTORS_PER_DEPT; p++)
        {
            const DoctorDef& doc = g_doctors[di][p];
            const SchedSlot* slots = 0;
            int slotCount = 0;
            if (doc.title == 4)      { slots = g_schedChief;     slotCount = 3; }
            else if (doc.title == 3) { slots = g_schedAssociate; slotCount = 3; }
            else                     { slots = g_schedAttend;    slotCount = 3; }

            int docId = doctorIdOf(di, p);
            for (int k = 0; k < slotCount; k++)
            {
                int day = slots[k].day;
                int slot = slots[k].slot;
                int capacity = slots[k].capacity;

                /* Deterministic booked count: morning sessions fill up more
                   than afternoon ones, mimicking real outpatient patterns.
                   No random numbers, so output is reproducible. */
                int seed = (docId * 7 + day * 13 + slot * 29) % 100;
                int booked;
                if (slot == 1) booked = capacity * 55 / 100 + seed * capacity / 300;
                else           booked = capacity * 25 / 100 + seed * capacity / 400;
                if (booked > capacity) booked = capacity;

                std::fprintf(fp, "%d|%d|%d|%d|%d\n", docId, day, slot, capacity, booked);
                rows++;
            }
        }
    }
    std::fclose(fp);
    std::printf("  %-18s %4d 行\n", "schedules.txt", rows);
}

void writePatients(const char* dir)
{
    char path[512];
    joinPath(path, sizeof(path), dir, "patients.txt");
    FILE* fp = std::fopen(path, "wb");
    if (fp == 0) { reportError("cannot write %s", path); return; }

    writeHeader(fp, "患者信息  格式：患者编号|姓名|联系电话");
    for (int i = 0; i < PATIENT_COUNT; i++)
        std::fprintf(fp, "%d|%s|%s\n", g_patients[i].id, g_patients[i].name, g_patients[i].phone);
    std::fclose(fp);
    std::printf("  %-18s %4d 行\n", "patients.txt", PATIENT_COUNT);
}

/* ==========================================================================
 *  Consistency checks
 *  These catch the mistakes that are hard to spot by eye: a symptom name
 *  spelt differently in two tables, a weight out of range, a department
 *  that no symptom ever points at (it would then be unreachable in triage).
 * ==========================================================================*/

int symptomIndexByName(const char* name)
{
    for (int i = 0; i < SYMPTOM_COUNT; i++)
        if (std::strcmp(g_symptoms[i].name, name) == 0) return i;
    return -1;
}

void runChecks()
{
    std::printf("\n");
    std::printf("----------------------------------------------------------------------\n");
    std::printf("  一致性检查\n");
    std::printf("----------------------------------------------------------------------\n");

    /* every edge must reference a known symptom and department */
    for (int e = 0; e < EDGE_COUNT; e++)
    {
        if (symptomIndexByName(g_edges[e].symptom) < 0)
            reportError("关联边的症状 '%s' 不在症状字典中", g_edges[e].symptom);
        if (deptIndexById(g_edges[e].dept) < 0)
            reportError("关联边的科室 %d 不存在", g_edges[e].dept);
        if (g_edges[e].weight <= 0.0 || g_edges[e].weight > 1.0)
            reportError("关联边权重越界: %s -> %d (%.2f)",
                        g_edges[e].symptom, g_edges[e].dept, g_edges[e].weight);
    }

    /* every symptom must be linked to at least one department */
    for (int s = 0; s < SYMPTOM_COUNT; s++)
    {
        bool linked = false;
        for (int e = 0; e < EDGE_COUNT; e++)
            if (std::strcmp(g_edges[e].symptom, g_symptoms[s].name) == 0) { linked = true; break; }
        if (!linked)
            reportWarn("症状 '%s' 没有任何科室关联，导诊时输入它不会有结果", g_symptoms[s].name);
    }

    /* every department should be reachable from triage */
    for (int d = 0; d < DEPT_COUNT; d++)
    {
        bool linked = false;
        for (int e = 0; e < EDGE_COUNT; e++)
            if (g_edges[e].dept == g_depts[d].id) { linked = true; break; }
        if (!linked)
            reportError("科室 %s 没有任何症状指向它，导诊永远推不到它", g_depts[d].name);
    }

    /* duplicate symptom names would silently merge in the dictionary */
    for (int i = 0; i < SYMPTOM_COUNT; i++)
        for (int j = i + 1; j < SYMPTOM_COUNT; j++)
            if (std::strcmp(g_symptoms[i].name, g_symptoms[j].name) == 0)
                reportError("症状字典出现重复名称 '%s'", g_symptoms[i].name);

    /* duplicate room ids would make two doctors share a consulting room */
    for (int i = 0; i < DEPT_COUNT; i++)
    {
        for (int p = 0; p < DOCTORS_PER_DEPT; p++)
        {
            char ri[32];
            makeRoomId(ri, sizeof(ri), i, p);
            for (int i2 = 0; i2 < DEPT_COUNT; i2++)
                for (int p2 = 0; p2 < DOCTORS_PER_DEPT; p2++)
                {
                    if (i == i2 && p == p2) continue;
                    char rj[32];
                    makeRoomId(rj, sizeof(rj), i2, p2);
                    if (std::strcmp(ri, rj) == 0)
                        reportError("诊室编号重复: %s", ri);
                }
        }
    }

    if (g_errorCount == 0 && g_warnCount == 0)
        std::printf("  全部检查通过：关联边 %d 条，诊所编号 %d 个，均无冲突\n",
                    EDGE_COUNT, DEPT_COUNT * DOCTORS_PER_DEPT);
    else
        std::printf("  错误 %d 项，告警 %d 项\n", g_errorCount, g_warnCount);
}

/* ==========================================================================
 *  main
 * ==========================================================================*/
int main(int argc, char** argv)
{
    const char* outdir = (argc > 1) ? argv[1] : "files";
    char cmd[512];

    std::printf("======================================================================\n");
    std::printf("  生成测试数据 -> %s\n", outdir);
    std::printf("======================================================================\n");

    /* create the output directory (harmless if it already exists) */
    std::snprintf(cmd, sizeof(cmd), "if not exist \"%s\" mkdir \"%s\"", outdir, outdir);
    if (std::system(cmd) != 0)
        std::printf("  (无法自动创建目录，若写入失败请手工建立)\n");

    writeDepartments(outdir);
    writeDoctors(outdir);
    writeSymptoms(outdir);
    writeSymptomDept(outdir);
    writeSchedules(outdir);
    writePatients(outdir);

    runChecks();

    std::printf("======================================================================\n");
    std::printf("  科室 %d 个 | 医生 %d 人 | 症状 %d 条 | 患者 %d 人 | 关联边 %d 条\n",
                DEPT_COUNT, DEPT_COUNT * DOCTORS_PER_DEPT,
                SYMPTOM_COUNT, PATIENT_COUNT, EDGE_COUNT);
    std::printf("======================================================================\n");

    if (g_errorCount > 0)
    {
        std::printf("  发现 %d 个错误，请修正后重新生成。\n", g_errorCount);
        return 1;
    }
    return 0;
}