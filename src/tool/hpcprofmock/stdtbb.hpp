// Mockup for hpcprof

#include <functional>
#include <tbb/concurrent_vector.h>
#include <tbb/concurrent_hash_map.h>
#include <tbb/concurrent_unordered_set.h>

namespace stdtbb {

namespace {
  template<class T, class H, class E>
  class StdHashCompare {
    H hasher;
  public:
    StdHashCompare() = default;
    StdHashCompare(const StdHashCompare&) = default;
    ~StdHashCompare() = default;
    bool equal(const T& a, const T& b) const { return E{}(a, b); }
    std::size_t hash(const T& a) const { return hasher(a); }
  };
}

template<
  class K, class V,
  class H = std::hash<K>, class E = std::equal_to<K>,
  class A = std::allocator<std::pair<const K, V>>
>
class unordered_map
  : private tbb::concurrent_hash_map<K, V, StdHashCompare<K,H,E>, A> {
  using base = tbb::concurrent_hash_map<K, V, StdHashCompare<K,H,E>, A>;
public:
  std::pair<const typename base::value_type*, bool> insert(const typename base::value_type& v) {
    typename base::const_accessor ca;
    bool b = base::insert(ca, v);
    return std::make_pair(ca.operator->(), b);
  }
  std::pair<const typename base::value_type*, bool> insert(typename base::value_type&& v) {
    typename base::const_accessor ca;
    bool b = base::insert(ca, v);
    return std::make_pair(ca.operator->(), b);
  }
  using base::size;
  using base::begin;
  using base::end;
  void reserve(std::size_t c) {
    base::rehash(c * 2);
  }
};

template<
  class K, class H = std::hash<K>, class E = std::equal_to<K>,
  class A = std::allocator<K>
>
using unordered_set = tbb::concurrent_unordered_set<K, H, E, A>;

template<
  class T,
  class A = std::allocator<T>
>
using vector = tbb::concurrent_vector<T, A>;

}

#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace stdlock {

template<class K, class V>
class unordered_map {
  using core_t = std::unordered_map<K, V>;
  core_t core;
  std::shared_mutex lock;
public:
  std::pair<typename core_t::const_iterator, bool> insert(const typename core_t::value_type& v) {
    {
      std::shared_lock l(lock);
      auto x = core.find(v.first);
      if(x != core.end()) return {x, false};
    }
    {
      std::unique_lock l(lock);
      return core.insert(v);
    }
  }
  std::pair<typename core_t::const_iterator, bool> insert(typename core_t::value_type&& v) {
    {
      std::shared_lock l(lock);
      auto x = core.find(v.first);
      if(x != core.end()) return {x, false};
    }
    {
      std::unique_lock l(lock);
      return core.insert(v);
    }
  }
  std::size_t size() const { return core.size(); }
  typename core_t::const_iterator begin() const { return core.begin(); }
  typename core_t::const_iterator end() const { return core.end(); }
};

}
