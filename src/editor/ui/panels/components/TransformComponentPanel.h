#pragma once

#include "../Panel.h"

#include "scene/components/TransformComponent.h"

namespace Atlas::Editor::UI {

    class TransformComponentPanel : public Panel {

    public:
        TransformComponentPanel() : Panel("Transform component") {}

        bool Render(TransformComponent& transform);

    };

}
