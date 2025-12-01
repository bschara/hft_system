add_library(MDataIngester MDataIngester.cpp)
target_include_directories(MDataIngester PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(MDataIngester PUBLIC OrderBook)
target_link_libraries(MDataIngester PUBLIC BMWebSocket)
