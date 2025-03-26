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

    // Lista temporanea per memorizzare le istruzioni da eliminare
    std::vector<Instruction *> ToErase;

    for (auto it = B.begin(); it != B.end(); ) {
        Instruction *I = &*it++;

        // Controlla se l'istruzione è un'addizione
        if (auto *AddInst = dyn_cast<BinaryOperator>(I)) {
            if (AddInst->getOpcode() == Instruction::Add) {
                // Ottieni gli operandi dell'addizione
                Value *Op1 = AddInst->getOperand(0);
                Value *Op2 = AddInst->getOperand(1);

                // Controlla se uno degli operandi è una costante 1
                if (auto *ConstOne = dyn_cast<ConstantInt>(Op2)) {
                    if (ConstOne->isOne()) {
                        // Cerca un'istruzione successiva che sia una sottrazione
                        for (auto SubIt = it; SubIt != B.end(); ++SubIt) {
                            Instruction *SubInst = &*SubIt;
                            if (auto *SubBinOp = dyn_cast<BinaryOperator>(SubInst)) {
                                if (SubBinOp->getOpcode() == Instruction::Sub) {
                                    // Ottieni gli operandi della sottrazione
                                    Value *SubOp1 = SubBinOp->getOperand(0);
                                    Value *SubOp2 = SubBinOp->getOperand(1);

                                    // Controlla se la sottrazione usa il risultato dell'addizione
                                    // e se il secondo operando è una costante 1
                                    if (SubOp1 == AddInst && 
                                        isa<ConstantInt>(SubOp2) && 
                                        cast<ConstantInt>(SubOp2)->isOne()) {
                                        
                                        // Messaggio di debug
                                        llvm::errs() << "Ottimizzazione: " << *SubBinOp 
                                                     << " sostituito con " << *Op1 << "\n";

                                        // Sostituisci il risultato della sottrazione con `b` (Op1)
                                        SubBinOp->replaceAllUsesWith(Op1);

                                        // Aggiungi l'istruzione alla lista per l'eliminazione
                                        ToErase.push_back(SubBinOp);
                                        Transformed = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Elimina in modo sicuro tutte le istruzioni raccolte
    for (Instruction *Inst : ToErase) {
        Inst->eraseFromParent();
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
