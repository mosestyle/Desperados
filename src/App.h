#pragma once
#include <SDL.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "input/TouchInput.h"
#include "screens/Screen.h"

struct AppOptions {
    std::string dataPath;      // explicit data folder (desktop)
    int startLevel = 0;        // open this level directly (1-25)
    std::string autotest;      // screenshot prefix for the scripted self-test (desktop)
    int width = 1600, height = 720;
    std::string navDump;       // --navdump file.bmp: save the walkable grid of the level
    std::string shot;          // --shot file.bmp: render the level once and save a screenshot
    int shotFrame = 40;        // --shotframe N: frame at which the screenshot is taken
    bool cones = false;        // --cones: show every enemy's field of view
    float heroX = -1, heroY = -1; // --hero x,y: place the selected hero (debug)
    std::string shotSeries;    // --series prefix: screenshots every 60 frames
    float viewX = -1, viewY = -1, viewZoom = 0;
};

class App {
public:
    explicit App(AppOptions opt) : opt_(std::move(opt)) {}
    int run();

    SDL_Renderer* renderer() const { return renderer_; }
    int width() const { return w_; }
    int height() const { return h_; }
    float uiScale() const { return uiScale_; }  // ~1 on a 720p-tall screen

    void push(std::unique_ptr<Screen> s);
    void pop();
    void replace(std::unique_ptr<Screen> s);
    void quit() { running_ = false; }
    bool findData();

private:
    bool init();
    void shutdown();
    void updateSize();
    void runAutotestStep(uint32_t now);
    void injectFinger(SDL_EventType type, SDL_FingerID id, float x, float y);
    void saveScreenshot(const std::string& path);

    AppOptions opt_;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    int w_ = 0, h_ = 0;
    float uiScale_ = 1;
    bool running_ = true;
    TouchInput input_;
    std::vector<std::unique_ptr<Screen>> screens_;
    std::vector<std::unique_ptr<Screen>> graveyard_;  // screens popped during a frame

    // autotest
    struct Step { uint32_t at; std::function<void()> fn; };
    std::vector<Step> steps_;
    size_t stepIndex_ = 0;
    uint32_t testStart_ = 0;
    int frameCount_ = 0;
};
