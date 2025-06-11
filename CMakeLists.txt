cmake_minimum_required(VERSION 3.20)
project(vimDAW)

set(RTAUDIO_INCLUDE_DIR "/usr/include")
set(RTAUDIO_LIBRARY "/usr/lib/librtaudio.so")

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED fluidsynth)
pkg_check_modules(GLFW REQUIRED glfw3)
pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
include_directories(${HARFBUZZ_INCLUDE_DIRS})
include_directories(${GLFW_INCLUDE_DIRS})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Ensures no compiler-specific extensions are used

find_package(SDL3 REQUIRED CONFIG)

find_package(SDL3_ttf REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(RTMIDI REQUIRED rtmidi)
include_directories(${RTMIDI_INCLUDE_DIRS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

include_directories(${PORTAUDIO_INCLUDE_DIRS})
link_directories(${PORTAUDIO_LIBRARY_DIRS})

include_directories(/usr/lib)
link_directories(/usr/lib)

find_package(CURL REQUIRED)

find_package(Freetype REQUIRED)

find_package(OpenGL REQUIRED)

find_path(LUAJIT_INCLUDE_DIR luajit-2.1/lua.hpp)
find_library(LUAJIT_LIB luajit-5.1)

if (NOT LUAJIT_INCLUDE_DIR OR NOT LUAJIT_LIB)
    message(FATAL_ERROR "LuaJIT not found.")
endif()

include_directories(${LUAJIT_INCLUDE_DIR})

find_package(GLEW REQUIRED)
find_package(glm REQUIRED)

add_executable(vimDAW src/render_and_input.cpp src/vimDAW.cpp)

target_link_libraries(vimDAW PRIVATE
    ${CURL_LIBRARIES}
    SDL3::SDL3-shared
    SDL3_ttf::SDL3_ttf-shared
    ${RTAUDIO_LIBRARY}
    ${PORTAUDIO_LIBRARIES}
    ${RTMIDI_LIBRARIES}
    ${FLUIDSYNTH_LIBRARIES}
    ${FREETYPE_LIBRARIES}
    ${GLFW_LIBRARIES}
    OpenGL::GL
    ${HARFBUZZ_LIBRARIES}
    ${LUAJIT_LIB}
    GLEW::GLEW
    glm::glm
)

target_include_directories(vimDAW PRIVATE 
    ${RTAUDIO_INCLUDE_DIR} 
    ${FLUIDSYNTH_INCLUDE_DIRS}
    ${FREETYPE_INCLUDE_DIRS}
    ${GLEW_INCLUDE_DIRS}
    ${GLM_INCLUDE_DIRS}
)
