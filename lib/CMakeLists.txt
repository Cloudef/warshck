add_subdirectory(glhck)
add_subdirectory(libwebsockets)
add_subdirectory(glfwhck)


include_directories(
  libsocketio/include 
)

file(GLOB LIBSOCKETIO_SOURCES libsocketio/src/*.c)
add_library(libsocketio ${LIBSOCKETIO_SOURCES})
target_link_libraries(libsocketio uriparser)


list(APPEND CMAKE_CXX_FLAGS -std=c++11)
file(GLOB JSON_SOURCES chck/json/json.c)
add_library(json ${JSON_SOURCES})
