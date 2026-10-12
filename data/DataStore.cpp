/* ============================================================================
 *  data/DataStore.cpp  --  数据仓库实现
 * ----------------------------------------------------------------------------
 *  声明见 data/DataStore.h。
 *
 *  编码约定：读取顺序表元素统一用 ptrAt()（返回指针，越界得 0），
 *            不用 at()。因为 at() 需要一个"默认值"参数，读元素时
 *            还得先造一个 dummy 对象，既啰嗦又容易写错。
 * ==========================================================================*/

#include "data/DataStore.h"
#include <cstdio>
#include <cstring>

/* ==========================================================================
 *  构造与析构
 * ==========================================================================*/
DataStore::DataStore()
{
    /* 为 0 号医生占位，避免网格下标与医生编号错位 */
    ensureSlotGrid(0);
}

DataStore::~DataStore()
{
}

/* ==========================================================================
 *  加载阶段
 * ==========================================================================*/

void DataStore::clear()
{
    m_departments.clear();
    m_doctors.clear();
    m_symptoms.clear();
    m_schedules.clear();
    m_patients.clear();
    m_appointments.clear();

    m_deptIndex.clear();
    m_doctorIndex.clear();
    m_symptomIndex.clear();
    m_patientIndex.clear();
    m_apptIndex.clear();
    m_scheduleIndex.clear();

    m_deptNameIndex.clear();
    m_doctorNameIndex.clear();
    m_symptomNameIndex.clear();

    m_slotGrid.clear();
    m_symptomDepts.clear();
    ensureSlotGrid(0);
}

void DataStore::reserveFor(int deptCount, int doctorCount, int symptomCount,
                           int scheduleCount, int patientCount, int apptCount)
{
    /* 在实际数量之上留一点余量，让加载过程中顺序表不再扩容。
       这样加载结束后索引就是最终状态，不必重建。 */
    if (deptCount     > 0) m_departments.ensureCapacity(deptCount + 8);
    if (doctorCount   > 0) m_doctors.ensureCapacity(doctorCount + 8);
    if (scheduleCount > 0) m_schedules.ensureCapacity(scheduleCount + 16);
    if (patientCount  > 0) m_patients.ensureCapacity(patientCount + 8);
    if (apptCount     > 0) m_appointments.ensureCapacity(apptCount + 16);

    if (symptomCount > 0)
    {
        m_symptoms.ensureCapacity(symptomCount + 8);
        /* 症状-科室关联表按症状下标组织：先给每个症状放一个空列表，
           这样按下标就能直接取到，不需要每次判空。 */
        m_symptomDepts.ensureCapacity(symptomCount + 8);
        while (m_symptomDepts.size() < symptomCount)
        {
            SeqList<SymptomDeptLink> empty;
            m_symptomDepts.insertBack(empty);
        }
    }
}

void DataStore::addDepartment(const Department& d)
{
    int index = m_departments.insertBack(d);
    m_deptIndex.put(d.id, index);
    m_deptNameIndex.put(d.name, d.id);
}

void DataStore::addDoctor(const Doctor& d)
{
    int index = m_doctors.insertBack(d);
    m_doctorIndex.put(d.id, index);
    m_doctorNameIndex.put(d.name, d.id);

    ensureSlotGrid(d.id);
}

void DataStore::addSymptom(const Symptom& s)
{
    int index = m_symptoms.insertBack(s);
    m_symptomIndex.put(s.index, index);
    m_symptomNameIndex.put(s.name, s.index);
}

void DataStore::addSchedule(const Schedule& s)
{
    int index = m_schedules.insertBack(s);
    m_scheduleIndex.put(scheduleKey(s.doctorId, s.day, s.slot), index);

    /* 同步写入排班网格，使"按医生+日期+时段"的查询变成一次数组下标访问 */
    ensureSlotGrid(s.doctorId);
    int gridIndex = s.doctorId * SLOT_PER_WEEK + Schedule::slotIndex(s.day, s.slot);
    if (gridIndex >= 0 && gridIndex < m_slotGrid.size())
        m_slotGrid.setAt(gridIndex, index);
}

void DataStore::addPatient(const Patient& p)
{
    int index = m_patients.insertBack(p);
    m_patientIndex.put(p.id, index);
}

void DataStore::addAppointment(const Appointment& a)
{
    int index = m_appointments.insertBack(a);
    m_apptIndex.put(a.id, index);
}

