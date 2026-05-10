cmake_minimum_required(VERSION 3.15)
project(myicc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(myicc
    src/main.cpp
    src/nvapi_wrapper.cpp
    src/color_controller.cpp
    src/gamma.cpp
    src/color_enhance.cpp
)

target_include_directories(myicc PRIVATE thirdparty/nvapi src)

target_link_libraries(myicc
    user32
    gdi32
    comctl32
    psapi
)

# Statically link CRT for portability
set_property(TARGET myicc PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
