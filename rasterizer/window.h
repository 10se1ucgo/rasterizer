#pragma once

#include "SDL.h"

#include <string>

namespace hamlet {
    // yes this is a rip from ace but code reuse is good right??

    enum class WINDOW_STYLE {
        WINDOWED,
        FULLSCREEN,
        FULLSCREEN_BORDERLESS
    };

    struct Window {
        Window(std::string caption, int w, int h, WINDOW_STYLE style);
        ~Window();

        Window(const Window &other) = delete; void operator=(const Window &other) = delete;
        Window(Window &&) = delete; void operator=(Window &&) = delete;

        void run_main_loop();
        void quit() { this->quitting = true; }
    private:
        void update(double dt);
        void poll_events();

        SDL_Window *window;
        bool quitting;
        double time;
    };
}
