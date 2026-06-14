#include "utils/config/stream_config/stream_config.h"
#include "utils/config/csv.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>

StreamConfig::StreamConfig(const std::string& csvPath) {
    loadCSV(csvPath);
}

const std::vector<StreamRow>& StreamConfig::getRows() const {
    return rows_;
}

void StreamConfig::loadCSV(const std::string& csvPath) {
    io::CSVReader<3> in(csvPath);
    in.read_header(io::ignore_extra_column, "exchange", "symbol", "stream");

    StreamRow r;
    while (in.read_row(r.exchange, r.symbol, r.stream)) {
        std::transform(r.stream.begin(), r.stream.end(),
                       r.stream.begin(), ::tolower);

        rows_.push_back(r);
    }

    if (rows_.empty()) {
        throw std::runtime_error("No streams loaded from CSV");
    }
}

std::string StreamConfig::buildBinanceURL() const {
    std::ostringstream oss;
    bool first = true;

    for (const auto& r : rows_) {
        if (r.exchange != "BINANCE") continue;

        if (!first) oss << "/";
        first = false;

        oss << r.stream;
    }

    return "wss://stream.binance.com:9443/stream?streams=" + oss.str();
}
