#pragma once
#include "common.h"

struct Material;
struct Shader;

void render_init();
void render_wait_idle();
bool render_should_close();
void render_begin_frame();
void render_end_frame();

void render_create_shader(Shader** shader, Shader_Data* shader_data);
void render_destroy_shader(Shader* shader);

void render_create_material(Material** material, Shader* shader);
void render_destroy_material(Material* material);
void render_draw(Material* material);