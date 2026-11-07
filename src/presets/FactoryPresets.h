#pragma once
// Factory preset data. Each preset lists only what differs from the parameter
// defaults (real units: Hz, ms, s, 0..1 for percentages, 0/1 for switches,
// index for choices).
//
// Macro routes come from a standard kit built from the stages each preset
// uses (see FactoryPresets.cpp), so every preset is playable from the four
// macros; a preset adds its own signature routes on top.

#include "../Parameters.h"

#include <array>
#include <vector>

namespace vessel
{

struct Route
{
    Src src;
    P dst;
    float amount;
};

struct FactoryPreset
{
    const char* name;
    const char* category;
    const char* description;
    std::vector<std::pair<P, float>> values;
    std::vector<Route> routes;
    bool macroKit = true;    // add the standard macro routes
};

const std::vector<FactoryPreset>& factoryPresets();

// The standard macro routes for a set of stages (also used by the randomizer).
std::vector<Route> macroKit (const std::array<bool, kNumStages>& stagesUsed);

} // namespace vessel
