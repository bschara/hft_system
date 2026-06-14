add_library(MarketDataIngester
    market_data_ingester.cpp
    sbe_decoder.cpp
    sbe_venue_parser.cpp
)
target_include_directories(MarketDataIngester PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(MarketDataIngester PUBLIC OrderBook WebSocket)
