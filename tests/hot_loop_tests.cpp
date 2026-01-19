// External
#include <gmock/gmock.h>  // for EXPECT_THAT, HasSubstr, etc
#include <gtest/gtest.h>  // for TEST, ASSERT_NO_THROW, etc

// C++ core
#include <stdexcept>  // for logic_error, invalid_argument, etc

TEST(ExampleTest, TrivialTest) {
  EXPECT_TRUE(true) << "Trivial test failed";
}
