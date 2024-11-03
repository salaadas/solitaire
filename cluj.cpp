// @Incomplete: We need a way to render text from the fixed width texture.
// @Incomplete: Do victory screen.

//
// :ClujUndoHack
// @Hack @Temporary: Right now, there isn't a way for us to undo properly in this game.
// So we need to either make is_inverted a property of the base Card,
//               OR     make the undo system accepts more types of cards, which is
//                      is better but really right now only one game uses it :)

#include "cluj.h"
#include "solitaire.h"
#include "main.h"
#include "opengl.h"
#include "draw.h"
#include "catalog.h"

static constexpr auto MIN_RANK = 6;
static constexpr auto MAX_RANK = 14;

static constexpr auto STARTING_LETTER_RANK = 11; // For V, D, K, T

static constexpr auto CARDS_PER_RANK = 4;
static constexpr auto MAX_STARTING_CARDS_PER_STACK_COUNT = 6;

static Cluj_Game_Visuals game_visuals;

static Stack *dealing_stack;

static Texture_Map *card_front_map;
static Texture_Map *card_inverted_map;
static Texture_Map *card_back_map;
static Texture_Map *card_stack_map;

static Texture_Map *release_region_map;

static RArr<Texture_Map*> main_number_maps;
static Texture_Map *heart_map;
static Texture_Map *leaf_map;
static Texture_Map *arrow_map;
static Texture_Map *diamond_map;

static RArr<Texture_Map*> main_face_maps;
static RArr<Texture_Map*> main_face_inverted_maps;
static RArr<Texture_Map*> main_face_glitch_maps;

static void init_textures(Entity_Manager *manager)
{
    auto gv = &game_visuals;

    // @Incomplete: Figure out the offscreen buffer size thing.
    {
        auto w = gv->default_playfield_width;
        auto h = gv->default_playfield_height;

        auto board_size = Vector2(w, h);
        set_game_window_properties(manager, board_size);
    }

    release_region_map = load_nearest_mipmapped_texture(String("cluj_card_region"));

    //
    // Override the card size in the Entity Manager with the scaled one.
    //
    card_front_map    = load_nearest_mipmapped_texture(String("cluj_card_front_2"));
    card_inverted_map = load_nearest_mipmapped_texture(String("cluj_invert_card_2"));
    card_back_map     = load_nearest_mipmapped_texture(String("cluj_card_back"));
    card_stack_map    = load_nearest_mipmapped_texture(String("cluj_card_stack"));

    auto card_size = Vector2(card_front_map->width, card_front_map->height) * CLUJ_GAME_SCALE;
    set_general_card_size(manager, card_size.x, card_size.y);

    heart_map   = load_nearest_mipmapped_texture(String("cluj_heart"));
    leaf_map    = load_nearest_mipmapped_texture(String("cluj_leaf"));
    arrow_map   = load_nearest_mipmapped_texture(String("cluj_arrow"));
    diamond_map = load_nearest_mipmapped_texture(String("cluj_diamond"));

    auto add_map_to_array = [](RArr<Texture_Map*> *array, String name) {
        auto map = load_nearest_mipmapped_texture(name);
        assert(map);
        array_add(array, map);
    };

    add_map_to_array(&main_number_maps, String("cluj_number_6"));
    add_map_to_array(&main_number_maps, String("cluj_number_7"));
    add_map_to_array(&main_number_maps, String("cluj_number_8"));
    add_map_to_array(&main_number_maps, String("cluj_number_9"));
    add_map_to_array(&main_number_maps, String("cluj_number_10"));

    add_map_to_array(&main_face_maps, String("cluj_v_card"));
    add_map_to_array(&main_face_maps, String("cluj_d_card"));
    add_map_to_array(&main_face_maps, String("cluj_k_card"));
    add_map_to_array(&main_face_maps, String("cluj_t_card"));

    add_map_to_array(&main_face_inverted_maps, String("cluj_invert_v_card"));
    add_map_to_array(&main_face_inverted_maps, String("cluj_invert_d_card"));
    add_map_to_array(&main_face_inverted_maps, String("cluj_invert_k_card"));
    add_map_to_array(&main_face_inverted_maps, String("cluj_invert_t_card"));

    add_map_to_array(&main_face_glitch_maps, String("cluj_glitch_v_card"));
    add_map_to_array(&main_face_glitch_maps, String("cluj_glitch_d_card"));
    add_map_to_array(&main_face_glitch_maps, String("cluj_glitch_k_card"));
    add_map_to_array(&main_face_glitch_maps, String("cluj_glitch_t_card"));
}

