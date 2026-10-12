/* ============================================================================
 *  service/DoctorQuery.cpp  --  医生查询实现
 * ----------------------------------------------------------------------------
 *  声明见 service/DoctorQuery.h。
 *
 *  本文件里的表格打印同样要处理"中文按显示宽度对齐"的问题，
 *  做法与 service/Triage.cpp 一致：自己算显示宽度再手工补空格。
 * ==========================================================================*/

#include "service/DoctorQuery.h"
#include "ds/Sort.h"
#include "util/Console.h"
#include <cstdio>
#include <cstring>

/* ==========================================================================
 *  构造
 * ==========================================================================*/
DoctorQueryService::DoctorQueryService(DataStore* store)
{
    m_store = store;
}

/* ==========================================================================
 *  号源统计
 * --------------------------------------------------------------------------
 *  遍历全部排班，累计某位医生的号源总数与剩余数。
 *
 *  数据量：90 条排班 × 30 位医生 = 最坏 2700 次比较，微秒级。
 *  这里特意不建"医生 -> 排班"的索引，因为：
 *      * 数据量极小，建索引的收益看不出来；
 *      * 少一层缓存就少一处可能与主表不同步的地方。
 *  若将来排班表涨到几万条，再在 DataStore 里加"医生 -> 排班集合"的桶。
 * ==========================================================================*/
void DoctorQueryService::slotStats(int doctorId, int* remainingOut, int* totalOut) const
{
    int remaining = 0;
    int total     = 0;

    if (m_store != 0)
    {
        SeqList<Schedule>& schedules = const_cast<DataStore*>(m_store)->schedules();
        for (int i = 0; i < schedules.size(); i++)
        {
            const Schedule* s = schedules.ptrAt(i);
            if (s == 0 || s->doctorId != doctorId) continue;
            total     += s->capacity;
            remaining += s->remain();
        }
    }

    if (remainingOut != 0) *remainingOut = remaining;
    if (totalOut != 0)     *totalOut     = total;
}

int DoctorQueryService::availableDayCount(int doctorId) const
{
    bool seen[7];
    for (int i = 0; i < 7; i++) seen[i] = false;

    if (m_store != 0)
    {
        SeqList<Schedule>& schedules = const_cast<DataStore*>(m_store)->schedules();
        for (int i = 0; i < schedules.size(); i++)
        {
            const Schedule* s = schedules.ptrAt(i);
            if (s == 0 || s->doctorId != doctorId) continue;
            if (s->remain() > 0) seen[s->day] = true;
        }
    }

    int count = 0;
    for (int i = 0; i < 7; i++) if (seen[i]) count++;
    return count;
}

/* ==========================================================================
 *  条件判定
 * ==========================================================================*/
bool DoctorQueryService::matchCondition(const Doctor* d, const QueryCondition& cond) const
{
    if (d == 0) return false;

    /* 科室：-1 表示不限 */
    if (cond.deptId >= 0 && d->deptId != cond.deptId) return false;

    /* 职称：位掩码；0 表示不限 */
    if (!cond.titleAllowed(d->title)) return false;

    /* 擅长领域：子串匹配，空串表示不限。
       用子串而不是完全相等，所以查"高血压"能命中"高血压,冠心病,心律失常"。 */
    if (cond.goodAt[0] != '\0' && !d->goodAtContains(cond.goodAt)) return false;

    return true;
}

/* ==========================================================================
 *  填充一条结果的派生信息
 * ==========================================================================*/
bool DoctorQueryService::fillRow(int doctorId, DoctorRow* rowOut) const
{
    if (rowOut == 0 || m_store == 0) return false;

    const Doctor* d = m_store->findDoctor(doctorId);
    if (d == 0) { rowOut->init(); return false; }

    rowOut->init();
    rowOut->id     = d->id;
    rowOut->deptId = d->deptId;
    rowOut->title  = d->title;
    rowOut->rating = d->rating;
    std::snprintf(rowOut->name,   NAME_LEN, "%s", d->name);
    std::snprintf(rowOut->goodAt, TEXT_LEN, "%s", d->goodAt);
    std::snprintf(rowOut->roomId, ROOM_LEN, "%s", d->roomId);

    const Department* dept = m_store->findDepartment(d->deptId);
    if (dept != 0) std::snprintf(rowOut->deptName, NAME_LEN, "%s", dept->name);

    slotStats(doctorId, &rowOut->remainingSlots, &rowOut->totalSlots);
    rowOut->availableDays = availableDayCount(doctorId);

    return true;
}

/* ==========================================================================
 *  排序：职称降序，其次评分降序，再次编号升序
 *  编号兜底是为了让顺序稳定、可复现 —— 不会因为排序不稳定而两次结果不同
 * ==========================================================================*/
