add_library(OrderGateway order_gateway.cpp)
target_include_directories(OrderGateway PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# target_link_libraries(OrderGateway PUBLIC quickfix OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(OrderGateway PUBLIC OpenSSL::SSL OpenSSL::Crypto)