static bool check_for_victory(Entity_Manager *manager)
{
    if (manager->moving_cards.count) return false;
    for (auto it : manager->base_pile)
    {
        if (!it->cards.count) continue;
        if (!it->closed) return false;
    }

    return true;
}

static void do_victory_animation(Entity_Manager *manager)
{
    manager->win_count += 1;
    manager->in_game_state = In_Game_State::END_GAME;

    printf("Win!\n");
    // @Incomplete: Show some victory panel.
}

static void post_move_reevaluate(Entity_Manager *manager)
{
    if (manager->in_game_state != In_Game_State::PLAYING) return;
    if (manager->dragging_stack->cards.count) return; // Don't do automove while we are dragging stuff.
    if (manager->moving_cards.count) return; // There are still moving stuff, so don't reeval.

    // Reevaluate all the inverted cards, if they are in the right positions, un-invert them.
    for (auto stack : manager->base_pile)
    {
        if (!stack->cards.count) continue;

        auto last = Down<Cluj_Card>(array_peek_last(&stack->cards));
        if (!last->card->is_inverted) continue;

        if (stack->cards.count == 1)
        {
            last->card->is_inverted = false;
            continue;
        }

        auto before_last = Down<Cluj_Card>(stack->cards[last->card->row_position - 1]);
        if (before_last->rank - last->rank == 1)
        {
            last->card->is_inverted = false;
            continue;
        }
    }

    //
    // Close stack if the stack is in the order 6, 7, 8, 9, 10, V, D, K, T
    //
    auto should_close_stack = [](Stack *stack) -> bool {
        if (stack->cards.count != (MAX_RANK - MIN_RANK + 1)) return false;

        auto should_close = true;
        for (auto it_index = 0; it_index < stack->cards.count-1; ++it_index)
        {
            auto upper = Down<Cluj_Card>(stack->cards[it_index]);
            auto lower = Down<Cluj_Card>(stack->cards[it_index + 1]);

            if ((it_index == 0) && (upper->rank != MAX_RANK))
            {
                should_close = false;
                break;
            }

            if (upper->rank - lower->rank != 1)
            {
                should_close = false;
                break;
            }
        }

        return should_close;
    };

    for (auto stack : manager->base_pile)
    {
        auto closed_before = stack->closed;
        stack->closed = should_close_stack(stack);

        if (stack->closed == closed_before) continue;

        // @Hack: :UnifyHack For BASE stacks, we need to fix the visual position of each thing to
        // start from the visual start of the stack because we don't want the cards to
        // fan out when closed.
        if (stack->closed)
        {
            for (auto it : stack->cards)
            {
                assert(it->visual_start_time < 0);
                assert(stack->stack_index == it->in_stack_index);

                add_visual_interpolation(Move_Type::UNIFY, it, stack->visual_start, stack, stack->z_offset + it->row_position);
            }

            stack->cards.count = 0;
        }
    }

    auto victory = check_for_victory(manager);
    if (victory)
    {
        do_victory_animation(manager);
    }
}

static void post_undo_reevaluate(Entity_Manager *manager)
{
    // @Fixme: Right now, undo is wrong because it does not revert back to non-inverted state.
    // @Fixme: Right now, undo is wrong because it does not revert back to non-inverted state.
    // @Fixme: Right now, undo is wrong because it does not revert back to non-inverted state.
    // @Fixme: Right now, undo is wrong because it does not revert back to non-inverted state.
    // @Fixme: Right now, undo is wrong because it does not revert back to non-inverted state.
    // @Fixme: Right now, undo is wrong because it does not revert back to non-inverted state.
    post_move_reevaluate(manager);
}

