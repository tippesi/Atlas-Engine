#pragma once

#include "texture/Texture2D.h"

#include <map>

namespace Atlas::Editor {

    enum class IconType {
        Audio = 0,
        Camera,
        Document,
        Folder,
        Image,
        Mesh,
        Play,
        Stop,
        Scene,
        Terrain,
        Font,
        Settings,
        ArrowUp,
        ArrowLeft,
        ArrowRight,
        Prefab,
        Eye
    };

    class Icons {
    public:
        Icons();

        Texture::Texture2D& Get(IconType type);

    private:
        void LoadIcons();

        std::map<IconType, Texture::Texture2D> icons;

        bool darkModeIcons = true;

    };

}