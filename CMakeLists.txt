cmake_minimum_required(VERSION 3.16)
project(RuFuS LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(RUFUS_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake CACHE INTERNAL "")

option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)
option(RUFUS_LINK_LLVM_SHARED "Link against shared LLVM libraries" OFF)
option(RUFUS_BUILD_TESTS "Build RuFuS tests" ON)
option(RUFUS_BUILD_EXAMPLES "Build RuFuS examples" ON)

# Reporting
if(BUILD_SHARED_LIBS AND NOT RUFUS_LINK_LLVM_SHARED)
  message(STATUS
    "NOTE: Building librufus.so with static LLVM embedded. "
    "This is valid but creates a large binary (~75MB+).\n"
    "         For smaller binaries, but a runtime dependency on LLVM, set RUFUS_LINK_LLVM_SHARED=ON")
endif()

message(STATUS "RuFuS Configuration:")
if(BUILD_SHARED_LIBS)
  message(STATUS "  Library type: SHARED")
else()
  message(STATUS "  Library type: STATIC")
endif()

if(RUFUS_LINK_LLVM_SHARED)
  message(STATUS "  LLVM linking: SHARED")
else()
  message(STATUS "  LLVM linking: STATIC")
endif()

# Dependency wrangling
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")

# Find the Clang that matches this LLVM installation
find_program(LLVM_CLANG_EXECUTABLE
    NAMES clang++-${LLVM_VERSION_MAJOR} clang++
    HINTS ${LLVM_TOOLS_BINARY_DIR}
    DOC "Clang compiler for generating IR"
)

if(NOT LLVM_CLANG_EXECUTABLE)
    message(FATAL_ERROR "Could not find clang++ matching LLVM ${LLVM_PACKAGE_VERSION}")
endif()

message(STATUS "Using Clang: ${LLVM_CLANG_EXECUTABLE}")

# Make it available the IR builder
set(RUFUS_CLANG_EXECUTABLE ${LLVM_CLANG_EXECUTABLE} CACHE INTERNAL "")

if (RUFUS_LINK_LLVM_SHARED)
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

# Include custom CMake functions
include(cmake/RuFuS.cmake)

# Tests
if (RUFUS_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Examples
if (RUFUS_BUILD_EXAMPLES)
  # add_executable(demo examples/main.cpp)  # A limitations exists in embed_ir_as_header, whereby we must create the executable within the same CMakeLists.txt as embed_ir_as_header
  add_subdirectory(examples)
endif()

# Define the library
add_library(rufus src/rufus.cpp)
target_include_directories(rufus PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_include_directories(rufus PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(rufus PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(rufus PRIVATE ${llvm_libs})

# NEW: Add alias and export cmake dir
add_library(RuFuS::rufus ALIAS rufus)
set(RUFUS_CMAKE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cmake CACHE INTERNAL "")
