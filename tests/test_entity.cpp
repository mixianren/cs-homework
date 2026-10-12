/* ============================================================================
 *  tests/test_entity.cpp  ——  数据实体自检程序
 * ----------------------------------------------------------------------------
 *  验证 model/Entity.h 里 5 个结构体的基本行为：
 *  初始化、派生计算（剩余号源）、字符串安全、状态判定、辅助函数。
 *
 *  编译运行：
 *      cmake --build --preset debug --target test_entity
 *      ctest --preset debug
 *  或直接：
 *      g++ -std=c++17 -Wall -Wextra -O2 -I. tests/test_entity.cpp -o test_entity.exe
 * ==========================================================================*/

#include <cstdio>
#include "model/Entity.h"

/* ==========================================================================
 *  极简断言工具
 * ==========================================================================*/
static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* what, int line)
{
    if (condition)
    {
        g_pass++;
    }
    else
    {
        g_fail++;
        std::printf("    [失败] %s   （第 %d 行）\n", what, line);
    }
}

#define CHECK(cond, msg) check((cond), (msg), __LINE__)

/* ==========================================================================
 *  1. 科室
 * ==========================================================================*/
static void testDepartment()
{
    std::printf("[1] 科室 Department\n");

    Department d;
    d.init();

    CHECK(d.id == -1, "初始化后编号为 -1（表示尚未赋值）");
    CHECK(d.floor == 1, "初始化后楼层为 1");
    CHECK(d.name[0] == '\0', "初始化后名称为空串");
    CHECK(d.room[0] == '\0', "初始化后房间号为空串");

    copyText(d.name, "口腔科", NAME_LEN);
    d.id    = 6;
    d.floor = 2;
    copyText(d.room, "2F-N03", ROOM_LEN);

    CHECK(d.id == 6, "赋值后编号为 6");
    CHECK(d.name[0] != '\0', "赋值后名称非空");

    /* 用 strcmp 反证 copyText 写对了 */
    CHECK(std::strcmp(d.name, "口腔科") == 0, "名称内容正确");
    CHECK(std::strcmp(d.room, "2F-N03") == 0, "房间号内容正确");

    /* 重新 init 应当彻底清空 */
    d.init();
    CHECK(d.id == -1 && d.name[0] == '\0', "重新 init 后字段被清空");
}

/* ==========================================================================
 *  2. 医生
 * ==========================================================================*/
static void testDoctor()
{
    std::printf("[2] 医生 Doctor\n");

    Doctor doc;
    doc.init();

    CHECK(doc.id == -1, "初始化后编号为 -1");
    CHECK(doc.deptId == -1, "初始化后科室编号为 -1");
    CHECK(doc.title == TITLE_ATTENDING, "初始化后职称为主治医师");
    CHECK(doc.rating == 0.0, "初始化后评分为 0");
    CHECK(doc.goodAt[0] == '\0', "初始化后擅长领域为空");

    /* ---- 擅长领域的子串匹配 ---- */
    copyText(doc.goodAt, "高血压,冠心病,心律失常", TEXT_LEN);

    CHECK(doc.goodAtContains("高血压"), "命中完整词");
    CHECK(doc.goodAtContains("冠心病"), "命中中间词");
    CHECK(doc.goodAtContains("心律失常"), "命中末尾词");
    CHECK(doc.goodAtContains("高血压,冠心病"), "命中跨逗号的连续片段");
    CHECK(!doc.goodAtContains("糖尿病"), "未标注的词不命中");
    CHECK(!doc.goodAtContains(""), "空关键字不命中（避免误判为全部命中）");
    CHECK(!doc.goodAtContains(0), "空指针不崩溃，返回 false");
    CHECK(!doc.goodAtContains("心脏"), "只匹配到半个词不算命中（心脏 不在串中）");

    /* ---- 职称名称与挂号费 ---- */
    CHECK(std::strcmp(titleName(TITLE_CHIEF), "主任医师") == 0, "主任医师名称正确");
    CHECK(std::strcmp(titleName(TITLE_RESIDENT), "住院医师") == 0, "住院医师名称正确");
    CHECK(std::strcmp(titleName(99), "未知职称") == 0, "非法职称返回未知");
    CHECK(titleFee(TITLE_CHIEF) == 60, "主任医师挂号费 60 元");
    CHECK(titleFee(TITLE_RESIDENT) == 10, "住院医师挂号费 10 元");
    CHECK(titleFee(TITLE_CHIEF) > titleFee(TITLE_RESIDENT), "职称越高挂号费越贵");

    doc.init();
    CHECK(doc.goodAt[0] == '\0', "重新 init 后擅长领域被清空");
}

