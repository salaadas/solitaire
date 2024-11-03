//
//             URGENT!
//

// Do the @Incomplete features of each game.

// @Fixme: Shenzhen has a problem of cards repeating texture if you move it up.

// Add game_window_scale.

// Maybe cleanup entity manager stuff for the menu chooser.

// Game cleanup.
// Game cleanup.
// Game cleanup.
// Game cleanup.

// @Speed: Maybe do bucket array for the creation of the cards instead of New'ing stuff.

// Port the UI library from Sokoban to this game.

// I think we want a different offscreen buffer for each game, or we could have a
// texture map renderred for each game if the game is not active!

 // @Incomplete: We want a menu for choosing games later.
// I'm thinking of a OS-like thing where you are allowed to choose which solitaire app is running.
// With this, we need to port over the UI library from Sokoban.

// Different mouse cursor.

// Proletariat.
// Birds of a feather.

// Instruction panels and Stats panel.
// Add game frame. with win count readout and restart button and undo button.

// Audio/Music.
// Add save game states.

//
// BUGS!!!!!!!!!!!!!!!!!!!!!:
// BUGS!!!!!!!!!!!!!!!!!!!!!:
//

// Undo was crashing on us once. Maybe that has something with undo'ing an empty stack.
// @Incomplete: Undo system right now ignores your undo if there are moving cards.

// Why did the card dealing not working that one time?

// Why is temporary storage randomly crashing on us.

// @Speed @Bug: Figure out why we can't do memcpy/memmove for ordered_remove.

// @Cleanup: :DelayHack Figure out something better.
// :DoubleClick Figure out how to best do the double click stuff.

//
// STUFF TO THINK ABOUT:
//

// Maybe rect should start from the top left????
// @Cleanup: Clean up the making of the hitbox of the stacks.
// :Hitbox Don't use the size of the image itself...

#include "solitaire.h"
#include "events.h"
#include "time_info.h"
#include "hud.h"
#include "undo.h"
#include "metadata.h"

#include "shenzhen.h"
#include "sawayama.h"
#include "kabufuda.h"
#include "cluj.h"

#include <time.h> // For time().

f32 mouse_pointer_x, mouse_pointer_y;

constexpr auto STARTING_VARIANT = Solitaire_Variant::SAWAYAMA;

Solitaire_Variant solitaire_variant;

Entity_Manager *shenzhen_entity_manager;
Entity_Manager *sawayama_entity_manager;
Entity_Manager *kabufuda_entity_manager;
Entity_Manager *cluj_entity_manager;

Default_Game_Visuals default_game_visuals;

Entity_Manager *get_current_entity_manager()
{
    switch (solitaire_variant)
    {
        case Solitaire_Variant::UNKNOWN:  return NULL;
        case Solitaire_Variant::SHENZHEN: return shenzhen_entity_manager;
        case Solitaire_Variant::SAWAYAMA: return sawayama_entity_manager;
        case Solitaire_Variant::KABUFUDA: return kabufuda_entity_manager;
        case Solitaire_Variant::CLUJ:     return cluj_entity_manager;
        default: assert(0);
    }

    return NULL;
}

void set_game_visuals(Entity_Manager *manager, Default_Game_Visuals *visuals)
{
    assert(!manager->game_visuals);
    manager->game_visuals = visuals;
}

Entity_Manager *make_entity_manager()
{
    auto manager = New<Entity_Manager>();
    manager->undo_handler = New<Undo_Handler>();

    return manager;
}

Default_Game_Visuals *get_game_visuals(Entity_Manager *manager)
{
    if (!manager->game_visuals) return &default_game_visuals;
    return manager->game_visuals;
}

void set_general_card_size(Entity_Manager *manager, f32 width, f32 height)
{
    manager->card_size_is_set = true;
    manager->card_size = Vector2(width, height);
}

Card *register_general_card(Entity_Manager *manager, _type_Type card_type, void *derived_pointer)
{
    assert(manager);
    assert(card_type != _make_Type(Unknown_Card));
    assert(manager->card_size_is_set);

    auto card = New<Card>(); // @Speed: Do bucket array.

    card->entity_manager  = manager;
    card->card_id         = manager->next_pid_to_issue;
    card->card_type       = card_type;
    card->derived_pointer = derived_pointer;

    manager->next_pid_to_issue += 1;

    array_add(&manager->all_cards, card);

    if (manager->add_card_to_type_array)
    {
        manager->add_card_to_type_array(card);
    }

    return card;
}

