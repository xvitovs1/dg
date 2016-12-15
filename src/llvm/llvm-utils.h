#ifndef _DG_LLVM_UTILS_H_
#define _DG_LLVM_UTILS_H_

#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Instructions.h>

namespace dg {
namespace llvmutils {

using namespace llvm;

/* ----------------------------------------------
 * -- PRINTING
 * ---------------------------------------------- */
inline void print(const Value *val,
                  raw_ostream& os,
                  const char *prefix=nullptr,
                  bool newline = false)
{
    if (prefix)
        os << prefix;

    if (isa<Function>(val))
        os << val->getName().data();
    else
        os << *val;

    if (newline)
        os << "\n";
}

inline void printerr(const char *msg, const Value *val, bool newline = true)
{
    print(val, errs(), msg, newline);
}

/* ----------------------------------------------
 * -- CASTING
 * ---------------------------------------------- */
inline bool isPointerOrIntegerTy(const Type *Ty)
{
    return Ty->isPointerTy() || Ty->isIntegerTy();
}

// can the given function be called by the given call inst?
inline bool callIsCompatible(const Function *F, const CallInst *CI)
{
    using namespace llvm;

    if (F->isVarArg()) {
        if (F->arg_size() > CI->getNumArgOperands())
            return false;
    } else {
        if (F->arg_size() != CI->getNumArgOperands())
            return false;
    }

    if (!F->getReturnType()->canLosslesslyBitCastTo(CI->getType()))
        // it showed up that the loosless bitcast is too strict
        // alternative since we can use the constexpr castings
        if (!(isPointerOrIntegerTy(F->getReturnType()) && isPointerOrIntegerTy(CI->getType())))
            return false;

    int idx = 0;
    for (auto A = F->arg_begin(), E = F->arg_end(); A != E; ++A, ++idx) {
        Type *CTy = CI->getArgOperand(idx)->getType();
        Type *ATy = A->getType();

        if (!(isPointerOrIntegerTy(CTy) && isPointerOrIntegerTy(ATy)))
            if (!CTy->canLosslesslyBitCastTo(ATy))
                return false;
    }

    return true;
}


/* ----------------------------------------------
 * -- VALUE AND INSTRUCTIONS HELPERS
 * ---------------------------------------------- */
template <typename ValueT>
struct ValueInfo {
    const ValueT *value;

    ValueInfo(const ValueT& val) : value(&val) {}
    ValueInfo(const ValueT *val) : value(val) {}

    template <typename T>
    bool isa() const { return llvm::isa<T>(value); }
    template <typename T>
    T* get() const { return llvm::dyn_cast<T>(value); }

    const ValueT operator*() { return *value; }
    const ValueT* operator*() const { return value; }
    const ValueT* operator->() { return value; }
    const ValueT* operator->() const { return value; }
    operator const ValueT*() const { return value; }
    explicit operator bool() const { return value != nullptr; }

    bool isConstantZero() const {
        using namespace llvm;
        if (const ConstantInt *C = dyn_cast<ConstantInt>(val))
            return C->isZero();
        return false;
    }
};

// InstT is either const or non-const llvm::Instuction
template <typename InstT>
struct InstInfo : public ValueInfo<InstT> {
    InstInfo(const InstT& I) : ValueInfo<InstT>(&I) {
        assert(llvm::isa<llvm::Instruction>(&I));
    }

    InstInfo(const InstT *I) : ValueInfo<InstT>(I) {
        assert(llvm::isa<llvm::Instruction>(I));
    }

    operator const llvm::Instruction *() const { return this->value; }

    //ValueInfo getOperand(int idx) { return ValueInfo(instr->getOperand(idx); }
    //ConstValueInfo getOperand(int idx) const { return ConstValueInfo(instr->getOperand(idx); }

    bool isCallTo(const char *name) const
    {
      using namespace llvm;
      if (const CallInst *CI = dyn_cast<CallInst>(this->value)) {
        const Function *F
            = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
        return F && F->getName().equals(name);
      }

      return false;
    }

    template <typename T>
    bool isCallTo(const std::initializer_list<T> &names) const
    {
      using namespace llvm;
      if (const CallInst *CI = dyn_cast<CallInst>(this->value)) {
        const Function *F
            = dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts());
        if (F) {
            for (const auto& name : names) {
                if (F->getName().equals(name))
                    return true;
            }
        }
      }

      return false;
    }
};

struct CallInstInfo : public InstInfo<llvm::CallInst> {
    CallInstInfo(const llvm::CallInst& I) : InstInfo<llvm::CallInst>(&I) {}
    CallInstInfo(const llvm::CallInst *I) : InstInfo<llvm::CallInst>(I) {}

    CallInstInfo(const llvm::Instruction &I)
        : InstInfo<llvm::CallInst>(llvm::cast<llvm::CallInst>(I)) {}
    CallInstInfo(const llvm::Instruction *I)
        : InstInfo<llvm::CallInst>(llvm::cast<llvm::CallInst>(I)) {}

    const Function *getFunction() const {
        return llvm::dyn_cast<llvm::Function>(
            this->value->getCalledValue()->stripPointerCasts()
        );
    }
};

} // namespace llvmutils
} // namespace dg

#endif //_DG_LLVM_UTILS_H_

