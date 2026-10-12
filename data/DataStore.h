#ifndef HMS_DATASTORE_H
#define HMS_DATASTORE_H

/* ============================================================================
 *  data/DataStore.h  --  数据仓库
 * ----------------------------------------------------------------------------
 *  对应设计说明书 3.3 节。
 *
 *  职责：集中持有全部数据，并提供按编号 / 名称的快速查找。
 *        业务层（service/）只通过本类访问数据，不直接操作各个顺序表。
 *
 *  【为什么要有这一层】
 *  如果让智能导诊、医生查询、预约管理各自去遍历顺序表，
 *  那么"按编号查医生"的逻辑就会散落在三四个文件里。
 *  集中到 DataStore 之后：
 *      * 查编号的逻辑只有一份，改一处即全部生效；
 *      * 后面加"就诊路线""评价""转诊"等模块时，只往这里加数据和方法，
 *        已有模块一行都不用改。
 *
 *  【索引与主表的关系】
 *  主表（顺序表）存放实体本身；索引（哈希表）只存"编号 -> 下标"。
 *  实体只有一份，索引只记录位置，所以不存在两份数据不同步的问题。
 *
 *  【一个必须注意的前提】
 *  索引里存的是实体在顺序表中的下标，不是指针。因此顺序表扩容
 *  （内部数组重新分配）不会让索引失效 —— 这正是"用下标不用指针"的好处。
 * ==========================================================================*/

#include "ds/SeqList.h"
#include "ds/HashTable.h"
#include "model/Entity.h"

/* ==========================================================================
 *  症状与科室的关联关系（加权二部图的一条边）
 *  保存在症状这一侧：每个症状挂一条"关联科室"的列表。
 * ==========================================================================*/
struct SymptomDeptLink
{
    int    deptId;      /* 关联的科室编号 */
    double weight;      /* 关联强度，0 ~ 1 */

    void init()
    {
        deptId = -1;
        weight = 0.0;
    }
};

/* ==========================================================================
 *  数据仓库
 * ==========================================================================*/
class DataStore
{
public:
    DataStore();
    ~DataStore();

    /* 持有顺序表，禁止拷贝，避免重复释放内存 */
    DataStore(const DataStore&) = delete;
    DataStore& operator=(const DataStore&) = delete;

    /* ==================================================================
     *  加载阶段使用
     * ==================================================================*/

    /* 清空全部数据，准备重新加载 */
    void clear();

    /* 按预期规模预留容量。加载前调用可以避免顺序表反复扩容。 */
    void reserveFor(int deptCount, int doctorCount, int symptomCount,
                    int scheduleCount, int patientCount, int apptCount);

    /* 添加一条记录并建立索引。
       这些函数由 file/FileLoader 在解析文本文件时调用。 */
    void addDepartment(const Department& d);
    void addDoctor(const Doctor& d);
    void addSymptom(const Symptom& s);
    void addSchedule(const Schedule& s);
    void addPatient(const Patient& p);
    void addAppointment(const Appointment& a);

    /* 给某个症状挂一条"关联科室"的边（导诊用） */
    void addSymptomDept(int symptomIndex, int deptId, double weight);

    /* 加载收尾：检查数据完整性，把发现的问题写进 problems。
       返回 false 表示存在致命问题（例如科室表为空），程序不应继续。 */
    bool finishLoading(char* problemsOut, int problemsCap);

    /* ==================================================================
     *  主表访问
     *  返回引用而不是副本，调用方可以直接修改（例如扣减号源）
     * ==================================================================*/
    SeqList<Department>&  departments()  { return m_departments; }
    SeqList<Doctor>&      doctors()      { return m_doctors; }
    SeqList<Symptom>&     symptoms()     { return m_symptoms; }
    SeqList<Schedule>&    schedules()    { return m_schedules; }
    SeqList<Patient>&     patients()     { return m_patients; }
    SeqList<Appointment>& appointments() { return m_appointments; }

    int departmentCount() const { return m_departments.size(); }
    int doctorCount()     const { return m_doctors.size(); }
    int symptomCount()    const { return m_symptoms.size(); }
    int scheduleCount()   const { return m_schedules.size(); }
    int patientCount()    const { return m_patients.size(); }
    int appointmentCount() const { return m_appointments.size(); }

    /* ==================================================================
     *  按编号查找（O(1)，走哈希索引）
     *  找不到返回 0，调用方必须判空
     * ==================================================================*/
    Department*  findDepartment(int id);
    Doctor*      findDoctor(int id);
    Patient*     findPatient(int id);
    Symptom*     findSymptom(int index);
    Appointment* findAppointment(int id);

