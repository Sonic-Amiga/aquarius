set (SRCS main.cpp
          httpd.cpp
          hwconfig.cpp
          hwstate.cpp
          hardware.cpp
          i2c_hw.cpp
          logging.cpp
          owfs_hw.cpp
          userdb.cpp
          wiringpi_hw.cpp)

set (LIBS microhttpd wiringPi)

set (CMAKE_CXX_STANDARD 11)

project(Aquarius)
add_executable(aquarius ${SRCS})
target_link_libraries(aquarius ${LIBS})

install(TARGETS aquarius RUNTIME DESTINATION bin)
install(DIRECTORY etc/ DESTINATION /etc)
install(DIRECTORY web/ DESTINATION share/aquarius/web)
