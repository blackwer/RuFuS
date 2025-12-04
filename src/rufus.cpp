#include <rufus.hpp>

// LLVM Core
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IRReader/IRReader.h>

// LLVM Passes and Optimization
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/TargetParser/Host.h>

// LLVM JIT
#include <llvm/ExecutionEngine/Orc/LLJIT.h>

// LLVM Support
#include <llvm/Demangle/Demangle.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/Utils/Cloning.h>

// Core Pass Infrastructure
#include <llvm/Analysis/CGSCCPassManager.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Passes/PassBuilder.h>

// Individual Optimization Passes
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar/DCE.h>
#include <llvm/Transforms/Scalar/EarlyCSE.h>
#include <llvm/Transforms/Scalar/LICM.h>
#include <llvm/Transforms/Scalar/LoopRotation.h>
#include <llvm/Transforms/Scalar/LoopUnrollPass.h>
#include <llvm/Transforms/Scalar/SCCP.h>
#include <llvm/Transforms/Scalar/SROA.h>
#include <llvm/Transforms/Scalar/SimplifyCFG.h>
#include <llvm/Transforms/Utils/Mem2Reg.h>
#include <llvm/Transforms/Vectorize/LoopVectorize.h>
#include <llvm/Transforms/Vectorize/SLPVectorizer.h>

// Pass Adaptors
#include <llvm/Transforms/Scalar/LoopPassManager.h>

#include <algorithm>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

// Private interface
struct RuFuS::Impl {
    Impl();

    llvm::LLVMContext Ctx;
    llvm::SMDiagnostic Err;
    std::unique_ptr<llvm::Module> M;
    std::unique_ptr<llvm::orc::LLJIT> JIT;
    std::unique_ptr<llvm::TargetMachine> TM;

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    std::string target_triple;
    std::string CPU;
    llvm::SubtargetFeatures Features;

    void initialize_target();
    void initialize_pass_managers();
    void initialize_jit();
    llvm::Function *find_function_by_demangled_name(const std::string &target);
    llvm::FunctionType *create_specialized_function_type(llvm::Function *F, const std::set<unsigned> &args_to_remove);
    std::string create_specialized_name(const std::string &demangled_name,
                                        const std::map<std::string, int> &const_args);
    void replace_alloca_with_constant(llvm::AllocaInst *AI, llvm::Constant *ConstVal);
    llvm::Function *clone_and_specialize_arguments(llvm::Function *F, const std::map<std::string, int> &const_args,
                                                   const std::string &specialized_name);
    void specialize_internal_variables(llvm::Function *F, const std::map<std::string, int> &const_vars);
    void inline_all_calls(llvm::Function *F);
    void optimize_function(llvm::Function *F);
    void disable_optimizations();
    void optimize_for_jit(llvm::Module *M, llvm::TargetMachine *TM);
    void strip_loop_metadata(llvm::Function *F);
    void fix_function_attributes(llvm::Function *F);
    std::map<llvm::Function *, bool> is_optimized;

    unsigned MaxVectorWidth = 128;
};

RuFuS::Impl::Impl() {
    initialize_target();
    initialize_pass_managers();
    initialize_jit();
}

std::string RuFuS::Impl::create_specialized_name(const std::string &demangled_name,
                                                 const std::map<std::string, int> &const_args) {
    size_t paren_pos = demangled_name.find('(');
    std::string basename = demangled_name.substr(0, paren_pos);

    // Create hash of full signature for uniqueness
    std::hash<std::string> hasher;
    size_t sig_hash = hasher(demangled_name);

    std::ostringstream oss;
    oss << basename;

    // Add const args
    for (const auto &[name, value] : const_args)
        oss << "_" << name << "_" << value;

    // Add short hash for overload disambiguation
    oss << "_" << std::hex << std::setw(8) << std::setfill('0') << (sig_hash & 0xFFFFFFFF);

    return oss.str();
}

