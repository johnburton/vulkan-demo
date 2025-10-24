#pragma once
#include "common.h"

void assets_load_shaders(Shader_Data* shaders, const char* vert_path, const char* frag_path);
void assets_free_shaders(Shader_Data* shaders);