Stack *get_stack_by_index(Entity_Manager *manager, i64 index)
{
    assert(manager);

    auto stack = manager->all_stacks[index];
    assert(stack->stack_index == index);

    return stack;
}

Stack *get_stack(Card *card)
{
    auto stack = get_stack_by_index(card->entity_manager, card->in_stack_index);
    return stack;
}

Stack *get_previous_stack(Card *card)
{
    auto stack = get_stack_by_index(card->entity_manager, card->previous_stack_index);
    return stack;
}

void set_stack(Card *card, Stack *stack)
{
    assert(card && stack);
    card->in_stack_index = stack->stack_index;
}

void set_previous_stack(Card *card, Stack *stack)
{
    assert(card && stack);
    card->previous_stack_index     = stack->stack_index;
    card->previous_visual_position = card->visual_position;
}

void shuffle(RArr<Card*> *cards)
{
    for (auto i = cards->count - 1; i > 1; --i)
    {
        auto j = rand() % (i + 1);
        Swap(&(*cards)[i], &(*cards)[j]);
    }
}

void get_quad(Rect r, Vector2 *p0, Vector2 *p1, Vector2 *p2, Vector2 *p3)
{
    p0->x = r.x;
    p1->x = r.x + r.w;
    p2->x = r.x + r.w;
    p3->x = r.x;

    p0->y = r.y;
    p1->y = r.y;
    p2->y = r.y + r.h;
    p3->y = r.y + r.h;
}

Rect get_rect(f32 x, f32 y, f32 w, f32 h)
{
    Rect r;
    r.x = x;
    r.y = y;
    if (w < 0) r.x += w;
    if (h < 0) r.y += h;

    r.w = fabsf(w);
    r.h = fabsf(h);

    return r;
}

Vector2 top_left(Rect rect)
{
    return Vector2(rect.x, rect.y + rect.h);
}

Stack *create_stack(Entity_Manager *manager, Stack_Type stack_type, i64 z_layer_offset, Rect release_region)
{
    auto stack = New<Stack>(); // @Speed: Use bucket arrays.
    stack->stack_type     = stack_type;
    stack->z_offset       = z_layer_offset;
    stack->release_region = release_region;

    // @Incomplete: We should not use the release region as the visual start, instead, we should
    // think about making this a parameter to create_stack.
    stack->visual_start = top_left(release_region);
    stack->stack_index  = manager->all_stacks.count;

    array_add(&manager->all_stacks, stack);

    return stack;
}

bool should_remove_card(Move_Type move_type)
{
    switch (move_type)
    {
        // These should be removed manually becaue we may pick/place a cascade of cards or
        // revert a failed move while holding a cascade of cards.
        case Move_Type::PICK:
        case Move_Type::PLACE:
        case Move_Type::UNIFY:
        case Move_Type::FAILED_MOVE: return false;

        case Move_Type::DEAL:
        case Move_Type::AUTOMOVE: return true;

        case Move_Type::DRAW_CARD: return false; // This is because we popped the cards ourselves.

        default: {
            assert(0);
            return false; // Should not get here, because the switch-case above should be complete!
        }
    }
}

bool should_delay_card(Card *card)
{
    auto move_type = card->visual_move_type;
    assert(move_type != Move_Type::PICK); // Picking up cards should not get here!

    switch (move_type)
    {
        case Move_Type::FAILED_MOVE:
        case Move_Type::UNIFY:
        case Move_Type::PLACE: {
            return card->row_position == (get_stack(card)->cards.count - 1);
        } break;

        case Move_Type::DEAL:
        case Move_Type::DRAW_CARD:
        case Move_Type::AUTOMOVE: return true;

        default: {
            assert(0);
            return false; // Should not get here, because the switch-case above should be complete!
        }
    }
}

void physically_move_one_card(Card *card, Stack *destination_stack)
{
    assert(destination_stack);

    auto old_stack = get_stack(card);
    if (should_remove_card(card->visual_move_type)) // @Cleanup: Make visual_move_type a parameter.
    {
        auto me = pop(&old_stack->cards);
        assert(me == card); // Should be moving from the last most card first.
    }

    set_stack(card, destination_stack);

    card->row_position = destination_stack->cards.count;
    card->z_layer      = destination_stack->z_offset + card->row_position;

    array_add(&destination_stack->cards, card);
}

Transaction_Id get_next_transaction_id(Entity_Manager *manager)
{
    auto result = manager->next_transaction_to_issue;
    manager->next_transaction_to_issue += 1;
    return result;
}