/* ==========================================================================
 *  3. 排班（重点：号源计算与二维下标）
 * ==========================================================================*/
static void testSchedule()
{
    std::printf("[3] 排班 Schedule\n");

    Schedule s;
    s.init();

    CHECK(s.doctorId == -1, "初始化后医生编号为 -1");
    CHECK(s.capacity == 0, "初始化后号源总数为 0");
    CHECK(s.booked == 0, "初始化后已预约数为 0");
    CHECK(s.remain() == 0, "初始化后剩余号源为 0");
    CHECK(s.isFull(), "号源为 0 时视为已满");
    CHECK(!s.bookable(), "号源为 0 时不可预约");

    /* ---- 正常情况 ---- */
    s.doctorId = 3;
    s.capacity = 20;
    s.booked   = 5;

    CHECK(s.remain() == 15, "剩余号源 = 20 - 5 = 15");
    CHECK(!s.isFull(), "还有余号，不算满");
    CHECK(s.bookable(), "有余号时可以预约");

    /* ---- 号源刚好用完 ---- */
    s.booked = 20;
    CHECK(s.remain() == 0, "全部约满时剩余为 0");
    CHECK(s.isFull(), "约满时 isFull 为真");
    CHECK(!s.bookable(), "约满时不可预约");

    /* ---- 边界：booked 超过 capacity（数据异常时的防御） ---- */
    s.booked = 25;
    CHECK(s.remain() == 0, "已预约数超过总数时，剩余号源按 0 处理而不是负数");
    CHECK(s.isFull(), "异常数据下仍然判定为满");

    /* ---- 占用率 ---- */
    s.booked = 10;
    CHECK(s.usageRatio() > 0.49 && s.usageRatio() < 0.51, "占用率 = 10/20 = 0.5");
    s.capacity = 0;
    CHECK(s.usageRatio() == 0.0, "总数为 0 时占用率按 0 处理，不产生除零");

    /* ---- 二维下标换算：这是排班二维数组的核心 ---- */
    CHECK(Schedule::slotIndex(0, SLOT_MORNING)   == 0, "周一上午 -> 下标 0");
    CHECK(Schedule::slotIndex(0, SLOT_AFTERNOON) == 1, "周一下午 -> 下标 1");
    CHECK(Schedule::slotIndex(1, SLOT_MORNING)   == 2, "周二上午 -> 下标 2");
    CHECK(Schedule::slotIndex(1, SLOT_AFTERNOON) == 3, "周二下午 -> 下标 3");
    CHECK(Schedule::slotIndex(6, SLOT_AFTERNOON) == 13, "周日下午 -> 下标 13（共 14 个时段）");

    /* 下标必须两两不同，否则二维数组会互相覆盖 */
    {
        bool unique = true;
        for (int d = 0; d < 7; d++)
        {
            for (int sl = 1; sl <= SLOT_COUNT; sl++)
            {
                for (int d2 = 0; d2 < 7; d2++)
                {
                    for (int sl2 = 1; sl2 <= SLOT_COUNT; sl2++)
                    {
                        if (d == d2 && sl == sl2) continue;
                        if (Schedule::slotIndex(d, sl) == Schedule::slotIndex(d2, sl2))
                            unique = false;
                    }
                }
            }
        }
        CHECK(unique, "14 个（日期,时段）组合映射到 14 个互不相同的下标");
    }

    /* ---- 时段名称 ---- */
    CHECK(std::strcmp(slotName(SLOT_MORNING), "上午") == 0, "上午名称正确");
    CHECK(std::strcmp(slotName(SLOT_AFTERNOON), "下午") == 0, "下午名称正确");
    CHECK(std::strcmp(weekdayName(0), "周一") == 0, "周一名称正确");
    CHECK(std::strcmp(weekdayName(6), "周日") == 0, "周日名称正确");
    CHECK(std::strcmp(weekdayName(9), "??") == 0, "非法日期不越界，返回 ??");
}

