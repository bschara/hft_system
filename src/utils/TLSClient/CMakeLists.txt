add_library(TLSClient TLSClient.cpp)
target_include_directories(TLSClient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(TLSClient PUBLIC OpenSSL::SSL OpenSSL::Crypto)