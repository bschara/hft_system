add_library(BMWebSocket BMWebSocket.cpp)
target_include_directories(BMWebSocket PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(BMWebSocket PUBLIC OpenSSL::SSL OpenSSL::Crypto)