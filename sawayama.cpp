//
// In this game, 'small_icon' means the number texture map for the card,
// and the 'large_icon' means the graphics in the middle of the card,
// if the large_icon is NULL, we know that the card is within the range 1..10
// and we should render pattern inside accordingly.
//

// @Incomplete: Card shadow.
// @Incomplete: End game card glossy animation from top down.
// @Incomplete: Card should automove using the parabolic trajectory.
// @Incomplete: Auto move system should not move cards if that card hinders the progress of the game.

#include "sawayama.h"
#include "solitaire.h"
#include "main.h"
#include "opengl.h"
#include "draw.h"
#include "time_info.h"

static constexpr Rank MIN_RANK = 1;
static constexpr Rank MAX_RANK = 13;

static constexpr Rank J_RANK = 11;
static constexpr Rank Q_RANK = 12;
static constexpr Rank K_RANK = 13;

constexpr auto MAX_STARTING_CARDS_PER_STACK_COUNT = 7;

static Texture_Map *board_map;
static Texture_Map *card_front_map;
static Texture_Map *card_back_map;
static Texture_Map *card_shadow_map;
static Texture_Map *card_stack_map;

// Large icons.
static Texture_Map *red_king_map;
static Texture_Map *red_queen_map;
static Texture_Map *red_jack_map;

static Texture_Map *black_king_map;
static Texture_Map *black_queen_map;
static Texture_Map *black_jack_map;

static RArr<Texture_Map*> big_ace_maps;

// Small icons.
static RArr<Texture_Map*> number_maps;
static RArr<Texture_Map*> suits_maps;

// Victory textures.
static Texture_Map        *win_banner_map;
static RArr<Texture_Map*> red_king_gloss_maps;
static RArr<Texture_Map*> black_king_gloss_maps;

// @Incomplete: Make it gloss from the top down.
static i64 current_king_gloss_map_index = 0;
static f32 current_king_gloss_elapsed   = 0;

static f32 win_banner_elapsed = 0.f;

static Sawayama_Game_Visuals game_visuals;

static SArr<Stack*> foundation_pile;

Stack* draw_stack;
Stack *pick_stack;

bool is_red(Color color)
{
    return (color == Color::HEART) || (color == Color::DIAMOND);
}

static void Make(Entity_Manager *manager, Color color, Rank rank)
{
    assert((MIN_RANK <= rank) && (rank <= MAX_RANK));

    auto card = New<Number>();
    auto base = register_general_card(manager, _make_Type(Number), card);

    card->card = base;

    base->front_map  = card_front_map;
    base->back_map   = card_back_map;
    base->shadow_map = card_shadow_map;

    base->color = color;
    card->rank  = rank;

    base->small_icon = number_maps[rank - MIN_RANK];

    assert((color == Color::HEART) || (color == Color::LEAF) || (color == Color::ARROW) || (color == Color::DIAMOND));

    Texture_Map *large_map = NULL;
    if (rank == MIN_RANK)
    {
        large_map = big_ace_maps[(i64)color];
    }
    else if (rank == J_RANK)
    {
        if (is_red(color)) large_map = red_jack_map;
        else               large_map = black_jack_map;
    }
    else if (rank == Q_RANK)
    {
        if (is_red(color)) large_map = red_queen_map;
        else               large_map = black_queen_map;
    }
    else if (rank == K_RANK)
    {
        if (is_red(color)) large_map = red_king_map;
        else               large_map = black_king_map;
    }

    base->large_icon = large_map;
}

