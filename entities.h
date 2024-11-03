#pragma once

#include "common.h"
#include "draw.h"

struct Texture_Map;
struct Stack;

typedef u32 Transaction_Id;
typedef u32 Pid;

typedef i32 Rank; // [MIN_RANK...MAX_RANK]

// @Cleanup: Author all the places that convert from Color to int.
enum class Color : u8
{
    // Shenzhen.
    RED = 0,
    GREEN,
    WHITE,
    SHENZHEN_FUNDAMENTAL_COUNT,
    FLOWER,

    // Sawayama.
    HEART = 0,
    LEAF,
    ARROW,
    DIAMOND,
    SAWAYAMA_FUNDAMENTAL_COUNT,
};

enum class Move_Type : u8
{
    PLACE = 0,
    PICK,
    DEAL,
    AUTOMOVE,
    FAILED_MOVE,
    DRAW_CARD,
    UNIFY,
};

struct Entity_Manager;
struct Card
{
    Pid card_id = 0;
    _type_Type card_type = _make_Type(Card);

    void *derived_pointer = NULL;

    Entity_Manager *entity_manager = NULL;

    Texture_Map *large_icon = NULL;
    Texture_Map *small_icon = NULL;

    Texture_Map *front_map    = NULL;
    Texture_Map *back_map     = NULL;
    Texture_Map *shadow_map   = NULL;
    Texture_Map *textured_map = NULL;

    bool is_inverted = false; // @Hack :ClujUndoHack, if this is a one-off thing, I can live with this hack, but if here is another game that requires special serialization, refactor the undo system.

    Color color;

    i64 in_stack_index = -1;

    i64 previous_stack_index = -1; // This gets assigned everytime we start dragging it. If this card has some cascades below it when dragging, the cards below will have its 'previous_stack' assigned too.
    Vector2 previous_visual_position;

    Stack *visual_end_at_stack = NULL; // @Fixme: Get rid of this!!!

    Transaction_Id in_transaction_id = 0;
    bool did_physical_move = false;

    i64 row_position = 0; // Position within the stack where 0 means the most occluded card.

    i64 z_layer = 0;
    i64 visual_end_z_layer = 0;
    i64 visual_start_z_layer = 0;

    Rect hitbox;

    //
    // Visual stuff.
    //
    Vector2 visual_start;    // The top left corner.
    Vector2 visual_position; // The top left corner.
    Vector2 visual_end;      // The top left corner.
    Move_Type visual_move_type;

    f32 visual_start_time = -1;
    f32 visual_elapsed  = 0;
    f32 visual_duration = 0;

    f32 highlight_duration = 0;
    bool should_highlight = false;
};

struct Number
{
    Card *card;
    Rank rank;
};

struct Flower
{
    Card *card;
};

struct Dragon
{
    Card *card;
};

struct Unknown_Card
{
    Card *card;
};

enum class Stack_Type
{
    BASE,
    DRAGON,
    FLOWER,
    FOUNDATION,
    DRAGGING,

    DRAW, // For Sawayama, this is the pile that we can draw cards.
    PICK, // For Sawayama, when we draw cards, the cards will move to the pick pile for us to pick from cards.

    TOP, // For Kabufuda.
};

struct Stack
{
    i64 stack_index = -1;

    RArr<Card*> cards;
    Stack_Type stack_type = Stack_Type::BASE;

    bool closed = false;

    // This is for empty foundation stacks when cards about to move to the cell but they have not yet started.
    // We mark this so that when the automove system will not register this stack as the destination stack.
    bool will_be_occupied = false;

    // The region for releasing the dragging stack to.
    // This is only used in when handling mouse dragging and releasing event.
    Rect release_region;  // @Note that the DRAGGING type does not use this.

    // The starting position of the stack, right now, this is @Incomplete
    // because it uses the top left corner of the 'release_region', which
    // is false because we may want to enlarge the release region for better dropping cards feel.
    Vector2 visual_start; // @Note that the DRAGGING type does not use this.

    i64 z_offset = 0;
};

struct Default_Game_Visuals
{
    f32 default_card_row_offset = .135f; // This is with respect to the card_size.y.

    f32 default_card_visual_duration = .17f; // In seconds.
    f32 default_card_delay_duration  = .1f;  // In seconds.

    f32 default_dealing_visual_duration = .17f; // In seconds.
    f32 default_dealing_delay_duration  = .1f;  // In seconds.

    // @Cleanup: Make this a parameter for the add_visual_interpolation function.
    i64 default_animating_z_layer_offset  = 80; // This must be higher than the total number of cards when in DEALING mode.
};