void RuFuS::Impl::initialize_target() {
    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

    target_triple = llvm::sys::getDefaultTargetTriple();
    CPU = llvm::sys::getHostCPUName().str();

    // Collect host CPU features
    for (const auto &F : llvm::sys::getHostCPUFeatures()) {
        if (F.second)
            Features.AddFeature(F.first());
    }

    // Create target machine
    std::string Error;
    const llvm::Target *target = llvm::TargetRegistry::lookupTarget(target_triple, Error);
    if (target) {
        TM.reset(
            target->createTargetMachine(target_triple, CPU, Features.getString(), llvm::TargetOptions(), std::nullopt));
        if (TM) {
            // Query TTI for the actual register width
            // This works for x86, ARM, RISC-V, etc.
            auto TTI = TM.get()->getTargetIRAnalysis();

            // Get the actual hardware vector register width
            MaxVectorWidth = 128; // Safe default

            // Try to get it from target machine features
            llvm::StringRef Features = TM.get()->getTargetFeatureString();
            if (Features.contains("avx512"))
                MaxVectorWidth = 512;
            else if (Features.contains("avx"))
                MaxVectorWidth = 256;
            else if (Features.contains("neon"))
                MaxVectorWidth = 128;
            else if (Features.contains("sve"))
                MaxVectorWidth = 2048; // ARM SVE
        } else {
            MaxVectorWidth = 128;
        }
    }
}

void RuFuS::Impl::initialize_pass_managers() {
    llvm::PassBuilder PB(TM.get());
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
}

void RuFuS::Impl::initialize_jit() {
    auto jit_or_err = llvm::orc::LLJITBuilder().create();
    if (!jit_or_err) {
        llvm::errs() << "Failed to create JIT\n";
        return;
    }
    JIT = std::move(*jit_or_err);

    // NEW: Add support for C standard library symbols
    auto &ES = JIT->getExecutionSession();
    auto &MainJD = JIT->getMainJITDylib();

    auto DLSG = llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(JIT->getDataLayout().getGlobalPrefix());

    if (!DLSG) {
        llvm::errs() << "Failed to create dynamic library search generator\n";
        return;
    }

    MainJD.addGenerator(std::move(*DLSG));
}

void RuFuS::Impl::disable_optimizations() {
    for (auto &F : M->functions()) {
        if (!F.isDeclaration()) {
            F.addFnAttr(llvm::Attribute::OptimizeNone);
            F.removeFnAttr("min-legal-vector-width");
            F.addFnAttr("min-legal-vector-width", std::to_string(MaxVectorWidth));
            F.addFnAttr("prefer-vector-width", std::to_string(MaxVectorWidth));
        }
    }
}

llvm::Function *RuFuS::Impl::find_function_by_demangled_name(const std::string &target) {
    auto normalize = [](const std::string &s) {
        std::string result = s;
        result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
        return result;
    };

    std::string normalized_target = normalize(target);

    for (auto &F : M->functions()) {
        if (F.isDeclaration())
            continue;

        std::string demangled = llvm::demangle(F.getName().str());
        std::string normalized_demangled = normalize(demangled);

        if (normalized_demangled == normalized_target || normalized_demangled.find(normalized_target) == 0) {
            return &F;
        }
    }
    return nullptr;
}

llvm::FunctionType *RuFuS::Impl::create_specialized_function_type(llvm::Function *F,
                                                                  const std::set<unsigned> &args_to_remove) {
    std::vector<llvm::Type *> new_param_types;
    unsigned idx = 0;

    for (auto &Arg : F->args()) {
        if (args_to_remove.find(idx) == args_to_remove.end())
            new_param_types.push_back(Arg.getType());
        idx++;
    }

    return llvm::FunctionType::get(F->getReturnType(), new_param_types, F->isVarArg());
}

void RuFuS::Impl::replace_alloca_with_constant(llvm::AllocaInst *AI, llvm::Constant *ConstVal) {

    llvm::SmallVector<llvm::Instruction *, 16> to_remove;

    // Collect all users and replace loads, mark stores for deletion
    for (llvm::User *U : llvm::make_early_inc_range(AI->users())) {
        if (auto *LI = llvm::dyn_cast<llvm::LoadInst>(U)) {
            // Replace load with constant
            LI->replaceAllUsesWith(ConstVal);
            to_remove.push_back(LI);
        } else if (auto *SI = llvm::dyn_cast<llvm::StoreInst>(U)) {
            // Mark store for removal
            to_remove.push_back(SI);
        }
    }

    // Remove dead instructions
    for (llvm::Instruction *I : to_remove) {
        I->eraseFromParent();
    }

    // Remove the alloca itself
    AI->eraseFromParent();
}