static void init_stack_piles(Entity_Manager *manager)
{
    auto gv = &game_visuals;

    // Dragging is a special stack that does not have a release region.
    manager->dragging_stack = create_stack(manager, Stack_Type::DRAGGING, gv->dragging_z_layer_offset, {});

    auto w =      release_region_map->width  * CLUJ_GAME_SCALE;
    auto h = -1 * release_region_map->height * CLUJ_GAME_SCALE;

    auto dealing_region = get_rect(gv->dealing_stack_x, gv->dealing_stack_y, w, h);
    dealing_stack = create_stack(manager, Stack_Type::TOP, gv->deal_stack_z_layer_offset, dealing_region);

    // Base pile.
    array_resize(&manager->base_pile, CLUJ_BASE_COLUMNS_COUNT);
    for (auto it_index = 0; it_index < CLUJ_BASE_COLUMNS_COUNT; ++it_index)
    {
        // The release region for the base pile extends far down.
        auto release_region = get_rect(gv->base_x[it_index], gv->base_y, w, -1 * gv->base_y);
        auto stack = create_stack(manager, Stack_Type::BASE, gv->base_z_layer_offset, release_region);

        manager->base_pile[it_index] = stack;
        array_reserve(&stack->cards, MAX_STARTING_CARDS_PER_STACK_COUNT);
    }
}

static void Make(Entity_Manager *manager, Rank rank, Texture_Map *front_map, Texture_Map *inverted_map, Texture_Map *glitch_map, Texture_Map *corner_number, Texture_Map *card_pattern)
{
    assert((MIN_RANK <= rank) && (rank <= MAX_RANK));

    auto card = New<Cluj_Card>();
    auto base = register_general_card(manager, _make_Type(Cluj_Card), card);

    card->card = base;

    base->front_map = front_map;
    base->back_map  = card_back_map;

    card->inverted_map = inverted_map;
    card->glitch_map   = glitch_map;

    base->small_icon = corner_number;
    base->large_icon = card_pattern;
    
    card->rank  = rank;
    base->color = Color::WHITE; // Color doesn't matter in here.
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

    array_reserve(&manager->all_cards, CARDS_PER_RANK * (MAX_RANK - MIN_RANK + 1));
    array_reset(&manager->all_cards); // @Cleaup: Redundant with cleanup_level?

    for (auto rank = MIN_RANK; rank < STARTING_LETTER_RANK; ++rank)
    {
        auto number_map = main_number_maps[rank - MIN_RANK];

        Make(manager, rank, card_front_map, card_inverted_map, NULL, number_map, heart_map);
        Make(manager, rank, card_front_map, card_inverted_map, NULL, number_map, leaf_map);
        Make(manager, rank, card_front_map, card_inverted_map, NULL, number_map, arrow_map);
        Make(manager, rank, card_front_map, card_inverted_map, NULL, number_map, diamond_map);
    }

    for (auto rank = STARTING_LETTER_RANK; rank <= MAX_RANK; ++rank)
    {
        auto face_map     = main_face_maps[rank - STARTING_LETTER_RANK];
        auto inverted_map = main_face_inverted_maps[rank - STARTING_LETTER_RANK];
        auto glitch_map   = main_face_glitch_maps[rank - STARTING_LETTER_RANK];

        for (auto i = 0; i < CARDS_PER_RANK; ++i)
        {
            Make(manager, rank, face_map, inverted_map, glitch_map, NULL, NULL);
        }
    }

    if constexpr (!DEBUG_NO_SHUFFLE)
    {
        shuffle(&manager->all_cards);
    }

    set_dealing_stack(manager, dealing_stack);

    for (auto it_index = 0; it_index < manager->all_cards.count; ++it_index)
    {
        auto it = manager->all_cards[it_index];
        set_stack(it, dealing_stack);

        it->row_position    = it_index;
        it->z_layer         = dealing_stack->z_offset + it->row_position;
        it->visual_position = dealing_stack->visual_start; // Might be wrong.

        array_add(&dealing_stack->cards, it);
    }

    auto gv = &game_visuals;

    auto it_index = dealing_stack->cards.count - 1;
    for (auto row = 0; row < MAX_STARTING_CARDS_PER_STACK_COUNT; ++row)
    {
        for (auto column = 0; column < manager->base_pile.count; ++column)
        {
            auto card = dealing_stack->cards[it_index];
            auto destination_stack = manager->base_pile[column];

            deal_one_card_with_offset(card, destination_stack, row);
            it_index -= 1;
        }
    }
}

