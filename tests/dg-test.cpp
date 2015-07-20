#include <assert.h>
#include <cstdarg>
#include <cstdio>

#include "test-runner.h"

#include "../src/DependenceGraph.h"
#include "../src/EdgesContainer.h"

namespace dg {
namespace tests {

class TestDG;
class TestNode;

class TestNode : public Node<TestDG, int, TestNode *>
{
public:
    TestNode(int k)
        : Node<TestDG, int, TestNode *>(k) {}
};

class TestDG : public DependenceGraph<int, TestNode *>
{
public:
#ifdef ENABLE_CFG
    typedef BBlock<TestNode *> BasicBlock;
#endif // ENABLE_CFG

    bool addNode(TestNode *n)
    {
        return DependenceGraph<int, TestNode *>
                ::addNode(n->getKey(), n);
    }
};

class TestConstructors : public Test
{
    TestConstructors() : Test("constructors test") {}

    void test()
    {
        TestDG d;

        check(d.getEntry() == nullptr, "BUG: garbage in entry");
        check(d.size() == 0, "BUG: garbage in nodes_num");

        //TestNode n;
        TestNode n(8);

        check(!n.hasSubgraphs(), "BUG: garbage in subgraph");
        check(n.subgraphsNum() == 0, "BUG: garbage in subgraph");
        check(n.getParameters() == nullptr, "BUG: garbage in parameters");
    }
};

class TestAdd : public Test
{
public:
    TestAdd() : Test("edges adding test")
    {}

    void test()
    {
        TestDG d;
        //TestNode n1, n2;
        TestNode n1(1);
        TestNode n2(2);

        check(n1.addControlDependence(&n2), "adding C edge claims it is there");
        check(n2.addDataDependence(&n1), "adding D edge claims it is there");

        d.addNode(&n1);
        d.addNode(&n2);

        check(d.find(100) == d.end(), "found unknown node");
        check(d.find(1) != d.end(), "didn't find node, find bug");
        check(d.find(2) != d.end(), "didn't find node, find bug");
        check(d.find(3) == d.end(), "found unknown node");

        check(d.getNode(3) == nullptr, "getNode bug");
        check(d.getNode(1) == &n1, "didn't get node that is in graph");

        d.setEntry(&n1);
        check(d.getEntry() == &n1, "BUG: Entry setter");

        int n = 0;
        for (auto I = d.begin(), E = d.end(); I != E; ++I) {
            ++n;
            check((*I).second == &n1 || (*I).second == &n2,
                    "Got some garbage in nodes");
        }

        check(n == 2, "BUG: adding nodes to graph, got %d instead of 2", n);

        int nn = 0;
        for (auto ni = n1.control_begin(), ne = n1.control_end();
             ni != ne; ++ni){
            check(*ni == &n2, "got wrong control edge");
            ++nn;
        }

        check(nn == 1, "bug: adding control edges, has %d instead of 1", nn);

        nn = 0;
        for (auto ni = n2.data_begin(), ne = n2.data_end();
             ni != ne; ++ni) {
            check(*ni == &n1, "got wrong control edge");
            ++nn;
        }

        check(nn == 1, "BUG: adding dep edges, has %d instead of 1", nn);
        check(d.size() == 2, "BUG: wrong nodes num");

        // adding the same node should not increase number of nodes
        check(!d.addNode(&n1), "should get false when adding same node");
        check(d.size() == 2, "BUG: wrong nodes num (2)");
        check(!d.addNode(&n2), "should get false when adding same node (2)");
        check(d.size() == 2, "BUG: wrong nodes num (2)");

        // don't trust just the counter
        n = 0;
        for (auto I = d.begin(), E = d.end(); I != E; ++I)
            ++n;

        check(n == 2, "BUG: wrong number of nodes in graph", n);

        // we're not a multi-graph, each edge is there only once
        // try add multiple edges
        check(!n1.addControlDependence(&n2),
             "adding multiple C edge claims it is not there");
        check(!n2.addDataDependence(&n1),
             "adding multiple D edge claims it is not there");

        nn = 0;
        for (auto ni = n1.control_begin(), ne = n1.control_end(); ni != ne; ++ni){
            check(*ni == &n2, "got wrong control edge (2)");
            ++nn;
        }

        check(nn == 1, "bug: adding control edges, has %d instead of 1 (2)", nn);

        nn = 0;
        for (auto ni = n2.data_begin(), ne = n2.data_end();
             ni != ne; ++ni) {
            check(*ni == &n1, "got wrong control edge (2) ");
            ++nn;
        }

        check(nn == 1,
                "bug: adding dependence edges, has %d instead of 1 (2)", nn);

        TestNode *&rn = d.getRef(3); // getRef creates node if not present
        check(d.getNode(3) == rn, "get ref did not create node");
    }
};

class TestContainer : public Test
{
public:
    TestContainer() : Test("container test")
    {}

