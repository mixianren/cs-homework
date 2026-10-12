#ifndef HMS_TRIAGE_H
#define HMS_TRIAGE_H

/* ============================================================================
 *  service/Triage.h  --  导诊推荐
 * ----------------------------------------------------------------------------
 *  对应设计说明书 2.4 节。这是本题的核心功能 —— 题目在【实现提示】里
 *  明确指出："症状与科室的关联关系可以用加权二部图表示……智能导诊可基于
 *  加权二部图设计算法，将患者症状与科室描述进行匹配。"
 *
 *  ----------------------------------------------------------------------------
 *  输入方式：从症状列表中选择
 *
 *  本实现采用"从症状列表勾选"而不是"患者手打一段描述"，理由是：
 *      * 界面层只需把选中的症状下标传进来，不需要任何文本解析；
 *      * 不存在"哪个科"打错字、口语夹字导致识别失败的困扰；
 *      * 演示时不必依赖机房的输入法，选中即有结果。
 *
 *  两条输入路径在算法上是同一个函数 —— 本类只认"症状下标数组"，
 *  至于这些下标是怎么来的（勾选、或将来改成文本解析）与算法无关。
 *  这是"业务逻辑与界面解耦"的直接体现。
 *
 *  ----------------------------------------------------------------------------
 *  算法总览（四步，刻意保持简单）
 *
 *    第一步  科室打分
 *            对选中的每个症状 s，取其关联表，对每条边 (科室 d, 权重 w)：
 *                score[d] = score[d] + w
 *
 *    第二步  折算匹配度
 *            match[d] = score[d] / (选中症状数) × 100%
 *
 *    第三步  排序
 *            按 match 降序；得分相同时按科室编号升序，
 *            保证输出顺序稳定、可复现
 *
 *    第四步  取前 K 名
 *            默认输出前 5 名，最多 8 名
 *
 *  ----------------------------------------------------------------------------
 *  关于权重的一个关键设计（写在数据里，不写在代码里）
 *
 *    如果"内科"这类综合科室被标注了大量常见症状、权重又不压低，那么无论
 *    患者选什么，内科的累加分都会很高 —— 连"牙痛"都会先推荐内科。
 *
 *    本工程的对策是在 symptom_dept.txt 里把综合科室的权重压到 0.15~0.30，
 *    把专科（口腔科、眼科、皮肤科）给到 0.80~0.95。累加后专科自然排在前面。
 *
 *    这是纯数据层面的调整，不改一行代码。答辩时可以直接讲：
 *    "老师若问为什么牙痛不推内科 —— 因为内科对牙痛根本没有关联边，
 *      而口腔科有 0.90。"
 * ==========================================================================*/

#include "data/DataStore.h"

/* ==========================================================================
 *  一条科室推荐结果
 * ==========================================================================*/
struct DeptRecommendation
{
    int    deptId;          /* 科室编号 */
    char   deptName[NAME_LEN];
    int    floor;
    int    doctorCount;     /* 该科医生数，供界面展示 */

    double score;           /* 累计得分 = Σ 权重 */
    int    matchPercent;    /* 匹配度百分数 = score / 选中症状数 × 100 */

    int    hitCount;        /* 命中了该科几个症状 */
    char   evidence[TEXT_LEN];  /* 推荐依据，如 "牙痛 0.90"，让患者看得见理由 */

    void init()
    {
        deptId       = -1;
        floor        = 1;
        doctorCount  = 0;
        score        = 0.0;
        matchPercent = 0;
        hitCount     = 0;
        deptName[0]  = '\0';
        evidence[0]  = '\0';
    }
};

/* ==========================================================================
 *  一次导诊的完整结果
 * ==========================================================================*/
struct TriageResult
{
    static const int MAX_SYMPTOMS = 16;   /* 一次最多选 16 个症状 */
    static const int MAX_RECS     = 8;    /* 最多给出 8 条推荐 */

    /* 本次选中的症状下标 */
    int  symptomIndex[MAX_SYMPTOMS];
    int  symptomCount;

    /* 选中症状的拼接文字，如 "牙痛、头痛" —— 回显给患者确认 */
    char parsedText[TEXT_LEN];

    /* 推荐结果，已按匹配度降序排列 */
    DeptRecommendation recs[MAX_RECS];
    int recCount;

    /* 是否建议人工分诊（选得太少，推荐参考价值有限） */
    bool needManualTriage;

    /* 给患者的提示语 */
    char advice[TEXT_LEN];

    void init()
    {
        for (int i = 0; i < MAX_SYMPTOMS; i++) symptomIndex[i] = -1;
        symptomCount     = 0;
        parsedText[0]    = '\0';
        for (int i = 0; i < MAX_RECS; i++) recs[i].init();
        recCount         = 0;
        needManualTriage = false;
        advice[0]        = '\0';
    }
};

/* ==========================================================================
 *  导诊服务
 * ==========================================================================*/
class TriageService
{
public:
    explicit TriageService(DataStore* store);

    /* ------------------------------------------------------------------
     *  核心：给定选中的症状，输出前 topK 名推荐科室
     *  topK <= 0 时用默认值 5，超过上限自动收敛到 MAX_RECS。
     *  返回推荐个数，结果写入 result。
     * ------------------------------------------------------------------ */
    int recommend(const int* symptomIndex, int symptomCount,
                  TriageResult* result, int topK = 0) const;

    /* 便捷版：从 DataStore 按症状下标直接推荐（界面层最常用） */
    int recommendByIndex(const int* symptomIndex, int symptomCount,
                         TriageResult* result, int topK = 0) const
    {
        return recommend(symptomIndex, symptomCount, result, topK);
    }

    /* ------------------------------------------------------------------
     *  单独取某个科室的累计得分
     *  供解释"为什么推荐它"以及测试核对使用
     * ------------------------------------------------------------------ */
    double scoreOfDept(int deptId, const int* symptomIndex, int symptomCount) const;

    /* ------------------------------------------------------------------
     *  界面辅助
     * ------------------------------------------------------------------ */

    /* 打印推荐结果表格（含回显、依据、建议语）
       中文按显示宽度对齐，见 .cpp 里的说明 */
    void printResult(const TriageResult& result) const;

    /* 按部位分组打印症状列表，供"选择症状"界面使用。
       返回列出的症状个数。 */
    int printSymptomsByBodyPart() const;

    /* 打印某一部位下的全部症状，供二级菜单使用。
       返回该部位的症状个数，越界部位返回 0。 */
    int printSymptomsOfBodyPart(int bodyPart) const;

private:
    DataStore* m_store;

    /* 内部：把一条证据追加到字符串（带长度保护） */
    static void appendEvidence(char* buf, int cap, const char* text);
};

#endif /* HMS_TRIAGE_H */
