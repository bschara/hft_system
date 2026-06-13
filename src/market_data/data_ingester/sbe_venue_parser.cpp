#include "market_data/data_ingester/sbe_venue_parser.h"
#include "market_data/data_ingester/sbe_decoder.h"
#include <cstring>
#include <iostream>

SBEVenueParser::SBEVenueParser(SchemaRegistry schemas, const VenueRegistry &registry,
                                std::string venue_name)
    : schemas_(std::move(schemas)), registry_(registry), venue_name_(std::move(venue_name))
{}

void SBEVenueParser::parse(std::span<const uint8_t>      payload,
                           Common::LFQueue<MarketUpdate> &queue)
{
    size_t cursor = 0;
    while (cursor < payload.size())
    {
        // SBE header: templateId at byte offset 2 (uint16 LE)
        if (cursor + 4 > payload.size()) break;
        uint16_t templateId;
        std::memcpy(&templateId, payload.data() + cursor + 2, sizeof(uint16_t));

        const MessageSchema *schema = schemas_.lookup(templateId);
        if (!schema) {
            std::cerr << "[SBEVenueParser] unknown templateId=" << templateId << "\n";
            break;
        }

        size_t consumed = SBEDecoder::decode(payload.subspan(cursor), *schema,
                                             queue, registry_, venue_name_);
        if (consumed == 0) break;
        cursor += consumed;
    }
}
