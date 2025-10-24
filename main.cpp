#include "assets.h"
#include "common.h"
#include "render.h"

int main() {
    render_init();

    Shader_Data shader_data;
    assets_load_shaders(&shader_data, 
        "build/assets/shaders/triangle.vert.spv", 
        "build/assets/shaders/triangle.frag.spv");

    Shader* shader;
    render_create_shader(&shader, &shader_data);

    while (!render_should_close()) {
        render_begin_frame();
        render_end_frame();
    }

    render_destroy_shader(shader);
    assets_free_shaders(&shader_data);
}