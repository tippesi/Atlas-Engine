#include "StreamingBindings.h"
#include "MemberBindings.h"

#include "streaming/StreamingManager.h"

namespace Atlas::Scripting::Bindings {

    void GenerateStreamingBindings(sol::table* ns) {

        using Settings = Streaming::StreamingSettings;

        ns->new_usertype<Settings::Levels>("LevelStreamingSettings",
            "enable", &Settings::Levels::enable,
            "budget", &Settings::Levels::budget,
            "minResolution", &Settings::Levels::minResolution,
            "resolutionScale", &Settings::Levels::resolutionScale,
            "reductionDelay", &Settings::Levels::reductionDelay,
            "maxPendingLoads", &Settings::Levels::maxPendingLoads
        );

        ns->new_usertype<Settings::Meshes>("MeshStreamingSettings",
            "enable", &Settings::Meshes::enable,
            "budget", &Settings::Meshes::budget,
            "distanceMargin", &Settings::Meshes::distanceMargin,
            "rayTracingDistance", &Settings::Meshes::rayTracingDistance,
            "reductionDelay", &Settings::Meshes::reductionDelay,
            "maxPendingLoads", &Settings::Meshes::maxPendingLoads
        );

        ns->new_usertype<Settings>("StreamingSettings",
            "textures", DependentMember<&Settings::textures>(),
            "impostors", DependentMember<&Settings::impostors>(),
            "meshes", DependentMember<&Settings::meshes>()
        );

        ns->new_usertype<Streaming::ResidencyStats>("ResidencyStats",
            "count", sol::readonly(&Streaming::ResidencyStats::count),
            "residentCount", sol::readonly(&Streaming::ResidencyStats::residentCount),
            "pendingLoads", sol::readonly(&Streaming::ResidencyStats::pendingLoads),
            "loadCount", sol::readonly(&Streaming::ResidencyStats::loadCount),
            "residentBytes", sol::readonly(&Streaming::ResidencyStats::residentBytes),
            "requestedBytes", sol::readonly(&Streaming::ResidencyStats::requestedBytes),
            "budgetBias", sol::readonly(&Streaming::ResidencyStats::budgetBias)
        );

        ns->new_usertype<Streaming::StreamingManager::Stats>("StreamingStats",
            "textures", DependentMember<&Streaming::StreamingManager::Stats::textures>(),
            "impostors", DependentMember<&Streaming::StreamingManager::Stats::impostors>(),
            "meshes", DependentMember<&Streaming::StreamingManager::Stats::meshes>()
        );

        // The settings are shared by all scenes, scripts change them e.g. from a graphics menu
        ns->new_usertype<Streaming::StreamingManager>("StreamingManager",
            "settings", sol::var(std::ref(Streaming::StreamingManager::settings)),
            "GetStats", &Streaming::StreamingManager::GetStats
        );

    }

}
