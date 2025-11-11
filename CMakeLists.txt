cmake_minimum_required(VERSION 3.16)
project(RuFuS LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LLVM REQUIRED CONFIG)

option(LLVM_LINK_SHARED "Link against shared LLVM libraries" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries instead of static" OFF)

find_package(Python3 REQUIRED)
include(cmake/RuFuS.cmake)

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

add_library(rufus src/rufus.cpp)
target_include_directories(rufus PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_include_directories(rufus PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(rufus PRIVATE ${LLVM_DEFINITIONS})
target_link_libraries(rufus PUBLIC ${llvm_libs})

add_subdirectory(examples)

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
