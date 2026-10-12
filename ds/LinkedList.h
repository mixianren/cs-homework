#ifndef HMS_LINKEDLIST_H
#define HMS_LINKEDLIST_H

/* ============================================================================
 *  ds/LinkedList.h  ——  双向链表（带头结点）
 * ----------------------------------------------------------------------------
 *  用途：存放患者的预约记录，支持频繁的取消（中间删除）操作。
 *  对应项目框架.txt 第四章。
 *
 *  为什么用链表而不是顺序表：
 *      取消预约需要从中间删掉一条记录。
 *      顺序表删除要把后面所有元素前移一位，是 O(n)；
 *      双向链表删除只要改前驱和后继两个指针，是 O(1)。
 *
 *  内部结构（带头结点的双向链表）：
 *          head → [哨兵] ⇄ [结点1] ⇄ [结点2] ⇄ ... ⇄ [尾结点]
 *
 *  头结点不存有效数据。好处是"在第一个位置插入/删除"与"在中间插入/删除"
 *  的代码完全一样，不需要单独判断空链表和首元素这两种特殊情况。
 *
 *  【为什么本文件没有对应的 .cpp】
 *  与 SeqList.h 同理：这是模板类，实现必须写在头文件里，否则链接期报
 *  undefined reference。详见 ds/SeqList.h 顶部的完整说明。
 * ==========================================================================*/

#include <cstdlib>

template <typename T>
class LinkedList
{
public:
    /* ---------------- 结点 ---------------- */
    struct Node
    {
        T     data;
        Node* prev;
        Node* next;
    };

    /* ---------------- 构造与析构 ---------------- */

    LinkedList()
    {
        head = new Node;
        head->prev = 0;
        head->next = 0;
        length = 0;
    }

    ~LinkedList()
    {
        clear();
        delete head;
        head = 0;
    }

    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    /* ---------------- 容量与状态 ---------------- */

    int  size()    const { return length; }
    bool isEmpty() const { return length == 0; }

    /* 第一个有效结点，空链表返回 0 —— 供外部遍历使用 */
    Node* first() const
    {
        return (head->next == 0) ? 0 : head->next;
    }

    /* 取某结点的后继，到尾部返回 0 */
    static Node* nextOf(Node* node)
    {
        return (node == 0) ? 0 : node->next;
    }

    /* ---------------- 插入 ---------------- */

    /* 头部插入，O(1) */
    Node* pushFront(const T& value)
    {
        return insertAfter(head, value);
    }

    /* 尾部插入。本实现不维护尾指针，需要从头走到尾，是 O(n)。
       预约记录只在末尾追加、且一次遍历就能完成，因此这样实现足够，
       少维护一个指针也少一处出错的地方。 */
    Node* pushBack(const T& value)
    {
        Node* tail = head;
        while (tail->next != 0) tail = tail->next;
        return insertAfter(tail, value);
    }

    /* 在指定结点之后插入，O(1) */
    Node* insertAfter(Node* pos, const T& value)
    {
        if (pos == 0) return 0;

        Node* node = new Node;
        node->data = value;
        node->prev = pos;
        node->next = pos->next;

        if (pos->next != 0) pos->next->prev = node;
        pos->next = node;

        length++;
        return node;
    }

    /* ---------------- 删除 ---------------- */

    /* 删除指定结点，O(1)。
       注意：这里省掉的是"删除动作"的开销，
       找到这个结点本身仍然要 O(n) 遍历。 */
    bool erase(Node* node)
    {
        if (node == 0 || node == head) return false;

        node->prev->next = node->next;
        if (node->next != 0) node->next->prev = node->prev;

        delete node;
        length--;
        return true;
    }

    /* 按条件查找第一个匹配结点，找不到返回 0 */
    Node* find(bool (*match)(const T& item, const void* key), const void* key) const
    {
        for (Node* p = head->next; p != 0; p = p->next)
        {
            if (match(p->data, key)) return p;
        }
        return 0;
    }

    /* 按条件删除第一个匹配结点，返回是否删除 */
    bool removeIf(bool (*match)(const T& item, const void* key), const void* key)
    {
        Node* node = find(match, key);
        if (node == 0) return false;
        return erase(node);
    }

    /* ---------------- 遍历 ---------------- */

    void forEach(void (*visit)(T& item, void* context), void* context)
    {
        for (Node* p = head->next; p != 0; p = p->next) visit(p->data, context);
    }

    void forEach(void (*visit)(const T& item, void* context), void* context) const
    {
        for (Node* p = head->next; p != 0; p = p->next) visit(p->data, context);
    }

    void clear()
    {
        Node* p = head->next;
        while (p != 0)
        {
            Node* next = p->next;
            delete p;
            p = next;
        }
        head->next = 0;
        length = 0;
    }

private:
    Node* head;    /* 头结点（哨兵），不存有效数据 */
    int   length;  /* 有效元素个数 */
};

#endif /* HMS_LINKEDLIST_H */
