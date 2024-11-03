#include "metadata.h"
#include "visit_struct.h"
#include "solitaire.h"
#include "table.h"

Metadata *base_card_metadata;
// i64 base_card_runtime_size; // @Cleanup: Maybe we can remove this too.

//
// @Important: This file should be the only place where we place all of the VISITABLE_STRUCT() declarations.
//

VISITABLE_STRUCT(Vector2, x, y);
VISITABLE_STRUCT(Vector3, x, y, z);
VISITABLE_STRUCT(Vector4, x, y, z, w);

VISITABLE_STRUCT(String, count, data);

VISITABLE_STRUCT(Rect, x, y, w, h);

// We don't need to store the Pid for the Card because
// when we diff the cards, we also put in the Pid if it is different.
//
// Also, we don't serialize the texture maps because when we have the Pid,
// we should be able to get the right card, given that we have the seed for the level.
//
// Therefore, we don't need to store the color either, because that can be traced back using Pid.
VISITABLE_STRUCT(Card,
                 hitbox,
                 z_layer,
                 in_stack_index,
                 is_inverted,
                 previous_stack_index,
                 row_position,
                 did_physical_move,
                 visual_position,
                 visual_move_type);

VISITABLE_STRUCT(Stack,
                 closed);

namespace vs = visit_struct;

//
// Functions below:
//

u8 *metadata_slot(Card *card, Metadata_Item *item)
{
    if (item->imported_from_base_entity)
    {
        auto p = reinterpret_cast<u8*>(card);
        return p + item->byte_offset;
    }
    else
    {
        auto p = reinterpret_cast<u8*>(card->derived_pointer);
        return p + item->byte_offset;
    }
}

// @Cleanup: Rename the derived_* variables, that is misleading
template <typename T>
void fill_metadata(T *instance, Metadata *m, bool from_base_entity, i64 offset_relative_to_initial_instance, bool is_top_level)
{
    u8 *cursor = reinterpret_cast<u8*>(instance);

    vs::for_each(*instance, [&](const char *field, auto &ref_value) {
        u8 *member_pointer     = reinterpret_cast<u8*>(&ref_value);
        i64 member_byte_offset = member_pointer - cursor;
        i64 actual_byte_offset = member_byte_offset + offset_relative_to_initial_instance;

        auto actual_value = ref_value;
        auto probe_type = _make_Type(decltype(actual_value));

        Metadata_Item::Item_Info info = Metadata_Item::UNKNOWN;

        // Tagging the info for the item
        // @Speed: I suppose it does not matter much because we only call this before the game loop.
        {
            auto name_of_type = String(probe_type.name());
            const String type_type_name("type_index");

            // Because these two structs are polymorphic structs, they need to be compared by names....
            const String dynamic_array_name("Resizable_Array");
            const String static_array_name("Static_Array");

            if (std::is_pod<decltype(actual_value)>())
            {
                info = Metadata_Item::POD;
            }
            else if (cmp_var_type_to_type(probe_type, String))
            {
                info = Metadata_Item::STRING;
            }
            else if (cmp_var_type_to_type(probe_type, Rect))
            {
                info = Metadata_Item::POD;
            }
            else if (contains(name_of_type, type_type_name))
            {
                info = Metadata_Item::TYPE;
                logprint("fill_metadata", "Adding type as a metadata item for struct '%s', field '%s' (not supposed to happen!)\n", _make_Type(*instance).name(), field);
            }
            else if (contains(name_of_type, dynamic_array_name))
            {
                // @Incomplete: Make sure the array only contains POD elements!
                info = Metadata_Item::DYNAMIC_ARRAY;

                // element_runtime_size = array_sizeof_my_elements(&derived_ref_value);
                // printf("Size of my element is: %ld\n", element_runtime_size);

                assert(0); // @Incomplete;
            }
            else if (contains(name_of_type, static_array_name))
            {
                // @Incomplete: Make sure the array only contains POD elements!
                info = Metadata_Item::STATIC_ARRAY;

                assert(0); // @Incomplete;
            }
            else
            {
                logprint("fill_metadata", "Tagging Metadata_Item::UNKNOWN as the type of the field for '%s'!\n", field);
                assert(0);
            }
        }

        bool is_leaf = true; // @Hack
        bool old_is_top_level = is_top_level; // @Hack

        if (cmp_var_type_to_type(probe_type, Card*))
        {
            // Skipping the base struct.
            is_top_level = false;
            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Vector2))
        {
            Vector2 v2_instance;
            fill_metadata(&v2_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Vector3))
        {
            Vector3 v3_instance;
            fill_metadata(&v3_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Vector4))
        {
            Vector4 v4_instance;
            fill_metadata(&v4_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }
        else if (cmp_var_type_to_type(probe_type, Rect))
        {
            Rect rect_instance;
            fill_metadata(&rect_instance, m, from_base_entity, actual_byte_offset, false);

            is_leaf = false;
        }

        // printf("[field]: leaves #%ld top levels #%ld, '%s', offsets '%ld', from base? %d\tsizeof = %ld\n",
        //        m->leaf_items.count, m->top_level_items.count, field, actual_byte_offset,
        //        from_base_entity, sizeof(actual_value));

        auto m_item = New<Metadata_Item>(false);

        m_item->metadata = m;
        m_item->name = String(field); // @Important: Apparently we don't need to do copy string here. So don't free the name.
        m_item->byte_offset = actual_byte_offset;
        m_item->info = info;

        m_item->runtime_size = sizeof(actual_value);
        m_item->imported_from_base_entity = from_base_entity;

        if (is_leaf)
        {
            // Add the item to the list of derived items in the metadata
            array_add(&m->leaf_items, m_item);
        }

        if (is_top_level)
        {
            // Add the item to the list of top level items in the metadata
            array_add(&m->top_level_items, m_item);
        }

        // @Hack:
        is_top_level = old_is_top_level;
    });
}

void init_metadata()
{
    // Metadata for the base card.
    {
        base_card_metadata = New<Metadata>(); // @Leak:
        base_card_metadata->entity_type = _make_Type(Card);

        Card dummy;
        fill_metadata(&dummy, base_card_metadata, true, 0, true);

//        auto size = sizeof(Card);
//        base_card_runtime_size = size;
    }
}
