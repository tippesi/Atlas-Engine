package com.atlasengine.app;

import org.libsdl.app.SDLActivity;

public class AtlasEngineActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
		// Add libraries here. The name of the library should
		// match the name of the CMake project.
        return new String[]{
                "SDL2",
                "assimp",
                "AtlasEngine"
        };
    }

}
