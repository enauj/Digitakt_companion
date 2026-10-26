#pragma once
#include <juce_core/juce_core.h>
#include "Parameters.h"
#include <cmath>
#include <optional>

namespace companion
{
struct PatchState
{
    int engine = 0, page = 0, channel = 9;
    std::array<float, parameterCount> parameters {};
    std::array<int, 8> cc {20, 21, 22, 23, 24, 25, 26, 27};

    juce::var toJson() const
    {
        auto* object = new juce::DynamicObject();
        object->setProperty("schema", "digitakt-companion");
        object->setProperty("version", 1);
        object->setProperty("engine", engine);
        object->setProperty("page", page);
        object->setProperty("channel", channel);
        juce::Array<juce::var> values, mappings;
        for (auto v : parameters) values.add(v);
        for (auto c : cc) mappings.add(c);
        object->setProperty("parameters", values);
        object->setProperty("cc", mappings);
        return juce::var(object);
    }

    static std::optional<PatchState> fromJson(const juce::var& json)
    {
        if (!json.isObject() || json.getProperty("schema", {}).toString() != "digitakt-companion") return {};
        const auto version = json.getProperty("version", {});
        if (!version.isInt() || (int)version != 1) return {};
        const auto paramsOwner = json.getProperty("parameters", {}), ccOwner = json.getProperty("cc", {});
        const auto* values = paramsOwner.getArray();
        const auto* mappings = ccOwner.getArray();
        if (values == nullptr || values->size() != parameterCount || mappings == nullptr || mappings->size() != 8) return {};
        PatchState result;
        for (int i = 0; i < parameterCount; ++i)
        {
            const auto& v = (*values)[i];
            if (!(v.isInt() || v.isInt64() || v.isDouble()) || !std::isfinite((double)v) || (double)v < 0.0 || (double)v > 1.0) return {};
            result.parameters[(size_t)i] = (float)(double)v;
        }
        for (int i = 0; i < 8; ++i)
        {
            const auto& v = (*mappings)[i];
            if (!v.isInt() || (int)v < 0 || (int)v > 119) return {};
            result.cc[(size_t)i] = (int)v;
            for (int j = 0; j < i; ++j) if (result.cc[(size_t)j] == result.cc[(size_t)i]) return {};
        }
        const auto e = json.getProperty("engine", {}), p = json.getProperty("page", {}), ch = json.getProperty("channel", {});
        if (!e.isInt() || (int)e < 0 || (int)e > 2 || !p.isInt() || (int)p < 0 || (int)p > 2 || !ch.isInt() || (int)ch < 1 || (int)ch > 16) return {};
        result.engine = (int)e;
        result.page = (int)p;
        result.channel = (int)ch;
        return result;
    }
};
}
