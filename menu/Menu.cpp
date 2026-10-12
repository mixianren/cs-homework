/* ============================================================================
 *  menu/Menu.cpp  --  界面层实现
 * ----------------------------------------------------------------------------
 *  声明见 menu/Menu.h。
 *
 *  本文件只做三件事：显示信息、接收输入、把输入转成对业务层的调用。
 *  任何"这个能不能做""这个值合不合法"的判断都在业务层。
 * ==========================================================================*/

#include "menu/Menu.h"
#include "util/Console.h"
#include <cstdio>
#include <cstring>

/* ==========================================================================
 *  构造
 * ==========================================================================*/
Menu::Menu(DataStore* store)
{
    m_store  = store;
    m_triage = new TriageService(store);
    m_query  = new DoctorQueryService(store);
}

/* ==========================================================================
 *  主菜单
 * ==========================================================================*/
void Menu::printMainMenu() const
{
    std::printf("\n");
    printRule(70);
    std::printf("  主菜单\n");
    printRule(70);
    std::printf("\n");
    std::printf("    1. 智能导诊          —— 按症状推荐科室\n");
    std::printf("    2. 医生查询          —— 按科室 / 职称 / 擅长 / 姓名\n");
    std::printf("    3. 排班与号源展示    —— 查看医生本周号源\n");
    std::printf("\n");
    std::printf("    0. 退出系统\n");
    std::printf("\n");
    printRule(70);
}

bool Menu::mainLoop()
{
    while (true)
    {
        printMainMenu();

        int choice = 0;
        if (!promptIntInRange("  请选择：", 0, 3, &choice))
        {
            /* 输入无效或流已结束。若是 EOF，readLine 返回 false，
               promptInt 会报"输入无效" —— 这里再做一次判断：
               如果标准输入已经结束，就没有继续的必要了。 */
            if (std::feof(stdin)) return false;
            continue;
        }

        switch (choice)
        {
            case 1: doTriage();        break;
            case 2: doDoctorQuery();   break;
            case 3: doScheduleView();  break;
            case 0:
                std::printf("\n  感谢使用，再见。\n\n");
                return true;
            default: break;
        }
    }
}

/* ==========================================================================
 *  1. 智能导诊
 * ==========================================================================*/
void Menu::printSymptomPicker() const
{
    std::printf("\n");
    printRule(70);
    std::printf("  请选择症状所属部位\n");
    printRule(70);
    std::printf("\n");

    /* 按部位统计症状数 —— 这是纯粹的展示需求，直接读数据即可 */
    SeqList<Symptom>& symptoms = m_store->symptoms();

    for (int part = 1; part <= 7; part++)
    {
        int count = 0;
        for (int i = 0; i < symptoms.size(); i++)
        {
            const Symptom* s = symptoms.ptrAt(i);
            if (s != 0 && s->bodyPart == part) count++;
        }
        if (count == 0) continue;

        char label[64];
        std::snprintf(label, sizeof(label), "%d. %s", part, bodyPartName(part));
        std::printf("    ");
        printField(label, 18);
        std::printf("%d 项\n", count);
    }

    std::printf("\n    0. 返回主菜单\n");
    printRule(70);
}

