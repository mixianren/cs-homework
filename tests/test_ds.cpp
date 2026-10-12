/* ============================================================================
 *  tests/test_ds.cpp  ——  自研容器自检程序
 * ----------------------------------------------------------------------------
 *  对 ds/ 目录下五个容器逐一验证，每个容器覆盖：正常用法、边界情况、错误输入。
 *
 *  编译（在 HospitalMS 目录下）：
 *      g++ -std=c++17 -Wall -Wextra -O2 -I. tests/test_ds.cpp -o tests/test_ds.exe
 *
 *  运行：
 *      tests\test_ds.exe
 *
 *  输出：每项断言一行；最后统计通过/失败数量。
 * ==========================================================================*/

#include <cstdio>
#include "ds/SeqList.h"
#include "ds/LinkedList.h"
#include "ds/HashTable.h"
#include "ds/Queue.h"
#include "ds/Sort.h"

/* ==========================================================================
 *  极简断言工具
 * ==========================================================================*/
static int g_pass = 0;
static int g_fail = 0;

static void check(bool condition, const char* what)
{
    if (condition)
    {
        g_pass++;
    }
    else
    {
        g_fail++;
        std::printf("    [失败] %s   （第 %d 行）\n", what, __LINE__);
    }
}

/* ==========================================================================
 *  测试用的简单结构体：模拟"医生"
 * ==========================================================================*/
struct TestDoctor
{
    int  id;
    char name[16];
    int  title;      /* 职称：1~4 */
};

/* 按编号匹配，供 SeqList::indexOf / LinkedList::find 使用 */
static bool matchDoctorId(const TestDoctor& item, const void* key)
{
    int id = *(const int*)key;
    return item.id == id;
}

/* 按编号匹配（链表用，签名相同） */
static bool matchInt(const int& item, const void* key)
{
    int value = *(const int*)key;
    return item == value;
}

/* ==========================================================================
 *  1. 顺序表
 * ==========================================================================*/
static void testSeqList()
{
    std::printf("[1] 顺序表 SeqList\n");

    SeqList<int> list;

    /* ---- 初始状态 ---- */
    check(list.size() == 0, "新建的顺序表长度为 0");
    check(list.isEmpty(), "新建的顺序表 isEmpty 为真");

    /* ---- 尾部插入 ---- */
    for (int i = 0; i < 100; i++) list.insertBack(i * 3);
    check(list.size() == 100, "插入 100 个元素后长度为 100");
    check(list.getCapacity() >= 100, "容量已自动扩容到 100 以上");

    /* ---- 按下标访问 ---- */
    check(list.at(0, -1) == 0, "第 0 个元素是 0");
    check(list.at(99, -1) == 297, "第 99 个元素是 297");
    check(list.at(100, -1) == -1, "越界访问返回默认值，不崩溃");
    check(list.at(-5, -1) == -1, "负下标返回默认值，不崩溃");

    /* ---- 修改元素 ---- */
    check(list.setAt(0, 999), "修改第 0 个元素成功");
    check(list.at(0, -1) == 999, "修改后读到的值已更新");
    check(!list.setAt(500, 1), "越界写入返回 false");

    /* ---- 按下标插入 ---- */
    check(list.insertAt(0, 111), "在下标 0 处插入成功");
    check(list.at(0, -1) == 111, "插入后首位是新元素");
    check(list.at(1, -1) == 999, "原首位被后移一位");
    check(list.size() == 101, "插入后长度加 1");
    check(!list.insertAt(999, 1), "越界位置插入返回 false");

    /* ---- 按条件查找 ---- */
    SeqList<TestDoctor> doctors;
    TestDoctor d1; d1.id = 7;  d1.title = 4;
    TestDoctor d2; d2.id = 15; d2.title = 2;
    TestDoctor d3; d3.id = 23; d3.title = 3;
    d1.name[0] = 'A'; d1.name[1] = '\0';
    d2.name[0] = 'B'; d2.name[1] = '\0';
    d3.name[0] = 'C'; d3.name[1] = '\0';
    doctors.insertBack(d1);
    doctors.insertBack(d2);
    doctors.insertBack(d3);

    int findId = 15;
    check(doctors.indexOf(matchDoctorId, &findId) == 1, "按编号 15 找到下标 1");
    findId = 99;
    check(doctors.indexOf(matchDoctorId, &findId) == -1, "查找不存在的编号返回 -1");

    check(doctors.ptrAt(2) != 0 && doctors.ptrAt(2)->id == 23, "ptrAt 可取到元素指针");
    check(doctors.ptrAt(9) == 0, "ptrAt 越界返回空指针");

    /* ---- 删除 ---- */
    check(doctors.erase(1), "删除下标 1 成功");
    check(doctors.size() == 2, "删除后长度为 2");
    check(doctors.at(1, d1).id == 23, "后面的元素整体前移，下标 1 变成原来的下标 2");
    check(!doctors.erase(50), "删除越界下标返回 false");

    findId = 23;
    check(doctors.removeIf(matchDoctorId, &findId), "按条件删除成功");
    check(doctors.size() == 1, "条件删除后长度为 1");

    /* ---- 清空 ---- */
    doctors.clear();
    check(doctors.isEmpty(), "清空后 isEmpty 为真");
    check(doctors.size() == 0, "清空后长度为 0");
}

