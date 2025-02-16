enable_testing()

include(FetchContent)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)
add_executable(test_test test.cpp)
target_link_libraries(test_test PRIVATE GTest::gtest_main)
gtest_discover_tests(test_test)
