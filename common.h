#pragma once
#include <stdint.h>

struct Shader_Data {
    char* vert_source;
    char* frag_source;
    uint32_t vert_size;
    uint32_t frag_size;
};