#ifndef RUFUS_HPP
#define RUFUS_HPP

// LLVM Core
#include <cstddef>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>

// LLVM Passes and Optimization
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Transforms/Utils/Cloning.h>

// LLVM Target and Code Generation
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

// LLVM JIT
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

// LLVM Support
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>

class RuntimeSpecializer {
  private:
    llvm::LLVMContext Ctx;
    llvm::SMDiagnostic Err;
    std::unique_ptr<llvm::Module> M;
    std::unique_ptr<llvm::orc::LLJIT> JIT;
    std::unique_ptr<llvm::TargetMachine> TM;

    // Analysis managers (initialized once in constructor)
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    std::string target_triple;
    std::string CPU;
    llvm::SubtargetFeatures Features;

    void initialize_target();

    void initialize_pass_managers();

    void disable_optimizations();

    llvm::Function *find_function_by_demangled_name(const std::string &target);

    llvm::FunctionType *create_specialized_function_type(llvm::Function *F, const std::set<unsigned> &args_to_remove);

    llvm::Function *specialize_cloned_function(llvm::Function *F, const std::map<std::string, int> &const_args,
                                               const std::string &specialized_name);
    std::string create_specialized_name(const std::string &demangled_name, const std::map<std::string, int> &const_args);

  public:
    RuntimeSpecializer();

    RuntimeSpecializer &load_ir_file(const std::string &ir_file);

    RuntimeSpecializer &load_ir_string(const std::string &ir_source);

    RuntimeSpecializer &specialize_function(const std::string &demangled_name,
                                            const std::map<std::string, int> &const_args);

    RuntimeSpecializer &optimize();

    std::ptrdiff_t compile(const std::string &demangled_name, const std::map<std::string, int> &const_args);

    std::ptrdiff_t compile(const std::string &demangled_name);

    RuntimeSpecializer &print_module_ir();

    RuntimeSpecializer &print_debug_info();
};


#endif
