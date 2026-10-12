/* ============================================================================
 *  tests/test_query.cpp  --  doctor query self-test
 * ----------------------------------------------------------------------------
 *  Expected values below were computed from the actual files/doctors.txt:
 *      30 doctors, 3 per department
 *      titles: 10 x attending(2), 10 x associate chief(3), 10 x chief(4)
 *      goodAt contains 高血压 -> 1 (张建国)
 *      goodAt contains 头痛   -> 2 (范德明, 彭雪君)
 *      goodAt contains 牙     -> 2 (马文彬, 朱丽娟)
 *      goodAt contains 小儿   -> 3 (孙雅琴, 周晓彤, 吴俊杰)
 *      name contains 张/李/王/马/孙 -> 1 each
 *      内科 + 主任医师 -> 1 (张建国)
 *      副主任及以上 -> 20
 *
 *  Build and run:  build test
 * ==========================================================================*/

#include <cstdio>
#include <cstring>
#include "data/DataStore.h"
#include "file/FileLoader.h"
#include "service/DoctorQuery.h"

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

static const char* DATA_DIR = "files";
static DoctorRow g_rows[128];

/* does any row with this name exist in the result set */
static bool hasName(const DoctorRow* rows, int count, const char* name)
{
    for (int i = 0; i < count; i++)
        if (std::strcmp(rows[i].name, name) == 0) return true;
    return false;
}

/* ==========================================================================
 *  1. no condition: list everyone
 * ==========================================================================*/
static void testAll(DoctorQueryService* q)
{
    std::printf("[1] list all\n");

    int n = q->all(g_rows, 128);
    std::printf("      all doctors -> %d\n", n);
    CHECK(n == 30, "30 doctors in total");

    /* ordering: title descending */
    bool titleDescending = true;
    for (int i = 1; i < n; i++)
        if (g_rows[i].title > g_rows[i - 1].title) titleDescending = false;
    CHECK(titleDescending, "sorted by title, descending");

    /* within the same title, rating descending */
    bool ratingDescending = true;
    for (int i = 1; i < n; i++)
    {
        if (g_rows[i].title != g_rows[i - 1].title) continue;
        if (g_rows[i].rating > g_rows[i - 1].rating) ratingDescending = false;
    }
    CHECK(ratingDescending, "within one title, sorted by rating descending");

    /* the first one must be a chief physician */
    CHECK(g_rows[0].title == TITLE_CHIEF, "first row is a chief physician");

    /* every row carries derived info */
    bool allFilled = true;
    for (int i = 0; i < n; i++)
        if (g_rows[i].deptName[0] == '\0' || g_rows[i].totalSlots <= 0) allFilled = false;
    CHECK(allFilled, "every row has department name and slot totals");
}

/* ==========================================================================
 *  2. filter by department
 * ==========================================================================*/
static void testByDepartment(DoctorQueryService* q, DataStore* store)
{
    std::printf("[2] filter by department\n");

    int internal = store->departmentIdByName("内科");
    CHECK(internal >= 0, "内科 exists");

    int n = q->byDepartment(internal, g_rows, 128);
    std::printf("      内科 -> %d doctors\n", n);
    CHECK(n == 3, "内科 has 3 doctors");

    bool allInDept = true;
    for (int i = 0; i < n; i++)
        if (g_rows[i].deptId != internal) allInDept = false;
    CHECK(allInDept, "every returned doctor belongs to 内科");

    bool hasZhang = hasName(g_rows, n, "张建国");
    bool hasLi    = hasName(g_rows, n, "李慧敏");
    bool hasWang  = hasName(g_rows, n, "王浩然");
    CHECK(hasZhang && hasLi && hasWang, "内科 returns 张建国/李慧敏/王浩然");

    /* 张建国 is chief, so he must come first in this department */
    CHECK(std::strcmp(g_rows[0].name, "张建国") == 0, "chief physician ranks first");

    /* every department must return exactly 3 */
    bool allThree = true;
    for (int d = 0; d < store->departmentCount(); d++)
    {
        const Department* dept = store->departments().ptrAt(d);
        if (dept == 0) continue;
        int c = q->byDepartment(dept->id, g_rows, 128);
        if (c != 3) { allThree = false; std::printf("      %s has %d doctors\n", dept->name, c); }
    }
    CHECK(allThree, "all 10 departments have exactly 3 doctors");

    /* nonexistent department */
    CHECK(q->byDepartment(999, g_rows, 128) == 0, "unknown department -> 0 results");
    CHECK(q->byDepartment(-1, g_rows, 128) == 30, "deptId -1 means no filter -> all 30");
}

