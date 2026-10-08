package com.mosestyle.desperados.original;

import org.libsdl.app.SDLActivity;

/**
 * Runs the original Linux build of Desperados (the user's own copy) through Box64.
 * libmain.so (launcher.c) does the work once SDL has created the window. StartActivity makes
 * sure the app may read the Desperados folder before this starts.
 */
public class OriginalActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }
}
