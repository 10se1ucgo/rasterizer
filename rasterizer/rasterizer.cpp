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
    using avec4 = glm::aligned_vec4;
    using amat4 = glm::aligned_mat4x4;
    using aivec4 = glm::aligned_ivec4;
#else
    using avec4 = glm::vec4;
    using amat4 = glm::mat4x4;
    using aivec4 = glm::ivec4;
#endif

    const avec4 all_255s(255.f, 255.f, 255.f, 255.f);

    inline float edge(glm::vec2 p, glm::vec2 d) {
        return glm::determinant(glm::mat2(p, d));
    }

    inline bool all_components_positive(const avec4 &v) {
#if GLM_ARCH & GLM_ARCH_SSE42_BIT
        // _mm_movemask_ps returns an integer bitmask of all of the sign bits of v
        // e.g. 0b1001 = x,w negative, y,z positive.
        // in this case, 0b0000 means all positive
        return !_mm_movemask_ps(v.data);
#else
        return v.x >= 0 && v.y >= 0 && v.z >= 0 && v.w >= 0;
#endif
    }

    inline glm::u8vec4 interp_colors(const avec4 &bary, const avec4 &r, const avec4 &g, const avec4 &b, const avec4 &a) {
#if GLM_ARCH & GLM_ARCH_SSE42_BIT
        const auto br = _mm_dp_ps(bary.data, r.data, 0b11110001); // dot(bary, r), store result in .x
        const auto bg = _mm_dp_ps(bary.data, g.data, 0b11110001); // dot(bary, g), store result in .x
        const auto brg = _mm_insert_ps(br, bg, 0b00011100); // brg = (br.x, bg.x, 0, 0);
        const auto bb = _mm_dp_ps(bary.data, b.data, 0b11110001); // ditto
        const auto ba = _mm_dp_ps(bary.data, a.data, 0b11110001); // ditto
        const auto bba = _mm_insert_ps(bb, ba, 0b00011100); // bba = (bb.x, ba.x, 0, 0);
        const auto cb = _mm_movelh_ps(brg, bba); // cb = (brg.x, brg.y, bba.x bba.y);
        const auto cbf255 = _mm_mul_ps(cb, all_255s.data); // cb *= 255.f;

        // vec4->u8vec4 conversion
        // TODO: these pack instructions are used pretty inefficiently.
        // Buffer up multiple pixels at a time and then pack them together.
        // How? Not sure, maybe 2x2 tiles like how GPUs do it.
        const auto cbi32 = _mm_cvtps_epi32(cbf255); // cb = i32vec4(cb)
        const auto cbi16 = _mm_packus_epi32(cbi32, cbi32); // cb = i16vec4(cb)
        const auto cbi8 = _mm_packus_epi16(cbi16, cbi16); // cb = i8vec4(cb);
        uint32_t color = _mm_cvtsi128_si32(cbi8); // lower 32 bits of cb
        return *reinterpret_cast<glm::u8vec4 *>(&color);
#else
        return glm::u8vec4(avec4(dot(bary, r), dot(bary, g), dot(bary, b), dot(bary, a)) * 255.f);
#endif
    }

    struct render_context {
        hamlet::framebuffer fbo;
        glm::vec2 origin, size;
        float near = 0.f, far = 1.f;
        avec4 vp_center, vp_scale;

        render_context(int w, int h) : fbo(w, h) {
            this->viewport(0, 0, w, h);
            this->clear();
        }

        void depth_range(const float near, const float far) {
            this->near = near;
            this->far = far;
        }

        void viewport(const int x, const int y, const int width, const int height) {
            this->origin = { x, y };
            this->size = { width, height };
            // viewport transformations
            this->vp_center = { x + (float(width) / 2.f), y + (float(height) / 2.f), (this->near + this->far) / 2.f, 0.f };
            this->vp_scale = { this->size.x / 2.f, this->size.y / 2.f, (this->far - this->near) / 2.f, 1.0f };
        }

        void clip2ndc(avec4 &clip) {
            float w = 1.f/clip.w;
            clip *= w;
            clip.w = w;
        }

        void ndc2viewport(avec4 &ndc) {
            // FMA??
            ndc *= this->vp_scale;
            ndc += this->vp_center;
        }

        void clear(glm::vec4 color = glm::vec4(0, 0, 0, 1)) {
            glm::u8vec4 c32(color * 255.f);
            std::fill_n(this->fbo.color_attachment(), this->fbo.num_pixels(), c32);
        }

        void draw_triangle(avec4 a, avec4 b, avec4 c, glm::vec4 ac, glm::vec4 bc, glm::vec4 cc) {
            this->ndc2viewport(a); this->ndc2viewport(b); this->ndc2viewport(c);
            float inverse_area = 1.0f / edge(c - a, b - a);

            auto bcd = (c - b);
            auto cad = (a - c);
            auto abd = (b - a);

            avec4 reds   (ac.r, bc.r, cc.r, 0.0f),
                  greens (ac.g, bc.g, cc.g, 0.0f),
                  blues  (ac.b, bc.b, cc.b, 0.0f),
                  alphas (ac.a, bc.a, cc.a, 0.0f),
                  zs     ( a.z,  b.z,  c.z, 0.0f),
                  ws     ( a.w,  b.w,  c.w, 0.0f);

            glm::ivec2 hi = round(max(a, max(b, c))),
                       lo = round(min(a, min(b, c)));

            avec4 p(lo.x + 0.5f, lo.y + 0.5f, 0.0f, 0.0f);
            avec4 bary_o(edge(p - b, bcd), edge(p - c, cad), edge(p - a, abd), 1.0f);
            avec4 dy(bcd.y, cad.y, abd.y, 0.0f);
            avec4 dx(bcd.x, cad.x, abd.x, 0.0f);

            bary_o *= inverse_area;
            dx *= inverse_area;
            dy *= inverse_area;

            for (int y = lo.y; y <= hi.y; ++y) {
                avec4 bary(bary_o);
                p.x = lo.x + 0.5f;

                bool last_inside = false;
                for (int x = lo.x; x <= hi.x; ++x) {
                    bool inside = all_components_positive(bary);
                    last_inside |= inside;
                    if (inside) {
                        // depth values are interpolated with no perspective (See: OpenGL spec 14.10)
                        p.z = glm::dot(bary, zs);
                        // Vertex attributes (e.g.: color, textures) are interpolated with perspective (See: OpenGL spec 14.09)
                        // p.w = bary.x/a.w + bary.y/b.w + bary.z/c.w (ws is inverse w, see `clip2ndc`)
                        p.w = glm::dot(bary, ws);
                        // bary * ws == {bary.x/a.w, bary.y/b.w, bary.z/c.w}
                        // TODO: replace separate dot product and multiplication w/ one mul + horizontal sum (less latency i believe)
                        this->fbo.pixel(x, y) = interp_colors((bary * ws) / p.w, reds, greens, blues, alphas);
                    } else if (last_inside) {
                        // triangles are convex and each raster line is thus continuous
                        // if we were on the triangle and then fell off, it means this line is finished.
                        break;
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

