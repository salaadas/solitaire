// :PixelCoord
// Right now, we are using the pixel coordinates, which is kinda yikes.... also, they are starting at the top right..

#include "draw.h"
#include "main.h"
#include "opengl.h"
#include "hud.h"
#include "solitaire.h"

Shader *shader_argb_and_texture;
Shader *shader_argb_no_texture;
Shader *shader_text;

#define cat_find(catalog, name) catalog_find(&catalog, String(name));
void init_draw()
{
    //
    // Shaders:
    //
    shader_argb_no_texture   = cat_find(shader_catalog, "argb_no_texture");  assert(shader_argb_no_texture);
    shader_argb_and_texture  = cat_find(shader_catalog, "argb_and_texture"); assert(shader_argb_and_texture);
    shader_text              = cat_find(shader_catalog, "text");             assert(shader_text);

    shader_argb_no_texture->backface_cull  = false;
    shader_argb_and_texture->backface_cull = false;
}
#undef cat_find

void rendering_2d_right_handed()
{
    f32 w = render_target_width;
    f32 h = render_target_height;
    if (h < 1) h = 1;

    auto tm = glm::ortho(0.f, w, 0.f, h, Z_NEAR, Z_FAR);

    view_to_proj_matrix    = tm;
    world_to_view_matrix   = Matrix4(1.0);
    object_to_world_matrix = Matrix4(1.0);

    refresh_transform();
}

void rendering_2d_right_handed_unit_scale()
{
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!
    // @Bug: This function does not take in x and y in range of [-1 to 1] like I thought!

    // @Note: cutnpaste from rendering_2d_right_handed
    f32 h = render_target_height / (f32)render_target_width;

    // This is a GL-style projection matrix mapping to [-1, 1] for x and y
    auto tm = Matrix4(1.0);
    tm[0][0] = 2;
    tm[1][1] = 2 / h;
    tm[3][0] = -1;
    tm[3][1] = -1;

    view_to_proj_matrix    = tm;
    world_to_view_matrix   = Matrix4(1.0);
    object_to_world_matrix = Matrix4(1.0);

    refresh_transform();
}

void draw_generated_quads(Dynamic_Font *font, Vector4 color, f32 theta, f32 z_layer)
{
    rendering_2d_right_handed();
    set_shader(shader_text);

    // glBlendFunc(GL_SRC1_COLOR, GL_ONE_MINUS_SRC1_COLOR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, 1.0); // @Investigate: What is anisotropy have to do with font rendering?
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    GLuint last_texture_id = 0xffffffff;

    immediate_begin();

    for (auto quad : font->current_quads)
    {
        auto page = quad.glyph->page;
        auto map  = &page->texture;

        if (page->dirty)
        {
            page->dirty = false;
            auto bitmap = page->bitmap_data;

            // Regenerating the texture. Or should we not?
            {
                // if (map->id == 0xffffffff || !map->id)
                {
                    // printf("Generating a texture for font page\n");
                    glGenTextures(1, &map->id);
                    glBindTexture(GL_TEXTURE_2D, map->id);
                }

                map->format = bitmap->XXX_format;
                update_texture_from_bitmap(map, bitmap);
            }
        }

        if (map->id != last_texture_id)
        {
            // @Speed
            // This will cause a flush for every call to draw_text.
            // But if we don't do this then we won't set the texture.
            // Need to refactor the text rendering code so that we don't have to deal with this
            immediate_flush();
            last_texture_id = map->id;
            set_texture(String("diffuse_texture"), map);
        }

        immediate_letter_quad(quad, color, theta, z_layer);
//        immediate_flush();
    }

    immediate_flush();
}

void draw_prepared_text(Dynamic_Font *font, i64 x, i64 y, Vector4 color, f32 theta, f32 z_layer)
{
    generate_quads_for_prepared_text(font, x, y);
    draw_generated_quads(font, color, theta, z_layer);
}

i64 draw_text(Dynamic_Font *font, i64 x, i64 y, String text, Vector4 color)
{
    auto width = prepare_text(font, text);
    draw_prepared_text(font, x, y, color);

    return width;
}

/*
void round(Vector2 *p)
{
    p->x = floorf(p->x + .5f);
    p->y = floorf(p->y + .5f);
}
*/

