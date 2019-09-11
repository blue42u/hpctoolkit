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
