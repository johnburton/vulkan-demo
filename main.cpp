#include "render.h"

int main() {
    render_init();

    while (!render_should_close()) {
        render_begin_frame();
        render_end_frame();
    }
}