int Menu::pickSymptoms(int* symptomIndex, int maxCount)
{
    if (symptomIndex == 0 || maxCount <= 0) return 0;

    /* ---- 第一步：选部位 ---- */
    printSymptomPicker();

    int part = 0;
    if (!promptIntInRange("  请选择部位：", 0, 7, &part)) return 0;
    if (part == 0) return 0;

    /* ---- 第二步：列出该部位的症状 ---- */
    SeqList<Symptom>& symptoms = m_store->symptoms();

    int  partIndices[64];
    int  partCount = 0;

    for (int i = 0; i < symptoms.size() && partCount < 64; i++)
    {
        const Symptom* s = symptoms.ptrAt(i);
        if (s != 0 && s->bodyPart == part) partIndices[partCount++] = s->index;
    }

    if (partCount == 0)
    {
        std::printf("  该部位下没有症状。\n");
        return 0;
    }

    std::printf("\n");
    printRule(70);
    std::printf("  【%s】请选择症状\n", bodyPartName(part));
    printRule(70);
    std::printf("\n");

    /* 每行 3 个，按显示宽度对齐 */
    for (int i = 0; i < partCount; i++)
    {
        const Symptom* s = m_store->findSymptom(partIndices[i]);
        if (s == 0) continue;

        char label[64];
        std::snprintf(label, sizeof(label), "%d.%s", i + 1, s->name);

        if (i % 3 == 0) std::printf("    ");
        printField(label, 18);
        if (i % 3 == 2 || i == partCount - 1) std::printf("\n");
    }

    std::printf("\n    可一次选择多个，用空格分隔，例如：1 3\n");
    std::printf("    输入 0 返回主菜单\n");
    printRule(70);

    /* ---- 第三步：读一行并解析出若干编号 ---- */
    char line[128];
    if (!promptLine("\n  请输入症状编号：", line, sizeof(line))) return 0;

    /* 逐个取出空白分隔的整数；非数字的片段跳过并提示 */
    int count = 0;
    int i     = 0;

    while (line[i] != '\0' && count < maxCount)
    {
        while (line[i] == ' ' || line[i] == '\t') i++;
        if (line[i] == '\0') break;

        /* 解析一个整数 */
        int value = 0;
        bool isNumber = false;

        while (line[i] >= '0' && line[i] <= '9')
        {
            value = value * 10 + (line[i] - '0');
            i++;
            isNumber = true;
        }

        /* 跳过这个片段剩余的非空白字符 */
        bool hadGarbage = false;
        while (line[i] != '\0' && line[i] != ' ' && line[i] != '\t')
        {
            hadGarbage = true;
            i++;
        }

        if (!isNumber || hadGarbage)
        {
            std::printf("  ⚠ 忽略无法识别的输入：不是有效的编号\n");
            continue;
        }

        if (value == 0) return 0;                  /* 用户要求返回 */

        if (value < 1 || value > partCount)
        {
            std::printf("  ⚠ 忽略越界编号 %d（应为 1 ~ %d）\n", value, partCount);
            continue;
        }

        int idx = partIndices[value - 1];

        /* 去重：同一个症状选两次只算一次 */
        bool already = false;
        for (int k = 0; k < count; k++)
            if (symptomIndex[k] == idx) { already = true; break; }

        if (already)
        {
            const Symptom* s = m_store->findSymptom(idx);
            std::printf("  ⚠ 「%s」已选过，忽略重复\n", (s != 0) ? s->name : "?");
            continue;
        }

        symptomIndex[count++] = idx;
    }

    return count;
}

void Menu::doTriage()
{
    int symptomIndex[TriageResult::MAX_SYMPTOMS];

    int count = pickSymptoms(symptomIndex, TriageResult::MAX_SYMPTOMS);

    if (count == 0)
    {
        std::printf("\n  未选择任何症状，已返回。\n");
        return;
    }

    /* 交给业务层算，界面层不参与任何打分或排序 */
    TriageResult result;
    m_triage->recommend(symptomIndex, count, &result, 5);

    std::printf("\n");
    printRule(70);
    std::printf("  导诊结果\n");
    printRule(70);
    std::printf("\n");

    m_triage->printResult(result);

    pause();
}

/* ==========================================================================
 *  2. 医生查询
 * ==========================================================================*/
void Menu::doDoctorQuery()
{
    while (true)
    {
        std::printf("\n");
        printRule(70);
        std::printf("  医生查询\n");
        printRule(70);
        std::printf("\n");
        std::printf("    1. 按科室查询        —— 列出某科室的全部医生\n");
        std::printf("    2. 按职称查询        —— 列出某职称的全部医生\n");
        std::printf("    3. 按擅长领域查询    —— 例如「高血压」「牙」\n");
        std::printf("    4. 按姓名查询        —— 支持只输入姓氏\n");
        std::printf("\n");
        std::printf("    0. 返回主菜单\n");
        std::printf("\n");
        printRule(70);

        int choice = 0;
        if (!promptIntInRange("  请选择：", 0, 4, &choice))
        {
            if (std::feof(stdin)) return;
            continue;
        }

        switch (choice)
        {
            case 1: queryByDepartment(); break;
            case 2: queryByTitle();      break;
            case 3: queryByGoodAt();     break;
            case 4: queryByName();       break;
            case 0: return;
            default: break;
        }
    }
}

/* ---- 公共：列出科室清单供选择 ---- */
static void listDepartments(DataStore* store)
{
    std::printf("\n");
    SeqList<Department>& depts = store->departments();
    for (int i = 0; i < depts.size(); i++)
    {
        const Department* d = depts.ptrAt(i);
        if (d == 0) continue;

        char label[64];
        std::snprintf(label, sizeof(label), "%d. %s", d->id, d->name);

        /* 列宽 16：最长标签形如 "10. 消化内科" = 4 + 8 = 12 列，留 4 列间隙 */
        std::printf("    ");
        printField(label, 16);
        if (i % 3 == 2 || i == depts.size() - 1) std::printf("\n");
    }
    std::printf("\n");
}

