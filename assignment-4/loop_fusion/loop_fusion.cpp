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

    errs() << "TestPass running on function: " << F.getName() << "\n";
    // Ottieni LoopInfo e DominatorTree
    auto &LI = FAM.getResult<LoopAnalysis>(F);
    auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);

    bool Changed = false;

    // Itera su tutti i loop nella funzione
    SmallVector<Loop *, 8> Worklist;
    errs() << "Iterating over loops in the function...\n";
    for (Loop *TopLevelLoop : LI) {
        errs() << "Top-level loop found.\n";
        for (Loop *L : depth_first(TopLevelLoop)) {
            errs() << "Processing loop...\n";
            // We only handle inner-most loops.
            if (L->isInnermost()) {
                errs() << "Innermost loop added to worklist.\n";
                Worklist.push_back(L);
            }
        }
    }

    errs() << "Checking adjacent loops for fusion...\n";
    for (size_t i = 0; i + 1 < Worklist.size(); ++i) {
        Loop *L0 = Worklist[i];
        Loop *L1 = Worklist[i + 1];

        if (areLoopsAdjacent(L0, L1, DT)) {
            errs() << "Loop " << i << " and Loop " << i + 1 << " are adjacent.\n";
            Changed = true;
        } else {
            errs() << "Loop " << i << " and Loop " << i + 1 << " are NOT adjacent.\n";
        }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
}

bool areLoopsAdjacent(Loop *L0, Loop *L1, DominatorTree &DT) {
    errs() << "Checking if loops are adjacent...\n";
    BasicBlock *L0Exit = nullptr;
    SmallVector<BasicBlock *, 8> ExitBlocks;
    L0->getExitBlocks(ExitBlocks);

    if (ExitBlocks.size() != 1) {
        errs() << "Loop 0 has more than one exit block. Not supported.\n";
        return false; // supportiamo solo loop con un'unica uscita
    }

    L0Exit = ExitBlocks[0];
    errs() << "Loop 0 exit block: " << L0Exit->getName() << "\n";

    BasicBlock *L1Preheader = L1->getLoopPreheader();
    if (!L1Preheader) {
        errs() << "Loop 1 does not have a preheader. Not adjacent.\n";
        return false;
    }

    errs() << "Loop 1 preheader: " << L1Preheader->getName() << "\n";

    // Verifica se l'exit block di L0 è direttamente connesso al preheader di L1
    for (BasicBlock *Succ : successors(L0Exit)) {
        errs() << "Checking successor of Loop 0 exit block: " << Succ->getName() << "\n";
        if (Succ == L1Preheader) {
            errs() << "Loop 0 exit block is connected to Loop 1 preheader. Loops are adjacent.\n";
            return true;
        }
    }

    errs() << "Loop 0 exit block is NOT connected to Loop 1 preheader. Loops are NOT adjacent.\n";
    return false;
}

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