static void init_textures(Entity_Manager *manager)
{
    board_map = load_nearest_mipmapped_texture(String("sawayama_board"));

    // @Incomplete: We want better screen adjustments later
    {
        auto board_size = Vector2(board_map->width, board_map->height) * SAWAYAMA_GAME_SCALE;
        set_game_window_properties(manager, board_size);
    }

    card_front_map = load_nearest_mipmapped_texture(String("sawayama_front"));
    auto card_size = Vector2(card_front_map->width, card_front_map->height) * SAWAYAMA_GAME_SCALE;

    //
    // Override the card size in the Entity Manager with the scaled one.
    //
    set_general_card_size(manager, card_size.x, card_size.y);

    card_back_map = load_nearest_mipmapped_texture(String("sawayama_back"));
    card_shadow_map = load_nearest_mipmapped_texture(String("sawayama_card_shadow"));

    card_stack_map = load_nearest_mipmapped_texture(String("sawayama_card_stack"));

    // Small icons.
    auto add_map_to_array = [](RArr<Texture_Map*> *array, String name) {
        auto map = load_nearest_mipmapped_texture(name);
        array_add(array, map);
    };

    array_reserve(&number_maps, MAX_RANK - MIN_RANK + 1);
    add_map_to_array(&number_maps, String("sawayama_new_letter_a"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_2"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_3"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_4"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_5"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_6"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_7"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_8"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_9"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_10"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_j"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_q"));
    add_map_to_array(&number_maps, String("sawayama_new_letter_k"));

    // Kings, queens, and jacks.
    red_king_map  = load_nearest_mipmapped_texture(String("sawayama_red_king"));
    red_queen_map = load_nearest_mipmapped_texture(String("sawayama_red_queen"));
    red_jack_map  = load_nearest_mipmapped_texture(String("sawayama_red_jack"));

    black_king_map  = load_nearest_mipmapped_texture(String("sawayama_black_king"));
    black_queen_map = load_nearest_mipmapped_texture(String("sawayama_black_queen"));
    black_jack_map  = load_nearest_mipmapped_texture(String("sawayama_black_jack"));

    auto add_map_by_color = [](RArr<Texture_Map*> *array, String name, Color color) {
        auto map = load_nearest_mipmapped_texture(name);
        (*array)[(i64)color] = map;
    };

    // Aces.
    array_resize(&big_ace_maps, (i64)Color::SAWAYAMA_FUNDAMENTAL_COUNT);
    add_map_by_color(&big_ace_maps, String("sawayama_a_heart"),   Color::HEART);
    add_map_by_color(&big_ace_maps, String("sawayama_a_leaf"),    Color::LEAF);
    add_map_by_color(&big_ace_maps, String("sawayama_a_arrow"),   Color::ARROW);
    add_map_by_color(&big_ace_maps, String("sawayama_a_diamond"), Color::DIAMOND);

    // Suits.
    array_resize(&suits_maps, (i64)Color::SAWAYAMA_FUNDAMENTAL_COUNT);
    add_map_by_color(&suits_maps, String("sawayama_new_letter_heart"),   Color::HEART);
    add_map_by_color(&suits_maps, String("sawayama_new_letter_leaf"),    Color::LEAF);
    add_map_by_color(&suits_maps, String("sawayama_new_letter_arrow"),   Color::ARROW);
    add_map_by_color(&suits_maps, String("sawayama_new_letter_diamond"), Color::DIAMOND);

    // Win textures.
    win_banner_map = load_nearest_mipmapped_texture(String("sawayama_you_win"));
    add_map_to_array(&red_king_gloss_maps, String("sawayama_red_king_win_1"));
    add_map_to_array(&red_king_gloss_maps, String("sawayama_red_king_win_2"));
    add_map_to_array(&red_king_gloss_maps, String("sawayama_red_king_win_3"));
    add_map_to_array(&red_king_gloss_maps, String("sawayama_red_king_win_4"));

    add_map_to_array(&black_king_gloss_maps, String("sawayama_black_king_win_1"));
    add_map_to_array(&black_king_gloss_maps, String("sawayama_black_king_win_2"));
    add_map_to_array(&black_king_gloss_maps, String("sawayama_black_king_win_3"));
    add_map_to_array(&black_king_gloss_maps, String("sawayama_black_king_win_4"));

    assert(red_king_gloss_maps.count == black_king_gloss_maps.count);
}

static bool check_for_victory(Entity_Manager *manager)
{
    if (manager->moving_cards.count) return false;
    if (draw_stack->cards.count) return false;
    if (pick_stack->cards.count) return false;

    for (auto it : manager->base_pile) if (it->cards.count) return false;

    for (auto it : foundation_pile)
    {
        if (it->cards.count != (MAX_RANK - MIN_RANK + 1)) return false;
    }

    return true;
}

static void do_victory_animation_set_up(Entity_Manager *manager)
{
    manager->win_count += 1;
    manager->in_game_state = In_Game_State::END_GAME;

    //
    // Victory states:
    //
    win_banner_elapsed = 0;
    current_king_gloss_map_index = 0;
}

static void post_move_reevaluate(Entity_Manager *manager)
{
    if (manager->in_game_state != In_Game_State::PLAYING) return;
    if (manager->dragging_stack->cards.count) return; // Don't do automove while we are dragging stuff.
    if (manager->moving_cards.count) return; // There are still moving stuff, so don't reeval.

    auto highest_mark = get_temporary_storage_mark();
    defer {set_temporary_storage_mark(highest_mark);};
    
    RArr<Card*> considered;
    considered.allocator = {global_context.temporary_storage, __temporary_allocator};
    array_reserve(&considered, 1 + manager->base_pile.count); // Plus one to account for the last card of the PICK stack.

    for (auto stack : manager->base_pile)
    {
        if (!stack->cards.count) continue;
        auto last = array_peek_last(&stack->cards);
        
        array_add(&considered, last);
    }

    if (pick_stack->cards.count)
    {
        auto last = array_peek_last(&pick_stack->cards);
        array_add(&considered, last);
    }

    if (!draw_stack->closed && draw_stack->cards.count)
    {
        auto last = array_peek_last(&draw_stack->cards);
        array_add(&considered, last);
    }

    for (i64 it_index = 0; it_index < considered.count; ++it_index)
    {
        auto it = considered[it_index];
        assert(it->visual_start_time < 0);

        auto number = Down<Number>(it);
        auto [first_matching_stack, first_unoccupied_stack] = find_stack_to_place_card(it->color, it->card_type, foundation_pile);

        if (number->rank == MIN_RANK)
        {
            // If this card is the start of the foundation series, move it to the first empty stack.
            assert(first_unoccupied_stack->stack_type != Stack_Type::BASE);
            move_one_card(Move_Type::AUTOMOVE, it, first_unoccupied_stack);
            break;
        }
        else
        {
            // Only move if:
            //   a) There is a card with the same color but a rank lower by 1 already in the foundation pile.
            // AND
            //   b) The difference between the rank of this card and the other cards of other colors
            //      in the foundation pile is not greater than 2.
            Stack *destination_stack = NULL;
            bool should_move = true;

            for (auto stack : foundation_pile)
            {
                constexpr auto MAX_AUTOMOVE_DIFF = 2;

                if (!stack->cards.count)
                {
                    // For empty foundations, we assume that the max value there is 0.
                    // so if the rank of the number card is greater than 2, we cancel.
                    if (number->rank > MAX_AUTOMOVE_DIFF)
                    {
                        should_move = false;
                        break;
                    }

                    continue;
                }

                auto last = array_peek_last(&stack->cards);
                auto dest_number = Down<Number>(last);

                if (last->color == it->color)
                {
                    // We must be one higher.
                    if (number->rank - dest_number->rank != 1)
                    {
                        should_move = false;
                        break;
                    }

                    destination_stack = stack;
                }
                else
                {
                    if (number->rank - dest_number->rank > MAX_AUTOMOVE_DIFF)
                    {
                        should_move = false;
                        break;
                    }
                }
            }

            if (!should_move) continue;
            if (destination_stack)
            {
                assert(destination_stack->stack_type != Stack_Type::BASE);
                move_one_card(Move_Type::AUTOMOVE, it, destination_stack);
                break;
            }
        }
    }

    set_temporary_storage_mark(highest_mark); // So that we don't pegged the temporary storage.

    auto victory = check_for_victory(manager);
    if (victory)
    {
        do_victory_animation_set_up(manager);
    }
}

static void post_undo_reevaluate(Entity_Manager *manager)
{
    // @Hack:
    // Re-close the draw stack if there is more than one card in there.
    // We can do this because the cell in the draw stack can store at most one card at a time.
    // And 52 / 3 = 17.333 so there is 17 times we draw 3 cards and 1 time where we draw 2 cards,
    // so when we undo, we never reach a point where the draw stack can contain 1 card while it is
    // still in the draw state. This will be wrong if our deck changes count.
    //                                                                            October 31, 2024.
    draw_stack->closed = draw_stack->cards.count > 1;

    post_move_reevaluate(manager);
}

static void init_stack_piles(Entity_Manager *manager)
{
    auto gv = &game_visuals;

    // Dragging is a special stack that does not have a release region.
    manager->dragging_stack = create_stack(manager, Stack_Type::DRAGGING, gv->dragging_z_layer_offset, {});

    assert(manager->card_size_is_set);
    auto w = manager->card_size.x;      // :PixelCoord
    auto h = -1 * manager->card_size.y; // :PixelCoord

    // Foundations.
    array_resize(&foundation_pile, SAWAYAMA_FOUNDATION_ROWS_COUNT);
    for (i64 it_index = 0; it_index < SAWAYAMA_FOUNDATION_ROWS_COUNT; ++it_index)
    {
        auto release_region = get_rect(gv->foundation_x, gv->foundation_y[it_index], w, h);
        foundation_pile[it_index] = create_stack(manager, Stack_Type::FOUNDATION, gv->foundation_z_layer_offset, release_region);
    }

    // Base.
    array_resize(&manager->base_pile, SAWAYAMA_BASE_COLUMNS_COUNT);
    for (auto it_index = 0; it_index < SAWAYAMA_BASE_COLUMNS_COUNT; ++it_index)
    {
        // The release region for the base pile extends far down.
        auto release_region = get_rect(gv->base_x[it_index], gv->base_y, w, -1 * gv->base_y);
        auto stack = create_stack(manager, Stack_Type::BASE, gv->base_z_layer_offset, release_region);

        manager->base_pile[it_index] = stack;

        array_reserve(&stack->cards, MAX_STARTING_CARDS_PER_STACK_COUNT);
    }

    // Draw and pick.
    {
        auto release_region = get_rect(gv->draw_stack_x, gv->draw_stack_y, w, h);
        draw_stack = create_stack(manager, Stack_Type::DRAW, gv->draw_z_layer_offset, release_region);

        release_region = get_rect(gv->pick_stack_x, gv->pick_stack_y, w, h);
        pick_stack = create_stack(manager, Stack_Type::PICK, gv->pick_z_layer_offset, release_region);
    }
}

static void level_from_seed(Entity_Manager *manager, i64 seed)
{
    //
    // Resetting all the arrays and stacks.
    //
    array_reset(&manager->all_cards);
    array_reset(&manager->all_stacks);
    array_reset(&manager->moving_cards);

    init_stack_piles(manager);

    //
    // Game states.
    //
    manager->is_dragging = false;
    manager->in_game_state = In_Game_State::DEALING;
    manager->next_transaction_to_issue = 1; // Reset the transaction counter.

    manager->current_level_seed = seed;
    srand(manager->current_level_seed);

    array_reserve(&manager->all_cards, (MAX_RANK - MIN_RANK + 1) * (i64)Color::SAWAYAMA_FUNDAMENTAL_COUNT);
    array_reset(&manager->all_cards); // @Cleaup: Redundant with cleanup_level?

    for (auto rank = MIN_RANK; rank <= MAX_RANK; ++rank)
    {
        Make(manager, Color::HEART,   rank);
        Make(manager, Color::LEAF,    rank);
        Make(manager, Color::ARROW,   rank);
        Make(manager, Color::DIAMOND, rank);
    }

    if constexpr (!DEBUG_NO_SHUFFLE)
    {
        shuffle(&manager->all_cards);
    }

    set_dealing_stack(manager, draw_stack);

    for (auto it_index = 0; it_index < manager->all_cards.count; ++it_index)
    {
        auto it = manager->all_cards[it_index];

        set_stack(it, draw_stack);

        it->row_position = it_index;
        it->z_layer      = draw_stack->z_offset + it->row_position;
        it->visual_position = draw_stack->visual_start;

        array_add(&draw_stack->cards, it);

    }

    auto gv = &game_visuals;

    auto it_index = draw_stack->cards.count - 1;
    for (auto row = 0; row < MAX_STARTING_CARDS_PER_STACK_COUNT; ++row)
    {
        for (auto column = row; column < manager->base_pile.count; ++column)
        {
            auto card = draw_stack->cards[it_index];
            auto destination_stack = manager->base_pile[column];

            deal_one_card_with_offset(card, destination_stack, row);
            it_index -= 1;
        }
    }
}

static bool check_descending_of_numbers(Card *upper, Card *lower)
{
    auto c1 = (is_red(upper->color) && !is_red(lower->color)) || (!is_red(upper->color) && is_red(lower->color));
    if (!c1) return false;

    auto n1 = Down<Number>(upper);
    auto n2 = Down<Number>(lower);
    auto c2 = n1->rank - n2->rank == 1;
    if (!c2) return false;

    return true;
}

static Stack* can_move(f32 release_mouse_x, f32 release_mouse_y, Stack *stack_to_move)
{
    if (!stack_to_move->cards.count) return NULL;
    auto manager = stack_to_move->cards[0]->entity_manager;
    assert(manager);

    //
    // Determine which stack is under our cursor:
    //
    Stack *destination_stack = NULL;
    for (auto it : manager->all_stacks)
    {
        if (it->stack_type == Stack_Type::DRAGGING) continue;
        if (it == stack_to_move) continue;

        if (is_inside(release_mouse_x, release_mouse_y, it->release_region))
        {
            destination_stack = it;
            break;
        }
    }

    if (!destination_stack) return NULL;

    if constexpr (MOVE_EVERYWHERE_CHEAT)
    {
        return destination_stack;
    }

    switch (destination_stack->stack_type)
    {
        case Stack_Type::BASE: {
            if (destination_stack->cards.count > 0)
            {
                auto last_card_in_destination = array_peek_last(&destination_stack->cards);
                auto first_card_to_move = stack_to_move->cards[0];

                auto valid = check_descending_of_numbers(last_card_in_destination, first_card_to_move);
                if (!valid) break;
            }

            return destination_stack;
        } break;
        case Stack_Type::DRAW: {
            if (destination_stack->closed) break;
            if (destination_stack->cards.count) break;

            return destination_stack;
        } break;
        case Stack_Type::FOUNDATION: {
            if (stack_to_move->cards.count > 1) break;

            auto peeked = array_peek_last(&stack_to_move->cards);
            auto n = Down<Number>(peeked);

            if (!destination_stack->cards.count)
            {
                if (n->rank == MIN_RANK) return destination_stack;
            }
            else
            {
                auto last_card_in_destination = array_peek_last(&destination_stack->cards);
                auto other_n = Down<Number>(last_card_in_destination);
                if ((n->card->color == other_n->card->color) && (n->rank - other_n->rank == 1)) return destination_stack;
            }
        } break;
    }

    return NULL;
}

static bool can_drag(Card *card)
{
    auto stack = get_stack(card);
    assert(stack);
    
    if (stack->closed) return false;

    switch (stack->stack_type)
    {
        case Stack_Type::DRAW: {
            return stack->cards.count == 1;
        } break;
        case Stack_Type::FOUNDATION: {
            return false;
        } break;
        case Stack_Type::PICK: {
            // The card from the pick stack can only be drag one at a time starting from the last-most one.
            return card->row_position == (stack->cards.count - 1);
        } break;
        case Stack_Type::BASE: {
            if (!stack->cards.count) return false;

            for (auto row = card->row_position; row < (stack->cards.count - 1); ++row)
            {
                auto upper = stack->cards[row];
                auto lower = stack->cards[row + 1];

                if (!check_descending_of_numbers(upper, lower)) return false;
            }

            return true;
        } break;
        default: {
            assert(0);
            return false;
        }
    }
}

static bool should_render_card_stack_texture(Stack *stack)
{
    // Only render the stack if the count is > 1, because when the count is 1, we don't see it anyways.
    auto b = (stack->cards.count > 1) && (stack->closed || (stack->stack_type != Stack_Type::BASE && stack->stack_type != Stack_Type::DRAGGING));

    // Skip out on the PICKING and FOUNDATION stack.
    b = b && (stack->stack_type != Stack_Type::PICK && stack->stack_type != Stack_Type::FOUNDATION);
    return b;
}

static void render_card_from_top_left(Card *card)
{
    set_shader(shader_argb_and_texture);

    auto manager  = card->entity_manager;
    auto in_stack = get_stack(card);

    auto gv   = &game_visuals;
    auto vpos = card->visual_position;

    auto should_render_stack = should_render_card_stack_texture(in_stack);
    if (should_render_stack)
    {
        if (card->row_position == 0)
        {
            // @Incomplete: Missing shadow for the stack base.

            auto sp0 = vpos + Vector2(0, -manager->card_size.y);
            auto sp1 = sp0 + Vector2(manager->card_size.x, 0);

            auto csh = card_stack_map->height * SAWAYAMA_GAME_SCALE;
            auto sp2 = sp1 + Vector2(0, csh);
            auto sp3 = sp0 + Vector2(0, csh);

            set_texture(String("diffuse_texture"), card_stack_map);
            immediate_quad(sp0, sp1, sp2, sp3, 0xffffffff, card->z_layer);
        }

        auto y_offset = (in_stack->cards.count - 1) * gv->card_fixed_pixel_offset_amount;
        vpos.y += y_offset;
    }

    // @Incomplete: Drawing the card shadow.
    // @Incomplete: Drawing the card shadow.
    // @Incomplete: Drawing the card shadow.
    // @Incomplete: Drawing the card shadow.

    Texture_Map *card_map = NULL;
    if (in_stack->closed) card_map = card->back_map;
    else                  card_map = card->front_map;

    assert(card_map);
    set_texture(String("diffuse_texture"), card_map);
    immediate_quad_from_top_left(vpos, manager->card_size, Vector4(1, 1, 1, 1), card->z_layer);

    // Drawing the hitbox of the card.
    if constexpr (DEBUG_HITBOX)
    {
        set_shader(shader_argb_no_texture);
        immediate_quad_from_top_left(top_left(card->hitbox), Vector2(card->hitbox.w, card->hitbox.h), Vector4(0, 1, 1, .4f), card->z_layer);
        set_shader(shader_argb_and_texture);
    }

    if (in_stack->closed) return;

    auto n = Down<Number>(card);

    auto cw = manager->card_size.x;
    auto ch = manager->card_size.y;

    auto suit_map = suits_maps[(i64)card->color];
    auto sw = suit_map->width  * SAWAYAMA_GAME_SCALE;
    auto sh = suit_map->height * SAWAYAMA_GAME_SCALE;

    Vector4 color;
    if (is_red(card->color)) color = gv->card_red_color;
    else                     color = gv->card_black_color;

    if (manager->in_game_state == In_Game_State::END_GAME && n->rank == MAX_RANK)
    {
        // If we won, we do the glossy thing for the K cards.
        Texture_Map *card_map = NULL;
        if (is_red(card->color))
        {
            card_map = red_king_gloss_maps[current_king_gloss_map_index];
        }
        else
        {
            card_map = black_king_gloss_maps[current_king_gloss_map_index];
        }

        current_king_gloss_elapsed += timez.current_dt;
        if (current_king_gloss_elapsed >= gv->per_gloss_map_duration)
        {
            current_king_gloss_elapsed -= gv->per_gloss_map_duration;
            current_king_gloss_map_index = (current_king_gloss_map_index + 1) % red_king_gloss_maps.count;
        }

        set_texture(String("diffuse_texture"), card_map);
        immediate_quad_from_top_left(vpos, manager->card_size, Vector4(1, 1, 1, 1), card->z_layer);
    }
    else
    {
        // Large icon.
        if (card->large_icon)
        {
            auto card_center = vpos + Vector2(cw * .5f, -ch * .5f);

            auto iw = card->large_icon->width  * SAWAYAMA_GAME_SCALE;
            auto ih = card->large_icon->height * SAWAYAMA_GAME_SCALE;

            set_texture(String("diffuse_texture"), card->large_icon);
            immediate_image(card_center, Vector2(iw, ih), Vector4(1, 1, 1, 1), 0, false, card->z_layer);
        }
        else
        {
            // Drawing the large icons for the stuff that is not A, J, Q, K.
            i32 num_columns;
            if (n->rank < 4) num_columns = 1;
            else             num_columns = 2;

            auto stuff_per_column = static_cast<i32>(n->rank / num_columns);
            if (stuff_per_column > 4) stuff_per_column = 4;

            auto ypad = sh * .98f;

            auto start_y = vpos.y - ch + ypad;
            auto end_y   = vpos.y - ypad;

            set_texture(String("diffuse_texture"), suit_map);

            // Middle x.
            auto x = vpos.x + cw * .5f;

            // Left and right x.
            auto ox      = sw * .80f;
            auto x_left  = x - ox;
            auto x_right = x + ox;

            if (num_columns == 1)
            {
                for (auto it_index = 0; it_index < stuff_per_column; ++it_index)
                {
                    auto t = it_index / static_cast<f32>(stuff_per_column - 1);
                    auto y = lerp(start_y, end_y, t);

                    immediate_image(Vector2(x, y), Vector2(sw, sh), color, 0, false, card->z_layer);
                }
            }
            else
            {
                auto whats_left = n->rank - stuff_per_column * num_columns;
                auto middle_to_render = whats_left ? 1 : 0;

                auto total = whats_left + stuff_per_column;
                for (auto it_index = 0; it_index < total; ++it_index)
                {
                    auto t = it_index / static_cast<f32>(total - 1);
                    t = 1 - t; // To render from top down.
                    assert(t >= 0);
                    auto y = lerp(start_y, end_y, t);

                    if (middle_to_render)
                    {
                        if ((it_index == 1) || (it_index == total-2))
                        {
                            immediate_image(Vector2(x, y), Vector2(sw, sh), color, 0, false, card->z_layer);

                            whats_left -= 1;
                            if (!whats_left) middle_to_render = 0;
                            else             middle_to_render += 1;

                            continue;
                        }
                    }

                    immediate_image(Vector2(x_left,  y), Vector2(sw, sh), color, 0, false, card->z_layer);
                    immediate_image(Vector2(x_right, y), Vector2(sw, sh), color, 0, false, card->z_layer);
                }
            }
        }
    }

    {
        auto ox = cw * .058f;
        auto oy = ch * .025f;

        // Draw the stuff below the number.
        {
            set_texture(String("diffuse_texture"), suit_map);
            auto stuff_oy = oy + sh * .85f;

            auto top_left = vpos;
            top_left.x += ox;
            top_left.y -= stuff_oy;
            immediate_quad_from_top_left(top_left, Vector2(sw, sh), color, card->z_layer);

            auto bottom_right = vpos + Vector2(cw, -ch);
            bottom_right += Vector2(-sw, sh);
            bottom_right.x -= ox;
            bottom_right.y += stuff_oy;
            immediate_quad_from_top_left(bottom_right, Vector2(sw, sh), color, card->z_layer, 180);
        }

        // The rank.
        set_texture(String("diffuse_texture"), card->small_icon);

        // Rank width, height.
        auto rw = card->small_icon->width  * SAWAYAMA_GAME_SCALE;
        auto rh = card->small_icon->height * SAWAYAMA_GAME_SCALE;

        auto top_left = vpos;
        top_left.x += ox;
        top_left.y -= oy;
        immediate_quad_from_top_left(top_left, Vector2(rw, rh), color, card->z_layer);

        auto bottom_right = vpos + Vector2(cw, -ch);
        bottom_right += Vector2(-rw, rh);
        bottom_right.x -= ox;
        bottom_right.y += oy;
        immediate_quad_from_top_left(bottom_right, Vector2(rw, rh), color, card->z_layer, 180);
    }
}

static void draw(Entity_Manager *manager)
{
    rendering_2d_right_handed();

    {
        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), board_map);

        auto w = render_target_width;
        auto h = render_target_height;

        auto p0 = Vector2(0, 0);
        auto p1 = Vector2(w, 0);
        auto p2 = Vector2(w, h);
        auto p3 = Vector2(0, h);

        immediate_quad(p0, p1, p2, p3, 0xffffffff);
        immediate_flush();
    }

    auto gv = &game_visuals;

    if (DEBUG_RELEASE_REGION)
    {
        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), card_front_map);

        immediate_quad_from_top_left(draw_stack->visual_start, size_of_rect(draw_stack->release_region), Vector4(0, 1, 0, .6));
        immediate_quad_from_top_left(pick_stack->visual_start, size_of_rect(pick_stack->release_region), Vector4(0, 1, 0, .6));

        for (auto it : manager->base_pile)
        {
            immediate_quad_from_top_left(it->visual_start, size_of_rect(it->release_region), Vector4(1, .3, 0, .6));
        }

        for (auto it : foundation_pile)
        {
            immediate_quad_from_top_left(it->visual_start, size_of_rect(it->release_region), Vector4(1, .3, .8, .6));
        }
    }

    //
    // Drawing the cards.
    //
    
    // First we draw the base pile, we do this separately because cards can have transparency
    // and so we want the base pile to be rendered from left to right, first to last.
    for (auto stack : manager->base_pile)
    {
        for (auto card : stack->cards)
        {
            // Only render non-moving cards in the base pile in this loop.
            if (card->visual_start_time < 0) render_card_from_top_left(card);
        }
    }
    
    // Then draw all the rest of the static cards. After that is done, we then draw the animating cards.
    // This is because the animtating cards have transparency and so we must draw them after we have the whole scene.
    //
    // Also, we need to sort the animating cards based on their z_layer.
    //
    // This array not only contains animating cards but also foundations and dragon piles one.
    //
    // @Cleanup: We should consider rendering things separately....
    RArr<Card*> animating_cards;
    animating_cards.allocator = {global_context.temporary_storage, __temporary_allocator};

    array_reserve(&animating_cards, manager->all_cards.count); // @Bug @Fixme: If we don't do a array reserve here, we will encounter some nasty problems. (We also have problems if we reserve it with the size of moving_cards.count?????

    for (auto it : manager->all_cards)
    {
        auto stack = get_stack(it);
        if ((stack->stack_type != Stack_Type::BASE) || ((it->visual_start_time >= 0)))
        {
            array_add(&animating_cards, it);
        }
    }

    array_qsort(&animating_cards, sort_card_z_layers);

    for (auto it : animating_cards)
    {
        render_card_from_top_left(it);
    }

    if (manager->in_game_state == In_Game_State::END_GAME)
    {
        auto duration = gv->win_banner_duration;

        if (win_banner_elapsed < duration)
        {
            win_banner_elapsed += timez.current_dt;
            if (win_banner_elapsed > duration) win_banner_elapsed = duration;
        }

        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), win_banner_map);

        auto w_max = win_banner_map->width  * SAWAYAMA_GAME_SCALE;

        if (w_max == 0) w_max = 1; // @Robustness: Add epsilon.
        auto w_t = win_banner_elapsed / duration;

        auto w = lerp(0, w_max, w_t);
        auto h = win_banner_map->height * SAWAYAMA_GAME_SCALE;

        auto p3 = Vector2(gv->win_banner_x, gv->win_banner_y);
        auto p0 = p3 - Vector2(0, h);
        auto p1 = p0 + Vector2(w, 0);
        auto p2 = p3 + Vector2(w, 0);

        auto u0 = Vector2(0,   0);
        auto u1 = Vector2(w_t, 0);
        auto u2 = Vector2(w_t, 1);
        auto u3 = Vector2(0,   1);

        immediate_quad(p0, p1, p2, p3, u0, u1, u2, u3, 0xffffffff); // @Incomplete: Do we want z layer for this?
    }

    immediate_flush();
}

