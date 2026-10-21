cmake_minimum_required(VERSION 3.20)
project(apexrace VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SEEDED_BUGS "Enable seeded intentional logic bugs" OFF)

if(ENABLE_SEEDED_BUGS)
    message(STATUS "ApexRace: Compiling with seeded intentional bugs ENABLED")
    add_compile_definitions(SEED_BUG_1 SEED_BUG_2 SEED_BUG_3 SEED_BUG_4 SEED_BUG_5 SEED_BUG_6)
else()
    message(STATUS "ApexRace: Compiling with clean/fixed service logic")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

if(WIN32)
    set(SYSTEM_LIBS ws2_32)
else()
    set(SYSTEM_LIBS pthread)
endif()

add_library(core_services STATIC
    src/service/timing_engine.cpp
    src/service/radio_parser.cpp
    src/service/pit_strategy.cpp
    src/service/incident_queue.cpp
    src/service/steward_decisions.cpp
)

add_executable(server src/main.cpp)
target_link_libraries(server PRIVATE core_services ${SYSTEM_LIBS})

add_executable(challenge_tests tests/test_suite.cpp)
target_link_libraries(challenge_tests PRIVATE core_services ${SYSTEM_LIBS})
