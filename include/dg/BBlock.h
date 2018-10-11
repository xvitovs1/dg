#ifndef _DG_BBLOCK_H_
#define _DG_BBLOCK_H_

#include <cassert>
#include <list>

#include "ADT/DGContainer.h"
#include "analysis/Analysis.h"

#ifndef ENABLE_CFG
#error "BBlock.h needs be included with ENABLE_CFG"
#endif // ENABLE_CFG

namespace dg {

/// ------------------------------------------------------------------
//   BBlockBase
//     Base for Basic blocks -- just a list of nodes and successors
//     and predecessors of the blocks
/// ------------------------------------------------------------------
template <typename NodeT, typename BBlockT>
class BBlockBase {
public:
    struct BBlockEdge {
        using LabelT = uint32_t;

        BBlockEdge(BBlockT* t, LabelT label = 0)
            : target(t), label(label) {}

        BBlockT *target;
        // we'll have just numbers as labels now.
        // We can change it if there's a need
        LabelT label;

        bool operator==(const BBlockEdge& oth) const
        {
            return target == oth.target && label == oth.label;
        }

        bool operator!=(const BBlockEdge& oth) const
        {
            return !operator==(oth);
        }

        bool operator<(const BBlockEdge& oth) const
        {
            return target == oth.target ?
                        label < oth.label : target < oth.target;
        }
    };

    BBlockBase(NodeT *first = nullptr) {
        if (first) {
            append(first);
        }
    }

    using BBlockContainerT = EdgesContainer<BBlock<NodeT>>;
    // we don't need labels with predecessors
    using PredContainerT = EdgesContainer<BBlock<NodeT>>;
    using SuccContainerT = DGContainer<BBlockEdge>;

    SuccContainerT& successors() { return nextBBs; }
    const SuccContainerT& successors() const { return nextBBs; }

    PredContainerT& predecessors() { return prevBBs; }
    const PredContainerT& predecessors() const { return prevBBs; }

    const std::list<NodeT *>& getNodes() const { return nodes; }
    std::list<NodeT *>& getNodes() { return nodes; }

    bool empty() const { return nodes.empty(); }
    size_t size() const { return nodes.size(); }

    void append(NodeT *n)
    {
        assert(n && "Cannot add null node to BBlock");

        n->setBasicBlock(static_cast<BBlockT *>(this));
        nodes.push_back(n);
    }

    void prepend(NodeT *n)
    {
        assert(n && "Cannot add null node to BBlock");

        n->setBasicBlock(static_cast<BBlockT *>(this));
        nodes.push_front(n);
    }

    // return true if all successors point
    // to the same basic block (not considering labels,
    // just the targets)
    bool successorsAreSame() const
    {
        if (nextBBs.size() < 2)
            return true;

        typename SuccContainerT::const_iterator start, iter, end;
        iter = nextBBs.begin();
        end = nextBBs.end();

        BBlock<NodeT> *block = iter->target;
        // iterate over all successor and
        // check if they are all the same
        for (++iter; iter != end; ++iter)
            if (iter->target != block)
                return false;

        return true;
    }

    // remove all edges from/to this BB and reconnect them to
    // other nodes
    void isolate()
    {
        // take every predecessor and reconnect edges from it
        // to successors
        for (BBlock<NodeT> *pred : prevBBs) {
            // find the edge that is going to this node
            // and create new edges to all successors. The new edges
            // will have the same label as the found one
            DGContainer<BBlockEdge> new_edges;
            for (auto I = pred->nextBBs.begin(),E = pred->nextBBs.end(); I != E;) {
                auto cur = I++;
                if (cur->target == this) {
                    // create edges that will go from the predecessor
                    // to every successor of this node
                    for (const BBlockEdge& succ : nextBBs) {
                        // we cannot create an edge to this bblock (we're isolating _this_ bblock),
                        // that would be incorrect. It can occur when we're isolatin a bblock
                        // with self-loop
                        if (succ.target != this)
                            new_edges.insert(BBlockEdge(succ.target, cur->label));
                    }

                    // remove the edge from predecessor
                    pred->nextBBs.erase(*cur);
                }
            }

            // add newly created edges to predecessor
            for (const BBlockEdge& edge : new_edges) {
                assert(edge.target != this
                       && "Adding an edge to a block that is being isolated");
                pred->addSuccessor(edge);
            }
        }

        removeSuccessors();

        // NOTE: nextBBs were cleared in removeSuccessors()
        prevBBs.clear();
    }

