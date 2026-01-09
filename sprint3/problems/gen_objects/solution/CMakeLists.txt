cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# Threads
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Sources
file(GLOB_RECURSE ALL_SRCS CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/src/*.cpp
)

# Server 
add_executable(game_server ${ALL_SRCS})
target_include_directories(game_server PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(game_server PRIVATE
    Threads::Threads
    CONAN_PKG::boost
)

set_target_properties(game_server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Tests
# Исключаем main.cpp
set(TEST_SOURCES "")
foreach(SRC ${ALL_SRCS})
    if(NOT SRC MATCHES ".*main\\.cpp$")
        list(APPEND TEST_SOURCES ${SRC})
    endif()
endforeach()

file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/tests/*.cpp
)
list(APPEND TEST_SOURCES ${TEST_FILES})

if(TEST_FILES)
    add_executable(tests ${TEST_SOURCES})
    target_include_directories(tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

    target_link_libraries(tests PRIVATE
        Threads::Threads
        CONAN_PKG::catch2
        CONAN_PKG::boost
    )

    set_target_properties(tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
endif()