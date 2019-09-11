// Mockup for hpcprof

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <cmath>
#include <limits>
#include <stack>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <omp.h>

#include "stdtbb.hpp"

using std::string;
#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/analysis/ArgsHPCProf.hpp>

namespace {
  struct MergeKey {
    MergeKey() : lmip(0), lmid(0), ip(nullptr), ai(lush_assoc_info_NULL),
      isleaf(false) {};
    MergeKey(const Prof::CCT::ADynNode& n) : lmip(n.lmIP_real()),
      lmid(n.lmId_real()), ip(n.lip()), ai(n.assocInfo()), isleaf(n.isLeaf()) {};
    bool operator==(const MergeKey& o) const {
      return lmid == o.lmid && lmip == o.lmip && isleaf == o.isleaf
        && lush_lip_eq(ip, o.ip)
        && lush_assoc_info_eq(ai, o.ai);
    };
  private:
    friend std::hash<MergeKey>;
    VMA lmip;
    Prof::LoadMap::LMId_t lmid;
    lush_lip_t* ip;
    lush_assoc_info_t ai;
    bool isleaf;
  };
}

template<>
class std::hash<MergeKey> {
  std::hash<VMA> vma;
  std::hash<Prof::LoadMap::LMId_t> lmid;
  std::hash<std::uint32_t> u32;
  std::hash<std::uint64_t> u64;

  static constexpr unsigned int bits = std::numeric_limits<std::size_t>::digits;
  static constexpr unsigned int mask = bits - 1;
  static_assert(0 == (bits & (bits - 1)), "value to rotate must be a power of 2");
  static constexpr std::size_t rotl(std::size_t n, unsigned int c) {
    return (n << (mask & c)) | (n >> (-(mask & c))&mask);
  }
public:
  std::size_t operator()(const MergeKey& k) const {
    std::size_t sponge = 0x9;  // Very simple seed value.
    sponge = rotl(sponge ^ vma(k.lmip), 1);
    sponge = rotl(sponge ^ lmid(k.lmid), 3);
    if(k.ip) {  // Squeeze up all the bits in there
      sponge = rotl(sponge ^ u64(k.ip->data8[0]), 5);
      sponge = rotl(sponge ^ u64(k.ip->data8[1]), 7);
    } else  // Squeeze up a magic number, difference in rotation.
      sponge = rotl(sponge ^ 0x11, 5);
    sponge = rotl(sponge ^ u32((k.ai.u.len << 8) | k.ai.u.as), 11);
    if(k.isleaf)  // Another magic number
      sponge = rotl(sponge ^ 0x5, 13);
    return sponge;
  }
};

template<template<typename K, typename V> class BaseMap>
class Node {
public:
  using child_map = BaseMap<MergeKey, Node*>;
  child_map children;

  template<template<typename K, typename V> class X>
  bool isSameAs(const Node<X>& o, int depth = 0) const {
    // Check that our childcounts are the same
    if(children.size() != o.children.size()) return false;
    // For each child, try to find a matching child on the other side
    // Keep a std::set to remember which children have been matched.
    std::unordered_set<const Node<X>*> matched;
    int idx = 0;
    for(const auto& child: children) {
      bool ok = false;
      for(const auto& other: o.children)
        if(matched.count(other.second) == 0 && child.second->isSameAs(*other.second)) {
          matched.insert(other.second);
          ok = true;
          break;
        }
      idx++;
      if(!ok) return false;
    }
    // Everything seems to have worked...
    return true;
  }

  // Merge operation
  Node& operator+=(const Node& o) {
    for(const auto& oc: o.children) {
      auto x = children.insert(oc);  // Copy MergeKey and Node*
      if(!x.second)  // Had a node, recurse down and merge.
        *x.first->second += *oc.second;
    }
    return *this;
  }

private:
  void convert(const Prof::CCT::ANode& n, Node*& tmp) {
    using namespace Prof::CCT;
    ANodeChildIterator it(&n);
    for(ANode* cur = nullptr; (cur = it.current()); it++) {
      auto& cdn = dynamic_cast<Prof::CCT::ADynNode&>(*cur);
      auto& x = ensureChildFor(MergeKey(cdn), tmp);
      x->convert(cdn, tmp);
    }
  }

