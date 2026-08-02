cmake_minimum_required(VERSION 3.10)
project(MiniCOBOL VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    main.cpp
    lexer.cpp
    pic_descriptor.cpp
    parser.cpp
    semantic_analyzer.cpp
)

# Header files
set(HEADERS
    token.h
    ast.h
    pic_descriptor.h
    lexer.h
    parser.h
    semantic_analyzer.h
    code_generator.h
)

# Create executable
add_executable(minicobol ${SOURCES} ${HEADERS})

# Enable warnings
if(MSVC)
    target_compile_options(minicobol PRIVATE /W4)
else()
    target_compile_options(minicobol PRIVATE -Wall -Wextra -Wpedantic -O3)
endif()

# Installation
install(TARGETS minicobol DESTINATION bin)
