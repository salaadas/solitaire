#pragma once

#include "common.h"
#include "entities.h"

// This is used for matching what key press maps to highlight of what card.
struct Keymap_Highlight_Entry // :Rename
{
    _type_Type card_type = _make_Type(Card);

    //
    // Used for numbers:
    //
    Rank number_rank;

    //
    // Used for dragons:
    //
    Color dragon_color;
};

enum class In_Game_State
{
    DEALING = 0,
    PLAYING,
    END_GAME,
};

struct Undo_Handler;
struct Entity_Manager
{
    In_Game_State in_game_state;
    i64 current_level_seed = 0;

    Default_Game_Visuals *game_visuals = NULL;

    bool is_dragging = false;

    RArr<Card*>   all_cards;  // @Robustness @Cleanup: We really should consider using bucket arrays.
    RArr<Stack*>  all_stacks;

    SArr<Stack*>  base_pile;

    Stack *dragging_stack = NULL;
    Stack *dealing_stack  = NULL;

    // @Cleanup: :DelayHack
    // SUPER SUPER @Hack: This will be added to the current time of each iteration to figure out how much to
    // delay a movement of a card.
    // Then, the first frame afterwards that does not add any visual interpolation will reset this.
    f32 total_card_delay_time = 0;
    i64 total_visual_interpolations_added_this_frame = 0;

    RArr<Card*> moving_cards; // This will replace the two things above.

    Pid next_pid_to_issue = 1;
    Transaction_Id next_transaction_to_issue = 0;

    i64 win_count = 0; // @Cleanup: Move to Campaign_State?

    Undo_Handler *undo_handler = NULL;

    Vector2 card_size; // General size for a card.
    bool card_size_is_set = false;

    //
    // Game window properties:
    //
    bool has_set_offscreen_buffer_properties = false;
    Vector2 game_window_offset;
    Vector2 game_window_size;
    bool should_resize_offscreen_buffer = true;
    // Texture_Map *last_renderred_texture_map = NULL; @Incomplete:

    // @Incomplete: Maybe we also want the game_window_scale too.

    //
    // Function pointers:
    //
    void (*init_textures)(Entity_Manager *manager)             = NULL;
    void (*post_move_reevaluate)(Entity_Manager *manager)      = NULL;
    void (*post_undo_reevaluate)(Entity_Manager *manager)      = NULL;
    void (*level_from_seed)(Entity_Manager *manager, i64 seed) = NULL;

    bool (*is_done_with_dealing)(Entity_Manager *manager)      = NULL;

    Stack* (*can_move)(f32 release_mouse_x, f32 release_mouse_y, Stack *stack_to_move) = NULL;
    bool (*can_drag)(Card *card) = NULL;

    void (*draw)(Entity_Manager *manager) = NULL;

    // These are optional.
    void (*check_for_cards_highlight)(Entity_Manager *manager);
    void (*add_card_to_type_array)(Card *card) = NULL;
    bool (*handle_game_mouse_pointer_events)(Entity_Manager *manager, f32 mouse_x, f32 mouse_y, bool pressed) = NULL;
    bool (*condition_for_post_move_reevaluate)(Card *card) = NULL;
};

enum class Solitaire_Variant : u8
{
    UNKNOWN = 0,
    SHENZHEN,
    SAWAYAMA,
    KABUFUDA,
    CLUJ,
};

// In 2D this is flipped because our rendering system is right-handed.
constexpr auto Z_NEAR = -200.f;
constexpr auto Z_FAR  = 0.f;

extern f32 mouse_pointer_x, mouse_pointer_y;

constexpr auto DEVELOPER_MODE         = true;
constexpr auto MOVE_EVERYWHERE_CHEAT  = false;
constexpr auto DEBUG_HITBOX           = false;
constexpr auto DEBUG_RELEASE_REGION   = false;
constexpr auto DEBUG_NO_SHUFFLE       = false;

void init_game();
void read_input();
void simulate();

Entity_Manager *get_current_entity_manager();

void card_delay_start_of_frame_update(); // :DelayHack
void card_delay_end_of_frame_update();   // :DelayHack

void reset_visual_interpolation(Card *card);

bool sort_card_z_layers(Card *a, Card *b);

Stack *get_stack(Card *card);
void set_stack(Card *card, Stack *stack);
Stack *get_previous_stack(Card *card);
void set_previous_stack(Card *card, Stack *stack);

void set_dealing_stack(Entity_Manager *manager, Stack *stack);

my_pair<Stack* /* first stack with matching color */, Stack* /* first unoccupied stack */> find_stack_to_place_card(Color color, _type_Type card_type, SArr<Stack*> pile);

// If called in a loop, supply the iteration_index.
void move_one_card(Move_Type move_type, Card *card, Vector2 visual_end, Stack *destination_stack, i64 iteration_index = 0, f32 duration = -1, f32 delay_duration = -1);

// This makes the visual end be the visual start of the destination stack.
void move_one_card(Move_Type move_type, Card *card, Stack *destination_stack, i64 iteration_index = 0, f32 duration = -1, f32 delay_duration = -1);

Stack *create_stack(Entity_Manager *manager, Stack_Type stack_type, i64 z_layer_offset, Rect release_region);

void shuffle(RArr<Card*> *cards);

Card *get_card_under_cursor(Entity_Manager *manager, f32 mouse_x, f32 mouse_y);

bool is_inside(f32 x, f32 y, Rect r);
bool is_inside_circle(f32 x, f32 y, Vector2 center, f32 radius);

void get_quad(Rect r, Vector2 *p0, Vector2 *p1, Vector2 *p2, Vector2 *p3);
Rect get_rect(f32 x, f32 y, f32 w, f32 h);
Vector2 top_left(Rect rect);

void set_general_card_size(Entity_Manager *manager, f32 width, f32 height);
Card *register_general_card(Entity_Manager *manager, _type_Type card_type, void *derived_pointer);

void deal_one_card_with_offset(Card *card, Stack *to_stack, i64 iteration_index);

void set_game_visuals(Entity_Manager *manager, Default_Game_Visuals *default_visuals);

void add_visual_interpolation(Move_Type move_type, Card *card, Vector2 visual_end, Stack *destination_stack, i64 row_position_after_move, f32 duration = -1, f32 delay_duration = -1);

void set_game_window_properties(Entity_Manager *manager, Vector2 board_size);

// @Cleanup: For faster build, only forward declare this.
template <typename T>
T *Down(Card *card)
{
    assert(cmp_var_type_to_type(card->card_type, T));
    return reinterpret_cast<T*>(card->derived_pointer);
}
