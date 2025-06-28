# CMakeLists.txt for Android project with Essentia integration

cmake_minimum_required(VERSION 3.18.1)

# Project name
project("VoiceGenderPavlokMonitor")

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable debug information for debugging
set(CMAKE_BUILD_TYPE Debug)

# Add compiler flags for better performance and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2 -ffast-math")

# Platform-specific settings
if(ANDROID)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frtti -fexceptions")
endif()

# Define source files
set(NATIVE_SOURCES
        essentia_jni.cpp
        essentia_wrapper.cpp
)

# Define header directories
set(ESSENTIA_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/essentia)
set(NATIVE_INCLUDE_DIR ${CMAKE_SOURCE_DIR})
set(EIGEN_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/eigen)

# Include directories
include_directories(
        ${ESSENTIA_INCLUDE_DIR}
        ${NATIVE_INCLUDE_DIR}
        ${EIGEN_INCLUDE_DIR}
)

# Pre-built Essentia library configuration
add_library(essentia STATIC IMPORTED)

# Set the location of the pre-built Essentia library for different architectures
set_target_properties(essentia PROPERTIES
        IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libessentia.a
)

# Optional: Set different library locations for different architectures
# if(${ANDROID_ABI} STREQUAL "arm64-v8a")
#     set_target_properties(essentia PROPERTIES
#         IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/arm64-v8a/libessentia.so)
# elseif(${ANDROID_ABI} STREQUAL "armeabi-v7a")
#     set_target_properties(essentia PROPERTIES
#         IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/armeabi-v7a/libessentia.so)
# elseif(${ANDROID_ABI} STREQUAL "x86")
#     set_target_properties(essentia PROPERTIES
#         IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/x86/libessentia.so)
# elseif(${ANDROID_ABI} STREQUAL "x86_64")
#     set_target_properties(essentia PROPERTIES
#         IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/../jniLibs/x86_64/libessentia.so)
# endif()

# Create the main native library
add_library(essentia_wrapper SHARED ${NATIVE_SOURCES})

# Link libraries
target_link_libraries(essentia_wrapper
        # Android system libraries
        android
        log

        # Essentia library
        essentia

        # Standard C++ libraries that Essentia might need
        atomic
)

# Optional: Add additional system libraries that Essentia might depend on
# Uncomment these if Essentia requires them
# target_link_libraries(essentia_wrapper
#     m           # Math library
#     dl          # Dynamic loading
#     pthread     # Threading
# )

# Compiler-specific options
target_compile_options(essentia_wrapper PRIVATE
        -fvisibility=hidden
        -ffunction-sections
        -fdata-sections
)

# Linker options
set_target_properties(essentia_wrapper PROPERTIES
        LINK_FLAGS "-Wl,--gc-sections -Wl,--exclude-libs,ALL"
)

# Optional: Set RPATH for finding shared libraries at runtime
# set_target_properties(essentia_wrapper PROPERTIES
#     BUILD_RPATH_USE_ORIGIN TRUE
#     INSTALL_RPATH "$ORIGIN"
# )

# Debug configuration
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(essentia_wrapper PRIVATE DEBUG=1)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
else()
    target_compile_definitions(essentia_wrapper PRIVATE NDEBUG=1)
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# Optional: Print configuration info
message(STATUS "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "ANDROID_ABI: ${ANDROID_ABI}")
message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "Essentia library location: ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libessentia.so")

# Optional: Check if Essentia library exists
if(EXISTS "${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libessentia.so")
    message(STATUS "✓ Essentia library found for ${ANDROID_ABI}")
else()
    message(WARNING "✗ Essentia library NOT found at: ${CMAKE_SOURCE_DIR}/../jniLibs/${ANDROID_ABI}/libessentia.so")
endif()

# Optional: Check if Essentia headers exist
if(EXISTS "${ESSENTIA_INCLUDE_DIR}")
    message(STATUS "✓ Essentia headers found at: ${ESSENTIA_INCLUDE_DIR}")
else()
    message(WARNING "✗ Essentia headers NOT found at: ${ESSENTIA_INCLUDE_DIR}")
endif()

# Optional: Check if Eigen headers exist
if(EXISTS "${EIGEN_INCLUDE_DIR}")
    message(STATUS "✓ Eigen headers found at: ${EIGEN_INCLUDE_DIR}")
else()
    message(WARNING "✗ Eigen headers NOT found at: ${EIGEN_INCLUDE_DIR}")
endif()

# Check specifically for Eigen Tensor module
if(EXISTS "${EIGEN_INCLUDE_DIR}/unsupported/Eigen/CXX11/Tensor")
    message(STATUS "✓ Eigen Tensor module found")
else()
    message(WARNING "✗ Eigen Tensor module NOT found at: ${EIGEN_INCLUDE_DIR}/unsupported/Eigen/CXX11/Tensor")
endif()