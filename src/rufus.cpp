#include <rufus.hpp>

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

std::string RuntimeSpecializer::create_specialized_name(const std::string &demangled_name,
                                                        const std::map<std::string, int> &const_args) {
    size_t paren_pos = demangled_name.find('(');
    std::string basename = demangled_name.substr(0, paren_pos);

    std::ostringstream oss;
    oss << basename << "_RFS";
    for (const auto &[name, value] : const_args)
        oss << "_" << name << "_" << value;

    return oss.str();
}

void RuntimeSpecializer::initialize_target() {
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
    }
}

void RuntimeSpecializer::initialize_pass_managers() {
    llvm::PassBuilder PB(TM.get());
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
}

void RuntimeSpecializer::disable_optimizations() {
    for (auto &F : M->functions()) {
        if (!F.isDeclaration())
            F.addFnAttr(llvm::Attribute::OptimizeNone);
    }
}

llvm::Function *RuntimeSpecializer::find_function_by_demangled_name(const std::string &target) {
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

llvm::FunctionType *RuntimeSpecializer::create_specialized_function_type(llvm::Function *F,
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

void RuntimeSpecializer::replace_alloca_with_constant(llvm::AllocaInst *AI, llvm::Constant *ConstVal) {

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

void RuntimeSpecializer::specialize_internal_variables(llvm::Function *F,
                                                       const std::map<std::string, int> &const_vars) {

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

llvm::Constant *RuntimeSpecializer::find_constant_by_debug_info(llvm::Function *F, const std::string &var_name,
                                                                int new_value) {
    // Iterate through debug info to find where variable was declared
    for (llvm::BasicBlock &BB : *F) {
        for (llvm::Instruction &I : BB) {
            if (auto *DVI = llvm::dyn_cast<llvm::DbgValueInst>(&I)) {
                if (auto *DIVar = DVI->getVariable()) {
                    if (DIVar->getName() == var_name) {
                        // Found the variable in debug info
                        // Now trace its uses
                        llvm::Value *V = DVI->getValue();
                        if (auto *CI = llvm::dyn_cast<llvm::Constant>(V)) {
                            // This constant replaced the variable
                            return CI;
                        }
                    }
                }
            }
        }
    }
    return nullptr;
}

llvm::Function *RuntimeSpecializer::clone_and_specialize_arguments(
    llvm::Function *F, const std::map<std::string, int> &const_function_args, const std::string &specialized_name) {

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

RuntimeSpecializer::RuntimeSpecializer() {
    initialize_target();
    initialize_pass_managers();
}

RuntimeSpecializer &RuntimeSpecializer::load_ir_file(const std::string &ir_file) {
    M = llvm::parseIRFile(ir_file, Err, Ctx);
    if (!M) {
        llvm::errs() << "Failed to load IR from: " << ir_file << "\n";
        Err.print("load_ir_file", llvm::errs());
    } else {
        disable_optimizations();
    }
    return *this;
}

RuntimeSpecializer &RuntimeSpecializer::load_ir_string(const std::string &ir_source) {
    auto mem_buf = llvm::MemoryBuffer::getMemBuffer(ir_source);
    M = llvm::parseIR(mem_buf->getMemBufferRef(), Err, Ctx);
    if (!M) {
        llvm::errs() << "Failed to load IR from string\n";
        Err.print("load_ir_string", llvm::errs());
    } else {
        disable_optimizations();
    }
    return *this;
}

RuntimeSpecializer &RuntimeSpecializer::specialize_function(const std::string &demangled_name,
                                                            const std::map<std::string, int> &const_args) {
    llvm::Function *F = find_function_by_demangled_name(demangled_name);
    if (!F) {
        llvm::errs() << "Function not found: " << demangled_name << "\n";
        return *this;
    }

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

    const std::string specialized_name = create_specialized_name(demangled_name, const_args);
    llvm::Function *specialized_func = clone_and_specialize_arguments(F, const_function_args, specialized_name);

    specialize_internal_variables(specialized_func, const_internal_vars);

    // Enable native optimizations on the specialized function
    specialized_func->removeFnAttr(llvm::Attribute::OptimizeNone);
    specialized_func->removeFnAttr(llvm::Attribute::NoInline);
    specialized_func->addFnAttr("target-cpu", CPU);
    specialized_func->addFnAttr("target-features", Features.getString());
    
    llvm::outs() << "Created: " << specialized_name << " (args: " << F->arg_size() << " -> "
                 << specialized_func->arg_size() << ")\n";

    return *this;
}

RuntimeSpecializer &RuntimeSpecializer::optimize() {
    if (!M)
        return *this;

    M->setTargetTriple(target_triple);
    M->setDataLayout(TM->createDataLayout());

    llvm::ModulePassManager MPM =
        llvm::PassBuilder(TM.get()).buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O3);

    MPM.run(*M, MAM);
    return *this;
}

RuntimeSpecializer &RuntimeSpecializer::print_module_ir() {
    if (M)
        M->print(llvm::outs(), nullptr);
    return *this;
}

RuntimeSpecializer &RuntimeSpecializer::print_debug_info() {
    if (!M) {
        llvm::errs() << "No module loaded\n";
        return *this;
    }

    for (auto &F : M->functions()) {
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

std::ptrdiff_t RuntimeSpecializer::compile(const std::string &demangled_name,
                                           const std::map<std::string, int> &const_args) {
    std::string specialized_name = create_specialized_name(demangled_name, const_args);

    if (!find_function_by_demangled_name(specialized_name)) {
        specialize_function(demangled_name, const_args).optimize();
    }

    return compile(specialized_name);
}

std::ptrdiff_t RuntimeSpecializer::compile(const std::string &demangled_name) {
    // Initialize JIT lazily
    if (!JIT) {
        auto jit_or_err = llvm::orc::LLJITBuilder().create();
        if (!jit_or_err) {
            llvm::errs() << "Failed to create JIT\n";
            return 0;
        }
        JIT = std::move(*jit_or_err);
    }

    // Find function
    llvm::Function *F = find_function_by_demangled_name(demangled_name);
    if (!F) {
        llvm::errs() << "Function not found: " << demangled_name << "\n";
        return 0;
    }

    // Clone function into new module
    auto new_ctx = std::make_unique<llvm::LLVMContext>();
    auto new_module = std::make_unique<llvm::Module>("jit_module", *new_ctx);
    new_module->setTargetTriple(M->getTargetTriple());
    new_module->setDataLayout(M->getDataLayout());

    llvm::ValueToValueMapTy VMap;
    llvm::Function *new_func =
        llvm::Function::Create(F->getFunctionType(), F->getLinkage(), F->getName(), new_module.get());
    new_func->copyAttributesFrom(F);

    auto new_arg_it = new_func->arg_begin();
    for (auto &arg : F->args()) {
        new_arg_it->setName(arg.getName());
        VMap[&arg] = &(*new_arg_it);
        ++new_arg_it;
    }

    llvm::SmallVector<llvm::ReturnInst *, 8> returns;
    llvm::CloneFunctionInto(new_func, F, VMap, llvm::CloneFunctionChangeType::LocalChangesOnly, returns);

    // Add to JIT
    std::string func_name = new_func->getName().str();
    auto TSM = llvm::orc::ThreadSafeModule(std::move(new_module), std::move(new_ctx));

    if (auto err = JIT->addIRModule(std::move(TSM))) {
        llvm::errs() << "Failed to add module to JIT\n";
        return 0;
    }

    // Lookup symbol
    auto sym_or_err = JIT->lookup(func_name);
    if (!sym_or_err) {
        llvm::errs() << "Failed to lookup symbol: " << func_name << "\n";
        return 0;
    }

    return sym_or_err->getValue();
}
