#include "Icons.h"
#include "Singletons.h"

namespace Atlas::Editor {

    Icons::Icons() {

        LoadIcons();

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
                { IconType::Scene, Texture::Texture2D("editor/icons/scene.png") },
                { IconType::Terrain, Texture::Texture2D("editor/icons/terrain.png") },
                { IconType::Settings, Texture::Texture2D("editor/icons/settings.png") },
                { IconType::Font, Texture::Texture2D("editor/icons/font.png") },
                { IconType::ArrowUp, Texture::Texture2D("editor/icons/arrowUp.png") },
                { IconType::ArrowLeft, Texture::Texture2D("editor/icons/arrowLeft.png") },
                { IconType::ArrowRight, Texture::Texture2D("editor/icons/arrowRight.png") },
                { IconType::Prefab, Texture::Texture2D("editor/icons/prefab.png") },
                { IconType::Eye, Texture::Texture2D("editor/icons/eye.png") },
                { IconType::Move, Texture::Texture2D("editor/icons/move.png") },
                { IconType::Scale, Texture::Texture2D("editor/icons/scale.png") },
                { IconType::Rotate, Texture::Texture2D("editor/icons/rotate.png") },
                { IconType::MoreHorizontal, Texture::Texture2D("editor/icons/more_horiz.png") },
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
                { IconType::Scene, Texture::Texture2D("editor/icons/scene_light.png") },
                { IconType::Terrain, Texture::Texture2D("editor/icons/terrain_light.png") },
                { IconType::Settings, Texture::Texture2D("editor/icons/settings_light.png") },
                { IconType::Font, Texture::Texture2D("editor/icons/font_light.png") },
                { IconType::ArrowUp, Texture::Texture2D("editor/icons/arrowUp_light.png") },
                { IconType::ArrowLeft, Texture::Texture2D("editor/icons/arrowLeft_light.png") },
                { IconType::ArrowRight, Texture::Texture2D("editor/icons/arrowRight_light.png") },
                { IconType::Prefab, Texture::Texture2D("editor/icons/prefab_light.png") },
                { IconType::Eye, Texture::Texture2D("editor/icons/eye_light.png") }, 
                { IconType::Move, Texture::Texture2D("editor/icons/move_light.png") },
                { IconType::Scale, Texture::Texture2D("editor/icons/scale_light.png") },
                { IconType::Rotate, Texture::Texture2D("editor/icons/rotate_light.png") },
                { IconType::MoreHorizontal, Texture::Texture2D("editor/icons/more_horiz_light.png") },
            };
        }

        darkModeIcons = darkMode;

    }

}