void RuFuS::Impl::specialize_internal_variables(llvm::Function *F, const std::map<std::string, int> &const_vars) {

    if (const_vars.empty())
        return;

    // Find allocas with matching names and replace their stores/loads
    llvm::SmallVector<llvm::AllocaInst *, 8> allocas_to_process;

    for (llvm::BasicBlock &BB : *F) {
        for (llvm::Instruction &I : BB) {
            if (auto *AI = llvm::dyn_cast<llvm::AllocaInst>(&I)) {
                std::string var_name = AI->getName().str();
                if (const_vars.count(var_name)) {
                    allocas_to_process.push_back(AI);
                }
            }
        }
    }

    // Process each matching alloca
    for (llvm::AllocaInst *AI : allocas_to_process) {
        std::string var_name = AI->getName().str();
        int const_value = const_vars.at(var_name);

        llvm::Type *alloca_type = AI->getAllocatedType();
        llvm::Constant *const_val = llvm::ConstantInt::get(alloca_type, const_value);

        // Replace all uses with the constant value
        replace_alloca_with_constant(AI, const_val);
    }
}

llvm::Function *RuFuS::Impl::clone_and_specialize_arguments(llvm::Function *F,
                                                            const std::map<std::string, int> &const_function_args,
                                                            const std::string &specialized_name) {
    // Build argument specialization info
    std::set<unsigned> args_to_remove;
    std::map<unsigned, int> arg_values;
    unsigned idx = 0;

    for (auto &Arg : F->args()) {
        std::string arg_name = Arg.getName().str();
        if (const_function_args.count(arg_name)) {
            args_to_remove.insert(idx);
            arg_values[idx] = const_function_args.at(arg_name);
        }
        idx++;
    }

    // Create new function with reduced signature
    llvm::FunctionType *new_type = create_specialized_function_type(F, args_to_remove);
    llvm::Function *new_func = llvm::Function::Create(new_type, F->getLinkage(), specialized_name, M.get());

    new_func->copyAttributesFrom(F);

    // Map old arguments to new arguments or constants
    llvm::ValueToValueMapTy VMap;
    auto new_arg_it = new_func->arg_begin();
    idx = 0;

    for (auto &old_arg : F->args()) {
        if (args_to_remove.count(idx)) {
            // Replace with constant
            VMap[&old_arg] = llvm::ConstantInt::get(old_arg.getType(), arg_values[idx]);
        } else {
            // Map to new argument
            new_arg_it->setName(old_arg.getName());
            VMap[&old_arg] = &(*new_arg_it);
            ++new_arg_it;
        }
        idx++;
    }

    // Clone function body
    llvm::SmallVector<llvm::ReturnInst *, 8> returns;
    llvm::CloneFunctionInto(new_func, F, VMap, llvm::CloneFunctionChangeType::LocalChangesOnly, returns);

    return new_func;
}

void RuFuS::Impl::inline_all_calls(llvm::Function *F) {
    llvm::outs() << "Inlining calls in function: " << F->getName() << "\n";

    bool changed = true;
    while (changed) {
        changed = false;
        llvm::SmallVector<llvm::CallInst *, 16> calls_to_inline;

        // Collect all call sites to defined functions
        for (llvm::BasicBlock &BB : *F) {
            for (llvm::Instruction &I : BB) {
                if (auto *CI = llvm::dyn_cast<llvm::CallInst>(&I)) {
                    llvm::Function *Callee = CI->getCalledFunction();
                    if (Callee && !Callee->isDeclaration() && !Callee->isIntrinsic()) {
                        calls_to_inline.push_back(CI);
                    }
                }
            }
        }

        // Inline each call
        for (llvm::CallInst *CI : calls_to_inline) {
            llvm::InlineFunctionInfo IFI;
            auto name = CI->getCalledFunction()->getName();
            llvm::InlineResult result = llvm::InlineFunction(*CI, IFI);
            if (result.isSuccess()) {
                changed = true;
            }
        }
    }
}

