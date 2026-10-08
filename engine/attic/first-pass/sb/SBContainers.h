// The original's container templates. They were header-only in the original (always inlined),
// so their method names are ours; their behaviour follows the inlined code.
//
// SBList<T>  (24 bytes in the original): a std::list<T> (+0 next, +4 prev, +8 count), then a
//            cached node (+0xc), a "cache invalid" flag (+0x10, set to 1 by constructors and by
//            every insertion/removal) and the cached index (+0x14). operator[] walks from the
//            closest of: the front, the back, or the cached node. The cache only speeds things up,
//            so here operator[] simply walks; results are the same.
// SBArray<T> (20 bytes): a std::vector<T> (+0 begin, +4 end, +8 capacity) and two more fields
//            (+0xc, +0x10 flag) the same way.
// SBListUnique<T>: an SBList that refuses duplicates on insertion.
// SB...AutoDelete: deletes the pointed objects when cleared / destroyed.
#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <vector>

template <class T>
class SBList {
public:
    typedef typename std::list<T>::iterator iterator;
    typedef typename std::list<T>::const_iterator const_iterator;

    SBList() {}
    ~SBList() {}

    uint32_t GetCount() const { return (uint32_t)items.size(); }
    bool IsEmpty() const { return items.empty(); }
    void AddLast(const T& v) { items.push_back(v); }
    void AddFirst(const T& v) { items.push_front(v); }
    // inserts before position i (i == count appends)
    void InsertAt(uint32_t i, const T& v) { items.insert(at(i), v); }
    void RemoveAt(uint32_t i) { items.erase(at(i)); }
    // removes the first element equal to v; returns whether one was found
    bool Remove(const T& v) {
        for (iterator it = items.begin(); it != items.end(); ++it)
            if (*it == v) { items.erase(it); return true; }
        return false;
    }
    void RemoveAll() { items.clear(); }
    bool Contains(const T& v) const { return std::find(items.begin(), items.end(), v) != items.end(); }
    // index of the first element equal to v, or -1
    int IndexOf(const T& v) const {
        int i = 0;
        for (const_iterator it = items.begin(); it != items.end(); ++it, ++i)
            if (*it == v) return i;
        return -1;
    }
    T& operator[](uint32_t i) { return *at(i); }
    const T& operator[](uint32_t i) const { return *const_cast<SBList*>(this)->at(i); }
    T& First() { return items.front(); }
    T& Last() { return items.back(); }

    iterator begin() { return items.begin(); }
    iterator end() { return items.end(); }
    const_iterator begin() const { return items.begin(); }
    const_iterator end() const { return items.end(); }

    std::list<T> items;

protected:
    iterator at(uint32_t i) {
        size_t n = items.size();
        if (i <= n / 2) { iterator it = items.begin(); std::advance(it, i); return it; }
        iterator it = items.end();
        std::advance(it, -(ptrdiff_t)(n - i));
        return it;
    }
};

template <class T>
class SBListUnique : public SBList<T> {
public:
    // returns false (and adds nothing) when v is already in the list
    bool AddLast(const T& v) { if (this->Contains(v)) return false; SBList<T>::AddLast(v); return true; }
    bool AddFirst(const T& v) { if (this->Contains(v)) return false; SBList<T>::AddFirst(v); return true; }
};

template <class T>
class SBListAutoDelete : public SBList<T> {
public:
    ~SBListAutoDelete() { DeleteAll(); }
    void DeleteAll() { for (T p : this->items) delete p; this->items.clear(); }
};

template <class T>
class SBArray {
public:
    typedef typename std::vector<T>::iterator iterator;
    typedef typename std::vector<T>::const_iterator const_iterator;

    SBArray() {}
    ~SBArray() {}

    uint32_t GetCount() const { return (uint32_t)items.size(); }
    bool IsEmpty() const { return items.empty(); }
    void Add(const T& v) { items.push_back(v); }
    void AddLast(const T& v) { items.push_back(v); }
    void InsertAt(uint32_t i, const T& v) { items.insert(items.begin() + i, v); }
    void RemoveAt(uint32_t i) { items.erase(items.begin() + i); }
    bool Remove(const T& v) {
        iterator it = std::find(items.begin(), items.end(), v);
        if (it == items.end()) return false;
        items.erase(it);
        return true;
    }
    void RemoveAll() { items.clear(); }
    void SetCount(uint32_t n) { items.resize(n); }
    void Reserve(uint32_t n) { items.reserve(n); }
    bool Contains(const T& v) const { return std::find(items.begin(), items.end(), v) != items.end(); }
    int IndexOf(const T& v) const {
        const_iterator it = std::find(items.begin(), items.end(), v);
        return it == items.end() ? -1 : (int)(it - items.begin());
    }
    T& operator[](uint32_t i) { return items[i]; }
    const T& operator[](uint32_t i) const { return items[i]; }
    T* GetData() { return items.data(); }

    iterator begin() { return items.begin(); }
    iterator end() { return items.end(); }
    const_iterator begin() const { return items.begin(); }
    const_iterator end() const { return items.end(); }

    std::vector<T> items;
};

template <class T>
class SBArrayAutoDelete : public SBArray<T> {
public:
    ~SBArrayAutoDelete() { DeleteAll(); }
    void DeleteAll() { for (T p : this->items) delete p; this->items.clear(); }
};