/* ==========================================================================
 *  2. 双向链表
 * ==========================================================================*/
static void testLinkedList()
{
    std::printf("[2] 双向链表 LinkedList\n");

    LinkedList<int> list;

    check(list.size() == 0, "新建链表长度为 0");
    check(list.isEmpty(), "新建链表 isEmpty 为真");
    check(list.first() == 0, "空链表 first() 返回空指针");

    /* ---- 插入 ---- */
    list.pushBack(10);
    list.pushBack(20);
    list.pushBack(30);
    check(list.size() == 3, "尾部插入 3 个元素后长度为 3");
    check(list.first()->data == 10, "第一个元素是 10");

    list.pushFront(5);
    check(list.size() == 4, "头部插入后长度为 4");
    check(list.first()->data == 5, "头部插入后第一个元素是 5");

    /* ---- 遍历顺序 ---- */
    {
        int     expected[4] = { 5, 10, 20, 30 };
        int     index = 0;
        bool    orderOk = true;
        LinkedList<int>::Node* p = list.first();
        while (p != 0)
        {
            if (index < 4 && p->data != expected[index]) orderOk = false;
            index++;
            p = LinkedList<int>::nextOf(p);
        }
        check(orderOk && index == 4, "沿 next 遍历顺序为 5 10 20 30");
    }

    /* ---- 反向遍历：验证 prev 指针正确 ---- */
    {
        LinkedList<int>::Node* p = list.first();
        while (LinkedList<int>::nextOf(p) != 0) p = LinkedList<int>::nextOf(p);
        check(p->data == 30, "尾结点是 30");

        int  expected[4] = { 30, 20, 10, 5 };
        int  index = 0;
        bool backOk = true;
        while (p != 0)
        {
            if (index < 4 && p->data != expected[index]) backOk = false;
            index++;
            p = p->prev;
            /* 头结点的 data 不参与比较，走到头结点时 prev 为空 */
            if (p != 0 && p->prev == 0) break;
        }
        check(backOk, "沿 prev 反向遍历顺序为 30 20 10 5");
    }

    /* ---- 查找 ---- */
    int key = 20;
    check(list.find(matchInt, &key) != 0, "按值 20 找到结点");
    key = 99;
    check(list.find(matchInt, &key) == 0, "查找不存在的值返回空指针");

    /* ---- 删除：这是链表相对顺序表的核心优势 ---- */
    key = 20;
    check(list.removeIf(matchInt, &key), "删除中间结点 20 成功");
    check(list.size() == 3, "删除后长度为 3");
    check(list.find(matchInt, &key) == 0, "被删除的值已找不到");

    /* 删除后链表结构仍然完整 */
    {
        int  expected[3] = { 5, 10, 30 };
        int  index = 0;
        bool ok = true;
        LinkedList<int>::Node* p = list.first();
        while (p != 0)
        {
            if (index < 3 && p->data != expected[index]) ok = false;
            index++;
            p = LinkedList<int>::nextOf(p);
        }
        check(ok && index == 3, "删除中间结点后其余顺序为 5 10 30");
    }

    /* ---- 删除头结点：验证哨兵头结点的作用 ---- */
    key = 5;
    check(list.removeIf(matchInt, &key), "删除首结点成功");
    check(list.first()->data == 10, "删除首结点后新的首结点是 10");

    /* ---- 清空 ---- */
    list.clear();
    check(list.isEmpty(), "清空后 isEmpty 为真");
    check(list.first() == 0, "清空后 first() 返回空指针");
}

