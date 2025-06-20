// src/SJ_3DModel.h
#pragma once
#include <stdbool.h>

// Declara tus funciones y tipos públicos:
bool load_model_c(const char *path);
void render_model_c();
void cleanup_global_meshes(void);