#pragma once

#include "../Panel.h"

#include "scene/components/LuaScriptComponent.h"
#include "../../popups/ResourceSelectionPopup.h"

namespace Atlas::Editor::UI
{

    class LuaScriptComponentPanel : public Panel {

    public:
        LuaScriptComponentPanel() : Panel("Lua script component") {}

        bool Render(Ref<Scene::Scene>& scene, Scene::Entity entity, LuaScriptComponent &luaScriptComponent);

    private:
        ResourceSelectionPopup scriptSelectionPopup;

    };

}