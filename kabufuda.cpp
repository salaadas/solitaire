// @Incomplete: Add difficulty panel chooser.

//
// The cards in Kabufuda don't have a general white front map that can be renderred to.
// Instead, each of the card contains a different front map depending on their rank.
//

#include "kabufuda.h"
#include "solitaire.h"
#include "main.h"
#include "opengl.h"
#include "draw.h"
#include "time_info.h"

static constexpr Rank MIN_RANK = 1;
static constexpr Rank MAX_RANK = 10;
static constexpr Rank WINDMILL_RANK = 4;

static constexpr auto CARDS_PER_RANK = 4;
static constexpr auto MAX_STARTING_CARDS_PER_STACK_COUNT = 5;

static Kabufuda_Game_Visuals game_visuals;

static Texture_Map *board_map;
static Texture_Map *card_back_map;
static Texture_Map *card_shadow_map;
static Texture_Map *card_stack_map;

// Out of the 4 windmills, one is a special one.
static Texture_Map *special_windmill_map;

static RArr<Texture_Map*> main_front_maps;
static SArr<Stack*> top_pile;

static Texture_Map *win_banner_left_map;
static Texture_Map *win_banner_right_map;
static Texture_Map *win_stamp_map;

static Texture_Map *unify_slot_overlay;
static Texture_Map *unify_icon_map;

static f32 win_banner_elapsed = 0;
static f32 win_stamp_elapsed  = 0;

// For Kabufuda, color doesn't matter, rather it's the rank that determine which card can
// go on to which, because of this, we set everyone to WHITE.
static void Make(Entity_Manager *manager, Rank rank, Texture_Map *front_map)
{
    assert((MIN_RANK <= rank) && (rank <= MAX_RANK));

    auto card = New<Number>();
    auto base = register_general_card(manager, _make_Type(Number), card);

    card->card = base;

    base->front_map  = front_map;
    base->back_map   = card_back_map;
    base->shadow_map = card_shadow_map;

    card->rank  = rank;
    base->color = Color::WHITE; // Color doesn't matter in Kabufuda.
}

static void init_textures(Entity_Manager *manager)
{
    board_map = load_linear_mipmapped_texture(String("kabufuda_board"));

    // @Incomplete: We want better screen adjustments later
    {
        auto board_size = Vector2(board_map->width, board_map->height) * KABUFUDA_GAME_SCALE;
        set_game_window_properties(manager, board_size);
    }

    //
    // Override the card size in the Entity Manager with the scaled one.
    //
    auto card_front_map = load_linear_mipmapped_texture(String("kabufuda_windmill"));
    auto card_size = Vector2(card_front_map->width, card_front_map->height) * KABUFUDA_GAME_SCALE;
        
    set_general_card_size(manager, card_size.x, card_size.y);

    card_back_map   = load_linear_mipmapped_texture(String("kabufuda_card_back"));
    card_shadow_map = load_linear_mipmapped_texture(String("kabufuda_card_shadow"));
    card_stack_map  = load_linear_mipmapped_texture(String("kabufuda_card_stack"));

    array_reserve(&main_front_maps, MAX_RANK - MIN_RANK + 1);
    auto add_front_map = [](RArr<Texture_Map*> *array, String image_name) {
        auto map = load_linear_mipmapped_texture(image_name);
        array_add(array, map);
    };

    add_front_map(&main_front_maps, String("kabufuda_red_mushroom"));
    add_front_map(&main_front_maps, String("kabufuda_two_legged"));
    add_front_map(&main_front_maps, String("kabufuda_three_legged"));
    add_front_map(&main_front_maps, String("kabufuda_windmill"));
    add_front_map(&main_front_maps, String("kabufuda_tent"));
    add_front_map(&main_front_maps, String("kabufuda_star"));
    add_front_map(&main_front_maps, String("kabufuda_t"));
    add_front_map(&main_front_maps, String("kabufuda_cockroach"));
    add_front_map(&main_front_maps, String("kabufuda_spark"));
    add_front_map(&main_front_maps, String("kabufuda_columbus"));

    special_windmill_map = load_linear_mipmapped_texture(String("kabufuda_special_windmill"));

    win_banner_left_map  = load_linear_mipmapped_texture(String("kabufuda_you"));
    win_banner_right_map = load_linear_mipmapped_texture(String("kabufuda_win"));
    win_stamp_map        = load_linear_mipmapped_texture(String("kabufuda_you_win_middle"));

    unify_slot_overlay = load_linear_mipmapped_texture(String("kabufuda_unify_slot_overlay"));
    unify_icon_map     = load_linear_mipmapped_texture(String("kabufuda_unify_icon"));
}

