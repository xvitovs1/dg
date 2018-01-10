#ifndef _DG_ANALYSIS_POINTS_TO_FLOW_SENSITIVE_WITHOUT_MERGE_H_
#define _DG_ANALYSIS_POINTS_TO_FLOW_SENSITIVE_WITHOUT_MERGE_H_

#include <cassert>

#include "Pointer.h"
#include "PointerSubgraph.h"

namespace dg {
namespace analysis {
namespace pta {

class PointsToFlowSensitiveWithoutMerge : public PointerAnalysis
{
    using MemoryObjectsSetT = std::set<MemoryObject *>;
    using MemoryMapT = std::map<const Pointer, MemoryObjectsSetT>;
    struct Data {
        MemoryMapT* memory_map = nullptr;
        unsigned dfsid = 0;
    };

    std::vector<std::unique_ptr<MemoryObject>> memory_objects;
    std::vector<std::unique_ptr<MemoryMapT>> memory_maps;
    // XXX: what happens when we are building the function using pointers??
    std::vector<Data> info;
    unsigned current_dfs = 0;

    Data& getData(PSNode *n) {
        assert(info.size() > n->getID());
        return info[n->getID()];
    }

    void setMM(PSNode *n, MemoryMapT *mm) {
        getData(n).memory_map = mm;
    }

    PointerSubgraph *PS;

public:
    // this is an easy but not very efficient implementation,
    // works for testing
    PointsToFlowSensitiveWithoutMerge(PointerSubgraph *ps)
    : PointerAnalysis(ps, UNKNOWN_OFFSET, false), PS(ps) {
        // initizlize memory for all nodes
        info.resize(ps->size());
        memory_maps.reserve(128);
    }

    MemoryMapT* getMM(PSNode *n) {
        return getData(n).memory_map;
    }


    static bool canChangeMM(PSNode *n) {
        if (n->predecessorsNum() == 0 || // root node
            n->getType() == PSNodeType::STORE ||
            n->getType() == PSNodeType::MEMCPY)
            return true;

        return false;
    }

    bool beforeProcessed(PSNode *n) override
    {
        // this may happen with function pointer calls after
        // a function is built due to a function pointer call.
        // We must create the missing info structures in this
        // case.
        if (info.size() != PS->size()) {
            info.resize(PS->size());
        }

        MemoryMapT *mm = getMM(n);
        if (mm)
            return false;

        // on these nodes the memory map can change
        if (canChangeMM(n) || n->predecessorsNum() != 1) { // root node or joins
            mm = new MemoryMapT();
            memory_maps.emplace_back(mm);
        } else {
            // this node can not change the memory map,
            // so just add a pointer from the predecessor
            // to this map
            PSNode *pred = n->getSinglePredecessor();
            mm = getMM(pred);
            assert(mm && "No memory map in the predecessor");
        }

        assert(mm && "Did not create the MM");

        // memory map initialized, set it as data,
        // so that we won't initialize it again
        setMM(n, mm);
        return true;
    }

    bool afterProcessed(PSNode *n) override
    {
        bool changed = false;
        PointsToSetT *strong_update = nullptr;

        MemoryMapT *mm = getMM(n);
        // we must have the memory map, we created it
        // in the beforeProcessed method
        assert(mm && "Do not have memory map");

        // every store is a strong update
        // FIXME: memcpy can be strong update too
        if (n->getType() == PSNodeType::STORE)
            strong_update = &n->getOperand(1)->pointsTo;

        // copy information from the predecessor if this is
        // a node that generates an information and it is not
        // a join
        if (canChangeMM(n) && n->predecessorsNum() == 1) {
            for (PSNode *p : n->getPredecessors()) {
                MemoryMapT *pm = getMM(p);
                // merge pm to mm (but only if pm was already created)
                if (pm) {
                    changed |= mergeMaps(mm, pm, strong_update);
                }
            }
        }

        return changed;
    }

