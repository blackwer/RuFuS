if(NOT DEFINED RUFUS_CMAKE_DIR)
    set(RUFUS_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")
endif()

function(embed_ir_as_header target_name source_file)
    # Parse additional arguments for include directories
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs INCLUDES DEFINITIONS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    set(IR_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.ll)
    set(HEADER_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_ir.h)

    set(CXX_STD ${CMAKE_CXX_STANDARD})
    if(NOT CXX_STD)
        set(CXX_STD 17)
    endif()
    set(COMPILE_FLAGS -std=c++${CXX_STD})

    foreach(dir ${ARG_INCLUDES})
        list(APPEND COMPILE_FLAGS -I${dir})
    endforeach()

    foreach(def ${ARG_DEFINITIONS})
        list(APPEND COMPILE_FLAGS -D${def})
    endforeach()

    foreach(dir ${CMAKE_INCLUDE_PATH})
        list(APPEND COMPILE_FLAGS -I${dir})
    endforeach()

    # Generate IR
    add_custom_command(
        OUTPUT ${IR_FILE}
        COMMAND ${CMAKE_CXX_COMPILER} ${COMPILE_FLAGS}
            -S -emit-llvm -O0
            -fno-discard-value-names
            -DNDEBUG
            ${CMAKE_CURRENT_SOURCE_DIR}/${source_file}
            -o ${IR_FILE}
        DEPENDS ${source_file}
        VERBATIM
    )

    # Convert to header
    add_custom_command(
        OUTPUT ${HEADER_FILE}
        COMMAND ${CMAKE_COMMAND}
            -DIR_FILE=${IR_FILE}
            -DHEADER_FILE=${HEADER_FILE}
            -DVAR_NAME=${target_name}_ir
            -P ${RUFUS_CMAKE_DIR}/embed_ir.cmake
        DEPENDS ${IR_FILE}
        VERBATIM
    )

    add_library(${target_name} INTERFACE)

    set_source_files_properties(${HEADER_FILE} PROPERTIES GENERATED TRUE)
    target_sources(${target_name} INTERFACE ${HEADER_FILE})
    target_include_directories(${target_name} INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR})
endfunction()
