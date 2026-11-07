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
        Pause,
        Scene,
        Terrain,
        Font,
        Settings,
        ArrowUp,
        ArrowDown,
        ArrowLeft,
        ArrowRight,
        Prefab,
        Visibility,
        VisibilityOff,
        Move,
        Scale,
        Rotate,
        MoreHorizontal,
        MoreVertical,
        Material,
        MeshSource,
        EnvironmentImage,
        Trash,
        Filter,
        OpenInNew,
        Reset,
        MaterialGraph,
        Ocean,
        Navigation,
        IrradianceVolume,
        GlobalIllumination,
        Reflection,
        Shadow,
        Clouds,
        Fog,
        Sky,
        Wind,
        PostProcessing,
        ViewList,
        ViewGrid
    };

    class Icons {
    public:
        Icons();

        Texture::Texture2D& Get(IconType type);

        // White in both modes, such that it can be tinted
        Texture::Texture2D& GetTintable(IconType type);

    private:
        void LoadIcons();

        std::map<IconType, Texture::Texture2D> icons;
        std::map<IconType, Texture::Texture2D> tintableIcons;

        bool darkModeIcons = true;

    };

}