void RuFuS::Impl::optimize_function(llvm::Function *F) {
    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB(TM.get());

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::FunctionPassManager FPM;

    // Add the key optimization passes in the right order
    FPM.addPass(llvm::PromotePass());

    FPM.addPass(llvm::InstCombinePass());
    FPM.addPass(llvm::SimplifyCFGPass());
    FPM.addPass(llvm::SROAPass(llvm::SROAOptions::ModifyCFG));
    FPM.addPass(llvm::EarlyCSEPass(true));

    // Loop optimization with MemorySSA enabled
    llvm::LoopPassManager LPM;
    LPM.addPass(llvm::LoopRotatePass());

    // Use LICMOptions with MemorySSA enabled
    llvm::LICMOptions LICMOpts;
    LPM.addPass(llvm::LICMPass(LICMOpts));
    FPM.addPass(llvm::createFunctionToLoopPassAdaptor(std::move(LPM), true));

    // Vectorization
    FPM.addPass(llvm::LoopVectorizePass());
    FPM.addPass(llvm::SLPVectorizerPass());

    // Loop unrolling
    FPM.addPass(llvm::LoopUnrollPass());

    // Propagate constants
    FPM.addPass(llvm::SCCPPass());

    // Cleanup
    FPM.addPass(llvm::InstCombinePass());
    FPM.addPass(llvm::SimplifyCFGPass());
    FPM.addPass(llvm::DCEPass());

    FPM.run(*F, FAM);
    is_optimized[F] = true;
}

void RuFuS::Impl::fix_function_attributes(llvm::Function *F) {
    F->removeFnAttr(llvm::Attribute::OptimizeNone);
    F->removeFnAttr(llvm::Attribute::NoInline);
    F->removeFnAttr(llvm::Attribute::MinSize);
    F->removeFnAttr(llvm::Attribute::OptimizeForSize);
    F->addFnAttr("target-cpu", CPU);
    F->addFnAttr("target-features", Features.getString());
}

void RuFuS::Impl::optimize_for_jit(llvm::Module *M, llvm::TargetMachine *TM) {
    for (auto &F : M->functions()) {
        if (!F.isDeclaration()) {
            F.removeFnAttr(llvm::Attribute::OptimizeNone);
            F.addFnAttr("no-trapping-math", "false");
            F.removeFnAttr("noinline");
            F.removeFnAttr("frame-pointer");
            F.removeFnAttr("min-legal-vector-width");
            F.addFnAttr("min-legal-vector-width", std::to_string(MaxVectorWidth));
            F.addFnAttr("prefer-vector-width", std::to_string(MaxVectorWidth));
            F.removeFnAttr("stack-protector-buffer-size");
            F.addFnAttr("no-infs-fp-math", "true");
            F.addFnAttr("no-nans-fp-math", "true");
            F.addFnAttr("no-signed-zeros-fp-math", "true");
            F.addFnAttr("unsafe-fp-math", "true");
        }
    }

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB(TM);

    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::ModulePassManager MPM;

    // Run O3 pipeline to normalize the IR
    MPM = PB.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);

    MPM.run(*M, MAM);
}

// ************************************************************************************** \\
//  ____  _   _ ____  _     ___ ____   ___ _   _ _____ _____ ____  _____ _    ____ _____  \\
// |  _ \| | | | __ )| |   |_ _/ ___| |_ _| \ | |_   _| ____|  _ \|  ___/ \  / ___| ____| \\
// | |_) | | | |  _ \| |    | | |      | ||  \| | | | |  _| | |_) | |_ / _ \| |   |  _|   \\
// |  __/| |_| | |_) | |___ | | |___   | || |\  | | | | |___|  _ <|  _/ ___ \ |___| |___  \\
// |_|    \___/|____/|_____|___\____| |___|_| \_| |_| |_____|_| \_\_|/_/   \_\____|_____| \\
//                                                                                        \\
// ************************************************************************************** \\

RuFuS::RuFuS() : impl(std::make_unique<Impl>()) {}

RuFuS::~RuFuS() = default;

RuFuS::RuFuS(RuFuS &&) noexcept = default;

RuFuS &RuFuS::operator=(RuFuS &&) noexcept = default;

RuFuS &RuFuS::load_ir_file(const std::string &ir_file) {
    impl->M = llvm::parseIRFile(ir_file, impl->Err, impl->Ctx);
    if (!impl->M) {
        llvm::errs() << "Failed to load IR from: " << ir_file << "\n";
        impl->Err.print("load_ir_file", llvm::errs());
    } else {
        impl->disable_optimizations();
    }
    return *this;
}

