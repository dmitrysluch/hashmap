//    DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE Version 2,
//    February 2020
//
//    Copyright(C) 2020 Sluch Dmitry<dbsluch@edu.hse.ru>
//
//    Everyone is permitted to copy and distribute verbatim
//    or modified copies of this license document,
//    and changing it is allowed as long as the name is changed.
//
//    DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE TERMS AND
//    CONDITIONS FOR COPYING,
//    DISTRIBUTION AND MODIFICATION
//
//    0. You just DO WHAT THE FUCK YOU WANT TO.
//    cpplint told that there should be a license so there it is

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <list>
#include <stdexcept>
#include <utility>
#include <vector>
// c++ 14 doesn't have optional,
// so I copypasted my implementation from oimp course
template <typename T>
class Optional {
 private:
  alignas(T) unsigned char data[sizeof(T)];
  bool defined = false;
  T &raw_value() { return *reinterpret_cast<T *>(data); }
  const T &raw_value() const { return *reinterpret_cast<const T *>(data); }

 public:
  Optional() = default;
  Optional(const T &elem) : defined(true) {new(data) T(elem); }
  Optional(T &&elem) : defined(true) {new(data) T(std::move(elem)); }
  Optional(const Optional &other) : defined(other.defined) {
    if (defined) {
      new(data) T(other.raw_value());
    }
  }
  Optional &operator=(const Optional &other) {
    if (defined && other.defined) {
      raw_value() = other.raw_value();
    } else if (defined && !other.defined) {
      raw_value().~T();
      defined = other.defined;
    } else if (!defined && other.defined) {
      new(data) T(other.raw_value());
      defined = other.defined;
    }
    return *this;
  }
  Optional &operator=(const T &elem) {
    if (defined) {
      raw_value() = elem;
    } else {
      defined = true;
      new(data) T(elem);
    }
    return *this;
  }
  Optional &operator=(T &&elem) {
    if (defined) {
      raw_value() = std::move(elem);
    } else {
      defined = true;
      new(data) T(std::move(elem));
    }
    return *this;
  }

  bool has_value() const { return defined; }

  T &operator*() { return raw_value(); }
  const T &operator*() const { return raw_value(); }

  T *operator->() { return &raw_value(); }
  const T *operator->() const { return &raw_value(); }
  T &value() {
    if (has_value()) {
      return raw_value();
    } else {
      throw std::out_of_range("bad optional access");
    }
  }
  const T &value() const {
    if (has_value()) {
      return raw_value();
    } else {
      throw std::out_of_range("bad optional access");
    }
  }

  void reset() {
    if (defined) {
      raw_value().~T();
      defined = false;
    }
  }
  operator bool() const { return has_value(); }
  ~Optional() { reset(); }
};

