// Mockup for hpcprof

#include <iostream>
#include <fstream>
#include <string>
#include <cstdint>
#include <stack>
#include <memory>
#include <unordered_set>

#include "tbboverlay.hpp"

using std::string;
#include <lib/analysis/CallPath.hpp>
#include <lib/analysis/Util.hpp>
#include <lib/support/RealPathMgr.hpp>
#include <lib/analysis/ArgsHPCProf.hpp>

namespace {
  struct MergeKey {
    MergeKey() : lmip_real(0), ip(nullptr), ai(lush_assoc_info_NULL) {};
    MergeKey(const Prof::CCT::ADynNode& n) : lmip_real(n.lmIP_real()), ip(n.lip()), ai(n.assocInfo()) {};
    bool operator==(const MergeKey& o) const {
      return lmip_real == o.lmip_real
        && lush_lip_eq(ip, o.ip)
        && lush_assoc_info_eq(ai, o.ai);
    };
  private:
    friend std::hash<MergeKey>;
    VMA lmip_real;
    lush_lip_t* ip;
    lush_assoc_info_t ai;
  };
}

template<>
class std::hash<MergeKey> {
public:
  size_t operator()(const MergeKey& k) const {
    return 0;
  }
};

class Node {
  using child_map = ch_map<MergeKey, Node*>;
  child_map children;
public:
  const Prof::CCT::ANode* source;

  Node() : source(nullptr) {};
  Node(const Prof::CCT::ANode& n) : source(&n) { convert(n); }
  Node(const Prof::CCT::ADynNode& n) : source(&n) { convert(n); }

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
    delete tmp;
  }
  
  void dump(const char* fn) {
    std::ofstream outf(fn);
    outf << "graph {\n";
    int id = 0;
    std::map<const Node*, int> idmap;
    foreach([&](const Node& n){
      idmap[&n] = id;
      outf << "  n" << id << ";\n";// << " [label=\"" << &n << "\"];\n";
      id++;
    });
    foreach([&](const Node& n, const Node* p){
      int nid = idmap[&n];
      int pid = idmap[p];
      outf << "  n" << nid << " -- n" << pid << ";\n";
    });
    outf << "}\n";
  }
  
  bool isSameAs(const Node& o) {
    // Check that our childcounts are the same
    if(children.size() != o.children.size()) return false;
    // For each child, try to find a matching child on the other side
    // Keep a std::set to remember which children have been matched.
    std::unordered_set<const Node*> matched;
    for(const auto&& child: children) {
      bool ok = false;
      for(const auto&& other: o.children)
        if(matched.count(&other) == 0 && child.isSameAs(other)) {
          matched.insert(&other);
          ok = true;
          break;
        }
      if(!ok) return false;
    }
    // Everything seems to have worked...
    return true;
  }

private:
  void foreach(std::function<void(const Node&, const Node*)> f, const Node* p) const {
    f(*this, p);
    for(const auto& child: children) child.second->foreach(f, this);
  }

  void convert(const Prof::CCT::ANode& n, Node*& tmp) {
    using namespace Prof::CCT;
    ANodeChildIterator it(&n);
    for(ANode* cur = nullptr; (cur = it.current()); it++) {
      auto& cdn = dynamic_cast<Prof::CCT::ADynNode&>(*cur);
      auto& x = ensureChildFor(cdn, tmp);
      x->convert(cdn, tmp);
    }
  }

  Node* const& ensureChildFor(Prof::CCT::ADynNode& n, Node*& next) {
    if(next == nullptr) next = new Node;
    next->source = &n;
    child_map::const_accessor ca;
    bool outf = children.insert(ca, {MergeKey(n), next});
    if(outf) next = nullptr;
    return ca->second;
  }
};

struct Args : public Analysis::ArgsHPCProf {
  void parse(int c, const char* const v[]) {
    Analysis::ArgsHPCProf::parse(c, v);
  }
  static const std::string cmd;
  const std::string getCmd() const { return cmd; }
};
const std::string Args::cmd = "hpcprofmock";

void prof_abort(int ec) { exit(ec); }

int main(int argc, char* const* argv) {
  std::cerr << "WARNING: This is a mockup!\n";
  
  Args args;
  args.parse(argc, argv);
  RealPathMgr::singleton().searchPaths(args.searchPathStr());
  auto nArgs = Analysis::Util::normalizeProfileArgs(args.profileFiles);
  
  std::vector<std::pair<std::string, Prof::CallPath::Profile*>> profs;
  for(auto&& fn: *nArgs.paths)
    profs.push_back({fn, Prof::CallPath::Profile::make(fn.c_str(), 0, NULL)});
  
  Node root;
  root.source = profs[0].second->cct()->root();
  #pragma omp parallel for
  for(auto&& pp: profs) root.convert(*pp.second->cct()->root());
  /* root.dump("ours.dot"); */
  
  Prof::CallPath::Profile* merged = profs[0].second;
  for(auto&& pp: profs) if(pp.second != merged)
    merged->merge(*pp.second,
      Prof::CallPath::Profile::Merge_MergeMetricByName,
      Prof::CCT::MrgFlg_NormalizeTraceFileY);
  
  Node root2(*merged->cct()->root());
  /* root2.dump("theirs.dot"); */

  if(root.isSameAs(root2)) std::cerr << "Same!\n";
  else std::cerr << "Different!\n";
  
  return 0;
}
