#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include <set>

using namespace llvm;

//-----------------------------------------------------------------------------
// TestPass implementation
//-----------------------------------------------------------------------------
namespace {

struct TestPass : PassInfoMixin<TestPass> {
  // Main entry point per il nuovo Pass Manager
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
    // Ottieni LoopInfo e DominatorTree
    auto &LI = FAM.getResult<LoopAnalysis>(F);
    auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);

    bool Changed = false;

    // Itera su tutti i loop nella funzione
    for (Loop *L : LI) {
        // Ottieni il preheader del loop
        BasicBlock *Preheader = L->getLoopPreheader();
        if (!Preheader) {
            errs() << "Loop senza preheader, skipping.\n";
            continue;
        }

        // Trova le uscite del loop
        SmallVector<BasicBlock *, 4> ExitBlocks;
        L->getExitBlocks(ExitBlocks);

        std::set<Instruction *> MovedInstructions;

        // Esegui una ricerca depth-first sui blocchi del loop
        for (BasicBlock *BB : L->blocks()) {
            for (Instruction &I : *BB) {
                if (isCandidateForCodeMotion(I, L, ExitBlocks, DT, MovedInstructions)) {
                    I.moveBefore(Preheader->getTerminator());
                    MovedInstructions.insert(&I);
                    errs() << "Moved instruction: " << I << "\n";
                    Changed = true;
                }
            }
        }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  // Funzione per verificare se un'istruzione è loop-invariant
  bool isLoopInvariant(Instruction &I, Loop *L) {
      // Controlla se tutte le dipendenze dell'istruzione sono definite fuori dal loop
      for (Value *Op : I.operands()) {
          if (Instruction *Inst = dyn_cast<Instruction>(Op)) {
              if (L->contains(Inst))
                  return false;
          }
      }

      return true;
  }

  // Funzione per verificare se un blocco domina tutte le uscite del loop
  bool dominatesAllExits(BasicBlock *BB, const SmallVectorImpl<BasicBlock *> &ExitBlocks, DominatorTree &DT) {
      for (BasicBlock *Exit : ExitBlocks) {
          if (!DT.dominates(BB, Exit))
              return false;
      }
      return true;
  }

  bool isReassignedInLoop(Instruction &I, Loop *L) {
      if (StoreInst *Store = dyn_cast<StoreInst>(&I)) {
          Value *Ptr = Store->getPointerOperand();
          for (BasicBlock *BB : L->blocks()) {
              for (Instruction &Inst : *BB) {
                  if (&Inst != &I && isa<StoreInst>(&Inst)) {
                      if (cast<StoreInst>(&Inst)->getPointerOperand() == Ptr)
                          return true;
                  }
              }
          }
      }
      return false;
  }

  bool dominatesAllUses(Instruction &I, Loop *L, DominatorTree &DT) {
      for (User *U : I.users()) {
          if (Instruction *UserInst = dyn_cast<Instruction>(U)) {
              if (L->contains(UserInst->getParent())) {
                  if (!DT.dominates(I.getParent(), UserInst->getParent()))
                      return false;
              }
          }
      }
      return true;
  }

  bool allDependenciesMoved(Instruction &I, const std::set<Instruction *> &MovedInstructions) {
      for (Value *Op : I.operands()) {
          if (Instruction *Inst = dyn_cast<Instruction>(Op)) {
              if (MovedInstructions.find(Inst) == MovedInstructions.end())
                  return false;
          }
      }
      return true;
  }

  bool isCandidateForCodeMotion(Instruction &I, Loop *L, const SmallVectorImpl<BasicBlock *> &ExitBlocks, DominatorTree &DT, const std::set<Instruction *> &MovedInstructions) {
      if (!isLoopInvariant(I, L))
          return false;

      if (!dominatesAllExits(I.getParent(), ExitBlocks, DT))
          return false;

      if (isReassignedInLoop(I, L))
          return false;

      if (!dominatesAllUses(I, L, DT))
          return false;

      if (!allDependenciesMoved(I, MovedInstructions))
          return false;

      return true;
  }

  // Questo pass è richiesto per le funzioni con l'attributo optnone
  static bool isRequired() { return true; }

};


//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "localOpts", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "local-opts") {
                    FPM.addPass(TestPass());
                    return true;
                  }
                  return false;
                });
          }};
}

// Core interface for pass plugins. Enables 'opt' to recognize TestPass.
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getTestPassPluginInfo();
}

} // namespace
