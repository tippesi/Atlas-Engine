#include "Icons.h"
#include "Singletons.h"

namespace Atlas::Editor {

    Icons::Icons() {

        LoadIcons();

    }

    Texture::Texture2D& Icons::GetTintable(IconType type) {

        // The dark mode icons are white already
        if (Singletons::config->darkMode)
            return Get(type);

        if (tintableIcons.empty()) {
            tintableIcons = std::map<IconType, Texture::Texture2D>{
                { IconType::Audio, Texture::Texture2D("editor/icons/audio.png") },
                { IconType::Document, Texture::Texture2D("editor/icons/document.png") },
                { IconType::Folder, Texture::Texture2D("editor/icons/folder.png") },
                { IconType::Image, Texture::Texture2D("editor/icons/image.png") },
                { IconType::Mesh, Texture::Texture2D("editor/icons/mesh.png") },
                { IconType::Scene, Texture::Texture2D("editor/icons/scene.png") },
                { IconType::Terrain, Texture::Texture2D("editor/icons/terrain.png") },
                { IconType::Font, Texture::Texture2D("editor/icons/font.png") },
                { IconType::Prefab, Texture::Texture2D("editor/icons/prefab.png") },
                { IconType::Material, Texture::Texture2D("editor/icons/material.png") },
                { IconType::MeshSource, Texture::Texture2D("editor/icons/meshSource.png") },
                { IconType::EnvironmentImage, Texture::Texture2D("editor/icons/environment.png") },
                { IconType::MaterialGraph, Texture::Texture2D("editor/icons/materialGraph.png") },
            };
        }

        if (!tintableIcons.contains(type))
            return tintableIcons[IconType::Document];

        return tintableIcons[type];

    }

    Texture::Texture2D &Icons::Get(IconType type) {

        auto darkMode = Singletons::config->darkMode;

        if (darkMode != darkModeIcons)
            LoadIcons();

        if (!icons.contains(type))
            return icons[IconType::Document];

        return icons[type];

    }

