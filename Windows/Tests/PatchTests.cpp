#include "PatchState.h"
#include "AudioDSP.h"
#include <cstdio>
#include <cstdlib>
#include <limits>

static void require(bool ok, const char* text)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", text); std::exit(1); }
}

int main(int argc, char** argv)
{
    companion::PatchState original;
    original.engine = 2; original.page = 1; original.channel = 14;
    original.cc = {48,49,50,51,52,53,54,55};
    for (int i = 0; i < companion::parameterCount; ++i) original.parameters[(size_t)i] = (float)i / 40.0f;
    auto decoded = companion::PatchState::fromJson(juce::JSON::parse(juce::JSON::toString(original.toJson())));
    require(decoded.has_value(), "parse serialized patch");
    require(decoded->engine == 2 && decoded->page == 1 && decoded->channel == 14 && decoded->cc == original.cc, "retain engine/page/channel/CC map");
    for (int i = 0; i < companion::parameterCount; ++i)
        require(std::abs(decoded->parameters[(size_t)i] - original.parameters[(size_t)i]) < .000001f, "retain every engine's parameters");
    auto invalid = original.toJson(); invalid.getDynamicObject()->setProperty("version", 2);
    require(!companion::PatchState::fromJson(invalid), "reject future versions");
    invalid = original.toJson(); invalid.getDynamicObject()->setProperty("version", "1");
    require(!companion::PatchState::fromJson(invalid), "reject incorrectly typed versions");
    invalid = original.toJson(); invalid.getDynamicObject()->setProperty("schema", "other-app");
    require(!companion::PatchState::fromJson(invalid), "reject other file schemas");
    for (const auto* field : {"engine", "page", "channel"})
    {
        invalid = original.toJson(); invalid.getDynamicObject()->setProperty(field, -1);
        require(!companion::PatchState::fromJson(invalid), "reject invalid selections");
    }
    auto duplicate = original; duplicate.cc[1] = duplicate.cc[0];
    require(!companion::PatchState::fromJson(duplicate.toJson()), "reject ambiguous CC assignments");
    auto badRange = original; badRange.parameters[12] = 1.01f;
    require(!companion::PatchState::fromJson(badRange.toJson()), "reject out of range parameters");
    badRange.parameters[12] = std::numeric_limits<float>::quiet_NaN();
    require(!companion::PatchState::fromJson(badRange.toJson()), "reject non-finite parameters");
    invalid = original.toJson(); invalid.getDynamicObject()->setProperty("parameters", juce::Array<juce::var>{0.1, 0.2});
    require(!companion::PatchState::fromJson(invalid), "reject incomplete parameter state");
    require(!companion::PatchState::fromJson(juce::JSON::parse("{broken")), "reject malformed JSON");
    if (argc == 2)
    {
        auto factory = companion::PatchState::fromJson(juce::JSON::parse(juce::File(juce::String::fromUTF8(argv[1]))));
        require(factory.has_value(), "factory patch validates");
        auto* synth = synth_create();
        for (int i = 0; i < companion::parameterCount; ++i)
            require(std::abs(factory->parameters[(size_t)i] - synth_get_parameter(synth, i)) < .000001f, "factory patch matches shared DSP defaults");
        synth_destroy(synth);
    }
    std::puts("PASS: JSON round trip, all 40 parameters, typed schema validation, invalid state rejection, factory defaults");
}
