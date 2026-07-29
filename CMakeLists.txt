cmake_minimum_required(VERSION 3.16)
project(nolimits-nlp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(ZLIB REQUIRED)

add_executable(nolimits-nlp
    src/main.cpp
    src/aes.cpp
    src/endian_tools.cpp
    src/nlpxfile.cpp
    src/nlpzfile.cpp
    src/package.cpp
    src/ziparchive.cpp
    src/zipbuilder.cpp
)

target_include_directories(nolimits-nlp PRIVATE src)
target_link_libraries(nolimits-nlp PRIVATE ZLIB::ZLIB)

if(MSVC)
    target_compile_definitions(nolimits-nlp PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
