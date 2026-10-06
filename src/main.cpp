// OpenDesperados - an open re-implementation of the Desperados: Wanted Dead or Alive
// engine for touch screens. Requires the original game data (not included).
#include <SDL.h>
#include <SDL_main.h>

#include <cstdlib>
#include <cstring>
#include <string>

#include "App.h"

int main(int argc, char* argv[]) {
    AppOptions opt;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* def) -> std::string { return i + 1 < argc ? argv[++i] : def; };
        if (a == "--data") opt.dataPath = next("");
        else if (a == "--level") opt.startLevel = std::atoi(next("0").c_str());
        else if (a == "--autotest") opt.autotest = next("autotest");
        else if (a == "--shot") opt.shot = next("shot.bmp");
        else if (a == "--view") {  // x,y,zoom
            std::string v = next("0,0,1");
            SDL_sscanf(v.c_str(), "%f,%f,%f", &opt.viewX, &opt.viewY, &opt.viewZoom);
        } else if (a == "--size") {
            std::string s = next("1600x720");
            size_t x = s.find('x');
            if (x != std::string::npos) { opt.width = std::atoi(s.c_str()); opt.height = std::atoi(s.c_str() + x + 1); }
        }
    }
    App app(opt);
    return app.run();
}