void DataStore::addSymptomDept(int symptomIndex, int deptId, double weight)
{
    if (symptomIndex < 0) return;

    /* 下标越界时补齐空列表（正常情况下 reserveFor 已经铺好） */
    while (m_symptomDepts.size() <= symptomIndex)
    {
        SeqList<SymptomDeptLink> empty;
        m_symptomDepts.insertBack(empty);
    }

    SeqList<SymptomDeptLink>* list = m_symptomDepts.ptrAt(symptomIndex);
    if (list == 0) return;

    /* 同一个 (症状, 科室) 重复出现时取较大权重，而不是重复插入 */
    for (int i = 0; i < list->size(); i++)
    {
        SymptomDeptLink* existing = list->ptrAt(i);
        if (existing == 0) continue;

        if (existing->deptId == deptId)
        {
            if (weight > existing->weight) existing->weight = weight;
            return;
        }
    }

    SymptomDeptLink link;
    link.init();
    link.deptId = deptId;
    link.weight = weight;
    list->insertBack(link);
}

/* ==========================================================================
 *  加载收尾检查
 * ==========================================================================*/

/* 把一条问题追加到报告缓冲区，注意不要写越界 */
static void appendProblem(char* buf, int cap, const char* text)
{
    if (buf == 0 || cap <= 0) return;
    int used = (int)std::strlen(buf);
    if (used >= cap - 1) return;
    std::strncat(buf, text, (size_t)(cap - used - 1));
}

bool DataStore::finishLoading(char* problemsOut, int problemsCap)
{
    char buf[256];

    if (problemsOut != 0 && problemsCap > 0) problemsOut[0] = '\0';

    /* ---- 致命问题：直接不能运行 ---- */
    if (m_departments.isEmpty())
    {
        appendProblem(problemsOut, problemsCap, "科室表为空，无法运行\n");
        return false;
    }
    if (m_doctors.isEmpty())
    {
        appendProblem(problemsOut, problemsCap, "医生表为空，无法运行\n");
        return false;
    }

    /* ---- 非致命问题：报告但继续 ---- */

    if (m_departments.size() < 10)
    {
        std::snprintf(buf, sizeof(buf), "科室只有 %d 个，题目要求不少于 10 个\n",
                      m_departments.size());
        appendProblem(problemsOut, problemsCap, buf);
    }

    if (m_doctors.size() < 30)
    {
        std::snprintf(buf, sizeof(buf), "医生只有 %d 人，题目要求不少于 30 人\n",
                      m_doctors.size());
        appendProblem(problemsOut, problemsCap, buf);
    }

    /* 每位医生都应有排班，否则患者在界面上能看到他却永远约不上 */
    for (int i = 0; i < m_doctors.size(); i++)
    {
        const Doctor* d = m_doctors.ptrAt(i);
        if (d == 0) continue;

        bool found = false;
        for (int k = 0; k < m_schedules.size(); k++)
        {
            const Schedule* s = m_schedules.ptrAt(k);
            if (s != 0 && s->doctorId == d->id) { found = true; break; }
        }
        if (!found)
        {
            std::snprintf(buf, sizeof(buf), "医生 %d 没有任何排班记录\n", d->id);
            appendProblem(problemsOut, problemsCap, buf);
            break;      /* 只报第一条，避免刷屏 */
        }
    }

    /* 号源自洽性：已预约数不能超过号源上限 */
    for (int i = 0; i < m_schedules.size(); i++)
    {
        const Schedule* s = m_schedules.ptrAt(i);
        if (s != 0 && s->booked > s->capacity)
        {
            std::snprintf(buf, sizeof(buf),
                          "排班数据异常：医生 %d %s%s 已预约 %d 超过上限 %d\n",
                          s->doctorId, weekdayName(s->day), slotName(s->slot),
                          s->booked, s->capacity);
            appendProblem(problemsOut, problemsCap, buf);
            break;
        }
    }

    /* 症状-科室关联为空则导诊完全失效 */
    bool anyLink = false;
    for (int i = 0; i < m_symptomDepts.size() && !anyLink; i++)
    {
        const SeqList<SymptomDeptLink>* list = m_symptomDepts.ptrAt(i);
        if (list != 0 && !list->isEmpty()) anyLink = true;
    }
    if (!anyLink)
    {
        appendProblem(problemsOut, problemsCap,
                      "症状-科室关联表为空，智能导诊将无法给出结果\n");
    }

    return true;
}

