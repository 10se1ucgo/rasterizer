#include "framebuffer.h"

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
        tga_image_spec spec = { 0, 0, uint16_t(this->_width), uint16_t(this->_height), 32, 8 };
        // no image ID, no color map, uncompressed, true color.
        tga_header header = { 0, 0, 2 , 0, 0, spec };
        
        // RGBA -> BGRA
        const size_t px = this->num_pixels();
        const auto buf = std::make_unique<color32[]>(px); // todo: make_unique_for_overwrite
        memcpy(buf.get(), this->_color_attachment.get(), sizeof(color32) * px);
        for (size_t i = 0; i < px; ++i) {
            std::swap(buf[i].r, buf[i].b);
        }

        FILE *fp = fopen(file_name.c_str(), "wb");
        fwrite(&header, sizeof(header), 1, fp);
        fwrite(buf.get(), sizeof(color32), px, fp);
        fclose(fp);
    }
}