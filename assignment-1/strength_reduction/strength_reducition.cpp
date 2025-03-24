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
    bool runOnBasicBlock(BasicBlock &B) {
      bool Changed = false;  // Tiene traccia se il blocco è stato modificato
      LLVMContext &Context = B.getContext();  // Contesto LLVM per creare costanti
  
      // Itera su tutte le istruzioni nel Basic Block
      for (auto &I : B) {
          // --- Strength Reduction per MOLTIPLICAZIONE (es: 15*x → (x << 4) - x) ---
          if (auto *Mul = dyn_cast<BinaryOperator>(&I)) {  // Controlla se è un'operazione binaria
              if (Mul->getOpcode() == Instruction::Mul) {  // Se è una moltiplicazione
                  Value *LHS = Mul->getOperand(0);  // Primo operando
                  Value *RHS = Mul->getOperand(1);  // Secondo operando
  
                  // Cerca la costante tra gli operandi (controlla LHS e RHS)
                  ConstantInt *C = dyn_cast<ConstantInt>(LHS);
                  if (!C) C = dyn_cast<ConstantInt>(RHS);
                  
                  if (C) {  // Se uno degli operandi è una costante
                      int64_t ConstVal = C->getSExtValue();  // Valore numerico della costante
                      Value *VarOp = (C == LHS) ? RHS : LHS;  // Seleziona l'operando variabile
  
                      // Cerca la potenza di 2 più vicina (es: 15 → 16, 7 → 8)
                      bool IsPowerOfTwoMinusOne = (ConstVal & (ConstVal + 1)) == 0;  // 2^n -1 ?
                      
                      if (IsPowerOfTwoMinusOne) {  // Se la costante è del tipo 2^n -1
                          int64_t Power = ConstVal + 1;  // Calcola 2^n (es: 15+1=16)
                          int ShiftAmount = 0;  // Contatore per lo shift
                          
                          // Calcola log2(Power) con ciclo semplice (es: 16 → 4)
                          for (int64_t Temp = 1; Temp < Power; Temp <<= 1) {
                              ShiftAmount++;
                          }
  
                          // Crea l'istruzione SHL (shift left)
                          Value *Shl = BinaryOperator::CreateShl(
                              VarOp, 
                              ConstantInt::get(Context, APInt(32, ShiftAmount)), 
                              "shl", 
                              Mul
                          );
  
                          // Crea l'istruzione SUB (sottrai la variabile originale)
                          Value *Sub = BinaryOperator::CreateSub(Shl, VarOp, "sub", Mul);
                          
                          // Sostituisci la moltiplicazione con SHL + SUB
                          Mul->replaceAllUsesWith(Sub);
                          Mul->eraseFromParent();  // Rimuovi la vecchia istruzione
                          Changed = true;
                      }
                  }
              }
          }
  
          // --- Strength Reduction per DIVISIONE (es: x/8 → x >> 3) ---
          if (auto *Div = dyn_cast<BinaryOperator>(&I)) {  // Controlla se è una divisione
              if (Div->getOpcode() == Instruction::SDiv || Div->getOpcode() == Instruction::UDiv) {
                  Value *RHS = Div->getOperand(1);  // Secondo operando (divisore)
                  
                  if (auto *C = dyn_cast<ConstantInt>(RHS)) {  // Se il divisore è una costante
                      int64_t ConstVal = C->getSExtValue();
                      
                      // Controlla se la costante è una potenza di 2 positiva
                      if (ConstVal > 0 && (ConstVal & (ConstVal - 1)) == 0) {  // 2^n ?
                          int ShiftAmount = 0;  // Contatore per lo shift
                          
                          // Calcola log2(ConstVal) con ciclo semplice (es: 8 → 3)
                          for (int64_t Temp = 1; Temp < ConstVal; Temp <<= 1) {
                              ShiftAmount++;
                          }
  
                          // Crea l'istruzione SHR (shift right)
                          Value *Shr = BinaryOperator::CreateLShr(
                              Div->getOperand(0),  // Dividendo
                              ConstantInt::get(Context, APInt(32, ShiftAmount)), 
                              "shr", 
                              Div
                          );
                          
                          // Sostituisci la divisione con SHR
                          Div->replaceAllUsesWith(Shr);
                          Div->eraseFromParent();  // Rimuovi la vecchia istruzione
                          Changed = true;
                      }
                  }
              }
          }
      }
      return Changed;  // Indica se il Basic Block è stato modificato
  }  }

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
