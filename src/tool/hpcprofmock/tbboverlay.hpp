// Mockup for hpcprof

#include <functional>
#include <tbb/concurrent_hash_map.h>

template<typename T>
class TBBStdHash {
  std::hash<T> hasher;
public:
  TBBStdHash() = default;
  TBBStdHash(const TBBStdHash<T>&) = default;
  ~TBBStdHash() = default;
  bool equal(const T& a, const T& b) const { return a == b; }
  std::size_t hash(const T& a) const { return hasher(a); }
};

template<typename K, typename V>
using ch_map = tbb::concurrent_hash_map<K, V,
  TBBStdHash<K>, std::allocator<std::pair<K,V>>>;
