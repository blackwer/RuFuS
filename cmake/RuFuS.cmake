set(RUFUS_CMAKE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(embed_ir_as_header target_name source_file)
    # Use absolute paths
    get_filename_component(SOURCE_ABS ${source_file} ABSOLUTE 
        BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
    
    set(IR_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}.ll)
    set(HEADER_FILE ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_ir.h)
    
    # Step 1: Generate IR
    add_custom_command(
        OUTPUT ${IR_FILE}
        COMMAND ${CMAKE_CXX_COMPILER}
            -S -emit-llvm -O0 -g
            -fno-discard-value-names
            -Xclang -disable-llvm-passes
            ${SOURCE_ABS}
            -o ${IR_FILE}
        DEPENDS ${SOURCE_ABS}
        COMMENT "Generating IR for ${source_file}"
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    # Step 2: Convert to header (use RUFUS_CMAKE_DIR)
    add_custom_command(
        OUTPUT ${HEADER_FILE}
        COMMAND ${Python3_EXECUTABLE} 
            ${RUFUS_CMAKE_DIR}/ir_to_header.py
            ${IR_FILE}
            ${HEADER_FILE}
            ${target_name}_ir
        DEPENDS ${IR_FILE} ${RUFUS_CMAKE_DIR}/ir_to_header.py
        COMMENT "Embedding IR as header"
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )
    
    # Step 3: Create interface library (header-only)
    add_library(${target_name}_ir INTERFACE)
    target_sources(${target_name}_ir INTERFACE ${HEADER_FILE})
    target_include_directories(${target_name}_ir INTERFACE 
        ${CMAKE_CURRENT_BINARY_DIR})
    
    # Make sure header is generated
    add_custom_target(${target_name}_ir_generate DEPENDS ${HEADER_FILE})
    add_dependencies(${target_name}_ir ${target_name}_ir_generate)
endfunction()
