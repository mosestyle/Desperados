package com.mosestyle.desperados.engine;

import org.libsdl.app.SDLActivity;

/**
 * The new Desperados engine (engine/src): reads the player's own copy of the game from the
 * Desperados folder. StartActivity makes sure the app may read that folder before this starts.
 */
public class EngineActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
