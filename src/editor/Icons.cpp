#include "Icons.h"

namespace Atlas::Editor {

    Icons::Icons() {

        icons = std::map<IconType, Texture::Texture2D> {
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
            { IconType::Font, Texture::Texture2D("editor/icons/font.png") },
        };

    }

    Texture::Texture2D &Icons::Get(IconType type) {

        if (!icons.contains(type))
            return icons[IconType::Document];

        return icons[type];

    }

}