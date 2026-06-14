add_library(WebSocket websocket.cpp)
target_include_directories(WebSocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(WebSocket PUBLIC OpenSSL::SSL OpenSSL::Crypto)