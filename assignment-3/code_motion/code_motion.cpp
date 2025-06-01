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
            std::vector<Instruction*> ToMove;

            for (Instruction &I : *BB) {
                if (isCandidateForCodeMotion(I, L, BB, ExitBlocks, DT, MovedInstructions)) {
                    ToMove.push_back(&I);
                }
            }

            for (Instruction *I : ToMove) {
                errs() << "Moving instruction: " << *I << " to the preheader of the loop.\n";
                I->moveBefore(Preheader->getTerminator());
                MovedInstructions.insert(I);
                errs() << "The instruction has been moved correctly.\n";
                Changed = true;
            }
        }
    }

    return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  // Funzione per verificare se un'istruzione è loop-invariant
bool isLoopInvariant(Instruction &I, Loop *L, DominatorTree &DT) {
    errs() << "[DEBUG] Controllo loop-invariance per: " << I << "\n";
    // Se l'istruzione non è nel loop, è invariante per definizione
    if (!L->contains(&I)) {
        errs() << "[DEBUG] Istruzione " << I << " non è nel loop, quindi è invariante.\n";
        return true;
    }

    for (Value *Op : I.operands()) {
        if (Instruction *Inst = dyn_cast<Instruction>(Op)) {
            errs() << "[DEBUG] Controllo operando: " << *Inst << "\n";
            if (L->contains(Inst)) {
                // Caso 1: Se l'operando non domina I, non è invariante
                if (!DT.dominates(Inst, &I)) {
                    errs() << "[DEBUG] Operando " << *Inst << " non domina " << I << ", quindi non è invariante.\n";
                    return false;
                }
                // Caso 2: Verifica ricorsivamente l'operando
                if (!isLoopInvariant(*Inst, L, DT)) {
                    errs() << "[DEBUG] Operando " << *Inst << " non è invariante nel loop.\n";
                    return false;
                }
            }
        }
    }
    errs() << "[DEBUG] Istruzione " << I << " è loop-invariant.\n";
    return true;
}

// Funzione per verificare se un blocco domina tutte le uscite del loop
bool dominatesAllExits(BasicBlock *BB, const SmallVectorImpl<BasicBlock *> &ExitBlocks, DominatorTree &DT) {
    errs() << "[DEBUG] Controllo se il blocco " << BB->getName() << " domina tutte le uscite del loop.\n";
    for (BasicBlock *Exit : ExitBlocks) {
        errs() << "[DEBUG] Controllo uscita: " << Exit->getName() << "\n";
        if (!DT.dominates(BB, Exit)) {
            errs() << "[DEBUG] Il blocco " << BB->getName() << " NON domina l'uscita " << Exit->getName() << "\n";
            return false;
        }
    }
    errs() << "[DEBUG] Il blocco " << BB->getName() << " domina tutte le uscite del loop.\n";
    return true;
}

bool isReassignedInLoop(Instruction &I, Loop *L) {
    errs() << "[DEBUG] Controllo se l'istruzione è riscritta nel loop: " << I << "\n";
    if (StoreInst *Store = dyn_cast<StoreInst>(&I)) {
        Value *Ptr = Store->getPointerOperand();
        for (BasicBlock *BB : L->blocks()) {
            for (Instruction &Inst : *BB) {
                if (&Inst != &I && isa<StoreInst>(&Inst)) {
                    if (cast<StoreInst>(&Inst)->getPointerOperand() == Ptr) {
                        errs() << "[DEBUG] Trovata riscrittura su " << *Ptr << " in " << Inst << "\n";
                        return true;
                    }
                }
            }
        }
    }
    errs() << "[DEBUG] Nessuna riscrittura trovata per " << I << "\n";
    return false;
}

bool dominatesAllUses(Instruction &I, Loop *L, DominatorTree &DT) {
    errs() << "[DEBUG] Controllo se " << I << " domina tutti gli usi nel loop.\n";
    for (User *U : I.users()) {
        auto *UserInst = dyn_cast<Instruction>(U);
        if (!UserInst) {
            errs() << "[DEBUG] User non è un'istruzione, skip.\n";
            continue;
        }
        errs() << "[DEBUG] Controllo uso in: " << *UserInst << "\n";
        if (L->contains(UserInst->getParent())) {
            if (!DT.dominates(I.getParent(), UserInst->getParent())) {
                errs() << "[DEBUG] " << *I.getParent() << " NON domina " << *UserInst->getParent() << "\n";
                return false;
            }
        }
    }
    errs() << "[DEBUG] " << I << " domina tutti gli usi nel loop.\n";
    return true;
}

bool allDependenciesMoved(Instruction &I, const std::set<Instruction *> &MovedInstructions) {
    errs() << "[DEBUG] Controllo se tutte le dipendenze di " << I << " sono state già mosse.\n";
    for (Value *Op : I.operands()) {
        if (Instruction *Inst = dyn_cast<Instruction>(Op)) {
            errs() << "[DEBUG] Dipendenza: " << *Inst << "\n";
            if (MovedInstructions.find(Inst) == MovedInstructions.end()) {
                errs() << "[DEBUG] Dipendenza NON soddisfatta: " << *Inst << "\n";
                return false;
            }
        }
    }
    errs() << "[DEBUG] Tutte le dipendenze di " << I << " sono soddisfatte.\n";
    return true;
}

  bool isCandidateForCodeMotion(Instruction &I, Loop *L, BasicBlock *BB ,const SmallVectorImpl<BasicBlock *> &ExitBlocks, DominatorTree &DT, const std::set<Instruction *> &MovedInstructions) {
    
    // Ignora le istruzioni che non sono candidati per il code motion
    if (isa<PHINode>(&I)) return false;
    if (isa<llvm::BranchInst>(&I)) return false;
    if (isa<llvm::CallInst>(&I)) return false;
    if (isa<llvm::LoadInst>(&I)) return false;
    if (isa<llvm::StoreInst>(&I)) return false;
    if (isa<ICmpInst>(&I)) return false;
    if (isa<FCmpInst>(&I)) return false;
    if (I.isTerminator()) return false;
    
    if (!isLoopInvariant(I, L, DT)) {
                errs() << "Istruzione non loop-invariant: " << I << "\n";
                return false;
        }

        if (!dominatesAllExits(BB, ExitBlocks, DT)) {
                errs() << "Blocco non domina tutte le uscite del loop: " << *BB << "\n";
                return false;
        }

        if (isReassignedInLoop(I, L)) {
                errs() << "Istruzione riscritta nel loop: " << I << "\n";
                return false;   
        }

        if (!dominatesAllUses(I, L, DT)) {
                errs() << "Istruzione " << I << " non domina tutti gli usi nel loop.\n";
                return false;
        }

        if (!allDependenciesMoved(I, MovedInstructions)) {
                errs() << "Dipendenze non soddisfatte per l'istruzione: " << I << "\n";
                return false;
        }

        errs() << "Istruzione candidata per il code motion: " << I << "\n";
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
