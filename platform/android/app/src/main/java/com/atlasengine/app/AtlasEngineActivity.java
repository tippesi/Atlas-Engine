package com.atlasengine.app;

import org.libsdl.app.SDLActivity;

public class AtlasEngineActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL2",
                "assimp",
                "AtlasEngine"
        };
    }

}
