#ifndef HMS_QUEUE_H
#define HMS_QUEUE_H

/* ============================================================================
 *  ds/Queue.h  ——  循环队列
 * ----------------------------------------------------------------------------
 *  用途：候诊排队，实现"先到先服务"。
 *  对应项目框架.md 6.2 节。
 *
 *  内部结构（环形数组）：
 *      T*  data      环形数组
 *      int front     队头下标
 *      int rear      队尾下标（指向下一个空位）
 *      int capacity  容量
 *
 *  入队：data[rear] = x;  rear  = (rear  + 1) % capacity;
 *  出队：x = data[front]; front = (front + 1) % capacity;
 *
 *  【为什么要空出一个位置】
 *  队空和队满时，如果只用 front == rear 判断，两种情况的条件完全一样，
 *  无法区分。牺牲一个存储位置后：
 *      队空：front == rear
 *      队满：(rear + 1) % capacity == front
 *  两个状态就区分开了。这是最省事的解决办法。
 *
 *  因此队列实际最多容纳 capacity - 1 个元素。
 *
 *  【为什么本文件没有对应的 .cpp】
 *  与 SeqList.h 同理：这是模板类，实现必须写在头文件里，否则链接期报
 *  undefined reference。详见 ds/SeqList.h 顶部的完整说明。
 * ==========================================================================*/

template <typename T>
class Queue
{
public:
    /* ---------------- 构造与析构 ---------------- */

    explicit Queue(int cap = 32)
    {
        if (cap < 2) cap = 2;          /* 至少要能放 1 个元素 */
        capacity = cap;
        data     = new T[capacity];
        front    = 0;
        rear     = 0;
    }

    ~Queue()
    {
        delete[] data;
        data = 0;
    }

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    /* ---------------- 状态判断 ---------------- */

    bool isEmpty() const { return front == rear; }

    bool isFull() const { return (rear + 1) % capacity == front; }

    /* 当前元素个数 */
    int size() const
    {
        return (rear - front + capacity) % capacity;
    }

    int getCapacity() const { return capacity; }

    /* ---------------- 入队与出队 ---------------- */

    /* 入队，队列满返回 false */
    bool enqueue(const T& value)
    {
        if (isFull()) return false;

        data[rear] = value;
        rear       = (rear + 1) % capacity;
        return true;
    }

    /* 出队，队列空返回 false（不修改 valueOut） */
    bool dequeue(T* valueOut)
    {
        if (isEmpty()) return false;

        if (valueOut != 0) *valueOut = data[front];
        front = (front + 1) % capacity;
        return true;
    }

    /* 取队头元素但不删除，队空返回 false */
    bool peekFront(T* valueOut) const
    {
        if (isEmpty()) return false;
        if (valueOut != 0) *valueOut = data[front];
        return true;
    }

    /* ---------------- 其他 ---------------- */

    /* 按下标访问（0 表示队头），越界返回 false */
    bool at(int index, T* valueOut) const
    {
        if (index < 0 || index >= size()) return false;
        if (valueOut != 0) *valueOut = data[(front + index) % capacity];
        return true;
    }

    void clear()
    {
        front = 0;
        rear  = 0;
    }

    /* 遍历：从队头到队尾依次访问，用回调替代迭代器 */
    void forEach(void (*visit)(const T& item, void* context), void* context) const
    {
        int n = size();
        for (int i = 0; i < n; i++)
        {
            visit(data[(front + i) % capacity], context);
        }
    }

private:
    T*  data;       /* 环形数组 */
    int front;      /* 队头下标 */
    int rear;       /* 队尾下标（下一个空位） */
    int capacity;   /* 容量，实际最多放 capacity - 1 个元素 */
};

#endif /* HMS_QUEUE_H */
