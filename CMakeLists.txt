cmake_minimum_required(VERSION 3.20)
project(vimDAW)

# Add JUCE
set(JUCE_DIR "/home/manslayer/JUCE" CACHE PATH "Path to JUCE directory")
add_subdirectory(${JUCE_DIR} JUCE)

# Ensure compile_commands.json is generated
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Ensures no compiler-specific extensions are used

# Find SDL3
find_package(SDL3 REQUIRED CONFIG)

# Find SDL3_ttf
find_package(SDL3_ttf REQUIRED)

find_package(CURL REQUIRED)

# Define your target
add_executable(vimDAW src/main.cpp)

target_link_libraries(vimDAW PRIVATE ${CURL_LIBRARIES})

# Link SDL3 and SDL3_ttf
target_link_libraries(vimDAW PRIVATE SDL3::SDL3-shared SDL3_ttf::SDL3_ttf-shared)

# Link JUCE modules (modify as needed)
target_link_libraries(vimDAW PRIVATE
    juce::juce_core
    juce::juce_gui_basics
    juce::juce_audio_basics
)

# Include JUCE headers
target_include_directories(vimDAW PRIVATE ${JUCE_DIR})

