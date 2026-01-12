cmake_minimum_required(VERSION 3.13)
project(game_server)

# Явно указываем C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Подключение Conan
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

# Все исходные файлы
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

add_executable(game_server ${SOURCES} ${HEADERS})

# Включаем директорию src для include
target_include_directories(game_server PRIVATE ${CMAKE_SOURCE_DIR}/src)

# Связывание с библиотеками через Conan
conan_target_link_libraries(game_server)