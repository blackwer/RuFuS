# Setup the unit test executable
set(utest_ex_name "rufus_unit_tests.X")
add_executable(${utest_ex_name} ${CMAKE_CURRENT_SOURCE_DIR}/GTestMain.cpp)

# Unit tests are a separate executable and depend on ${PROJECT_NAME} lib and GoogleTest/GoogleMock
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        release-1.12.1
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Link against gtest_main and rufus
target_link_libraries(${utest_ex_name} PRIVATE 
    rufus
    GTest::gtest
    GTest::gtest_main
    GTest::gmock
    GTest::gmock_main)
target_include_directories(${utest_ex_name} PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/tests
    "${googletest_SOURCE_DIR}/googlemock/include"
    "${googletest_SOURCE_DIR}/googletest/include")

# Add the specific test modules here
embed_ir_as_header(hot_loop hot_loop.cpp)

target_sources(${utest_ex_name} PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/hot_loop_tests.cpp
)
target_link_libraries(${utest_ex_name} PRIVATE hot_loop)

# Post processing: Let GTest parse the executable for tests and register them with CTest
include(GoogleTest)            # CMake >= 3.10
gtest_discover_tests(${utest_ex_name}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DISCOVERY_TIMEOUT 60
    TEST_PREFIX "unit."
)