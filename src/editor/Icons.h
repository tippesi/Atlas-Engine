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
        ViewGrid,
        Log,
        Error,
        Warning,
        Notifications,
        ScaleInverse,
        Pin,
        PinFilled,
        Profiler,
        Save,
        Undo,
        Redo,
        Search,
        Close,
        SpotLight,
        Player,
        Trigger
    };

    class Icons {
    public:
        Icons();

        Texture::Texture2D& Get(IconType type);

        // White in both modes, such that it can be tinted
        Texture::Texture2D& GetTintable(IconType type);

    private:
        static void LoadIcons(std::map<IconType, Texture::Texture2D>& icons, const std::string& suffix);

        static const std::map<IconType, const char*> iconNames;

        std::map<IconType, Texture::Texture2D> whiteIcons;
        // Loaded in the light mode
        std::map<IconType, Texture::Texture2D> blackIcons;

    };

}
