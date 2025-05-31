cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME security-view)

project(${PROJECT_NAME})

set(ncnn_DIR "/home/joshpi/ncnn/build/install/lib/cmake/ncnn")

find_package(OpenCV REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ncnn REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE ${OpenCV_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} vendor/cpp-httplib)
target_link_libraries(${PROJECT_NAME} PRIVATE ${OpenCV_LIBS} OpenSSL::SSL OpenSSL::Crypto ncnn)
