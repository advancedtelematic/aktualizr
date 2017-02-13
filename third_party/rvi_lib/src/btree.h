#ifndef _BTREE_H_
#define _BTREE_H_

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <stddef.h>
#include <stdbool.h>


//
//  The following defines will enable some debugging features if they are
//  defines.  If these features are not desired, either comment out the
//  following lines or undefine the symbols.
//
//  BTREE_DEBUG: If defined, will cause the btree "print" functions to be
//               compiled and linked.
//
//  BTREE_TRACE: If defined, will cause the btree code to emit a message at
//               the beginning of each function indicating that that function
//               is being executed.  If defined, the TRACE macro below is
//               enabled.  The TRACE macro is identical to a "printf"
//               invocation except it can be completely eliminated from the
//               compiled output if BTREE_TRACE is not defined.
//
#undef BTREE_DEBUG
#undef BTREE_TRACE

#ifdef BTREE_DEBUG
#   define LOG(...) printf ( __VA_ARGS__ )
#   define PRINT_NODE(...) print_single_node ( __VA_ARGS__ )
#   define PRINT_SUBTREE(...) print_subtree ( __VA_ARGS__ )
#   define PRINT_DATA(...) printFunction ( __VA_ARGS__ )
#else
#   define LOG(...)
#   define PRINT_NODE(...)
#   define PRINT_SUBTREE(...)
#   define PRINT_DATA(...)
#endif

#ifdef BTREE_TRACE
#   define TRACE(...) printf ( __VA_ARGS__ )
#else
#   define TRACE(...)
#endif


//
//  The following defines allow us to redefine the names of some of the system
//  functions that we need.  The user can supply his own versions as long as
//  the function signature is the same as the system version by simply
//  redefining the following symbols to point to his own implementations.
//
#define MEM_ALLOC malloc
#define MEM_FREE  free
#define COPY      memmove
#define PRINT     printf

//
//  Define the callback function types used by the btree code.
//
typedef int  (*compareFunc) ( void*, void* );

typedef void (*traverseFunc)( void* );

typedef void (*printFunc)   ( char*, void* );


//
//  Define the structure of a single btree node.
//
typedef struct bt_node_t
{
    struct bt_node_t*  next;        // Pointer used for linked list traversal
    struct bt_node_t*  parent;      // Pointer to the parent of this node
    bool               leaf;        // Used to indicate whether leaf or not
    unsigned int       keysInUse;   // Number of keys currently defined
    unsigned int       level;       // Level of this node in the btree
    void**             dataRecords; // Array of data record pointers
    struct bt_node_t** children;    // Array of link pointers to children nodes

}   bt_node_t;


//
//  Define the btree definition structure.  In this incarnation of the btree
//  algorithm, the keys are pointers to a user specified data structure.  The
//  keys and values are then contained in that user data structure and the
//  btree supplied comparison function is called to compare the keys in two
//  instances of these records.
//
typedef struct btree_t
{
    unsigned int order;           // B-Tree order
    unsigned int nodeFullSize;    // The number of records in a full node
    unsigned int sizeofKeys;      // The total size of the keys in one node
    unsigned int sizeofPointers;  // The total size of the pointers in one node
    unsigned int count;           // The total number of records in the btree
    bt_node_t*   root;            // Root of the btree
    compareFunc  compareCB;       // Key compare function

}   btree_t;


//
//  Define the btree iterator control structure.  This structure is used to
//  keep track of positions within the btree and is used by the iterator set
//  of functions to enable the "get_next" and "get_previous" functions.
//
typedef struct btree_iterator_t
{
    btree_t*     btree;
    void*        key;
    bt_node_t*   node;
    unsigned int index;

}   btree_iterator_t;

typedef btree_iterator_t* btree_iter;


//
//  Define the public API to the btree library.
//
extern btree_t* btree_create   ( unsigned int order, compareFunc compareFunction );

extern void     btree_destroy  ( btree_t* btree );

extern int      btree_insert   ( btree_t* btree, void* data );

extern int      btree_delete   ( btree_t* btree, bt_node_t* subtree, void* key );

extern void*    btree_get_min  ( btree_t* btree );

extern void*    btree_get_max  ( btree_t* btree );

extern void*    btree_search   ( btree_t* btree, void* key );

extern void     btree_traverse ( btree_t* btree, traverseFunc traverseCB );

//
//  Define the btree iterator functions.
//
//  To iterate through a range of values in the btree the user needs to
//  execute one of the "find" functions to position an iterator which is
//  returned by the "find" function and then iterate from there by calling
//  either the btree_iter_next or btree_iter_previous functions until the
//  position in the btree no longer satisfies the user's terminating
//  conditions.
//
//  The user may iterate forward (to increasing keys) or backward by using the
//  either the btree_iter_next or btree_iter_previous functions.
//
//     Example usage (assuming the btree is populated with "userData" records):
//
//         userData record = { 12, "" };
//
//         btree_iter iter = btree_find ( &record );
//
//         while ( ! btree_iter_at_end ( iter ) )
//         {
//             userData* returnedData = btree_iter_data ( iter );
//             ...
//             btree_iter_next ( iter );
//         }
//         btree_iter_cleanup ( iter );
//
//     The above example will find all of the records in the btree beginning with
//     { 12, "" } to the end of the btree.
//
//  Note that this iterator definition is designed to operate similarly to the
//  C++ Standard Template Library's container iterators.  There are obviously
//  some differences due to the language differences but it's pretty close.
//

