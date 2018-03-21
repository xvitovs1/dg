#ifndef _DG_POINTER_GRAPH_VALIDATOR_H_
#define _DG_POINTER_GRAPH_VALIDATOR_H_

#include <string>
#include "analysis/PointsTo/PointerGraph.h"

namespace dg {
namespace analysis {
namespace pta {
namespace debug {


/**
 * Take PointerGraph instance and check
 * whether it is not broken
 */
class PointerGraphValidator {
    const PointerGraph *PS;

    /* These methods return true if the graph is invalid */
    bool checkEdges();
    bool checkNodes();
    bool checkOperands();

    virtual void reportInvalNumberOfOperands(const PSNode *);

protected:
    std::string errors{};

public:
    PointerGraphValidator(const PointerGraph *ps) : PS(ps) {}
    virtual ~PointerGraphValidator() = default;

    bool validate();
    const std::string& getErrors() const { return errors; }
};

} // namespace debug
} // namespace pta
} // namespace analysis
} // namespace dg



#endif // _DG_POINTER_GRAPH_VALIDATOR_H_
