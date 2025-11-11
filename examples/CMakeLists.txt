embed_ir_as_header(hot_loop hot_loop.cpp)

# Proof of concept executable
add_executable(demo main.cpp)
target_include_directories(demo PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(demo rufus hot_loop_ir)
