#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/DependenceAnalysis.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
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
    auto &PDT = FAM.getResult<PostDominatorTreeAnalysis>(F);
    auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
    auto &DI = FAM.getResult<DependenceAnalysis>(F);

    bool Changed = false;

    // Itera su tutti i loop nella funzione
    SmallVector<Loop *, 8> Worklist;
    errs() << "Iterating over loops in the function...\n";
    for (Loop *TopLevelLoop : LI) {
        for (Loop *L : depth_first(TopLevelLoop)) {
            if (L->isInnermost()) {
                Worklist.push_back(L);
                }
            }
    }

    // Ordina i loop secondo l'ordine di dominanza
    llvm::sort(Worklist, [&](Loop *A, Loop *B) {
        return DT.dominates(A->getHeader(), B->getHeader());
    });

    errs() << "Checking adjacent loops for fusion...\n";
    for (size_t i = 0; i + 1 < Worklist.size(); ++i) {
        Loop *L0 = Worklist[i];
        Loop *L1 = Worklist[i + 1];

        if( areLoopsAdjacent(L0,L1,DT) && 
            areControlFlowEquivalent(L0, L1, DT, PDT) && 
            equalTripCount(L0, L1, SE) &&
            controlDependencies(L0, L1, DI) ) 
            {
                //loop fusion logic 
                errs() << "Loop " << i << " and Loop " << i + 1 << " can be fused.\n";
                
                bool fused = fuseLoops(L0, L1, LI);
                
                if (fused) {
                    Changed = true;
                    EliminateUnreachableBlocks(F);
                    errs() << "Successfully fused loops " << i << " and " << i + 1 << ".\n";
                } else {
                    errs() << "Failed to fuse loops " << i << " and " << i + 1 << ".\n";
                }
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
    // errs() << "Loop 0 exit block: " << L0Exit->getName() << "\n";

    BasicBlock *L1Preheader = L1->getLoopPreheader();
    if (!L1Preheader) {
        errs() << "Loop 1 does not have a preheader. Not adjacent.\n";
        return false;
    }

    errs() << "Loop 0 exit block:\n";
    L0Exit->print(errs());
    errs() << "\n";

    errs() << "Loop 1 preheader:\n";
    L1Preheader->print(errs());
    errs() << "\n";

    if (L0Exit == L1Preheader) {
        errs() << "L0Exit and L1Preheader are the SAME block! Loops are adjacent.\n";
        return true;
    }

    // errs() << "Loop 1 preheader: " << L1Preheader->getName() << "\n";

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

bool areControlFlowEquivalent(Loop *L0, Loop *L1, DominatorTree &DT, PostDominatorTree &PDT) {
    BasicBlock *H0 = L0->getHeader();
    BasicBlock *H1 = L1->getHeader();

    // Condizione dalla slide
    if (DT.dominates(H0, H1) && PDT.dominates(H1, H0)) {
        errs() << "Loops are control flow equivalent.\n";
        return true;
    }

    errs() << "Loops are NOT control flow equivalent.\n";
    return false;
}

bool equalTripCount(Loop *L0, Loop *L1, ScalarEvolution &SE) {
    // Controlla se i loop hanno lo stesso trip count
    auto TC0 = SE.getSmallConstantTripCount(L0);
    auto TC1 = SE.getSmallConstantTripCount(L1);

    // const SCEV *TripCount = SE.getBackedgeTakenCount(L0);
    // errs() << "TripCount SCEV: " << *TripCount << "\n";

    errs() << "Trip count for Loop 0: " << (TC0) << "\n";
    errs() << "Trip count for Loop 1: " << (TC1) << "\n";

    if (TC0 && TC1) {
        return true;
    }
    return false;
}

bool controlDependencies(Loop *L0, Loop *L1, DependenceInfo &DI) {
    // Controlla le dipendenze tra tutte le istruzioni dei due loop
    for (BasicBlock *BB0 : L0->blocks()) {
        for (Instruction &I0 : *BB0) {
            for (BasicBlock *BB1 : L1->blocks()) {
                for (Instruction &I1 : *BB1) {
                    if (auto D = DI.depends(&I0, &I1, true)) {
                        if (D->isConfused() || D->isOrdered()) {
                            errs() << "Found problematic dependency:\n";
                            I0.print(errs());
                            errs() << "\n  and\n";
                            I1.print(errs());
                            errs() << "\n";
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

BasicBlock *resolveEffectivePreheader(BasicBlock *Exit, BasicBlock *L1Header) {
    // Se Exit ha un solo successore
    if (Exit->getTerminator()->getNumSuccessors() == 1) {
        BasicBlock *Succ = Exit->getTerminator()->getSuccessor(0);
        // Se anche Succ ha un solo successore, ed è L1Header
        if (Succ->getTerminator()->getNumSuccessors() == 1 &&
            Succ->getTerminator()->getSuccessor(0) == L1Header) {
            return Succ;
        }
    }
    return Exit; // altrimenti, l'uscita è già il preheader
}

BasicBlock *getEntryBlock(Loop *L) {
        if (!L) return nullptr;
        BasicBlock *PreHeader = L->getLoopPreheader();
        if (!PreHeader) return nullptr;
        if(!L->isGuarded()) {
            return PreHeader;
        } 
        return PreHeader->getUniqueSuccessor();
}

BasicBlock *getBody(Loop *L) {
    return (dyn_cast<BranchInst>(L->getHeader()->getTerminator()))->getSuccessor(0);
}

bool fuseLoops(Loop *L0, Loop *L1, LoopInfo &LI) {
     // Get the body of L1 to be inserted in L0
        BasicBlock* Body1 = getBody(L1);

        // Exit block of L1 is the entry block of L0
        BasicBlock* Exit0 = getEntryBlock(L0);
        BasicBlock* Latch0 = L0->getLoopLatch();
        BasicBlock* Header0 = L0->getHeader();

        BasicBlock* Exit1 = L1->getExitBlock();
        BasicBlock* Latch1 = L1->getLoopLatch();
        BasicBlock* Header1 = L1->getHeader();

    if (!Header0 || !Header1 || !Latch0 || !Latch1) {
        errs() << "One of the key blocks is missing\n";
        return false;
    }

    // 1. Modificare gli usi della induction variable nel body del
    // loop 1 con quelli della induction variable del loop 0
    // Trova le IV (phi node) nei due header
    PHINode *IV0 = dyn_cast<PHINode>(&Header0->front());
    PHINode *IV1 = dyn_cast<PHINode>(&Header1->front());
    if (!IV0 || !IV1) {
        errs() << "Failed to find induction variables\n";
        return false;
    }
    // Rimpiazza tutti gli usi dell'induction variable di L1 con quella di L0
    IV1->replaceAllUsesWith(IV0);

    //Link the terminator of the header of L0 to the body of L1
    Header0->getTerminator()->replaceUsesOfWith(Exit0, Exit1);

    // Predecessor blocks of Latch0 must now have Body1 as successor
    for (BasicBlock *Pred : predecessors(Latch0)) {
        // Replace the successor of the predecessor with Body1
        Pred->getTerminator()->replaceUsesOfWith(Latch0, Body1);
    }

    // I blocchi predecessori di Latch1 devono ora avere Latch0 come successore
    for (BasicBlock *Pred : predecessors(Latch1)) {
        // Replace the successor of the predecessor with Latch0
        Pred->getTerminator()->replaceUsesOfWith(Latch1, Latch0);
    }

    //aggiungi i blocchi di L1 al loop di L0
    for (BasicBlock *BB : L1->blocks()) {
        if (BB != Header1 && BB != Latch1) {
            L0->addBasicBlockToLoop(BB, LI);
        }
    }

    LI.erase(L1); // Elimina il loop L1 dalla LoopInfo
    
    // Controlla che L1 non ci sia più nella LoopInfo
    for (auto &L : LI) {
        if (L == L1) {
            errs() << "ERROR: Loop L1 still found in LoopInfo!\n";
            break;
        }
    }
    errs() << "Confirmed: Loop L1 removed from LoopInfo.\n";

    return true;
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
