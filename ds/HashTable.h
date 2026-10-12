#ifndef HMS_HASHTABLE_H
#define HMS_HASHTABLE_H

/* ============================================================================
 *  ds/HashTable.h  ——  哈希表（除留余数法 + 链地址法）—— 声明
 * ----------------------------------------------------------------------------
 *  实现见 ds/HashTable.cpp。
 *  对应设计说明书 2.1 节（3）。
 *
 *  【为什么不是模板类】
 *  本系统只需要两种键：整数编号、字符串名称。为它们各写一个类，代码比模板
 *  更短、更好读。两个类的值统一为 int（实体在顺序表中的下标）。
 *  因为不是模板，所以按常规习惯拆成 .h（声明）+ .cpp（实现）。
 *
 *  【哈希函数】整数键：key % BUCKET_COUNT
 *              字符串：各字符 ASCII 值累加后 % BUCKET_COUNT
 *
 *  【冲突处理】链地址法：哈希值相同的记录挂在同一个桶的链上，
 *              查找时先算桶号，再沿链逐个比较 key。
 *
 *  【容量】固定为质数 1009，不做动态扩容
 *          本系统数据规模固定且远小于容量（最多的是排班 90 条、症状 59 条），
 *          装载因子约 0.20，实测最长链不超过 3。
 *          若不扩容的理由需要说明：扩容要重新散列全部元素，而本系统的数据量
 *          在编译期就是已知量级，一次取足够大的容量比动态扩容更简单可靠。
 * ==========================================================================*/

/* ==========================================================================
 *  整数键哈希表：编号 → 下标
 * ==========================================================================*/
class HashTable
{
public:
    /* 桶的个数，取质数有助于把哈希值分散开 */
    static const int BUCKET_COUNT = 1009;

    struct Node
    {
        int   key;     /* 键：实体编号 */
        int   value;   /* 值：实体在顺序表中的下标 */
        Node* next;    /* 同桶内下一个结点 */
    };

    HashTable();
    ~HashTable();

    /* 持有堆内存，禁止浅拷贝，避免重复释放 */
    HashTable(const HashTable&) = delete;
    HashTable& operator=(const HashTable&) = delete;

    /* ---------------- 状态 ---------------- */
    int  size() const;
    bool isEmpty() const;

    /* ---------------- 主要操作 ---------------- */
    void put(int key, int value);            /* 插入或更新 */
    bool get(int key, int* valueOut) const;  /* 查找，找到返回 true */
    int  getOrDefault(int key, int dflt) const;
    bool contains(int key) const;
    bool remove(int key);                    /* 删除，返回是否存在 */
    void clear();

    /* 最长链的长度 —— 自检用，验证哈希是否分散得好 */
    int longestChain() const;

private:
    int hashOf(int key) const;

    Node** buckets;   /* 桶数组 */
    int    count;     /* 已存键值对个数 */
};

/* ==========================================================================
 *  字符串键哈希表：名称 → 下标
 *  用于"科室名称 → 科室编号""症状名称 → 症状下标"这类按名字查的场景。
 * ==========================================================================*/
class StrHashTable
{
public:
    static const int BUCKET_COUNT = 1009;
    static const int MAX_KEY_LEN  = 32;

    struct Node
    {
        char  key[MAX_KEY_LEN];
        int   value;
        Node* next;
    };

    StrHashTable();
    ~StrHashTable();

    StrHashTable(const StrHashTable&) = delete;
    StrHashTable& operator=(const StrHashTable&) = delete;

    /* ---------------- 状态 ---------------- */
    int  size() const;
    bool isEmpty() const;

    /* ---------------- 主要操作 ---------------- */
    void put(const char* key, int value);
    bool get(const char* key, int* valueOut) const;
    int  getOrDefault(const char* key, int dflt) const;
    bool contains(const char* key) const;
    bool remove(const char* key);
    void clear();

    int longestChain() const;

    /* ---------------- 自己实现的字符串工具（静态，供外部复用） ---------------- */

    /* 字符串比较：相等返回 0 */
    static int compare(const char* a, const char* b);

    /* 拷贝字符串，超过上限就截断，保证目标串一定以 '\0' 结尾 */
    static void copyString(char* dst, const char* src, int maxLen);

private:
    int hashOf(const char* key) const;

    Node** buckets;
    int    count;
};

#endif /* HMS_HASHTABLE_H */
