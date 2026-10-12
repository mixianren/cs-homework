#ifndef HMS_SORT_H
#define HMS_SORT_H

/* ============================================================================
 *  ds/Sort.h  ——  自研排序算法 —— 声明
 * ----------------------------------------------------------------------------
 *  实现见 ds/Sort.cpp。
 *  对应项目框架.md 6.2 节。
 *
 *  本工程不使用 std::sort，三个排序算法全部自己实现。
 *  因为都是普通函数（不是模板），所以按常规习惯拆成 .h（声明）+ .cpp（实现）。
 *
 *  统一接口约定：
 *      cmp(a, b) 返回 负数 表示 a 应排在 b 前面
 *                返回 0    表示两者顺序无所谓
 *                返回 正数 表示 a 应排在 b 后面
 *  一套比较规则同时支持升序和降序（把 cmp 取反即可）。
 * ==========================================================================*/

#include <cstdlib>

/* 比较函数的类型：a、b 指向要比较的两个元素 */
typedef int (*CompareFunc)(const void* a, const void* b);

/* ==========================================================================
 *  三个排序算法
 *  参数：array 数组首地址，n 元素个数，size 单个元素的字节数，cmp 比较函数
 * ==========================================================================*/

/* 插入排序 O(n^2)，稳定。数据量小或接近有序时非常快 */
void insertionSort(void* array, int n, int size, CompareFunc cmp);

/* 冒泡排序 O(n^2)，稳定。加了标志位，某轮无交换即提前结束 */
void bubbleSort(void* array, int n, int size, CompareFunc cmp);

/* 快速排序 O(n log n)，不稳定。三者取中选基准，避免有序数据退化 */
void quickSort(void* array, int n, int size, CompareFunc cmp);

/* ==========================================================================
 *  常用比较函数（业务层直接用，不必每次重写）
 * ==========================================================================*/
int compareIntAsc(const void* a, const void* b);
int compareIntDesc(const void* a, const void* b);
int compareDoubleAsc(const void* a, const void* b);

/* 浮点降序 —— 导诊结果按匹配度从高到低排序时使用 */
int compareDoubleDesc(const void* a, const void* b);

/* 字符串升序（按 C 字符串比较规则） */
int compareStringAsc(const void* a, const void* b);

/* ==========================================================================
 *  二分查找（要求数组已按升序排好）
 *  用途：在有序的排班表中快速定位某一天
 *  找到返回下标，找不到返回 -1
 * ==========================================================================*/
int binarySearchInt(const int* array, int n, int target);

#endif /* HMS_SORT_H */