/* ==========================================================================
 *  按编号查找
 * ==========================================================================*/

Department* DataStore::findDepartment(int id)
{
    int index = -1;
    if (!m_deptIndex.get(id, &index)) return 0;
    return m_departments.ptrAt(index);
}

Doctor* DataStore::findDoctor(int id)
{
    int index = -1;
    if (!m_doctorIndex.get(id, &index)) return 0;
    return m_doctors.ptrAt(index);
}

Patient* DataStore::findPatient(int id)
{
    int index = -1;
    if (!m_patientIndex.get(id, &index)) return 0;
    return m_patients.ptrAt(index);
}

Symptom* DataStore::findSymptom(int index)
{
    /* 症状的索引键是它自己的 index 字段 */
    int pos = -1;
    if (!m_symptomIndex.get(index, &pos)) return 0;
    return m_symptoms.ptrAt(pos);
}

Appointment* DataStore::findAppointment(int id)
{
    int index = -1;
    if (!m_apptIndex.get(id, &index)) return 0;
    return m_appointments.ptrAt(index);
}

/* const 版本：内部逻辑相同，只是返回常量指针。
   用 const_cast 复用实现，避免把查询逻辑写两遍。 */
const Department*  DataStore::findDepartment(int id) const  { return const_cast<DataStore*>(this)->findDepartment(id); }
const Doctor*      DataStore::findDoctor(int id) const      { return const_cast<DataStore*>(this)->findDoctor(id); }
const Patient*     DataStore::findPatient(int id) const     { return const_cast<DataStore*>(this)->findPatient(id); }
const Symptom*     DataStore::findSymptom(int index) const  { return const_cast<DataStore*>(this)->findSymptom(index); }
const Appointment* DataStore::findAppointment(int id) const { return const_cast<DataStore*>(this)->findAppointment(id); }

/* ==========================================================================
 *  按名称查找
 * ==========================================================================*/

int DataStore::departmentIdByName(const char* name) const
{
    if (name == 0) return -1;
    return m_deptNameIndex.getOrDefault(name, -1);
}

int DataStore::doctorIdByName(const char* name) const
{
    if (name == 0) return -1;
    return m_doctorNameIndex.getOrDefault(name, -1);
}

int DataStore::symptomIndexByName(const char* name) const
{
    if (name == 0) return -1;
    return m_symptomNameIndex.getOrDefault(name, -1);
}

/* ==========================================================================
 *  排班查找：优先走 O(1) 的网格，网格未命中再查哈希表兜底
 * ==========================================================================*/

Schedule* DataStore::findScheduleByKey(int doctorId, int day, int slot)
{
    if (doctorId < 0 || day < 0 || day > 6) return 0;
    if (slot < 1 || slot > SLOT_COUNT) return 0;

    /* 主通道：排班网格。doctorId * 14 + (day * 2 + slot - 1) */
    int gridIndex = doctorId * SLOT_PER_WEEK + Schedule::slotIndex(day, slot);
    if (gridIndex >= 0 && gridIndex < m_slotGrid.size())
    {
        const int* cell = m_slotGrid.ptrAt(gridIndex);
        if (cell != 0 && *cell >= 0) return m_schedules.ptrAt(*cell);
    }

    /* 兜底通道：直接查哈希索引（网格未覆盖的情况） */
    int tableIndex = -1;
    if (!m_scheduleIndex.get(scheduleKey(doctorId, day, slot), &tableIndex)) return 0;
    return m_schedules.ptrAt(tableIndex);
}

const Schedule* DataStore::findScheduleByKey(int doctorId, int day, int slot) const
{
    return const_cast<DataStore*>(this)->findScheduleByKey(doctorId, day, slot);
}

/* ==========================================================================
 *  号源操作
 * --------------------------------------------------------------------------
 *  题目实现提示："预约管理需要保证号源的并发安全，预约时检查并扣减剩余号源"
 *  这里的做法是把"检查"和"扣减"写在同一个函数里，中间不插入任何其它操作，
 *  调用方只能通过返回值判断成败，无法绕过检查直接改 booked。
 * ==========================================================================*/

bool DataStore::reserveSlot(int doctorId, int day, int slot, int* seqOut)
{
    Schedule* s = findScheduleByKey(doctorId, day, slot);
    if (s == 0) return false;               /* 该时段医生不排班 */
    if (s->remain() <= 0) return false;     /* 号源已满 */

    s->booked = s->booked + 1;              /* 扣减号源 */

    if (seqOut != 0) *seqOut = s->booked;   /* 就诊序号 = 已预约数 */
    return true;
}