void reset_visual_interpolation(Card *card)
{
    card->visual_start_time = -1;
    card->visual_elapsed    = 0;
    card->visual_duration   = 0;
}

void add_visual_interpolation(Move_Type move_type, Card *card, Vector2 visual_end, Stack *destination_stack, i64 row_position_after_move, f32 duration, f32 delay_duration)
{
    auto manager = card->entity_manager;

    card->visual_start_time = timez.current_time + manager->total_card_delay_time;
    card->visual_elapsed = 0;

    card->visual_move_type = move_type;

    auto gv = get_game_visuals(manager);
    if (duration < 0) card->visual_duration = gv->default_card_visual_duration;
    else              card->visual_duration = duration;

    card->visual_start = card->visual_position;
    card->visual_end  = visual_end;

    card->did_physical_move = false;

    // @Fixme: These are the artifacts of updating logical stuff inside visual_interpolation!!!!!!
    // @Fixme: These are the artifacts of updating logical stuff inside visual_interpolation!!!!!!
    // @Fixme: These are the artifacts of updating logical stuff inside visual_interpolation!!!!!!
    card->visual_end_at_stack  = destination_stack;
    card->visual_start_z_layer = row_position_after_move + gv->default_animating_z_layer_offset;

    // When dealing cards, we want the first deal to has the highest z just so that it looks nicer.
    // Which means the lower the card is pulled from the stack, the higher the z of it is.
    if (move_type == Move_Type::DEAL)
    {
        auto from_stack_count = get_stack(card)->cards.count;
        card->visual_start_z_layer += from_stack_count - 1 - card->row_position;
    }
    else
    {
        // @Incomplete @Hack: Figure out a better scalar multiple for card delay time.
        
        // This is assuming the largest amount of cards to be moved at one time is MAX_RANK - MIN_RANK + 1.
//        auto card_delay_scalar = MAX_RANK - MIN_RANK + 1; @Incomplete: We want this.
        auto card_delay_scalar = 15; // @Hardcoded:
        
        // @Fixme: This is scary because it may exceed the Z_* boundaries.
        card->visual_start_z_layer += manager->total_card_delay_time * (card_delay_scalar);
        if (card->visual_start_z_layer > -Z_NEAR) card->visual_start_z_layer = -Z_NEAR;
    }

    if (destination_stack) card->visual_end_z_layer = row_position_after_move + destination_stack->z_offset;
    else                   card->visual_end_z_layer = card->visual_start_z_layer;

    array_add_if_unique(&manager->moving_cards, card); // @Speed: We would like to author all the places that calls this in order to make sure that we don't duplicate but ehh....

    // :DelayHack
    if (should_delay_card(card))
    {
        f32 delay;
        if (delay_duration < 0) delay = gv->default_card_delay_duration;
        else                    delay = delay_duration;

        manager->total_card_delay_time += delay;
    }
    
    manager->total_visual_interpolations_added_this_frame += 1;
}

// If called in a loop, supply the iteration_index.
void move_one_card(Move_Type move_type, Card *card, Vector2 visual_end, Stack *destination_stack, i64 iteration_index, f32 duration, f32 delay_duration)
{
    auto manager = card->entity_manager;
    card->in_transaction_id = get_next_transaction_id(manager);

    i64 row_position_after_move;
    if (destination_stack)
    {
        row_position_after_move = destination_stack->cards.count + iteration_index;

        // @Hack:
        if (!destination_stack->cards.count && destination_stack->stack_type == Stack_Type::FOUNDATION)
        {
            assert(!destination_stack->will_be_occupied);
            destination_stack->will_be_occupied = true; // @Fixme @Bug: This is a bug right here because we should set will be occupied when the card starts to move, not when we schedule it. The reason why it isn't causing any damage is that currently 'will_be_occupied' is only useful for moving up the number 1's. But we should fix it anyways.
        }
    }
    else
    {
        row_position_after_move = card->row_position;
    }

    add_visual_interpolation(move_type, card, visual_end, destination_stack, row_position_after_move, duration, delay_duration);
}

void move_card_to_base_with_offset(Move_Type move_type, Card *card, Stack *to_stack, i64 row_offset, f32 duration = -1, f32 delay_duration = -1)
{
    auto manager = card->entity_manager;
    auto gv = get_game_visuals(manager);

    assert(manager->card_size_is_set);

    auto visual_end = to_stack->visual_start;

    auto row_position_after_move = to_stack->cards.count + row_offset;
    visual_end.y -= row_position_after_move * manager->card_size.y * gv->default_card_row_offset;

    move_one_card(move_type, card, visual_end, to_stack, row_offset, duration, delay_duration);
}

