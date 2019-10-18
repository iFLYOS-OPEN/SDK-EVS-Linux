find_package(PkgConfig)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)

set(PORTAUDIO_LIBRARY ${PORTAUDIO_LIBRARIES})