  Node* const& ensureChildFor(const MergeKey& k, Node*& next) {
    if(next == nullptr) next = new Node;
    auto x = children.insert({k, next});
    if(x.second) next = nullptr;  // Our node was inserted, don't reuse.
    return x.first->second;  // The final result in the map
  }

public:
  Node() {};
  Node(const Prof::CCT::ANode& n) { convert(n); }
  Node(const Prof::CCT::ADynNode& n) { convert(n); }

  enum class Order { pre, post, };

  void foreach(std::function<void(const Node&)> f, Order o = Order::pre) const {
    if(o == Order::pre) f(*this);
    for(const auto& child: children) child.second->foreach(f, o);
    if(o == Order::post) f(*this);
  }
  void foreach(std::function<void(const Node&, const Node*)> f) const {
    foreach(f, nullptr);
  }

  void convert(const Prof::CCT::ANode& n) {
    Node* tmp = nullptr;
    convert(n, tmp);
    if(tmp) delete tmp;
  }

  Node& operator+=(const Prof::CallPath::Profile* p) {
    convert(*p->cct()->root());
    return *this;
  }

  std::ostream& dump(std::ostream& os, int depth) {
    if(depth == 0) return os;
    os << "Node " << this << " (" << children.size() << " children)\n";
    os << "  Children:";
    bool init = true;
    for(const auto& c: children) {
      os << (init ? " " : ", ") << c.second;
      init = false;
    }
    os << "\n";
    for(const auto& c: children) c.second->dump(os, depth-1);
    return os;
  }

private:
  void foreach(std::function<void(const Node&, const Node*)> f, const Node* p) const {
    f(*this, p);
    for(const auto& child: children) child.second->foreach(f, this);
  }

};

namespace {
  template<typename K, typename V>
  using stdtbb_umap_2 = stdtbb::unordered_map<K, V>;
  template<typename K, typename V>
  using std_umap_2 = std::unordered_map<K, V>;
  template<typename K, typename V>
  struct myhash {
    std::hash<V> real;
    std::size_t operator()(const std::pair<K,V>& v) const noexcept {
      return real(v.second);
    }
  };
  template<typename K, typename V>
  using std_uset_2 = std::unordered_set<std::pair<K, V>, myhash<K, V>>;
}

using ParallelNode = Node<stdtbb_umap_2>;
using LockedNode = Node<stdlock::unordered_map>;
using SerialNode = Node<std_umap_2>;
using CompleteNode = Node<std_uset_2>;

struct Args : public Analysis::ArgsHPCProf {
  void parse(int c, const char* const v[]) {
    Analysis::ArgsHPCProf::parse(c, v);
  }
  static const std::string cmd;
  const std::string getCmd() const { return cmd; }
};
const std::string Args::cmd = "hpcprofmock";

template<bool allowzero>
struct stats {
  int sum, sumsq, cnt;
  int min, max;
  stats() : sum(0), sumsq(0), cnt(0), min(0), max(0) {};
  template<class X>
  stats& operator+=(const X& root) {
    root.foreach([this](const X& n){
      int sz = n.children.size();
      if(sz == 0 && !allowzero) return;
      cnt++;
      sum += sz;
      sumsq += sz * sz;
      if(min == 0 || sz < min) min = sz;
      if(sz > max) max = sz;
    });
    return *this;
  }
  friend std::ostream& operator<<(std::ostream& os, const stats& s) {
    double avg = (double)s.sum / s.cnt;
    double sqavg = (double)s.sumsq / s.cnt;
    os << "min/max/avg/sdev: "
      << s.min << "/" << s.max << "/"
      << avg << "/" << std::sqrt(sqavg - avg*avg);
    return os;
  }
};

template<typename X, typename Y>
static void sanity(const char* name, const X& ours, const Y& orig) {
  stats<true> all;
  stats<false> nozero;
  ours.foreach([&](const X& n){ all += n; nozero += n; });
  std::cerr << name << ":\n"
    << "  Isomorphic to original: " << (ours.isSameAs(orig) ? "OK" : "FAIL") << "\n"
    << "  Branching Factor:      " << all << "\n"
    << "  Branching Factor (>0): " << nozero << "\n";
}

void prof_abort(int ec) { exit(ec); }

#pragma omp declare reduction(+:ParallelNode:omp_out += omp_in)
#pragma omp declare reduction(+:SerialNode:omp_out += omp_in)

