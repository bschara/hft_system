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
    src/market_data/order_book
    src/market_data/data_ingester
    src/market_data/historical_data_aggregator
    src/utils/websocket
    src/utils/tls_client
    src/utils/http_client
    src/utils/stream_config
    src/order_gateway
    src/strategies
    src/pcm_model
    src/oms
)

foreach(dir ${SUBMODULE_DIRS})
    add_subdirectory(${dir})
endforeach()

add_executable(MainExec src/main.cpp)

target_link_libraries(MainExec
    PRIVATE
        MarketDataIngester
        WebSocket
        OrderGateway
        TLSClient
        HttpClient
        StreamConfig
        MidPriceReversion
        HistoricalDataAggergator
        PCModel
        OpenSSL::SSL
        OpenSSL::Crypto
        simdjson::simdjson
        PostgreSQL::PostgreSQL
        OrderManagementSystem
)
