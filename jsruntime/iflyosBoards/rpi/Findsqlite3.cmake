#find_package(PkgConfig)
#pkg_check_modules(SQLITE3 REQUIRED sqlite3)
set(SQLITE3_LDFLAGS "-lsqlite3")#override
set(SQLITE3_LIBRARY "-lsqlite3")#override

#FIND_PATH(SQLITE3_INCLUDE_DIR sqlite3.h ${CMAKE_SYSROOT}/usr)
#FIND_LIBRARY(SQLITE3_LIBRARY libsqlite3.so ${CMAKE_SYSROOT}/usr)
#
#if (SQLITE_INCLUDE_DIR AND SQLITE_LIBRARY)
#    set(SQLITE3_FIND TRUE)
#endif()