void deal_one_card_with_offset(Card *card, Stack *to_stack, i64 iteration_index)
{
    auto gv             = get_game_visuals(card->entity_manager);
    auto duration       = gv->default_dealing_visual_duration;
    auto delay_duration = gv->default_dealing_delay_duration;

    move_card_to_base_with_offset(Move_Type::DEAL, card, to_stack, iteration_index, duration, delay_duration);
}

// This makes the visual end be the visual start of the destination stack.
void move_one_card(Move_Type move_type, Card *card, Stack *destination_stack, i64 iteration_index, f32 duration, f32 delay_duration)
{
    assert(destination_stack);
    move_one_card(move_type, card, destination_stack->visual_start, destination_stack, iteration_index, duration, delay_duration);
}

void init_new_level(Entity_Manager *manager)
{
    auto seed = static_cast<i64>(time(NULL)) + static_cast<i64>(timez.real_world_time * 100); // Arbitrary.

    if constexpr (DEVELOPER_MODE)
    {
        logprint("init_new_level", "Game seed is %ld, win count is %ld\n", seed, manager->win_count);
    }

    assert(manager->level_from_seed);
    manager->level_from_seed(manager, seed);
}

void restart_current_level(Entity_Manager *manager)
{
    assert(manager->level_from_seed);
    manager->level_from_seed(manager, manager->current_level_seed);
}

bool is_inside(f32 x, f32 y, Rect r)
{
    return (x >= r.x) && (x <= r.x + r.w) && (y >= r.y) && (y <= r.y + r.h);
}

bool is_inside_circle(f32 x, f32 y, Vector2 center, f32 radius)
{
    auto dist = glm::distance(Vector2(x, y), center);
    return dist <= radius;
};

void cleanup_level(Entity_Manager *manager)
{
    //
    // @Speed: We should use bucket arrays for these.
    //
    for (auto it : manager->all_cards)
    {
        if (it->derived_pointer) my_free(it->derived_pointer);
        my_free(it);
    }

    for (auto it : manager->all_stacks) my_free(it);
}

void init_game()
{
    init_metadata();

    solitaire_variant = STARTING_VARIANT; // @Incomplete: Do a menu chooser type of thing later.

    Entity_Manager *manager = NULL;
    switch (solitaire_variant)
    {
        // @Cleanup: These could be cleanup.
        // @Cleanup: These could be cleanup.
        // @Cleanup: These could be cleanup.

        case Solitaire_Variant::UNKNOWN: exit(1); break; // @Incomplete: Log error.
        case Solitaire_Variant::SHENZHEN: {
            if (!shenzhen_entity_manager) shenzhen_entity_manager = make_entity_manager();

            manager = shenzhen_entity_manager;
            init_shenzhen_manager(manager);
        } break;
        case Solitaire_Variant::SAWAYAMA: {
            if (!sawayama_entity_manager) sawayama_entity_manager = make_entity_manager();

            manager = sawayama_entity_manager;
            init_sawayama_manager(manager);
        } break;
        case Solitaire_Variant::KABUFUDA: {
            if (!kabufuda_entity_manager) kabufuda_entity_manager = make_entity_manager();

            manager = kabufuda_entity_manager;
            init_kabufuda_manager(manager);
        } break;
        case Solitaire_Variant::CLUJ: {
            if (!cluj_entity_manager) cluj_entity_manager = make_entity_manager();

            manager = cluj_entity_manager;
            init_cluj_manager(manager);
        } break;
    }
    
    assert(manager->init_textures);
    manager->init_textures(manager);
    init_new_level(manager);
//    init_keymap_highlight(); // @Incomplete: Turn this back on.
}

void maybe_mark_undo_end_frame(Entity_Manager *manager)
{
    if (manager->moving_cards.count) return;
    
    if (manager->in_game_state == In_Game_State::DEALING)
    {
        undo_mark_beginning(manager);
    }

    if (manager->in_game_state != In_Game_State::PLAYING) return;

    undo_end_frame(manager->undo_handler);
}

