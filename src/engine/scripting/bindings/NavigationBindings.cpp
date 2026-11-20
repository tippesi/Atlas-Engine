#include "NavigationBindings.h"

#include "navigation/Navigation.h"
#include "scene/Scene.h"

namespace Atlas::Scripting::Bindings {

    void GenerateNavigationBindings(sol::table* ns) {

        ns->new_enum<JobPriority>("JobPriority", {
            { "High", JobPriority::High },
            { "Medium", JobPriority::Medium },
            { "Low", JobPriority::Low }
            });

        ns->new_usertype<Navigation::NavigationSettings>("NavigationSettings",
            sol::call_constructor,
            sol::constructors<Navigation::NavigationSettings()>(),
            "agentRadius", &Navigation::NavigationSettings::agentRadius,
            "agentHeight", &Navigation::NavigationSettings::agentHeight,
            "agentMaxClimb", &Navigation::NavigationSettings::agentMaxClimb,
            "agentMaxSlope", &Navigation::NavigationSettings::agentMaxSlope,
            "cellSize", &Navigation::NavigationSettings::cellSize,
            "cellHeight", &Navigation::NavigationSettings::cellHeight,
            "tileSize", &Navigation::NavigationSettings::tileSize
            );

        ns->new_usertype<Navigation::NavigationMesh>("NavigationMesh",
            sol::call_constructor,
            sol::factories([](const Navigation::NavigationSettings& settings) {
                return CreateRef<Navigation::NavigationMesh>(settings);
                }),
            "Update", &Navigation::NavigationMesh::Update,
            "Invalidate", sol::overload(
                sol::resolve<void(const Volume::AABB&)>(&Navigation::NavigationMesh::Invalidate),
                sol::resolve<void()>(&Navigation::NavigationMesh::Invalidate)),
            "IsBuilt", &Navigation::NavigationMesh::IsBuilt,
            "FindPath", [](Navigation::NavigationMesh& navigationMesh, vec3 start, vec3 end) {
                return sol::as_table(navigationMesh.FindPath(start, end));
                },
            "FindNearestPoint", &Navigation::NavigationMesh::FindNearestPoint,
            "MoveAlongSurface", &Navigation::NavigationMesh::MoveAlongSurface,
            "IsWalkable", &Navigation::NavigationMesh::IsWalkable,
            "FindRandomPoint", &Navigation::NavigationMesh::FindRandomPoint,
            "GetTileCount", &Navigation::NavigationMesh::GetTileCount,
            "GetPendingTileCount", &Navigation::NavigationMesh::GetPendingTileCount,
            "searchExtents", &Navigation::NavigationMesh::searchExtents,
            "maxTileStartsPerUpdate", &Navigation::NavigationMesh::maxTileStartsPerUpdate,
            "maxConcurrentBuilds", &Navigation::NavigationMesh::maxConcurrentBuilds,
            "buildPriority", &Navigation::NavigationMesh::buildPriority
            );

        ns->new_usertype<Navigation::Navigation>("Navigation",
            sol::call_constructor,
            sol::factories([]() { return CreateRef<Navigation::Navigation>(); }),
            "enable", &Navigation::Navigation::enable,
            "radius", &Navigation::Navigation::radius,
            "settings", &Navigation::Navigation::settings,
            "maxTileStartsPerUpdate", &Navigation::Navigation::maxTileStartsPerUpdate,
            "maxConcurrentBuilds", &Navigation::Navigation::maxConcurrentBuilds,
            "buildPriority", &Navigation::Navigation::buildPriority,
            "mesh", &Navigation::Navigation::mesh
            );

    }

}