template <class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
  typedef std::pair<const KeyType, ValueType> PairType;

 public:
  typedef typename std::list<PairType>::iterator iterator;
  typedef typename std::list<PairType>::const_iterator const_iterator;

 private:
  typedef Optional<iterator> PointerType;
  static const size_t kInverseMinLoadFactor = 8;
  static const size_t kInverseNormalLoadFactor = 4;
  static const size_t kInverseMaxLoadFactor = 2;
  std::list<PairType> list_;
  size_t size_ = 0;
  std::vector<PointerType> table_;
  Hash hasher_;
  inline void insert_internal(std::vector<PointerType> *dst,
                              PointerType pointer) {
    size_t idx = hasher_(pointer.value()->first) % dst->size();
    for (; (*dst)[idx]; idx = (idx + 1) % dst->size()) {
    }
    (*dst)[idx] = pointer;
  }
  inline size_t find_internal(const KeyType &key) const {
    size_t idx = hasher_(key) % table_.size();
    for (; table_[idx]; idx = (idx + 1) % table_.size()) {
      if (table_[idx].value()->first == key) {
        return idx;
      }
    }
    return idx;
  }
  void rehash(size_t new_capacity) {
    std::vector<PointerType> new_table(new_capacity);
    for (auto &ptr : table_) {
      if (ptr) {
        insert_internal(&new_table, ptr);
      }
    }
    table_ = std::move(new_table);
  }

 public:
  HashMap(Hash hasher = Hash()) : hasher_(hasher) {}
  template <typename Iterator>
  HashMap(Iterator begin, Iterator end, Hash hasher = Hash())
      : list_(begin, end),
        size_(list_.size()),  // if begin and end are RandomAccessIterators using
                             // distance(begin, end) would be faster, but with
                             // InputIterator that code can cause UB
        table_(size_ * kInverseNormalLoadFactor),
        hasher_(hasher) {
    for (auto iter = list_.begin(); iter != list_.end(); ++iter) {
      insert_internal(&table_, iter);
    }
  }
  HashMap(std::initializer_list<PairType> initializer, Hash hasher = Hash())
      : list_(initializer),
        size_(initializer.size()),
        table_(size_ * kInverseNormalLoadFactor),
        hasher_(hasher) {
    for (auto iter = list_.begin(); iter != list_.end(); ++iter) {
      insert_internal(&table_, iter);
    }
  }
  HashMap(const HashMap &other) : HashMap(other.begin(), other.end()) {}
  HashMap(HashMap &&other) {
    std::swap(table_, other.table_);
    std::swap(list_, other.list_);
    std::swap(hasher_, other.hasher_);
    std::swap(size_, other.size_);
  }
  HashMap &operator=(const HashMap &other) {
    list_.clear();  // can't assing list because keys stored in list are marked
                    // as constant
    std::copy(other.list_.begin(), other.list_.end(),
              std::back_inserter(list_));
    table_ = other.table_;
    size_ = other.size_;
    hasher_ = other.hasher_;
    return *this;
  }
  HashMap &operator=(HashMap &&other) {
    std::swap(table_, other.table_);
    std::swap(list_, other.list_);
    std::swap(hasher_, other.hasher_);
    std::swap(size_, other.size_);
    return *this;
  }
  size_t size() const { return size_; }
  bool empty() const { return !size_; }
  Hash hash_function() const { return hasher_; }
  void insert(const PairType &key_value) {
    if ((size_ + 1) * kInverseMaxLoadFactor > table_.size())
      rehash((size_ + 1) * kInverseNormalLoadFactor);
    size_t idx = find_internal(key_value.first);
    if (!table_[idx]) {
      list_.push_back(key_value);
      table_[idx] = --list_.end();
      ++size_;
    }
  }
  void erase(const KeyType &key) {
    if (empty()) return;
    size_t idx = find_internal(key);
    if (table_[idx]) {
      // recover chains
      list_.erase(table_[idx].value());
      for (size_t i = (idx + 1) % table_.size(); table_[i];
           i = (i + 1) % table_.size()) {
        size_t hash = hasher_(table_[i].value()->first) % table_.size();
        if (hash <= idx || hash > i) {
          table_[idx] = table_[i];
          idx = i;
          continue;
        }
      }
      table_[idx].reset();
      --size_;
    }
    if (size_ * kInverseMinLoadFactor < table_.size())
      rehash(size_ * kInverseNormalLoadFactor);
  }
  iterator find(const KeyType &key) {
    if (empty()) return list_.end();
    size_t idx = find_internal(key);
    if (table_[idx])
      return table_[idx].value();
    else
      return list_.end();
  }
  const_iterator find(const KeyType &key) const {
    if (empty()) return list_.end();
    size_t idx = find_internal(key);
    if (table_[idx])
      return table_[idx].value();
    else
      return list_.end();
  }
  ValueType &operator[](const KeyType &key) {
    size_t idx;
    if ((size() + 1) * kInverseMaxLoadFactor >= table_.size()) {
      rehash((size() + 1) * kInverseNormalLoadFactor);
    }
    idx = find_internal(key);
    if (!table_[idx]) {
      list_.push_back(PairType(key, ValueType()));
      ++size_;
      table_[idx] = --list_.end();
    }
    return table_[idx].value()->second;
  }
  const ValueType &at(const KeyType &key) const {
    if (empty()) throw std::out_of_range("Key doesn't exists");
    size_t idx = find_internal(key);
    if (!table_[idx]) throw std::out_of_range("Key doesn't exists");
    return table_[idx].value()->second;
  }
  void clear() {
    list_.clear();
    table_.clear();
    size_ = 0;
  }
  iterator begin() { return list_.begin(); }
  iterator end() { return list_.end(); }
  const_iterator begin() const { return list_.begin(); }
  const_iterator end() const { return list_.end(); }
};
