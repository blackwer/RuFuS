cmake_minimum_required(VERSION 3.16)
project(RuFuS LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)
option(RUFUS_LINK_LLVM_SHARED "Link against shared LLVM libraries" OFF)
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

# Examples
if (RUFUS_BUILD_EXAMPLES)
  include(cmake/RuFuS.cmake)
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
target_link_libraries(rufus PUBLIC ${llvm_libs})

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
