project(Aquarius)

set (SRCS main.cpp
          fileio_hw.cpp
          httpd.cpp
          hwconfig.cpp
          hwstate.cpp
          hardware.cpp
          i2c_hw.cpp
          logging.cpp
          userdb.cpp
          wiringpi_hw.cpp)

set (LIBS microhttpd wiringPi xml2)

set (CMAKE_CXX_STANDARD 11)
find_package(LibXml2 REQUIRED)

include_directories(${LIBXML2_INCLUDE_DIR})

add_executable(aquarius ${SRCS})
target_link_libraries(aquarius ${LIBS})

install(TARGETS aquarius RUNTIME DESTINATION bin)
install(DIRECTORY etc/ DESTINATION /etc)
install(DIRECTORY web/ DESTINATION share/aquarius/web)