void Menu::queryByDepartment()
{
    std::printf("\n  可选科室：\n");
    listDepartments(m_store);

    int deptId = 0;
    if (!promptInt("  请输入科室编号（0 返回）：", &deptId)) return;
    if (deptId == 0) return;

    DoctorRow rows[128];
    int count = m_query->byDepartment(deptId, rows, 128);

    std::printf("\n");
    if (count == 0)
    {
        /* 区分"科室不存在"与"科室没有医生" —— 前者提示编号错，后者不会发生 */
        m_query->printRows(rows, 0);
        std::printf("  （科室编号 %d 可能不存在，请核对上面的清单）\n", deptId);
    }
    else
    {
        m_query->printRows(rows, count);
    }

    pause();
}

void Menu::queryByTitle()
{
    std::printf("\n  可选职称：\n\n");
    m_query->printTitleMenu();
    std::printf("\n");

    int title = 0;
    if (!promptIntInRange("  请输入职称编号（0 返回）：", 0, 4, &title)) return;
    if (title == 0) return;

    DoctorRow rows[128];
    int count = m_query->byTitle(title, rows, 128);

    std::printf("\n");
    m_query->printRows(rows, count);

    pause();
}

void Menu::queryByGoodAt()
{
    char keyword[NAME_LEN];
    if (!promptLine("\n  请输入擅长领域关键字（如 高血压、牙、头痛；直接回车返回）：",
                    keyword, sizeof(keyword)))
        return;

    if (keyword[0] == '\0') return;

    DoctorRow rows[128];
    int count = m_query->byGoodAt(keyword, rows, 128);

    std::printf("\n");
    if (count == 0)
        std::printf("  没有医生的擅长领域包含「%s」。\n", keyword);
    else
        m_query->printRows(rows, count);

    pause();
}

void Menu::queryByName()
{
    char keyword[NAME_LEN];
    if (!promptLine("\n  请输入医生姓名或姓氏（直接回车返回）：",
                    keyword, sizeof(keyword)))
        return;

    if (keyword[0] == '\0') return;

    DoctorRow rows[128];
    int count = m_query->byName(keyword, rows, 128);

    std::printf("\n");
    if (count == 0)
        std::printf("  没有找到姓名包含「%s」的医生。\n", keyword);
    else
        m_query->printRows(rows, count);

    pause();
}

/* ==========================================================================
 *  3. 排班与号源展示
 * ==========================================================================*/
void Menu::doScheduleView()
{
    std::printf("\n");
    printRule(70);
    std::printf("  排班与号源展示\n");
    printRule(70);
    std::printf("\n");
    std::printf("    输入医生编号查看其本周号源；输入 0 查看全部医生的汇总。\n");
    std::printf("    （医生编号可先在「医生查询」里查到）\n\n");

    int doctorId = 0;
    if (!promptInt("  请输入医生编号：", &doctorId)) return;

    if (doctorId != 0)
    {
        std::printf("\n");
        m_query->printDoctorSchedule(doctorId);
        pause();
        return;
    }

    /* ---- 全部医生的号源汇总 ---- */
    DoctorRow rows[128];
    int count = m_query->all(rows, 128);

    std::printf("\n");
    printRule(70);
    std::printf("  全院号源汇总（按科室）\n");
    printRule(70);

    SeqList<Department>& depts = m_store->departments();
    int grandRemaining = 0;
    int grandTotal     = 0;

    for (int d = 0; d < depts.size(); d++)
    {
        const Department* dept = depts.ptrAt(d);
        if (dept == 0) continue;

        int deptRemaining = 0;
        int deptTotal     = 0;
        int doctorCount   = 0;

        for (int i = 0; i < count; i++)
        {
            if (rows[i].deptId != dept->id) continue;
            deptRemaining += rows[i].remainingSlots;
            deptTotal     += rows[i].totalSlots;
            doctorCount++;
        }

        if (doctorCount == 0) continue;

        char label[64];
        std::snprintf(label, sizeof(label), "  %s", dept->name);

        printField(label, 16);
        std::printf("医生 %d 人   " , doctorCount);

        char slotText[64];
        std::snprintf(slotText, sizeof(slotText), "余 %d / 共 %d",
                      deptRemaining, deptTotal);
        printField(slotText, 20);

        /* 占用率用等号条粗略表示，一眼看出紧张程度 */
        int ratio = (deptTotal > 0)
                      ? (int)((deptTotal - deptRemaining) * 100 / deptTotal)
                      : 0;
        std::printf("已约 %3d%%  ", ratio);
        for (int k = 0; k < ratio / 5; k++) std::printf("=");
        std::printf("\n");

        grandRemaining += deptRemaining;
        grandTotal     += deptTotal;
    }

    printRule(70);
    std::printf("  全院合计：余 %d / 共 %d 个号源\n", grandRemaining, grandTotal);
    printRule(70);

    pause();
}