/* ==========================================================================
 *  3. 哈希表
 * ==========================================================================*/
static void testHashTable()
{
    std::printf("[3] 哈希表 HashTable / StrHashTable\n");

    /* ---- 整数键 ---- */
    HashTable table;

    check(table.size() == 0, "新建哈希表长度为 0");
    check(!table.contains(1), "空表查询返回 false");
    check(table.getOrDefault(1, -1) == -1, "空表查询返回默认值");

    /* 插入 100 条，键故意打乱顺序 */
    for (int i = 0; i < 100; i++)
    {
        table.put(i * 7 + 3, i);
    }
    check(table.size() == 100, "插入 100 条后长度为 100");

    /* 逐条验证都能查回来 */
    {
        bool allOk = true;
        for (int i = 0; i < 100; i++)
        {
            if (table.getOrDefault(i * 7 + 3, -1) != i) allOk = false;
        }
        check(allOk, "100 条记录全部能正确查回");
    }

    check(table.getOrDefault(123456, -1) == -1, "查询不存在的键返回默认值");

    /* 覆盖同一个键 */
    table.put(3, 888);
    check(table.getOrDefault(3, -1) == 888, "重复插入同一个键会覆盖原值");
    check(table.size() == 100, "覆盖不会增加长度");

    /* 负数键 */
    table.put(-5, 42);
    check(table.getOrDefault(-5, -1) == 42, "负数键也能正确存储和查询");
    check(table.size() == 101, "插入负数键后长度为 101");

    /* 哈希分布 */
    std::printf("      哈希分布：桶数 %d，已存 %d 条，最长链 %d\n",
                HashTable::BUCKET_COUNT, table.size(), table.longestChain());
    check(table.longestChain() <= 3, "最长链不超过 3（说明哈希分散得不错）");

    /* 删除 */
    check(table.remove(3), "删除已存在的键返回 true");
    check(!table.contains(3), "删除后查不到该键");
    check(table.size() == 100, "删除后长度减 1");
    check(!table.remove(3), "重复删除同一个键返回 false");

    /* ---- 字符串键 ---- */
    StrHashTable strTable;

    strTable.put("内科",     1);
    strTable.put("外科",     2);
    strTable.put("口腔科",   6);
    strTable.put("神经内科", 9);
    check(strTable.size() == 4, "字符串表插入 4 条");

    check(strTable.getOrDefault("口腔科", -1) == 6, "按中文名称查到正确编号");
    check(strTable.getOrDefault("神经内科", -1) == 9, "相似名称不会互相干扰");
    check(strTable.getOrDefault("不存在", -1) == -1, "查询不存在的名称返回默认值");
    check(!strTable.contains("内科2"), "前缀相同但不是同一个键，查不到");

    strTable.put("内科", 100);
    check(strTable.getOrDefault("内科", -1) == 100, "字符串键重复插入会覆盖");
    check(strTable.size() == 4, "覆盖不增加长度");

    check(strTable.remove("外科"), "删除字符串键成功");
    check(strTable.size() == 3, "删除后长度为 3");

    /* 字符串工具函数的边界 */
    check(StrHashTable::compare("abc", "abc") == 0, "compare 相同字符串返回 0");
    check(StrHashTable::compare("abc", "abd") < 0, "compare 判断大小正确");

    char buf[8];
    StrHashTable::copyString(buf, "这是一个很长的名字", 8);
    check(buf[7] == '\0', "copyString 超长时截断并保证以 0 结尾");
}