RuFuS &RuFuS::load_ir_string(const std::string &ir_source) {
    auto mem_buf = llvm::MemoryBuffer::getMemBuffer(ir_source);
    impl->M = llvm::parseIR(mem_buf->getMemBufferRef(), impl->Err, impl->Ctx);
    if (!impl->M) {
        llvm::errs() << "Failed to load IR from string\n";
        impl->Err.print("load_ir_string", llvm::errs());
    } else {
        impl->disable_optimizations();
    }
    return *this;
}

void RuFuS::Impl::strip_loop_metadata(llvm::Function *F) {
    for (llvm::BasicBlock &BB : *F) {
        for (llvm::Instruction &I : BB) {
            // Find loop backedge branches
            if (auto *BI = llvm::dyn_cast<llvm::BranchInst>(&I)) {
                llvm::MDNode *LoopMD = BI->getMetadata(llvm::LLVMContext::MD_loop);
                if (!LoopMD)
                    continue;

                // Create new loop metadata without unroll-disable directives
                llvm::SmallVector<llvm::Metadata *, 4> NewOps;

                for (unsigned i = 0; i < LoopMD->getNumOperands(); ++i) {
                    llvm::MDNode *Op = llvm::dyn_cast<llvm::MDNode>(LoopMD->getOperand(i));
                    if (!Op)
                        continue;

                    // Skip unroll-disable metadata
                    if (Op->getNumOperands() > 0) {
                        if (auto *MDS = llvm::dyn_cast<llvm::MDString>(Op->getOperand(0))) {
                            llvm::StringRef Name = MDS->getString();
                            if (Name == "llvm.loop.unroll.disable" || Name == "llvm.loop.unroll.runtime.disable") {
                                continue; // Skip this metadata
                            }
                        }
                    }

                    NewOps.push_back(Op);
                }

                // Create new loop metadata
                if (!NewOps.empty()) {
                    llvm::MDNode *NewLoopMD = llvm::MDNode::get(F->getContext(), NewOps);
                    // Self-reference for loop identification
                    NewLoopMD->replaceOperandWith(0, NewLoopMD);
                    BI->setMetadata(llvm::LLVMContext::MD_loop, NewLoopMD);
                } else {
                    // Remove loop metadata entirely
                    BI->setMetadata(llvm::LLVMContext::MD_loop, nullptr);
                }
            }
        }
    }
}

RuFuS &RuFuS::specialize_function(const std::string &demangled_name, const std::map<std::string, int> &const_args) {
    llvm::Function *F = impl->find_function_by_demangled_name(demangled_name);
    if (!F) {
        llvm::errs() << "Function not found: " << demangled_name << "\n";
        return *this;
    }

    impl->inline_all_calls(F);

    // Separate const_args into arguments vs internal variables
    std::map<std::string, int> const_function_args;
    std::map<std::string, int> const_internal_vars;

    for (const auto &[name, value] : const_args) {
        bool is_arg = false;
        for (auto &Arg : F->args()) {
            if (Arg.getName() == name) {
                const_function_args[name] = value;
                is_arg = true;
                break;
            }
        }
        if (!is_arg) {
            const_internal_vars[name] = value;
        }
    }

    const std::string specialized_name = impl->create_specialized_name(demangled_name, const_args);
    llvm::Function *specialized_func = impl->clone_and_specialize_arguments(F, const_function_args, specialized_name);

    impl->specialize_internal_variables(specialized_func, const_internal_vars);
    impl->strip_loop_metadata(specialized_func);
    impl->fix_function_attributes(specialized_func);

    llvm::outs() << "Created: " << specialized_name << " (args: " << F->arg_size() << " -> "
                 << specialized_func->arg_size() << ")\n";

    return *this;
}

RuFuS &RuFuS::optimize() {
    if (!impl->M)
        return *this;

    for (llvm::Function &F : *impl->M) {
        if (!F.isDeclaration() && !F.hasFnAttribute(llvm::Attribute::OptimizeNone) && !impl->is_optimized[&F]) {
            impl->optimize_function(&F);
        }
    }

    return *this;
}

RuFuS &RuFuS::print_module_ir() {
    if (impl->M)
        impl->M->print(llvm::outs(), nullptr);
    return *this;
}

