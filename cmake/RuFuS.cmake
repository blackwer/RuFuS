if(NOT DEFINED RUFUS_CMAKE_DIR)
    set(RUFUS_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR} CACHE INTERNAL "")
endif()

function(embed_ir_as_header target_name source_file)
    set(IR_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.ll)
    set(HEADER_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_ir.h)

    # Step 1: Generate IR
    add_custom_command(
        OUTPUT ${IR_FILE}
        COMMAND ${CMAKE_CXX_COMPILER}
            -S -emit-llvm -O0
            -fno-discard-value-names
            -DNDEBUG
            ${CMAKE_CURRENT_SOURCE_DIR}/${source_file}
            -o ${IR_FILE}
        DEPENDS ${source_file}
        VERBATIM
    )

    # Step 2: Convert to header (use RUFUS_CMAKE_DIR)
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

    # Custom target to tie it together
    add_library(${target_name}_ir INTERFACE)

    set_source_files_properties(${HEADER_FILE} PROPERTIES GENERATED TRUE)
    target_sources(${target_name}_ir INTERFACE ${HEADER_FILE})
    target_include_directories(${target_name}_ir INTERFACE
        ${CMAKE_CURRENT_BINARY_DIR})
endfunction()
