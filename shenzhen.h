#pragma once

#include "common.h"
#include "entities.h"

struct Dragon_Button
{
    enum Dragon_Button_State
    {
        UP = 0,
        DOWN,
        ACTIVE,
        DRAGON_BUTTON_STATE_COUNT
    };

    Texture_Map *maps[DRAGON_BUTTON_STATE_COUNT];
    Dragon_Button_State state = Dragon_Button_State::UP;

    Vector2 center;

    // @Cleanup: We want to have the Color enum in here too.
};

constexpr auto SHENZHEN_DRAGON_TYPES_COUNT       = (i64)Color::SHENZHEN_FUNDAMENTAL_COUNT;
constexpr auto SHENZHEN_FOUNDATION_COLUMNS_COUNT = (i64)Color::SHENZHEN_FUNDAMENTAL_COUNT;
constexpr auto SHENZHEN_BASE_COLUMNS_COUNT       = 8;

struct Shenzhen_Game_Visuals
{
    // :PixelCoord Think about how we can remove the pixel coordinates....
    // These are here so that we can tweak them later, but for now, we can't because it is
    // aligned by pixel with respect to the game board.

    f32 DRAGON_BUTTON_X   = 533.f;
    f32 DRAGON_BUTTON_R_Y = 751.f;
    f32 DRAGON_BUTTON_G_Y = 668.f;
    f32 DRAGON_BUTTON_W_Y = 585.f;

    // @Cleanup: This is very hacky and bad because what if the size of the image change?
    // This radius is used both for visual and hitbox of the dragon buttons.
    f32 DRAGON_BUTTON_RADIUS = 36.0f; // Radius in pixels.

    f32 dragon_buttons_y[SHENZHEN_DRAGON_TYPES_COUNT] = {DRAGON_BUTTON_R_Y, DRAGON_BUTTON_G_Y, DRAGON_BUTTON_W_Y};

    f32 X_1 = 45.f;
    f32 X_2 = 197.f;
    f32 X_3 = 349.f;

    f32 X_4 = 501.f;
    f32 X_5 = 653.f;

    f32 X_6 = 805.f;
    f32 X_7 = 957.f;
    f32 X_8 = 1109.f;

    // // :Lefthanded
    // f32 FINALIZE_ROW_Y = 20.f;
    // f32 BASE_ROW_Y     = 283.f;

    f32 FINALIZE_ROW_Y = 785.f;
    f32 BASE_ROW_Y     = 522.f;

    f32 flower_x       = 614.f;

    f32 numbers_x[SHENZHEN_BASE_COLUMNS_COUNT]          = {X_1, X_2, X_3, X_4, X_5, X_6, X_7, X_8};
    f32 dragons_x[SHENZHEN_DRAGON_TYPES_COUNT]          = {X_1, X_2, X_3};
    f32 foundation_x[SHENZHEN_FOUNDATION_COLUMNS_COUNT] = {X_6, X_7, X_8};

    Vector4 red_number_color   = Vector4(.68, .16, .07, 1);
    Vector4 green_number_color = Vector4(.07, .43, .29, 1);
    Vector4 white_number_color = Vector4(.05, .05, .05, 1);

    // f32 card_row_offset = .135f; // This is with respect to the card_front height.

    // f32 automove_visual_duration = .25f; // In seconds.
    // f32 automove_delay_duration  = .14f; // In seconds.

    f32 end_game_visual_duration = .70f; // In seconds.
    f32 end_game_delay_duration  = .06f; // In seconds.

    Vector4 base_card_color = Vector4(1, 1, 1, 1);
    Vector4 default_card_highlight_color = Vector4(.98f, .94f, .69f, 1.f); // @Incomplete: We should provide more variety of colors for different highlights of cards. WE SHOULD MAKE HILIT COLOR A PART OF THE CARD STRUCT LATER.

    // @Fixme: Make this take into account the game scale too.
    f32 card_fixed_pixel_offset_amount = .5f; // We would like something with respect to the card's height.

    f32 end_y_line = -10.0f;

    //
    // Z layer offsets:
    //
    i64 base_z_layer_offset       = 0;
    i64 flower_z_layer_offset     = 10;
    i64 dragon_z_layer_offset     = 10;
    i64 foundation_z_layer_offset = 10;

    i64 dragging_z_layer_offset   = 100;
};

// extern Shenzhen_Game_Visuals game_visuals;

struct Entity_Manager;
void init_shenzhen_manager(Entity_Manager *manager);
