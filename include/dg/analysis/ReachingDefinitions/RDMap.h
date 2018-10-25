#ifndef _DG_DEF_MAP_H_
#define _DG_DEF_MAP_H_

#include <set>
#include <map>
#include <cassert>

#include "dg/analysis/Offset.h"

namespace dg {
namespace analysis {
namespace rd {

class RDNode;
class ReachingDefinitionsAnalysis;

/// Take two intervals (a, a_len) and (b, b_len) where 'a' ('b', resp.) is the
// start of the interval and 'a_len' ('b_len', resp.) is the length of the
// interval and check whether their are disjunctive.
// The length can be Offset::UNKNOWN for unknown length.
// The start ('a' and 'b') must be concrete numbers.
// \return true iff intervals are disjunctive
//         false iff intervals are not disjunctive
inline bool
intervalsDisjunctive(uint64_t a, uint64_t a_len,
                     uint64_t b, uint64_t b_len)
{
    assert(a != Offset::UNKNOWN && "Start of an interval is unknown");
    assert(b != Offset::UNKNOWN && "Start of an interval is unknown");
    assert(a_len > 0 && "Interval of lenght 0 given");
    assert(b_len > 0 && "Interval of lenght 0 given");

    if (a_len == Offset::UNKNOWN) {
        if (b_len == Offset::UNKNOWN) {
            return false;
        } else {
            // b_len is concrete and a_len is unknown
            // use less or equal, because we are starting
            // from 0 and the bytes are distinct (e.g. 4th byte
            // is on offset 3)
            return (a <= b) ? false : b_len <= a - b;
        }
    } else if (b_len == Offset::UNKNOWN) {
        return (a <= b) ? a_len <= b - a : false;
    }

    // the lenghts and starts are both concrete
    return ((a <= b) ? (a_len <= b - a) : (b_len <= a - b));
}

///
// Take two intervals (a1, a2) and (b1, b2)
// (over non-negative whole numbers) and check
//  whether they overlap (not sharply, i.e.
//  if a2 == b1, then itervals already overlap)
inline bool
intervalsOverlap(uint64_t a1, uint64_t a2,
                 uint64_t b1, uint64_t b2)
{
    return !intervalsDisjunctive(a1, a2, b1, b2);
}

struct DefSite
{
    DefSite(RDNode *t,
            const Offset& o = Offset::UNKNOWN,
            const Offset& l = Offset::UNKNOWN)
        : target(t), offset(o), len(l)
    {
        assert((o.isUnknown() || l.isUnknown() ||
               *o + *l > 0) && "Invalid offset and length given");
    }

    bool operator<(const DefSite& oth) const
    {
        return target == oth.target ?
                (offset == oth.offset ? len < oth.len : offset < oth.offset)
                : target < oth.target;
    }

    bool operator==(const DefSite& oth) const {
        return target == oth.target && offset == oth.offset && len == oth.len;
    }

    // what memory this node defines
    RDNode *target;
    // on what offset
    Offset offset;
    // how many bytes
    Offset len;
};

extern RDNode *UNKNOWN_MEMORY;

// wrapper around std::set<> with few
// improvements that will be handy in our set-up
class RDNodesSet {
    using ContainerTy = std::set<RDNode *>;

    ContainerTy nodes;
    bool is_unknown;

public:
    RDNodesSet() : is_unknown(false) {}

    // the set contains unknown mem. location
    void makeUnknown()
    {
        nodes.clear();
        nodes.insert(UNKNOWN_MEMORY);
        is_unknown = true;
    }

    bool insert(RDNode *n)
    {
        if (is_unknown)
            return false;

        if (n == UNKNOWN_MEMORY) {
            makeUnknown();
            return true;
        } else
            return nodes.insert(n).second;
    }

    size_t count(RDNode *n) const
    {
        return nodes.count(n);
    }

    size_t size() const
    {
        return nodes.size();
    }

    void clear()
    {
        nodes.clear();
        is_unknown = false;
    }

    bool isUnknown() const
    {
        return is_unknown;
    }

    ContainerTy::iterator begin() { return nodes.begin(); }
    ContainerTy::iterator end() { return nodes.end(); }
    ContainerTy::const_iterator begin() const { return nodes.begin(); }
    ContainerTy::const_iterator end() const { return nodes.end(); }

    ContainerTy& getNodes() {
        return nodes;
    };

};

using DefSiteSetT = std::set<DefSite>;

class BasicRDMap
{
public:
    using MapT = std::map<DefSite, RDNodesSet>;
    using iterator = MapT::iterator;
    using const_iterator = MapT::const_iterator;

    BasicRDMap() {}
    BasicRDMap(const BasicRDMap& o);

    bool merge(const BasicRDMap *o,
               DefSiteSetT *without = nullptr,
               bool strong_update_unknown = true,
               Offset::type max_set_size  = Offset::UNKNOWN,
               bool merge_unknown     = false);
    bool add(const DefSite&, RDNode *n);
    bool update(const DefSite&, RDNode *n);
    bool empty() const { return defs.empty(); }

    // @return iterators for the range of pointers that has the same object
    // as the given def site
    std::pair<BasicRDMap::iterator, BasicRDMap::iterator>
    getObjectRange(const DefSite&);

    std::pair<BasicRDMap::iterator, BasicRDMap::iterator>
    getObjectRange(RDNode *);

    bool defines(const DefSite& ds) { return defs.count(ds) != 0; }
    bool definesWithAnyOffset(const DefSite& ds);

    iterator begin() { return defs.begin(); }
    iterator end() { return defs.end(); }
    const_iterator begin() const { return defs.begin(); }
    const_iterator end() const { return defs.end(); }

    RDNodesSet& get(const DefSite& ds){ return defs[ds]; }
    //const RDNodesSetT& get(const DefSite& ds) const { return defs[ds]; }
    RDNodesSet& operator[](const DefSite& ds) { return defs[ds]; }

    //RDNodesSet& get(RDNode *, const Offset&);
    // gather reaching definitions of memory [n + off, n + off + len]
    // and store them to the @ret
    size_t get(RDNode *n, const Offset& off,
               const Offset& len, std::set<RDNode *>& ret);
    size_t get(DefSite& ds, std::set<RDNode *>& ret);

    const MapT& getDefs() const { return defs; }

private:
     MapT defs;
};

using RDMap = BasicRDMap;

} // rd
} // analysis
} // dg

#endif