static bool is_done_with_dealing(Entity_Manager *manager)
{
    // Once the deal stack is empty, we know we have dealt out everything.
    if (dealing_stack->cards.count) return false;

    // Double check to see if there is anything still moving.
    if (manager->moving_cards.count) return false;

    return true;
}

// @Cleanup: Do a clean up pass on this function.
static bool should_index_be_renderred_at_middle(Rank rank, i64 index, i64 total)
{
    if (index < 0)      return false;
    if (index == total) return false;

    auto b = index == 1 && rank == 7;
    b = b || (index == 2 && rank == 9);
    b = b || (rank == 10 && (index == 1 || index == total-2));

    return b;
}

static bool should_render_card_stack_texture(Stack *stack)
{
    // Only render the stack if the count is > 1, because when the count is 1, we don't see it anyways.
    auto b = (stack->cards.count > 1) && (stack->closed || (stack->stack_type != Stack_Type::BASE && stack->stack_type != Stack_Type::DRAGGING));

    return b;
}

static void render_card_from_top_left(Card *card)
{
    set_shader(shader_argb_and_texture);

    auto manager = card->entity_manager;
    assert(manager->card_size_is_set);

    auto vpos = card->visual_position;

    auto cluj_card = Down<Cluj_Card>(card);
    auto in_stack  = get_stack(card);

    auto should_render_stack = should_render_card_stack_texture(in_stack);
    if (should_render_stack)
    {
        if (card->row_position == 0)
        {
            auto sp0 = vpos + Vector2(0, -manager->card_size.y);
            auto sp1 = sp0 + Vector2(manager->card_size.x, 0);

            auto csh = card_stack_map->height * CLUJ_GAME_SCALE;
            auto sp2 = sp1 + Vector2(0, csh);
            auto sp3 = sp0 + Vector2(0, csh);

            set_texture(String("diffuse_texture"), card_stack_map);
            immediate_quad(sp0, sp1, sp2, sp3, 0xffffffff, card->z_layer);
        }

        auto y_offset = (in_stack->cards.count - 1) * game_visuals.card_fixed_pixel_offset_amount;
        vpos.y += y_offset;
    }

    Texture_Map *card_map = NULL;
    if (in_stack->closed)
    {
        card_map = card->back_map;
    }
    else
    {
        if (cluj_card->card->is_inverted)
        {
            auto b = (in_stack->stack_type == Stack_Type::DRAGGING) && (cluj_card->glitch_map != NULL);
            if (b) card_map = cluj_card->glitch_map;
            else   card_map = cluj_card->inverted_map;
        }
        else
        {
            card_map = card->front_map;
        }
    }

    assert(card_map);
    set_texture(String("diffuse_texture"), card_map);
    immediate_quad_from_top_left(vpos, manager->card_size, Vector4(1, 1, 1, 1), card->z_layer);

    if (DEBUG_HITBOX)
    {
        set_shader(shader_argb_no_texture);

        Vector2 p0, p1, p2, p3;
        get_quad(card->hitbox, &p0, &p1, &p2, &p3);

        auto color = argb_color(Vector4(1, 0, 1, .35f));
        immediate_quad(p0, p1, p2, p3, color, card->z_layer);

        set_shader(shader_argb_and_texture);
    }

    if (in_stack->closed) return;

    auto suit_map = card->large_icon;
    if (!suit_map) return;

    auto number_map = card->small_icon;
    assert(number_map);

    // We know we are rendering a number if we get here.
    auto cw = manager->card_size.x;
    auto ch = manager->card_size.y;

    //
    // The number rank rendering part.
    //
    auto ox = cw * .054f;
    auto oy = ch * .042f;

    // Rank width, height.
    auto rw = card->small_icon->width  * CLUJ_GAME_SCALE;
    auto rh = card->small_icon->height * CLUJ_GAME_SCALE;

    auto top_left = vpos;
    top_left.x += ox;
    top_left.y -= oy;

    set_shader(shader_argb_no_texture);

    // The backing part.
    {
        Vector4 backing_color; // Backing is same color as the card's color.
        if (cluj_card->card->is_inverted) backing_color = Vector4(1, 1, 1, 1);
        else                              backing_color = Vector4(0, 0, 0, 1);

        auto backing_w = cw * .156f;
        auto backing_h = ch * .142f;
        immediate_quad_from_top_left(top_left, Vector2(backing_w, backing_h), backing_color, card->z_layer); // Top left.

        auto backing_pos_bottom_right = vpos + Vector2(cw, -ch);
        backing_pos_bottom_right += Vector2(-backing_w, backing_h);
        backing_pos_bottom_right.x -= ox;
        backing_pos_bottom_right.y += oy;
        immediate_quad_from_top_left(backing_pos_bottom_right, Vector2(backing_w, backing_h), backing_color, card->z_layer, 180); // Bottom right.
    }

    // The number part.
    Vector4 color; // Number is black if background is white, and vice versa.
    if (cluj_card->card->is_inverted) color = Vector4(0, 0, 0, 1);
    else                              color = Vector4(1, 1, 1, 1);

    set_shader(shader_argb_and_texture);
    set_texture(String("diffuse_texture"), number_map);

    immediate_quad_from_top_left(top_left, Vector2(rw, rh), color, card->z_layer); // Top left.

    auto bottom_right = vpos + Vector2(cw, -ch);
    bottom_right += Vector2(-rw, rh);
    bottom_right.x -= ox;
    bottom_right.y += oy;
    immediate_quad_from_top_left(bottom_right, Vector2(rw, rh), color, card->z_layer, 180); // Bottom right.

    //
    // The pattern part.
    //
    {
        auto sw = suit_map->width  * CLUJ_GAME_SCALE;
        auto sh = suit_map->height * CLUJ_GAME_SCALE;

        auto num_columns = 2;
        auto stuff_per_column = static_cast<i32>(cluj_card->rank / num_columns);
        if (stuff_per_column > 4) stuff_per_column = 4;
    
        auto ypad = sh * 1.6f;
        if (cluj_card->rank > 8) ypad = sh * 1.35f;

        auto start_y = vpos.y - ch + ypad;
        auto end_y   = vpos.y - ypad;

        set_texture(String("diffuse_texture"), suit_map);

        // Middle x.
        auto x = vpos.x + cw * .5f;

        // Left and right x.
        auto ox      = sw * .80f;
        auto x_left  = x - ox;
        auto x_right = x + ox;

        auto whats_left = cluj_card->rank - stuff_per_column * num_columns;
        auto middle_to_render = whats_left ? 1 : 0;

        auto total = whats_left + stuff_per_column;
        for (auto it_index = 0; it_index < total; ++it_index)
        {
            auto t = it_index / static_cast<f32>(total - 1);
            t = 1 - t; // To render from top down.
            assert(t >= 0);

            constexpr auto OFFSET_T = .03f;

            if (middle_to_render)
            {
                auto b = should_index_be_renderred_at_middle(cluj_card->rank, it_index, total);
                if (b)
                {
                    if (cluj_card->rank == 10)
                    {
                        auto sign = (it_index < (total-1)/2) ? +1 : -1;
                        t += sign * OFFSET_T;
                    }

                    auto y = lerp(start_y, end_y, t);
                    immediate_image(Vector2(x, y), Vector2(sw, sh), color, 0, false, card->z_layer);

                    whats_left -= 1;
                    if (!whats_left) middle_to_render = 0;
                    else             middle_to_render += 1;

                    continue;
                }
            }

            f32 sign = 0;
            f32 sign_9 = (cluj_card->rank == 9) ? -1 : 1;

            if (it_index <= (total-1)/2)
            {
                if (should_index_be_renderred_at_middle(cluj_card->rank, it_index + sign_9 * -1, total))
                {
                    sign = +1 * sign_9;
                }
            }
            else
            {
                if (should_index_be_renderred_at_middle(cluj_card->rank, it_index + sign_9 * + 1, total))
                {
                    sign = -1 * sign_9;
                }
            }

            t += sign * OFFSET_T;
            auto y = lerp(start_y, end_y, t);

            immediate_image(Vector2(x_left,  y), Vector2(sw, sh), color, 0, false, card->z_layer);
            immediate_image(Vector2(x_right, y), Vector2(sw, sh), color, 0, false, card->z_layer);
        }
    }

    immediate_flush();
}

