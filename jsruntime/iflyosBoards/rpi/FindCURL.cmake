find_package(PkgConfig)
pkg_check_modules(CURL REQUIRED libcurl)

set(CURL_LIBRARY ${CURL_LIBRARIES})
