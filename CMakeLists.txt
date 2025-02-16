cmake_minimum_required(VERSION 3.15)
project(vimDAW)

# Specify JUCE path
set(JUCE_PATH "/home/manslayer/JUCE")
add_subdirectory(${JUCE_PATH} JUCE)

# Define the JUCE application
juce_add_gui_app(vimDAW
    PRODUCT_NAME "vimDAW"
    VERSION "1.0.0"
    SOURCES
        src/main.cpp
        # Add other source files here
)

# Link necessary JUCE modules
target_link_libraries(vimDAW
    PRIVATE
        juce_audio_basics
        juce_audio_devices
        juce_audio_formats
        juce_gui_basics
)

# Set application name
target_compile_definitions(vimDAW PRIVATE JUCE_APPLICATION_NAME="vimDAW")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