static bool check_for_victory(Entity_Manager *manager)
{
    if (manager->moving_cards.count) return false;
    
    for (auto it : top_pile)           if (it->cards.count && !it->closed) return false;
    for (auto it : manager->base_pile) if (it->cards.count && !it->closed) return false;

    return true;
}

static void do_victory_animation_set_up(Entity_Manager *manager)
{
    manager->win_count += 1;
    manager->in_game_state = In_Game_State::END_GAME;

    win_banner_elapsed = 0;
    win_stamp_elapsed  = 0;
}

static void post_move_reevaluate(Entity_Manager *manager)
{
    if (manager->in_game_state != In_Game_State::PLAYING) return;
    if (manager->dragging_stack->cards.count) return; // Don't do automove while we are dragging stuff.
    if (manager->moving_cards.count) return; // There are still moving stuff, so don't reeval.

    // @Speed: Realistically we only need this extra check while we are in DEVELOPER mode,
    // but a loop of range 4 isn't that much to worry about.
    auto should_close_stack = [](Stack *stack) -> bool {
        if (stack->cards.count != CARDS_PER_RANK) return false;

        auto should_close = true;

        for (auto it_index = 0; it_index < stack->cards.count-1; ++it_index)
        {
            auto upper = Down<Number>(stack->cards[it_index]);
            auto lower = Down<Number>(stack->cards[it_index + 1]);

            if (upper->rank != lower->rank)
            {
                should_close = false;
                break;
            }
        }

        return should_close;
    };

    // Check all the stacks in the top pile and the base pile, if there is any stack with
    // count == 'CARDS_PER_RANK', we check loop through and check the rank, if the ranks are
    // the same, we close the stack.
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

    for (auto stack : top_pile)
    {
        stack->closed = should_close_stack(stack);
    }

    auto victory = check_for_victory(manager);
    if (victory)
    {
        do_victory_animation_set_up(manager);
    }
}

bool should_unify(Stack *from_stack, Stack *to_stack)
{
    if (!from_stack->cards.count) return false;

    if (to_stack->cards.count)
    {
        auto first_card_to_move = Down<Number>(from_stack->cards[0]);

        auto same_count = 0;
        for (auto it : to_stack->cards)
        {
            auto n = Down<Number>(it);

            if (n->rank != first_card_to_move->rank) return false;
            else same_count += 1;
        }

        if (same_count + from_stack->cards.count != CARDS_PER_RANK) return false;
    }
    else
    {
        if (from_stack->cards.count != CARDS_PER_RANK) return false;
    }

    return true;
}