/* ==========================================================================
 *  4. 患者
 * ==========================================================================*/
static void testPatient()
{
    std::printf("[4] 患者 Patient\n");

    Patient p;
    p.init();

    CHECK(p.id == -1, "初始化后编号为 -1");
    CHECK(p.name[0] == '\0', "初始化后姓名为空");
    CHECK(p.phone[0] == '\0', "初始化后电话为空");

    p.id = 1;
    copyText(p.name, "陈志远", NAME_LEN);
    copyText(p.phone, "13800001001", PHONE_LEN);

    CHECK(std::strcmp(p.name, "陈志远") == 0, "姓名正确");
    CHECK(std::strcmp(p.phone, "13800001001") == 0, "电话正确");
    CHECK(std::strlen(p.phone) == 11, "电话长度为 11 位");
}

/* ==========================================================================
 *  5. 预约
 * ==========================================================================*/
static void testAppointment()
{
    std::printf("[5] 预约 Appointment\n");

    Appointment a;
    a.init();

    CHECK(a.id == -1, "初始化后预约号为 -1");
    CHECK(a.patientId == -1, "初始化后患者编号为 -1");
    CHECK(a.status == APPT_BOOKED, "初始化后状态为已预约");
    CHECK(a.seqNo == 0, "初始化后就诊序号为 0");
    CHECK(a.symptom[0] == '\0', "初始化后症状描述为空");

    /* ---- 状态判定 ---- */
    a.status = APPT_BOOKED;
    CHECK(a.active(), "已预约占用号源");
    CHECK(a.cancellable(), "已预约可以取消");

    a.status = APPT_DONE;
    CHECK(a.active(), "已完成仍占用号源（占用过这个名额）");
    CHECK(!a.cancellable(), "已完成的预约不能取消");

    a.status = APPT_CANCELLED;
    CHECK(!a.active(), "已取消不再占用号源");
    CHECK(!a.cancellable(), "已取消的预约不能重复取消");

    /* ---- 状态名称 ---- */
    CHECK(std::strcmp(apptStatusName(APPT_BOOKED), "已预约") == 0, "已预约名称正确");
    CHECK(std::strcmp(apptStatusName(APPT_CANCELLED), "已取消") == 0, "已取消名称正确");
    CHECK(std::strcmp(apptStatusName(APPT_DONE), "已完成") == 0, "已完成名称正确");
    CHECK(std::strcmp(apptStatusName(77), "未知") == 0, "非法状态返回未知");
}

/* ==========================================================================
 *  6. 字符串工具的安全性（最容易出问题的地方）
 * ==========================================================================*/
