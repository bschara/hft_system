#pragma once

#include <string>
#include "venue_parser.h"
#include "schema_registry.h"
#include "market_data/venue_registry.hpp"

class SBEVenueParser : public VenueParser
{
public:
    SBEVenueParser(SchemaRegistry schemas, const VenueRegistry &registry, std::string venue_name);

    // Outer dispatch loop: reads templateId → looks up schema → calls SBEDecoder::decode
    void parse(std::span<const uint8_t>      payload,
               Common::LFQueue<MarketUpdate> &queue) override;

private:
    SchemaRegistry      schemas_;
    const VenueRegistry &registry_;
    std::string         venue_name_;
};