static bool is_done_with_dealing(Entity_Manager *manager)
{
    // For Sawayama, we are done with the dealing when we have all the cards in the
    // triangle starting form.

    // Double check to see if there is anything still moving.
    if (manager->moving_cards.count) return false;
    return true;
}

static bool maybe_draw_some_cards(Entity_Manager *manager, f32 mouse_x, f32 mouse_y, bool pressed)
{
    if (manager->moving_cards.count) return false;
    if (manager->dragging_stack->cards.count) return false; // Just in case...
    if (!pressed) return false;

    auto to_draw = get_card_under_cursor(manager, mouse_x, mouse_y);
    if (!to_draw) return false;

    auto in_stack = get_stack(to_draw);
    if (in_stack->stack_type != Stack_Type::DRAW) return false;
    if (!in_stack->closed) return false; // Only draw cards while the stack is in the closed state.

    constexpr i64 CARDS_DRAW_PER_CLICK = 3;
    auto num_cards_to_draw = std::min(CARDS_DRAW_PER_CLICK, in_stack->cards.count);

    auto destination_stack = pick_stack;
    auto gv = &game_visuals;

    for (auto i = 0; i < num_cards_to_draw; ++i)
    {
        auto popped = pop(&in_stack->cards);

        auto visual_end              = destination_stack->visual_start;
        auto row_position_after_move = destination_stack->cards.count + i;
        visual_end.x += row_position_after_move * gv->card_column_offset * manager->card_size.x;

        move_one_card(Move_Type::DRAW_CARD, popped, visual_end, destination_stack, i);
    }

    in_stack->closed = in_stack->cards.count;
    return true;
}

bool should_do_post_move_reevaluate(Card *card)
{
    auto move_type = card->visual_move_type;

    // Either it is not the DRAW move or it is the last thing that gets drawn.
    return (move_type != Move_Type::DRAW_CARD) || (card->entity_manager->moving_cards.count == 0);
}

void init_sawayama_manager(Entity_Manager *m)
{
    set_game_visuals(m, &game_visuals.default_game_visuals);

    m->init_textures = init_textures;
    m->post_move_reevaluate = post_move_reevaluate;
    m->post_undo_reevaluate = post_undo_reevaluate;
    m->level_from_seed = level_from_seed;

    m->condition_for_post_move_reevaluate = should_do_post_move_reevaluate;

    // m->add_card_to_type_array = add_card_to_type_array;

    m->is_done_with_dealing = is_done_with_dealing;

    m->draw = draw;

    m->can_move = can_move;
    m->can_drag = can_drag;

    m->handle_game_mouse_pointer_events = maybe_draw_some_cards;
    // m->check_for_cards_highlight = check_for_cards_highlight;
}
