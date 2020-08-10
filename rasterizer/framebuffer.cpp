#include "framebuffer.h"

#include "fmt/format.h"
#include "fmt/color.h"

namespace hamlet {
#pragma pack(push, 1)
    struct tga_image_spec {
        uint16_t x_origin, y_origin;
        uint16_t width, height;
        uint8_t bpp;
        uint8_t descriptor; // 0bUUDDAAAA -> U = unused D = direction A = alpha depth
        // we probably only want 0b00001000 = 8 = 8 bit alpha depth
    };


    struct tga_header {
        uint8_t id_length, map_type, image_type;
        int32_t map_spec_hi; int8_t map_spec_lo;
        tga_image_spec spec;
    };
#pragma pack(pop)

    void framebuffer::write_to_tga(const std::string &file_name) const {
        tga_image_spec spec = { 0, 0, uint16_t(this->width), uint16_t(this->height), 32, 8 };
        // no image ID, no color map, uncompressed, true color.
        tga_header header = { 0, 0, 2 , 0, 0, spec };
        
        // RGBA -> BGRA
        const auto px = this->num_pixels();
        color32 *buf = new color32[px];
        memcpy(buf, this->color_attachment.get(), sizeof(color32) * px);
        for (size_t i = 0; i < px; ++i) {
            std::swap(buf[i].r, buf[i].b);
        }

        FILE *fp = fopen(file_name.c_str(), "wb");
        fwrite(&header, sizeof(header), 1, fp);
        fwrite(buf, sizeof(color32), px, fp);
        fclose(fp);

        delete[] buf;
    }
}


int main(void) {
    fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "Hello World!\n");
    hamlet::framebuffer fbo(256, 256);
    fbo.pixel(128, 128) = glm::u8vec4(126, 32, 59, 255);
    fbo.write_to_tga("test.tga");
}

