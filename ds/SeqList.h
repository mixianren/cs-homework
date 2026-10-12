#ifndef HMS_SEQLIST_H
#define HMS_SEQLIST_H

/* ============================================================================
 *  ds/SeqList.h  --  顺序表（动态数组）
 * ----------------------------------------------------------------------------
 *  用途：存放科室表、医生表、患者表等实体集合。
 *  对应设计说明书 2.1 节（1）。
 *
 *  内部结构：
 *      T*  data      指向堆上数组
 *      int length    当前元素个数
 *      int capacity  当前可容纳个数
 *
 *  【为什么本文件没有对应的 .cpp】
 *  这是一个模板类，实现必须与声明放在同一个文件里。
 *  原因：模板只有在实例化成具体类型（如 SeqList<Doctor>）时才生成代码，
 *        编译器在实例化那一刻必须看到完整的函数体。如果按常规把实现放进
 *        SeqList.cpp，编译 main.cpp 时只能看到声明、看不到函数体，
 *        链接阶段就会报 undefined reference。
 *  这是 C++ 模板的固有要求，也是通用做法（标准库的 <vector>、<map>
 *  同样是声明与实现全部写在头文件中）。
 *  本工程中非模板的 HashTable、Sort 则按常规习惯拆成 .h + .cpp。
 *
 *  【关于移动语义】
 *  本类禁用了拷贝（拷贝构造与拷贝赋值都 = delete），因为顺序表持有堆内存，
 *  浅拷贝会导致两块内存指向同一个数组、析构时重复释放。
 *  但"禁用拷贝"不等于"不能转移所有权"：扩容时必须把旧数组的元素搬到新数组，
 *  嵌套使用时（如 SeqList< SeqList<T> >）也需要把内层列表搬进外层的槽位。
 *  所以这里实现了移动构造与移动赋值：只转移指针，不复制数据。
 * ==========================================================================*/

#include <cstdlib>

template <typename T>
class SeqList
{
public:
    /* ---------------- 构造与析构 ---------------- */

    SeqList()
    {
        capacity = 8;
        length   = 0;
        data     = new T[capacity];
    }

    ~SeqList()
    {
        delete[] data;
        data = 0;
        length = 0;
        capacity = 0;
    }

    /* 禁止拷贝：两个对象指向同一块数组会导致重复释放 */
    SeqList(const SeqList&) = delete;
    SeqList& operator=(const SeqList&) = delete;

    /* 允许移动：只把指针和计数接过来，源对象置空 */
    SeqList(SeqList&& other)
    {
        data     = other.data;
        length   = other.length;
        capacity = other.capacity;

        other.data     = 0;
        other.length   = 0;
        other.capacity = 0;
    }

    SeqList& operator=(SeqList&& other)
    {
        if (this != &other)
        {
            delete[] data;

            data     = other.data;
            length   = other.length;
            capacity = other.capacity;

            other.data     = 0;
            other.length   = 0;
            other.capacity = 0;
        }
        return *this;
    }

    /* ---------------- 容量与状态 ---------------- */

    int  size()     const { return length; }
    int  getCapacity() const { return capacity; }
    bool isEmpty()  const { return length == 0; }

    void clear() { length = 0; }

    /* ---------------- 扩容 ---------------- */

    /* 容量不足时按 2 倍增长。
       元素用移动语义搬到新数组：对 int 这类基本类型等价于直接拷贝，
       对内层 SeqList 则是转移指针，既不复制数据也不会重复释放。 */
    void ensureCapacity(int need)
    {
        if (need <= capacity) return;

        int newCapacity = capacity;
        while (newCapacity < need) newCapacity = newCapacity * 2;

        T* newData = new T[newCapacity];
        for (int i = 0; i < length; i++) newData[i] = static_cast<T&&>(data[i]);

        delete[] data;
        data     = newData;
        capacity = newCapacity;
    }

    /* ---------------- 插入 ---------------- */

    /* 尾部插入，返回新元素的下标。
       参数用 const 引用：既能接左值也能接右值（不要另写一个 T&& 重载，
       否则非 const 左值会优先匹配 T&& 版本，反而引发重载解析问题）。
       写入时按右值处理，让支持移动赋值的元素类型少复制一次。 */
    int insertBack(const T& value)
    {
        ensureCapacity(length + 1);
        data[length] = static_cast<T&&>(const_cast<T&>(value));
        return length++;
    }

    /* 在指定位置插入（原位置及之后的元素整体后移） */
    bool insertAt(int pos, const T& value)
    {
        if (pos < 0 || pos > length) return false;

        ensureCapacity(length + 1);
        for (int i = length; i > pos; i--)
            data[i] = static_cast<T&&>(data[i - 1]);
        data[pos] = static_cast<T&&>(const_cast<T&>(value));
        length++;
        return true;
    }

    /* ---------------- 访问 ---------------- */

    /* 按下标读取，越界返回 dflt，不崩溃。
       注意：读元素时更推荐 ptrAt()，避免为了传默认值而构造 dummy 对象。 */
    T at(int index, const T& dflt) const
    {
        if (index < 0 || index >= length) return dflt;
        return data[index];
    }

    /* 按下标写入，越界返回 false */
    bool setAt(int index, const T& value)
    {
        if (index < 0 || index >= length) return false;
        data[index] = value;
        return true;
    }

    /* 取元素指针，越界返回 0 —— 业务层修改实体字段、或读取元素时使用 */
    T* ptrAt(int index)
    {
        if (index < 0 || index >= length) return 0;
        return &data[index];
    }

    const T* ptrAt(int index) const
    {
        if (index < 0 || index >= length) return 0;
        return &data[index];
    }

    /* ---------------- 查找与删除 ---------------- */

    /* 按条件查找第一个匹配元素，找不到返回 -1
       match 是函数指针：返回 1 表示该元素就是我们要找的 */
    int indexOf(bool (*match)(const T& item, const void* key), const void* key) const
    {
        for (int i = 0; i < length; i++)
        {
            if (match(data[i], key)) return i;
        }
        return -1;
    }

    /* 删除指定下标，后面的元素整体前移，返回是否成功
       注意：删除是 O(n)，这是顺序表的固有代价 */
    bool erase(int index)
    {
        if (index < 0 || index >= length) return false;

        for (int i = index; i < length - 1; i++)
            data[i] = static_cast<T&&>(data[i + 1]);
        length--;
        return true;
    }

    /* 按条件删除第一个匹配元素 */
    bool removeIf(bool (*match)(const T& item, const void* key), const void* key)
    {
        int index = indexOf(match, key);
        if (index < 0) return false;
        return erase(index);
    }

    /* ---------------- 遍历 ---------------- */

    /* 依次访问每个元素，visit 可读取或修改元素。
       用回调替代迭代器，这是本工程统一的遍历方式。 */
    void forEach(void (*visit)(T& item, void* context), void* context)
    {
        for (int i = 0; i < length; i++) visit(data[i], context);
    }

    void forEach(void (*visit)(const T& item, void* context), void* context) const
    {
        for (int i = 0; i < length; i++) visit(data[i], context);
    }

private:
    T*  data;       /* 指向堆上数组 */
    int length;     /* 当前元素个数 */
    int capacity;   /* 当前可容纳个数 */
};

#endif /* HMS_SEQLIST_H */
