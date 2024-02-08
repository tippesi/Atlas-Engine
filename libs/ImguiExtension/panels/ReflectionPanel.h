#pragma once

#include "Panel.h"

#include "lighting/Reflection.h"

namespace Atlas::ImguiExtension {

    class ReflectionPanel : public Panel {

    public:
        ReflectionPanel() : Panel("Reflection properties") {}

        void Render(Ref<Lighting::Reflection>& reflection);

    };

}