//
//  The btree_find function is similar to the btree_search function except
//  that it will find the first key that is greater than or equal to the
//  specified value (rather than just the key that is equal to the specified
//  value).  This function is designed to be used for forward iterations using
//  the iter->next function to initialize the user's iterator to the first key
//  location.
//
extern btree_iter btree_find ( btree_t* btree, void* key );

//
//  The btree_rfind function is similar to the btree_search function except
//  that it will find the last key that is less than or equal to the specified
//  value (rather than just the key that is equal to the specified value).
//  This function is designed to be used for reverse iterations using the
//  iter->previous function to initialize the user's iterator to the last key
//  location.
//
extern btree_iter btree_rfind ( btree_t* btree, void* key );

//
//  Position the specified iterator to the first record in the btree.
//
extern btree_iter btree_iter_begin ( btree_t* btree );

//
//  Position the specified itrator to the "end" position in the btree (which
//  is past the last record of the btree.
//
extern btree_iter btree_iter_end ( btree_t* btree );

//
//  Position the specified iterator to the next higher key value in the btree
//  from the current position.
//
extern void btree_iter_next ( btree_iter iter );

//
//  Position the specified iterator to the next lower key value in the btree
//  from the current position.
//
extern void btree_iter_previous ( btree_iter iter );

//
//  Compare the 2 iterators and return their relative values as determined by
//  the comparison function of the current btree.
//
extern int btree_iter_cmp ( btree_iter iter1, btree_iter iter2 );

//
//  Test to see if the given iterator is at the end of the btree.
//
extern bool btree_iter_at_end ( btree_iter iter );

//
//  Return the pointer to the user data structure that is stored in the btree
//  at the current iterator position.
//
extern void* btree_iter_data ( btree_iter iter );

//
//  Cleanup the specified iterator and release any resources being used by
//  this iterator.  This function MUST be called when the user is finished
//  using an iterator or resources (such as memory blocks) will be leaked to
//  the system.  Once this function has been called with an iterator, that
//  iterator may not be used again for any of the btree functions unless it
//  has subsequently been reinitialized using one of the iterator
//  initialization functions defined here.
//
extern void btree_iter_cleanup ( btree_iter iter );


//
//  M a c r o   D e f i n i t i o n s
//
//  Define some helper functions that get defined as inlines so they will most
//  likely disappear when the code is compiled without debug enabled.
//
static inline unsigned int getCount ( btree_t* btree )
{
    return btree->count;
}


static inline bt_node_t* getLeftChild ( bt_node_t* node, unsigned int index )
{
    return node->children[index];
}


static inline bt_node_t* getRightChild ( bt_node_t* node, unsigned int index )
{
    return node->children[index + 1];
}


static inline bt_node_t* getDataRecord ( bt_node_t* node, unsigned int index )
{
    return node->dataRecords[index];
}


//
//  Define some things that are only used when debugging the btree code.
//
#ifdef BTREE_DEBUG

extern void print_subtree ( btree_t* btree, bt_node_t* node, printFunc printCB );

#endif


#if 0
[ from /home/dmies/git/btree/btree.h ]

//
//  The following is the fixed size of each node of the btree.  This size
//  should be chosen to be a multiple of the system memory page size.  The
//  system memory page size can be gotten with sysconf(_SC_PAGE_SIZE).
//
//  TODO: Change the node size to be dynamically derived from the above system
//  call and build the MIN and MAX values below from that.  This will make the
//  code more portable to different platforms.
//
#define NODE_SIZE ( 4096 )

//
//  The formula for the maximum number of keys in each tree node is derived
//  from the following formula:
//
//      NODE_SIZE = sizeof(count) + sizeof(data)*MAX + sizeof(link)*(MAX+1)
//
//  Substituting all the numbers what we know...
//
//      4096      = 8             + 16 * MAX          + 8 * (MAX + 1)
//      4096      = 8             + 16 * MAX          + 8 * MAX + 8
//      4096      = 8             + 24 * MAX + 8
//      4096      = 24 * MAX + 16
//
//  Solving this equation for MAX gives:
//
//      MAX = ( 4096 - 16 ) / 24
//      MAX = 170
//
//  This is basically a calculation of the (fixed) size of a node divided by
//  the size of all of the fields in the node.  Note that this calculation
//  needs to take into account any alignment that might take place inside the
//  structure.  The beginning of the structure will always be on a memory page
//  boundary so no alignment takes place there.
//
#define MAX ( ( NODE_SIZE - 2 * sizeof(unsigned long) ) / ( 3 * sizeof(unsigned long) ) )
#define MIN ( MAX / 2 )

#endif

#endif
