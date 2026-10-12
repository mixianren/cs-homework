/* ============================================================================
 *  tests/test_triage.cpp  --  triage self-test
 * ----------------------------------------------------------------------------
 *  The algorithm takes symptom indices as input (the user picks symptoms from a
 *  list), so this test selects symptoms directly instead of feeding text.
 *
 *  Every expected value below was computed BY HAND from files/symptom_dept.txt,
 *  not copied from the design document.
 *
 *  Build and run:  build test
 * ==========================================================================*/

#include <cstdio>
#include <cstring>
#include "data/DataStore.h"
#include "file/FileLoader.h"
#include "service/Triage.h"

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

/* helper: look up a symptom index by name, reporting failure clearly */
static int symIndex(DataStore* store, const char* name)
{
    int idx = store->symptomIndexByName(name);
    if (idx < 0) std::printf("    [setup] symptom '%s' not found in dictionary\n", name);
    return idx;
}

/* ==========================================================================
 *  1. scoring - expected values hand-computed from the data file
 * ==========================================================================*/
static void testScore(TriageService* triage, DataStore* store)
{
    std::printf("[1] scoring\n");

    int tooth = symIndex(store, "牙痛");
    CHECK(tooth >= 0, "牙痛 is in the symptom dictionary");

    /* ---- single symptom: score == weight ---- */
    {
        const int idx[1] = { tooth };
        double score = triage->scoreOfDept(6, idx, 1);      /* dept 6 = 口腔科 */
        std::printf("      牙痛 -> 口腔科 score %.2f (expect 0.90)\n", score);
        CHECK(score > 0.89 && score < 0.91, "score for 口腔科 is 0.90");

        /* the data file has no edge from 牙痛 to any other department */
        CHECK(triage->scoreOfDept(1, idx, 1) == 0.0, "no edge 牙痛 -> 内科, score 0");
        CHECK(triage->scoreOfDept(3, idx, 1) == 0.0, "no edge 牙痛 -> 儿科, score 0");
        CHECK(triage->scoreOfDept(9, idx, 1) == 0.0, "no edge 牙痛 -> 神经内科, score 0");
    }

    /* ---- several symptoms accumulate ---- */
    {
        int head  = symIndex(store, "头痛");
        int fever = symIndex(store, "发热");
        int cough = symIndex(store, "咳嗽");
        CHECK(head >= 0 && fever >= 0 && cough >= 0, "all three symptoms exist");

        /* data: 头痛 -> 9:0.85, 1:0.25
                 发热 -> 3:0.80, 1:0.25, 2:0.20
                 咳嗽 -> 1:0.70, 3:0.50                        */
        const int idx[3] = { head, fever, cough };

        double internal = triage->scoreOfDept(1, idx, 3);   /* 内科 */
        std::printf("      头痛+发热+咳嗽 -> 内科 %.2f (expect 1.20)\n", internal);
        CHECK(internal > 1.19 && internal < 1.21, "内科 = 0.25+0.25+0.70 = 1.20");

        double pedia = triage->scoreOfDept(3, idx, 3);      /* 儿科 */
        std::printf("      头痛+发热+咳嗽 -> 儿科 %.2f (expect 1.30)\n", pedia);
        CHECK(pedia > 1.29 && pedia < 1.31, "儿科 = 0.80+0.50 = 1.30");

        double neuro = triage->scoreOfDept(9, idx, 3);      /* 神经内科 */
        CHECK(neuro > 0.84 && neuro < 0.86, "神经内科 = 0.85");

        double surgery = triage->scoreOfDept(2, idx, 3);    /* 外科 */
        CHECK(surgery > 0.19 && surgery < 0.21, "外科 = 0.20");

        /* departments with no edge at all must score exactly 0 */
        CHECK(triage->scoreOfDept(4, idx, 3) == 0.0, "骨科 has no edge, score 0");
        CHECK(triage->scoreOfDept(5, idx, 3) == 0.0, "眼科 has no edge, score 0");
        CHECK(triage->scoreOfDept(6, idx, 3) == 0.0, "口腔科 has no edge, score 0");
        CHECK(triage->scoreOfDept(7, idx, 3) == 0.0, "皮肤科 has no edge, score 0");
        CHECK(triage->scoreOfDept(8, idx, 3) == 0.0, "耳鼻喉科 has no edge, score 0");
        CHECK(triage->scoreOfDept(10, idx, 3) == 0.0, "消化内科 has no edge, score 0");
    }

    /* ---- boundary conditions ---- */
    {
        const int idx[1] = { tooth };
        CHECK(triage->scoreOfDept(6, 0, 0) == 0.0, "zero symptoms -> score 0");
        CHECK(triage->scoreOfDept(6, idx, -1) == 0.0, "negative count -> score 0");
        CHECK(triage->scoreOfDept(-1, idx, 1) == 0.0, "invalid dept -> score 0");
        CHECK(triage->scoreOfDept(999, idx, 1) == 0.0, "unknown dept -> score 0");
    }
}

