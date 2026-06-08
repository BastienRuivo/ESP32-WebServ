#pragma once

#include <ArduinoJson.h>

struct WeightData
{
    unsigned long date;
    float weight;

    static void Jsonify(const WeightData& data, JsonObject& json)
    {
        json["date"] = data.date;
        json["weight"] = data.weight;
    }

    static void Jsonify(const WeightData& data, JsonDocument& json)
    {
        json["date"] = data.date;
        json["weight"] = data.weight;
    }
};