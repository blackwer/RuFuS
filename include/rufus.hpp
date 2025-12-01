#ifndef RUFUS_HPP
#define RUFUS_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>

class RuFuS {
  private:
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    RuFuS();
    ~RuFuS();

    // Move constructor/assignment (needed for unique_ptr)
    RuFuS(RuFuS &&) noexcept;
    RuFuS &operator=(RuFuS &&) noexcept;

    // Delete copy (or implement if needed)
    RuFuS(const RuFuS &) = delete;
    RuFuS &operator=(const RuFuS &) = delete;

    RuFuS &load_ir_file(const std::string &ir_file);
    RuFuS &load_ir_string(const std::string &ir_source);
    RuFuS &specialize_function(const std::string &demangled_name, const std::map<std::string, int> &const_args);
    RuFuS &optimize();

    template <typename FuncType>
    FuncType compile(const std::string &demangled_name, const std::map<std::string, int> &const_args) {
        return reinterpret_cast<FuncType>(compile(demangled_name, const_args));
    };

    template <typename FuncType>
    FuncType compile(const std::string &demangled_name) {
        return reinterpret_cast<FuncType>(compile(demangled_name));
    };

    RuFuS &print_module_ir();
    RuFuS &print_debug_info();

  private:
    std::uintptr_t compile(const std::string &demangled_name, const std::map<std::string, int> &const_args);
    std::uintptr_t compile(const std::string &demangled_name);
};

#endif
