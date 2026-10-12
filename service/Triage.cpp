/* ============================================================================
 *  service/Triage.cpp  --  导诊推荐实现
 * ----------------------------------------------------------------------------
 *  声明见 service/Triage.h。算法说明也在那里，这里只讲实现上的取舍。
 *
 *  本文件不含任何文本解析：输入是"选中的症状下标"，输出是推荐科室列表。
 *  至于症状下标是勾选来的还是别的方式来的，与算法无关 —— 这正是把
 *  业务逻辑与界面解耦的好处。
 * ==========================================================================*/

#include "service/Triage.h"
#include "ds/Sort.h"
#include "util/Console.h"
#include <cstdio>
#include <cstring>

/* ==========================================================================
 *  构造
 * ==========================================================================*/
TriageService::TriageService(DataStore* store)
{
    m_store = store;
}

/* ==========================================================================
 *  第一步：某个科室的累计得分
 *  就是"把选中症状连到该科室的所有边的权重加起来"
 * ==========================================================================*/
double TriageService::scoreOfDept(int deptId, const int* symptomIndex,
                                  int symptomCount) const
{
    if (m_store == 0 || symptomIndex == 0 || symptomCount <= 0) return 0.0;

    double score = 0.0;
    for (int i = 0; i < symptomCount; i++)
        score += m_store->symptomDeptWeight(symptomIndex[i], deptId);
    return score;
}

/* ==========================================================================
 *  第二、三、四步：打分、折算、排序、取前 K
 * ==========================================================================*/

/* 排序用的临时结构 */
struct ScoreEntry
{
    int    deptId;
    double score;
};

/* 比较函数：按得分降序。得分相同时按科室编号升序，
   保证输出顺序稳定、可复现（不会因为排序不稳定而每次结果不同）。 */
static int compareScoreDesc(const void* a, const void* b)
{
    const ScoreEntry* x = (const ScoreEntry*)a;
    const ScoreEntry* y = (const ScoreEntry*)b;

    if (x->score > y->score) return -1;
    if (x->score < y->score) return  1;
    if (x->deptId < y->deptId) return -1;
    if (x->deptId > y->deptId) return  1;
    return 0;
}

