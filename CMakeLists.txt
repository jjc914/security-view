cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME security-view)

project(${PROJECT_NAME})

find_package(OpenCV REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${httplib_INCLUDE_DIR})

message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "OpenCV include path: ${OpenCV_INCLUDE_DIRS}")
