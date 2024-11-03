//
// The undo system here is a stripped down version of the undo system in Sokoban.
// For more information, see the full implementation of it over in the Sokoban project.
//

// Use a pool for allocations here. The pool should be freed at the end of each level.

#include "undo.h"
#include "solitaire.h"
#include "metadata.h"
#include "string_builder.h"
#include "hud.h"
#include "pool.h"

static Pool undo_card_and_record_pool;
static bool undo_pool_initted = false;

template <typename T>
void copy_slot(T *src, T *dest, Metadata_Item *item)
{
    if (item->info == Metadata_Item::POD)
    {
        auto slot_src  = metadata_slot(src, item);
        auto slot_dest = metadata_slot(dest, item);
        memcpy(slot_dest, slot_src, item->runtime_size);
    }
    else if (item->info == Metadata_Item::STRING)
    {
        auto slot_src  = reinterpret_cast<String*>(metadata_slot(src, item));
        auto slot_dest = reinterpret_cast<String*>(metadata_slot(dest, item));

        if (slot_dest->count) free_string(slot_dest);
        *slot_dest = copy_string(*slot_src);
    }
    else if (item->info == Metadata_Item::STATIC_ARRAY)
    {
        // @Incomplete:
        assert(0);
    }
    else if (item->info == Metadata_Item::DYNAMIC_ARRAY)
    {
        // @Incomplete: Not tested.

        // This assumes the array contains POD elements.
        auto src_array  = reinterpret_cast<RArr<u8>*>(metadata_slot(src, item));
        auto dest_array = reinterpret_cast<RArr<u8>*>(metadata_slot(dest, item));

        array_reset(dest_array);

        // I guess the dest_array should have the same allocator?
        dest_array->allocator.proc = src_array->allocator.proc;
        dest_array->allocator.data = src_array->allocator.data;

        dest_array->allocated = 0;
        dest_array->count     = 0;

        array_reserve(dest_array, src_array->count);
        dest_array->count = src_array->count;

        memcpy(dest_array->data, src_array->data, src_array->count);
    }
    else if (item->info == Metadata_Item::TYPE)
    {
        logprint("copy_slot", "Not supposed to copy Metadata_Item::TYPE!\n");
        assert(0);
    }
    else
    {
        logprint("copy_slot", "In struct copying of field '%s' has unsupported type %d!\n", temp_c_string(item->name), item->info);
        assert(0);
    }
}

void copy_card_data(Card *src, Card *dest)
{
    assert(src->card_type == dest->card_type);
    assert(src->card_id == dest->card_id);

    for (auto item : base_card_metadata->top_level_items)
    {
        copy_slot(src, dest, item); // @Incomplete: :VersionHandling
    }
}

Card *clone_card_via_alloc(Card *card)
{
    auto result = reinterpret_cast<Card*>(get(&undo_card_and_record_pool, sizeof(Card)));
    result->card_type       = card->card_type;
    result->card_id         = card->card_id;
    result->derived_pointer = card->derived_pointer;

    copy_card_data(card, result);
    return result;
}

void undo_mark_beginning(Entity_Manager *manager)
{
    auto handler = manager->undo_handler;

    assert(handler);
    handler->enabled = true;
    handler->manager = manager;

    for (auto it : handler->undo_records)
    {
        if (it->transactions) free_string(&it->transactions);
    }

    if (!undo_pool_initted)
    {
        undo_pool_initted = true;

        set_allocators(&undo_card_and_record_pool);
        undo_card_and_record_pool.memblock_size = 100 * sizeof(Card);
    }
    else
    {
        release(&undo_card_and_record_pool);
    }

    array_reset(&handler->undo_records);
    table_reset(&handler->cached_card_states);

    for (auto it : manager->all_cards)
    {
        auto clone = clone_card_via_alloc(it);
        table_add(&handler->cached_card_states, it->card_id, clone);
    }
}

void increment_pack_count(Pack_Info *info, Card *card)
{
    if (info->slot_count == 0)
    {
        //
        // If we are writing the first metadata item, we also write before it the Pid of the card
        // and the slot count (how many things changed for this card).
        //
        put(info->builder, static_cast<u32>(card->card_id)); // Storing the Pid of the card.

        auto success = ensure_contiguous_space(info->builder, 1);
        assert(success);

        info->pointer_to_slot_count = get_cursor(info->builder);
        put(info->builder, static_cast<u8>(0)); // Placeholder for the slot count to be overwritten in the future.
    }

    info->slot_count += 1;
}

