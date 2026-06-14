add_library(HttpClient http_client.cpp)
find_package(CURL REQUIRED)
target_include_directories(HttpClient PUBLIC ${CURL_INCLUDE_DIRS})
target_link_libraries(HttpClient PUBLIC ${CURL_LIBRARIES})
