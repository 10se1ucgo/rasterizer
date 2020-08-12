#pragma once

#include "glm/glm.hpp"

#include <memory>
#include <string>


namespace hamlet {
    using color32 = glm::u8vec4;

    struct depth_t {
        constexpr static int DEPTH_BITS = 24;
        uint32_t depth : DEPTH_BITS, stencil : (32 - DEPTH_BITS);
    };

    struct framebuffer {
        framebuffer(int width, int height) {
            this->resize(width, height);
        }

        void resize(int width, int height) {
            this->_width = width;
            this->_height = height;
            this->_color_attachment = std::make_unique<color32[]>(this->num_pixels());
            this->_depth_attachment = std::make_unique<depth_t[]>(this->num_pixels());
        }

        color32 &pixel(int x, int y) const {
            return this->_color_attachment[y * this->_width + x];
        }

        depth_t &depth(int x, int y) const {
            return this->_depth_attachment[y * this->_width + x];
        }

        size_t num_pixels() const {
            return size_t(this->_width) * this->_height;
        }

        void write_to_tga(const std::string &file_name) const;

        int width() const { return this->_width; }
        int height() const { return this->_height; }
        color32 *color_attachment() const { return this->_color_attachment.get(); }
        depth_t *depth_attachment() const { return this->_depth_attachment.get(); }
    private:
        int _width, _height;
        std::unique_ptr<color32[]> _color_attachment;
        std::unique_ptr<depth_t[]> _depth_attachment;
    };
}
