#ifndef _DG_ANALYSIS_POINTS_TO_WITH_INVALIDATE_H_
#define _DG_ANALYSIS_POINTS_TO_WITH_INVALIDATE_H_

#include <cassert>

#include "Pointer.h"
#include "PointerSubgraph.h"
#include "PointsToFlowSensitive.h"

namespace dg {
namespace analysis {
namespace pta {

class PointsToWithInvalidate : public PointsToFlowSensitive
{
    static bool canChangeMM(PSNode *n) {
        if (n->predecessorsNum() == 0 || // root node
            n->getType() == PSNodeType::STORE ||
            n->getType() == PSNodeType::MEMCPY ||
            n->getType() == PSNodeType::FREE ||
            n->getType() == PSNodeType::INVALIDATE_LOCALS)
            return true;

        return false;
    }

public:
    using MemoryMapT = PointsToFlowSensitive::MemoryMapT;

    // this is an easy but not very efficient implementation,
    // works for testing
    PointsToWithInvalidate(PointerSubgraph *ps)
    : PointsToFlowSensitive(ps) {}

    bool beforeProcessed(PSNode *n) override
    {
        MemoryMapT *mm = n->getData<MemoryMapT>();
        if (mm)
            return false;

        bool changed = false;

        // on these nodes the memory map can change
        if (canChangeMM(n)) { // root node
            // FIXME: we're leaking the memory maps
            mm = new MemoryMapT();
        } else if (n->predecessorsNum() > 1) {
            // this is a join node, create a new map and
            // merge the predecessors to it
            mm = new MemoryMapT();

            // merge information from predecessors into the new map
            // XXX: this is necessary also with the merge in afterProcess,
            // because this copies the information even for single
            // predecessor, whereas afterProcessed copies the
            // information only for two or more predecessors
            for (PSNode *p : n->getPredecessors()) {
                MemoryMapT *pm = p->getData<MemoryMapT>();
                // merge pm to mm (if pm was already created)
                if (pm) {
                    changed |= mergeMaps(mm, pm, nullptr);
                }
            }
        } else {
            // this node can not change the memory map,
            // so just add a pointer from the predecessor
            // to this map
            PSNode *pred = n->getSinglePredecessor();
            mm = pred->getData<MemoryMapT>();
            assert(mm && "No memory map in the predecessor");
        }

        assert(mm && "Did not create the MM");

        // memory map initialized, set it as data,
        // so that we won't initialize it again
        n->setData<MemoryMapT>(mm);

        // ignore any changes here except when we merged some new information.
        // The other changes we'll detect later
        return changed;
    }

    bool afterProcessed(PSNode *n) override
    {
        bool changed = false;
        PointsToSetT aux_strong_update;
        PointsToSetT *strong_update = nullptr;

        MemoryMapT *mm = n->getData<MemoryMapT>();
        // we must have the memory map, we created it
        // in the beforeProcessed method
        assert(mm && "Do not have memory map");

        // destroy local memory objects if this is the invalidate_locals
        // -- we do not need them anymore
        if (n->getType() == PSNodeType::INVALIDATE_LOCALS) {
            for (auto IT = mm->begin(), ET = mm->end(); IT != ET;) {
                PSNode *key_target = IT->first.target;
                if (key_target->getParent() == n->getParent() &&
                    !key_target->isHeap() && !key_target->isGlobal()) {
                    auto tmp = IT++;
                    aux_strong_update.insert(tmp->first);
                    mm->erase(tmp);
                } else
                    ++IT;
            }

            for (auto& p : n->pointsTo)
                aux_strong_update.insert(p);

            strong_update = &aux_strong_update;
        }

        // every store is a strong update
        // FIXME: memcpy can be strong update too
        if (n->getType() == PSNodeType::STORE)
            strong_update = &n->getOperand(1)->pointsTo;

        if (n->getType() == PSNodeType::FREE)
            // in this points-to we store the pointers for which the
            // memory is invalidated
            strong_update = &n->pointsTo;

        // merge information from predecessors if there's
        // more of them (if there's just one predecessor
        // and this is not a store, the memory map couldn't
        // change, so we don't have to do that)
        if (n->predecessorsNum() > 1 || strong_update
            || n->getType() == PSNodeType::MEMCPY) {
            assert(canChangeMM(n));

            for (PSNode *p : n->getPredecessors()) {
                MemoryMapT *pm = p->getData<MemoryMapT>();
                // merge pm to mm (but only if pm was already created)
                if (pm) {
                    changed |= mergeMaps(mm, pm, strong_update);
                }
            }
        }

        return changed;
    }

    void getMemoryObjectsPointingTo(PSNode *where, const Pointer& pointer,
                          std::vector<MemoryObject *>& objects) override
    {
        MemoryMapT *mm= where->getData<MemoryMapT>();
        assert(mm && "Node does not have memory map");

        for (auto& I : *mm) {
            for (MemoryObject *mo : I.second) {
                for (auto& it : mo->pointsTo) {
                    for (const auto& ptr : it.second) {
                        if (ptr.target == pointer.target) {
                            objects.push_back(mo);
                            break;
                        }
                    }
                }
            }
        }
    }
    
    void getLocalMemoryObjects(PSNode *where, 
                          std::vector<MemoryObject *>& objects) override
    {
        MemoryMapT *mm= where->getData<MemoryMapT>();
        assert(mm && "Node does not have memory map");

        for (auto& I : *mm) {
            for (MemoryObject *mo : I.second) {
                for (auto& it : mo->pointsTo) {
                    for (const auto& ptr : it.second) {
                        if (ptr.isValid() &&
                            !ptr.target->isHeap() &&
                            !ptr.target->isGlobal() &&
                            ptr.target->getParent() == where->getParent()) {
                            objects.push_back(mo);
                            break;
                        }
                    }
                }
            }
        }
    }


protected:

    PointsToWithInvalidate() = default;

};

} // namespace pta
} // namespace analysis
} // namespace dg

#endif // _DG_ANALYSIS_POINTS_TO_WITH_INVALIDATE_H_

