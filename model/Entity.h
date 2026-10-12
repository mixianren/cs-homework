#ifndef HMS_ENTITY_H
#define HMS_ENTITY_H

/* ============================================================================
 *  model/Entity.h  ——  数据实体定义
 * ----------------------------------------------------------------------------
 *  对应项目框架.txt 第七章。
 *
 *  本文件只定义结构体和常量，不含业务逻辑。业务函数放在 service/ 目录下。
 *
 *  【三个设计约定】
 *
 *  1. 字符串用定长字符数组，不用 std::string
 *     理由：课程设计禁用标准类库；定长数组赋值即拷贝，不需要管理内存，
 *           结构体可以放心按值传递、整块写入文件。
 *     代价：超长内容会被截断，因此各字段长度按实际数据留了余量。
 *
 *  2. 实体之间只存编号，不存指针
 *     例如 Appointment 里存的是 patientId 而不是 Patient*。
 *     理由：数组扩容后指针会失效，用编号则永远安全；
 *           需要实体时通过 DataStore 按编号查，是 O(1) 的。
 *
 *  3. 结构体保持"纯数据"，只有极少数派生计算写成成员函数
 *     如 Schedule::remain()（剩余号源 = 总号源 - 已预约）。
 *     这类函数只是几个字段的算术，放在结构体里最自然；
 *     任何涉及查找、修改别的实体的逻辑一律不放进来。
 * ==========================================================================*/

#include <cstring>

/* ==========================================================================
 *  字段长度上限
 *  中文在 UTF-8 下占 3 字节，所以 32 字节约 10 个汉字。
 *  数值按实际数据留了 1.5 倍余量，避免截断。
 * ==========================================================================*/
const int NAME_LEN  = 32;    /* 姓名、科室名、症状名 */
const int PHONE_LEN = 16;    /* 电话号码 */
const int ROOM_LEN  = 16;    /* 房间号，如 "3F-N01" */
const int TEXT_LEN  = 128;   /* 较长文本：擅长领域、常见症状 */

/* ==========================================================================
 *  预约状态
 *  用一组整型常量而不是 enum，便于直接写入文本文件、便于和用户输入比较
 * ==========================================================================*/
const int APPT_BOOKED    = 0;   /* 已预约，尚未就诊 */
const int APPT_CANCELLED = 1;   /* 患者主动取消，号源已归还 */
const int APPT_DONE      = 2;   /* 已完成就诊 */

/* 状态名，界面层直接用，避免在多个地方重复写中文字面量 */
inline const char* apptStatusName(int status)
{
    if (status == APPT_BOOKED)    return "已预约";
    if (status == APPT_CANCELLED) return "已取消";
    if (status == APPT_DONE)      return "已完成";
    return "未知";
}

/* 该状态是否占用了号源 */
inline bool apptHoldsSlot(int status)
{
    return status == APPT_BOOKED || status == APPT_DONE;
}

/* ==========================================================================
 *  医生职称
 * ==========================================================================*/
const int TITLE_RESIDENT  = 1;   /* 住院医师 */
const int TITLE_ATTENDING = 2;   /* 主治医师 */
const int TITLE_ASSOCIATE = 3;   /* 副主任医师 */
const int TITLE_CHIEF     = 4;   /* 主任医师 */

inline const char* titleName(int title)
{
    if (title == TITLE_RESIDENT)  return "住院医师";
    if (title == TITLE_ATTENDING) return "主治医师";
    if (title == TITLE_ASSOCIATE) return "副主任医师";
    if (title == TITLE_CHIEF)     return "主任医师";
    return "未知职称";
}

/* 挂号费（元）：职称越高越贵，用于预约凭证展示 */
inline int titleFee(int title)
{
    if (title == TITLE_RESIDENT)  return 10;
    if (title == TITLE_ATTENDING) return 20;
    if (title == TITLE_ASSOCIATE) return 40;
    if (title == TITLE_CHIEF)     return 60;
    return 0;
}

/* ==========================================================================
 *  门诊时段
 * ==========================================================================*/
const int SLOT_MORNING   = 1;   /* 上午 08:00-12:00 */
const int SLOT_AFTERNOON = 2;   /* 下午 14:00-17:30 */
const int SLOT_COUNT     = 2;   /* 一天两个时段 */

