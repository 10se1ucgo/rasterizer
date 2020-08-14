#define GLM_FORCE_SWIZZLE
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

    inline avec4 interpolate_attribute(const avec4 &bary, const avec4 &r, const avec4 &g, const avec4 &b, const avec4 &a) {
#if GLM_ARCH & GLM_ARCH_SSE42_BIT
        const auto br = _mm_dp_ps(bary.data, r.data, 0b11110001); // dot(bary, r), store result in .x
        const auto bg = _mm_dp_ps(bary.data, g.data, 0b11110001); // dot(bary, g), store result in .x
        const auto brg = _mm_insert_ps(br, bg, 0b00011100); // brg = (br.x, bg.x, 0, 0);
        const auto bb = _mm_dp_ps(bary.data, b.data, 0b11110001); // ditto
        const auto ba = _mm_dp_ps(bary.data, a.data, 0b11110001); // ditto
        const auto bba = _mm_insert_ps(bb, ba, 0b00011100); // bba = (bb.x, ba.x, 0, 0);
        const auto cb = _mm_movelh_ps(brg, bba); // cb = (brg.x, brg.y, bba.x bba.y);
        avec4 c;
        c.data = cb;
        return c;
#else
        return avec4(dot(bary, r), dot(bary, g), dot(bary, b), dot(bary, a)) * 255.f;
#endif
    }

    inline color32 vec4_to_color32(const avec4 &color) {
#if GLM_ARCH & GLM_ARCH_SSE42_BIT
        // vec4->u8vec4 conversion
        // TODO: these pack instructions are used pretty inefficiently.
        // Buffer up multiple pixels at a time and then pack them together.
        // How? Not sure, maybe 2x2 tiles like how GPUs do it.
        const auto cbf255 = _mm_mul_ps(color.data, all_255s.data); // cb *= 255.f;
        const auto cbi32 = _mm_cvtps_epi32(cbf255); // cb = i32vec4(cb)
        const auto cbi16 = _mm_packus_epi32(cbi32, cbi32); // cb = i16vec4(cb)
        const auto cbi8 = _mm_packus_epi16(cbi16, cbi16); // cb = i8vec4(cb);
        uint32_t c = _mm_cvtsi128_si32(cbi8); // lower 32 bits of cb
        return *reinterpret_cast<color32 *>(&c);
#else
        return glm::u8vec4(color);
#endif
    }

    inline depth_t float_to_depth(float z) {
        // Since we know z is in [0, 1], we can avoid the bitwise AND the bitfield generates.
        uint32_t d = uint32_t(z * ((1 << depth_t::DEPTH_BITS) - 1));
        return *reinterpret_cast<depth_t *>(&d);
    }

    void vert_shader(avec4 &position, glm::vec4 &color);
    avec4 frag_shader(const avec4 &frag_coord, const avec4 &frag_color);

    template<int N, std::size_t... Idx>
    void evaluate_vert_shader(avec4 &position, glm::vec4(&attrs)[N], std::index_sequence<Idx...>) {
        return vert_shader(position, attrs[Idx]...);
    }

    template<int N, std::size_t... Idx>
    avec4 evaluate_frag_shader(const avec4 &frag_coord, const avec4(&attrs)[N], std::index_sequence<Idx...>) {
        return frag_shader(frag_coord, attrs[Idx]...);
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
            depth_t d;
            d.depth = (1 << depth_t::DEPTH_BITS) - 1;
            std::fill_n(this->fbo.color_attachment(), this->fbo.num_pixels(), c32);
            std::fill_n(this->fbo.depth_attachment(), this->fbo.num_pixels(), d);
        }

        // All vertex attributes are type vec4.
        // Vertex shader outputs are the same as their inputs, 
        // All vertices require a position attribute. The first input to the vertex shader will be the vertex position.
        // Each attribute is passed as a reference, which should then be transformed by the function.
        template<int NAttrs>
        void draw_triangle(avec4 a, avec4 b, avec4 c, const glm::vec4(&a_attrs)[NAttrs], const glm::vec4(&b_attrs)[NAttrs], const glm::vec4(&c_attrs)[NAttrs]) {
            // this is....horrendously bad.
            // TODO: This isn't a good solution. Investigate better ones!
            // (Maybe using a matrix, passing a reference to each column, then transposing after?)
            glm::vec4 a_attr[NAttrs];
            glm::vec4 b_attr[NAttrs];
            glm::vec4 c_attr[NAttrs];
            memcpy(a_attr, a_attrs, sizeof(a_attr));
            memcpy(b_attr, b_attrs, sizeof(b_attr));
            memcpy(c_attr, c_attrs, sizeof(c_attr));
            evaluate_vert_shader(a, a_attr, std::make_index_sequence<NAttrs>{});
            evaluate_vert_shader(b, b_attr, std::make_index_sequence<NAttrs>{});
            evaluate_vert_shader(c, c_attr, std::make_index_sequence<NAttrs>{});

            this->clip2ndc(a); this->clip2ndc(b); this->clip2ndc(c);
            this->ndc2viewport(a); this->ndc2viewport(b); this->ndc2viewport(c);
            float inverse_area = 1.0f / edge(c - a, b - a);

            auto bcd = (c - b);
            auto cad = (a - c);
            auto abd = (b - a);

            // having each component in its own packed float makes it much easier (faster?) to perform
            // fragment input attribute interpolation w/ SSE instructions.
            // The value for each component at a specific fragment is dot(perspective_correct_barycentric, component_vector);
            avec4 attr_xs[NAttrs],
                  attr_ys[NAttrs],
                  attr_zs[NAttrs],
                  attr_ws[NAttrs];
            for(int i = 0; i < NAttrs; ++i) {
                attr_xs[i] = avec4(a_attr[i].x, b_attr[i].x, c_attr[i].x, 0.0f);
                attr_ys[i] = avec4(a_attr[i].y, b_attr[i].y, c_attr[i].y, 0.0f);
                attr_zs[i] = avec4(a_attr[i].z, b_attr[i].z, c_attr[i].z, 0.0f);
                attr_ws[i] = avec4(a_attr[i].w, b_attr[i].w, c_attr[i].w, 0.0f);
            }
            
            avec4 zs(a.z, b.z, c.z, 0.0f),
                  ws(a.w, b.w, c.w, 0.0f);

            glm::vec2 fhi = round(max(a, max(b, c))),
                      flo = round(min(a, min(b, c)));

            glm::ivec2 hi = fhi,
                       lo = flo;

            avec4 p(lo.x + 0.5f, lo.y + 0.5f, 0.0f, 0.0f);
            avec4 bary_o(edge(p - b, bcd), edge(p - c, cad), edge(p - a, abd), 1.0f);
            avec4 dy(bcd.y, cad.y, abd.y, 0.0f);
            avec4 dx(bcd.x, cad.x, abd.x, 0.0f);

            bary_o *= inverse_area;
            dx *= inverse_area;
            dy *= inverse_area;

            for (int y = lo.y; y <= hi.y; ++y) {
                avec4 bary(bary_o);
                p.x = flo.x + 0.5f;

                bool last_inside = false;
                for (int x = lo.x; x <= hi.x; ++x) {
                    bool inside = all_components_positive(bary);
                    last_inside |= inside;
                    if (inside) {
                        // depth and w values are interpolated with no perspective (See: OpenGL 4.6 spec 14.10)
                        p.z = glm::dot(bary, zs);
                        p.w = glm::dot(bary, ws);

                        // scuffed depth test
                        // TODO: stencil testing? maybe i should just use a float depth buffer and forget about stencils
                        // much easier computationally than to do float->int conversion all the time.
                        depth_t incoming(float_to_depth(p.z));
                        depth_t &stored(this->fbo.depth(x, y));

                        if (incoming.depth < stored.depth) {
                            // depth test passed
                            stored = incoming;
                            // Vertex attributes (e.g.: color, textures) are interpolated with perspective (See: OpenGL 4.6 spec 14.09)
                            // p.w == bary.x/a.w + bary.y/b.w + bary.z/c.w (ws is inverse w, see `clip2ndc`
                            // bary * ws == {bary.x/a.w, bary.y/b.w, bary.z/c.w}
                            // TODO: replace separate dot product and multiplication w/ one mul + horizontal sum (less latency i believe)
                            auto psc_bary(bary * ws / p.w);
                            avec4 frag_attrs[NAttrs];
                            for (int i = 0; i < NAttrs; ++i) {
                                frag_attrs[i] = interpolate_attribute(psc_bary, attr_xs[i], attr_ys[i], attr_zs[i], attr_ws[i]);
                            }

                            auto frag_color = evaluate_frag_shader(p, frag_attrs, std::make_index_sequence<NAttrs>{});
                            this->fbo.pixel(x, y) = vec4_to_color32(frag_color);
                        }
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

    void vert_shader(avec4 &position, glm::vec4 &color) {
        
    }

    avec4 frag_shader(const avec4 &frag_coord, const avec4 &frag_color) {
        return avec4(.5f + .5f * glm::vec3(sin(frag_coord)), 1.0f);
    }
}

int main(int argc, char **argv) {
    hamlet::render_context rc(4096, 4096);

    glm::vec4 r[] = {{ 1, 0, 0, 1.0 }};
    glm::vec4 g[] = {{ 0, 1, 0, 1.0 }};
    glm::vec4 b[] = {{ 0, 0, 1, 1.0 }};

    auto start = std::chrono::high_resolution_clock::now();
    rc.draw_triangle({ -1, -1, -0.5, 1 }, { 0, 1, -0.5, 1 }, { 1, -1, -0.5, 1 }, r, g, b);
    rc.draw_triangle({ -0.5f, 0.5f, 0, 1}, { 0.5f, 0.5f, 0, 1}, { -0.25f, -0.25f, 0, 1}, b, g, r);
    auto end = std::chrono::high_resolution_clock::now();
    fmt::print("Rendering took: {}s", std::chrono::duration<double>(end - start).count());
    rc.fbo.write_to_tga("test.tga");
}

