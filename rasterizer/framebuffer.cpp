#include "framebuffer.h"

#include "fmt/format.h"
#include "fmt/color.h"

int main(void) {
    fmt::print(fmt::fg(fmt::color::red) | fmt::emphasis::bold, "Hello World!\n");
}