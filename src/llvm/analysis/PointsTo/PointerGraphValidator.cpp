#include <llvm/IR/Value.h>
#include "PointerGraphValidator.h"

namespace dg {
namespace analysis {
namespace pta {
namespace debug {

static const llvm::Value *getValue(PSNode *nd) {
    return nd->getUserData<llvm::Value>();
}

/*
 *void LLVMPointerGraphValidator::reportInvalNumberOfOperands(PSNode *nd) {
 *}
 */


} // namespace debug
} // namespace pta
} // namespace analysis
} // namespace dg

