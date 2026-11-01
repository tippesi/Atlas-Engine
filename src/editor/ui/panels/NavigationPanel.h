#pragma once

#include "Panel.h"
#include "navigation/Navigation.h"

#include <optional>

namespace Atlas::Editor::UI {

    class NavigationPanel : public Panel {

    public:
        NavigationPanel() : Panel("Navigation") {}

        void Render(Ref<Navigation::Navigation>& navigation);

        bool showMesh = false;

        // The drawn edges, collected again when the mesh changes
        std::vector<vec3> meshEdges;
        uint64_t meshEdgesRevision = 0;
        const Navigation::NavigationMesh* meshEdgesMesh = nullptr;

        // Right clicks into the viewport set the start and the goal of a test path
        bool pathTest = false;
        std::optional<vec3> pathStart;
        std::optional<vec3> pathGoal;
        std::vector<vec3> path;

    };

}
