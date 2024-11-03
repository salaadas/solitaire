#pragma once

#include "common.h"
#include "entities.h"

constexpr auto SAWAYAMA_GAME_SCALE = 2.f;

constexpr auto SAWAYAMA_FOUNDATION_ROWS_COUNT = 4;
constexpr auto SAWAYAMA_BASE_COLUMNS_COUNT    = 7;

struct Sawayama_Game_Visuals
{
    // :PixelCoord.
    f32 foundation_x   =  13.f * SAWAYAMA_GAME_SCALE;
    f32 fy_1 = 358.f * SAWAYAMA_GAME_SCALE;
    f32 fy_2 = 268.f * SAWAYAMA_GAME_SCALE;
    f32 fy_3 = 178.f * SAWAYAMA_GAME_SCALE;
    f32 fy_4 =  88.f * SAWAYAMA_GAME_SCALE;

    f32 foundation_y[SAWAYAMA_FOUNDATION_ROWS_COUNT] = {fy_1, fy_2, fy_3, fy_4};

    f32 base_y = 263.f * SAWAYAMA_GAME_SCALE;

    f32 bx_1 = 100.f * SAWAYAMA_GAME_SCALE;
    f32 bx_2 = 165.f * SAWAYAMA_GAME_SCALE;
    f32 bx_3 = 230.f * SAWAYAMA_GAME_SCALE;
    f32 bx_4 = 295.f * SAWAYAMA_GAME_SCALE;
    f32 bx_5 = 360.f * SAWAYAMA_GAME_SCALE;
    f32 bx_6 = 424.f * SAWAYAMA_GAME_SCALE;
    f32 bx_7 = 490.f * SAWAYAMA_GAME_SCALE;

    f32 base_x[SAWAYAMA_BASE_COLUMNS_COUNT] = {bx_1, bx_2, bx_3, bx_4, bx_5, bx_6, bx_7};

    f32 draw_stack_y = 348.f * SAWAYAMA_GAME_SCALE;
    f32 draw_stack_x = bx_1;

    f32 pick_stack_x = bx_2;
    f32 pick_stack_y = draw_stack_y;

    Vector4 card_red_color   = Vector4(.921f, .349f, .309f, 1);
    Vector4 card_black_color = Vector4(.184f, .176f, .176f, 1);

    Default_Game_Visuals default_game_visuals;

    // This is the offset for the cards in the PICK stack.
    // the value is relative to the card size's width.
    f32 card_column_offset = .24f;

    f32 card_fixed_pixel_offset_amount = .3f * SAWAYAMA_GAME_SCALE;

    f32 win_banner_x        = 88.f * SAWAYAMA_GAME_SCALE;
    f32 win_banner_y        = 140.f * SAWAYAMA_GAME_SCALE;
    f32 win_banner_duration = .7f; // In seconds.

    f32 per_gloss_map_duration = .3f; // In seconds.

    //
    // Z layer offsets:
    //
    i64 base_z_layer_offset       = 0;
    i64 draw_z_layer_offset       = 10;
    i64 pick_z_layer_offset       = 10;
    i64 foundation_z_layer_offset = 10;
    i64 dragging_z_layer_offset   = 100;

    Sawayama_Game_Visuals()
    {
        default_game_visuals.default_card_row_offset = .20f;

        default_game_visuals.default_dealing_visual_duration = .25f;
        default_game_visuals.default_dealing_delay_duration  = .1f;
    }
};

struct Entity_Manager;
void init_sawayama_manager(Entity_Manager *manager);
