#include "framebuffer.h"

#include "fmt/format.h"
#include "fmt/color.h"
#include "glm/glm.hpp"


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

        void draw_triangle(glm::vec2 a, glm::vec2 b, glm::vec2 c) {
            this->ndc2viewport(a); this->ndc2viewport(b); this->ndc2viewport(c);
            float area = 1.0f / edge(c - a, b - a);

            glm::ivec2 hi = round(max(a, max(b, c))),
                       lo = round(min(a, min(b, c)));

            for (int y = lo.y; y <= hi.y; ++y) {
                for (int x = lo.x; x <= hi.x; ++x) {
                    glm::vec2 p(x + 0.5f, y + 0.5f);
                    glm::vec3 bary{edge(p - b, c - b), edge(p - c, a - c), edge(p - a, b - a)};
                    bool inside = glm::all(glm::greaterThanEqual(bary, glm::vec3(0)));
                    if (inside) {
                        this->fbo.pixel(x, y) = glm::u8vec4(255);
                    }
                }
            }
        }
    };
}

int main(void) {
    fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "Hello World!\n");
    hamlet::render_context rc(256, 256);
    rc.draw_triangle({-0.5f, -0.5f}, {0.f, 0.5f}, {0.5f, -0.5f});
    rc.fbo.write_to_tga("test.tga");
}