    void test()
    {
#if ENABLE_CFG
        TestNode n1(1);
        TestNode n2(2);

        EdgesContainer<TestNode *> IT;
        EdgesContainer<TestNode *> IT2;

        check(IT == IT2, "empty containers does not equal");
        check(IT.insert(&n1), "returned false with new element");
        check(IT.size() == 1, "size() bug");
        check(IT2.size() == 0, "size() bug");
        check(IT != IT2, "different containers equal");
        check(IT2.insert(&n1), "returned false with new element");
        check(IT == IT2, "containers with same content does not equal");

        check(!IT.insert(&n1), "double inserted element");
        check(IT.insert(&n2), "unique element wrong retval");
        check(IT2.insert(&n2), "unique element wrong retval");

        check(IT == IT2, "containers with same content does not equal");
#endif
    }
};


class TestCFG : public Test
{
public:
    TestCFG() : Test("CFG edges test")
    {}

    void test()
    {
#if ENABLE_CFG

        TestDG d;
        TestNode n1(1);
        TestNode n2(2);

        d.addNode(&n1);
        d.addNode(&n2);

        check(!n1.hasSuccessor(),
                "hasSuccessor returned true on node without successor");
        check(!n2.hasSuccessor(),
                "hasSuccessor returned true on node without successor");
        check(!n1.hasPredcessor(),
                "hasPredcessor returned true on node without successor");
        check(!n2.hasPredcessor(),
                "hasPredcessor returned true on node without successor");
        check(n1.getSuccessor() == nullptr, "succ initialized with garbage");
        check(n2.getSuccessor() == nullptr, "succ initialized with garbage");
        check(n1.getPredcessor() == nullptr, "pred initialized with garbage");
        check(n2.getPredcessor() == nullptr, "pred initialized with garbage");

        check(n1.setSuccessor(&n2) == nullptr,
                "adding successor edge claims it is there");
        check(n1.hasSuccessor(), "hasSuccessor returned false");
        check(!n1.hasPredcessor(), "hasPredcessor returned true");
        check(n2.hasPredcessor(), "hasPredcessor returned false");
        check(!n2.hasSuccessor(), "hasSuccessor returned false");
        check(n1.getSuccessor() == &n2, "get/addSuccessor bug");
        check(n2.getPredcessor() == &n1, "get/addPredcessor bug");

        // basic blocks
        TestDG::BasicBlock BB(&n1);
        check(BB.getFirstNode() == &n1, "first node incorrectly set");
        check(BB.setLastNode(&n2) == nullptr, "garbage in lastNode");
        check(BB.getLastNode() == &n2, "bug in setLastNode");

        check(BB.successorsNum() == 0, "claims: %u", BB.successorsNum());
        check(BB.predcessorsNum() == 0, "claims: %u", BB.predcessorsNum());

        TestNode n3(3);
        TestNode n4(4);
        d.addNode(&n3);
        d.addNode(&n4);

        TestDG::BasicBlock BB2(&n3), BB3(&n3);

        check(BB.addSuccessor(&BB2), "the edge is there");
        check(!BB.addSuccessor(&BB2), "added even when the edge is there");
        check(BB.addSuccessor(&BB3), "the edge is there");
        check(BB.successorsNum() == 2, "claims: %u", BB.successorsNum());

        check(BB2.predcessorsNum() == 1, "claims: %u", BB2.predcessorsNum());
        check(BB3.predcessorsNum() == 1, "claims: %u", BB3.predcessorsNum());
        check(*(BB2.predcessors().begin()) == &BB, "wrong predcessor set");
        check(*(BB3.predcessors().begin()) == &BB, "wrong predcessor set");

        for (auto s : BB.successors())
            check(s == &BB2 || s == &BB3, "Wrong succ set");

        BB2.removePredcessors();
        check(BB.successorsNum() == 1, "claims: %u", BB.successorsNum());
        check(BB2.predcessorsNum() == 0, "has successors after removing");

        BB.removeSuccessors();
        check(BB.successorsNum() == 0, "has successors after removing");
        check(BB2.predcessorsNum() == 0, "removeSuccessors did not removed BB"
                                        " from predcessor");
        check(BB3.predcessorsNum() == 0, "removeSuccessors did not removed BB"
                                    " from predcessor");
#endif // ENABLE_CFG
    }
};

class TestRemove : public Test
{
public:
    TestRemove() : Test("edges removing test")
    {}