void DataStore::releaseSlot(int doctorId, int day, int slot)
{
    Schedule* s = findScheduleByKey(doctorId, day, slot);
    if (s == 0) return;
    if (s->booked > 0) s->booked = s->booked - 1;   /* 归还，但不允许变成负数 */
}

/* ==========================================================================
 *  症状-科室关联
 * ==========================================================================*/

int DataStore::symptomDeptCount(int symptomIndex) const
{
    if (symptomIndex < 0 || symptomIndex >= m_symptomDepts.size()) return 0;
    const SeqList<SymptomDeptLink>* list = m_symptomDepts.ptrAt(symptomIndex);
    return (list == 0) ? 0 : list->size();
}

bool DataStore::symptomDeptAt(int symptomIndex, int k,
                              int* deptIdOut, double* weightOut) const
{
    if (symptomIndex < 0 || symptomIndex >= m_symptomDepts.size()) return false;

    const SeqList<SymptomDeptLink>* list = m_symptomDepts.ptrAt(symptomIndex);
    if (list == 0 || k < 0 || k >= list->size()) return false;

    const SymptomDeptLink* link = list->ptrAt(k);
    if (link == 0) return false;

    if (deptIdOut != 0) *deptIdOut = link->deptId;
    if (weightOut != 0) *weightOut = link->weight;
    return true;
}

double DataStore::symptomDeptWeight(int symptomIndex, int deptId) const
{
    int count = symptomDeptCount(symptomIndex);
    for (int k = 0; k < count; k++)
    {
        int    d = -1;
        double w = 0.0;
        if (symptomDeptAt(symptomIndex, k, &d, &w) && d == deptId) return w;
    }
    return 0.0;
}

double DataStore::deptWeightSquareSum(int deptId) const
{
    double sum = 0.0;
    for (int s = 0; s < m_symptomDepts.size(); s++)
    {
        double w = symptomDeptWeight(s, deptId);
        sum += w * w;
    }
    return sum;
}

/* ==========================================================================
 *  内部工具
 * ==========================================================================*/

void DataStore::ensureSlotGrid(int doctorId)
{
    if (doctorId < 0) return;

    int need = (doctorId + 1) * SLOT_PER_WEEK;
    if (m_slotGrid.size() >= need) return;

    int old = m_slotGrid.size();
    for (int i = old; i < need; i++) m_slotGrid.insertBack(-1);
}

/* ==========================================================================
 *  统计与自检
 * ==========================================================================*/

void DataStore::printSummary() const
{
    int edges = 0;
    for (int i = 0; i < m_symptomDepts.size(); i++) edges += symptomDeptCount(i);

    std::printf("  科室 %d 个 | 医生 %d 人 | 症状 %d 条 | 患者 %d 人\n",
                m_departments.size(), m_doctors.size(),
                m_symptoms.size(), m_patients.size());
    std::printf("  排班 %d 条 | 预约 %d 条 | 症状-科室关联 %d 条\n",
                m_schedules.size(), m_appointments.size(), edges);
    std::printf("  索引条目：科室 %d | 医生 %d | 症状 %d | 患者 %d | 预约 %d | 排班 %d\n",
                m_deptIndex.size(), m_doctorIndex.size(), m_symptomIndex.size(),
                m_patientIndex.size(), m_apptIndex.size(), m_scheduleIndex.size());
    std::printf("  排班网格 %d 格（%d 位医生 × %d 个时段）\n",
                m_slotGrid.size(), m_doctors.size() + 1, SLOT_PER_WEEK);
}