static void testTextSafety()
{
    std::printf("[6] 字符串工具安全性\n");

    /* ---- 正常拷贝 ---- */
    char buf[16];
    copyText(buf, "abc", 16);
    CHECK(std::strcmp(buf, "abc") == 0, "正常拷贝内容正确");

    /* ---- 刚好放满：15 个字符 + 结尾 '\0' ---- */
    copyText(buf, "123456789012345", 16);
    CHECK(std::strlen(buf) == 15, "恰好 15 字符时全部放下");
    CHECK(buf[15] == '\0', "结尾补了 '\\0'");

    /* ---- 超长必须截断，且不能越界写 ---- */
    {
        /* 用带哨兵的缓冲区，验证没有写到范围之外 */
        char guard[24];
        for (int i = 0; i < 24; i++) guard[i] = (char)0x7F;   /* 先填哨兵值 */

        copyText(guard, "这是一个非常长的字符串，肯定会被截断", 16);

        CHECK(std::strlen(guard) == 15, "超长输入截断到 15 个字符");
        CHECK(guard[15] == '\0', "截断后第 16 字节是 '\\0'");
        CHECK(guard[16] == (char)0x7F, "第 17 字节未被写入（没有越界）");
        CHECK(guard[23] == (char)0x7F, "缓冲区末尾未被破坏");
    }

    /* ---- 空指针与空串 ---- */
    copyText(buf, "", 16);
    CHECK(buf[0] == '\0', "拷贝空串得到空串");

    copyText(buf, 0, 16);
    CHECK(buf[0] == '\0', "源指针为空时安全处理，不崩溃");

    copyText(0, "abc", 16);
    CHECK(true, "目标指针为空时不崩溃");

    /* ---- clearText ---- */
    copyText(buf, "abc", 16);
    clearText(buf, 16);
    CHECK(buf[0] == '\0', "clearText 清空字符串");

    clearText(0, 16);
    CHECK(true, "clearText 收到空指针时不崩溃");

    /* ---- 反复拷贝不残留 ---- */
    copyText(buf, "AAAAAAAAAA", 16);
    copyText(buf, "BB", 16);
    CHECK(std::strcmp(buf, "BB") == 0, "短串覆盖长串后不残留旧内容");
    CHECK(std::strlen(buf) == 2, "长度正确，说明结尾 '\\0' 写对了");
}

/* ==========================================================================
 *  7. 结构体尺寸与可拷贝性
 * ==========================================================================*/
static void testStructProperties()
{
    std::printf("[7] 结构体属性\n");

    std::printf("      sizeof(Department)  = %d 字节\n", (int)sizeof(Department));
    std::printf("      sizeof(Doctor)      = %d 字节\n", (int)sizeof(Doctor));
    std::printf("      sizeof(Schedule)    = %d 字节\n", (int)sizeof(Schedule));
    std::printf("      sizeof(Patient)     = %d 字节\n", (int)sizeof(Patient));
    std::printf("      sizeof(Appointment) = %d 字节\n", (int)sizeof(Appointment));

    /* 结构体应当足够小，能放心地放进顺序表 */
    CHECK(sizeof(Department) < 512, "科室结构体小于 512 字节");
    CHECK(sizeof(Doctor) < 512, "医生结构体小于 512 字节");
    CHECK(sizeof(Schedule) < 64, "排班结构体小于 64 字节（5 个 int）");
    CHECK(sizeof(Appointment) < 128, "预约结构体小于 128 字节");

    /* ---- 按值拷贝应当是完全独立的副本 ---- */
    Doctor a;
    a.init();
    a.id = 7;
    copyText(a.name, "张三", NAME_LEN);

    Doctor b = a;                 /* 值拷贝 */
    b.id = 8;
    copyText(b.name, "李四", NAME_LEN);

    CHECK(a.id == 7, "拷贝后修改副本，原对象编号不受影响");
    CHECK(std::strcmp(a.name, "张三") == 0, "拷贝后修改副本名称，原对象内容不受影响");
    CHECK(b.id == 8 && std::strcmp(b.name, "李四") == 0, "副本内容正确");

    /* ---- 结构体里没有指针，所以整体清零是安全的 ---- */
    Schedule s;
    s.init();
    s.capacity = 20;
    std::memset(&s, 0, sizeof(s));
    CHECK(s.capacity == 0 && s.booked == 0, "memset 清零后字段归零（说明没有指针成员）");
}

/* ==========================================================================
 *  主函数
 * ==========================================================================*/
int main()
{
    std::printf("========================================================\n");
    std::printf("  数据实体自检 —— model/Entity.h\n");
    std::printf("========================================================\n");

    testDepartment();
    testDoctor();
    testSchedule();
    testPatient();
    testAppointment();
    testTextSafety();
    testStructProperties();

    std::printf("--------------------------------------------------------\n");
    std::printf("  通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    std::printf("========================================================\n");

    return (g_fail == 0) ? 0 : 1;
}
