#include "assets.h"

#include <stdio.h>
#include <stdlib.h>

void assets_load_shaders(Shader_Data* shaders, const char* vert_path, const char* frag_path) {
    FILE* vert_file = fopen(vert_path, "rb");
    if (!vert_file) {
        fprintf(stderr, "Failed to open vertex shader file: %s\n", vert_path);
        exit(EXIT_FAILURE);
    }
    fseek(vert_file, 0, SEEK_END);
    shaders->vert_size = ftell(vert_file);
    fseek(vert_file, 0, SEEK_SET);
    shaders->vert_source = (char*)malloc(shaders->vert_size + 1);
    fread((void*)shaders->vert_source, 1, shaders->vert_size, vert_file);
    shaders->vert_source[shaders->vert_size] = '\0';
    fclose(vert_file);

    FILE* frag_file = fopen(frag_path, "rb");
    if (!frag_file) {
        fprintf(stderr, "Failed to open fragment shader file: %s\n", frag_path);
        free((void*)shaders->vert_source);
        exit(EXIT_FAILURE);
    }
    fseek(frag_file, 0, SEEK_END);
    shaders->frag_size = ftell(frag_file);
    fseek(frag_file, 0, SEEK_SET);
    shaders->frag_source = (char*)malloc(shaders->frag_size + 1);
    fread((void*)shaders->frag_source, 1, shaders->frag_size, frag_file);
    shaders->frag_source[shaders->frag_size] = '\0';
    fclose(frag_file);
}

void assets_free_shaders(Shader_Data* shaders) {
    free((void*)shaders->vert_source);
    free((void*)shaders->frag_source);
}