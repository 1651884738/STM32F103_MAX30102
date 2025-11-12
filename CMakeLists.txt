cmake_minimum_required(VERSION 3.22)

#
# This file is generated only once,
# and is not re-generated if converter is called multiple times.
#
# User is free to modify the file as much as necessary
#

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)


# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME MAX30102)

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

## 使用软件浮点 ABI，因为 Cortex-M3 没有硬件 FPU
#set(FLOAT_ABI_FLAGS "-mfloat-abi=soft")
#
## 将标志添加到 C 和 C++ 编译选项中
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLOAT_ABI_FLAGS}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLOAT_ABI_FLAGS}")

# Core project settings
project(${CMAKE_PROJECT_NAME})
message("Build type: " ${CMAKE_BUILD_TYPE})

# Enable CMake support for ASM and C languages
enable_language(C ASM)

# 【关键】查找CMSIS-DSP浮点源文件
file(GLOB_RECURSE CMSIS_DSP_SOURCES
        "Drivers/CMSIS/DSP/Source/*/*.c" # 简化写法，包含所有f32源文件
)
list(REMOVE_DUPLICATES CMSIS_DSP_SOURCES) # 避免重复

file(GLOB_RECURSE USER_LIB_SOURCES "lib/oled/src/*.c")

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        lib/oled/src/delay.c
        lib/oled/inc/delay.h
        lib/oled/src/soft_i2c.c
        lib/oled/inc/soft_i2c.h
        lib/oled/src/max30102.c
        lib/oled/inc/max30102.h
        Core/Src/ppg_filter.c
        Core/Inc/ppg_filter.h
        Core/Src/ppg_algorithm.c
        Core/Inc/ppg_algorithm.h
        Core/Src/ppg_algorithm_v2.c
        Core/Inc/ppg_algorithm_v2.h
        #        ${CMSIS_DSP_SOURCES} # 添加DSP源文件
)

# 添加下面这行来启用 printf 的浮点数支持
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE -Wl,-u,_printf_float)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Link directories setup
target_link_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined library search paths
)

# Add sources to executable
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user sources here
        lib/oled/src/oled.c
        lib/oled/src/font.c
        lib/oled/src/delay.c
        lib/oled/src/soft_i2c.c
        lib/oled/src/max30102.c
        Core/Src/ppg_filter.c
        Core/Src/ppg_algorithm.c
        Core/Src/ppg_algorithm_v2.c

)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined include paths
        lib/oled/inc
        Drivers/CMSIS/Include
        Drivers/CMSIS/DSP/Include
        Drivers/STM32F1xx_HAL_Driver/Inc
        Drivers/STM32F1xx_HAL_Driver/Inc/Legacy


)




# Add project symbols (macros)
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # Add user defined symbols
        ARM_MATH_CM3
)

# Remove wrong libob.a library dependency when using cpp files
list(REMOVE_ITEM CMAKE_C_IMPLICIT_LINK_LIBRARIES ob)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    m
    # Add user defined libraries
)