/* ==========================================================================
 *  3. filter by title
 * ==========================================================================*/
static void testByTitle(DoctorQueryService* q)
{
    std::printf("[3] filter by title\n");

    int n = q->byTitle(TITLE_CHIEF, g_rows, 128);
    std::printf("      主任医师 -> %d\n", n);
    CHECK(n == 10, "10 chief physicians");

    bool allChief = true;
    for (int i = 0; i < n; i++)
        if (g_rows[i].title != TITLE_CHIEF) allChief = false;
    CHECK(allChief, "every result is a chief physician");

    CHECK(q->byTitle(TITLE_ASSOCIATE, g_rows, 128) == 10, "10 associate chiefs");
    CHECK(q->byTitle(TITLE_ATTENDING, g_rows, 128) == 10, "10 attending physicians");
    CHECK(q->byTitle(TITLE_RESIDENT, g_rows, 128) == 0, "no residents in the data");

    /* ---- title mask: chief OR associate = 20 ---- */
    {
        QueryCondition cond;
        cond.init();
        cond.setTitle(TITLE_CHIEF);
        cond.setTitle(TITLE_ASSOCIATE);

        int m = q->query(cond, g_rows, 128);
        std::printf("      主任医师 或 副主任医师 -> %d\n", m);
        CHECK(m == 20, "title mask (chief|associate) -> 20 doctors");

        bool allSenior = true;
        for (int i = 0; i < m; i++)
            if (g_rows[i].title != TITLE_CHIEF && g_rows[i].title != TITLE_ASSOCIATE)
                allSenior = false;
        CHECK(allSenior, "every result is chief or associate chief");
    }

    /* ---- invalid title value ----
       A filter was requested but matches no valid title, so the result must be
       empty.  This must NOT be confused with "no title filter". */
    CHECK(q->byTitle(999, g_rows, 128) == 0, "invalid title 999 -> 0 results");
    CHECK(q->byTitle(0, g_rows, 128) == 0, "title 0 -> 0 results");
    CHECK(q->byTitle(-5, g_rows, 128) == 0, "negative title -> 0 results");

    /* ---- clearTitles() restores "no filter" ---- */
    {
        QueryCondition cond;
        cond.init();
        cond.setTitle(TITLE_CHIEF);
        CHECK(q->query(cond, g_rows, 128) == 10, "chief filter -> 10");

        cond.clearTitles();
        CHECK(q->query(cond, g_rows, 128) == 30, "after clearTitles -> no filter, 30");
    }
}

/* ==========================================================================
 *  4. filter by goodAt
 * ==========================================================================*/
static void testByGoodAt(DoctorQueryService* q)
{
    std::printf("[4] filter by goodAt\n");

    int n = q->byGoodAt("高血压", g_rows, 128);
    std::printf("      擅长含'高血压' -> %d\n", n);
    CHECK(n == 1, "1 doctor is good at 高血压");
    if (n >= 1) CHECK(std::strcmp(g_rows[0].name, "张建国") == 0, "it is 张建国");

    n = q->byGoodAt("头痛", g_rows, 128);
    std::printf("      擅长含'头痛' -> %d\n", n);
    CHECK(n == 2, "2 doctors are good at 头痛");
    CHECK(hasName(g_rows, n, "范德明") && hasName(g_rows, n, "彭雪君"),
          "they are 范德明 and 彭雪君");

    n = q->byGoodAt("牙", g_rows, 128);
    std::printf("      擅长含'牙' -> %d\n", n);
    CHECK(n == 2, "substring match: '牙' hits 马文彬(牙体牙髓病) and 朱丽娟(儿童龋齿? no)");
    CHECK(hasName(g_rows, n, "马文彬"), "马文彬 matched");

    n = q->byGoodAt("小儿", g_rows, 128);
    std::printf("      擅长含'小儿' -> %d\n", n);
    CHECK(n == 3, "3 pediatricians mention 小儿");
    CHECK(hasName(g_rows, n, "孙雅琴") && hasName(g_rows, n, "周晓彤") && hasName(g_rows, n, "吴俊杰"),
          "all three pediatricians matched");

    CHECK(q->byGoodAt("不存在的专长", g_rows, 128) == 0, "unknown keyword -> 0 results");
    CHECK(q->byGoodAt("", g_rows, 128) == 30, "empty keyword means no filter -> 30");

    /* substring semantics: a partial word still matches */
    n = q->byGoodAt("高血压", g_rows, 128);
    CHECK(n >= 1, "substring match works on a partial term");
}

