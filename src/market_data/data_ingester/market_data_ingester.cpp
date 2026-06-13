#include "market_data/data_ingester/market_data_ingester.h"
#include <iostream>

MarketDataIngester::MarketDataIngester(Common::LFQueue<MarketUpdate> &mDataQueue,
                                       utility::TLSClient            &tlsClient,
                                       utility::WebSocket            &socketClient,
                                       VenueParser                   &parser)
    : updatesQueue(mDataQueue), tls_client(tlsClient),
      web_socket(socketClient), parser_(parser)
{}

MarketDataIngester::~MarketDataIngester()
{
    running = false;
}

void MarketDataIngester::startReceiving(std::string_view path, std::string_view protocol)
{
    if (!running) running = true;

    if (!tls_client.connect()) {
        std::cerr << "TLS connection failed\n";
        return;
    }

    if (!web_socket.perform_handshake(path, protocol)) {
        std::cerr << "WebSocket handshake failed\n";
        return;
    }

    while (running)
    {
        auto frame = web_socket.read_frame();
        if (!frame) continue;

        if (frame->opcode == 0x2) // binary frame
            parser_.parse(frame->payload, updatesQueue);
    }
}

void MarketDataIngester::stopReceiving()
{
    running = false;
}