void end_visual_interpolation(Card *card)
{
    card->visual_start_time = -1;

    card->visual_position = card->visual_end;
    card->visual_elapsed  = card->visual_duration;

    // Set the z_layer to be the correct one.
    card->z_layer = card->visual_end_z_layer;

    if (card->visual_end_at_stack) card->visual_end_at_stack->will_be_occupied = false;

    auto manager = card->entity_manager;

    auto removed_from_moving_array = array_unordered_remove_by_value(&manager->moving_cards, card);
    assert(removed_from_moving_array);

    assert(manager->post_move_reevaluate);
    if (manager->condition_for_post_move_reevaluate)
    {
        if (manager->condition_for_post_move_reevaluate(card))
        {
            manager->post_move_reevaluate(manager);
        }
    }
    else
    {
        manager->post_move_reevaluate(manager);
    }

    // We don't want to clutter the undo system with a bunch of failed moves.
    if (card->visual_move_type != Move_Type::FAILED_MOVE)
    {
        maybe_mark_undo_end_frame(manager);
    }
}

void update_single_visual_interpolation(Card *card)
{
    if (card->visual_start_time < 0) return; // It is done.
    if (timez.current_time < card->visual_start_time) return; // Not started yet.

    auto dt = timez.current_dt;
    card->visual_elapsed += dt;
    if (card->visual_elapsed >= card->visual_duration)
    {
        end_visual_interpolation(card);
        return;
    }

    auto denom = card->visual_duration;
    constexpr auto EPSILON = .000001f;
    if (denom < EPSILON) denom = 1;

    auto visual_t = card->visual_elapsed / denom;
    f32 position_t = -1;

    if (card->visual_move_type == Move_Type::DEAL)
    {
        position_t = visual_t;
    }
    else
    {
        if (visual_t <= .5f)
        {
//        position_t = 2.f * visual_t * visual_t;
//        position_t = 2.f * visual_t * (1.f - visual_t);
            position_t = visual_t;
        }
        else
        {
            visual_t -= .5f;
            position_t = 2.f * visual_t * (1.f - visual_t) + .5f;
        }
    }

    assert(0 <= position_t && position_t <= 1);
    card->visual_position = lerp(card->visual_start, card->visual_end, position_t);
}

void card_delay_start_of_frame_update() // :DelayHack
{
    auto manager = get_current_entity_manager();

    manager->total_visual_interpolations_added_this_frame = 0;

    if (manager->total_card_delay_time > 0)
    {
        manager->total_card_delay_time -= timez.current_dt;
        if (manager->total_card_delay_time <= 0) manager->total_card_delay_time = 0;
    }
}

void card_delay_end_of_frame_update() // :DelayHack
{
    auto manager = get_current_entity_manager();

    if (!manager->total_visual_interpolations_added_this_frame)
    {
        // Reset the delay.
        manager->total_card_delay_time = 0;
    }
}

bool sort_card_z_layers(Card *a, Card *b)
{
    return a->z_layer < b->z_layer;
}

bool sort_by_visual_transaction(Card *a, Card *b)
{
    return a->in_transaction_id < b->in_transaction_id;
}

// This assumes you would do the visual_position for the cards by yourself.
// @Cleanup: Remove this function, instead, we should be using Move_Type::PICK with move_one_card.
RArr<Card*> snap_cards_to_stack(Stack *from_stack, i64 start_index, Stack *to_stack)
{
    assert(from_stack->cards.data != to_stack->cards.data);

    RArr<Card*> moved;
    moved.allocator = {global_context.temporary_storage, __temporary_allocator};

    for (i64 it_index = start_index; it_index < from_stack->cards.count; ++it_index)
    {
        auto card = from_stack->cards[it_index];

        // Rare case in normal play but if we turn up the visual interpolation duration,
        // but we actually want all the cards to be visually positioned correct when we move a stack.
        if (card->visual_start_time >= 0)
        {
            // If we were in a middle of a visual interpolation while cutting to another stack, we jump to the end.
            end_visual_interpolation(card);
        }

        card->visual_move_type = Move_Type::PICK;
        physically_move_one_card(card, to_stack);
        array_add(&moved, card);
    }

    from_stack->cards.count -= moved.count;

    return moved;
}

Card *get_card_under_cursor(Entity_Manager *manager, f32 mouse_x, f32 mouse_y)
{
    Card *result = NULL;
    for (auto it : manager->all_cards)
    {
        if (it->visual_start_time >= 0)
        {
            // :DoubleClick We are apparently skipping if we are double clicking.
            continue;
        }

        auto inside = is_inside(mouse_x, mouse_y, it->hitbox);
        if (inside)
        {
            if (!result)
            {
                result = it;
                continue;
            }

            // @Incomplete: We should compare based on the z_layer.
            assert(result->row_position != it->row_position); // Sanity check, because we can't pick two things with the same row at the same time.

            if (result->row_position < it->row_position) result = it;
//            if (result->z_layer < it->z_layer) result = it;
        }
    }

    return result;
}

