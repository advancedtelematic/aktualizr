/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


#ifndef _RVI_LIST_H_
#define _RVI_LIST_H_

typedef struct TRviListEntry
{
    struct TRviListEntry* next;
    void*                  pointer;

}   TRviListEntry;


typedef struct TRviList
{
    TRviListEntry* listHead;
    TRviListEntry* listTail;

    unsigned int    count;

}   TRviList;


int rviListInitialize ( TRviList* list );

int rviListInsert ( TRviList* list, void* record );

int rviListRemove ( TRviList* list, void* record );

int rviListRemoveHead ( TRviList* list, void** record );

static inline unsigned int rviListGetCount ( TRviList* list )
{
    return list->count;
}


#endif // _RVI_LIST_H_
