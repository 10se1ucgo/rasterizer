#include "framebuffer.h"

#include "fmt/format.h"
#include "fmt/color.h"
#include "glm/glm.hpp"
#ifdef GLM_FORCE_ALIGNED_GENTYPES
#include "glm/gtc/type_aligned.hpp"
#endif
#include <chrono>


namespace hamlet {
#ifdef GLM_FORCE_ALIGNED_GENTYPES
    using vec4 = glm::aligned_vec4;
    using mat4 = glm::aligned_mat4x4;
    using ivec4 = glm::aligned_ivec4;
#else
    using vec4 = glm::vec4;
    using mat4 = glm::mat4x4;
    using ivec4 = glm::ivec4;
#endif

    const vec4 all_255s(255.f, 255.f, 255.f, 255.f);

    inline float edge(glm::vec2 p, glm::vec2 d) {
        return glm::determinant(glm::mat2(p, d));
    }

    inline bool all_components_positive(vec4 &v) {
#if GLM_ARCH & GLM_ARCH_SSE42_BIT
        // _mm_movemask_ps returns an integer bitmask of all of the sign bits of v
        // e.g. 0b1001 = x,w negative, y,z positive.
        // in this case, 0b0000 means all positive
        return !_mm_movemask_ps(v.data);
#else
        return v.x >= 0 && v.y >= 0 && v.z >= 0 && v.w >= 0;
#endif
    }

    inline glm::u8vec4 interp_colors(const vec4 &bary, const vec4 &r, const vec4 &g, const vec4 &b, const vec4 &a) {
#if GLM_ARCH & GLM_ARCH_SSE42_BIT
        const auto br = _mm_dp_ps(bary.data, r.data, 0b11110001); // dot(bary, r), store result in .x
        const auto bg = _mm_dp_ps(bary.data, g.data, 0b11110001); // dot(bary, g), store result in .x
        const auto brg = _mm_insert_ps(br, bg, 0b00011100); // brg = (br.x, bg.x, 0, 0);
        const auto bb = _mm_dp_ps(bary.data, b.data, 0b11110001); // ditto
        const auto ba = _mm_dp_ps(bary.data, a.data, 0b11110001); // ditto
        const auto bba = _mm_insert_ps(bb, ba, 0b00011100); // bba = (bb.x, ba.x, 0, 0);
        const auto cb = _mm_movelh_ps(brg, bba); // cb = (brg.x, brg.y, bba.x bba.y);
        const auto cbf32 = _mm_mul_ps(cb, all_255s.data); // cb *= 255.f;
        // vec4->u8vec4 conversion is definitely a large bottleneck, this is likely where the main SIMD speedup is from.
        const auto cbi32 = _mm_cvtps_epi32(cbf32); // cb = i32vec4(cb)
        const auto cbi16 = _mm_packus_epi32(cbi32, cbi32); // cb = i16vec4(cb)
        const auto cbi8 = _mm_packus_epi16(cbi16, cbi16); // cb = i8vec4(cb);
        uint32_t color = _mm_cvtsi128_si32(cbi8); // lower 32 bits of cb
        return *reinterpret_cast<glm::u8vec4 *>(&color);
#else
        return glm::u8vec4(vec4(dot(bary, r), dot(bary, g), dot(bary, b), dot(bary, a)) * 255.f);
#endif
    }

    struct render_context {
        hamlet::framebuffer fbo;
        glm::vec2 origin, size;

        render_context(int w, int h) : fbo(w, h) {
            this->viewport(0, 0, w, h);
            this->clear();
        }

        void viewport(int x, int y, int width, int height) {
            this->origin = { x, y };
            this->size = { width, height };
        }

        void ndc2viewport(vec4 &ndc) {
            auto t((glm::vec2(ndc.x, ndc.y)+1.0f) * (this->size*0.5f) + this->origin);
            ndc.x = t.x;
            ndc.y = t.y;
        }

        void clear(glm::vec4 color = glm::vec4(0, 0, 0, 1)) {
            glm::u8vec4 c32(color * 255.f);
            std::fill_n(this->fbo.color_attachment(), this->fbo.num_pixels(), c32);
        }

        void draw_triangle(vec4 a, vec4 b, vec4 c, glm::vec4 ac, glm::vec4 bc, glm::vec4 cc) {
            this->ndc2viewport(a); this->ndc2viewport(b); this->ndc2viewport(c);
            float inverse_area = 1.0f / edge(c - a, b - a);


            auto bcd = (c - b);
            auto cad = (a - c);
            auto abd = (b - a);

            vec4 reds   { ac.r, bc.r, cc.r, 0.0f },
                 greens { ac.g, bc.g, cc.g, 0.0f },
                 blues  { ac.b, bc.b, cc.b, 0.0f },
                 alphas { ac.a, bc.a, cc.a, 0.0f };

            glm::ivec2 hi = round(max(a, max(b, c))),
                       lo = round(min(a, min(b, c)));

            // mat4 colors(ac, bc, cc, vec4(0.0f, 0.0f, 0.0f, 0.0f));

            vec4 p(lo.x + 0.5f, lo.y + 0.5f, 0.0f, 0.0f);
            vec4 bary_o{ edge(p - b, bcd), edge(p - c, cad), edge(p - a, abd), 1.0f };
            vec4 dy{ bcd.y, cad.y, abd.y, 0.0f };
            vec4 dx{ bcd.x, cad.x, abd.x, 0.0f };

            bary_o *= inverse_area;
            dx *= inverse_area;
            dy *= inverse_area;

            for (int y = lo.y; y <= hi.y; ++y) {
// #ifdef GLM_FORCE_ALIGNED_GENTYPES
//                 vec4 bary;
//                 bary.data = bary_o.data;
// #else
                vec4 bary(bary_o);
// #endif

                p.x = lo.x + 0.5f;
                for (int x = lo.x; x <= hi.x; ++x) {
                    bool inside = all_components_positive(bary);
                    if (inside) {
                        this->fbo.pixel(x, y) = interp_colors(bary, reds, greens, blues, alphas);
                    }
                    bary += dy;
                    p.x++;
                }
                bary_o -= dx;
                p.y++;
            }
        }
    };
}

int main(void) {
    hamlet::render_context rc(4096, 4096);

    glm::vec4 r = { 1, 0, 0, 1.0 };
    glm::vec4 g = { 0, 1, 0, 1.0 };
    glm::vec4 b = { 0, 0, 1, 1.0 };

    auto start = std::chrono::high_resolution_clock::now();
    rc.draw_triangle({ -0.5f, -0.5f, 0, 1}, { -0.25f, 0.25f, 0, 1}, { 0.5f, -0.5f, 0, 1}, r, g, b);
    rc.draw_triangle({ -0.5f, 0.5f, 0, 1}, { 0.5f, 0.5f, 0, 1}, { -0.25f, -0.25f, 0, 1}, b, g, r);
    auto end = std::chrono::high_resolution_clock::now();
    fmt::print("Rendering took: {}s", std::chrono::duration<double>(end - start).count());
    rc.fbo.write_to_tga("test.tga");
}

