cmake_minimum_required(VERSION 3.18)
project(TokenStatus VERSION 1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "TokenStatus is a Windows Notepad++ plugin; use a Windows C++ toolchain.")
endif()

add_library(TokenStatus SHARED src/TokenStatus.cpp)
target_compile_features(TokenStatus PRIVATE cxx_std_17)
target_compile_definitions(TokenStatus PRIVATE UNICODE _UNICODE)
target_link_libraries(TokenStatus PRIVATE user32 comctl32)

if(MSVC)
  target_compile_options(TokenStatus PRIVATE /W4)
endif()

set_target_properties(TokenStatus PROPERTIES PREFIX "" OUTPUT_NAME "TokenStatus")
