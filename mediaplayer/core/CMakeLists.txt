cmake_minimum_required(VERSION 3.0)
project(android-system-core)

INCLUDE(GNUInstallDirs)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
add_subdirectory(libcutils)
add_subdirectory(libutils)

#FILE(GLOB INCLUDE_FILES "include/cutils/*.h")
#INSTALL(FILES ${INCLUDE_FILES} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cutils)

FILE(GLOB INCLUDE_FILES "include/utils/*.h")
INSTALL(FILES ${INCLUDE_FILES} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/utils)