    void Icons::LoadIcons() {

        auto darkMode = Singletons::config->darkMode;

        if (darkMode) {
            icons = std::map<IconType, Texture::Texture2D>{
                { IconType::Audio, Texture::Texture2D("editor/icons/audio.png") },
                { IconType::Camera, Texture::Texture2D("editor/icons/camera.png") },
                { IconType::Document, Texture::Texture2D("editor/icons/document.png") },
                { IconType::Folder, Texture::Texture2D("editor/icons/folder.png") },
                { IconType::Image, Texture::Texture2D("editor/icons/image.png") },
                { IconType::Mesh, Texture::Texture2D("editor/icons/mesh.png") },
                { IconType::Play, Texture::Texture2D("editor/icons/play.png") },
                { IconType::Stop, Texture::Texture2D("editor/icons/stop.png") },
                { IconType::Pause, Texture::Texture2D("editor/icons/pause.png") },
                { IconType::Scene, Texture::Texture2D("editor/icons/scene.png") },
                { IconType::Terrain, Texture::Texture2D("editor/icons/terrain.png") },
                { IconType::Settings, Texture::Texture2D("editor/icons/settings.png") },
                { IconType::Font, Texture::Texture2D("editor/icons/font.png") },
                { IconType::ArrowUp, Texture::Texture2D("editor/icons/arrowUp.png") },
                { IconType::ArrowDown, Texture::Texture2D("editor/icons/arrowDown.png") },
                { IconType::ArrowLeft, Texture::Texture2D("editor/icons/arrowLeft.png") },
                { IconType::ArrowRight, Texture::Texture2D("editor/icons/arrowRight.png") },
                { IconType::Prefab, Texture::Texture2D("editor/icons/prefab.png") },
                { IconType::Visibility, Texture::Texture2D("editor/icons/visibility.png") },
                { IconType::VisibilityOff, Texture::Texture2D("editor/icons/visibility_off.png") },
                { IconType::Move, Texture::Texture2D("editor/icons/move.png") },
                { IconType::Scale, Texture::Texture2D("editor/icons/scale.png") },
                { IconType::Rotate, Texture::Texture2D("editor/icons/rotate.png") },
                { IconType::MoreHorizontal, Texture::Texture2D("editor/icons/moreHoriz.png") },
                { IconType::MoreVertical, Texture::Texture2D("editor/icons/moreVert.png") },
                { IconType::Material, Texture::Texture2D("editor/icons/material.png") },
                { IconType::MeshSource, Texture::Texture2D("editor/icons/meshSource.png") },
                { IconType::EnvironmentImage, Texture::Texture2D("editor/icons/environment.png") },
                { IconType::Trash, Texture::Texture2D("editor/icons/delete.png") },
                { IconType::Filter, Texture::Texture2D("editor/icons/filter.png") },
                { IconType::OpenInNew, Texture::Texture2D("editor/icons/openInNew.png") },
                { IconType::Reset, Texture::Texture2D("editor/icons/reset.png") },
                { IconType::MaterialGraph, Texture::Texture2D("editor/icons/materialGraph.png") },
                { IconType::Ocean, Texture::Texture2D("editor/icons/ocean.png") },
                { IconType::Navigation, Texture::Texture2D("editor/icons/navigation.png") },
                { IconType::IrradianceVolume, Texture::Texture2D("editor/icons/irradianceVolume.png") },
                { IconType::GlobalIllumination, Texture::Texture2D("editor/icons/globalIllumination.png") },
                { IconType::Reflection, Texture::Texture2D("editor/icons/reflection.png") },
                { IconType::Shadow, Texture::Texture2D("editor/icons/shadow.png") },
                { IconType::Clouds, Texture::Texture2D("editor/icons/clouds.png") },
                { IconType::Fog, Texture::Texture2D("editor/icons/fog.png") },
                { IconType::Sky, Texture::Texture2D("editor/icons/sky.png") },
                { IconType::Wind, Texture::Texture2D("editor/icons/wind.png") },
                { IconType::PostProcessing, Texture::Texture2D("editor/icons/postProcessing.png") },
                { IconType::ViewList, Texture::Texture2D("editor/icons/viewList.png") },
                { IconType::ViewGrid, Texture::Texture2D("editor/icons/viewGrid.png") },
            };
        }
        else {
            icons = std::map<IconType, Texture::Texture2D>{
                { IconType::Audio, Texture::Texture2D("editor/icons/audio_light.png") },
                { IconType::Camera, Texture::Texture2D("editor/icons/camera_light.png") },
                { IconType::Document, Texture::Texture2D("editor/icons/document_light.png") },
                { IconType::Folder, Texture::Texture2D("editor/icons/folder_light.png") },
                { IconType::Image, Texture::Texture2D("editor/icons/image_light.png") },
                { IconType::Mesh, Texture::Texture2D("editor/icons/mesh_light.png") },
                { IconType::Play, Texture::Texture2D("editor/icons/play_light.png") },
                { IconType::Stop, Texture::Texture2D("editor/icons/stop_light.png") },
                { IconType::Pause, Texture::Texture2D("editor/icons/pause_light.png") },
                { IconType::Scene, Texture::Texture2D("editor/icons/scene_light.png") },
                { IconType::Terrain, Texture::Texture2D("editor/icons/terrain_light.png") },
                { IconType::Settings, Texture::Texture2D("editor/icons/settings_light.png") },
                { IconType::Font, Texture::Texture2D("editor/icons/font_light.png") },
                { IconType::ArrowUp, Texture::Texture2D("editor/icons/arrowUp_light.png") },
                { IconType::ArrowDown, Texture::Texture2D("editor/icons/arrowDown_light.png") },
                { IconType::ArrowLeft, Texture::Texture2D("editor/icons/arrowLeft_light.png") },
                { IconType::ArrowRight, Texture::Texture2D("editor/icons/arrowRight_light.png") },
                { IconType::Prefab, Texture::Texture2D("editor/icons/prefab_light.png") },
                { IconType::Visibility, Texture::Texture2D("editor/icons/visibility_light.png") }, 
                { IconType::VisibilityOff, Texture::Texture2D("editor/icons/visibility_off_light.png") }, 
                { IconType::Move, Texture::Texture2D("editor/icons/move_light.png") },
                { IconType::Scale, Texture::Texture2D("editor/icons/scale_light.png") },
                { IconType::Rotate, Texture::Texture2D("editor/icons/rotate_light.png") },
                { IconType::MoreHorizontal, Texture::Texture2D("editor/icons/moreHoriz_light.png") },
                { IconType::MoreVertical, Texture::Texture2D("editor/icons/moreVert_light.png") },
                { IconType::Material, Texture::Texture2D("editor/icons/material_light.png") },
                { IconType::MeshSource, Texture::Texture2D("editor/icons/meshSource_light.png") },
                { IconType::EnvironmentImage, Texture::Texture2D("editor/icons/environment_light.png") },
                { IconType::Trash, Texture::Texture2D("editor/icons/delete_light.png") },
                { IconType::Filter, Texture::Texture2D("editor/icons/filter_light.png") },
                { IconType::OpenInNew, Texture::Texture2D("editor/icons/openInNew_light.png") },
                { IconType::Reset, Texture::Texture2D("editor/icons/reset_light.png") },
                { IconType::MaterialGraph, Texture::Texture2D("editor/icons/materialGraph_light.png") },
                { IconType::Ocean, Texture::Texture2D("editor/icons/ocean_light.png") },
                { IconType::Navigation, Texture::Texture2D("editor/icons/navigation_light.png") },
                { IconType::IrradianceVolume, Texture::Texture2D("editor/icons/irradianceVolume_light.png") },
                { IconType::GlobalIllumination, Texture::Texture2D("editor/icons/globalIllumination_light.png") },
                { IconType::Reflection, Texture::Texture2D("editor/icons/reflection_light.png") },
                { IconType::Shadow, Texture::Texture2D("editor/icons/shadow_light.png") },
                { IconType::Clouds, Texture::Texture2D("editor/icons/clouds_light.png") },
                { IconType::Fog, Texture::Texture2D("editor/icons/fog_light.png") },
                { IconType::Sky, Texture::Texture2D("editor/icons/sky_light.png") },
                { IconType::Wind, Texture::Texture2D("editor/icons/wind_light.png") },
                { IconType::PostProcessing, Texture::Texture2D("editor/icons/postProcessing_light.png") },
                { IconType::ViewList, Texture::Texture2D("editor/icons/viewList_light.png") },
                { IconType::ViewGrid, Texture::Texture2D("editor/icons/viewGrid_light.png") },
            };
        }

        darkModeIcons = darkMode;

    }

}