/* ==========================================================================
 *  2. match percent and ranking
 * ==========================================================================*/
static void testRecommend(TriageService* triage, DataStore* store)
{
    std::printf("[2] match percent and ranking\n");

    /* ---- a single specialist symptom ---- */
    {
        int tooth = symIndex(store, "牙痛");
        const int idx[1] = { tooth };

        TriageResult r;
        int n = triage->recommend(idx, 1, &r, 5);
        std::printf("      [牙痛] -> %d recommendation(s)\n", n);
        CHECK(n == 1, "牙痛 recommends exactly one department");
        if (n >= 1)
        {
            CHECK(r.recs[0].deptId == 6, "top choice is 口腔科");
            CHECK(std::strcmp(r.recs[0].deptName, "口腔科") == 0, "name matches");
            std::printf("      top: %s  %d%%  (expect 90%%)\n",
                        r.recs[0].deptName, r.recs[0].matchPercent);
            CHECK(r.recs[0].matchPercent == 90, "percent = 0.90 / 1 = 90%");
            CHECK(r.recs[0].hitCount == 1, "hit count is 1");
            CHECK(std::strstr(r.recs[0].evidence, "牙痛") != 0, "evidence mentions 牙痛");
        }
    }

    /* ---- three symptoms: full expected table ---- */
    {
        int head  = symIndex(store, "头痛");
        int fever = symIndex(store, "发热");
        int cough = symIndex(store, "咳嗽");
        const int idx[3] = { head, fever, cough };

        TriageResult r;
        int n = triage->recommend(idx, 3, &r, 5);
        std::printf("      [头痛+发热+咳嗽] recommends:\n");
        for (int i = 0; i < n; i++)
            std::printf("        %d. %-10s %3d%%   %s\n",
                        i + 1, r.recs[i].deptName, r.recs[i].matchPercent,
                        r.recs[i].evidence);

        CHECK(n == 4, "four departments have a non-zero score");

        /* expected order: 儿科 43, 内科 40, 神经内科 28, 外科 7 */
        CHECK(r.recs[0].deptId == 3,  "1st is 儿科");
        CHECK(r.recs[0].matchPercent == 43, "儿科 = 1.30/3 = 43%");
        CHECK(r.recs[1].deptId == 1,  "2nd is 内科");
        CHECK(r.recs[1].matchPercent == 40, "内科 = 1.20/3 = 40%");
        CHECK(r.recs[2].deptId == 9,  "3rd is 神经内科");
        CHECK(r.recs[2].matchPercent == 28, "神经内科 = 0.85/3 = 28%");
        CHECK(r.recs[3].deptId == 2,  "4th is 外科");
        CHECK(r.recs[3].matchPercent == 7,  "外科 = 0.20/3 = 7%");
    }

    /* ---- the 牙痛+头痛+发热 example quoted in the design document ---- */
    {
        int tooth = symIndex(store, "牙痛");
        int head  = symIndex(store, "头痛");
        int fever = symIndex(store, "发热");
        const int idx[3] = { tooth, head, fever };

        TriageResult r;
        int n = triage->recommend(idx, 3, &r, 5);
        std::printf("      [牙痛+头痛+发热] recommends:\n");
        for (int i = 0; i < n; i++)
            std::printf("        %d. %-10s %3d%%\n",
                        i + 1, r.recs[i].deptName, r.recs[i].matchPercent);

        /* expected: 口腔科 30, 神经内科 28, 儿科 27, 内科 17, 外科 7 */
        CHECK(n == 5, "five departments have a non-zero score");
        CHECK(r.recs[0].deptId == 6 && r.recs[0].matchPercent == 30, "1st 口腔科 30%");
        CHECK(r.recs[1].deptId == 9 && r.recs[1].matchPercent == 28, "2nd 神经内科 28%");
        CHECK(r.recs[2].deptId == 3 && r.recs[2].matchPercent == 27, "3rd 儿科 27%");
        CHECK(r.recs[3].deptId == 1 && r.recs[3].matchPercent == 17, "4th 内科 17%");
        CHECK(r.recs[4].deptId == 2 && r.recs[4].matchPercent == 7,  "5th 外科 7%");
    }

    /* ---- results must be sorted descending ---- */
    {
        int head  = symIndex(store, "头痛");
        int fever = symIndex(store, "发热");
        int cough = symIndex(store, "咳嗽");
        const int idx[3] = { head, fever, cough };

        TriageResult r;
        triage->recommend(idx, 3, &r, 8);
        bool descending = true;
        for (int i = 1; i < r.recCount; i++)
            if (r.recs[i].matchPercent > r.recs[i - 1].matchPercent) descending = false;
        CHECK(descending, "recommendations sorted by percent, descending");
    }

    /* ---- output must be stable: same input, same order ---- */
    {
        int head  = symIndex(store, "头痛");
        int fever = symIndex(store, "发热");
        int cough = symIndex(store, "咳嗽");
        const int idx[3] = { head, fever, cough };

        TriageResult a, b;
        triage->recommend(idx, 3, &a, 8);
        triage->recommend(idx, 3, &b, 8);

        bool same = (a.recCount == b.recCount);
        for (int i = 0; i < a.recCount && same; i++)
            if (a.recs[i].deptId != b.recs[i].deptId) same = false;
        CHECK(same, "repeated calls give identical order (stable sorting)");
    }

    /* ---- recall: every department is reachable from some symptom ---- */
    {
        std::printf("      per-department reachability:\n");
        bool allReachable = true;
        for (int d = 0; d < store->departmentCount(); d++)
        {
            const Department* dept = store->departments().ptrAt(d);
            if (dept == 0) continue;

            int bestSym = -1;
            for (int s = 0; s < store->symptomCount(); s++)
            {
                if (store->symptomDeptWeight(s, dept->id) >= 0.5) { bestSym = s; break; }
            }
            if (bestSym < 0)
            {
                std::printf("        %-10s no strong symptom\n", dept->name);
                allReachable = false;
                continue;
            }

            const int idx[1] = { bestSym };
            TriageResult r;
            triage->recommend(idx, 1, &r, 5);

            bool found = false;
            for (int i = 0; i < r.recCount; i++)
                if (r.recs[i].deptId == dept->id) found = true;

            std::printf("        %-10s %s\n", dept->name, found ? "reachable" : "NOT reachable");
            if (!found) allReachable = false;
        }
        CHECK(allReachable, "all 10 departments are reachable from some symptom");
    }

    /* ---- precision: a specialist symptom must not be outranked ---- */
    {
        /* 眼干涩 -> 眼科 0.80.  乏力 -> 内科 0.30, 消化内科 0.20.
           Expected: 眼科 0.80/2 = 40% beats 内科 0.30/2 = 15%.
           This shows a specialist symptom is not crowded out by a broad
           department that merely has a weak general association. */
        int dryEye = symIndex(store, "眼干涩");
        int fatigue = symIndex(store, "乏力");
        const int idx[2] = { dryEye, fatigue };

        TriageResult r;
        triage->recommend(idx, 2, &r, 5);
        std::printf("      [眼干涩+乏力] recommends:\n");
        for (int i = 0; i < r.recCount; i++)
            std::printf("        %d. %-10s %3d%%\n",
                        i + 1, r.recs[i].deptName, r.recs[i].matchPercent);
        CHECK(r.recCount >= 1 && r.recs[0].deptId == 5,
              "眼科 tops when a weak general symptom is added");
        CHECK(r.recs[0].matchPercent == 40, "眼科 = 0.80/2 = 40%");
        if (r.recCount >= 2)
        {
            CHECK(r.recs[1].deptId == 1, "内科 is 2nd");
            CHECK(r.recs[1].matchPercent == 15, "内科 = 0.30/2 = 15%");
        }

        /* tie-breaking rule: equal score -> ascending department id.
           眼干涩 0.80 (眼科) and 发热 0.80 (儿科) both give 40%,
           and 儿科 has the smaller id, so it must come first. */
        int fever = symIndex(store, "发热");
        const int tie[2] = { dryEye, fever };
        TriageResult rt;
        triage->recommend(tie, 2, &rt, 5);
        std::printf("      [眼干涩+发热] tie 40%% -> first is %s\n", rt.recs[0].deptName);
        CHECK(rt.recs[0].deptId == 3 && rt.recs[1].deptId == 5,
              "equal score breaks the tie by ascending department id");
    }
}

