#include "window.h"
#include "fmt/format.h"

#include <stdexcept>


namespace hamlet {
    namespace {
        void SDL_ERROR(const std::string &msg) {
            
        }
    }

    Window::Window(std::string caption, int w, int h, WINDOW_STYLE style) {
        SDL_Init(SDL_INIT_VIDEO);

        Uint32 flags = style == WINDOW_STYLE::WINDOWED ? 0 : SDL_WINDOW_FULLSCREEN;
        this->window = SDL_CreateWindow(caption.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h, flags);

    }

    Window::~Window() {
        SDL_DestroyWindow(this->window);
        SDL_Quit();
    }

    void Window::run_main_loop() {
        Uint64 last = SDL_GetPerformanceCounter();
        while (!this->quitting) {
            Uint64 now = SDL_GetPerformanceCounter();
            double dt = (now - last) / double(SDL_GetPerformanceFrequency());
            this->time += dt;
            last = now;

            this->update(dt);
        }
    }

    void Window::update(double dt) {
        this->poll_events();
    }

    void Window::poll_events() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch(e.type) {
            case SDL_QUIT:
                this->quit();
                break;
            default:
                break;
            }
        }
    }
}
