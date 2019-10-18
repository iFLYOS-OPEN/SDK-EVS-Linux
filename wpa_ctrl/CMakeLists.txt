cmake_minimum_required(VERSION 2.8)

project(wpa_client)

add_library(wpa_client SHARED wpa_ctrl.c common.c wpa_debug.c wpabuf.c trace.c os_unix.c)

install(TARGETS wpa_client
	LIBRARY DESTINATION lib)
