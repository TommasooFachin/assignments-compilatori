#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;

//-----------------------------------------------------------------------------
// TestPass implementation
//-----------------------------------------------------------------------------
namespace {

struct TestPass : PassInfoMixin<TestPass> {
  // Main entry point per il nuovo Pass Manager
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Transformed = false;
  
    for (auto &B : F) {
      if (runOnBasicBlock(B)) {
        Transformed = true;
      }
    }

    // Restituisci PreservedAnalyses::none() se ci sono state trasformazioni,
    // altrimenti PreservedAnalyses::all().
    return Transformed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  bool runOnBasicBlock(BasicBlock &B) {
    bool Transformed = false;

    for (auto it = B.begin(); it != B.end(); ) {
        Instruction *I = &*it++;
        
        llvm::errs() << "Analizzando istruzione: " << *I << "\n";

        // Controlla se l'istruzione è un'operazione binaria
        if (auto *BinOp = dyn_cast<BinaryOperator>(I)) {
            // Controlla se è un'addizione
            if (BinOp->getOpcode() == Instruction::Add) {
                Value *Op1 = BinOp->getOperand(0);
                Value *Op2 = BinOp->getOperand(1);

                // Controlla se uno degli operandi è una costante 0
                if (auto *C = dyn_cast<ConstantInt>(Op1)) {
                    if (C->isZero()) {
                        // Messaggio di debug
                        llvm::errs() << "Ottimizzazione: " << *BinOp 
                                     << " sostituito con " << *Op2 << "\n";

                        // Sostituisci l'istruzione con l'altro operando
                        BinOp->replaceAllUsesWith(Op2);
                        BinOp->eraseFromParent();
                        Transformed = true;
                        continue;
                    }
                }

                if (auto *C = dyn_cast<ConstantInt>(Op2)) {
                    if (C->isZero()) {
                        // Messaggio di debug
                        llvm::errs() << "Ottimizzazione: " << *BinOp 
                                     << " sostituito con " << *Op1 << "\n";

                        // Sostituisci l'istruzione con l'altro operando
                        BinOp->replaceAllUsesWith(Op1);
                        BinOp->eraseFromParent();
                        Transformed = true;
                        continue;
                    }
                }
            }

            // Controlla se è una moltiplicazione
            if (BinOp->getOpcode() == Instruction::Mul) {
                Value *Op1 = BinOp->getOperand(0);
                Value *Op2 = BinOp->getOperand(1);

                // Controlla se uno degli operandi è una costante 1
                if (auto *C = dyn_cast<ConstantInt>(Op1)) {
                    if (C->isOne()) {
                        // Messaggio di debug
                        llvm::errs() << "Ottimizzazione: " << *BinOp 
                                     << " sostituito con " << *Op2 << "\n";

                        // Sostituisci l'istruzione con l'altro operando
                        BinOp->replaceAllUsesWith(Op2);
                        BinOp->eraseFromParent();
                        Transformed = true;
                        continue;
                    }
                }

                if (auto *C = dyn_cast<ConstantInt>(Op2)) {
                    if (C->isOne()) {
                        // Messaggio di debug
                        llvm::errs() << "Ottimizzazione: " << *BinOp 
                                     << " sostituito con " << *Op1 << "\n";

                        // Sostituisci l'istruzione con l'altro operando
                        BinOp->replaceAllUsesWith(Op1);
                        BinOp->eraseFromParent();
                        Transformed = true;
                        continue;
                    }
                }
            }
        }
    }

    return Transformed;
  }

  // Questo pass è richiesto per le funzioni con l'attributo optnone
  static bool isRequired() { return true; }
};

} // namespace

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
