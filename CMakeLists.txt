cmake_minimum_required(VERSION 3.6)
project(covert_http)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -lcurl")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin")

set(SERVER_SOURCE_FILES server/main.cpp)
set(CLIENT_SOURCE_FILES client/main.cpp)

add_executable(server.out ${SERVER_SOURCE_FILES})
add_executable(client.out ${CLIENT_SOURCE_FILES})