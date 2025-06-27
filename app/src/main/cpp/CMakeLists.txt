cmake_minimum_required(VERSION 3.10)
project(VoiceGenderPavlokMonitor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define paths relative to this CMakeLists.txt (located in app/src/main/cpp)
set(CPP_DIR ${CMAKE_CURRENT_LIST_DIR})
set(PROJECT_ROOT ${CPP_DIR}/..)

# 1) Include directories for headers:
#    - Essentia headers: app/src/main/cpp/include/essentia
#    - Eigen headers:      app/src/main/cpp/include/Eigen and unsupported/Eigen
include_directories(
        ${CPP_DIR}/include
)

# 2) List your JNI wrapper source file(s)
set(SRC_FILES
        ${CPP_DIR}/voice_analysis_engine.cpp
)

# 3) Build the JNI bridge library
add_library(
        voice_analysis_engine
        SHARED
        ${SRC_FILES}
)

# 4) Import the prebuilt Essentia library
add_library(
        essentia
        SHARED
        IMPORTED
)
# Essentia .so should live in app/src/main/jniLibs/<ABI>/libessentia.so
set_target_properties(essentia PROPERTIES
        IMPORTED_LOCATION
        ${PROJECT_ROOT}/jniLibs/${ANDROID_ABI}/libessentia.so
)

# 5) Find Android log library
find_library(
        log-lib
        log
)

# 6) Link Essentia and log to your JNI library
target_link_libraries(
        voice_analysis_engine
        PRIVATE
        essentia
        ${log-lib}
)

# 7) Ensure include path is applied to the JNI library target
#    This allows #include <essentia/...> and #include <Eigen/...>
target_include_directories(
        voice_analysis_engine
        PRIVATE
        ${CPP_DIR}/include
)

# (Optional) Only package specific ABIs in your APK
# set_property(TARGET voice_analysis_engine PROPERTY ANDROID_ABI "arm64-v8a;armeabi-v7a")

# (Optional) Additional compiler flags
# target_compile_options(voice_analysis_engine PRIVATE -fexceptions -frtti)
