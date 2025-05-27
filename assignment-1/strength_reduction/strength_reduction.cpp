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
    bool Changed = false;  // Tiene traccia se il blocco è stato modificato
    LLVMContext &Context = B.getContext();  // Contesto LLVM per creare costanti
  
    // Itera su tutte le istruzioni nel Basic Block
    for (auto it = B.begin(); it != B.end(); ) {
      Instruction *I = &*it++;        
      // Controlla se l'istruzione è binaria
      if (auto *BinOp = dyn_cast<BinaryOperator>(I)) {  
          
          // --- Strength Reduction per MOLTIPLICAZIONE (es: x*8 -> x << 3, x*15 -> (x << 4) - x) ---
            if (BinOp->getOpcode() == Instruction::Mul) { 
                  Value *LHS = BinOp->getOperand(0);  // Primo operando
                  Value *RHS = BinOp->getOperand(1);  // Secondo operando
  
                  // Cerca la costante tra gli operandi (controlla LHS e RHS)
                  ConstantInt *C = dyn_cast<ConstantInt>(LHS);
                  if (!C) C = dyn_cast<ConstantInt>(RHS);
                  
                  // Se uno degli operandi è una costante
                  if (C) {  
                      // ottiene il valore della costante estendendo a 64 bit signed
                      int64_t ConstVal = C->getSExtValue();  
                      Value *VarOp = (C == LHS) ? RHS : LHS;  // Seleziona l'operando variabile
  
                      // Cerca la potenza di 2 più vicina (es: 15 → 16, 7 → 8)
                      bool IsPowerOfTwoMinusOne = (ConstVal & (ConstVal + 1)) == 0;  // 2^n -1 ?
                      bool IsPowerOfTwo = (ConstVal & (ConstVal - 1)) == 0;  // 2^n ?
                      
                      if (!IsPowerOfTwo && !IsPowerOfTwoMinusOne) {
                          continue;  // Salta se non è una costante 2^n o 2^n -1
                      }
                      
                      int64_t Power = (IsPowerOfTwo) ? ConstVal : ConstVal + 1;  // Calcola 2^n (es: 15+1=16)
                      int ShiftAmount = 0;  // Contatore per lo shift

                      // Calcola log2(Power) con ciclo semplice (es: 16 → 4)
                      // Il ciclo lo facciamo partire da 1 ed usiamo l'operatore <
                      // potremmo anche fare partire da 2 ed usare l'operatore <=
                      for (int64_t Temp = 1; Temp < Power; Temp <<= 1) {
                        ShiftAmount++;
                      }

                      // Crea l'istruzione SHL (shift left)
                      Instruction *Shl = BinaryOperator::CreateShl(
                          VarOp, 
                          ConstantInt::get(Context, APInt(32, ShiftAmount))
                      );

                      Shl->insertAfter(BinOp);  // Inserisci SHL dopo MUL

                      // Se la costante è del tipo 2^n -1, crea l'istruzione SUB
                      // e la inserisce dopo SHL
                      // infine elimina la vecchia istruzione MUL
                      if (IsPowerOfTwoMinusOne) {  // Se la costante è del tipo 2^n -1
                        Instruction *Sub = BinaryOperator::CreateSub(Shl, VarOp);
                        Sub->insertAfter(Shl);  // Inserisci SUB dopo SHL
                        BinOp->replaceAllUsesWith(Sub);
                      } else {
                        BinOp->replaceAllUsesWith(Shl);
                      }
                      BinOp->eraseFromParent();  // Rimuovi la vecchia istruzione
                      Changed = true; // Indica che il blocco è stato modificato
                  }
             }

             // --- Strength Reduction per DIVISIONE (es: x/8 → x >> 3) ---
             // SDiv = Signed Division, UDiv = Unsigned Division
             if (BinOp->getOpcode() == Instruction::SDiv || BinOp->getOpcode() == Instruction::UDiv)
             {
               Value *RHS = BinOp->getOperand(1); // Secondo operando (divisore)

               // Se il divisore è una costante
               if (auto *C = dyn_cast<ConstantInt>(RHS))
               { 
                // ottiene il valore della costante estendendo a 64 bit signed
                 int64_t ConstVal = C->getSExtValue();

                 // Controlla se la costante è una potenza di 2 positiva
                 if (ConstVal > 0 && (ConstVal & (ConstVal - 1)) == 0)
                 {                      // 2^n ?
                   int ShiftAmount = 0; // Contatore per lo shift

                   // Calcola log2(ConstVal) con ciclo semplice (es: 8 → 3)
                   for (int64_t Temp = 1; Temp < ConstVal; Temp <<= 1)
                   {
                     ShiftAmount++;
                   }

                   // Crea l'istruzione SHR (shift right)
                   Instruction *Shr = BinaryOperator::CreateLShr(
                    BinOp->getOperand(0), // Dividendo
                       ConstantInt::get(Context, APInt(32, ShiftAmount)));

                   // insert shr after div
                   Shr->insertAfter(BinOp);

                   // Sostituisci la divisione con SHR
                   BinOp->replaceAllUsesWith(Shr);
                   BinOp->eraseFromParent(); // Rimuovi la vecchia istruzione
                   Changed = true;
                 }
               }
              }
          }
      }
      return Changed;  // Indica se il Basic Block è stato modificato
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