inline const char* slotName(int slot)
{
    if (slot == SLOT_MORNING)   return "上午";
    if (slot == SLOT_AFTERNOON) return "下午";
    return "未知时段";
}

inline const char* slotTimeRange(int slot)
{
    if (slot == SLOT_MORNING)   return "08:00-12:00";
    if (slot == SLOT_AFTERNOON) return "14:00-17:30";
    return "--:--";
}

/* 一周日期：0 = 周一 ... 6 = 周日 */
inline const char* weekdayName(int day)
{
    static const char* names[7] = { "周一", "周二", "周三", "周四", "周五", "周六", "周日" };
    if (day < 0 || day > 6) return "??";
    return names[day];
}

/* ==========================================================================
 *  字符串工具
 *  替代 std::string 的最小功能集：赋值（带截断保护）与比较。
 *  注意：一定要保证目标串以 '\0' 结尾，否则后续 strcmp / printf 会越界。
 * ==========================================================================*/

/* 安全拷贝：超过上限就截断，始终补 '\0' */
inline void copyText(char* dst, const char* src, int maxLen)
{
    if (dst == 0 || maxLen <= 0) return;
    if (src == 0) { dst[0] = '\0'; return; }

    int i = 0;
    while (src[i] != '\0' && i < maxLen - 1)
    {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* 清空字符串 */
inline void clearText(char* dst, int maxLen)
{
    if (dst == 0 || maxLen <= 0) return;
    dst[0] = '\0';
}

/* ==========================================================================
 *  科室 Department
 *  数据规模要求：不少于 10 个
 * ==========================================================================*/
struct Department
{
    int  id;                  /* 科室编号，唯一 */
    char name[NAME_LEN];      /* 科室名称 */
    int  floor;               /* 所在楼层 */
    char room[ROOM_LEN];      /* 房间号，如 "3F-N01" */
    char location[TEXT_LEN];  /* 位置描述，如 "门诊楼3层东区" */
    char symptoms[TEXT_LEN];  /* 常见症状说明（数据文件未提供时留空） */

    /* 把各字段恢复成初始状态 */
    void init()
    {
        id    = -1;
        floor = 1;
        clearText(name, NAME_LEN);
        clearText(room, ROOM_LEN);
        clearText(location, TEXT_LEN);
        clearText(symptoms, TEXT_LEN);
    }
};

/* ==========================================================================
 *  医生 Doctor
 *  数据规模要求：不少于 30 人
 * ==========================================================================*/
struct Doctor
{
    int    id;                   /* 医生编号，唯一 */
    char   name[NAME_LEN];       /* 姓名 */
    int    deptId;               /* 所属科室编号 */
    int    title;                /* 职称，取值 TITLE_xxx */
    char   goodAt[TEXT_LEN];     /* 擅长领域，逗号分隔 */
    double rating;               /* 评分，0 ~ 5 */
    char   roomId[ROOM_LEN];     /* 诊室节点编号，后期做路线查询时用 */

    void init()
    {
        id     = -1;
        deptId = -1;
        title  = TITLE_ATTENDING;
        rating = 0.0;
        clearText(name, NAME_LEN);
        clearText(goodAt, TEXT_LEN);
        clearText(roomId, ROOM_LEN);
    }

    /* 是否擅长某个领域（子串匹配，所以查"高血压"能命中"高血压,冠心病"） */
    bool goodAtContains(const char* keyword) const
    {
        if (keyword == 0 || keyword[0] == '\0') return false;

        int n = 0;
        while (goodAt[n] != '\0') n++;
        int m = 0;
        while (keyword[m] != '\0') m++;
        if (m == 0 || m > n) return false;

        for (int i = 0; i + m <= n; i++)
        {
            int j = 0;
            while (j < m && goodAt[i + j] == keyword[j]) j++;
            if (j == m) return true;
        }
        return false;
    }
};

/* ==========================================================================
 *  排班 Schedule
 *  联合主键：(doctorId, day, slot)
 *  存储方式见项目框架.txt 第六章 —— 网格下标 = 医生编号 * 14 + slotIndex(day, slot)
 * ==========================================================================*/
struct Schedule
{
    int  doctorId;   /* 医生编号 */
    int  day;        /* 出诊日期：0 周一 ~ 6 周日 */
    int  slot;       /* 时段：SLOT_MORNING / SLOT_AFTERNOON */
    int  capacity;   /* 最大接诊数（号源总数） */
    int  booked;     /* 已预约数 */

    void init()
    {
        doctorId = -1;
        day      = 0;
        slot     = SLOT_MORNING;
        capacity = 0;
        booked   = 0;
    }

    /* 剩余号源。用减法而不是单独存一个字段，
       避免"总号源 - 已预约 = 剩余"三者不一致。 */
    int remain() const
    {
        int r = capacity - booked;
        return (r < 0) ? 0 : r;
    }

    bool isFull() const { return remain() <= 0; }

    /* 能否被预约 */
    bool bookable() const { return remain() > 0; }

    /* 号源占用率，界面用它标示紧张程度 */
    double usageRatio() const
    {
        if (capacity <= 0) return 0.0;
        return (double)booked / (double)capacity;
    }

    /* 把 (日期, 时段) 压成一个下标：周一上午=0，周一下午=1，周二上午=2 ……
       这是二维数组第二个下标的算法。 */
    static int slotIndex(int day, int slot)
    {
        return day * SLOT_COUNT + (slot - 1);
    }
};

/* ==========================================================================
 *  患者 Patient
 *  预约记录不放在这里，而是由 DataStore 用链表按患者编号维护，
 *  因为一个患者的预约条数会变化，实体里内嵌容器会让结构体变复杂。
 * ==========================================================================*/
struct Patient
{
    int  id;                  /* 患者编号，唯一 */
    char name[NAME_LEN];      /* 姓名 */
    char phone[PHONE_LEN];    /* 联系电话 */

    void init()
    {
        id = -1;
        clearText(name, NAME_LEN);
        clearText(phone, PHONE_LEN);
    }
};

/* ==========================================================================
 *  症状 Symptom
 *  症状字典里的一条。它是加权二部图左部的顶点，
 *  与科室的关联关系不放在这里，而是由 DataStore 统一维护
 *  （一条症状可以关联多个科室，放在结构体里会让它变得笨重）。
 * ==========================================================================*/
struct Symptom
{
    int  index;              /* 症状下标，等于它在字典中的位置，唯一 */
    char name[NAME_LEN];     /* 症状名称，如"牙痛" */
    int  bodyPart;           /* 部位码：1头颈 2胸腹 3四肢 4全身 5皮肤 6五官 7精神神经 */

    void init()
    {
        index    = -1;
        bodyPart = 0;
        clearText(name, NAME_LEN);
    }
};

/* 部位码 -> 名称，界面展示用 */
inline const char* bodyPartName(int part)
{
    if (part == 1) return "头颈";
    if (part == 2) return "胸腹";
    if (part == 3) return "四肢";
    if (part == 4) return "全身";
    if (part == 5) return "皮肤";
    if (part == 6) return "五官";
    if (part == 7) return "精神神经";
    return "其他";
}

/* ==========================================================================
 *  预约 Appointment
 * ==========================================================================*/
struct Appointment
{
    int  id;            /* 预约号，由系统递增分配，唯一 */
    int  patientId;     /* 患者编号 */
    int  doctorId;      /* 医生编号 */
    int  deptId;        /* 科室编号（冗余存储，便于按科室统计） */
    int  day;           /* 就诊日期 */
    int  slot;          /* 就诊时段 */
    int  seqNo;         /* 该时段内的就诊序号，从 1 开始 */
    int  status;        /* 状态，取值 APPT_xxx */
    char symptom[NAME_LEN];  /* 就诊时描述的症状（导诊结果的留痕） */

    void init()
    {
        id        = -1;
        patientId = -1;
        doctorId  = -1;
        deptId    = -1;
        day       = 0;
        slot      = SLOT_MORNING;
        seqNo     = 0;
        status    = APPT_BOOKED;
        clearText(symptom, NAME_LEN);
    }

    /* 是否仍占用号源 */
    bool active() const { return apptHoldsSlot(status); }

    /* 是否允许取消：只有还没就诊的预约能取消 */
    bool cancellable() const { return status == APPT_BOOKED; }
};

#endif /* HMS_ENTITY_H */