/* ==========================================================================
 *  3. no-result and boundary handling
 * ==========================================================================*/
static void testEdgeCases(TriageService* triage, DataStore* store)
{
    std::printf("[3] edge cases\n");

    /* ---- null / empty inputs must not crash ---- */
    {
        TriageResult r;
        CHECK(triage->recommend(0, 0, &r, 5) == 0, "null symptom list -> 0");
        CHECK(r.advice[0] != '\0', "an advisory is still produced");

        const int idx[1] = { 0 };
        CHECK(triage->recommend(idx, 0, &r, 5) == 0, "zero symptoms -> 0");
        CHECK(triage->recommend(idx, -3, &r, 5) == 0, "negative count -> 0");
        CHECK(triage->recommend(idx, 1, 0, 5) == 0, "null result pointer -> 0");
    }

    /* ---- topK clamping ---- */
    {
        int head = symIndex(store, "头痛");
        const int idx[1] = { head };

        TriageResult r;
        int n = triage->recommend(idx, 1, &r, 999);
        CHECK(n <= TriageResult::MAX_RECS, "topK is clamped to the array bound");
        CHECK(n >= 1, "at least one result for a valid symptom");
    }

    /* ---- invalid symptom index yields no recommendation ---- */
    {
        const int idx[1] = { 9999 };
        TriageResult r;
        int n = triage->recommend(idx, 1, &r, 5);
        CHECK(n == 0, "invalid symptom index -> 0 recommendations");
        CHECK(r.needManualTriage, "flagged for manual triage");
    }

    /* ---- negative symptom index ---- */
    {
        const int idx[1] = { -5 };
        TriageResult r;
        int n = triage->recommend(idx, 1, &r, 5);
        CHECK(n == 0, "negative symptom index -> 0 recommendations");
    }

    /* ---- too many symptoms for the array: must clamp, not overflow ---- */
    {
        int many[TriageResult::MAX_SYMPTOMS + 10];
        for (int i = 0; i < TriageResult::MAX_SYMPTOMS + 10; i++) many[i] = i % store->symptomCount();

        TriageResult r;
        int n = triage->recommend(many, TriageResult::MAX_SYMPTOMS + 10, &r, 8);
        CHECK(r.symptomCount == TriageResult::MAX_SYMPTOMS,
              "symptom count clamped to MAX_SYMPTOMS");
        CHECK(n <= TriageResult::MAX_RECS, "recommendation count clamped");
    }

    /* ---- single symptom triggers the manual-confirm hint ---- */
    {
        int tooth = symIndex(store, "牙痛");
        const int idx[1] = { tooth };
        TriageResult r;
        triage->recommend(idx, 1, &r, 5);
        CHECK(r.needManualTriage, "one symptom triggers the manual-confirm hint");
    }

    /* ---- two or more symptoms clear that hint ---- */
    {
        int tooth = symIndex(store, "牙痛");
        int head  = symIndex(store, "头痛");
        const int idx[2] = { tooth, head };
        TriageResult r;
        triage->recommend(idx, 2, &r, 5);
        CHECK(!r.needManualTriage, "two symptoms clear the hint");
    }
}

