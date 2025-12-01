add_library(HistoricalDataAggergator historical_aggregator.cpp)

find_package(simdjson CONFIG REQUIRED)
find_package(PostgreSQL REQUIRED)

target_link_libraries(HistoricalDataAggergator PRIVATE simdjson::simdjson PostgreSQL::PostgreSQL)