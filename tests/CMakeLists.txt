cmake_minimum_required(VERSION 3.20)
project(Beout_OSTests)

# GTest is provided via FetchContent in root CMakeLists.txt
add_executable(run_all_tests 
    main_test.cpp
    provisioning_test.cpp
    ../provisioning/cli_engine.cpp
    crypto_test.cpp
    activation_test.cpp
)

target_link_libraries(run_all_tests
    gtest_main
    gtest
    pthread
    database
    crypto
    activation
)

include(GoogleTest)
gtest_discover_tests(run_all_tests)
