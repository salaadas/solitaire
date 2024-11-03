// @Incomplete: Add the game scale for shenzhen, then we can do more refactoring.

// @Incomplete: Use the updated 4k textures.

// Should not highlight the cards when we press the dragon buttons.
// Why is r/g/w not lerping when we hold down the button.
// Different visual duration for different types of animations.

// Currently, the highlight color for the dragon hover button jumps whenever we release another button.
// Hold down the dragon button once we cannot press it further.

#include "shenzhen.h"
#include "solitaire.h"
#include "events.h"
#include "time_info.h"
#include "main.h"
#include "opengl.h"
#include "draw.h"

static constexpr Rank MIN_RANK = 1;
static constexpr Rank MAX_RANK = 9;
static constexpr auto STARTING_CARDS_PER_STACK_COUNT = 5;
static constexpr auto DRAGONS_COUNT_PER_COLOR = 4;

static Dragon_Button *pressed_dragon_button = NULL;

static Dragon_Button dragon_buttons[SHENZHEN_DRAGON_TYPES_COUNT];

Table<Key_Code, Keymap_Highlight_Entry> card_type_keymap_highlight_lookup; // :Rename
Key_Code keymap_highlight_code; // CODE_UNKNOWN means that we should not handle anything.    @Cleanup: I think we should remove this....

static Shenzhen_Game_Visuals game_visuals;

static SArr<Stack*>  foundation_pile;
static SArr<Stack*>  dragon_pile;
static Stack         *flower_stack = NULL;

static RArr<Dragon*> all_dragons;

// Texture_Map *table_small_map;
static Texture_Map *table_large_map;

static Texture_Map *dragon_red_large_map;
static Texture_Map *dragon_green_large_map;
static Texture_Map *dragon_white_large_map;
static Texture_Map *dragon_red_small_map;
static Texture_Map *dragon_green_small_map;
static Texture_Map *dragon_white_small_map;

static Texture_Map *flower_small_map;
static Texture_Map *flower_large_map;

static Texture_Map *card_stack_map; // @Note that only closed stack OR non-base stacks could render this.

static Texture_Map *card_front_map;
static Texture_Map *card_back_map;
static Texture_Map *card_shadow_map;
static Texture_Map *card_textured_map;

static Texture_Map *small_coins_map;
static Texture_Map *small_bamboo_map;
static Texture_Map *small_characters_map;

static RArr<Texture_Map*> red_number_maps;
static RArr<Texture_Map*> green_number_maps;
static RArr<Texture_Map*> white_number_maps;

static Dynamic_Font *number_small_font;

template <class Card_Type>
static Card_Type *Make(Entity_Manager *manager, Color color, Texture_Map *large_icon, Texture_Map *small_icon)
{
    auto card = New<Card_Type>();

    auto base  = register_general_card(manager, _make_Type(Card_Type), card);
    card->card = base;

    base->front_map    = card_front_map;
    base->back_map     = card_back_map;
    base->shadow_map   = card_shadow_map;
    base->textured_map = card_textured_map;

    base->color      = color;
    base->large_icon = large_icon;
    base->small_icon = small_icon;

    return card;
}

static void post_move_reevaluate(Entity_Manager *manager);

static void post_undo_reevaluate(Entity_Manager *manager)
{
    // Change the dragon stacks' states.
    for (auto it : dragon_pile)
    {
        if (it->cards.count != DRAGONS_COUNT_PER_COLOR)
        {
            it->closed = false;
            continue;
        }

        i64 dragons_count = 0;
        for (i64 dragon_index = 0; dragon_index < DRAGONS_COUNT_PER_COLOR; ++dragon_index)
        {
            auto card = it->cards[dragon_index];

            if (cmp_var_type_to_type(card->card_type, Dragon)) dragons_count += 1;
            else break;
        }

        it->closed = dragons_count == DRAGONS_COUNT_PER_COLOR;
    }

    post_move_reevaluate(manager);
}

static bool check_for_victory(Entity_Manager *manager)
{
    if (manager->moving_cards.count) return false;

    for (auto it : manager->base_pile)
    {
        if (it->cards.count) return false;
    }

    for (auto it : dragon_pile)
    {
        if (!(it->closed && it->cards.count == DRAGONS_COUNT_PER_COLOR)) return false;
    }

    if (!flower_stack->cards.count) return false;
    auto flower = flower_stack->cards[0];
    if (!cmp_var_type_to_type(flower->card_type, Flower)) return false;

    for (auto it : foundation_pile)
    {
        if (it->cards.count != (MAX_RANK - MIN_RANK + 1)) return false;
    }

    return true;
}