my_pair<Stack* /* first stack with matching color */, Stack* /* first unoccupied stack */> find_stack_to_place_card(Color color, _type_Type card_type, SArr<Stack*> pile)
{
    Stack *first_matching_stack_in_pile = NULL; // If there are multiple matching stacks, pick the first one.
    Stack *first_unoccupied_in_pile     = NULL;

    for (auto stack : pile)
    {
        if (stack->closed) continue;
        if (stack->will_be_occupied) continue;

        if (!stack->cards.count)
        {
            if (!first_unoccupied_in_pile) first_unoccupied_in_pile = stack;
        }
        else
        {
            auto peeked = array_peek_last(&stack->cards);
            if (peeked->card_type == card_type)
            {
                if (peeked->color == color)
                {
                    first_matching_stack_in_pile = stack;
                    break;
                }
            }
        }
    }

    return {first_matching_stack_in_pile, first_unoccupied_in_pile};
}

void interpolate_stack_to_another_stack(Move_Type move_type, Stack *from_stack, i64 start_index, Stack *to_stack)
{
    assert(from_stack->cards.data != to_stack->cards.data);

    auto moved_count = from_stack->cards.count - start_index;

    auto row_offset = 0;
    for (i64 it_index = start_index; it_index < from_stack->cards.count; ++it_index)
    {
        auto card = from_stack->cards[it_index];
        assert(card->visual_start_time < 0);

        if (move_type == Move_Type::FAILED_MOVE)
        {
            move_one_card(move_type, card, card->previous_visual_position, to_stack, row_offset);
        }
        else if (move_type == Move_Type::PLACE)
        {
            move_card_to_base_with_offset(move_type, card, to_stack, row_offset);
        }
        else
        {
            move_one_card(move_type, card, to_stack, row_offset);
        }
        
        row_offset += 1;
    }

    from_stack->cards.count -= moved_count;
}

bool handle_developer_event(Event event)
{
    if (!event.key_pressed) return false;

    auto manager = get_current_entity_manager();

    auto changed_time_rate = false;

    constexpr auto TIME_CHANGE_DELTA = 1/30.f;
    constexpr auto TIME_RATE_MIN     = .2f;
    constexpr auto TIME_RATE_MAX     = 4.f;
    
    switch (event.key_code)
    {
        case CODE_LEFT_BRACKET: {
            if (global_time_rate <= TIME_RATE_MIN) break;

            global_time_rate -= TIME_CHANGE_DELTA;
            changed_time_rate = true;
        } break;
        case CODE_RIGHT_BRACKET: {
            if (global_time_rate >+ TIME_RATE_MAX) break;

            global_time_rate += TIME_CHANGE_DELTA;
            changed_time_rate = true;
        } break;
        case CODE_F2: { // @Incomplete: Make the buttons for restart and new level.
            cleanup_level(manager);
            restart_current_level(manager);
        } break;
        case CODE_F3: { // @Incomplete: Make the buttons for restart and new level.
            cleanup_level(manager);
            init_new_level(manager);
        } break;
        case CODE_F9: { // @Incomplete: Make the buttons for undo.
            if (manager->in_game_state != In_Game_State::PLAYING) break;

            if (manager->moving_cards.count) break; // @Incomplete: We want undo even if there are moving cards.
            do_one_undo(manager);

            assert(manager->post_undo_reevaluate);
            manager->post_undo_reevaluate(manager);
        } break;
    }

    if (changed_time_rate)
    {
        auto current_rate = tprint(String("Time rate: %g"), global_time_rate);

        auto fader = game_report(current_rate);
        fader->fade_out_t = .5f;

        return true;
    }

    return false;
}

void calculate_one_hitbox(Card *card)
{
    auto vpos = card->visual_position;

    auto manager = card->entity_manager;
    assert(manager->card_size_is_set);

    auto card_size = manager->card_size;
    auto hitbox    = &card->hitbox;

    *hitbox = get_rect(vpos.x, vpos.y, card_size.x, -card_size.y);
}

void set_dealing_stack(Entity_Manager *manager, Stack *stack)
{
    assert(stack);
    manager->dealing_stack = stack;
    manager->dealing_stack->closed = true; // This makes the cards face downwards before dealt out to the base pile.
}