int DataStore::selfCheck(char* reportOut, int reportCap) const
{
    int problems = 0;

    if (reportOut != 0 && reportCap > 0) reportOut[0] = '\0';

    /* ---- 索引条目数必须与主表一致，否则说明有记录漏建索引 ---- */
    if (m_deptIndex.size() != m_departments.size())
    {
        char b[128];
        std::snprintf(b, sizeof(b), "科室索引 %d 条与主表 %d 条不一致\n",
                      m_deptIndex.size(), m_departments.size());
        appendProblem(reportOut, reportCap, b);
    }
    if (m_doctorIndex.size() != m_doctors.size())
    {
        char b[128];
        std::snprintf(b, sizeof(b), "医生索引 %d 条与主表 %d 条不一致\n",
                      m_doctorIndex.size(), m_doctors.size());
        appendProblem(reportOut, reportCap, b);
    }
    if (m_patientIndex.size() != m_patients.size())
    {
        char b[128];
        std::snprintf(b, sizeof(b), "患者索引 %d 条与主表 %d 条不一致\n",
                      m_patientIndex.size(), m_patients.size());
        appendProblem(reportOut, reportCap, b);
    }

    /* ---- 逐条验证编号索引指回的实体确实是自己 ---- */
    for (int i = 0; i < m_departments.size(); i++)
    {
        const Department* d = m_departments.ptrAt(i);
        if (d == 0) continue;

        const Department* found = findDepartment(d->id);
        if (found == 0 || found->id != d->id)
        {
            char b[128];
            std::snprintf(b, sizeof(b), "科室编号索引错位：编号 %d\n", d->id);
            appendProblem(reportOut, reportCap, b);
            break;
        }
    }

    for (int i = 0; i < m_doctors.size(); i++)
    {
        const Doctor* d = m_doctors.ptrAt(i);
        if (d == 0) continue;

        const Doctor* found = findDoctor(d->id);
        if (found == 0 || found->id != d->id)
        {
            char b[128];
            std::snprintf(b, sizeof(b), "医生编号索引错位：编号 %d\n", d->id);
            appendProblem(reportOut, reportCap, b);
            break;
        }
    }

    /* ---- 名称索引抽查：按名字必须查回同一个编号 ---- */
    for (int i = 0; i < m_departments.size(); i++)
    {
        const Department* d = m_departments.ptrAt(i);
        if (d == 0 || d->name[0] == '\0') continue;

        if (departmentIdByName(d->name) != d->id)
        {
            char b[160];
            std::snprintf(b, sizeof(b), "科室名称索引错位：%s\n", d->name);
            appendProblem(reportOut, reportCap, b);
            break;
        }
    }

    for (int i = 0; i < m_symptoms.size(); i++)
    {
        const Symptom* s = m_symptoms.ptrAt(i);
        if (s == 0 || s->name[0] == '\0') continue;

        if (symptomIndexByName(s->name) != s->index)
        {
            char b[160];
            std::snprintf(b, sizeof(b), "症状名称索引错位：%s\n", s->name);
            appendProblem(reportOut, reportCap, b);
            break;
        }
    }

    /* ---- 排班索引与网格必须都能指回主表里的同一条记录 ---- */
    for (int i = 0; i < m_schedules.size(); i++)
    {
        const Schedule* s = m_schedules.ptrAt(i);
        if (s == 0) continue;

        const Schedule* byKey = findScheduleByKey(s->doctorId, s->day, s->slot);
        if (byKey != s)
        {
            char b[160];
            std::snprintf(b, sizeof(b), "排班索引错位：医生 %d %s%s\n",
                          s->doctorId, weekdayName(s->day), slotName(s->slot));
            appendProblem(reportOut, reportCap, b);
            break;
        }
    }

    /* ---- 号源占用不得超过上限 ---- */
    for (int i = 0; i < m_schedules.size(); i++)
    {
        const Schedule* s = m_schedules.ptrAt(i);
        if (s != 0 && s->booked > s->capacity)
        {
            char b[160];
            std::snprintf(b, sizeof(b), "号源越界：医生 %d %s%s 已约 %d / 上限 %d\n",
                          s->doctorId, weekdayName(s->day), slotName(s->slot),
                          s->booked, s->capacity);
            appendProblem(reportOut, reportCap, b);
            break;
        }
    }

    /* ---- 症状-科室关联指向的科室必须存在 ---- */
    for (int s = 0; s < m_symptomDepts.size(); s++)
    {
        int count = symptomDeptCount(s);
        for (int k = 0; k < count; k++)
        {
            int    deptId = -1;
            double w = 0.0;
            if (!symptomDeptAt(s, k, &deptId, &w)) continue;

            if (findDepartment(deptId) == 0 || w <= 0.0 || w > 1.0)
            {
                char b[160];
                std::snprintf(b, sizeof(b),
                              "症状-科室关联异常：症状下标 %d -> 科室 %d 权重 %.2f\n",
                              s, deptId, w);
                appendProblem(reportOut, reportCap, b);
                break;
            }
        }
        if (problems > 0) break;
    }

    return problems;
}