    void test()
    {
        nodes_remove_edge_test();
        nodes_isolate_test();
        nodes_remove_test();
        bb_isolate_test();
    }

private:
    TestNode **create_full_graph(TestDG& d, int n)
    {
        TestNode **nodes = new TestNode *[n];

        for (int i = 0; i < n; ++i) {
            nodes[i] = new TestNode(i);
            d.addNode(nodes[i]);
        }

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j)
                    continue;

                nodes[i]->addDataDependence(nodes[j]);
                nodes[i]->addControlDependence(nodes[j]);
            }
        }

        assert(d.size() == n && "Bug in create_full_graph");
        return nodes;
    }

    void nodes_remove_edge_test()
    {
        TestDG d;
        TestNode n1(1);
        TestNode n2(2);
        d.addNode(&n1);
        d.addNode(&n2);

        check(n1.removeDataDependence(&n1) == false, "remove non-existing dep");
        check(n2.removeDataDependence(&n1) == false, "remove non-existing dep");

        n1.addDataDependence(&n2);
        n2.addControlDependence(&n1);
        check(n2.removeDataDependence(&n1) == false, "remove non-existing dep");
        check(n1.removeDataDependence(&n2) == true, "remove existing dep");
        check(n1.getDataDependenciesNum() == 0, "remove bug");
        check(n2.getDataDependenciesNum() == 0, "remove bug");
        check(n2.getControlDependenciesNum() == 1, "add or size method bug");
        check(n1.getRevControlDependenciesNum() == 1, "add or size method bug");
        check(n2.getDataDependenciesNum() == 0, "remove bug");
    }

    #define NODES_NUM 10
    void nodes_isolate_test()
    {
        TestDG d;
        TestNode **nodes = create_full_graph(d, NODES_NUM);

        // create CFG edges between nodes
        for (int i = 0; i < NODES_NUM - 1; ++i) {
            nodes[i]->setSuccessor(nodes[i + 1]);
        }

        nodes[0]->isolate();
        check(nodes[0]->getControlDependenciesNum() == 0, "isolate bug");
        check(nodes[0]->getDataDependenciesNum() == 0, "isolate bug");
        check(nodes[0]->getRevControlDependenciesNum() == 0, "isolate bug");
        check(nodes[0]->getRevDataDependenciesNum() == 0, "isolate bug");
        check(nodes[0]->hasSuccessor() == false, "isolate should remove successor");
        check(nodes[0]->hasPredcessor() == false, "isolate should remove successor");
        check(nodes[1]->hasPredcessor() == false, "isolate should remove successor");
        check(nodes[1]->getSuccessor() == nodes[2], "setSuccessor bug");

        nodes[5]->isolate();
        check(nodes[5]->hasSuccessor() == false, "isolate should remove successor");
        check(nodes[5]->hasPredcessor() == false, "isolate should remove successor");
        check(nodes[4]->getSuccessor() == nodes[6], "isolate should reconnect neighb.");
        check(nodes[6]->getPredcessor() == nodes[4], "isolate should reconnect neighb.");

        nodes[NODES_NUM - 1]->isolate();
        check(nodes[NODES_NUM - 1]->hasSuccessor() == false, "isolate should remove successor");
        check(nodes[NODES_NUM - 1]->hasPredcessor() == false, "isolate should remove successor");
        check(nodes[NODES_NUM - 2]->hasSuccessor() == false, "isolate should remove successor");
    }

    void nodes_remove_test()
    {
        TestDG d;
        TestNode **nodes = create_full_graph(d, NODES_NUM);

        TestNode *n = d.removeNode(5);
        check(n == nodes[5], "remove node didn't find node, returned %p - '%d'",
              n, n ? n->getKey() : -1);
        check(d.removeNode(NODES_NUM + 100) == nullptr, "remove weird unknown node");
        check(d.removeNode(5) == nullptr, "remove unknown node");
        check(d.deleteNode(5) == false, "delete unknown node");
        check(d.deleteNode(0) == true, "delete unknown node");

        check(d.size() == NODES_NUM - 2, "should have %d but have %d size",
              NODES_NUM - 2, d.size());

        for (int i = 1; i < NODES_NUM; ++i) {
            if (i != 5) {
                check(nodes[i]->getDataDependenciesNum() == NODES_NUM - 3,
                      "node[%d]: should have %u but have %u",
                      i, NODES_NUM - 3, nodes[i]->getDataDependenciesNum());
                check(nodes[i]->getControlDependenciesNum() == NODES_NUM - 3,
                      "node[%d]: should have %u but have %u",
                      i, NODES_NUM - 3, nodes[i]->getControlDependenciesNum());
                check(nodes[i]->getRevDataDependenciesNum() == NODES_NUM - 3,
                      "node[%d]: should have %u but have %u",
                      i, NODES_NUM - 3, nodes[i]->getRevDataDependenciesNum());
                check(nodes[i]->getRevControlDependenciesNum() == NODES_NUM - 3,
                      "node[%d]: should have %u but have %u",
                      i, NODES_NUM - 3, nodes[i]->getRevControlDependenciesNum());
            }
        }
    }

    void bb_isolate_test()
    {
        TestDG d;
        TestNode **nodes = create_full_graph(d, 15);

        // create first basic block that will contain first 5 nodes
        for (int i = 0; i < 5; ++i) {
            nodes[i]->setSuccessor(nodes[i + 1]);
        }

        TestDG::BasicBlock B1(nodes[0], nodes[5]);

        // another basic block of size 5
        for (int i = 6; i < 9; ++i) {
            nodes[i]->setSuccessor(nodes[i + 1]);
        }
        TestDG::BasicBlock B2(nodes[6], nodes[9]);

        // BBs of size 1
        TestDG::BasicBlock B3(nodes[10], nodes[10]);
        TestDG::BasicBlock B4(nodes[11], nodes[11]);

        for (int i = 12; i < 14; ++i) {
            nodes[i]->setSuccessor(nodes[i + 1]);
        }

        // and size 2
        TestDG::BasicBlock B5(nodes[12], nodes[14]);;

        B1.addSuccessor(&B2);
        B1.addSuccessor(&B3);
        B2.addSuccessor(&B3);
        B2.addSuccessor(&B4);
        B3.addSuccessor(&B4);
        B3.addSuccessor(&B5);
        B5.addPredcessor(&B3);
        B5.addPredcessor(&B4);

        B5.isolate();
        check(B5.successorsNum() == 0, "has succs after isolate");
        check(B5.predcessorsNum() == 0, "has preds after isolate");
        check(B3.successors().contains(&B5) == false, " dangling reference");
    }
};

}; // namespace tests
}; // namespace dg

int main(int argc, char *argv[])
{
    using namespace dg::tests;
    TestRunner Runner;

    Runner.add(new TestCFG());
    Runner.add(new TestContainer());
    Runner.add(new TestAdd());
    Runner.add(new TestRemove());

    return Runner();
}