    void removeNode(NodeT *n) { nodes.remove(n); }

    size_t successorsNum() const { return nextBBs.size(); }
    size_t predecessorsNum() const { return prevBBs.size(); }

    bool addSuccessor(const BBlockEdge& edge)
    {
        bool ret = nextBBs.insert(edge);
        edge.target->prevBBs.insert(this);

        return ret;
    }

    bool addSuccessor(BBlock<NodeT> *b, uint8_t label = 0)
    {
        return addSuccessor(BBlockEdge(b, label));
    }

    void removeSuccessors()
    {
        // remove references to this node from successors
        for (const BBlockEdge& succ : nextBBs) {
            // This assertion does not hold anymore, since if we have
            // two edges with different labels to the same successor,
            // and we remove the successor, then we remove 'this'
            // from prevBBs twice. If we'll add labels even to predecessors,
            // this assertion must hold again
            // bool ret = succ.target->prevBBs.erase(this);
            // assert(ret && "Did not have this BB in successor's pred");
            succ.target->prevBBs.erase(this);
        }

        nextBBs.clear();
    }

    bool hasSelfLoop()
    {
        return nextBBs.contains(this);
    }

    void removeSuccessor(const BBlockEdge& succ)
    {
        succ.target->prevBBs.erase(this);
        nextBBs.erase(succ);
    }

    unsigned removeSuccessorsTarget(BBlock<NodeT> *target)
    {
        unsigned removed = 0;
        SuccContainerT tmp;
        // approx
        for (auto& edge : nextBBs) {
            if (edge.target != target)
                tmp.insert(edge);
            else
                ++removed;
        }

        nextBBs.swap(tmp);
        return removed;
    }

    void removePredecessors()
    {
        for (BBlock<NodeT> *BB : prevBBs)
            BB->nextBBs.erase(this);

        prevBBs.clear();
    }

    // get first node from bblock
    // or nullptr if the block is empty
    NodeT *getFirstNode() const
    {
        if (nodes.empty())
            return nullptr;

        return nodes.front();
    }

    // get last node from block
    // or nullptr if the block is empty
    NodeT *getLastNode() const
    {
        if (nodes.empty())
            return nullptr;

        return nodes.back();
    }

private:
    // nodes contained in this bblock
    std::list<NodeT *> nodes;

    SuccContainerT nextBBs;
    PredContainerT prevBBs;
};

/// ------------------------------------------------------------------
// - BBlock
//     Basic block structure with additional information
//     related to basic blocks
/// ------------------------------------------------------------------
template <typename NodeT> class BBlock;

template <typename NodeT>
class BBlock : public BBlockBase<NodeT, BBlock<NodeT>> {
public:

    using BBlockContainerT = typename BBlockBase<NodeT, BBlock<NodeT>>::BBlockContainerT;
    using PredContainerT = typename BBlockBase<NodeT, BBlock<NodeT>>::PredContainerT;
    using SuccContainerT = typename BBlockBase<NodeT, BBlock<NodeT>>::SuccContainerT;

    BBlock<NodeT>(NodeT *head = nullptr) : BBlockBase<NodeT, BBlock<NodeT>>(head) {}

    const BBlockContainerT& controlDependence() const { return controlDeps; }
    const BBlockContainerT& revControlDependence() const { return revControlDeps; }

    bool hasControlDependence() const
    {
        return !controlDeps.empty();
    }

    // remove all edges from/to this BB and reconnect them to
    // other nodes
    void isolate()
    {
        BBlockBase<NodeT, BBlock<NodeT>>::isolate();

        // remove reverse edges to this BB
        for (BBlock<NodeT> *B : controlDeps) {
            // we don't want to corrupt the iterator
            // if this block is control dependent on itself.
            // We're gonna clear it anyway
            if (B == this)
                continue;

            B->revControlDeps.erase(this);
        }

        // clear also cd edges that blocks have
        // to this block
        for (BBlock<NodeT> *B : revControlDeps) {
            if (B == this)
                continue;

            B->controlDeps.erase(this);
        }

        revControlDeps.clear();
        controlDeps.clear();
    }

