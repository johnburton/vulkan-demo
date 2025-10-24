#pragma once
#include "common.h"

struct Shader;

void render_init();
bool render_should_close();
void render_begin_frame();
void render_end_frame();

void render_create_shader(Shader** shader, Shader_Data* shader_data);
void render_destroy_shader(Shader* shader);