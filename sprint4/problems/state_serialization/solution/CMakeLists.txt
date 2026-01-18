cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_TESTS "Build tests" ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup()

set(BOOST_LIB ${CONAN_LIBS_BOOST})
set(CATCH2_LIB ${CONAN_LIBS_CATCH2})

add_library(game_model STATIC
    src/model/model.cpp
    src/model/game_session.cpp
    src/model/dog.cpp
    src/json_loader.cpp
    src/loot_generator.cpp
    src/collision_detector.cpp
    src/boost_json.cpp
    src/app/players.cpp
    src/app/player.cpp
    src/serialization/model_serialization.cpp
)

target_include_directories(game_model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(game_model PUBLIC ${BOOST_LIB})

add_executable(game_server
    src/main.cpp
    src/app/application.cpp
    src/logger/logger.cpp
    src/http_server/http_server.cpp
    src/request_handler/request_handler.cpp
    src/request_handler/api_handler.cpp
    src/infrastructure/serializing_listener.cpp
)

target_link_libraries(game_server game_model)

if(BUILD_TESTS)
    add_executable(game_tests
        tests/collision-detector-tests.cpp
        tests/loot_generator_tests.cpp
		tests/state-serialization-tests.cpp
    )

    target_link_libraries(game_tests ${CATCH2_LIB} game_model)

    include(CTest)

    if(EXISTS "${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake")
        include("${CONAN_BUILD_DIRS_CATCH2_DEBUG}/Catch.cmake")
    elseif(EXISTS "${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake")
        include("${CONAN_BUILD_DIRS_CATCH2}/Catch.cmake")
    else()
        find_file(CATCH_CMAKE Catch.cmake
            PATHS
            ${CMAKE_BINARY_DIR}
            ${CMAKE_SOURCE_DIR}
            PATH_SUFFIXES share/cmake/Catch2
            NO_DEFAULT_PATH
        )
        if(CATCH_CMAKE)
            include(${CATCH_CMAKE})
        endif()
    endif()

    if(COMMAND catch_discover_tests)
        catch_discover_tests(game_tests)
    else()
        add_test(NAME game_tests COMMAND game_tests)
    endif()
endif()