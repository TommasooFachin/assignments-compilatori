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
