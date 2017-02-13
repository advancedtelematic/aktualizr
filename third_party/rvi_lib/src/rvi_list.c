/*
    Copyright (C) 2016, Jaguar Land Rover. All Rights Reserved.

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this file,
    You can obtain one at http://mozilla.org/MPL/2.0/.
*/


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rvi_list.h"


/*!-----------------------------------------------------------------------

    r v i _ l i s t _ i n i t i a l i z e

	@brief Initialize a new list data structure.

	This function will initialize a new list data structure by initializing
    all of the fields in the structure. 

    This function should be called when a new list is created before any data
    is inserted into the list.

	@param[in] list - The address of the list structure to initialize

	@return status - 0: Success
                    ~0: An error code

------------------------------------------------------------------------*/
int rviListInitialize ( TRviList* list )
{
    int status = 0;

    list->listHead = NULL;
    list->listTail = NULL;

    list->count    = 0;

    return status;
}


/*!-----------------------------------------------------------------------

    r v i _ l i s t _ i n s e r t

	@brief Insert a new data record into the specified list.

	This function will create a new list entry record, initialize it, put the
    specified pointer into it, and then insert this new record at the tail of
    the specified list.

	@param[in] list - The address of the list to insert into
	@param[in] record - The address of the data record to insert into the list.

	@return status - 0: Success
                    ~0: An error code

------------------------------------------------------------------------*/
int rviListInsert ( TRviList* list, void* record )
{
    TRviListEntry* newEntry;
    int status = 0;

    //
    //  Create a new list entry object.
    //
    newEntry = malloc ( sizeof(TRviListEntry) );
    if ( !newEntry )
    {
        return -ENOMEM;
    }
    //
    //  Initialize the fields in the new list entry structure.
    //
    newEntry->next    = NULL;
    newEntry->pointer = record;

    //
    //  Increment the list count for this new entry.
    //
    list->count++;

    //
    //  If this list is currently empty, initialize the head and tail pointers
    //  with the new entry.
    //
    if ( list->listHead == NULL )
    {
        list->listHead = newEntry;
        list->listTail = newEntry;
    }
    //
    //  If this list is not empty, add this new entry to the end of the list.
    //
    else
    {
        list->listTail->next = newEntry;
        list->listTail       = newEntry;
    }

    //
    //  Return the completion code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    r v i _ l i s t _ r e m o v e

	@brief Remove the specified record from the specified list

	This function will find the specifield data record in the specified list,
    remove that record from the list and update the list count.

	@param[in] list - The address of the list to insert into
	@param[in] record - The address of the data record to be removed from the
                        list.

	@return status - 0: Success
                    ~0: An error code

------------------------------------------------------------------------*/
int rviListRemove ( TRviList* list, void* record )
{
    TRviListEntry* current  = list->listHead;
    TRviListEntry* previous = NULL;
    int             status   = 0;

    //
    //  While we have not reached the end of the list...
    //
    while ( current != NULL )
    {
        //
        //  If this record matches the one the user asked us to remove...
        //
        if ( current->pointer == record )
        {
            //
            //  If this is not the first record in the list...
            //
            if ( previous != NULL )
            {
                //
                //  Remove this record from the list by rechaining the linked
                //  list around this record.
                //
                previous->next = current->next;
            }
            //
            //  If this is the first record in the list...
            //
            else
            {
                //
                //  Make the list head point to the next record.
                //
                list->listHead = current->next;
            }
            //
            //  In either case, decrement the count of the number of records
            //  in this list and free the list entry record we just removed.
            //
            --list->count;
            free ( current );
        }
        //
        //  If the current record isn't the record we are looking for, move on
        //  to the next record in the list.
        //
        previous = current;
        current  = current->next;
    }
    //
    //  If we did not find the specified record in the list, return an error
    //  code to the caller.
    //
    if ( current == NULL )
    {
        status = -ENOENT;
    }
    //
    //  Return the completion code to the caller.
    //
    return status;
}


/*!-----------------------------------------------------------------------

    r v i _ l i s t _ r e m o v e _ h e a d

	@brief Remove a data record from the specified list.

    This function will find the specified data record in the specified list
    and remove that record from the list.  The removed data pointer will be
    returned to the caller in the "record" argument supplied by the caller.

    Note that if the specified list is empty then a NULL pointer will be
    returned to the caller.

	@param[in] list - The address of the list to search
	@param[in] record - The address of where to store the removed record

	@return status - 0: Success
                    ~0: An error code

------------------------------------------------------------------------*/
int rviListRemoveHead ( TRviList* list, void** record )
{
    TRviListEntry* head;
    int             status = 0;

    //
    //  Return the list head to the caller.
    //
    head = list->listHead;
    *record = head;

    //
    //  If the list is not currently empty, update the list head pointer and
    //  decrement the list record count.
    //
    if ( head != NULL )
    {
        list->listHead = head->next;
        --list->count;
        free ( head );
    }
    //
    //  Return the completion code to the caller.
    //
    return status;
}
