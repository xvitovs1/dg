#ifndef _DG_MEMORY_OBJECT_H_
#define _DG_MEMORY_OBJECT_H_

#include <map>
#include <unordered_map>
#include <set>
#include <cassert>

#ifndef NDEBUG
#include <iostream>
#include "dg/analysis/PointsTo/PSNode.h"
#endif // not NDEBUG

#include "PointsToSet.h"

namespace dg {
namespace analysis {
namespace pta {

struct MemoryObject
{
    using PointsToMapT = std::map<Offset, PointsToSetT>;

    MemoryObject(/*uint64_t s = 0, bool isheap = false, */PSNode *n = nullptr)
        : node(n) /*, is_heap(isheap), size(s)*/ {}

    // where was this memory allocated? for debugging
    PSNode *node;
    // possible pointers stored in this memory object
    PointsToMapT pointsTo;

    PointsToSetT& getPointsTo(const Offset off) { return pointsTo[off]; }

    PointsToMapT::iterator find(const Offset off) {
        return pointsTo.find(off);
    }

    PointsToMapT::const_iterator find(const Offset off) const {
        return pointsTo.find(off);
    }

    PointsToMapT::iterator begin() { return pointsTo.begin(); }
    PointsToMapT::iterator end() { return pointsTo.end(); }
    PointsToMapT::const_iterator begin() const { return pointsTo.begin(); }
    PointsToMapT::const_iterator end() const { return pointsTo.end(); }

    bool addPointsTo(const Offset& off, const Pointer& ptr)
    {
        /*
        if (isUnknown())
            return false;
            */

        assert(ptr.target != nullptr
               && "Cannot have NULL target, use unknown instead");

        return pointsTo[off].add(ptr);
    }

    bool addPointsTo(const Offset& off, const PointsToSetT& pointers)
    {
        /*
        if (isUnknown())
            return false;
            */

        bool changed = false;

        for (const Pointer& ptr : pointers)
            changed |= addPointsTo(off, ptr);

        return changed;
    }

#ifndef NDEBUG
    void dump() const {
        std::cout << "MO [" << this << "] for ";
        node->dump(); 
    }

    void dumpv() const {
        dump();
        for (const auto& it : pointsTo) {
            std::cout << "[";
            it.first.dump();
            std::cout << "]";
            for (const auto& ptr : it.second) {
                std::cout << "  -> ";
                ptr.dump();
                std::cout << "\n";
            }
        }
        std::cout << "\n";
    }

    void print() const {
        dump();
        std::cout << "\n";
    }
#endif // not NDEBUG
};

} // namespace pta
} // namespace analysis
} // namespace dg

#endif // _DG_MEMORY_OBJECT_H_
