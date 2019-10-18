cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0" )

include(GNUInstallDirs)
project(rkplayer)

add_subdirectory(mutils)
#add_subdirectory(flora)
add_subdirectory(rklog)
#add_subdirectory(core)
add_subdirectory(librplayer)


set(CMAKE_CXX_STANDARD 14)

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -save-temps=obj")
add_executable(RMediaPlayer main.cpp iFLYOS_IPC.cpp)
target_link_libraries(RMediaPlayer rplayer)
target_include_directories(RMediaPlayer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/librplayer/include)

install(TARGETS RMediaPlayer
        RUNTIME DESTINATION bin)