#include "market_data/historical_data_aggregator/historical_aggregator.h"

HistoricalDataAggergator::HistoricalDataAggergator(HttpClient &client_ref, const char *_conninfo,
                                                   Common::LFQueue<MarketUpdate> *_market_data_queue, OrderBook &_order_book)
    : http_client(client_ref), conninfo(_conninfo), order_book(_order_book)
{
    this->conn = PQconnectdb(this->conninfo);
    this->market_data_queue = _market_data_queue;
}

HistoricalDataAggergator::~HistoricalDataAggergator()
{
    PQfinish(this->conn);
}

void HistoricalDataAggergator::connectToDB()
{
    if (PQstatus(conn) != CONNECTION_OK)
    {
        std::cerr << "❌ Connection failed: " << PQerrorMessage(conn);
        return;
    }

    std::cout << "✅ Connected to TimescaleDB!\n";

    // Check DB time — optional sanity check
    PGresult *res = PQexec(conn, "SELECT NOW();");

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        std::cerr << "❌ Query failed: " << PQerrorMessage(conn);
        PQclear(res);
        return;
    }

    std::cout << "📅 Current DB time: " << PQgetvalue(res, 0, 0) << "\n";
    PQclear(res);

    // ✅ Check if tick_data table exists
    const char *check_table_sql = R"(
        SELECT EXISTS (
            SELECT FROM information_schema.tables 
            WHERE table_name = 'tick_data'
        );
    )";

    res = PQexec(conn, check_table_sql);

    if (PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        std::cerr << "❌ Table existence check failed: " << PQerrorMessage(conn);
        PQclear(res);
        return;
    }

    bool table_exists = std::string(PQgetvalue(res, 0, 0)) == "t";
    PQclear(res);

    if (!table_exists)
    {
        std::cout << "📦 Table 'tick_data' does not exist. Creating...\n";

        const char *create_table_sql = R"(
            CREATE TABLE tick_data (
                time TIMESTAMPTZ NOT NULL,
                side TEXT NOT NULL,
                price DOUBLE PRECISION NOT NULL,
                quantity DOUBLE PRECISION NOT NULL
            );
        )";

        res = PQexec(conn, create_table_sql);

        if (PQresultStatus(res) != PGRES_COMMAND_OK)
        {
            std::cerr << "❌ Failed to create table: " << PQerrorMessage(conn);
            PQclear(res);
            return;
        }
        PQclear(res);

        // 🔁 Convert to hypertable (TimescaleDB)
        const char *hypertable_sql = R"(
            SELECT create_hypertable('tick_data', 'time', if_not_exists => TRUE);
        )";

        res = PQexec(conn, hypertable_sql);
        if (PQresultStatus(res) != PGRES_TUPLES_OK)
        {
            std::cerr << "❌ Failed to create hypertable: " << PQerrorMessage(conn);
        }
        else
        {
            std::cout << "✅ Table 'tick_data' created and converted to hypertable.\n";
        }
        PQclear(res);
    }
    else
    {
        std::cout << "✅ Table 'tick_data' already exists.\n";
    }
}

std::vector<double> HistoricalDataAggergator::computeReturns()
{
    std::vector<double> log_returns;

    if (num_of_elements < 2)
        return log_returns;

    for (int i = 1; i < tick_data.size(); i++)
    {
        if (tick_data[i] > 0 && tick_data[i - 1])
        {
            log_returns.push_back(std::log(tick_data[i] / tick_data[i - 1]));
        }
    }
    return log_returns;
}

void HistoricalDataAggergator::ComputeVolatility()
{

    std::vector<double> log_returns = computeReturns();

    if (num_of_elements == 0)
        this->volatility = 0;

    double sum = 0.0;
    for (double r : log_returns)
        sum += r;
    double mean = sum / num_of_elements;

    double var = 0.0;
    for (double r : log_returns)
    {
        var += (r - mean) * (r - mean);
    }
    var /= num_of_elements;

    std::cerr << "volatility " << std::sqrt(var) << std::endl;
    this->volatility = std::sqrt(var);
}

void HistoricalDataAggergator::run()
{
    if (running == false)
    {
        this->running = true;
    }

    while (running)
    {
        auto const *m_update = this->market_data_queue->getNextToRead();

        if (m_update)
        {
            insert_tick_data();
            ComputeVolatility();
            writeToDB(m_update);
            this->market_data_queue->updateReadIndex();
        }
    }
}

void HistoricalDataAggergator::insert_tick_data()
{
    if (write_index > TICK_DATA_SIZE)
    {
        write_index = 0;
    }

    tick_data[write_index] = order_book.getMidPrice();

    write_index++;

    if (num_of_elements < TICK_DATA_SIZE)
    {
        num_of_elements++;
    }
}

void HistoricalDataAggergator::stop()
{
    this->running = false;
}

double HistoricalDataAggergator::getVolatility()
{
    return volatility;
}

double HistoricalDataAggergator::getAverageDailyVolume()
{
    return average_daily_volume;
}

void HistoricalDataAggergator::writeToDB(const MarketUpdate *_market_update)
{
    if (!conn || PQstatus(conn) != CONNECTION_OK)
    {
        std::cerr << "❌ Not connected to DB.\n";
        return;
    }

    std::time_t ts_seconds = _market_update->_timestamp / 1000; // Assume ms
    char time_buf[64];
    std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", std::gmtime(&ts_seconds));

    const char *side_str = (_market_update->_side == Side::BUY) ? "BUY" : "SELL";

    const char *paramValues[4];
    int paramLengths[4];
    int paramFormats[4] = {0, 0, 0, 0}; // All text format

    std::string priceStr = std::to_string(_market_update->_price);
    std::string qtyStr = std::to_string(_market_update->_quantity);

    paramValues[0] = time_buf;
    paramValues[1] = side_str;
    paramValues[2] = priceStr.c_str();
    paramValues[3] = qtyStr.c_str();

    PGresult *res = PQexecParams(conn,
                                 "INSERT INTO tick_data (time, side, price, quantity) VALUES ($1, $2, $3, $4);",
                                 4,       // # of params
                                 nullptr, // Let PostgreSQL infer types
                                 paramValues,
                                 nullptr, // Param lengths (not needed for text)
                                 paramFormats,
                                 0 // Result format: 0 = text
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK)
    {
        // std::cerr << "❌ Insert failed: " << PQerrorMessage(conn) << std::endl;
    }
    else
    {
        // std::cout << "✅ Market update inserted.\n";
    }

    PQclear(res);
}
