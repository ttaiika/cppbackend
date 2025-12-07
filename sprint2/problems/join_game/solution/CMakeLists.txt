cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.81.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
	src/main.cpp
	src/http_server.cpp
	src/http_server.h
	src/sdk.h
	src/model.h
	src/model.cpp
	src/tagged.h
	src/boost_json.cpp
	src/json_loader.h
	src/json_loader.cpp
	src/request_handler.h
	src/logger.cpp
	src/logger.h
    src/my_logger.h
	src/logging_request_handler.h
	src/static_handler.cpp      
    src/api_handler.cpp      
	src/game_session.cpp    
	src/players.cpp 
	src/player.cpp 
)
# используем "импортированную" цель CONAN_PKG::boost
target_include_directories(game_server PRIVATE CONAN_PKG::boost)
# target_link_libraries(game_server )
target_link_libraries(game_server PRIVATE Threads::Threads CONAN_PKG::boost)

