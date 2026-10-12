#ifndef HMS_DOCTORQUERY_H
#define HMS_DOCTORQUERY_H

/* ============================================================================
 *  service/DoctorQuery.h  --  医生查询
 * ----------------------------------------------------------------------------
 *  对应项目框架.md 3.5 节。题目基本要求第二条：
 *      "医生查询：支持按科室、职称、擅长领域等条件查询医生"
 *
 *  ----------------------------------------------------------------------------
 *  实现思路：索引先行，过滤在后
 *
 *  最直接的做法是"把 30 位医生从头到尾遍历一遍，逐个判断是否符合条件"。
 *  本工程在这个基础上加了一步：**如果条件里指定了科室，就直接取该科室的
 *  医生集合，不遍历全体**。
 *
 *      指定科室时：候选集 = 该科医生（3 人），过滤它们
 *      未指定科室：候选集 = 全体医生（30 人），过滤它们
 *
 *  这样做的好处不只是快 —— 更重要的是**把"按科室找医生"这件事的入口
 *  收敛到一处**。以后若数据量涨到几千人，需要给科室建哈希桶，
 *  改的也只是这里，不用去改每个调用方。
 *
 *  注意：本工程数据量小（30 人），即使全部遍历也只有 30 次比较。
 *  用索引是为了"设计上说得通"，不是为了性能数字。
 *
 *  ----------------------------------------------------------------------------
 *  三个条件之间是"与"的关系
 *
 *      科室  &&  职称  &&  擅长领域
 *
 *  未填写的条件视为"不限"。职称用**位掩码**表示"关注哪几档"，
 *  这样"主任医师或副主任医师"这种组合条件也能一次判断，不用写多重分支。
 * ==========================================================================*/

#include "data/DataStore.h"

/* ==========================================================================
 *  查询条件
 *  各字段的"不限"取值：deptId = -1，titleMask = 0，goodAt 为空串
 * ==========================================================================*/
struct QueryCondition
{
    int  deptId;        /* 科室编号；-1 表示不限 */
    int  titleMask;     /* 职称位掩码，用 titleBit()/setTitle() 构造 */
    bool titleFilter;   /* 是否启用职称条件。见下方说明 */
    char goodAt[NAME_LEN];  /* 擅长领域关键字；空串表示不限 */

    void init()
    {
        deptId      = -1;
        titleMask   = 0;
        titleFilter = false;
        goodAt[0]   = '\0';
    }

    /* 把某档职称加入筛选条件。
       例：setTitle(TITLE_CHIEF); setTitle(TITLE_ASSOCIATE);
           表示"主任医师或副主任医师" */
    static int titleBit(int title)
    {
        return (title >= TITLE_RESIDENT && title <= TITLE_CHIEF)
                 ? (1 << (title - 1)) : 0;
    }

    /* 【为什么需要一个单独的 titleFilter 标志】
       如果只用 titleMask 判断，就会出现这样的矛盾：
           titleMask == 0  既表示"用户没有限定职称"，
                           又表示"用户限定了一个不存在的职称"。
       前者应当返回全部医生，后者应当返回 0 条 —— 同一个值没法同时满足。

       所以把两件事拆开：
           titleFilter == false          -> 不限职称，全部通过
           titleFilter == true, mask==0  -> 条件设了但无有效职称，全部不通过
           titleFilter == true, mask!=0  -> 只通过掩码内的职称 */
    void setTitle(int title)
    {
        titleFilter = true;
        titleMask  |= titleBit(title);
    }

    void clearTitles()
    {
        titleFilter = false;
        titleMask   = 0;
    }

    /* 某档职称是否通过筛选 */
    bool titleAllowed(int title) const
    {
        if (!titleFilter) return true;                  /* 不限 */
        if (titleMask == 0) return false;               /* 条件无效，全部拒绝 */
        return (titleMask & titleBit(title)) != 0;
    }
};

/* ==========================================================================
 *  一条查询结果
 *
 *  返回实体指针有风险：顺序表扩容会让旧指针失效。所以这里返回"实体的副本
 *  加上派生出来的展示信息"，调用方拿着就能直接打印，不必再回表查询。
 *  30 位医生按值拷贝一次的开销可以忽略（每人 200 字节，共 6 KB）。
 * ==========================================================================*/
struct DoctorRow
{
    /* ---- 实体字段副本 ---- */
    int    id;
    char   name[NAME_LEN];
    int    deptId;
    char   deptName[NAME_LEN];
    int    title;
    char   goodAt[TEXT_LEN];
    double rating;
    char   roomId[ROOM_LEN];

    /* ---- 派生信息 ---- */
    int    remainingSlots;   /* 本周剩余号源总数 */
    int    totalSlots;       /* 本周号源总数 */
    int    availableDays;    /* 本周有号的日期数（0~7） */

    void init()
    {
        id = -1; deptId = -1; title = 0; rating = 0.0;
        remainingSlots = 0; totalSlots = 0; availableDays = 0;
        name[0] = '\0'; deptName[0] = '\0'; goodAt[0] = '\0'; roomId[0] = '\0';
    }
};

/* ==========================================================================
 *  医生查询服务
 * ==========================================================================*/
class DoctorQueryService
{
public:
    explicit DoctorQueryService(DataStore* store);

    /* ------------------------------------------------------------------
     *  核心查询
     *  命中的医生写入 rowsOut（调用方保证容量 >= maxRows），返回命中个数。
     *  结果按"职称降序、评分降序"排列。
     * ------------------------------------------------------------------ */
    int query(const QueryCondition& cond, DoctorRow* rowsOut, int maxRows) const;

    /* 便捷方法 */
    int byDepartment(int deptId, DoctorRow* rowsOut, int maxRows) const;
    int byTitle(int title, DoctorRow* rowsOut, int maxRows) const;
    int byGoodAt(const char* keyword, DoctorRow* rowsOut, int maxRows) const;
    int byName(const char* keyword, DoctorRow* rowsOut, int maxRows) const;
    int all(DoctorRow* rowsOut, int maxRows) const;

    /* ------------------------------------------------------------------
     *  按医生编号取一条完整结果（预约成功后展示凭证时用）
     *  找不到返回 false
     * ------------------------------------------------------------------ */
    bool fillRow(int doctorId, DoctorRow* rowOut) const;

    /* ------------------------------------------------------------------
     *  号源统计（也供预约服务使用）
     * ------------------------------------------------------------------ */
    void slotStats(int doctorId, int* remainingOut, int* totalOut) const;
    int  availableDayCount(int doctorId) const;

    /* ------------------------------------------------------------------
     *  界面输出
     * ------------------------------------------------------------------ */
    void printRows(const DoctorRow* rows, int count) const;

    /* 打印职称选项菜单，供"按职称查询"界面使用 */
    void printTitleMenu() const;

    /* 打印某医生本周 7 天的号源网格（哪天有号、各时段余几个）。
       返回本周该医生的总剩余号源数；医生不存在时返回 -1。
       本方法原本属于预约服务，预约功能推迟后移到这里 ——
       "排班与号源展示"是本期实现的功能之一。 */
    int printDoctorSchedule(int doctorId) const;

private:
    DataStore* m_store;

    /* 判断一位医生是否满足全部条件 */
    bool matchCondition(const Doctor* d, const QueryCondition& cond) const;
};

#endif /* HMS_DOCTORQUERY_H */
