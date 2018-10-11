/// XXX add licence
//

#ifndef _BBLOCK_H_
#define _BBLOCK_H_

#include <cassert>
#include <list>

#include "dg/BBlock.h"

#ifndef ENABLE_CFG
#error "BBlock.h needs be included with ENABLE_CFG"
#endif // ENABLE_CFG

namespace dg {


/// ------------------------------------------------------------------
// - DGBBlock
//     Basic block structure for dependence graphs
/// ------------------------------------------------------------------
template <typename NodeT>
class DGBBlock : public BBlock<NodeT> {
public:
    using KeyT = typename NodeT::KeyType;
    using DependenceGraphT = typename NodeT::DependenceGraphType;

    DGBBlock(NodeT *head = nullptr, DependenceGraphT *dg = nullptr)
        : BBlock<NodeT>(head), key(KeyT()), dg(dg) {}

    ~DGBBlock() {
        if (delete_nodes_on_destr) {
            for (NodeT *nd : this->getNodes())
                delete nd;
        }
    }

    // similary to nodes, basic blocks can have keys
    // they are not stored anywhere, it is more due to debugging
    void setKey(const KeyT& k) { key = k; }
    const KeyT& getKey() const { return key; }

    // XXX we should do it a common base with node
    // to not duplicate this - something like
    // GraphElement that would contain these attributes
    void setDG(DependenceGraphT *d) { dg = d; }
    DependenceGraphT *getDG() const { return dg; }

    void remove(bool with_nodes = true)
    {
        // do not leave any dangling reference
        this->isolate();

        if (dg) {
#ifndef NDEBUG
            bool ret =
#endif
            dg->removeBlock(key);
            assert(ret && "BUG: block was not in DG");
            if (dg->getEntryBB() == this)
                dg->setEntryBB(nullptr);
        }

        if (with_nodes) {
            for (NodeT *n : this->getNodes()) {
                // we must set basic block to nullptr
                // otherwise the node will try to remove the
                // basic block again if it is of size 1
                n->setBasicBlock(nullptr);

                // remove dependency edges, let be CFG edges
                // as we'll destroy all the nodes
                n->removeCDs();
                n->removeDDs();
                // remove the node from dg
                n->removeFromDG();

                delete n;
            }
        }

        delete this;
    }

    void deleteNodesOnDestruction(bool v = true) {
        delete_nodes_on_destr = v;
    }

private:
    // optional key
    KeyT key;

    // reference to dg if needed
    DependenceGraphT *dg;

    // delete nodes on destruction of the block
    bool delete_nodes_on_destr = false;
};



} // namespace dg

#endif // _BBLOCK_H_
