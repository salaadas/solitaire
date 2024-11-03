#pragma once

#include "common.h"
#include "entities.h"

// The assets are in 4K, so we scale it down for now....
// Later, we actually want to handle resizing the offscreen buffer better.
constexpr auto KABUFUDA_GAME_SCALE = .54f; // @Incomplete: Handle game scales better later.

constexpr auto KABUFUDA_TOP_PILE_COLUMNS_COUNT = 4;
constexpr auto KABUFUDA_BASE_COLUMNS_COUNT = 8;

struct Kabufuda_Game_Visuals
{
    f32 x_1 = 99   * KABUFUDA_GAME_SCALE;
    f32 x_2 = 405  * KABUFUDA_GAME_SCALE;
    f32 x_3 = 711  * KABUFUDA_GAME_SCALE;
    f32 x_4 = 1017 * KABUFUDA_GAME_SCALE;
    f32 x_5 = 1322 * KABUFUDA_GAME_SCALE;
    f32 x_6 = 1628 * KABUFUDA_GAME_SCALE;
    f32 x_7 = 1933 * KABUFUDA_GAME_SCALE;
    f32 x_8 = 2238 * KABUFUDA_GAME_SCALE;

    f32 top_pile_y = 1564 * KABUFUDA_GAME_SCALE;
    f32 top_pile_x[KABUFUDA_TOP_PILE_COLUMNS_COUNT] = {x_3, x_4, x_5, x_6};

    f32 base_pile_y = 1051 * KABUFUDA_GAME_SCALE;
    f32 base_pile_x[KABUFUDA_BASE_COLUMNS_COUNT] = {x_1, x_2, x_3, x_4, x_5, x_6, x_7, x_8};

    Default_Game_Visuals default_game_visuals;

    f32 card_fixed_pixel_offset_amount = .54f; // @Fixme: Scale this according to KABUFUDA_SCALE == 1.

    f32 win_banner_y = 500 * KABUFUDA_GAME_SCALE;
    f32 win_banner_duration = .65f;

    f32 win_stamp_start_y = 370 * KABUFUDA_GAME_SCALE;
    f32 win_stamp_end_y = 360 * KABUFUDA_GAME_SCALE;
    f32 win_stamp_duration = .25f;

    f32 win_stamp_start_scale = 1.2f; // End scale is '1'.
    f32 win_stamp_start_alpha = .3f;  // End alpha is '1'.

    //
    // Z layer offsets:
    //
    i64 base_z_layer_offset       = 0;
    i64 top_pile_z_layer_offset   = 10;
    i64 dragging_z_layer_offset   = 100;
    i64 unify_z_layer_offset      = 90;

    Kabufuda_Game_Visuals()
    {
        default_game_visuals.default_card_row_offset = .16f; // @Fixme: Scale this according to the KABUFUDA_SCALE == 1

        default_game_visuals.default_dealing_visual_duration = .26f;
        default_game_visuals.default_dealing_delay_duration  = .08f;
    }
};

struct Entity_Manager;
void init_kabufuda_manager(Entity_Manager *manager);
