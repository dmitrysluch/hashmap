// Copyright 2020 Dmitry Sluch dbsluch@edu.hse.ru

#include <stddef.h>
#include <algorithm>
#include <iterator>
#include <list>
#include <stdexcept>
#include <utility>
#include <vector>

// Optional class manages value that may or may not be present.
// Optional isn't implemented in C++ 14, so I had to use my implementation.
template <typename T>
class Optional {
 public:
  Optional() = default;

  explicit Optional(const T& elem) : defined(true) { new(data) T(elem); }

  explicit Optional(T&& elem) : defined(true) { new(data) T(std::move(elem)); }

  Optional(const Optional& other) : defined(other.defined) {
    if (defined) {
      new(data) T(other.raw_value());
    }
  }

  Optional& operator=(const Optional& other) {
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

  Optional& operator=(const T& elem) {
    if (defined) {
      raw_value() = elem;
    } else {
      defined = true;
      new(data) T(elem);
    }
    return *this;
  }

  Optional& operator=(T&& elem) {
    if (defined) {
      raw_value() = std::move(elem);
    } else {
      defined = true;
      new(data) T(std::move(elem));
    }
    return *this;
  }

  bool has_value() const { return defined; }

  T& operator*() { return raw_value(); }
  const T& operator*() const { return raw_value(); }

  T* operator->() { return &raw_value(); }

  const T* operator->() const { return &raw_value(); }

  T& value() {
    if (has_value()) {
      return raw_value();
    } else {
      throw std::out_of_range("bad optional access");
    }
  }

  const T& value() const {
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

  ~Optional() { reset(); }

 private:
  alignas(T) unsigned char data[sizeof(T)];
  bool defined = false;
  T& raw_value() { return *reinterpret_cast<T*>(data); }
  const T& raw_value() const { return *reinterpret_cast<const T*>(data); }
};

// Implementation of key-value associative container based on a hash table
// using open addressing approach for collision resolution with linear probing
// and removing elements without tombstones.
// The table is resized by rehashing all the elements when the load factor
// becomes larger then 1/kInverseMaxLoadFactor
// or lower then 1/kInverseMinLoadFactor.
template <class KeyType, class ValueType, class Hash = std::hash<KeyType>>
class HashMap {
 public:
  typedef std::pair<const KeyType, ValueType> element;
  typedef typename std::list<element>::iterator iterator;
  typedef typename std::list<element>::const_iterator const_iterator;
  typedef Optional<iterator> optional_iterator;

  static constexpr size_t kInverseMinLoadFactor = 8;
  static constexpr size_t kInverseNormalLoadFactor = 4;
  static constexpr size_t kInverseMaxLoadFactor = 2;

  // Constructors.

  // Default constructor.
  HashMap(Hash hasher = Hash()) : table_(1), hasher_(hasher) {}

  // Constructs hash map from iterators.
  template <typename Iterator>
  HashMap(Iterator begin, Iterator end, Hash hasher = Hash())
      : list_(begin, end), size_(list_.size()), hasher_(hasher) {
    rehash();
  }

  // Constructs hash map from std::initializer_list.
  HashMap(std::initializer_list<element> initializer, Hash hasher = Hash())
      : HashMap(initializer.begin(), initializer.end()) {}

  // Copy constructor.
  HashMap(const HashMap& other) : HashMap(other.begin(), other.end()) {}

  // Move constructor.
  HashMap(HashMap&& other) {
    std::swap(table_, other.table_);
    std::swap(list_, other.list_);
    std::swap(hasher_, other.hasher_);
    std::swap(size_, other.size_);
  }

  // Assignment operators.

  // Copy assignment operator.
  HashMap& operator=(const HashMap& other) {
    if (&other == this) {
      return *this;
    }
    list_.clear();
    // Can't assing list because keys stored in list are marked as constant.
    std::copy(other.list_.begin(), other.list_.end(),
              std::back_inserter(list_));
    size_ = other.size_;
    hasher_ = other.hasher_;
    rehash();
    return *this;
  }

  // Move assignment operator.
  HashMap& operator=(HashMap&& other) {
    std::swap(table_, other.table_);
    std::swap(list_, other.list_);
    std::swap(hasher_, other.hasher_);
    std::swap(size_, other.size_);
    return *this;
  }

  // Returns the number of elements.
  size_t size() const { return size_; }

  // Checks whether the container is empty.
  bool empty() const { return !size_; }

  // Returns the function used to hash the keys.
  Hash hash_function() const { return hasher_; }

  // Inserts element into container, if there isn't element with the same key
  // already. Expected time complexity is O(1). Doesn't invalidates iterators.
  void insert(const element& key_value) {
    size_t cell = find_internal(table_, key_value.first);
    if (!table_[cell].has_value()) {
      list_.push_back(key_value);
      table_[cell] = --list_.end();
      ++size_;
      rehash_if_needed();
    }
  }

  // Removes element. Expected time complexity is O(1). Doesn't invalidate
  // iterators except the iterator to the erased element.
  void erase(const KeyType& key) {
    size_t cell = find_internal(table_, key);
    if (table_[cell].has_value()) {
      list_.erase(table_[cell].value());
      recover_chains(cell);
      --size_;
    }
    rehash_if_needed();
  }

  // Finds element with the specific key and returns an iterator.
  // Expected time complexity is O(1).
  iterator find(const KeyType& key) {
    size_t cell = find_internal(table_, key);
    if (table_[cell].has_value()) {
      return table_[cell].value();
    } else {
      return list_.end();
    }
  }

  // Finds element with the specific key and returns a constant iterator.
  // Expected time complexity is O(1).
  const_iterator find(const KeyType& key) const {
    size_t cell = find_internal(table_, key);
    if (table_[cell].has_value()) {
      return table_[cell].value();
    } else {
      return list_.end();
    }
  }

  // Returns a reference to the value that is mapped to the key. Performs
  // insertion if the key doesn't exist. Expected time complexity is O(1).
  ValueType& operator[](const KeyType& key) {
    size_t cell = find_internal(table_, key);
    if (!table_[cell].has_value()) {
      list_.push_back(element(key, ValueType()));
      ++size_;
      table_[cell] = --list_.end();
      rehash_if_needed();
      return (--list_.end())->second;
    }
    return table_[cell].value()->second;
  }

  // Returns a reference to the value that is mapped to the key. Throws
  // exception if the dey doesn't exists. Expected time complexity is O(1).
  const ValueType& at(const KeyType& key) const {
    size_t cell = find_internal(table_, key);
    if (!table_[cell].has_value()) {
      throw std::out_of_range("Key doesn't exists");
    }
    return table_[cell].value()->second;
  }

  // Erases all elements from the container.
  void clear() {
    list_.clear();
    table_.resize(1);
    table_[0].reset();
    size_ = 0;
  }

  // Iterators.
  iterator begin() { return list_.begin(); }
  iterator end() { return list_.end(); }
  const_iterator begin() const { return list_.begin(); }
  const_iterator end() const { return list_.end(); }

 private:
  std::list<element> list_;
  size_t size_ = 0;
  std::vector<optional_iterator> table_;
  Hash hasher_;

  // Finds the cell of the hash table in which iterator for the element with
  // given key is stored. If the key doesn't exist returns first free cell
  // in the chain.
  inline size_t find_internal(const std::vector<optional_iterator>& table,
                              const KeyType& key) const {
    size_t i = hasher_(key) % table.size();
    while (table[i].has_value()) {
      if (table[i].value()->first == key) {
        return i;
      }
      i = (i + 1) % table.size();
    }
    return i;
  }

  // Recovers the chains after an element was erased.
  // The function scans the elements following the one that has been deleted
  // until reaching the cell without an element and rearranges the elements
  // so that they may be accessed by the later search.
  // Expected time complexity is O(1).
  void recover_chains(size_t deleted) {
    size_t i = (deleted + 1) % table_.size();
    while (table_[i].has_value()) {
      // Get the cell of the hash table in which element in the i-th
      // cell would be placed if there were no collisions.
      size_t hash = hasher_(table_[i].value()->first) % table_.size();
      // Check whether the hash function points to the cell that goes in chain
      // before the one being deleted. If so put the i-th element in the cell
      // element from which has been deleted. Continue the algorithm as if
      // the i-sh element was the deleted one.
      if ((deleted < i && (hash <= deleted || hash > i)) ||
        (deleted > i && hash <= deleted && hash > i)) {
        table_[deleted] = table_[i];
        deleted = i;
      }
      i = (i + 1) % table_.size();
    }
    table_[deleted].reset();
  }

  size_t normal_capacity_for_size(size_t size) {
    return std::max<size_t>(1UL, size * kInverseNormalLoadFactor);
  }

  // Creates table of new capacity, copies all iterators to the new table,
  // doesn't invalidate iterators.
  void rehash() {
    std::vector<optional_iterator> new_table(normal_capacity_for_size(size_));
    for (auto iter = list_.begin(); iter != list_.end(); ++iter) {
      size_t cell = find_internal(new_table, iter->first);
      new_table[cell] = iter;
    }
    table_ = std::move(new_table);
  }

  // Checks whether table size hash to be increased or decreased and
  // triggers rehashing if so.
  void rehash_if_needed() {
    if (table_.size() < 1UL || table_.size() < kInverseMaxLoadFactor * size_) {
      rehash();
    } else if (table_.size() > kInverseMinLoadFactor * size_) {
      rehash();
    }
  }
};
