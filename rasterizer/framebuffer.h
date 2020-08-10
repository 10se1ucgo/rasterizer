#pragma once

#include "glm/glm.hpp"

#include <memory>
#include <string>


namespace hamlet {
    using color32 = glm::u8vec4;

    
    struct framebuffer {
        framebuffer(int width, int height) {
            this->resize(width, height);
        }

        void resize(int width, int height) {
            this->_width = width;
            this->_height = height;
            this->_color_attachment = std::make_unique<color32[]>(this->num_pixels());
        }

        color32 &pixel(int x, int y) {
            return this->_color_attachment[y * this->_width + x];
        }

        size_t num_pixels() const {
            return size_t(this->_width) * this->_height;
        }

        void write_to_tga(const std::string &file_name) const;

        int width() { return this->_width; }
        int height() { return this->_height; }
        color32 *color_attachment() const { return this->_color_attachment.get(); }
    private:
        int _width, _height;
        std::unique_ptr<color32[]> _color_attachment;
    };
}
