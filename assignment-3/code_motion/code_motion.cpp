#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/LoopInfo.h"
// #include "llvm/Analysis/LoopInfoWrapperPass.h"
#include "llvm/IR/Dominators.h"


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

        // Esegui una ricerca depth-first sui blocchi del loop
        for (BasicBlock *BB : L->blocks()) {
            for (Instruction &I : *BB) {
                // Controlla se l'istruzione è loop-invariant
                if (isLoopInvariant(I, L)) {
                    // Controlla se il blocco domina tutte le uscite del loop
                    if (dominatesAllExits(BB, ExitBlocks, DT)) {
                        // Sposta l'istruzione nel preheader
                        I.moveBefore(Preheader->getTerminator());
                        errs() << "Moved instruction: " << I << "\n";
                        Changed = true;
                    }
                }
            }
        }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  // Funzione per verificare se un'istruzione è loop-invariant
  bool isLoopInvariant(Instruction &I, Loop *L) {
      // // Controlla se l'istruzione è sicura da eseguire speculativamente
      // if (!I.isSafeToSpeculativelyExecute())
      //     return false;

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

  
  };

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
