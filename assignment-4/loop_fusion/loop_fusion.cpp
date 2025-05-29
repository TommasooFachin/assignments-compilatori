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

        if(!isLoopFusionCandidate(L0) || !isLoopFusionCandidate(L1)) 
            continue;

        if( areLoopsAdjacent(L0,L1,DT,LI) && 
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

bool isLoopFusionCandidate(Loop* L){
        // Check if the loop has a preheader, header, latch, exiting block and exit block
        if (!L->getLoopPreheader() || !L->getHeader() || !L->getLoopLatch() || !L->getExitingBlock() || !L->getExitBlock()) {
            outs()<<"Loop is not a candidate for fusion\n";
            return false;
        } 

        // Check if the loop is in a simplified form
        if (!L->isLoopSimplifyForm()) {
            outs()<<"Loop is not in a simplified form\n";
            return false;
        }
        return true;
}

bool areLoopsAdjacent(Loop *L0, Loop *L1, DominatorTree &DT, LoopInfo &LI) {
    errs() << "Checking if loops are adjacent...\n";

    errs() << "L0 is guarded: " << L0->isGuarded() << "\n";
    errs() << "L1 is guarded: " << L1->isGuarded() << "\n";

    BasicBlock *B0 = getEntryBlock(L0);
    BasicBlock *B1 = getEntryBlock(L1);

    errs() << "Entry block of L1: " << *B1 << "\n";

    if(L0->isGuarded()) {
        // Il successore non loop del guard branch di L0 deve essere l'entry block di L1
        BranchInst *B0Guard = dyn_cast<BranchInst>(B0->getTerminator());

        if(!B0Guard || B0Guard->isUnconditional())  return false;

        for(unsigned i = 0; i < B0Guard->getNumSuccessors(); i++)
            if(B0Guard->getSuccessor(i) == B1)
                return true;
    }

    SmallVector<BasicBlock*,4> ExitBlocks;
    L0->getExitingBlocks(ExitBlocks);

    for(BasicBlock *ExitBlock: ExitBlocks) {
        errs() << "Exiting Block of L0: " << *ExitBlock << "\n";
        Instruction *Term = ExitBlock->getTerminator();
        errs() << " with terminator: " << *Term << "\n";
        for(unsigned i = 0; i < Term->getNumSuccessors(); i++) {
            errs() << "Successor " << i << ": " << *(Term->getSuccessor(i)) << "\n";
            BasicBlock *succ = Term->getSuccessor(i);
            if(L0 != LI.getLoopFor(succ) && succ != B1) {
                errs() << "Successor of ExitBlock " <<i<< " is not the entry block of L1\n";
                return false;
            }
        }
    }

    //Controlla che nel preheader di L1 ossia B1 sia presente solo la branch NON condizionale
    if(B1->size() == 1) {
        if (const auto *BI = dyn_cast<BranchInst>(B1->getTerminator())) {
            errs() << "Preheader of Loop1 doesn't contain only a branch instrucion.\n";
            return BI->isUnconditional();
        }

    } 
      
    errs() << "Loops are not adjacent\n";
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
    // auto TC0 = SE.getSmallConstantTripCount(L0);
    // auto TC1 = SE.getSmallConstantTripCount(L1);

    auto TC0 = SE.getTripCountFromExitCount(SE.getExitCount(L0, L0->getExitingBlock()));
    auto TC1 = SE.getTripCountFromExitCount(SE.getExitCount(L1, L1->getExitingBlock()));

    errs() << "Trip count for Loop 0: " << (&TC0) << "\n";
    errs() << "Trip count for Loop 1: " << (&TC1) << "\n";

    if (TC0 && TC1 && TC0 == TC1) {
        return true;
    } else {
        errs() << "Trip counts are not equal, loops cannot be fused\n";
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
    // Ottieni il body di L1 da inserire in L0
       BasicBlock* Body1 = getBody(L1);

       // Il blocco di uscita di L1 è il blocco di ingresso di L0
       BasicBlock* Exit0 = getEntryBlock(L0);
       BasicBlock* Latch0 = L0->getLoopLatch();
       BasicBlock* Header0 = L0->getHeader();

       BasicBlock* Exit1 = L1->getExitBlock();
       BasicBlock* Latch1 = L1->getLoopLatch();
       BasicBlock* Header1 = L1->getHeader();

    if (!Header0 || !Header1 || !Latch0 || !Latch1) {
       errs() << "Uno dei blocchi chiave manca\n";
       return false;
    }

    // 1. Modifica gli usi della variabile di induzione nel body del
    // loop 1 con quelli della variabile di induzione del loop 0
    // Trova le IV (phi node) nei due header
    errs() << "Header0: " << *Header0 << "\n";
    errs() << "Header1: " << *Header1 << "\n";
    PHINode *IV0 = dyn_cast<PHINode>(&Header0->front());
    PHINode *IV1 = dyn_cast<PHINode>(&Header1->front());
    if (!IV0) {
       errs() << "Impossibile trovare la variabile di induzione L0\n";
       return false;
    }
    if (!IV1) {
       errs() << "Impossibile trovare la variabile di induzione L1\n";
       return false;
    }
    // Sostituisci tutti gli usi della variabile di induzione di L1 con quella di L0
    IV1->replaceAllUsesWith(IV0);

    // Collega il terminatore dell'header di L0 al body di L1
    Header0->getTerminator()->replaceUsesOfWith(Exit0, Exit1);

    // I blocchi predecessori di Latch0 devono ora avere Body1 come successore
    for (BasicBlock *Pred : predecessors(Latch0)) {
       // Sostituisci il successore del predecessore con Body1
       Pred->getTerminator()->replaceUsesOfWith(Latch0, Body1);
    }

    // I blocchi predecessori di Latch1 devono ora avere Latch0 come successore
    for (BasicBlock *Pred : predecessors(Latch1)) {
       // Sostituisci il successore del predecessore con Latch0
       Pred->getTerminator()->replaceUsesOfWith(Latch1, Latch0);
    }

    // Aggiungi i blocchi di L1 al loop di L0
    for (BasicBlock *BB : L1->blocks()) {
       if (BB != Header1 && BB != Latch1) {
          L0->addBasicBlockToLoop(BB, LI);
       }
    }

    LI.erase(L1); // Elimina il loop L1 dalla LoopInfo
    
    // Controlla che L1 non sia più presente nella LoopInfo
    for (auto &L : LI) {
       if (L == L1) {
          errs() << "ERRORE: Loop L1 ancora presente in LoopInfo!\n";
          break;
       }
    }
    errs() << "Confermato: Loop L1 rimosso da LoopInfo.\n";

    return true;
}

  static bool isRequired() { return true; }

};




//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getTestPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "loopFusion", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, FunctionPassManager &FPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "loop_fusion") {
                    FPM.addPass(TestPass());
                    FPM.addPass(LoopSimplifyPass());
                    // FPM.addPass(LoopRotatePass());
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