/* ==========================================================================
 *  5. combined conditions (AND semantics)
 * ==========================================================================*/
static void testCombined(DoctorQueryService* q, DataStore* store)
{
    std::printf("[5] combined conditions\n");

    int internal = store->departmentIdByName("内科");

    /* 内科 + 主任医师 -> only 张建国 */
    {
        QueryCondition cond;
        cond.init();
        cond.deptId = internal;
        cond.setTitle(TITLE_CHIEF);

        int n = q->query(cond, g_rows, 128);
        std::printf("      内科 + 主任医师 -> %d\n", n);
        CHECK(n == 1, "内科 + 主任医师 -> exactly 1");
        if (n >= 1) CHECK(std::strcmp(g_rows[0].name, "张建国") == 0, "it is 张建国");
    }

    /* 内科 + 主治医师 -> only 王浩然 */
    {
        QueryCondition cond;
        cond.init();
        cond.deptId = internal;
        cond.setTitle(TITLE_ATTENDING);

        int n = q->query(cond, g_rows, 128);
        CHECK(n == 1, "内科 + 主治医师 -> exactly 1");
        if (n >= 1) CHECK(std::strcmp(g_rows[0].name, "王浩然") == 0, "it is 王浩然");
    }

    /* 内科 + 擅长含"高血压" -> 张建国 */
    {
        QueryCondition cond;
        cond.init();
        cond.deptId = internal;
        std::snprintf(cond.goodAt, NAME_LEN, "高血压");

        int n = q->query(cond, g_rows, 128);
        CHECK(n == 1, "内科 + 擅长高血压 -> 1");
        if (n >= 1) CHECK(std::strcmp(g_rows[0].name, "张建国") == 0, "it is 张建国");
    }

    /* a combination that matches nothing */
    {
        QueryCondition cond;
        cond.init();
        cond.deptId = internal;
        std::snprintf(cond.goodAt, NAME_LEN, "白内障");

        CHECK(q->query(cond, g_rows, 128) == 0, "内科 + 擅长白内障 -> 0 (no such doctor)");
    }

    /* all three conditions together */
    {
        QueryCondition cond;
        cond.init();
        cond.deptId = internal;
        cond.setTitle(TITLE_CHIEF);
        std::snprintf(cond.goodAt, NAME_LEN, "冠心病");

        int n = q->query(cond, g_rows, 128);
        CHECK(n == 1, "内科 + 主任医师 + 冠心病 -> 1");
    }
}

/* ==========================================================================
 *  6. search by name
 * ==========================================================================*/
static void testByName(DoctorQueryService* q)
{
    std::printf("[6] search by name\n");

    int n = q->byName("张建国", g_rows, 128);
    CHECK(n == 1, "full name 张建国 -> 1");
    if (n >= 1) CHECK(std::strcmp(g_rows[0].name, "张建国") == 0, "correct doctor returned");

    n = q->byName("张", g_rows, 128);
    std::printf("      姓名含'张' -> %d\n", n);
    CHECK(n == 1, "surname 张 -> 1");

    n = q->byName("马", g_rows, 128);
    CHECK(n == 1 && std::strcmp(g_rows[0].name, "马文彬") == 0, "surname 马 -> 马文彬");

    CHECK(q->byName("不存在的人", g_rows, 128) == 0, "unknown name -> 0");
    CHECK(q->byName("", g_rows, 128) == 30, "empty name -> all 30");
    CHECK(q->byName(0, g_rows, 128) == 30, "null name -> all 30 (treated as no filter)");
}

/* ==========================================================================
 *  7. derived info: slots and days
 * ==========================================================================*/
