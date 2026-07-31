//! \file GTestMain.cpp
/// \brief Main function for all GoogleTest-driven tests within this library
///
/// Simply link this file with the test files you want to run. GTest will automatically find all the tests in the
/// linked files and run them.

// External
#include <gmock/gmock.h> // for EXPECT_THAT, HasSubstr, etc
#include <gtest/gtest.h> // for TEST, ASSERT_NO_THROW, etc

int main(int argc, char **argv) {
    testing::InitGoogleMock(&argc, argv);
    int return_val = RUN_ALL_TESTS();
    return return_val;
}