int TriageService::recommend(const int* symptomIndex, int symptomCount,
                             TriageResult* result, int topK) const
{
    if (result == 0) return 0;

    result->init();

    if (m_store == 0 || symptomIndex == 0 || symptomCount <= 0)
    {
        result->needManualTriage = true;
        std::snprintf(result->advice, TEXT_LEN,
                      "尚未选择症状，请先选择至少一个症状");
        return 0;
    }

    if (topK <= 0) topK = 5;
    if (topK > TriageResult::MAX_RECS) topK = TriageResult::MAX_RECS;

    /* 记录症状下标并生成回显文字 */
    int n = (symptomCount < TriageResult::MAX_SYMPTOMS)
              ? symptomCount : TriageResult::MAX_SYMPTOMS;
    result->symptomCount = n;

    for (int i = 0; i < n; i++)
    {
        result->symptomIndex[i] = symptomIndex[i];

        const Symptom* s = m_store->findSymptom(symptomIndex[i]);
        if (s == 0) continue;

        if (result->parsedText[0] != '\0')
            std::strncat(result->parsedText, "、",
                         TEXT_LEN - std::strlen(result->parsedText) - 1);
        std::strncat(result->parsedText, s->name,
                     TEXT_LEN - std::strlen(result->parsedText) - 1);
    }

    /* ---------------- 为每个科室累计得分 ---------------- */
    SeqList<Department>& depts = m_store->departments();

    static ScoreEntry entries[128];
    int entryCount = 0;

    for (int d = 0; d < depts.size() && entryCount < 128; d++)
    {
        const Department* dept = depts.ptrAt(d);
        if (dept == 0) continue;

        double score = scoreOfDept(dept->id, symptomIndex, n);
        if (score <= 0.0) continue;       /* 完全没关联的科室不参与排序 */

        entries[entryCount].deptId = dept->id;
        entries[entryCount].score  = score;
        entryCount++;
    }

    if (entryCount == 0)
    {
        result->needManualTriage = true;
        std::snprintf(result->advice, TEXT_LEN,
                      "选中的症状没有匹配到任何科室，建议前往导诊台人工分诊");
        return 0;
    }

    /* ---------------- 排序：用自研快速排序 ---------------- */
    quickSort(entries, entryCount, sizeof(ScoreEntry), compareScoreDesc);

    /* ---------------- 填充前 topK 名 ---------------- */
    int fill = (entryCount < topK) ? entryCount : topK;
    for (int i = 0; i < fill; i++)
    {
        DeptRecommendation& rec = result->recs[i];
        rec.init();

        const Department* dept = m_store->findDepartment(entries[i].deptId);
        if (dept == 0) continue;

        rec.deptId = dept->id;
        rec.floor  = dept->floor;
        std::snprintf(rec.deptName, NAME_LEN, "%s", dept->name);

        /* 该科医生数 */
        for (int k = 0; k < m_store->doctors().size(); k++)
        {
            const Doctor* doc = m_store->doctors().ptrAt(k);
            if (doc != 0 && doc->deptId == dept->id) rec.doctorCount++;
        }

        rec.score = entries[i].score;
        /* 匹配度 = 累计得分 / 选中症状数，换算成百分数 */
        rec.matchPercent = (int)(entries[i].score / (double)n * 100.0 + 0.5);

        /* 推荐依据：列出命中该科的每个症状及其权重 */
        for (int s = 0; s < n; s++)
        {
            double w = m_store->symptomDeptWeight(symptomIndex[s], dept->id);
            if (w <= 0.0) continue;

            const Symptom* sym = m_store->findSymptom(symptomIndex[s]);
            if (sym == 0) continue;

            rec.hitCount++;

            char piece[64];
            std::snprintf(piece, sizeof(piece), "%s%s %.2f",
                          (rec.evidence[0] != '\0') ? " + " : "",
                          sym->name, w);
            appendEvidence(rec.evidence, TEXT_LEN, piece);
        }
    }
    result->recCount = fill;

    /* ---------------- 生成建议语 ---------------- */
    if (fill > 0)
    {
        const DeptRecommendation& top = result->recs[0];
        std::snprintf(result->advice, TEXT_LEN,
                      "建议首选【%s】（%d 层），匹配度 %d%%",
                      top.deptName, top.floor, top.matchPercent);
    }

    /* 只选了一个症状时，匹配度参考价值有限，提示可以人工确认 */
    result->needManualTriage = (n < 2);

    return fill;
}

/* ==========================================================================
 *  辅助
 * ==========================================================================*/
void TriageService::appendEvidence(char* buf, int cap, const char* text)
{
    if (buf == 0 || text == 0 || cap <= 0) return;
    int used = (int)std::strlen(buf);
    if (used >= cap - 1) return;
    std::strncat(buf, text, (size_t)(cap - used - 1));
}

