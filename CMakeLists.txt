cmake_minimum_required(VERSION 3.10)
project(hft_system)

set(CMAKE_CXX_STANDARD 20)

set(QUICKFIX_ROOT "/workspace/quickfix/build")

include_directories(
    ${PROJECT_SOURCE_DIR}/include
    ${VCPKG_ROOT}/installed/x64-linux/include
    ${QUICKFIX_ROOT}/include
)

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(simdjson CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

set(SUBMODULE_DIRS
    src/benchmarks
    src/market_data/order_book
    src/market_data/data_ingester
    src/market_data/historical_data_aggregator
    src/utils/net/websocket
    src/utils/net/tls_client
    src/utils/net/http_client
    src/utils/config/stream_config
    src/order_gateway
    src/strategies
    src/pcm_model
    src/oms
)

foreach(dir ${SUBMODULE_DIRS})
    add_subdirectory(${dir})
endforeach()

option(BUILD_TESTING "Build unit tests" ON)
if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

add_executable(MainExec src/main.cpp)

target_link_libraries(MainExec
    PRIVATE
        MarketDataIngester
        WebSocket
        OrderBook
        OrderGateway
        TLSClient
        HttpClient
        StreamConfig
        MidPriceReversion
        OrderBookImbalance
        MicroMomentum
        HistoricalDataAggergator
        PCModel
        OpenSSL::SSL
        OpenSSL::Crypto
        simdjson::simdjson
        PostgreSQL::PostgreSQL
        OrderManagementSystem
)