/* ==========================================================================
 *  4. 循环队列
 * ==========================================================================*/
static void testQueue()
{
    std::printf("[4] 循环队列 Queue\n");

    Queue<int> queue(5);      /* 容量 5，实际最多放 4 个 */

    check(queue.isEmpty(), "新建队列 isEmpty 为真");
    check(!queue.isFull(), "新建队列 isFull 为假");
    check(queue.size() == 0, "新建队列长度为 0");
    check(queue.getCapacity() == 5, "容量为 5");
    check(!queue.dequeue(0), "空队列出队返回 false");
    check(!queue.peekFront(0), "空队列取队头返回 false");

    /* ---- 填满：验证"牺牲一个位置"的效果 ---- */
    check(queue.enqueue(10), "入队 10");
    check(queue.enqueue(20), "入队 20");
    check(queue.enqueue(30), "入队 30");
    check(queue.enqueue(40), "入队 40");
    check(queue.size() == 4, "容量 5 的队列最多放 4 个元素");
    check(queue.isFull(), "4 个元素后队列已满");
    check(!queue.enqueue(50), "队满时入队返回 false");

    /* ---- 先到先服务 ---- */
    int value = 0;
    check(queue.peekFront(&value) && value == 10, "队头是 10（取队头不删除）");
    check(queue.size() == 4, "取队头不改变长度");

    check(queue.dequeue(&value) && value == 10, "出队得到 10");
    check(queue.dequeue(&value) && value == 20, "出队得到 20");
    check(queue.size() == 2, "出队两个后长度为 2");
    check(!queue.isFull(), "出队后不再是满队列");

    /* ---- 环形绕圈：这是循环队列的关键测试 ---- */
    check(queue.enqueue(50), "再次入队 50（rear 绕回数组头部）");
    check(queue.enqueue(60), "再次入队 60");
    check(queue.size() == 4, "绕圈后长度仍为 4");
    check(queue.isFull(), "绕圈后再次填满");

    {
        int  expected[4] = { 30, 40, 50, 60 };
        int  index = 0;
        bool ok = true;
        for (int i = 0; i < 4; i++)
        {
            int v = 0;
            if (!queue.dequeue(&v)) { ok = false; break; }
            if (v != expected[index]) ok = false;
            index++;
        }
        check(ok, "绕圈后的出队顺序是 30 40 50 60（先进先出未被破坏）");
    }

    check(queue.isEmpty(), "全部出队后队列为空");
    check(queue.size() == 0, "全部出队后长度为 0");
}

/* ==========================================================================
 *  5. 排序
 * ==========================================================================*/
