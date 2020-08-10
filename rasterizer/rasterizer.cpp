#include "framebuffer.h"

#include "fmt/format.h"
#include "fmt/color.h"
#include "glm/glm.hpp"
#include <chrono>


namespace hamlet {
    float edge(glm::vec2 p, glm::vec2 d) {
        return glm::determinant(glm::mat2(p, d));
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

        void ndc2viewport(glm::vec2 &ndc) {
            ndc = (ndc+1.0f) * (this->size*0.5f) + this->origin;
        }

        void clear(glm::vec4 color = glm::vec4(0, 0, 0, 1)) {
            glm::u8vec4 c32(color * 255.f);
            std::fill_n(this->fbo.color_attachment(), this->fbo.num_pixels(), c32);
        }

        void draw_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c, glm::vec4 ac, glm::vec4 bc, glm::vec4 cc) {
            this->ndc2viewport(a); this->ndc2viewport(b); this->ndc2viewport(c);
            float inverse_area = 1.0f / edge(c - a, b - a);

            glm::ivec2 hi = round(max(a, max(b, c))),
                       lo = round(min(a, min(b, c)));

            for (int y = lo.y; y <= hi.y; ++y) {
                for (int x = lo.x; x <= hi.x; ++x) {
                    glm::vec2 p(x + 0.5f, y + 0.5f);
                    glm::vec3 bary{edge(p - b, c - b), edge(p - c, a - c), edge(p - a, b - a)};
                    bool inside = glm::all(glm::greaterThanEqual(bary, glm::vec3(0)));
                    if (inside) {
                        bary *= inverse_area;
                        float red = glm::dot(bary, {ac.r, bc.r, cc.r});
                        float green = glm::dot(bary, {ac.g, bc.g, cc.g});
                        float blue = glm::dot(bary, {ac.b, bc.b, cc.b});
                        float alpha = glm::dot(bary, {ac.a, bc.a, cc.a});
                        this->fbo.pixel(x, y) = glm::u8vec4(glm::vec4(red, green, blue, alpha) * 255.f);
                    }
                }
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
    rc.draw_triangle({ -0.5f, -0.5f}, { -0.25f, 0.25f}, { 0.5f, -0.5f}, r, g, b);
    rc.draw_triangle({ -0.5f, 0.5f}, { 0.5f, 0.5f}, { -0.25f, -0.25f}, b, g, r);
    auto end = std::chrono::high_resolution_clock::now();
    fmt::print("Rendering took: {}s", std::chrono::duration<double>(end - start).count());
    rc.fbo.write_to_tga("test.tga");
}