/* ==========================================================================
 *  4. echo text and advisory
 * ==========================================================================*/
static void testOutput(TriageService* triage, DataStore* store)
{
    std::printf("[4] echo and advisory\n");

    int tooth = symIndex(store, "牙痛");
    int head  = symIndex(store, "头痛");
    const int idx[2] = { tooth, head };

    TriageResult r;
    triage->recommend(idx, 2, &r, 5);

    std::printf("      echo: %s\n", r.parsedText);
    CHECK(r.parsedText[0] != '\0', "echo string produced");
    CHECK(std::strstr(r.parsedText, "牙痛") != 0, "echo mentions 牙痛");
    CHECK(std::strstr(r.parsedText, "头痛") != 0, "echo mentions 头痛");
    CHECK(r.symptomCount == 2, "symptom count is 2");
    CHECK(r.advice[0] != '\0', "advisory produced");
    std::printf("      advice: %s\n", r.advice);

    /* every recommendation must carry a non-empty evidence string */
    bool allHaveEvidence = true;
    for (int i = 0; i < r.recCount; i++)
        if (r.recs[i].evidence[0] == '\0') allHaveEvidence = false;
    CHECK(allHaveEvidence, "every recommendation carries evidence");

    /* doctor count must be filled in */
    bool allHaveDoctors = true;
    for (int i = 0; i < r.recCount; i++)
        if (r.recs[i].doctorCount <= 0) allHaveDoctors = false;
    CHECK(allHaveDoctors, "every recommendation reports a doctor count");
}