static void testSlotStats(DoctorQueryService* q, DataStore* store)
{
    std::printf("[7] slot statistics\n");

    int n = q->byDepartment(store->departmentIdByName("口腔科"), g_rows, 128);
    CHECK(n == 3, "口腔科 has 3 doctors");

    for (int i = 0; i < n; i++)
    {
        const DoctorRow& r = g_rows[i];
        std::printf("      %-8s total %3d  remaining %3d  days %d\n",
                    r.name, r.totalSlots, r.remainingSlots, r.availableDays);

        CHECK(r.totalSlots > 0, "total slots is positive");
        CHECK(r.remainingSlots >= 0, "remaining slots is not negative");
        CHECK(r.remainingSlots <= r.totalSlots, "remaining <= total");
        CHECK(r.availableDays >= 1 && r.availableDays <= 7, "available days in 1..7");
    }

    /* cross-check against DataStore: sum of capacities must match */
    {
        const Doctor* d = store->findDoctor(16);      /* 马文彬 */
        CHECK(d != 0, "doctor 16 exists");

        int remaining = 0, total = 0;
        q->slotStats(16, &remaining, &total);

        int manualTotal = 0, manualRemaining = 0;
        for (int i = 0; i < store->scheduleCount(); i++)
        {
            const Schedule* s = store->schedules().ptrAt(i);
            if (s == 0 || s->doctorId != 16) continue;
            manualTotal     += s->capacity;
            manualRemaining += s->remain();
        }
        CHECK(total == manualTotal, "slot total matches a manual sum over schedules");
        CHECK(remaining == manualRemaining, "remaining matches a manual sum");
    }

    /* unknown doctor */
    {
        int remaining = -1, total = -1;
        q->slotStats(9999, &remaining, &total);
        CHECK(remaining == 0 && total == 0, "unknown doctor -> 0/0");
        CHECK(q->availableDayCount(9999) == 0, "unknown doctor -> 0 available days");
    }

    /* fillRow on an unknown id */
    {
        DoctorRow row;
        CHECK(!q->fillRow(9999, &row), "fillRow on unknown id returns false");
        CHECK(row.id == -1, "row is reset to the initial state");
    }
}

/* ==========================================================================
 *  8. boundary handling
 * ==========================================================================*/
static void testBoundary(DoctorQueryService* q)
{
    std::printf("[8] boundary handling\n");

    /* maxRows clamping: must not write past the caller's buffer */
    {
        DoctorRow small[2];
        int n = q->all(small, 2);
        CHECK(n == 2, "maxRows limits the result count");

        int n0 = q->all(small, 0);
        CHECK(n0 == 0, "maxRows 0 -> 0 results");
        int nn = q->all(small, -1);
        CHECK(nn == 0, "negative maxRows -> 0 results");
    }

    /* null output buffer */
    {
        QueryCondition cond;
        cond.init();
        CHECK(q->query(cond, 0, 10) == 0, "null output buffer -> 0");
    }

    /* result must be reproducible: same query twice, same order */
    {
        DoctorRow a[128], b[128];
        int na = q->all(a, 128);
        int nb = q->all(b, 128);

        bool same = (na == nb);
        for (int i = 0; i < na && same; i++)
            if (a[i].id != b[i].id) same = false;
        CHECK(same, "repeated query gives identical order (stable sorting)");
    }

    /* ordering must be fully deterministic even with tied title and rating */
    {
        DoctorRow a[128];
        int n = q->byTitle(TITLE_CHIEF, a, 128);
        bool idAscendingOnTie = true;
        for (int i = 1; i < n; i++)
        {
            if (a[i].title != a[i - 1].title) continue;
            if (a[i].rating != a[i - 1].rating) continue;
            if (a[i].id < a[i - 1].id) idAscendingOnTie = false;
        }
        CHECK(idAscendingOnTie, "ties break by ascending id");
    }
}

/* ==========================================================================
 *  main
 * ==========================================================================*/
int main()
{
    std::printf("========================================================\n");
    std::printf("  doctor query self-test -- service/DoctorQuery\n");
    std::printf("========================================================\n");

    DataStore* store = new DataStore();
    FileLoader loader(DATA_DIR);
    if (!loader.loadAll(store))
    {
        std::printf("data load failed, cannot continue\n");
        delete store;
        return 1;
    }

    DoctorQueryService* q = new DoctorQueryService(store);

    testAll(q);
    testByDepartment(q, store);
    testByTitle(q);
    testByGoodAt(q);
    testCombined(q, store);
    testByName(q);
    testSlotStats(q, store);
    testBoundary(q);

    std::printf("\n");
    std::printf("----------------------------------------------------------------------\n");
    std::printf("  sample output\n");
    std::printf("----------------------------------------------------------------------\n");
    {
        std::printf("\n  [口腔科全部医生]\n\n");
        int n = q->byDepartment(store->departmentIdByName("口腔科"), g_rows, 128);
        q->printRows(g_rows, n);

        std::printf("\n  [全部主任医师]\n\n");
        n = q->byTitle(TITLE_CHIEF, g_rows, 128);
        q->printRows(g_rows, n);

        std::printf("\n  [擅长'头痛'的医生]\n\n");
        n = q->byGoodAt("头痛", g_rows, 128);
        q->printRows(g_rows, n);
    }

    delete q;
    delete store;

    std::printf("--------------------------------------------------------\n");
    std::printf("  passed %d, failed %d\n", g_pass, g_fail);
    std::printf("========================================================\n");

    return (g_fail == 0) ? 0 : 1;
}