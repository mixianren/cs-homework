/* ============================================================================
 *  ds/HashTable.cpp  ——  哈希表实现
 * ----------------------------------------------------------------------------
 *  声明见 ds/HashTable.h。
 * ==========================================================================*/

#include "ds/HashTable.h"

/* ==========================================================================
 *  整数键哈希表 HashTable
 * ==========================================================================*/

HashTable::HashTable()
{
    buckets = new Node*[BUCKET_COUNT];
    for (int i = 0; i < BUCKET_COUNT; i++) buckets[i] = 0;
    count = 0;
}

HashTable::~HashTable()
{
    clear();
    delete[] buckets;
    buckets = 0;
}

int HashTable::size() const
{
    return count;
}

bool HashTable::isEmpty() const
{
    return count == 0;
}

/* 除留余数法。负数键也要落在合法范围内 */
int HashTable::hashOf(int key) const
{
    int index = key % BUCKET_COUNT;
    if (index < 0) index = index + BUCKET_COUNT;
    return index;
}

void HashTable::put(int key, int value)
{
    int   index = hashOf(key);
    Node* p     = buckets[index];

    /* 键已存在则只更新值，不新增结点 */
    while (p != 0)
    {
        if (p->key == key)
        {
            p->value = value;
            return;
        }
        p = p->next;
    }

    /* 没找到，头插到该桶的链上 */
    Node* node     = new Node;
    node->key      = key;
    node->value    = value;
    node->next     = buckets[index];
    buckets[index] = node;
    count++;
}

bool HashTable::get(int key, int* valueOut) const
{
    int   index = hashOf(key);
    Node* p     = buckets[index];

    while (p != 0)
    {
        if (p->key == key)
        {
            if (valueOut != 0) *valueOut = p->value;
            return true;
        }
        p = p->next;
    }
    return false;
}

int HashTable::getOrDefault(int key, int dflt) const
{
    int value = 0;
    if (get(key, &value)) return value;
    return dflt;
}

bool HashTable::contains(int key) const
{
    return get(key, 0);
}

bool HashTable::remove(int key)
{
    int   index = hashOf(key);
    Node* p     = buckets[index];
    Node* prev  = 0;

    while (p != 0)
    {
        if (p->key == key)
        {
            /* 摘链：注意区分"删的是链上第一个"和"删的是中间结点" */
            if (prev == 0) buckets[index] = p->next;
            else           prev->next     = p->next;

            delete p;
            count--;
            return true;
        }
        prev = p;
        p    = p->next;
    }
    return false;
}

void HashTable::clear()
{
    for (int i = 0; i < BUCKET_COUNT; i++)
    {
        Node* p = buckets[i];
        while (p != 0)
        {
            Node* next = p->next;
            delete p;
            p = next;
        }
        buckets[i] = 0;
    }
    count = 0;
}

int HashTable::longestChain() const
{
    int longest = 0;

    for (int i = 0; i < BUCKET_COUNT; i++)
    {
        int   len = 0;
        Node* p   = buckets[i];
        while (p != 0)
        {
            len++;
            p = p->next;
        }
        if (len > longest) longest = len;
    }
    return longest;
}

/* ==========================================================================
 *  字符串键哈希表 StrHashTable
 * ==========================================================================*/

StrHashTable::StrHashTable()
{
    buckets = new Node*[BUCKET_COUNT];
    for (int i = 0; i < BUCKET_COUNT; i++) buckets[i] = 0;
    count = 0;
}

StrHashTable::~StrHashTable()
{
    clear();
    delete[] buckets;
    buckets = 0;
}

int StrHashTable::size() const
{
    return count;
}

bool StrHashTable::isEmpty() const
{
    return count == 0;
}

/* 各字符 ASCII 值累加后取余 */
int StrHashTable::hashOf(const char* key) const
{
    if (key == 0) return 0;

    unsigned int sum = 0;
    for (int i = 0; key[i] != '\0'; i++)
    {
        sum = sum + (unsigned char)key[i];
    }
    return (int)(sum % (unsigned int)BUCKET_COUNT);
}

void StrHashTable::put(const char* key, int value)
{
    if (key == 0) return;

    int   index = hashOf(key);
    Node* p     = buckets[index];

    while (p != 0)
    {
        if (compare(p->key, key) == 0)
        {
            p->value = value;
            return;
        }
        p = p->next;
    }

    Node* node     = new Node;
    copyString(node->key, key, MAX_KEY_LEN);
    node->value    = value;
    node->next     = buckets[index];
    buckets[index] = node;
    count++;
}

bool StrHashTable::get(const char* key, int* valueOut) const
{
    if (key == 0) return false;

    int   index = hashOf(key);
    Node* p     = buckets[index];

    while (p != 0)
    {
        if (compare(p->key, key) == 0)
        {
            if (valueOut != 0) *valueOut = p->value;
            return true;
        }
        p = p->next;
    }
    return false;
}

int StrHashTable::getOrDefault(const char* key, int dflt) const
{
    int value = 0;
    if (get(key, &value)) return value;
    return dflt;
}

bool StrHashTable::contains(const char* key) const
{
    return get(key, 0);
}

bool StrHashTable::remove(const char* key)
{
    if (key == 0) return false;

    int   index = hashOf(key);
    Node* p     = buckets[index];
    Node* prev  = 0;

    while (p != 0)
    {
        if (compare(p->key, key) == 0)
        {
            if (prev == 0) buckets[index] = p->next;
            else           prev->next     = p->next;

            delete p;
            count--;
            return true;
        }
        prev = p;
        p    = p->next;
    }
    return false;
}

void StrHashTable::clear()
{
    for (int i = 0; i < BUCKET_COUNT; i++)
    {
        Node* p = buckets[i];
        while (p != 0)
        {
            Node* next = p->next;
            delete p;
            p = next;
        }
        buckets[i] = 0;
    }
    count = 0;
}

int StrHashTable::longestChain() const
{
    int longest = 0;

    for (int i = 0; i < BUCKET_COUNT; i++)
    {
        int   len = 0;
        Node* p   = buckets[i];
        while (p != 0)
        {
            len++;
            p = p->next;
        }
        if (len > longest) longest = len;
    }
    return longest;
}

/* ---------------- 自己实现的字符串工具 ---------------- */

int StrHashTable::compare(const char* a, const char* b)
{
    if (a == 0 || b == 0) return (a == b) ? 0 : 1;

    int i = 0;
    while (a[i] != '\0' && b[i] != '\0')
    {
        if (a[i] != b[i]) return (a[i] < b[i]) ? -1 : 1;
        i++;
    }
    if (a[i] == b[i]) return 0;
    return (a[i] == '\0') ? -1 : 1;
}

void StrHashTable::copyString(char* dst, const char* src, int maxLen)
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