int main(int argc, char* const* argv) {
  std::cerr << "WARNING: This is a mockup!\n";

  Args args;
  args.parse(argc, argv);
  RealPathMgr::singleton().searchPaths(args.searchPathStr());
  auto nArgs = Analysis::Util::normalizeProfileArgs(args.profileFiles);

  // Read in all the data
  std::vector<std::pair<std::string, Prof::CallPath::Profile*>> profs;
  for(auto&& fn: *nArgs.paths)
    profs.push_back({fn, Prof::CallPath::Profile::make(fn.c_str(), 0, NULL)});

  // Attempt 1: Pour it all into one big pot.
  ParallelNode root_direct;
  #pragma omp parallel for schedule(dynamic)
  for(std::size_t i = 0; i < profs.size(); i++) root_direct += profs[i].second;

  // Attempt 1c: Try a non-TBB parallel implementation.
  LockedNode root_directlock;
  #pragma omp parallel for schedule(dynamic)
  for(std::size_t i = 0; i < profs.size(); i++) root_directlock += profs[i].second;

  // Attempt 2: Let OpenMP do a reduction with non-parallel versions.
  SerialNode root_omp;
  #pragma omp parallel for schedule(dynamic) reduction(+:root_omp)
  for(std::size_t i = 0; i < profs.size(); i++) root_omp += profs[i].second;

  // Attempt 2b: OpenMP reduction but with TBB-based nodes.
  /* ParallelNode root_omptbb; */
  /* #pragma omp parallel for schedule(dynamic) reduction(+:root_omptbb) */
  /* for(std::size_t i = 0; i < profs.size(); i++) root_omptbb += profs[i].second; */

  // Attempt 3: Use a reduction tree to merge the profiles.
  std::vector<SerialNode> roots_tree(omp_get_max_threads());
  #pragma omp parallel
  {
    int id = omp_get_thread_num();
    int num = omp_get_num_threads();
    SerialNode& myroot = roots_tree[id];
    #pragma omp for schedule(dynamic)
    for(std::size_t i = 0; i < profs.size(); i++) myroot += profs[i].second;
    for(int round = 0; (num >> round) != 0; round++) {
      #pragma omp barrier
      if((id & ((1<<(round+1))-1)) == 0) {  // We participate in this round
        int oid = (id & ~((1<<round)-1)) | (1<<round);
        if(oid < num) {  // Only if our partner exists
          myroot += roots_tree[oid];
        }
      }
    }
  }
  SerialNode& root_tree = roots_tree[0];

  // Attempt 3b: Reduction tree but with TBB-based nodes.
  /* std::vector<ParallelNode> roots_treetbb(omp_get_max_threads()); */
  /* #pragma omp parallel */
  /* { */
  /*   int id = omp_get_thread_num(); */
  /*   int num = omp_get_num_threads(); */
  /*   ParallelNode& myroot = roots_treetbb[id]; */
  /*   #pragma omp for schedule(dynamic) */
  /*   for(std::size_t i = 0; i < profs.size(); i++) myroot += profs[i].second; */
  /*   for(int round = 0; (num >> round) != 0; round++) { */
  /*     #pragma omp barrier */
  /*     if((id & ((1<<(round+1))-1)) == 0) {  // We participate in this round */
  /*       int oid = (id & ~((1<<round)-1)) | (1<<round); */
  /*       if(oid < num) {  // Only if our partner exists */
  /*         myroot += roots_treetbb[oid]; */
  /*       } */
  /*     } */
  /*   } */
  /* } */
  /* ParallelNode& root_treetbb = roots_treetbb[0]; */

  // Now that we're done with the data, construct the original merged tree.
  /* Prof::CallPath::Profile* merged = profs[0].second; */
  /* for(auto&& pp: profs) if(pp.second != merged) */
  /*   merged->merge(*pp.second, */
  /*     Prof::CallPath::Profile::Merge_MergeMetricByName, */
  /*     Prof::CCT::MrgFlg_NormalizeTraceFileY); */
  /* CompleteNode root_orig(*merged->cct()->root()); */

  // Check all the sanities. Templated above because I'm lazy.
  /* sanity("1. Direct Conversion", root_direct, root_orig); */
  /* sanity("1c. Direct Conversion w/ locks", root_directlock, root_orig); */
  /* sanity("2. OpenMP Reduction", root_omp, root_orig); */
  /* sanity("2b. OpenMP Reduction w/ TBB", root_omptbb, root_orig); */
  /* sanity("3. Tree Reduction", root_tree, root_orig); */
  /* sanity("3b. Tree Reduction w/ TBB", root_treetbb, root_orig); */

  return 0;
}