void immediate_image(Vector2 position, Vector2 size, Vector4 color, f32 theta, bool relative_to_render_height, f32 z_layer)
{
    assert((current_shader == shader_argb_and_texture) || (current_shader == shader_argb_no_texture));

    auto h = Vector2(size.x*.5f, 0);
    auto v = Vector2(0, size.y*.5f);

    auto pos = position;

    if (relative_to_render_height)
    {
        h.x *= render_target_height;
        v.y *= render_target_height;

        pos *= Vector2(render_target_width, render_target_height);
    }

    if (theta)
    {
        auto radians = theta * (TAU / 360.0f);

        h = rotate(h, radians);
        v = rotate(v, radians);
    }

    auto p0 = pos - h - v;
    auto p1 = pos + h - v;
    auto p2 = pos + h + v;
    auto p3 = pos - h + v;

    immediate_quad(p0, p1, p2, p3, argb_color(color), z_layer);
}

Vector2 size_of_rect(Rect r)
{
    return Vector2(r.w, r.h);
}

void immediate_quad_from_top_left(Vector2 top_left, Vector2 size, Vector4 fcolor, f32 z_layer, f32 theta)
{
    if (theta)
    {
        auto center = top_left + Vector2(size.x * .5f, -size.y * .5f);
        immediate_image(center, size, fcolor, theta, false, z_layer);
    }
    else
    {
        auto p3 = top_left;
        auto p0 = top_left - Vector2(0, size.y);
        auto p1 = p0 + Vector2(size.x, 0);
        auto p2 = p3 + Vector2(size.x, 0);

        auto icolor = argb_color(fcolor);
        immediate_quad(p0, p1, p2, p3, icolor, z_layer);
    }
}

void maybe_size_offscreen_buffer_for_game(Entity_Manager *manager)
{
    if (!manager->should_resize_offscreen_buffer) return;

    manager->should_resize_offscreen_buffer = false;
    auto window_size = manager->game_window_size;

    the_offscreen_buffer->width  = window_size.x;
    the_offscreen_buffer->height = window_size.y;
    size_color_target(the_offscreen_buffer, false);

    the_depth_buffer->width  = window_size.x;
    the_depth_buffer->height = window_size.y;

    size_depth_target(the_depth_buffer);

    // @Temporary: Re-calculate the offset to position the offscreen buffer to the middle of the screen.
    // This code will go once we do the active window.

    // @Incomplete: For now, every game will be positioned at the center of the screen, but
    // we want to move the windows around later.
    auto bb_size = Vector2(the_back_buffer->width, the_back_buffer->height);

    manager->game_window_offset = (bb_size - manager->game_window_size) * Vector2(.5f);
}

void draw_game()
{
    auto manager = get_current_entity_manager();

    assert(manager->has_set_offscreen_buffer_properties);
    if (was_window_resized_this_frame) manager->should_resize_offscreen_buffer = true;
    maybe_size_offscreen_buffer_for_game(manager);

    set_render_target(0, the_offscreen_buffer, the_depth_buffer);
    glClearColor(.13, .20, .23, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    assert(manager->draw);
    manager->draw(manager);

    // @Incomplete: Rewrite this so that we have better full-screen.
    {
        set_render_target(0, the_back_buffer);
        glClear(GL_COLOR_BUFFER_BIT);
        rendering_2d_right_handed();

        set_shader(shader_argb_and_texture);

        // f32 w = (f32)(the_offscreen_buffer->width);
        // f32 h = (f32)(the_offscreen_buffer->height);

        auto w = manager->game_window_size.x;
        auto h = manager->game_window_size.y;

        auto ox = manager->game_window_offset.x;
        auto oy = manager->game_window_offset.y;

        auto p0 = Vector2(ox,         oy);
        auto p1 = Vector2(ox + w,     oy);
        auto p2 = Vector2(ox + w, oy + h);
        auto p3 = Vector2(ox,     oy + h);

        set_texture(String("diffuse_texture"), the_offscreen_buffer);
        immediate_begin();
        immediate_quad(p0, p1, p2, p3, 0xffffffff);
        immediate_flush();
    }

    draw_hud();
    assert(num_immediate_vertices == 0);
}
