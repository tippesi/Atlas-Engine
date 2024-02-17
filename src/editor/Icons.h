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
        ArrowUp,
        ArrowLeft,
        ArrowRight
    };

    class Icons {
    public:
        Icons();

        Texture::Texture2D& Get(IconType type);

    private:
        std::map<IconType, Texture::Texture2D> icons;

    };

}