    bool addControlDependence(BBlock<NodeT> *b)
    {
        bool ret;
#ifndef NDEBUG
        bool ret2;
#endif

        ret = controlDeps.insert(b);
#ifndef NDEBUG
        ret2 =
#endif
        b->revControlDeps.insert(this);

        // we either have both edges or none
        assert(ret == ret2);

        return ret;
    }

    // XXX: do this optional?
    BBlockContainerT& getPostDomFrontiers() { return postDomFrontiers; }
    const BBlockContainerT& getPostDomFrontiers() const { return postDomFrontiers; }

    bool addPostDomFrontier(BBlock<NodeT> *BB)
    {
        return postDomFrontiers.insert(BB);
    }

    bool addDomFrontier(BBlock<NodeT> *DF)
    {
        return domFrontiers.insert(DF);
    }

    BBlockContainerT& getDomFrontiers() { return domFrontiers; }
    const BBlockContainerT& getDomFrontiers() const { return domFrontiers; }

    void setIPostDom(BBlock<NodeT> *BB)
    {
        assert(!ipostdom && "Already has the immedate post-dominator");
        ipostdom = BB;
        BB->postDominators.insert(this);
    }

    BBlock<NodeT> *getIPostDom() { return ipostdom; }
    const BBlock<NodeT> *getIPostDom() const { return ipostdom; }

    BBlockContainerT& getPostDominators() { return postDominators; }
    const BBlockContainerT& getPostDominators() const { return postDominators; }

    void setIDom(BBlock<NodeT>* BB)
    {
        assert(!idom && "Already has immediate dominator");
        idom = BB;
        BB->addDominator(this);
    }

    void addDominator(BBlock<NodeT>* BB)
    {
        assert( BB && "need dominator bblock" );
        dominators.insert(BB);
    }

    BBlock<NodeT> *getIDom() { return idom; }
    const BBlock<NodeT> *getIDom() const { return idom; }

    BBlockContainerT& getDominators() { return dominators; }
    const BBlockContainerT& getDominators() const { return dominators; }

    unsigned int getDFSOrder() const
    {
        return analysisAuxData.dfsorder;
    }

    // in order to fasten up interprocedural analyses,
    // we register all the call sites in the BBlock
    unsigned int getCallSitesNum() const
    {
        return callSites.size();
    }

    const std::set<NodeT *>& getCallSites()
    {
        return callSites;
    }

    bool addCallsite(NodeT *n)
    {
        assert(n->getBBlock() == this
               && "Cannot add callsite from different BB");

        return callSites.insert(n).second;
    }

    bool removeCallSite(NodeT *n)
    {
        assert(n->getBBlock() == this
               && "Removing callsite from different BB");

        return callSites.erase(n) != 0;
    }

    void setSlice(uint64_t sid)
    {
        slice_id = sid;
    }

    uint64_t getSlice() const { return slice_id; }

private:
    // when we have basic blocks, we do not need
    // to keep control dependencies in nodes, because
    // all nodes in block has the same control dependence
    BBlockContainerT controlDeps;
    BBlockContainerT revControlDeps;

    // post-dominator frontiers
    BBlockContainerT postDomFrontiers;
    BBlock<NodeT> *ipostdom{nullptr};
    // the post-dominator tree edges
    // (reverse to immediate post-dominator)
    BBlockContainerT postDominators;

    // parent of @this in dominator tree
    BBlock<NodeT> *idom{nullptr};
    // BB.dominators = all children in dominator tree
    BBlockContainerT dominators;
    // dominance frontiers
    BBlockContainerT domFrontiers;

    // is this block in some slice?
    uint64_t slice_id{0};

    // auxiliary data for analyses
    std::set<NodeT *> callSites;

    // auxiliary data for different analyses
    analysis::AnalysesAuxiliaryData analysisAuxData;
    friend class analysis::BBlockAnalysis<NodeT>;
};

} // namespace dg

#endif // _DG_BBLOCK_H_