void compare_metadata_items(Card *card_old, Card *card_new, Metadata_Item *item, i64 index, Pack_Info *info)
{
    auto slot_old = metadata_slot(card_old, item);
    auto slot_new = metadata_slot(card_new, item);

    auto size = item->runtime_size;
    auto differing = memcmp(slot_old, slot_new, size);
    
    if (differing)
    {
        auto builder = info->builder;

        if (item->info == Metadata_Item::POD)
        {
            // printf("Field '%s' with byte offset %ld is different!\n", temp_c_string(item->name), item->byte_offset);

            increment_pack_count(info, card_old);

            // @Incomplete: :VersionHandling for forward/backward compability when we load
            // a save game from an older version. We need to know which field of the old version
            // maps to the field in the new version. (Using Type_Manifest)
            //
            // We'll do the this when we start doing revision number for our entities.

            u8 field_index = static_cast<u8>(index);
            put(builder, field_index);

            put_n_bytes_with_endian_swap(builder, slot_old, slot_new, size);
        }
        else if (item->info == Metadata_Item::STRING)
        {
            // For the case of strings, the memcmp above was a conservative detection.
            // If the strings are byte-equal, then they are obviously equal. But if
            // they're not, they could point to two strings of the same length but
            // with differing contents. That is what we check here.

            auto s_old = reinterpret_cast<String*>(slot_old);
            auto s_new = reinterpret_cast<String*>(slot_new);
 
            if (*s_old != *s_new)
            {
                // printf("Field '%s' with byte offset %ld is different!\n", temp_c_string(item->name), item->byte_offset);

                increment_pack_count(info, card_old);

                //
                // @Incomplete: Check above for :VersionHandling.
                //

                u8 field_index = static_cast<u8>(index);
                put(builder, field_index);


                put(builder, *s_old);
                put(builder, *s_new);
            }
        }
        else
        {
            logprint("compare_metadats_items", "Field '%s' has unimplemented type info %d!\n", temp_c_string(item->name), item->info);
            assert(0);
        }
    }
}

void diff_card(Card *card_old, Card *card_new, Pack_Info *pack_info)
{
    assert(card_old->card_type == card_new->card_type);

    auto metadata = base_card_metadata;
    assert((metadata != NULL));

    auto num_fields = metadata->leaf_items.count;
    // Because visit_struct limits to 69
    assert((num_fields <= 69));

    i64 item_index = 0;
    for (auto item : metadata->leaf_items)
    {
        compare_metadata_items(card_old, card_new, item, item_index, pack_info);
        item_index += 1;
    }

    if (pack_info->slot_count)
    {
        assert(pack_info->pointer_to_slot_count);
        *pack_info->pointer_to_slot_count = pack_info->slot_count;
    }
}

void scan_one_entity(Undo_Handler *handler, Card *card, String_Builder *builder, i64 *counter)
{
    assert(card->visual_start_time < 0);

    auto card_old_ptr = table_find_pointer(&handler->cached_card_states, card->card_id);
    assert(card_old_ptr);

    Pack_Info pack_info;
    pack_info.builder = builder;

    auto card_old = *card_old_ptr;
    diff_card(card_old, card, &pack_info);

    if (pack_info.slot_count)
    {
        *counter += 1;
        *card_old_ptr = clone_card_via_alloc(card);
    }
}

void scan_for_changed_entities(Undo_Handler *handler, String_Builder *builder)
{
    auto manager = handler->manager;
    assert(manager);

    // @Cleanup: Change this variable from i16 -> u16.
    i64 counter = 0; // @Important: Expected to max at u16

    auto success = ensure_contiguous_space(builder, 2);
    assert(success);

    auto entity_count_cursor = get_cursor(builder);
    put(builder, static_cast<u16>(0)); // This is a placeholder for the future entity count.

    // printf(">> Before scanning:\n");

    // @Fixme: If for some reason scan_one_entity does not add detect any changes we are still
    // return a 3 bytes long string because of the above 2 puts. We may need to perform a check here
    // to see if the scan_one_entity did anything or not and return an empty string.
    // Maybe not! And we could just selectively choose where to call undo_end_frame.
    for (auto it : manager->all_cards)
    {
        // printf(" ! Scanning card with Pid %u\n", it->card_id);
        scan_one_entity(handler, it, builder, &counter);
    }

    // printf(">> Number of cards changed in this record is %ld\n", counter);
    // newline();
    // newline();
    // newline();
    // newline();

    auto lo = static_cast<u8>(counter & 0xff);
    auto hi = static_cast<u8>((counter >> 8) & 0xff);

    // Because our platform is little endian, we need to swap the bytes of the 'counter'.
    entity_count_cursor[0] = lo;
    entity_count_cursor[1] = hi;
}

