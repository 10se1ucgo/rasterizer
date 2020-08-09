#pragma once

#include "glm/glm.hpp"

#include <memory>


namespace hamlet {
    using color32 = glm::u8vec4;

    
    struct framebuffer {
        framebuffer(int width, int height) {
            this->resize(width, height);
        }

        void resize(int width, int height) {
            this->width = width;
            this->height = height;
            this->color_attachment = std::make_unique<color32[]>(this->num_pixels());
        }

        color32 &pixel(int x, int y) {
            return this->color_attachment[y * this->width + x];
        }

        size_t num_pixels() const {
            return size_t(this->width) * this->height;
        }

        int w() { return this->width; }
        int h() { return this->height; }
        const color32 *get() const { return this->color_attachment.get(); }
    private:
        int width, height;
        std::unique_ptr<color32[]> color_attachment;
    };
}