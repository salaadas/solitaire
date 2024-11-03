#pragma once

#include "common.h"
#include "entities.h"

struct Cluj_Card
{
    Card *card = NULL;
    Rank rank;

    //
    // :ClujUndoHack
    // @Note: When the card cheats, 'is_inverted' from the base Card is toggled.
    //
    //

    Texture_Map *inverted_map = NULL;
    Texture_Map *glitch_map   = NULL; // For the V, D, K, T cards, we have a glitch when picking up a cheat card.
};

constexpr auto CLUJ_GAME_SCALE = 1.8f;
constexpr auto CLUJ_BASE_COLUMNS_COUNT = 6;

struct Cluj_Game_Visuals
{
    f32 default_playfield_width  = 520 * CLUJ_GAME_SCALE;
    f32 default_playfield_height = 480 * CLUJ_GAME_SCALE;

    // f32 dealing_stack_x = (default_playfield_width - 71 * CLUJ_GAME_SCALE) * .5f;
    // f32 dealing_stack_y = default_playfield_height - 3/5.f * 71 * CLUJ_GAME_SCALE;

    f32 dealing_stack_x = 224.5f * CLUJ_GAME_SCALE;
    f32 dealing_stack_y = 437.4f * CLUJ_GAME_SCALE;

    f32 base_y = 320 * CLUJ_GAME_SCALE;

    f32 x_1 =  25.f * CLUJ_GAME_SCALE;
    f32 x_2 = 105.f * CLUJ_GAME_SCALE;
    f32 x_3 = 185.f * CLUJ_GAME_SCALE;
    f32 x_4 = 264.f * CLUJ_GAME_SCALE;
    f32 x_5 = 344.f * CLUJ_GAME_SCALE;
    f32 x_6 = 424.f * CLUJ_GAME_SCALE;

    f32 base_x[CLUJ_BASE_COLUMNS_COUNT] = {x_1, x_2, x_3, x_4, x_5, x_6};

    f32 card_fixed_pixel_offset_amount = .24f * CLUJ_BASE_COLUMNS_COUNT;

    Default_Game_Visuals default_game_visuals;

    //
    // Z layer offsets:
    //
    i64 base_z_layer_offset       = 0;
    i64 deal_stack_z_layer_offset = 10;
    i64 dragging_z_layer_offset   = 100;

    Cluj_Game_Visuals()
    {
        default_game_visuals.default_card_row_offset = .11f  * CLUJ_GAME_SCALE;

        default_game_visuals.default_dealing_visual_duration = .18f;
        default_game_visuals.default_dealing_delay_duration  = .06f;
    }
};

struct Entity_Manager;
void init_cluj_manager(Entity_Manager *m);