static void post_undo_reevaluate(Entity_Manager *manager)
{
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

    // Top pile.
    array_resize(&top_pile, KABUFUDA_TOP_PILE_COLUMNS_COUNT);
    for (i64 it_index = 0; it_index < KABUFUDA_TOP_PILE_COLUMNS_COUNT; ++it_index)
    {
        auto release_region = get_rect(gv->top_pile_x[it_index], gv->top_pile_y, w, h);
        top_pile[it_index] = create_stack(manager, Stack_Type::TOP, gv->top_pile_z_layer_offset, release_region);
    }

    // Base pile.
    array_resize(&manager->base_pile, KABUFUDA_BASE_COLUMNS_COUNT);
    for (auto it_index = 0; it_index < KABUFUDA_BASE_COLUMNS_COUNT; ++it_index)
    {
        // The release region for the base pile extends far down.
        auto release_region = get_rect(gv->base_pile_x[it_index], gv->base_pile_y, w, -1 * gv->base_pile_y);
        auto stack = create_stack(manager, Stack_Type::BASE, gv->base_z_layer_offset, release_region);

        manager->base_pile[it_index] = stack;
        array_reserve(&stack->cards, MAX_STARTING_CARDS_PER_STACK_COUNT);
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

    array_reserve(&manager->all_cards, CARDS_PER_RANK * (MAX_RANK - MIN_RANK + 1));
    array_reset(&manager->all_cards); // @Cleaup: Redundant with cleanup_level?

    for (auto rank = MIN_RANK; rank <= MAX_RANK; ++rank)
    {
        auto map = main_front_maps[rank - MIN_RANK];

        Make(manager, rank, map);
        Make(manager, rank, map);
        Make(manager, rank, map);

        // Make it a little bit special for the windmill.
        if (rank == WINDMILL_RANK) Make(manager, rank, special_windmill_map);
        else                       Make(manager, rank, map);
    }

    if constexpr (!DEBUG_NO_SHUFFLE)
    {
        shuffle(&manager->all_cards);
    }

    assert(top_pile.count);
    auto deal_stack = top_pile[0];
    set_dealing_stack(manager, deal_stack);

    for (auto it_index = 0; it_index < manager->all_cards.count; ++it_index)
    {
        auto it = manager->all_cards[it_index];

        set_stack(it, deal_stack);

        it->row_position    = it_index;
        it->z_layer         = deal_stack->z_offset + it->row_position;
        it->visual_position = deal_stack->visual_start;

        array_add(&deal_stack->cards, it);
    }

    auto gv = &game_visuals;

    auto it_index = deal_stack->cards.count - 1;
    for (auto row = 0; row < MAX_STARTING_CARDS_PER_STACK_COUNT; ++row)
    {
        for (auto column = 0; column < manager->base_pile.count; ++column)
        {
            auto card = deal_stack->cards[it_index];
            auto destination_stack = manager->base_pile[column];

            deal_one_card_with_offset(card, destination_stack, row);
            it_index -= 1;
        }
    }
}

static bool is_done_with_dealing(Entity_Manager *manager)
{
    assert(top_pile.count);
    auto deal_stack = top_pile[0];

    // Once the deal stack is empty, we know we have dealt out everything.
    if (deal_stack->cards.count) return false;

    // Double check to see if there is anything still moving.
    if (manager->moving_cards.count) return false;

    return true;
}

static bool should_render_card_stack_texture(Stack *stack)
{
    // Only render the stack if the count is > 1, because when the count is 1, we don't see it anyways.
    return (stack->cards.count > 1) && (stack->closed || (stack->stack_type != Stack_Type::BASE && stack->stack_type != Stack_Type::DRAGGING));
}

static void render_card_from_top_left(Card *card)
{
    set_shader(shader_argb_and_texture);

    auto manager = card->entity_manager;
    auto in_stack = get_stack(card);

    auto gv   = &game_visuals;
    auto vpos = card->visual_position;

    assert(manager->card_size_is_set);
    auto cw = manager->card_size.x;
    auto ch = manager->card_size.y;

    auto card_center   = vpos + Vector2(cw * .5f, -ch * .5f);
    auto shadow_width  = card->shadow_map->width * KABUFUDA_GAME_SCALE;
    auto shadow_height = card->shadow_map->height * KABUFUDA_GAME_SCALE;

    if (should_render_card_stack_texture(in_stack))
    {
        auto y_offset = (in_stack->cards.count - 1) * gv->card_fixed_pixel_offset_amount;

        if (card->row_position == 0)
        {
            // Shadow for the stack base only.
            set_texture(String("diffuse_texture"), card->shadow_map);
            immediate_image(card_center, Vector2(shadow_width, shadow_height), Vector4(1, 1, 1, 1), 0, false, card->z_layer);

            // Stack.
            auto sp0 = vpos + Vector2(0, -ch);
            auto sp1 = sp0  + Vector2(cw,  0);

            auto csh = card_stack_map->height * KABUFUDA_GAME_SCALE;
            auto sp2 = sp1 + Vector2(0, csh);
            auto sp3 = sp0 + Vector2(0, csh);

            set_texture(String("diffuse_texture"), card_stack_map);
            immediate_quad(sp0, sp1, sp2, sp3, 0xffffffff, card->z_layer);
        }

        vpos.y += y_offset;
    }
    else
    {
        // Shadow for everyone.
        set_texture(String("diffuse_texture"), card->shadow_map);
        immediate_image(card_center, Vector2(shadow_width, shadow_height), Vector4(1, 1, 1, 1), 0, false, card->z_layer);
    }

    Texture_Map *card_map = NULL;
    if (in_stack->closed) card_map = card->back_map;
    else                  card_map = card->front_map;

    assert(card_map);
    set_texture(String("diffuse_texture"), card_map);
    immediate_quad_from_top_left(vpos, manager->card_size, Vector4(1, 1, 1, 1), card->z_layer);

    if (DEBUG_HITBOX)
    {
        set_shader(shader_argb_no_texture);

        Vector2 p0, p1, p2, p3;
        get_quad(card->hitbox, &p0, &p1, &p2, &p3);

        auto color = argb_color(Vector4(0, 1, 1, .4f));
        immediate_quad(p0, p1, p2, p3, color, card->z_layer);
    }

    immediate_flush();
}

static void render_unify_drop_spot(Entity_Manager *manager, Stack *stack)
{
    set_shader(shader_argb_and_texture);
    set_texture(String("diffuse_texture"), unify_slot_overlay);

    auto gv = &game_visuals;

    auto z_offset = gv->unify_z_layer_offset;

    auto region_size = manager->card_size;
    
    f32 size_offset = 0;
    if (stack->cards.count && stack->stack_type == Stack_Type::BASE)
    {
        size_offset = (stack->cards.count - 1) * gv->default_game_visuals.default_card_row_offset * manager->card_size.y;
        
        region_size.y += size_offset;
    }

    assert(manager->card_size_is_set);
    immediate_quad_from_top_left(stack->visual_start, region_size, Vector4(1, 1, 1, 1), z_offset);

    // @Incomplete: When we do export difficulty, we want different icon for the base pile vs the top
    // pile.
    auto center = stack->visual_start;
    center.y -= size_offset;

    center.x += manager->card_size.x * .5f;
    center.y -= manager->card_size.y * .5f;

    auto icon_size = Vector2(unify_icon_map->width, unify_icon_map->height) * KABUFUDA_GAME_SCALE;

    set_texture(String("diffuse_texture"), unify_icon_map);
    immediate_image(center, icon_size, Vector4(1, 1, 1, 1), 0, false, z_offset);

    immediate_flush();
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

    if constexpr (DEBUG_RELEASE_REGION)
    {
        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), card_back_map);

        for (auto it : top_pile)
        {
            immediate_quad_from_top_left(it->visual_start, size_of_rect(it->release_region), Vector4(1, .3, .8, .4));
        }

        for (auto it : manager->base_pile)
        {
            immediate_quad_from_top_left(it->visual_start, size_of_rect(it->release_region), Vector4(1, .3, 0, .4));
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

    for (auto stack : top_pile)
    {
        for (auto card : stack->cards)
        {
            // Only render non-moving cards in the base pile in this loop.
            if (card->visual_start_time < 0) render_card_from_top_left(card);
        }
    }

    //
    // Rendering the unify drop spot.
    // Since this is static, it needs to be renderred before the animating cards.
    //
    for (auto stack : manager->base_pile)
    {
        if (should_unify(manager->dragging_stack, stack))
        {
            render_unify_drop_spot(manager, stack);
        }
    }

    for (auto stack : top_pile)
    {
        if (should_unify(manager->dragging_stack, stack))
        {
            render_unify_drop_spot(manager, stack);
        }
    }

    // Then draw all the rest of the static cards. After that is done, we then draw the animating cards.
    // This is because the animtating cards have transparency and so we must draw them after we have the whole scene.
    //
    // Also, we need to sort the animating cards based on their z_layer.
    //
    // @Cleanup: We should consider rendering things separately....
    RArr<Card*> animating_cards;
    animating_cards.allocator = {global_context.temporary_storage, __temporary_allocator};

    array_reserve(&animating_cards, manager->all_cards.count); // @Bug @Fixme: If we don't do a array reserve here, we will encounter some nasty problems. (We also have problems if we reserve it with the size of moving_cards.count?????

    for (auto it : manager->all_cards)
    {
        auto stack = get_stack(it);
//        if ((stack->stack_type != Stack_Type::BASE) || ((it->visual_start_time >= 0)))
        if ((stack->stack_type == Stack_Type::DRAGGING) || it->visual_start_time >= 0)
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
        auto gv = &game_visuals;
        auto duration = gv->win_banner_duration;

        if (win_banner_elapsed < duration)
        {
            win_banner_elapsed += timez.current_dt;
            if (win_banner_elapsed > duration) win_banner_elapsed = duration;
        }

        shader_argb_and_texture->diffuse_texture_wraps = false;
        defer {shader_argb_and_texture->diffuse_texture_wraps = true;};

        set_shader(shader_argb_and_texture);
        set_texture(String("diffuse_texture"), win_banner_left_map);

        auto window_width = manager->game_window_size.x;
        auto w = window_width * .5f;
        auto h = win_banner_left_map->height * KABUFUDA_GAME_SCALE;

        if (duration == 0) duration = 1; // @Robustness: We need epsilon.
        auto banner_t = win_banner_elapsed / duration;

        //
        // The left part.
        //
        auto x_offset = lerp(-w, 0, banner_t);

        auto p3 = Vector2(x_offset, gv->win_banner_y);
        auto p0 = p3 - Vector2(0, h);
        auto p1 = p0 + Vector2(w, 0);
        auto p2 = p3 + Vector2(w, 0);

        f32 denom = win_banner_left_map->width * KABUFUDA_GAME_SCALE;
        if (!denom) denom = 1;
        auto ux_start = 1 - w / denom;

        auto u0 = Vector2(ux_start, 0);
        auto u1 = Vector2(1,        0);
        auto u2 = Vector2(1,        1);
        auto u3 = Vector2(ux_start, 1);
        
        immediate_quad(p0, p1, p2, p3, u0, u1, u2, u3, 0xffffffff); // @Incomplete: Do we want z layer for this?

        //
        // The right part.
        //
        p3.x = window_width - w - x_offset;
        p0 = p3 - Vector2(0, h);
        p1 = p0 + Vector2(w, 0);
        p2 = p3 + Vector2(w, 0);

        u0 = Vector2(0,            0);
        u1 = Vector2(1 - ux_start, 0);
        u2 = Vector2(1 - ux_start, 1);
        u3 = Vector2(0,            1);

        set_texture(String("diffuse_texture"), win_banner_right_map);
        immediate_quad(p0, p1, p2, p3, u0, u1, u2, u3, 0xffffffff); // @Incomplete: Do we want z layer for this?

        //
        // The middle part.
        //
        if (banner_t > .9f) // @Hardcoded:
        {
            auto stamp_duration = gv->win_stamp_duration;
            if (win_stamp_elapsed < stamp_duration)
            {
                win_stamp_elapsed += timez.current_dt;
                if (win_stamp_elapsed > stamp_duration) win_stamp_elapsed = stamp_duration;
            }

            set_texture(String("diffuse_texture"), win_stamp_map);

            auto denom = stamp_duration;
            if (!denom) denom = 1;
            auto stamp_t = win_stamp_elapsed / denom;

            auto stamp_y = lerp(gv->win_stamp_start_y, gv->win_stamp_end_y, stamp_t);
            auto stamp_center = Vector2(window_width * .5f, stamp_y);

            auto alpha = lerp(gv->win_stamp_start_alpha, 1.f, stamp_t);

            auto size = Vector2(win_stamp_map->width, win_stamp_map->height) * KABUFUDA_GAME_SCALE;
            size *= lerp(gv->win_stamp_start_scale, 1.f, stamp_t);

            immediate_image(stamp_center, size, Vector4(1, 1, 1, alpha), 0, false, 0); // @Incomplete: Z-layer?
        }
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
            if (destination_stack->cards.count > 0)
            {
                auto last_card_in_destination = Down<Number>(array_peek_last(&destination_stack->cards));
                auto first_card_to_move       = Down<Number>(stack_to_move->cards[0]);

                if (last_card_in_destination->rank != first_card_to_move->rank) break;
            }

            return destination_stack;
        } break;        
        case Stack_Type::TOP: {
            if (!should_unify(stack_to_move, destination_stack))
            {
                if (!destination_stack->cards.count && stack_to_move->cards.count > 1)
                {
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
            return stack->cards.count == 1;
        } break;
        case Stack_Type::BASE: {
            if (!stack->cards.count) return false;

            for (auto row = card->row_position; row < (stack->cards.count - 1); ++row)
            {
                auto upper = Down<Number>(stack->cards[row]);
                auto lower = Down<Number>(stack->cards[row + 1]);

                if (upper->rank != lower->rank) return false;
            }

            return true;
        } break;
        default: {
            assert(0);
            return false;
        }
    }
}

void init_kabufuda_manager(Entity_Manager *m)
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