RuFuS &RuFuS::print_debug_info() {
    if (!impl->M) {
        llvm::errs() << "No module loaded\n";
        return *this;
    }

    for (auto &F : impl->M->functions()) {
        if (F.isDeclaration())
            continue;

        llvm::outs() << "\nFunction: " << llvm::demangle(F.getName().str()) << "\n";
        llvm::outs() << "  Mangled: " << F.getName() << "\n";
        llvm::outs() << "  Args: ";

        bool first = true;
        for (auto &arg : F.args()) {
            if (!first)
                llvm::outs() << ", ";
            llvm::outs() << arg.getName();
            first = false;
        }
        llvm::outs() << "\n";
    }

    return *this;
}

std::uintptr_t RuFuS::compile(const std::string &demangled_name, const std::map<std::string, int> &const_args) {
    std::string specialized_name = impl->create_specialized_name(demangled_name, const_args);

    if (!impl->find_function_by_demangled_name(specialized_name)) {
        specialize_function(demangled_name, const_args);
    }

    return compile(specialized_name);
}

std::uintptr_t RuFuS::compile(const std::string &demangled_name) {
    auto &JD = impl->JIT->getMainJITDylib();
    auto &ES = impl->JIT->getExecutionSession();

    llvm::Function *target_func = impl->find_function_by_demangled_name(demangled_name);
    if (!target_func) {
        llvm::errs() << "Function not found: " << demangled_name << "\n";
        return 0;
    }

    {
        auto sym_or_err = impl->JIT->lookup(target_func->getName());
        if (sym_or_err)
            return sym_or_err->getValue();
        llvm::consumeError(sym_or_err.takeError());
    }

    // Serialize the function and its dependencies to a string
    std::string module_str;
    llvm::raw_string_ostream OS(module_str);
    impl->M->print(OS, nullptr);
    OS.flush();

    // Parse into a new context
    auto new_ctx = std::make_unique<llvm::LLVMContext>();
    llvm::SMDiagnostic Err;
    auto new_module = llvm::parseIR(llvm::MemoryBufferRef(module_str, "module"), Err, *new_ctx);

    if (!new_module) {
        llvm::errs() << "Failed to parse module: ";
        Err.print("rufus", llvm::errs());
        return 0;
    }

    for (auto &F : *new_module) {
        if (F.isDeclaration())
            continue;
        if (&F == target_func)
            continue;

        // Check if in JIT
        auto Sym = ES.lookup({&JD}, ES.intern(F.getName()));
        if (Sym) {
            // Already compiled - make it a declaration
            if (F.hasComdat()) {
                F.setComdat(nullptr);
            }
            F.deleteBody();
            llvm::outs() << "Linked to existing: " << F.getName() << "\n";
        } else {
            // Not in JIT yet - keep the body, it will be compiled
            llvm::consumeError(Sym.takeError());
            llvm::outs() << "Will compile: " << F.getName() << "\n";
        }
    }

    // Find the function in the new module
    llvm::Function *new_func = new_module->getFunction(target_func->getName());
    if (!new_func) {
        llvm::errs() << "Function not found in cloned module\n";
        return 0;
    }

    // Verify
    if (llvm::verifyModule(*new_module, &llvm::errs())) {
        llvm::errs() << "Module verification failed\n";
        return 0;
    }
    llvm::outs() << "Module verified successfully\n";

    // Optimize whole module
    impl->optimize_for_jit(new_module.get(), impl->TM.get());
    llvm::outs() << "Module optimized for JIT successfully\n";

    // Create ThreadSafeModule with new context
    auto TSM = llvm::orc::ThreadSafeModule(std::move(new_module), std::move(new_ctx));

    if (auto err = impl->JIT->addIRModule(std::move(TSM))) {
        llvm::errs() << "JIT Error: " << llvm::toString(std::move(err)) << "\n";
        return 0;
    }
    llvm::outs() << "Module added to TSM successfully\n";

    auto sym_or_err = impl->JIT->lookup(new_func->getName());
    if (!sym_or_err) {
        llvm::errs() << "Lookup failed - compilation error occurred here\n";
        auto err = sym_or_err.takeError();
        llvm::handleAllErrors(
            std::move(err), [](const llvm::ErrorInfoBase &EI) { llvm::errs() << "  Error: " << EI.message() << "\n"; });
        return 0;
    }
    llvm::outs() << "Function looked up successfully\n";

    return sym_or_err->getValue();
}