/* ==========================================================================
 *  5. symptom listing helpers used by the menu
 * ==========================================================================*/
static void testSymptomListing(TriageService* triage, DataStore* store)
{
    std::printf("[5] symptom listing\n");

    int total = triage->printSymptomsByBodyPart();
    std::printf("      listed %d symptoms across all body parts\n", total);
    CHECK(total == store->symptomCount(), "listing covers every symptom");

    /* each of the 7 body parts used by the data must be non-empty */
    int partsWithSymptoms = 0;
    for (int part = 1; part <= 7; part++)
    {
        int c = triage->printSymptomsOfBodyPart(part);
        if (c > 0) partsWithSymptoms++;
    }
    CHECK(partsWithSymptoms == 7, "all 7 body parts are non-empty");

    /* an unused body part returns 0 */
    CHECK(triage->printSymptomsOfBodyPart(0) == 0, "body part 0 returns 0");
    CHECK(triage->printSymptomsOfBodyPart(99) == 0, "invalid body part returns 0");
}

/* ==========================================================================
 *  main
 * ==========================================================================*/
int main()
{
    std::printf("========================================================\n");
    std::printf("  triage self-test -- service/Triage\n");
    std::printf("========================================================\n");

    DataStore* store = new DataStore();
    FileLoader loader(DATA_DIR);
    if (!loader.loadAll(store))
    {
        std::printf("data load failed, cannot continue\n");
        delete store;
        return 1;
    }

    TriageService* triage = new TriageService(store);

    testScore(triage, store);
    testRecommend(triage, store);
    testEdgeCases(triage, store);
    testOutput(triage, store);
    testSymptomListing(triage, store);

    std::printf("\n");
    std::printf("----------------------------------------------------------------------\n");
    std::printf("  sample output\n");
    std::printf("----------------------------------------------------------------------\n");
    {
        int tooth = symIndex(store, "牙痛");
        int head  = symIndex(store, "头痛");
        int fever = symIndex(store, "发热");
        const int idx[3] = { tooth, head, fever };

        TriageResult r;
        triage->recommend(idx, 3, &r, 5);
        triage->printResult(r);

        std::printf("\n  symptom picker sample:\n\n");
        triage->printSymptomsByBodyPart();
        std::printf("\n  body part 1 (头颈):\n\n");
        triage->printSymptomsOfBodyPart(1);
    }

    delete triage;
    delete store;

    std::printf("--------------------------------------------------------\n");
    std::printf("  passed %d, failed %d\n", g_pass, g_fail);
    std::printf("========================================================\n");

    return (g_fail == 0) ? 0 : 1;
}