#ifndef _LLVM_DG_POINTER_GRAPH_VALIDATOR_H_
#define _LLVM_DG_POINTER_GRAPH_VALIDATOR_H_

#include "analysis/PointsTo/PointerGraph.h"
#include "analysis/PointsTo/PointerGraphValidator.h"

namespace dg {
namespace analysis {
namespace pta {
namespace debug {

/**
 * Take PointerGraph instance and check
 * whether it is not broken
 */
class LLVMPointerGraphValidator : public PointerGraphValidator {
public:
    LLVMPointerGraphValidator(PointerGraph *ps)
    : PointerGraphValidator(ps) {}

    ~LLVMPointerGraphValidator() = default;
};

} // namespace debug
} // namespace pta
} // namespace analysis
} // namespace dg



#endif // _LLVM_DG_POINTER_GRAPH_VALIDATOR_H_
