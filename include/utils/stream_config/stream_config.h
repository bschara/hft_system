#pragma once
#include <string>
#include <vector>

struct StreamRow {
    std::string exchange;
    std::string symbol;
    std::string stream;
};

class StreamConfig {
public:
    StreamConfig(const std::string& csvPath);

    const std::vector<StreamRow>& getRows() const;

    std::string buildBinanceURL() const;

private:
    std::vector<StreamRow> rows_;
    void loadCSV(const std::string& csvPath);
};
