#pragma once

#include <unordered_map>
#include "message_schema.h"

class SchemaRegistry
{
public:
    void registerSchema(MessageSchema s)
    {
        schemas_[s.template_id] = s;
    }

    const MessageSchema *lookup(uint16_t template_id) const
    {
        auto it = schemas_.find(template_id);
        return (it != schemas_.end()) ? &it->second : nullptr;
    }

private:
    std::unordered_map<uint16_t, MessageSchema> schemas_;
};