static void testSort()
{
    std::printf("[5] 排序 Sort\n");

    const int N = 500;
    int original[500];

    /* 造一组有重复值的乱序数据：用固定公式，保证每次运行结果一致 */
    for (int i = 0; i < N; i++) original[i] = (i * 37 + 11) % 100;

    /* ---- 插入排序 ---- */
    {
        int a[500];
        for (int i = 0; i < N; i++) a[i] = original[i];
        insertionSort(a, N, sizeof(int), compareIntAsc);

        bool ok = true;
        for (int i = 1; i < N; i++) if (a[i] < a[i - 1]) ok = false;
        check(ok, "插入排序结果升序");
        check(a[0] == 0 && a[N - 1] == 99, "插入排序后最小值为 0、最大值为 99");
    }

    /* ---- 冒泡排序 ---- */
    {
        int a[500];
        for (int i = 0; i < N; i++) a[i] = original[i];
        bubbleSort(a, N, sizeof(int), compareIntAsc);

        bool ok = true;
        for (int i = 1; i < N; i++) if (a[i] < a[i - 1]) ok = false;
        check(ok, "冒泡排序结果升序");
    }

    /* ---- 快速排序 ---- */
    {
        int a[500];
        for (int i = 0; i < N; i++) a[i] = original[i];
        quickSort(a, N, sizeof(int), compareIntAsc);

        bool ok = true;
        for (int i = 1; i < N; i++) if (a[i] < a[i - 1]) ok = false;
        check(ok, "快速排序结果升序");
        check(a[0] == 0 && a[N - 1] == 99, "快速排序后最小值为 0、最大值为 99");
    }

    /* ---- 三种排序结果必须一致 ---- */
    {
        int a[500], b[500], c[500];
        for (int i = 0; i < N; i++) { a[i] = original[i]; b[i] = original[i]; c[i] = original[i]; }
        insertionSort(a, N, sizeof(int), compareIntAsc);
        bubbleSort(b, N, sizeof(int), compareIntAsc);
        quickSort(c, N, sizeof(int), compareIntAsc);

        bool same = true;
        for (int i = 0; i < N; i++) if (a[i] != b[i] || b[i] != c[i]) same = false;
        check(same, "三种排序算法结果完全一致");
    }

    /* ---- 降序 ---- */
    {
        int a[500];
        for (int i = 0; i < N; i++) a[i] = original[i];
        quickSort(a, N, sizeof(int), compareIntDesc);

        bool ok = true;
        for (int i = 1; i < N; i++) if (a[i] > a[i - 1]) ok = false;
        check(ok, "用降序比较函数得到降序结果");
    }

    /* ---- 浮点数排序：导诊结果按匹配度排序就是这个场景 ---- */
    {
        double scores[6] = { 0.45, 0.90, 0.30, 0.85, 0.17, 0.75 };
        quickSort(scores, 6, sizeof(double), compareDoubleDesc);

        check(scores[0] == 0.90, "浮点降序首位是 0.90");
        check(scores[5] == 0.17, "浮点降序末位是 0.17");
        bool ok = true;
        for (int i = 1; i < 6; i++) if (scores[i] > scores[i - 1]) ok = false;
        check(ok, "浮点降序结果正确");
    }

    /* ---- 边界：0 个、1 个元素 ---- */
    {
        int a[1] = { 5 };
        quickSort(a, 0, sizeof(int), compareIntAsc);
        check(a[0] == 5, "对 0 个元素排序不改变数组");

        quickSort(a, 1, sizeof(int), compareIntAsc);
        check(a[0] == 5, "对 1 个元素排序不改变数组");
    }

    /* ---- 已经有序的数据：测试三者取中的效果 ---- */
    {
        int a[200];
        for (int i = 0; i < 200; i++) a[i] = i;      /* 完全升序 */
        quickSort(a, 200, sizeof(int), compareIntAsc);

        bool ok = true;
        for (int i = 1; i < 200; i++) if (a[i] < a[i - 1]) ok = false;
        check(ok, "对已完全有序的数据排序结果仍然正确（未退化出错）");
    }

    /* ---- 二分查找 ---- */
    {
        int a[10] = { 1, 3, 5, 7, 9, 11, 13, 15, 17, 19 };
        check(binarySearchInt(a, 10, 7)  == 3, "二分查找命中 7，下标为 3");
        check(binarySearchInt(a, 10, 1)  == 0, "二分查找命中首元素");
        check(binarySearchInt(a, 10, 19) == 9, "二分查找命中末元素");
        check(binarySearchInt(a, 10, 8)  == -1, "二分查找未命中返回 -1");
        check(binarySearchInt(a, 10, -5) == -1, "查找小于全部元素的值返回 -1");
        check(binarySearchInt(a, 10, 99) == -1, "查找大于全部元素的值返回 -1");
        check(binarySearchInt(a, 0,  1)  == -1, "空数组查找返回 -1");
    }
}

/* ==========================================================================
 *  主函数
 * ==========================================================================*/
int main()
{
    std::printf("========================================================\n");
    std::printf("  自研容器自检 —— ds/ 目录五个数据结构\n");
    std::printf("========================================================\n");

    testSeqList();
    testLinkedList();
    testHashTable();
    testQueue();
    testSort();

    std::printf("--------------------------------------------------------\n");
    std::printf("  通过 %d 项，失败 %d 项\n", g_pass, g_fail);
    std::printf("========================================================\n");

    return (g_fail == 0) ? 0 : 1;
}
