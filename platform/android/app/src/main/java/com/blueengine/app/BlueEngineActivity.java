package com.blueengine.app;

import org.libsdl.app.SDLActivity;

public class BlueEngineActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[]{
                "SDL2",
                "assimp",
                "BlueEngine"
        };
    }

}