static int compareDoctorRow(const void* a, const void* b)
{
    const DoctorRow* x = (const DoctorRow*)a;
    const DoctorRow* y = (const DoctorRow*)b;

    if (x->title != y->title) return (x->title > y->title) ? -1 : 1;

    if (x->rating > y->rating) return -1;
    if (x->rating < y->rating) return  1;

    if (x->id < y->id) return -1;
    if (x->id > y->id) return  1;
    return 0;
}

/* ==========================================================================
 *  核心查询
 * ==========================================================================*/
int DoctorQueryService::query(const QueryCondition& cond, DoctorRow* rowsOut,
                              int maxRows) const
{
    if (rowsOut == 0 || maxRows <= 0 || m_store == 0) return 0;

    int count = 0;

    /* ---- 阶段一：确定候选集，逐条过滤 ----
       若指定了科室，就直接从该科的医生集合出发，不必遍历全体。
       这就是"索引先行、过滤在后"。 */
    if (cond.deptId >= 0)
    {
        SeqList<Doctor>& doctors = const_cast<DataStore*>(m_store)->doctors();
        for (int i = 0; i < doctors.size() && count < maxRows; i++)
        {
            const Doctor* d = doctors.ptrAt(i);
            if (d == 0) continue;
            if (d->deptId != cond.deptId) continue;      /* 等价于直接取该科集合 */
            if (!matchCondition(d, cond)) continue;

            fillRow(d->id, &rowsOut[count]);
            count++;
        }
    }
    else
    {
        SeqList<Doctor>& doctors = const_cast<DataStore*>(m_store)->doctors();
        for (int i = 0; i < doctors.size() && count < maxRows; i++)
        {
            const Doctor* d = doctors.ptrAt(i);
            if (d == 0) continue;
            if (!matchCondition(d, cond)) continue;

            fillRow(d->id, &rowsOut[count]);
            count++;
        }
    }

    /* ---- 阶段二：排序 ---- */
    if (count > 1)
        quickSort(rowsOut, count, sizeof(DoctorRow), compareDoctorRow);

    return count;
}

/* ==========================================================================
 *  便捷方法
 * ==========================================================================*/
int DoctorQueryService::byDepartment(int deptId, DoctorRow* rowsOut, int maxRows) const
{
    QueryCondition cond;
    cond.init();
    cond.deptId = deptId;
    return query(cond, rowsOut, maxRows);
}

int DoctorQueryService::byTitle(int title, DoctorRow* rowsOut, int maxRows) const
{
    QueryCondition cond;
    cond.init();
    cond.setTitle(title);
    return query(cond, rowsOut, maxRows);
}

int DoctorQueryService::byGoodAt(const char* keyword, DoctorRow* rowsOut, int maxRows) const
{
    QueryCondition cond;
    cond.init();
    if (keyword != 0) std::snprintf(cond.goodAt, NAME_LEN, "%s", keyword);
    return query(cond, rowsOut, maxRows);
}

int DoctorQueryService::byName(const char* keyword, DoctorRow* rowsOut, int maxRows) const
{
    if (rowsOut == 0 || maxRows <= 0 || m_store == 0) return 0;

    int count = 0;
    SeqList<Doctor>& doctors = const_cast<DataStore*>(m_store)->doctors();

    for (int i = 0; i < doctors.size() && count < maxRows; i++)
    {
        const Doctor* d = doctors.ptrAt(i);
        if (d == 0) continue;

        /* 姓名子串匹配；空关键字视为不限，返回全部 */
        if (keyword != 0 && keyword[0] != '\0')
        {
            bool found = false;
            int nameLen = (int)std::strlen(d->name);
            int keyLen  = (int)std::strlen(keyword);
            for (int k = 0; k + keyLen <= nameLen && !found; k++)
            {
                int j = 0;
                while (j < keyLen && d->name[k + j] == keyword[j]) j++;
                if (j == keyLen) found = true;
            }
            if (!found) continue;
        }

        fillRow(d->id, &rowsOut[count]);
        count++;
    }

    if (count > 1) quickSort(rowsOut, count, sizeof(DoctorRow), compareDoctorRow);
    return count;
}

int DoctorQueryService::all(DoctorRow* rowsOut, int maxRows) const
{
    QueryCondition cond;
    cond.init();
    return query(cond, rowsOut, maxRows);
}

/* ==========================================================================
 *  界面输出
 * ==========================================================================*/
