cmake_minimum_required(VERSION 3.22)

# Test project for Method 1 PPG Pipeline
project(method1_pipeline_test C)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Include math library
find_library(MATH_LIBRARY m)
if(NOT MATH_LIBRARY)
    set(MATH_LIBRARY m)  # Fallback for most systems
endif()

# Create test executable
add_executable(method1_pipeline_test
    method1_pipeline_test.c
    ../Core/Src/ppg_algorithm.c
    ../Core/Src/ppg_filter.c
)

# Include directories
target_include_directories(method1_pipeline_test PRIVATE
    .
    ../Core/Inc
    ../Drivers/CMSIS/Include
)

# Link math library
target_link_libraries(method1_pipeline_test PRIVATE ${MATH_LIBRARY})

# Add compiler definitions for testing
target_compile_definitions(method1_pipeline_test PRIVATE
    TESTING_MODE=1
)

# Enable test discovery
enable_testing()

# Add test
add_test(NAME Method1PipelineTest COMMAND method1_pipeline_test)

# Set test properties
set_tests_properties(Method1PipelineTest PROPERTIES
    TIMEOUT 30
    PASS_REGULAR_EXPRESSION "All Tests Passed"
)