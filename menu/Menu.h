#ifndef HMS_MENU_H
#define HMS_MENU_H

/* ============================================================================
 *  menu/Menu.h  --  界面层
 * ----------------------------------------------------------------------------
 *  对应项目框架.txt 第一、二章。
 *
 *  ----------------------------------------------------------------------------
 *  界面层的两条硬规矩（写在 3.4 节）
 *
 *      * 只负责显示信息和接收用户输入，**不做业务判断**
 *        —— 例如"号源够不够""这个职称合不合法"都由业务层决定，
 *          界面层只把结果显示出来。
 *      * 不直接碰 DataStore 的数据结构细节
 *        —— 需要什么就调用 service/ 里的函数，拿到的都是可以直接打印的结构。
 *
 *  这样做的好处：把 menu/ 整个换掉（比如换成图形界面）不影响业务层。
 *
 *  ----------------------------------------------------------------------------
 *  菜单结构
 *
 *      主菜单
 *        1. 智能导诊        —— 按部位选症状，输出推荐科室
 *        2. 医生查询        —— 按科室 / 职称 / 擅长 / 姓名
 *        3. 排班与号源展示  —— 查某位医生本周的号源网格
 *        0. 退出
 * ==========================================================================*/

#include "data/DataStore.h"
#include "service/Triage.h"
#include "service/DoctorQuery.h"

class Menu
{
public:
    Menu(DataStore* store);

    /* 进入主菜单循环，直到用户选择退出。
       返回 false 表示输入流结束（例如控制台被重定向且已读完），
       此时也应退出。 */
    bool mainLoop();

private:
    DataStore*          m_store;
    TriageService*      m_triage;
    DoctorQueryService* m_query;

    /* ---- 主菜单 ---- */
    void printMainMenu() const;

    /* ---- 各功能 ---- */
    void doTriage();          /* 1. 智能导诊 */
    void doDoctorQuery();     /* 2. 医生查询 */
    void doScheduleView();    /* 3. 排班与号源展示 */

    /* ---- 医生查询的四个入口 ---- */
    void queryByDepartment();
    void queryByTitle();
    void queryByGoodAt();
    void queryByName();

    /* ---- 导诊：选症状 ----
       返回选中的症状个数，结果写入 symptomIndex（容量 MAX_SYMPTOMS）。
       用户中途输入 0 时返回 0。 */
    int pickSymptoms(int* symptomIndex, int maxCount);

    /* 按部位列出症状供选择 */
    void printSymptomPicker() const;
};

#endif /* HMS_MENU_H */
