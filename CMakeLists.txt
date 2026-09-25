cmake_minimum_required(VERSION 3.13)
project(raw C CXX)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DAUTO_DETECT_PLATFORM)
add_definitions(-DBYPASS_PROTECTION)
set(CMAKE_CXX_FLAGS " -Os -g -fno-rtti -fno-exceptions -Wall -Wno-unknown-pragmas -Wshadow -Wundef -Wwrite-strings -Wnon-virtual-dtor -Wno-multichar")

set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/saturn/src)
set(HOST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/saturn/host)

file(GLOB_RECURSE RAW_SOURCES ${SRC_ROOT}/*.cxx ${SRC_ROOT}/*.c)
list(FILTER RAW_SOURCES EXCLUDE REGEX "/src/system/")
file(GLOB HOST_SOURCES ${HOST_ROOT}/*.cxx)

file(GLOB SRC_ENTRIES LIST_DIRECTORIES true ${SRC_ROOT}/*)
set(SRC_DIRS "")
foreach(entry ${SRC_ENTRIES})
  if(IS_DIRECTORY ${entry} AND NOT entry MATCHES "/src/system$")
    list(APPEND SRC_DIRS ${entry})
  endif()
endforeach()

add_executable(raw ${RAW_SOURCES} ${HOST_SOURCES})
target_include_directories(raw PRIVATE ${SRC_ROOT} ${SRC_DIRS} ${HOST_ROOT})

find_package(SDL2 REQUIRED)
target_include_directories(raw PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(raw ${SDL2_LIBRARIES})
