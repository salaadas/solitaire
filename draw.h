#pragma once

#include "common.h"

struct Shader;
struct Dynamic_Font;

extern Shader *shader_argb_and_texture;
extern Shader *shader_argb_no_texture;
extern Shader *shader_text;

struct Rect
{
    f32 x = 0, y = 0;
    f32 w = 0, h = 0;
};

void init_draw();

void rendering_2d_right_handed();
void rendering_2d_right_handed_unit_scale();

void draw_prepared_text(Dynamic_Font *font, i64 x, i64 y, Vector4 color, f32 theta = 0, f32 z_layer = 0);
i64 draw_text(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color);

void immediate_image(Vector2 position, Vector2 size, Vector4 color, f32 theta, bool relative_to_render_height = true, f32 z_layer = 0);

Vector2 size_of_rect(Rect r);
void immediate_quad_from_top_left(Vector2 top_left, Vector2 size, Vector4 fcolor, f32 z_layer = 0, f32 theta = 0);

void draw_game();
