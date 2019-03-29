cmake_minimum_required(VERSION 3.0.2)

project(Aquarius)

set (SRCS main.cpp
          fileio_hw.cpp
          httpd.cpp
          hwconfig.cpp
          hwstate.cpp
          hardware.cpp
          i2c_hw.cpp
          logging.cpp
          userdb.cpp)

set (CMAKE_CXX_STANDARD 11)

if (WIN32)
    set (CMAKE_INSTALL_PREFIX "C:/aquarius")
    set (ETC "${CMAKE_INSTALL_PREFIX}/etc")
    # Apparently FindLibXml2 doesn't work on Windows
    set (LIBXML2_DIR "C:/libxml2")
	set (MICROHTTPD_DIR "C:/libmicrohttpd")
    set (LIBXML2_INCLUDE_DIR ${LIBXML2_DIR}/include)
    set (LIBXML2_LIB_DIR ${LIBXML2_DIR}/lib)
	# We don't want to bother about adding libraries to %PATH%
    install(DIRECTORY ${LIBXML2_DIR}/bin/ DESTINATION bin FILES_MATCHING PATTERN *.dll)
	set (MICROHTTPD_LIB_DIR ${MICROHTTPD_DIR}/x86/VS2017/${CMAKE_BUILD_TYPE}-static)
	include_directories(${MICROHTTPD_LIB_DIR})
	link_directories(${MICROHTTPD_LIB_DIR} ${LIBXML2_LIB_DIR})
	if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	    set (MICROHTTPD_LIB_NAME "libmicrohttpd_d")
	else ()
	    set (MICROHTTPD_LIB_NAME "libmicrohttpd")
	endif (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
	set (LIBS ${MICROHTTPD_LIB_NAME} libxml2)
else ()
    set (ETC "/etc")
    find_package(LibXml2 REQUIRED)
    set (LIBS microhttpd xml2)
endif (WIN32)

find_library(WIRINGPI wiringPi)

if (${WIRINGPI} STREQUAL "WIRINGPI-NOTFOUND")
    message ("WARNING: WiringPI support will not be built!")
else ()
    set (SRCS ${SRCS} wiringpi_hw.cpp)
	set (LIBS ${LIBS} wiringPi)
endif (${WIRINGPI} STREQUAL "WIRINGPI-NOTFOUND")

include_directories(${LIBXML2_INCLUDE_DIR})

add_executable(aquarius ${SRCS})
target_link_libraries(aquarius ${LIBS})

install(TARGETS aquarius RUNTIME DESTINATION bin)
install(DIRECTORY etc/ DESTINATION ${ETC})
install(DIRECTORY web/ DESTINATION share/aquarius/web)
