add_executable(extract_faces extract_faces.cpp)
target_include_directories(extract_faces PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(extract_faces PRIVATE ${OpenCV_LIBS} ncnn)

add_executable(generate_embedding_db generate_embedding_db.cpp)
target_include_directories(generate_embedding_db PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(generate_embedding_db PRIVATE ${OpenCV_LIBS} ncnn)