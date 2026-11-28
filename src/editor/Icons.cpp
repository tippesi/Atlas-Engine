#include "Icons.h"
#include "Singletons.h"

namespace Atlas::Editor {

    // Each icon has a white file and a black one ending with _light
    const std::map<IconType, const char*> Icons::iconNames = {
        { IconType::Audio, "audio" },
        { IconType::Camera, "camera" },
        { IconType::Document, "document" },
        { IconType::Folder, "folder" },
        { IconType::Image, "image" },
        { IconType::Mesh, "mesh" },
        { IconType::Play, "play" },
        { IconType::Stop, "stop" },
        { IconType::Pause, "pause" },
        { IconType::Scene, "scene" },
        { IconType::Terrain, "terrain" },
        { IconType::Settings, "settings" },
        { IconType::Font, "font" },
        { IconType::ArrowUp, "arrowUp" },
        { IconType::ArrowDown, "arrowDown" },
        { IconType::ArrowLeft, "arrowLeft" },
        { IconType::ArrowRight, "arrowRight" },
        { IconType::Prefab, "prefab" },
        { IconType::Visibility, "visibility" },
        { IconType::VisibilityOff, "visibility_off" },
        { IconType::Move, "move" },
        { IconType::Scale, "scale" },
        { IconType::Rotate, "rotate" },
        { IconType::MoreHorizontal, "moreHoriz" },
        { IconType::MoreVertical, "moreVert" },
        { IconType::Material, "material" },
        { IconType::MeshSource, "meshSource" },
        { IconType::EnvironmentImage, "environment" },
        { IconType::Trash, "delete" },
        { IconType::Filter, "filter" },
        { IconType::OpenInNew, "openInNew" },
        { IconType::Reset, "reset" },
        { IconType::MaterialGraph, "materialGraph" },
        { IconType::Ocean, "ocean" },
        { IconType::Navigation, "navigation" },
        { IconType::IrradianceVolume, "irradianceVolume" },
        { IconType::GlobalIllumination, "globalIllumination" },
        { IconType::Reflection, "reflection" },
        { IconType::Shadow, "shadow" },
        { IconType::Clouds, "clouds" },
        { IconType::Fog, "fog" },
        { IconType::Sky, "sky" },
        { IconType::Wind, "wind" },
        { IconType::PostProcessing, "postProcessing" },
        { IconType::ViewList, "viewList" },
        { IconType::ViewGrid, "viewGrid" },
        { IconType::Log, "log" },
        { IconType::Error, "error" },
        { IconType::Warning, "warning" },
        { IconType::Notifications, "notifications" },
        { IconType::ScaleInverse, "scaleInverse" },
        { IconType::Pin, "pin" },
        { IconType::PinFilled, "pinFilled" },
        { IconType::Profiler, "profiler" },
        { IconType::Save, "save" },
        { IconType::Undo, "undo" },
        { IconType::Redo, "redo" },
        { IconType::Search, "search" },
        { IconType::Close, "cancel" },
        { IconType::SpotLight, "spotLight" },
        { IconType::Player, "player" },
        { IconType::Trigger, "trigger" },
    };

    Icons::Icons() {

        LoadIcons(whiteIcons, ".png");

    }

    Texture::Texture2D& Icons::GetTintable(IconType type) {

        return whiteIcons.contains(type) ? whiteIcons[type] : whiteIcons[IconType::Document];

    }

    Texture::Texture2D& Icons::Get(IconType type) {

        if (Singletons::config->darkMode)
            return GetTintable(type);

        if (blackIcons.empty())
            LoadIcons(blackIcons, "_light.png");

        return blackIcons.contains(type) ? blackIcons[type] : blackIcons[IconType::Document];

    }

    void Icons::LoadIcons(std::map<IconType, Texture::Texture2D>& icons, const std::string& suffix) {

        for (const auto& [type, name] : iconNames)
            icons[type] = Texture::Texture2D("editor/icons/" + std::string(name) + suffix);

    }

}
