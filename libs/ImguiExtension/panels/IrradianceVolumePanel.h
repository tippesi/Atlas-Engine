#pragma once

#include "Panel.h"

#include "lighting/IrradianceVolume.h"

namespace Atlas::ImguiExtension {

    class IrradianceVolumePanel : public Panel {

    public:
        IrradianceVolumePanel() : Panel("Irradiance volume properties") {}

        void Render(Ref<Lighting::IrradianceVolume>& irradianceVolume, Ref<Scene::Scene>& scene);

    private:
        Volume::AABB CalculateSceneSize(Ref<Scene::Scene>& scene) const;

    };

}