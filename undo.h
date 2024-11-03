#pragma once

#include "common.h"
#include "table.h"

typedef u32 Pid;
struct Entity_Manager;
struct Card;
struct Stack;
struct String_Builder;

struct Undo_Record
{
    String transactions; // Things that changed in that frame.

    // @Incomplete: We may want level time if we want to playback the game. Could be a great idea.
};

struct Undo_Handler
{
    Entity_Manager *manager = NULL;
    RArr<Undo_Record*> undo_records;

    Table<Pid, Card*> cached_card_states;
    
    bool dirty   = false; // Used for save game and load game.
    bool enabled = false;
};

struct Pack_Info
{
    String_Builder *builder;
    u8 *pointer_to_slot_count = NULL;
    u8  slot_count = 0;
};

void undo_mark_beginning(Entity_Manager *manager);
void undo_end_frame(Undo_Handler *handler);
void do_one_undo(Entity_Manager *manager);