void DoctorQueryService::printRows(const DoctorRow* rows, int count) const
{
    if (count <= 0)
    {
        std::printf("  没有符合条件的医生。\n");
        return;
    }

    const int W_ID    = 4;
    const int W_NAME  = 10;
    const int W_DEPT  = 10;
    const int W_TITLE = 12;
    const int W_ROOM  = 8;
    const int W_RATE  = 6;
    const int W_SLOT  = 12;

    #define HMS_LINE() do { \
        std::printf("  +"); \
        for (int i = 0; i < W_ID    + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_NAME  + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_DEPT  + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_TITLE + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_ROOM  + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_RATE  + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_SLOT  + 2; i++) std::printf("-"); \
        std::printf("+\n"); \
    } while (0)

    HMS_LINE();
    std::printf("  | ");
    printField("编号", W_ID);    std::printf(" | ");
    printField("姓名", W_NAME);  std::printf(" | ");
    printField("科室", W_DEPT);  std::printf(" | ");
    printField("职称", W_TITLE); std::printf(" | ");
    printField("诊室", W_ROOM);  std::printf(" | ");
    printField("评分", W_RATE);  std::printf(" | ");
    printField("本周余号", W_SLOT); std::printf(" |\n");
    HMS_LINE();

    char buf[32];
    for (int i = 0; i < count; i++)
    {
        const DoctorRow& r = rows[i];

        std::printf("  | ");
        std::snprintf(buf, sizeof(buf), "%d", r.id);
        printField(buf, W_ID); std::printf(" | ");

        printField(r.name, W_NAME); std::printf(" | ");
        printField(r.deptName, W_DEPT); std::printf(" | ");
        printField(titleName(r.title), W_TITLE); std::printf(" | ");
        printField(r.roomId, W_ROOM); std::printf(" | ");

        std::snprintf(buf, sizeof(buf), "%.1f", r.rating);
        printField(buf, W_RATE); std::printf(" | ");

        std::snprintf(buf, sizeof(buf), "%d/%d", r.remainingSlots, r.totalSlots);
        printField(buf, W_SLOT); std::printf(" |\n");
    }
    HMS_LINE();

    #undef HMS_LINE

    std::printf("  共 %d 位医生。\n", count);
}

void DoctorQueryService::printTitleMenu() const
{
    std::printf("  请选择职称：\n");
    for (int t = TITLE_CHIEF; t >= TITLE_RESIDENT; t--)
        std::printf("    %d. %s\n", t, titleName(t));
}

/* ==========================================================================
 *  某医生本周号源网格
 * --------------------------------------------------------------------------
 *  布局：行是星期一到星期日，列是上午与下午。
 *  没有排班的格子显示"—"，排满的显示"已满 (已约/总数)"，
 *  还有号的显示"余 N 号 (已约/总数)"。
 *
 *  这个方法是"排班与号源展示"功能的核心输出，
 *  也是后续"在线预约"选时段时要用到的界面。
 * ==========================================================================*/
int DoctorQueryService::printDoctorSchedule(int doctorId) const
{
    if (m_store == 0) return -1;

    const Doctor* d = m_store->findDoctor(doctorId);
    if (d == 0)
    {
        std::printf("  医生编号 %d 不存在。\n", doctorId);
        return -1;
    }

    const Department* dept = m_store->findDepartment(d->deptId);

    std::printf("  %s（%s / %s）\n", d->name,
                (dept != 0) ? dept->name : "未知科室",
                titleName(d->title));
    if (d->roomId[0] != '\0')
        std::printf("  诊室：%s\n", d->roomId);
    std::printf("\n");

    const int W_DAY  = 8;
    const int W_MORN = 20;
    const int W_AFT  = 20;
    const int widths[3] = { W_DAY, W_MORN, W_AFT };

    printSeparator(widths, 3);
    std::printf("  | ");
    printField("星期", W_DAY);  std::printf(" | ");
    printField("上午", W_MORN); std::printf(" | ");
    printField("下午", W_AFT);  std::printf(" |\n");
    printSeparator(widths, 3);

    int weekRemaining = 0;
    int weekTotal     = 0;

    for (int day = 0; day < 7; day++)
    {
        char morning[64];
        char afternoon[64];
        morning[0]   = '\0';
        afternoon[0] = '\0';

        for (int slot = 1; slot <= SLOT_COUNT; slot++)
        {
            const Schedule* s = m_store->findScheduleByKey(doctorId, day, slot);
            char* target = (slot == SLOT_MORNING) ? morning : afternoon;

            if (s == 0)
            {
                std::snprintf(target, 64, "—");
                continue;
            }

            weekTotal     += s->capacity;
            weekRemaining += s->remain();

            if (s->isFull())
                std::snprintf(target, 64, "已满 (%d/%d)", s->booked, s->capacity);
            else
                std::snprintf(target, 64, "余 %d 号  (%d/%d)",
                              s->remain(), s->booked, s->capacity);
        }

        std::printf("  | ");
        printField(weekdayName(day), W_DAY); std::printf(" | ");
        printField(morning, W_MORN);         std::printf(" | ");
        printField(afternoon, W_AFT);        std::printf(" |\n");
    }
    printSeparator(widths, 3);

    std::printf("  本周共 %d 个号源，剩余 %d 个。\n", weekTotal, weekRemaining);
    return weekRemaining;
}