    const Department*  findDepartment(int id) const;
    const Doctor*      findDoctor(int id) const;
    const Patient*     findPatient(int id) const;
    const Symptom*     findSymptom(int index) const;
    const Appointment* findAppointment(int id) const;

    /* ==================================================================
     *  按名称查找（O(1)），找不到返回 -1
     * ==================================================================*/
    int departmentIdByName(const char* name) const;
    int doctorIdByName(const char* name) const;
    int symptomIndexByName(const char* name) const;

    /* ==================================================================
     *  排班：二维数组定位
     *  下标换算见 Schedule::slotIndex(day, slot)
     *  返回 0 表示该医生此刻不排班
     * ==================================================================*/
    Schedule*       findScheduleByKey(int doctorId, int day, int slot);
    const Schedule* findScheduleByKey(int doctorId, int day, int slot) const;

    /* 简化调用：直接给出层内下标 */
    Schedule*       slotAt(int doctorId, int day, int slot)
    {
        return findScheduleByKey(doctorId, day, slot);
    }

    /* 把排班二维数组铺开：slotGrid[医生编号 * SLOT_PER_WEEK + slotIndex]
       与主表同步维护，查询是 O(1)。 */
    static const int SLOT_PER_WEEK = 7 * SLOT_COUNT;   /* 14 个时段 */

    /* ==================================================================
     *  号源操作
     *  【重要】检查与扣减写在同一个函数里，中间不插入其它操作，
     *          这样就不会出现"检查通过但扣减时已被别人占满"的情况。
     *          题目实现提示要求"预约时检查并扣减剩余号源"，即指此点。
     * ==================================================================*/

    /* 检查并占用一个号源。
       成功返回 true 并给出就诊序号 seqOut（从 1 开始）；
       失败（不排班 / 号源已满）返回 false。 */
    bool reserveSlot(int doctorId, int day, int slot, int* seqOut);

    /* 归还一个号源（取消预约时调用）。
       已经归零时不会变成负数。 */
    void releaseSlot(int doctorId, int day, int slot);

    /* ==================================================================
     *  症状-科室关联（导诊使用）
     * ==================================================================*/
    int symptomDeptCount(int symptomIndex) const;
    /* 取第 k 条关联，越界返回 false */
    bool symptomDeptAt(int symptomIndex, int k,
                       int* deptIdOut, double* weightOut) const;

    /* 查询某症状对某科室的关联强度；无关联返回 0 */
    double symptomDeptWeight(int symptomIndex, int deptId) const;

    /* 某科室在全部症状上的关联强度平方和，供匹配度归一化使用 */
    double deptWeightSquareSum(int deptId) const;

    /* ==================================================================
     *  统计报告
     * ==================================================================*/
    void printSummary() const;

    /* 数据加载后的自检：索引与主表是否一致、号源是否越界等。
       返回发现的问题条数（0 表示全部正常）。 */
    int selfCheck(char* reportOut, int reportCap) const;

private:
    /* ---- 主表 ---- */
    SeqList<Department>  m_departments;
    SeqList<Doctor>      m_doctors;
    SeqList<Symptom>     m_symptoms;
    SeqList<Schedule>    m_schedules;
    SeqList<Patient>     m_patients;
    SeqList<Appointment> m_appointments;

    /* ---- 索引：编号 -> 主表下标 ---- */
    HashTable m_deptIndex;
    HashTable m_doctorIndex;
    HashTable m_symptomIndex;
    HashTable m_patientIndex;
    HashTable m_apptIndex;
    HashTable m_scheduleIndex;      /* 键 = Schedule::slotIndex 组合键 */

    /* ---- 索引：名称 -> 编号 / 下标 ---- */
    StrHashTable m_deptNameIndex;
    StrHashTable m_doctorNameIndex;
    StrHashTable m_symptomNameIndex;

    /* ---- 排班网格：doctorId * SLOT_PER_WEEK + slotIndex ---- */
    SeqList<int> m_slotGrid;        /* 存主表下标，-1 表示该时段不排班 */

    /* ---- 症状-科室关联 ---- */
    SeqList< SeqList<SymptomDeptLink> > m_symptomDepts;

    /* 组合键：把 (医生编号, 星期, 时段) 压成一个整数作为排班索引的键 */
    static int scheduleKey(int doctorId, int day, int slot)
    {
        return (doctorId * SLOT_PER_WEEK) + Schedule::slotIndex(day, slot);
    }

    /* 内部：确保排班网格足够大 */
    void ensureSlotGrid(int doctorId);
};

#endif /* HMS_DATASTORE_H */
