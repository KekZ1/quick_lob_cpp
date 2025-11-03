cmake_minimum_required(VERSION 3.29.2)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(lob)

# Create an interface library
# add_library(${CMAKE_PROJECT_NAME} INTERFACE)
add_library(${CMAKE_PROJECT_NAME} STATIC src/lob.cpp)


# Compiler options
target_compile_options(${CMAKE_PROJECT_NAME} PUBLIC# INTERFACE
    $<$<CONFIG:Debug>:-g -O0 -Wall>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -flto -fmarch=native>
)

# Include dirs
set(LIBS_DIR "C:/msys64/ucrt64/include")
target_include_directories(${PROJECT_NAME} PUBLIC# INTERFACE
    include
    ${LIBS_DIR}/boost
    ${LIBS_DIR}/abseil-cpp-master
)

# Add precompiled header
# target_precompile_headers(${CMAKE_PROJECT_NAME} INTERFACE include/lob.hpp)