void simulate()
{
    auto manager = get_current_entity_manager();

    if (manager->in_game_state == In_Game_State::DEALING)
    {
        assert(manager->is_done_with_dealing);

        auto done = manager->is_done_with_dealing(manager);
        if (done)
        {
            manager->in_game_state = In_Game_State::PLAYING;

            auto dealing_stack = manager->dealing_stack;
            assert(dealing_stack);

            // Only unclose the stack if there isn't any cards left.
            if (!dealing_stack->cards.count) dealing_stack->closed = false;

            // We manually do reevaluate because while we were visually moving cards,
            // the 'post_move_reevaluate' will be skipped out.
            assert(manager->post_move_reevaluate);
            manager->post_move_reevaluate(manager);
        }
    }

    if (manager->in_game_state == In_Game_State::PLAYING)
    {
        if (manager->check_for_cards_highlight)
        {
            manager->check_for_cards_highlight(manager);
        }
    }

/*
    // Visual positions for the base pile.                 @Cleanup: Is this needed?
    // @Incomplete: Fan in the cards/cascade if the stack is too long.
    for (i64 stack_index = 0; stack_index < manager->base_pile.count; ++stack_index)
    {
        auto stack = manager->base_pile[stack_index];
        for (auto card : stack->cards)
        {
            if (card->visual_start_time >= 0) continue;

            auto gv = &default_game_visuals;

            Vector2 visual_position;
            visual_position.x = gv->numbers_x[stack_index];

            auto row = card->row_position;
            auto y_offset = row * card_front_map->height * gv->card_row_offset;
            visual_position.y = gv->BASE_ROW_Y - y_offset;

            card->visual_position = visual_position;
        }
    }
*/

    if (manager->in_game_state != In_Game_State::END_GAME)
    {
        RArr<Card*> visual_interpolations_started_this_frame;
        visual_interpolations_started_this_frame.allocator = {global_context.temporary_storage, __temporary_allocator};
        array_reserve(&visual_interpolations_started_this_frame, manager->moving_cards.count);

        for (auto it : manager->moving_cards)
        {
            if (it->visual_start_time < 0) continue;
            if (timez.current_time < it->visual_start_time) continue;
        
            if (!it->did_physical_move)
            {
                it->did_physical_move = true;
                array_add(&visual_interpolations_started_this_frame, it);
            }
        }

        array_qsort(&visual_interpolations_started_this_frame, sort_by_visual_transaction);

        // For all the interpolations that will be start on this frame, we do a physical move.
        for (auto it : visual_interpolations_started_this_frame)
        {
            physically_move_one_card(it, it->visual_end_at_stack);
            it->z_layer = it->visual_start_z_layer; // Once it starts, we kick in the start z_layer.
        }
    }
    
    // Calculate the hitboxes of all cards.
    for (auto it : manager->all_cards)
    {
        update_single_visual_interpolation(it);
        calculate_one_hitbox(it);
    }

    if (manager->is_dragging)
    {
        for (auto it : manager->dragging_stack->cards)
        {
            it->visual_position += Vector2(mouse_delta_x, mouse_delta_y);
        }
    }
}

