/* ============================================================================
 *  ds/Sort.cpp  ——  自研排序算法实现
 * ----------------------------------------------------------------------------
 *  声明见 ds/Sort.h。
 * ==========================================================================*/

#include "ds/Sort.h"

/* ==========================================================================
 *  内部辅助函数
 * ==========================================================================*/

/* 按字节交换两个元素。因为要支持任意类型，所以按 size 个字节逐个交换 */
static void swapBytes(void* a, void* b, int size)
{
    unsigned char* p = (unsigned char*)a;
    unsigned char* q = (unsigned char*)b;

    for (int i = 0; i < size; i++)
    {
        unsigned char temp = p[i];
        p[i] = q[i];
        q[i] = temp;
    }
}

/* ==========================================================================
 *  插入排序
 * --------------------------------------------------------------------------
 *  思路：把第 i 个元素插入到前面已经排好序的部分中。
 *        从 i-1 往回找，比它大的元素依次后移，找到位置后放进去。
 * ==========================================================================*/
void insertionSort(void* array, int n, int size, CompareFunc cmp)
{
    if (array == 0 || n < 2 || cmp == 0 || size <= 0) return;

    unsigned char* base = (unsigned char*)array;
    unsigned char* key  = new unsigned char[size];   /* 暂存待插入的元素 */

    for (int i = 1; i < n; i++)
    {
        /* 把第 i 个元素暂存起来 */
        unsigned char* cur = base + (long)i * size;
        for (int k = 0; k < size; k++) key[k] = cur[k];

        /* 从 i-1 往前找插入位置，比 key 大的元素整体后移一位 */
        int j = i - 1;
        while (j >= 0 && cmp(base + (long)j * size, key) > 0)
        {
            unsigned char* from = base + (long)j * size;
            unsigned char* to   = base + (long)(j + 1) * size;
            for (int k = 0; k < size; k++) to[k] = from[k];
            j--;
        }

        /* 把 key 放到空出来的位置 */
        unsigned char* dst = base + (long)(j + 1) * size;
        for (int k = 0; k < size; k++) dst[k] = key[k];
    }

    delete[] key;
}

/* ==========================================================================
 *  冒泡排序
 * --------------------------------------------------------------------------
 *  思路：反复比较相邻两个元素，顺序不对就交换。
 *        每一轮走完，最大的元素就"冒"到了末尾。
 * ==========================================================================*/
void bubbleSort(void* array, int n, int size, CompareFunc cmp)
{
    if (array == 0 || n < 2 || cmp == 0 || size <= 0) return;

    unsigned char* base = (unsigned char*)array;

    for (int i = 0; i < n - 1; i++)
    {
        bool swapped = false;

        /* 末尾 i 个元素已经排好，不用再比 */
        for (int j = 0; j < n - 1 - i; j++)
        {
            unsigned char* a = base + (long)j * size;
            unsigned char* b = base + (long)(j + 1) * size;

            if (cmp(a, b) > 0)
            {
                swapBytes(a, b, size);
                swapped = true;
            }
        }

        if (!swapped) break;   /* 本轮无交换，说明已经有序，提前结束 */
    }
}

/* ==========================================================================
 *  快速排序
 * ==========================================================================*/

/* 内部递归函数 */
static void quickSortRec(void* array, int left, int right, int size,
                         CompareFunc cmp, int depthLimit)
{
    unsigned char* base = (unsigned char*)array;

    while (left < right)
    {
        /* 小区间改用插入排序，减少递归开销 */
        if (right - left < 8)
        {
            insertionSort(base + (long)left * size, right - left + 1, size, cmp);
            return;
        }

        /* 递归深度已达上限，同样改用插入排序收尾，避免栈溢出 */
        if (depthLimit <= 0)
        {
            insertionSort(base + (long)left * size, right - left + 1, size, cmp);
            return;
        }

        /* ---- 三者取中选基准 ----
           取"左端、中间、右端"三个元素的中位数作基准。
           如果固定取第一个元素作基准，遇到已经基本有序的数据时，
           每次分区都会极不平衡，递归深度变成 n，复杂度退化为 O(n^2)。 */
        int mid = left + (right - left) / 2;

        unsigned char* pLeft  = base + (long)left  * size;
        unsigned char* pMid   = base + (long)mid   * size;
        unsigned char* pRight = base + (long)right * size;

        if (cmp(pMid, pLeft) < 0)   swapBytes(pMid, pLeft, size);
        if (cmp(pRight, pLeft) < 0) swapBytes(pRight, pLeft, size);
        if (cmp(pRight, pMid) < 0)  swapBytes(pRight, pMid, size);
        /* 此时 pMid 是三者中的中位数，把它换到 left 位置作基准 */
        swapBytes(pMid, pLeft, size);

        /* ---- 分区：左右两个指针向中间扫描 ---- */
        int i = left;
        int j = right;

        while (i < j)
        {
            /* 从右往左找第一个 <= 基准的元素 */
            while (i < j && cmp(base + (long)j * size, pLeft) >= 0) j--;

            /* 从左往右找第一个 >= 基准的元素 */
            while (i < j && cmp(base + (long)i * size, pLeft) <= 0) i++;

            if (i < j)
            {
                swapBytes(base + (long)i * size, base + (long)j * size, size);
            }
        }

        /* 把基准放到它的最终位置 i */
        swapBytes(pLeft, base + (long)i * size, size);

        /* ---- 先处理较短的一边，长的一边用循环处理，减少递归层数 ---- */
        if (i - left < right - i)
        {
            quickSortRec(array, left, i - 1, size, cmp, depthLimit - 1);
            left = i + 1;
        }
        else
        {
            quickSortRec(array, i + 1, right, size, cmp, depthLimit - 1);
            right = i - 1;
        }
    }
}

void quickSort(void* array, int n, int size, CompareFunc cmp)
{
    if (array == 0 || n < 2 || cmp == 0 || size <= 0) return;

    /* 递归深度上限取 2 * log2(n) */
    int depthLimit = 0;
    int temp = n;
    while (temp > 0) { depthLimit++; temp = temp / 2; }
    depthLimit = depthLimit * 2;

    quickSortRec(array, 0, n - 1, size, cmp, depthLimit);
}

/* ==========================================================================
 *  常用比较函数
 * ==========================================================================*/

int compareIntAsc(const void* a, const void* b)
{
    int x = *(const int*)a;
    int y = *(const int*)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

int compareIntDesc(const void* a, const void* b)
{
    return -compareIntAsc(a, b);
}

int compareDoubleAsc(const void* a, const void* b)
{
    double x = *(const double*)a;
    double y = *(const double*)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

int compareDoubleDesc(const void* a, const void* b)
{
    return -compareDoubleAsc(a, b);
}

int compareStringAsc(const void* a, const void* b)
{
    const char* x = (const char*)a;
    const char* y = (const char*)b;

    int i = 0;
    while (x[i] != '\0' && y[i] != '\0')
    {
        if (x[i] != y[i]) return (x[i] < y[i]) ? -1 : 1;
        i++;
    }
    if (x[i] == y[i]) return 0;
    return (x[i] == '\0') ? -1 : 1;
}

/* ==========================================================================
 *  二分查找
 * ==========================================================================*/
int binarySearchInt(const int* array, int n, int target)
{
    if (array == 0 || n <= 0) return -1;

    int low  = 0;
    int high = n - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;

        if (array[mid] == target) return mid;
        if (array[mid] < target)  low  = mid + 1;
        else                      high = mid - 1;
    }
    return -1;
}
