add_executable(WsBench websocket_bench.cpp)
target_link_libraries(WsBench
    PRIVATE
        TLSClient
        WebSocket
        StreamConfig
        MarketDataIngester
        OrderBook
        OpenSSL::SSL
        OpenSSL::Crypto
)
