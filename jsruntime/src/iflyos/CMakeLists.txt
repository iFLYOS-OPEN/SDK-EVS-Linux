cmake_minimum_required(VERSION 3.0.2 FATAL_ERROR)
project(IFLYOSInterface LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

add_library(IFLYOSInterface STATIC IFLYOSInterface.cpp IFLYOSThreadPool.cpp iFLYOS_IPC.cpp)

add_library(IFLYOSJSExtension STATIC IFLYOSJerryScript.cpp IFLYOSJHelper.cpp)

if(LOG_MSG)
    add_definitions("-DLOG_MSG")
endif()

target_link_libraries(IFLYOSInterface "-pthread")

target_include_directories(IFLYOSInterface PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}")

target_include_directories(IFLYOSJSExtension PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/jerry/jerry-core/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../deps/libtuv/include)