static void do_victory_animation(Entity_Manager *manager)
{
    manager->win_count += 1;
    manager->in_game_state = In_Game_State::END_GAME;
    auto gv = &game_visuals;

    //
    // @Incomplete: We would want the cards to move down row by row starting with the left.
    //

    f32 duration = gv->end_game_visual_duration;
    f32 delay    = gv->end_game_delay_duration;

    for (i64 row = DRAGONS_COUNT_PER_COLOR - 1; row >= 0; --row) // From the top to the last.
    {
        for (i64 column = 0; column < dragon_pile.count; ++column)
        {
            auto card = dragon_pile[column]->cards[row];
            auto visual_end = card->visual_position;
            visual_end.y = gv->end_y_line;

            move_one_card(Move_Type::DEAL, card, visual_end, NULL, 0, duration, delay);
        }
    }

    manager->total_card_delay_time = 0; // @Hack: To get the cards to move down at the same time.
    for (auto it : flower_stack->cards)
    {
        auto visual_end = it->visual_position;
        visual_end.y = gv->end_y_line;
        move_one_card(Move_Type::DEAL, it, visual_end, NULL, 0, duration, delay);
    }

    manager->total_card_delay_time = 0; // @Hack: To get the cards to move down at the same time.
    for (i64 row = MAX_RANK - MIN_RANK; row >= 0; --row) // From the top to the last.
    {
        for (i64 column = 0; column < foundation_pile.count; ++column)
        {
            auto card = foundation_pile[column]->cards[row];
            auto visual_end = card->visual_position;
            visual_end.y = gv->end_y_line;

            move_one_card(Move_Type::DEAL, card, visual_end, NULL, 0, duration, delay);
        }
    }
}