void undo_end_frame(Undo_Handler *handler)
{
    if (!handler->enabled) return;

    String_Builder builder;
    scan_for_changed_entities(handler, &builder);

    auto s = builder_to_string(&builder);
    handler->dirty = true;

    auto record = reinterpret_cast<Undo_Record*>(get(&undo_card_and_record_pool, sizeof(Undo_Record)));
    record->transactions = s;

    array_add(&handler->undo_records, record);
}

Card *find_card(Entity_Manager *manager, Pid id)
{
    for (auto it : manager->all_cards) if (it->card_id == id) return it;
    return NULL;
}

// @Speed: Make the apply_diff 2 different procedures because we don't want to keep checking for apply_forward
void apply_diff(Card *card_dest, u8 num_slots_changed, String *transaction, bool apply_forward)
{
    for (i64 i = 0; i < num_slots_changed; ++i)
    {
        //
        // @Incomplete :VersionHandling Look this up in the Type_Manifest.
        //

        u8 field_index;
        get(transaction, &field_index);

        auto metadata = base_card_metadata;
        auto item = metadata->leaf_items[field_index];

        auto slot_dest = metadata_slot(card_dest, item);
        if (item->info == Metadata_Item::POD)
        {
            auto size = item->runtime_size;

            if (apply_forward)
            {
                advance(transaction, size);                 // Discard old value.
                memcpy(slot_dest, transaction->data, size); // Apply new value
                advance(transaction, size);                 // Advance past new value.
            }
            else
            {
                memcpy(slot_dest, transaction->data, size); // Apply old value
                advance(transaction, size);                 // Advance past old value.
                advance(transaction, size);                 // Discard new value.
            }
        }
        else if (item->info == Metadata_Item::STRING)
        {
            auto string_dest = reinterpret_cast<String*>(slot_dest);

            if (apply_forward)
            {
                discard_string(transaction);
                extract_string(transaction, string_dest);
            }
            else
            {
                extract_string(transaction, string_dest);
                discard_string(transaction);
            }
        }
        else
        {
            logprint("apply_diff", "Unhandled metadata info item type %d\n", item->info);
            assert(0);
        }
    }
}

template <typename V>
void array_add_at_index(RArr<V> *array, V value, i64 index)
{
    if (!array->count)
    {
        array_add(array, value);
    }
    else
    {
        if (array->count <= index) array_resize(array, index + 1);
        (*array)[index] = value;
    }
}

void revert_card_to_stack(Entity_Manager *manager, Card *card, i64 old_stack_index, i64 old_row_position)
{
    if ((card->in_stack_index == old_stack_index) && (card->row_position == old_row_position)) return;

    assert(manager == card->entity_manager);
    auto stack = get_stack(card);

    array_add_at_index(&stack->cards, card, card->row_position);
    assert(stack->stack_type != Stack_Type::DRAGGING);

    auto old_stack = manager->all_stacks[old_stack_index];
    assert(old_stack->stack_index == old_stack_index);

    // We can do pop without worrying about which array index to remove because in the case of
    // dragon merging or stuff like that, we rather not care about which array order to remove first,
    // because in the end we would nuke the stack anyways.
    assert(old_stack->stack_type != Stack_Type::DRAGGING);
    pop(&old_stack->cards);
}

void really_do_one_undo(Undo_Handler *handler, Undo_Record *record, bool is_redo)
{
    auto remaining = record->transactions;
    while (remaining)
    {
        u16 num_cards_changed;
        get(&remaining, &num_cards_changed);

        for (i64 i = 0; i < num_cards_changed; ++i)
        {
            u32 card_id;
            get(&remaining, &card_id);

            u8 num_slots_changed;
            get(&remaining, &num_slots_changed);

            // @Incomplete :VersionHandling Look this up in the Type_Manifest.

            auto manager = handler->manager;
            auto card_dest = find_card(manager, static_cast<Pid>(card_id));
            assert(card_dest);

            auto [cached_card, found] = table_find(&handler->cached_card_states, static_cast<Pid>(card_id));
            assert(found);

            // Roll back the cached version of the card to store an older version of the card.
            apply_diff(cached_card, num_slots_changed, &remaining, is_redo);

            auto old_stack_index = card_dest->in_stack_index; // Save these old values because copy_card_data modifies it.
            auto old_row_position = card_dest->row_position;
                
            copy_card_data(cached_card, card_dest);

            reset_visual_interpolation(card_dest);
            revert_card_to_stack(manager, card_dest, old_stack_index, old_row_position);
        }
    }
}

void do_one_undo(Entity_Manager *manager)
{
    auto handler = manager->undo_handler;

    // Checking and performing one undo.
    if (!handler->undo_records.count) return;

    auto record = pop(&handler->undo_records);
    really_do_one_undo(handler, record, false);
}
