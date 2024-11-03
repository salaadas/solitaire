#pragma once

#include "common.h"

struct Metadata_Item;

struct Metadata
{
    _type_Type entity_type = _make_Type(Metadata);

    RArr<Metadata_Item*> leaf_items;
    RArr<Metadata_Item*> top_level_items;
};

struct Metadata_Item
{
    Metadata *metadata;
    i64 byte_offset  = 0;
    i64 runtime_size = 0;

    // // Only used with array Metadata_Item
    // i64 element_runtime_size;

    String name; // Dynamically allocated for now.
    bool imported_from_base_entity = false; // Whether this field is from the base entity or from the derived.

    enum Item_Info : u8
    {
        UNKNOWN = 0,
        POD,
        STRING,
        TYPE,
        PID,              // Not handled, right now, Pid is considered as POD too.
        DYNAMIC_ARRAY,    // Not handled
        STATIC_ARRAY,     // Not handled
        COUNT
    };
    Item_Info info = UNKNOWN;
};

extern Metadata *base_card_metadata;

void init_metadata();

struct Card;
u8 *metadata_slot(Card *card, Metadata_Item *item);