static void draw(Entity_Manager *manager)
{
    rendering_2d_right_handed();

    auto gv = &game_visuals;

    // Render game playfield.
    {
        set_shader(shader_argb_no_texture);

        auto w = render_target_width;
        auto h = render_target_height;

        auto p0 = Vector2(0, 0);
        auto p1 = Vector2(w, 0);
        auto p2 = Vector2(w, h);
        auto p3 = Vector2(0, h);

        immediate_quad(p0, p1, p2, p3, 0xff000000);
        immediate_flush();
    }

    set_shader(shader_argb_and_texture);
    set_texture(String("diffuse_texture"), release_region_map);

    //
    // Render the individual stack's release region.
    //
    auto size = Vector2(release_region_map->width, release_region_map->height);
    size *= CLUJ_GAME_SCALE;

    auto pos_y = gv->base_y;
    for (auto i = 0; i < 6; ++i)
    {
        auto pos_x = gv->base_x[i];
        immediate_quad_from_top_left(Vector2(pos_x, pos_y), size, Vector4(1, 1, 1, 1));
    }

    if constexpr (DEBUG_RELEASE_REGION)
    {
        set_shader(shader_argb_no_texture);
        for (auto it : manager->base_pile)
        {
            immediate_quad_from_top_left(it->visual_start, size_of_rect(it->release_region), Vector4(1, .8, 0, .2));
        }

        immediate_flush();
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

    immediate_flush();
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
            if (destination_stack->closed) break;

            if (destination_stack->cards.count > 0)
            {
                auto last_card_in_destination = Down<Cluj_Card>(array_peek_last(&destination_stack->cards));

                // Cannot drag cards onto the cheated card.
                if (last_card_in_destination->card->is_inverted) return NULL;

                auto first_card_to_move = Down<Cluj_Card>(stack_to_move->cards[0]);

                if (last_card_in_destination->rank - first_card_to_move->rank != 1)
                {
                    // Cheat once, don't cheat more.
                    if (first_card_to_move->card->is_inverted) break;
                    if (first_card_to_move->card->previous_stack_index == destination_stack->stack_index) break;

                    if ((stack_to_move->cards.count == 1) && !last_card_in_destination->card->is_inverted)
                    {
                        // Cheating!
                        auto move_card = Down<Cluj_Card>(array_peek_last(&stack_to_move->cards));
                        move_card->card->is_inverted = true;

                        return destination_stack;
                    }

                    break;
                }
            }            

            return destination_stack;
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
        case Stack_Type::TOP: {
            return false;
        } break;
        case Stack_Type::BASE: {
            if (!stack->cards.count) return false;
            
            for (auto row = card->row_position; row < (stack->cards.count - 1); ++row)
            {
                auto upper = Down<Cluj_Card>(stack->cards[row]);
                auto lower = Down<Cluj_Card>(stack->cards[row + 1]);

                if (lower->card->is_inverted)       return false;
                if (upper->rank - lower->rank != 1) return false;
            }

            return true;
        } break;
        default: {
            assert(0);
            return false;
        }
    }
}

void init_cluj_manager(Entity_Manager *m)
{
    set_game_visuals(m, &game_visuals.default_game_visuals);

    m->init_textures        = init_textures;
    m->post_move_reevaluate = post_move_reevaluate;
    m->post_undo_reevaluate = post_undo_reevaluate;
    m->level_from_seed      = level_from_seed;

    m->is_done_with_dealing = is_done_with_dealing;

    m->draw = draw;

    m->can_move = can_move;
    m->can_drag = can_drag;
}