    void getMemoryObjects(PSNode *where, const Pointer& pointer,
                          std::vector<MemoryObject *>& objects) override {
        MemoryMapT *mm = getMM(where);
        assert(mm && "Node does not have memory map");

        auto bounds = getObjectRange(mm, pointer);
        // if we do not have any definition of this memory,
        // this is because we haven't merged the states at join.
        // Look up the definitions.
        if (bounds.first == bounds.second) {
            // For STORE we need the same behavior as in regular FS-PTA,
            // since it has the overwrite semantics -- that is do not lookup
            // definition but fall through to creating new memory objects
            if (where->getType() != PSNodeType::STORE)
                lookupDefinitions(where, pointer, objects);
        } else {
            // do we still need a lookup even when we found some definitions?
            bool do_lookup = true;
            bool found_unknown = false;

            for (MemoryMapT::iterator I = bounds.first; I != bounds.second; ++I) {
                assert(I->first.target == pointer.target
                        && "Bug in getObjectRange");

                bool is_unknown = I->first.offset.isUnknown();
                if (pointer.offset.isUnknown()
                    || is_unknown
                    || I->first.offset == pointer.offset) {
                    for (MemoryObject *mo : I->second)
                        objects.push_back(mo);

                    // if we found the definition with correct offset,
                    // we do not need to do further lookup
                    if (!is_unknown) {
                        do_lookup = false;
                    } else
                        found_unknown = true;

                    // do not break, we want to possibly find also definition
                    // on UNKNOWN offset
                }
            }

            // if the pointer has unknown offset, force the lookup.
            // This is an overapprox that may be expensive, FIXME.
            if (do_lookup || pointer.offset.isUnknown()) {
                assert(objects.empty() || found_unknown || pointer.offset.isUnknown());
                lookupDefinitions(where, pointer, objects);
            }

            assert((bounds.second == mm->end() ||
                   bounds.second->first.target != pointer.target)
                   && "Bug in getObjectRange");
        }

        // if we haven't found any memory object, but this psnode
        // is a write to memory, create a new one, so that
        // the write has something to write to
        if (objects.empty() && canChangeMM(where)) {
            MemoryObject *mo = new MemoryObject(pointer.target);
            memory_objects.emplace_back(mo);

            (*mm)[pointer].insert(mo);
            objects.push_back(mo);
        }
    }

protected:

    PointsToFlowSensitiveWithoutMerge() = default;

private:

    static bool comp(const std::pair<const Pointer, MemoryObjectsSetT>& a,
                     const std::pair<const Pointer, MemoryObjectsSetT>& b) {
        return a.first.target < b.first.target;
    }

    ///
    // get interator range for elements that have information
    // about the ptr.target node (ignoring the offsets)
    std::pair<MemoryMapT::iterator, MemoryMapT::iterator>
    getObjectRange(MemoryMapT *mm, const Pointer& ptr) {
        std::pair<const Pointer, MemoryObjectsSetT> what(ptr, MemoryObjectsSetT());
        return std::equal_range(mm->begin(), mm->end(), what, comp);
    }

    ///
    // Merge two Memory maps, return true if any new information was created,
    // otherwise return false
    bool mergeMaps(MemoryMapT *mm, MemoryMapT *pm,
                   PointsToSetT *strong_update) {
        bool changed = false;
        for (auto& it : *pm) {
            const Pointer& ptr = it.first;
            if (strong_update && strong_update->count(ptr))
                continue;

            // use [] to create the object if needed
            MemoryObjectsSetT& S = (*mm)[ptr];
            for (auto& elem : it.second)
                changed |= S.insert(elem).second;
        }

        return changed;
    }

    void lookupDefinitions(PSNode *start, const Pointer& pointer,
                           std::vector<MemoryObject *>& objects) {
        ++current_dfs;
        lookupDefinitionsRec(start, pointer, objects);
    }

    // lookup all reaching definitions of the memory pointed to by the pointer
    void lookupDefinitionsRec(PSNode *start, const Pointer& pointer,
                              std::vector<MemoryObject *>& objects) {
        for (PSNode *pred : start->getPredecessors()) {
            Data& data = getData(pred);
            if (data.dfsid == current_dfs)
                continue;
            else
                data.dfsid = current_dfs;

            MemoryMapT *mm = data.memory_map;
            if (!mm)
                continue;

            // FIXME:what to do with pointer.offset.isUnknown()

            auto it = mm->find(Pointer(pointer.target, UNKNOWN_OFFSET));
            if (it != mm->end()) {
                for (MemoryObject *o : it->second)
                    objects.push_back(o);
                // this may not have been the final definition
                // as the offset is UNKOWN
            }

            it = mm->find(pointer);
            if (it != mm->end()) {
                for (MemoryObject *o : it->second)
                    objects.push_back(o);
                // we're done, we found the definition
                continue;
            }

            // continue searching predecessors
            lookupDefinitionsRec(pred, pointer, objects);
        }
    }
};

} // namespace pta
} // namespace analysis
} // namespace dg

#endif // _DG_ANALYSIS_POINTS_TO_FLOW_SENSITIVE_H_