void drag_or_release(Entity_Manager *manager, bool should_drag)
{
    if (manager->handle_game_mouse_pointer_events)
    {
        auto handled = manager->handle_game_mouse_pointer_events(manager, mouse_pointer_x, mouse_pointer_y, should_drag);
        if (handled) return;
    }

    // Don't let you drag.
    if (manager->in_game_state != In_Game_State::PLAYING) return;

    auto dragging_stack = manager->dragging_stack;

    if (!should_drag)
    {
        if (manager->is_dragging)
        {
            //
            // Releasing the click.
            //
            assert(dragging_stack->cards.count);

            assert(manager->can_move);
            auto destination_stack = manager->can_move(mouse_pointer_x, mouse_pointer_y, dragging_stack);

            auto peeked = dragging_stack->cards[0];
            auto previous_stack = get_previous_stack(peeked);
            assert(previous_stack);

            if (!destination_stack || (previous_stack == destination_stack))
            {
                // If we cannot move, we reset our position to the previous stack.
                interpolate_stack_to_another_stack(Move_Type::FAILED_MOVE, dragging_stack, 0, previous_stack);
            }
            else
            {
                if (destination_stack->stack_type == Stack_Type::BASE)
                {
                    interpolate_stack_to_another_stack(Move_Type::PLACE, dragging_stack, 0, destination_stack);

                    // // @Hack: Unify hack, clean up with a better way to do this kind of things.
                    // if (manager->should_unify && manager->should_unify(dragging_stack, destination_stack))
                    // {
                    //     // Do the moving in for the destination stack.
                    //     interpolate_stack_to_another_stack(Move_Type::UNIFY, destination_stack, 0, destination_stack);

                    //     // Do the moving in for the dragging stack.
                    //     interpolate_stack_to_another_stack(Move_Type::UNIFY, dragging_stack, 0, destination_stack);
                        
                    // }
                    // else
                    // {
                    // }
                }
                else
                {
                    // @Cleanup: This part can be cleaned up because it is pretty similar to the interpolate_stack_* function.

                    // Move everything to the first position.
                    for (auto it_index = 0; it_index < dragging_stack->cards.count; ++it_index)
                    {
                        auto card = dragging_stack->cards[it_index];
                        move_one_card(Move_Type::PLACE, card, destination_stack, it_index);
                    }

                    dragging_stack->cards.count = 0;
                }
            }

            assert(dragging_stack->cards.count == 0);
            manager->is_dragging = false;
        }
    }
    else
    {
        assert(!manager->is_dragging);

        if (manager->moving_cards.count) return; // If we are automoving or dealing, we don't drag.

        auto to_pick = get_card_under_cursor(manager, mouse_pointer_x, mouse_pointer_y);
        if (!to_pick) return;

        assert(manager->can_drag);
        auto can = manager->can_drag(to_pick);
        if (can)
        {
            auto cut_stack = get_stack(to_pick);
            assert(cut_stack);
            assert(dragging_stack->cards.count == 0);

            auto cut_start_index = to_pick->row_position;
            auto moved = snap_cards_to_stack(cut_stack, cut_start_index, dragging_stack);

            for (auto it : moved) set_previous_stack(it, cut_stack);

            manager->is_dragging = true;
        }
    }
}

void set_game_window_properties(Entity_Manager *manager, Vector2 board_size)
{
    manager->has_set_offscreen_buffer_properties = true;
    manager->game_window_size = board_size;
}

// f32 time_of_previous_click = -1; :DoubleClick
void read_input()
{
    auto [mx, my] = get_mouse_pointer_position(glfw_window, true);

    auto manager = get_current_entity_manager();

    assert(manager->has_set_offscreen_buffer_properties);
    mx -= manager->game_window_offset.x;
    my -= manager->game_window_offset.y;

    mouse_pointer_x = mx;
    mouse_pointer_y = my;

    for (auto event : events_this_frame)
    {
        if (event.type != EVENT_KEYBOARD) continue;

        auto key     = event.key_code;
        auto pressed = event.key_pressed;

        if constexpr (DEVELOPER_MODE)
        {
            auto handled = handle_developer_event(event);
            if (handled) continue;
        }

        switch (key)
        {
            case CODE_MOUSE_LEFT: {
                drag_or_release(manager, static_cast<bool>(pressed));

                /* :DoubleClick
                   bool double_clicked = false;
                   if (pressed)
                   {
                   auto now = timez.real_world_time;
                   if (time_of_previous_click > 0)
                   {
                   auto diff = now - time_of_previous_click;
                    
                   constexpr auto DIFF_MIN = .05f;
                   constexpr auto DIFF_MAX = 0.2f;
                   double_clicked = (DIFF_MIN <= diff) && (diff <= DIFF_MAX);
                   }
                    
                   time_of_previous_click = now;
                   }
                */
            } break;
            default: {
/*  @Incomplete: Enable key highlighting after refactor
                if (!pressed)
                {
                    if (key == keymap_highlight_code)
                    {
                        keymap_highlight_code = CODE_UNKNOWN;
                        for (auto it : manager->all_cards) end_card_highlight(it);
                    }
                    
                    break;
                }
                else
                {
                    auto [_, found] = table_find(&card_type_keymap_highlight_lookup, key);
                    if (!found) break;

                    keymap_highlight_code = key;
                }
*/
            }
        }
    }

    per_frame_update_mouse_position();
}



/*  @Speed: Do this instead of the above...
    @Archive: Figure out why ordered_remove does not work with memcpy/memmove.

    auto mem_src = src->data + start_index;

    auto cards_to_remove = src->count - start_index;

    auto old_dest_count = dest->count;
    array_resize(dest, dest->count + cards_to_remove);

    printf("dest count is %ld\n", dest->count);

    auto mem_dest = dest->data + old_dest_count;
    memcpy(mem_dest, mem_src, cards_to_remove * sizeof(src[0]));

    src->count -= cards_to_remove;
*/
