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
  // Main entry point
  bool runOnFunction(Function &F) {
    bool Transformed = false;
  
    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
      if (runOnBasicBlock(*Iter)) {
        Transformed = true;
      }
    }
  
    return Transformed;
  }

  bool runOnBasicBlock(BasicBlock &B) {
    

  }

  PreservedAnalyses run(Function &F, FunctionAnalysisManager &) {
    bool Transformed = false;

    for (auto Iter = F.begin(); Iter != F.end(); ++Iter) {
      if (runOnBasicBlock(*Iter)) {
        Transformed = true;
      }
    }

    // Restituisci PreservedAnalyses::none() se ci sono state trasformazioni,
    // altrimenti PreservedAnalyses::all().
    return Transformed ? PreservedAnalyses::none() : PreservedAnalyses::all();
  }

  // This pass is required for functions with optnone attribute.
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
