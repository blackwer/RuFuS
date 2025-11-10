cmake_minimum_required(VERSION 3.16)
project(RuFuS LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

option(LLVM_LINK_SHARED "Link against shared LLVM libraries" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)

if (LLVM_LINK_SHARED)
  set(llvm_libs LLVM)
else()
  llvm_map_components_to_libnames(llvm_libs
    Core Support IRReader BitReader BitWriter
    Analysis Passes TransformUtils InstCombine
    AggressiveInstCombine Vectorize IPO
    Target MC MCParser MCDisassembler
    X86CodeGen X86AsmParser X86Desc X86Info X86Disassembler
    OrcJIT OrcShared OrcTargetProcess ExecutionEngine
    MCJIT RuntimeDyld JITLink
    Demangle BinaryFormat Remarks BitstreamReader
    TextAPI ProfileData
  )
endif()

function(add_llvm_ir_target source_file)
    get_filename_component(basename ${source_file} NAME_WE)
    set(output_file ${CMAKE_CURRENT_BINARY_DIR}/${basename}.ll)
    add_custom_command(
        OUTPUT ${output_file}
        COMMAND clang++ -S -emit-llvm -O0 -g -Xclang -disable-llvm-passes -fno-discard-value-names
                ${CMAKE_CURRENT_SOURCE_DIR}/${source_file}
                -o ${output_file}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source_file}
        COMMENT "Generating IR: ${basename}.ll"
        VERBATIM
    )
    set(${basename}_IR ${output_file} PARENT_SCOPE)
endfunction()

add_llvm_ir_target(src/hot_loop.cpp)
add_custom_target(generate_ir ALL DEPENDS ${hot_loop_IR})

add_library(rufus src/rufus.cpp)
target_include_directories(rufus PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(rufus PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(rufus PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(rufus PUBLIC ${llvm_libs})

# Proof of concept executable
add_executable(demo examples/main.cpp)
target_include_directories(demo PRIVATE include)
target_link_libraries(demo rufus)
add_dependencies(demo generate_ir)

# Install and export
install(TARGETS rufus
    EXPORT RuFuSTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(EXPORT RuFuSTargets
    FILE RuFuSTargets.cmake
    NAMESPACE RuFuS::
    DESTINATION lib/cmake/RuFuS
)

# Create config file that finds LLVM
include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/RuFuSConfig.cmake
    INSTALL_DESTINATION lib/cmake/RuFuS
)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/RuFuSConfig.cmake
    DESTINATION lib/cmake/RuFuS
)