void TriageService::printResult(const TriageResult& result) const
{
    if (result.parsedText[0] != '\0')
        std::printf("  您选择的症状：%s\n\n", result.parsedText);

    if (result.recCount <= 0)
    {
        std::printf("  %s\n", result.advice);
        return;
    }

    /* 列宽（按显示宽度算） */
    const int W_NO    = 4;
    const int W_NAME  = 10;
    const int W_MATCH = 8;
    const int W_DOC   = 6;
    const int W_EVID  = 24;

    #define HMS_LINE() do { \
        std::printf("  +"); \
        for (int i = 0; i < W_NO    + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_NAME  + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_MATCH + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_DOC   + 2; i++) std::printf("-"); \
        std::printf("+"); \
        for (int i = 0; i < W_EVID  + 2; i++) std::printf("-"); \
        std::printf("+\n"); \
    } while (0)

    HMS_LINE();
    std::printf("  | ");
    printField("序号", W_NO);      std::printf(" | ");
    printField("科室", W_NAME);    std::printf(" | ");
    printField("匹配度", W_MATCH); std::printf(" | ");
    printField("医生", W_DOC);     std::printf(" | ");
    printField("推荐依据", W_EVID); std::printf(" |\n");
    HMS_LINE();

    char numBuf[16];
    char matchBuf[16];
    char docBuf[16];

    for (int i = 0; i < result.recCount; i++)
    {
        const DeptRecommendation& r = result.recs[i];

        std::snprintf(numBuf,   sizeof(numBuf),   "%d",     i + 1);
        std::snprintf(matchBuf, sizeof(matchBuf), "%d%%",   r.matchPercent);
        std::snprintf(docBuf,   sizeof(docBuf),   "%d 人",  r.doctorCount);

        std::printf("  | ");
        printField(numBuf, W_NO);       std::printf(" | ");
        printField(r.deptName, W_NAME); std::printf(" | ");
        printField(matchBuf, W_MATCH);  std::printf(" | ");
        printField(docBuf, W_DOC);      std::printf(" | ");
        printField(r.evidence, W_EVID); std::printf(" |\n");
    }
    HMS_LINE();

    #undef HMS_LINE

    if (result.advice[0] != '\0')
        std::printf("  %s\n", result.advice);

    if (result.needManualTriage)
        std::printf("  提示：只选了一个症状，推荐结果仅供参考，建议向导诊台确认。\n");
}

/* ==========================================================================
 *  症状列表打印（供"选择症状"界面使用）
 * ==========================================================================*/

/* 部位码 1..7 对应的名称，与 Entity.h 里的 bodyPartName 保持一致 */
static const int BODY_PART_COUNT = 7;

int TriageService::printSymptomsByBodyPart() const
{
    if (m_store == 0) return 0;

    std::printf("  ┌──────┬──────────────┬────────┐\n");
    std::printf("  | ");
    printField("序号", 4); std::printf(" | ");
    printField("部位", 12); std::printf(" | ");
    printField("症状数", 6); std::printf(" |\n");
    std::printf("  +------+--------------+--------+\n");

    int total = 0;
    for (int part = 1; part <= BODY_PART_COUNT; part++)
    {
        int count = 0;
        for (int i = 0; i < m_store->symptomCount(); i++)
        {
            const Symptom* s = m_store->symptoms().ptrAt(i);
            if (s != 0 && s->bodyPart == part) count++;
        }
        if (count == 0) continue;

        char numBuf[8];
        char cntBuf[16];
        std::snprintf(numBuf, sizeof(numBuf), "%d", part);
        std::snprintf(cntBuf, sizeof(cntBuf), "%d 项", count);

        std::printf("  | ");
        printField(numBuf, 4);             std::printf(" | ");
        printField(bodyPartName(part), 12); std::printf(" | ");
        printField(cntBuf, 6);             std::printf(" |\n");

        total += count;
    }
    std::printf("  +------+--------------+--------+\n");
    return total;
}

int TriageService::printSymptomsOfBodyPart(int bodyPart) const
{
    if (m_store == 0) return 0;

    /* 收集该部位的症状 */
    int indices[128];
    int count = 0;
    for (int i = 0; i < m_store->symptomCount() && count < 128; i++)
    {
        const Symptom* s = m_store->symptoms().ptrAt(i);
        if (s != 0 && s->bodyPart == bodyPart) indices[count++] = s->index;
    }

    if (count == 0) return 0;

    /* 每行排 3 条，按显示宽度对齐。
       列宽取 16：最长标签形如 `1.记忆力减退` = 序号2 + 5个汉字10 = 12 列，
       留 4 列间隙避免相邻标签粘在一起。 */
    const int W_NAME = 16;
    for (int i = 0; i < count; i++)
    {
        const Symptom* s = m_store->findSymptom(indices[i]);
        if (s == 0) continue;

        char label[32];
        std::snprintf(label, sizeof(label), "%d.%s", i + 1, s->name);

        if (i % 3 == 0) std::printf("   ");
        printField(label, W_NAME);
        if (i % 3 == 2 || i == count - 1) std::printf("\n");
    }
    return count;
}