static void post_move_reevaluate(Entity_Manager *manager)
{
    if (manager->in_game_state != In_Game_State::PLAYING) return;
    if (manager->dragging_stack->cards.count) return; // Don't do automove while we are dragging stuff.
    if (manager->moving_cards.count) return; // There are still moving stuff, so don't reeval.

    //
    // Evaluate all dragons and update the dragon buttons' states.
    //
    i64 visible_dragons_of_color[SHENZHEN_DRAGON_TYPES_COUNT] = {0};

    for (auto stack : manager->base_pile)
    {
        if (!stack->cards.count) continue;

        auto last = array_peek_last(&stack->cards);
        if (cmp_var_type_to_type(last->card_type, Dragon))
        {
            auto color_index = (i64)last->color; // @Cleanup: Yikes.
            visible_dragons_of_color[color_index] += 1;
        }
    }

    for (auto stack : dragon_pile)
    {
        if (!stack->cards.count) continue;

        auto last = array_peek_last(&stack->cards);
        if (cmp_var_type_to_type(last->card_type, Dragon))
        {
            auto color_index = (i64)last->color; // @Cleanup: Yikes.
            visible_dragons_of_color[color_index] += 1;
        }
    }

    for (i64 it_index = 0; it_index < SHENZHEN_DRAGON_TYPES_COUNT; ++it_index)
    {
        auto dragon_button = &dragon_buttons[it_index];
        auto count = visible_dragons_of_color[it_index];

        dragon_button->state = Dragon_Button::UP;
        if (count != DRAGONS_COUNT_PER_COLOR) continue;

        auto color = (Color)it_index; // @Cleanup: Yikes.
        auto [first_matching_stack, first_unoccupied_stack] = find_stack_to_place_card(color, _make_Type(Dragon), dragon_pile);
        if (first_matching_stack || first_unoccupied_stack)
        {
            dragon_button->state = Dragon_Button::ACTIVE;
        }
    }

    //
    // The rules for automoving is as follows:
    // - If there is a visible 1 in the base pile or the dragon pile, we always do an auto move.
    // - We try to automove if there is a card with the same color as the one in the foundation pile and
    //   has 1 rank higher.
    // - We don't automove if the card we are planning to automove has a difference between the ranks > 2 compared
    //   to all other cards in the founddation pile. This is because if the difference is high enough, automoving
    //   will hinder the ability to sort cards.
    //   For an empty foundation stack, we consider that as a card with rank 0.
    //

    auto highest_mark = get_temporary_storage_mark();
    defer {set_temporary_storage_mark(highest_mark);};

    // The considered array consists of cards that are not occluded and also are not Dragons.
    RArr<Card*> considered;
    considered.allocator = {global_context.temporary_storage, __temporary_allocator};
    array_reserve(&considered, dragon_pile.count + manager->base_pile.count);

    for (auto stack : dragon_pile)
    {
        if (stack->closed) continue;
        if (!stack->cards.count) continue;

        auto last = array_peek_last(&stack->cards);
        if (cmp_var_type_to_type(last->card_type, Dragon)) continue;

        array_add(&considered, last);
    }

    for (auto stack : manager->base_pile)
    {
        if (!stack->cards.count) continue;

        auto last = array_peek_last(&stack->cards);
        if (cmp_var_type_to_type(last->card_type, Dragon)) continue;

        array_add(&considered, last);
    }

    for (i64 it_index = 0; it_index < considered.count; ++it_index)
    {
        auto it = considered[it_index];
        assert(it->visual_start_time < 0);

        if (cmp_var_type_to_type(it->card_type, Flower))
        {
            assert(flower_stack->cards.count == 0); // Only one flower per game.
            move_one_card(Move_Type::AUTOMOVE, it, flower_stack);
            break;
        }
        else
        {
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
    }

    set_temporary_storage_mark(highest_mark); // So that we don't pegged the temporary storage.

    auto victory = check_for_victory(manager);
    if (victory)
    {
        do_victory_animation(manager);
    }
}

static Vector2 vector_to_distance_with;
static bool sort_by_distance(Dragon *a, Dragon *b)
{
    auto a_dot = glm::distance(a->card->visual_position, vector_to_distance_with);
    auto b_dot = glm::distance(b->card->visual_position, vector_to_distance_with);

    return a_dot < b_dot;
}

static RArr<Dragon*> get_dragons_with_this_color(Entity_Manager *manager, Color color)
{
    RArr<Dragon*> matching_dragons;
    matching_dragons.allocator = {global_context.temporary_storage, __temporary_allocator};

    for (auto it : all_dragons)
    {
        if (it->card->color != color) continue;
        array_add(&matching_dragons, it);
    }

    return matching_dragons;
}

static void collect_dragons_of_color(Entity_Manager *manager, Color color)
{
    //
    // Find the destination stack within the dragon pile.
    //
    auto [first_matching_stack, first_unoccupied_stack] = find_stack_to_place_card(color, _make_Type(Dragon), dragon_pile);

    // If we cannot find an unoccupied stack or there isn't one with our dragon in it, bail.
    if (!first_matching_stack && !first_unoccupied_stack) return;

    auto matching_dragons = get_dragons_with_this_color(manager, color);

    // The first condition to collect dragons is that all the dragons of that color
    // must be visible (not occluded).
    //
    // The second condition could be one of the two following:
    //   a) If not any of the dragons of that color is in the 'dragon_pile', the 'dragon_pile' must
    //     have at least one unoccupied slot.
    //   b) Otherwise, if there is at least one dragon of that color already inside the 'dragon_pile',
    //     we are good to collect because given the First condition, all the dragons of this color is visible!
    //     
    // We prioritize b) over a) because you would expect the dragons to be collected with the
    // existing one in the 'dragon_pile' if there is already one there.
    Stack *destination_stack = NULL;
    if (first_matching_stack)        destination_stack = first_matching_stack;
    else if (first_unoccupied_stack) destination_stack = first_unoccupied_stack;

    // Collect the dragons from closest to farthest.
    vector_to_distance_with = destination_stack->visual_start;
    array_qsort(&matching_dragons, sort_by_distance);

    assert(destination_stack);
    destination_stack->closed = true;

    for (i64 it_index = 0; it_index < matching_dragons.count; ++it_index)
    {
        auto it = matching_dragons[it_index];
        move_one_card(Move_Type::AUTOMOVE, it->card, destination_stack, it_index);
    }
}

static bool maybe_press_or_release_dragon_button(Entity_Manager *manager, f32 mouse_x, f32 mouse_y, bool pressed)
{
    auto radius = game_visuals.DRAGON_BUTTON_RADIUS;

    if (!pressed)
    {
        if (!pressed_dragon_button) return false;

        //
        // Release the button.
        //
        // @Incomplete: Play release sound.
        pressed_dragon_button->state = Dragon_Button::UP;
        pressed_dragon_button = NULL;

        return true;
    }

    for (i64 it_index = 0; it_index < SHENZHEN_DRAGON_TYPES_COUNT; ++it_index)
    {
        auto button = &dragon_buttons[it_index];

        if (pressed && is_inside_circle(mouse_x, mouse_y, button->center, radius))
        {
            //
            // Press the button.
            //
            // @Incomplete: Play press sound.
            if (button->state == Dragon_Button::UP)
            {
                // Do nothing.
            }
            else if (button->state == Dragon_Button::ACTIVE)
            {
                auto dragon_color = (Color)it_index; // @Cleanup: Yikes. We should consider storing the color inside the button.
                collect_dragons_of_color(manager, dragon_color);
            }

            button->state = Dragon_Button::DOWN;
            pressed_dragon_button = button;

            return true;
        }
    }

    return false; // This is to shut up the compiler.
}

static bool check_descending_of_numbers(Card *upper, Card *lower)
{
    if (!cmp_var_type_to_type(upper->card_type, Number)) return false;
    if (!cmp_var_type_to_type(lower->card_type, Number)) return false;

    auto n1 = Down<Number>(upper);
    auto n2 = Down<Number>(lower);

    if (!((n1->rank - n2->rank == 1) && (n1->card->color != n2->card->color))) return false;

    return true;
}

static Stack *can_move(f32 release_mouse_x, f32 release_mouse_y, Stack *stack_to_move)
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
        case Stack_Type::DRAGON: {
            if (stack_to_move->cards.count > 1) break;
            if (destination_stack->cards.count) break; // Only one card in the dragon cell at a time.

            return destination_stack;
        } break;
        case Stack_Type::FLOWER: {
            if (stack_to_move->cards.count > 1) break;

            auto peek = array_peek_last(&stack_to_move->cards);
            if (cmp_var_type_to_type(peek->card_type, Flower))
            {
                // Only one flower in the game.
                assert(destination_stack->cards.count == 0);

                return destination_stack;
            }
        } break;
        case Stack_Type::FOUNDATION: {
            if (stack_to_move->cards.count > 1) break;

            auto peeked = array_peek_last(&stack_to_move->cards);
            if (!cmp_var_type_to_type(peeked->card_type, Number)) break;

            auto n = Down<Number>(peeked);

            if (!destination_stack->cards.count)
            {
                if (n->rank == MIN_RANK) return destination_stack;
            }
            else
            {
                auto last_card_in_destination = array_peek_last(&destination_stack->cards);
                assert(cmp_var_type_to_type(last_card_in_destination->card_type, Number));

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
        case Stack_Type::FLOWER:
        case Stack_Type::FOUNDATION: {
            return false;
        } break;
        case Stack_Type::DRAGON: {
            return true;
        } break;
        case Stack_Type::BASE: {
            if (cmp_var_type_to_type(card->card_type, Dragon) || cmp_var_type_to_type(card->card_type, Flower))
            {
                // These can only be dragged alone without the cascade.
                return card->row_position == (stack->cards.count - 1);
            }
            else
            {
                for (auto row = card->row_position; row < (stack->cards.count - 1); ++row)
                {
                    auto upper = stack->cards[row];
                    auto lower = stack->cards[row + 1];
                    
                    if (!check_descending_of_numbers(upper, lower)) return false;
                }

                return true;
            }
        } break;
        default: {
            assert(0);
            return false;
        }
    }
}

static void update_or_start_card_highlight(Card *card)
{
    if (card->should_highlight)
    {
        auto dt = timez.current_dt;
        card->highlight_duration += dt;
    }
    else
    {
        // @Incomplete: Do some fade-in.
        card->should_highlight = true;
        card->highlight_duration = 0;
    }
}

static void end_card_highlight(Card *card)
{
    if (!card->should_highlight) return;

    // @Incomplete: Do some fade-out.
    card->should_highlight = false;
}

static bool check_for_dragons_highlighting(Entity_Manager *manager)
{
    for (i64 it_index = 0; it_index < SHENZHEN_DRAGON_TYPES_COUNT; ++it_index)
    {
        auto button = &dragon_buttons[it_index];

        auto radius = game_visuals.DRAGON_BUTTON_RADIUS;
        auto is_inside = is_inside_circle(mouse_pointer_x, mouse_pointer_y, button->center, radius);

        // Highlight all dragons with that color when we hover over the dragon button. Or if the dragons
        // were highlighted before, we end it.
        auto color = (Color)it_index; // @Cleanup: Yikes.
        auto matching_dragons = get_dragons_with_this_color(manager, color);

        if (!is_inside)
        {
            for (auto it : matching_dragons) end_card_highlight(it->card);
            continue;
        }

        if (!matching_dragons.count) continue; // Should log or do something here because this should not happen.

        // If the dragons is in the closed stack, don't bother highlighting them.

        auto should_skip = false;
        for (auto it : matching_dragons)
        {
            if (get_stack(it->card)->closed)
            {
                should_skip = true;
                break;
            }
        }

        if (should_skip) continue;
        for (auto it : matching_dragons) update_or_start_card_highlight(it->card);

        // We can't hover over two buttons at the same time unless our layout is messed up very bad.
        // If we reach here, then we know we will highlight some dragons, so we bail.
        return true;
    }

    return false;
}

static void check_for_cards_highlight(Entity_Manager *manager)
{
    //
    // If we are hovering over a dragon button and pressing a key to highlight at the same time, we
    // only highlight the dragons.
    //

    if (!manager->dragging_stack->cards.count)
    {
        auto handled = check_for_dragons_highlighting(manager);
        if (handled) return;
    }

    //
    // Check for highlight using keymap characters.
    //
    if (keymap_highlight_code != CODE_UNKNOWN) // @Cleanup: Think of a better way.
    {
        auto [entry, found] = table_find(&card_type_keymap_highlight_lookup, keymap_highlight_code);

        if (found)
        {
            // @Speed: We could cache the array to the type with the entry but ehhhhh.
            for (auto it : manager->all_cards)
            {
                bool maybe_end_highlight = false;
                if (it->card_type != entry.card_type) maybe_end_highlight = true;

                if (!maybe_end_highlight)
                {
                    if (cmp_var_type_to_type(entry.card_type, Number))
                    {
                        auto n = Down<Number>(it);
                        if (n->rank != entry.number_rank) maybe_end_highlight = true;
                    }
                    else if (cmp_var_type_to_type(entry.card_type, Dragon))
                    {
                        if (it->color != entry.dragon_color) maybe_end_highlight = true;
                    }
                }

                if (maybe_end_highlight)
                {
                    end_card_highlight(it);
                    continue;
                }

                update_or_start_card_highlight(it);
            }
        }
    }
}

static void init_stack_piles(Entity_Manager *manager)
{
    auto gv = &game_visuals;

    // Dragging is a special stack that does not have a release region.
    manager->dragging_stack = create_stack(manager, Stack_Type::DRAGGING, gv->dragging_z_layer_offset, {});

    auto w = card_front_map->width;       // :PixelCoord
    auto h = -1 * card_front_map->height; // :PixelCoord

    auto visual_y = gv->FINALIZE_ROW_Y;

    // Dragons.
    array_resize(&dragon_pile, SHENZHEN_DRAGON_TYPES_COUNT);
    for (i64 it_index = 0; it_index < dragon_pile.count; ++it_index)
    {
        auto release_region = get_rect(gv->dragons_x[it_index], visual_y, w, h);
        dragon_pile[it_index] = create_stack(manager, Stack_Type::DRAGON, gv->dragon_z_layer_offset, release_region);
    }

    // Foundations.
    array_resize(&foundation_pile, SHENZHEN_FOUNDATION_COLUMNS_COUNT);
    for (i64 it_index = 0; it_index < foundation_pile.count; ++it_index)
    {
        auto release_region = get_rect(gv->foundation_x[it_index], visual_y, w, h);
        foundation_pile[it_index] = create_stack(manager, Stack_Type::FOUNDATION, gv->foundation_z_layer_offset, release_region);
    }

    // Flower.
    auto release_region = get_rect(gv->flower_x, visual_y, w, h);
    flower_stack = create_stack(manager, Stack_Type::FLOWER, gv->flower_z_layer_offset, release_region);
}

static Number *Make_Number(Entity_Manager *manager, Color color, Rank rank)
{
    assert((MIN_RANK <= rank) && (rank <= MAX_RANK));

    Texture_Map *large_map;
    Texture_Map *small_map;

    auto large_map_index = rank - 1;
    if (color == Color::RED)
    {
        large_map = red_number_maps[large_map_index];
        small_map = small_coins_map;
    }
    else if (color == Color::GREEN)
    {
        large_map = green_number_maps[large_map_index];
        small_map = small_bamboo_map;
    }
    else if (color == Color::WHITE)
    {
        large_map = white_number_maps[large_map_index];
        small_map = small_characters_map;
    }
    else
    {
        logprint("Make_Number", "Color value %d is invalid for number card with rank %d!\n", (i32)color, rank);
        assert(0);
    }

    auto number_card = Make<Number>(manager, color, large_map, small_map);
    number_card->rank = rank;

    return number_card;
}

static void level_from_seed(Entity_Manager *manager, i64 seed)
{
    //
    // Resetting all the arrays and stacks
    //
    array_reset(&manager->all_cards);
    array_reset(&all_dragons);

    array_reset(&manager->all_stacks);
    array_reset(&manager->moving_cards);

    init_stack_piles(manager);

    //
    // Setting game states.
    //
    manager->is_dragging = false;
    manager->in_game_state = In_Game_State::DEALING;
    manager->next_transaction_to_issue = 1; // Reset the transaction counter.

    //
    // Game seed:
    //

    manager->current_level_seed = seed;
    srand(manager->current_level_seed);

    //
    // Add the cards and shuffle it up.
    //
    array_reserve(&manager->all_cards, SHENZHEN_BASE_COLUMNS_COUNT * STARTING_CARDS_PER_STACK_COUNT);
    array_reset(&manager->all_cards); // @Cleaup: Redundant with cleanup_level?

    // The positions of the cards will be back-filled later on!
    for (auto i = MIN_RANK; i <= MAX_RANK; ++i)
    {
        auto red   = Make_Number(manager, Color::RED,   i);
        auto green = Make_Number(manager, Color::GREEN, i);
        auto white = Make_Number(manager, Color::WHITE, i);
    }

    for (auto i = 1; i <= DRAGONS_COUNT_PER_COLOR; ++i)
    {
        auto red   = Make<Dragon>(manager, Color::RED,   dragon_red_large_map,   dragon_red_small_map);
        auto green = Make<Dragon>(manager, Color::GREEN, dragon_green_large_map, dragon_green_small_map);
        auto white = Make<Dragon>(manager, Color::WHITE, dragon_white_large_map, dragon_white_small_map);
    }

    Make<Flower>(manager, Color::FLOWER, flower_large_map, flower_small_map);

    if constexpr (!DEBUG_NO_SHUFFLE)
    {
        shuffle(&manager->all_cards);
    }

    auto gv = &game_visuals;
    auto w = card_front_map->width; // :PixelCoord @Cleanup
    auto h = -gv->BASE_ROW_Y;                // :PixelCoord   The release region for the base pile extends far down.

    array_resize(&manager->base_pile, SHENZHEN_BASE_COLUMNS_COUNT);
    for (auto it_index = 0; it_index < SHENZHEN_BASE_COLUMNS_COUNT; ++it_index)
    {
        auto release_region = get_rect(gv->numbers_x[it_index], gv->BASE_ROW_Y, w, h);
        auto stack = create_stack(manager, Stack_Type::BASE, gv->base_z_layer_offset, release_region);
        manager->base_pile[it_index] = stack;

        array_reserve(&stack->cards, STARTING_CARDS_PER_STACK_COUNT);
    }

    //
    // Place all the cards at the flower cell and then starts to deal out the cards from
    // left to right, top to bottom.
    //
    auto all_cards_count = manager->all_cards.count;
    auto it_index = all_cards_count - 1;
    for (auto row = 0; row < STARTING_CARDS_PER_STACK_COUNT; ++row)
    {
        for (auto column = SHENZHEN_BASE_COLUMNS_COUNT - 1; column >= 0; --column)
        {
            auto card = manager->all_cards[it_index];

            set_stack(card, flower_stack);

            card->row_position = all_cards_count - it_index - 1;
            assert(card->row_position >= 0);
            card->z_layer      = get_stack(card)->z_offset + card->row_position;

            card->visual_position = flower_stack->visual_start;
            array_add(&flower_stack->cards, card);

            Stack *destination_stack = manager->base_pile[column];
            card->visual_end_at_stack = destination_stack;

            it_index -= 1;
        }
    }

    set_dealing_stack(manager, flower_stack);

    it_index = flower_stack->cards.count - 1;
    for (auto row = 0; row < STARTING_CARDS_PER_STACK_COUNT; ++row)
    {
        for (auto column = SHENZHEN_BASE_COLUMNS_COUNT - 1; column >= 0; --column)
        {
            auto card = flower_stack->cards[it_index];

            deal_one_card_with_offset(card, card->visual_end_at_stack, row);
            it_index -= 1;
        }
    }
}

static void init_keymap_highlight() // @Incomplete: This is not being invoked.
{
    // @Cleanup? 
    // We could think of making a seperate load file for these keymaps later
    // but don't think it will be generalizable for other solitaire variants, but idk.

    // Init keymap for the numbers.
    // Example: If you press 1, you highlight all the 1's, regardless of their color.
    for (Rank r = MIN_RANK; r <= MAX_RANK; ++r)
    {
        Keymap_Highlight_Entry entry;
        entry.card_type = _make_Type(Number);
        entry.number_rank = r;

        auto key = (Key_Code)(CODE_0 + r);
        table_add(&card_type_keymap_highlight_lookup, key, entry);
    }

    // Keymap for dragons.
    // Press 'r' for red dragon, 'g' for green, and 'w' for white.
    {
        Keymap_Highlight_Entry entry;
        entry.card_type = _make_Type(Dragon);

        entry.dragon_color = Color::RED;
        table_add(&card_type_keymap_highlight_lookup, CODE_R, entry);

        entry.dragon_color = Color::GREEN;
        table_add(&card_type_keymap_highlight_lookup, CODE_G, entry);

        entry.dragon_color = Color::WHITE;
        table_add(&card_type_keymap_highlight_lookup, CODE_W, entry);
    }

    // Flower.
    {
        Keymap_Highlight_Entry entry;
        entry.card_type = _make_Type(Flower);

        table_add(&card_type_keymap_highlight_lookup, CODE_F, entry);
    }
}

static void init_textures(Entity_Manager *manager)
{
//    table_small_map = cat_find(texture_catalog, "table_small");
    table_large_map = load_linear_mipmapped_texture(String("table_large"));

    // @Incomplete: We want better screen adjustments later
    {
        auto board_size = Vector2(table_large_map->width, table_large_map->height); // @Fixme @Incomplete: Add SHENZHEN_GAME_SCALE.
        set_game_window_properties(manager, board_size);
    }

    card_front_map    = load_linear_mipmapped_texture(String("card_front"));
    card_back_map     = load_linear_mipmapped_texture(String("card_back"));
    card_shadow_map   = load_linear_mipmapped_texture(String("card_shadow"));
    card_textured_map = load_linear_mipmapped_texture(String("card_texture"));

    set_general_card_size(manager, card_front_map->width, card_front_map->height); // @Fixme: Add SCALE.

    dragon_red_large_map   = load_linear_mipmapped_texture(String("dragon_red"));
    dragon_green_large_map = load_linear_mipmapped_texture(String("dragon_green"));
    dragon_white_large_map = load_linear_mipmapped_texture(String("dragon_white"));

    dragon_red_small_map   = load_linear_mipmapped_texture(String("small_dragon_red"));
    dragon_green_small_map = load_linear_mipmapped_texture(String("small_dragon_green"));
    dragon_white_small_map = load_linear_mipmapped_texture(String("small_dragon_white"));

    flower_small_map = load_linear_mipmapped_texture(String("small_flower"));
    flower_large_map = load_linear_mipmapped_texture(String("flower"));

    card_stack_map = load_linear_mipmapped_texture(String("stack_side"));

    auto reserve_count = MAX_RANK - MIN_RANK + 1;
    array_reserve(&red_number_maps,   reserve_count);
    array_reserve(&green_number_maps, reserve_count);
    array_reserve(&white_number_maps, reserve_count);

    auto add_number_map = [](Rank rank) {
        assert((MIN_RANK <= rank) && (rank <= MAX_RANK));

        auto red   = tprint(String("coins_%d"),  rank);
        auto green = tprint(String("bamboo_%d"), rank);
        auto white = tprint(String("char_%d"),   rank);

        auto red_map   = load_linear_mipmapped_texture(red);
        auto green_map = load_linear_mipmapped_texture(green);
        auto white_map = load_linear_mipmapped_texture(white);

        assert(red_map);
        assert(green_map);
        assert(white_map);

        array_add(&red_number_maps,   red_map);
        array_add(&green_number_maps, green_map);
        array_add(&white_number_maps, white_map);
    };

    add_number_map(1);
    add_number_map(2);
    add_number_map(3);
    add_number_map(4);
    add_number_map(5);
    add_number_map(6);
    add_number_map(7);
    add_number_map(8);
    add_number_map(9);

    small_coins_map      = load_linear_mipmapped_texture(String("small_coins"));
    small_characters_map = load_linear_mipmapped_texture(String("small_characters"));
    small_bamboo_map     = load_linear_mipmapped_texture(String("small_bamboo"));

    {
        auto gv = &game_visuals;
        auto cx = gv->DRAGON_BUTTON_X;

        Dragon_Button *button;

        // @Cleanup: Maybe make a procedure for this.
        button = &dragon_buttons[(i64)Color::RED];
        button->maps[Dragon_Button::UP]     = load_linear_mipmapped_texture(String("button_red_up"));
        button->maps[Dragon_Button::DOWN]   = load_linear_mipmapped_texture(String("button_red_down"));
        button->maps[Dragon_Button::ACTIVE] = load_linear_mipmapped_texture(String("button_red_active"));
        button->center = Vector2(cx, gv->dragon_buttons_y[(i64)Color::RED]);

        button = &dragon_buttons[(i64)Color::GREEN];
        button->maps[Dragon_Button::UP]     = load_linear_mipmapped_texture(String("button_green_up"));
        button->maps[Dragon_Button::DOWN]   = load_linear_mipmapped_texture(String("button_green_down"));
        button->maps[Dragon_Button::ACTIVE] = load_linear_mipmapped_texture(String("button_green_active"));
        button->center = Vector2(cx, gv->dragon_buttons_y[(i64)Color::GREEN]);

        button = &dragon_buttons[(i64)Color::WHITE];
        button->maps[Dragon_Button::UP]     = load_linear_mipmapped_texture(String("button_white_up"));
        button->maps[Dragon_Button::DOWN]   = load_linear_mipmapped_texture(String("button_white_down"));
        button->maps[Dragon_Button::ACTIVE] = load_linear_mipmapped_texture(String("button_white_active"));
        button->center = Vector2(cx, gv->dragon_buttons_y[(i64)Color::WHITE]);
    }
}

static bool handle_game_mouse_pointer_events(Entity_Manager *manager, f32 mouse_x, f32 mouse_y, bool pressed)
{
    auto handled = maybe_press_or_release_dragon_button(manager, mouse_x, mouse_y, pressed);
    return handled;
}

static void add_card_to_type_array(Card *card)
{
    if (cmp_var_type_to_type(card->card_type, Dragon)) array_add(&all_dragons, Down<Dragon>(card));
}

static Vector4 get_card_highlight_color(Card *card)
{
    // @Cleanup: Maybe we should use the base/highlight color from the Card struct itself.
    auto base_color      = game_visuals.base_card_color;
    auto highlight_color = game_visuals.default_card_highlight_color;

    auto blend_factor = sinf(TAU * card->highlight_duration * .5f);
    blend_factor += 1;
    blend_factor *= .5f;
    Clamp(&blend_factor, 0.f, 1.f);

    return lerp(base_color, highlight_color, blend_factor);
}

static bool should_render_card_stack_texture(Stack *stack)
{
    // Only render the stack if the count is > 1, because when the count is 1, we don't see it anyways.
    return (stack->cards.count > 1) && (stack->closed || (stack->stack_type != Stack_Type::BASE && stack->stack_type != Stack_Type::DRAGGING));
}

static void render_card_from_top_left(Card *card)
{
    Texture_Map *card_map = card->front_map;
    auto stack = get_stack(card);
    if (stack->closed) card_map = card->back_map;
    else               card_map = card->front_map;

    rendering_2d_right_handed();
    set_shader(shader_argb_and_texture);

    f32 w = card_map->width;
    f32 h = card_map->height;
    auto pos = card->visual_position;

    auto p3 = Vector2(pos.x, pos.y);
    auto p2 = p3 + Vector2(w, 0);
    auto p1 = p2 - Vector2(0, h);
    auto p0 = p3 - Vector2(0, h);

    auto z_layer = card->z_layer;

    auto white = Vector4(1, 1, 1, 1);

    // Render the card stack, only if we are the first card in the stack.
    auto should_render_stack = should_render_card_stack_texture(stack);
    if (should_render_stack)
    {
        if (card->row_position == 0)
        {
            // Shadow for the stack's base.
            set_texture(String("diffuse_texture"), card->shadow_map);
            immediate_quad(p0, p1, p2, p3, 0xffffffff, z_layer);

            auto sp0 = p0;
            auto sp1 = p1;
            auto sp2 = sp1 + Vector2(0, card_stack_map->height);
            auto sp3 = sp0 + Vector2(0, card_stack_map->height);

            set_texture(String("diffuse_texture"), card_stack_map);
            immediate_quad(sp0, sp1, sp2, sp3, 0xffffffff, z_layer);
        }

        // :PixelCoord, we are offsetting each card by their row position * fixed_pixel_amount,
        // which is not so great in my opinion.
        auto y_offset = (stack->cards.count - 1) * game_visuals.card_fixed_pixel_offset_amount;
        p0.y += y_offset;
        p1.y += y_offset;
        p2.y += y_offset;
        p3.y += y_offset;
    }
    else
    {
        // Shadow for everyone, I don't know if we should optimize for when card is in stack or not....
        set_texture(String("diffuse_texture"), card->shadow_map);
        immediate_quad(p0, p1, p2, p3, 0xffffffff, z_layer);
    }

    // Render the card front.
    {
        assert(card_map);
        set_texture(String("diffuse_texture"), card_map);

        Vector4 color = white;
        if (card->should_highlight) color = get_card_highlight_color(card);

        immediate_quad(p0, p1, p2, p3, white, white, color, color, z_layer);
    }

    if (stack->closed)
    {
        // Only render the card texture if our stack is closed.
        set_texture(String("diffuse_texture"), card->textured_map);
        immediate_quad(p0, p1, p2, p3, 0xffffffff, z_layer);

        return;
    }

    auto top_left     = p3;
    auto bottom_right = p1;

    //
    // The icons.
    //
    auto large_icon = card->large_icon;
    if (large_icon)
    {
        set_texture(String("diffuse_texture"), large_icon);
        p3 = top_left + Vector2((w - large_icon->width) / 2.f, - (h - large_icon->height) / 2.f);
        p2 = p3 + Vector2(large_icon->width, 0);
        p1 = p2 - Vector2(0, large_icon->height);
        p0 = p3 - Vector2(0, large_icon->height);
    
        immediate_quad(p0, p1, p2, p3, 0xffffffff, z_layer);
    }

    auto mx      = w * .055f; // @Hardcoded:
    auto my      = w * .04f;
    auto text_my = my;

    if (cmp_var_type_to_type(card->card_type, Number))
    {
        mx = w * .08f;
        my = w * .24f;
    }

    Vector2 top_icon_pos;
    Vector2 bottom_icon_pos;

    auto small_icon = card->small_icon;
    if (small_icon)
    {
        set_texture(String("diffuse_texture"), small_icon);

        auto ox = small_icon->width/2.f + mx;
        auto oy = small_icon->height/2.f + my;
        top_icon_pos = top_left + Vector2(ox, -oy);
        immediate_image(top_icon_pos, Vector2(small_icon->width, small_icon->height), white, 0, false, z_layer);

        auto y_offset = oy * 1.1f; // Offset it up a bit more because there is the shaded area of the card...

        bottom_icon_pos = bottom_right + Vector2(-ox, y_offset);
        immediate_image(bottom_icon_pos, Vector2(small_icon->width, small_icon->height), white, 180, false, z_layer);
    }

    if (cmp_var_type_to_type(card->card_type, Number))
    {
        auto number_card = Down<Number>(card);
        auto rank = number_card->rank;

        auto gv = &game_visuals;

        Vector4 number_color;
        switch (card->color)
        {
            case Color::RED:   number_color = gv->red_number_color;   break;
            case Color::GREEN: number_color = gv->green_number_color; break;
            case Color::WHITE: number_color = gv->white_number_color; break;
            default: assert(0);
        }

        //
        // Draw the numbers!
        //
        auto font = number_small_font;
        auto width = prepare_text(font, tprint(String("%d"), rank));

        auto y_pad = small_icon->height*.5f + font->character_height * .2f;

        auto x = top_icon_pos.x - width*.5f;
        auto y = top_icon_pos.y + y_pad;
        draw_prepared_text(font, x, y, number_color, 0, z_layer);

        // The flipped number in the bottom right.
        x = bottom_icon_pos.x - width*.5f;
        y = bottom_icon_pos.y - font->character_height*.64f - y_pad;
        draw_prepared_text(font, x, y, number_color, 180, z_layer);
    }

    if (DEBUG_HITBOX)
    {
        set_shader(shader_argb_no_texture);

        Vector2 p0, p1, p2, p3;
        get_quad(card->hitbox, &p0, &p1, &p2, &p3);

        auto color = argb_color(Vector4(0, 1, 1, .4f));
        immediate_quad(p0, p1, p2, p3, color, z_layer);
    }

    immediate_flush();
}

static void draw(Entity_Manager *manager)
{
    if (!number_small_font || was_window_resized_this_frame) // @Yikes: Change the font depending on the size of the screen too.
    {
        number_small_font = get_font_at_size(FONT_FOLDER, String("Glacial-Bold.ttf"), 26.0f);
    }

    {
        rendering_2d_right_handed();
        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), table_large_map);

        // auto w = render_target_width;
        // auto h = render_target_height;
        auto w = table_large_map->width;
        auto h = table_large_map->height;

        auto p0 = Vector2(0, 0);
        auto p1 = Vector2(w, 0);
        auto p2 = Vector2(w, h);
        auto p3 = Vector2(0, h);

        immediate_quad(p0, p1, p2, p3, 0xffffffff);
        immediate_flush();
    }

    if (DEBUG_RELEASE_REGION)
    {
        set_shader(shader_argb_no_texture);

        auto fcolor = Vector4(1, .3, 0, .3);
        auto icolor = argb_color(fcolor);

        for (auto stack : manager->all_stacks)
        {
            if (stack->stack_type == Stack_Type::DRAGGING) continue;

            Vector2 p0, p1, p2, p3;
            get_quad(stack->release_region, &p0, &p1, &p2, &p3);

            immediate_quad(p0, p1, p2, p3, icolor);
        }
    }
    // Draw dragon buttons.
    {
        set_shader(shader_argb_and_texture);

        auto gv = &game_visuals;
        auto size = Vector2(gv->DRAGON_BUTTON_RADIUS * 2.f, gv->DRAGON_BUTTON_RADIUS * 2.f);

        for (i64 it_index = 0; it_index < SHENZHEN_DRAGON_TYPES_COUNT; ++it_index)
        {
            auto button = dragon_buttons[it_index];

            auto map = button.maps[button.state];
            set_texture(String("diffuse_texture"), map);

            immediate_image(button.center, size, Vector4(1, 1, 1, 1), 0, false);
        }
    }

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

    immediate_flush();
}

static bool is_done_with_dealing(Entity_Manager *manager)
{
    // Once the flower stack is empty, we know we have dealt out everything.
    if (flower_stack->cards.count) return false;

    // Double check to see if there is anything still moving.
    if (manager->moving_cards.count) return false;

    return true;
}

void init_shenzhen_manager(Entity_Manager *m)
{
    m->init_textures = init_textures;
    m->post_move_reevaluate = post_move_reevaluate;
    m->post_undo_reevaluate = post_undo_reevaluate;
    m->level_from_seed = level_from_seed;

    m->add_card_to_type_array = add_card_to_type_array;

    m->draw = draw;

    m->can_move = can_move;
    m->can_drag = can_drag;
    m->handle_game_mouse_pointer_events = handle_game_mouse_pointer_events;

    m->is_done_with_dealing = is_done_with_dealing;

    m->check_for_cards_highlight = check_for_cards_highlight;
}
