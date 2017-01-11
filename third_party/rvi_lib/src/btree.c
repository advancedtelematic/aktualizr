#include <errno.h>
#include <string.h>

#include "btree.h"


extern void printFunction ( char* leader, void* data );

//
//  Define an enumerated type that we can use to specify "left" and "right".
//
typedef enum
{
    left = -1,
    right = 1

}   position_t;


//
//  Define the btree position definition structure.  This structure contains
//  all the information needed to uniquely define a position in the tree.
//
typedef struct
{
    bt_node_t*   node;
    unsigned int index;

}   nodePosition;


//
//  Define the local functions that are not exposed in the API.
//
#ifdef BTREE_DEBUG
static void print_single_node ( btree_t* btree, bt_node_t* node,
                                printFunc printCB );
#endif

static bt_node_t* allocate_btree_node ( btree_t* btree );

static void btree_insert_nonfull ( btree_t* btree, bt_node_t* parent_node,
                                   void* data );

static int free_btree_node ( bt_node_t* node );

static nodePosition get_btree_node ( btree_t* btree, void* key );

static int delete_key_from_node ( btree_t* btree, nodePosition* nodePosition );

static void move_key ( btree_t* btree, bt_node_t* node, unsigned int index,
                       position_t pos );

static nodePosition get_max_key_pos ( btree_t* btree, bt_node_t* subtree );

static nodePosition get_min_key_pos ( btree_t* btree, bt_node_t* subtree );

static bt_node_t* merge_siblings ( btree_t* btree, bt_node_t* parent,
                                   unsigned int index );

static void btree_traverse_node ( bt_node_t* subtree,
                                  traverseFunc traverseFunction );

/**
*   Used to create a btree with just an empty root node.  Note that the
*   "order" parameter below is the minumum number of keys that exist in each
*   node of the tree.  This is typically 1/2 of a "full" node.  Also note that
*   the number of keys is one less than the number of child pointers.  That's
*   because each key has a left and a right pointer but two adjacent keys
*   share a pointer (the right pointer of one is the left pointer of the
*   other).
*
*   @param order The order of the B-tree
*   @return The pointer to an empty B-tree
*/
btree_t* btree_create ( unsigned int order, compareFunc compareFunction )
{
    btree_t* btree;

    TRACE ( "In btree_create\n" );

    //
    //  Go allocate a memory block for a new btree data structure.
    //
    btree = MEM_ALLOC ( sizeof(struct btree_t) );

    //
    //  Initialize all the fields in the new btree structure.
    //
    btree->order           = order;
    btree->nodeFullSize    = 2 * order - 1;
    btree->sizeofKeys      = ( 2 * order - 1 ) * sizeof(void*);
    btree->sizeofPointers  = 2 * order * sizeof(void*);
    btree->count           = 0;
    btree->compareCB       = compareFunction;

    //
    //  Go allocate the root node of the tree.
    //
    btree->root = allocate_btree_node ( btree );

    //
    //  Return the pointer to the btree to the caller.
    //
    return btree;
}

/**
*       Function used to allocate memory for the btree node
*       @param order Order of the B-Tree
*   @param leaf boolean set true for a leaf node
*       @return The allocated B-tree node
*/
static bt_node_t* allocate_btree_node ( btree_t* btree )
{
    bt_node_t* node;

    //
    //  Go create a new node data structure.
    //
    node = (bt_node_t*)MEM_ALLOC ( sizeof(struct bt_node_t) );

    TRACE ( "In allocate_btree_node - Allocated %p\n", node );

    //
    //  Set the number of records in this node to zero.
    //
    node->keysInUse = 0;

    //
    //  Allocate the memory for the array of record pointers and put the
    //  address of that array into the node.
    //
    node->dataRecords = MEM_ALLOC ( btree->sizeofKeys );

    //
    //  Allocate the memory for the array of child node pointers and put the
    //  address of that array into the node.
    //
    node->children = MEM_ALLOC ( btree->sizeofPointers );

    //
    //  Mark this new node as a leaf node.
    //
    node->leaf = true;

    //
    //  Set the tree level to zero.
    //
    node->level = 0;

    //
    //  Set the linked list pointer to NULL.
    //
    node->next = NULL;

    //
    //  Set the parent node link to NULL.
    //
    node->parent = NULL;

    //
    //  Return the address of the new node to the caller.
    //
    return node;
}

/**
*       Function used to free the memory allocated to the b-tree
*       @param node The node to be freed
*       @param order Order of the B-Tree
*       @return The allocated B-tree node
*/
static int free_btree_node ( bt_node_t* node )
{
    TRACE ( "In free_btree_node - Freeing %p\n", node );

    MEM_FREE ( node->children );
    MEM_FREE ( node->dataRecords );
    MEM_FREE ( node );

    return 0;
}

/**
*   Used to split the child node and adjust the parent so that
*   it has two children
*   @param parent Parent Node
*   @param index Index of the parent node where the new record will be put
*   @param child  Full child node
*
*/
static void btree_split_child ( btree_t*     btree,
                                bt_node_t*   parent,
                                unsigned int index,
                                bt_node_t*   child )
{
    int          i = 0;
    unsigned int order = btree->order;

    TRACE ( "In btree_split_child\n" );

    //
    //  Go create a new tree node as our "new child".  The new node will be
    //  created at the same level as the old node (at least initially - It may
    //  get moved when we balance the tree later) and to the right of the node
    //  that is being split.
    //
    bt_node_t* newChild = allocate_btree_node ( btree );

    //
    //  Initialize the fields of the new node appropriately.
    //
    newChild->leaf = child->leaf;
    newChild->level = child->level;
    newChild->keysInUse = btree->order - 1;
    newChild->parent = parent;

    //
    //  Move the keys from beyond the split point from the old child to the
    //  beginning of the new child.
    //
    for ( i = 0; i < order - 1; i++ )
    {
        newChild->dataRecords[i] = child->dataRecords[i + order];

        //
        //  If this is not a leaf node, also copy the "children" pointers from
        //  the old child to the new one.
        //
        if ( ! child->leaf )
        {
            newChild->children[i] = child->children[i + order];

            //
            //  Make the parent pointer in the child that we just moved now
            //  point to the new child rather than the old child from which it
            //  was moved.
            //
            newChild->children[i]->parent = newChild;
        }
    }
    //
    //  If this is not a leaf node, copy the last node pointer (remember that
    //  there is one extra node pointer for a given number of keys).
    //
    if ( ! child->leaf )
    {
        newChild->children[i] = child->children[i + order];

        //
        //  Make the parent pointer in the child that we just moved now point
        //  to the new child rather than the old child from which it was
        //  moved.
        //
        newChild->children[i]->parent = newChild;
    }
    //
    //  Adjust the record count of the old child to be correct now that we've
    //  copied out about half of the data in this node.
    //
    child->keysInUse = order - 1;

    //
    //  Move all the child pointers of the parent up (towards the end) of the
    //  parent node to make room for the new node we just created.
    //
    for ( i = parent->keysInUse + 1; i > index + 1; i-- )
    {
        parent->children[i] = parent->children[i - 1];
    }
    //
    //  Store the new child we just created into the node list of the parent.
    //
    parent->children[index + 1] = newChild;

    //
    //  Move all the data records in the parent one place to the right as well
    //  to make room for the new child.
    //
    for ( i = parent->keysInUse; i > index; i-- )
    {
        parent->dataRecords[i] = parent->dataRecords[i - 1];
    }
    //
    //  Move the key that was used to split the node from the child to the
    //  parent.  Note that it was split at the "index" value specified by the
    //  caller.
    //
    parent->dataRecords[index] = child->dataRecords[order - 1];

    //
    //  Increment the number of records now in the parent.
    //
    parent->keysInUse++;
}

/**
*   Used to insert a key in the non-full node
*   @param btree The btree
*   @param parentNode The node address of the parent node
*   @param the data to be inserted into the tree
*   @return void
*
*   TODO: I can't believe that this function can't fail!  It has no return
*   code!
*
*   TODO: We could make the node search more efficient by implementing a
*   binary search instead of the linear one.  For small tree orders, the
*   linear search is more efficient but as the tree orders get larger, the
*   binary search will win.
*/
static void btree_insert_nonfull ( btree_t*   btree,
                                   bt_node_t* parentNode,
                                   void*      data )
{
    int        i;
    bt_node_t* child;
    bt_node_t* node = parentNode;

    TRACE ( "In btree_insert_nonfull\n" );

//
//  Note that his "goto" has been implemented to eliminate a recursive call to
//  this function.
//
insert:

    //
    //  Position us at the end of the records in this node.
    //
    i = node->keysInUse - 1;

    //
    //  If this is a leaf node...
    //
    if ( node->leaf )
    {
        //
        //  Move all the records in the node one position to the right
        //  (working backwards from the end) until we get to the point where
        //  the new data will go.
        //
        while ( i >= 0 &&
                ( btree->compareCB ( data, node->dataRecords[i] ) < 0 ) )
        {
            node->dataRecords[i + 1] = node->dataRecords[i];
            i--;
        }
        //
        //  Put the new data into this node and increment the number of
        //  records in the node.
        //
        node->dataRecords[i + 1] = data;
        node->keysInUse++;
    }
    //
    //  If this is not a leaf node...
    //
    else
    {
        //
        //  Search the node records backwards from the end until we find the
        //  point at which the new data should go.
        //
        while ( i >= 0 &&
                ( btree->compareCB ( data, node->dataRecords[i] ) < 0 ) )
        {
            i--;
        }
        //
        //  Grab the right hand child of this position so we can check to see
        //  if the new data goes into the current node or the right child
        //  node.
        //
        i++;
        child = node->children[i];

        //
        //  If the child node is full...
        //
        if ( child->keysInUse == btree->nodeFullSize )
        {
            //
            //  Go split this node into 2 nodes.
            //
            btree_split_child ( btree, node, i, child );

            //
            //  If the new data is greater than the current record then
            //  increment to the next record.
            //
            if ( btree->compareCB ( data, node->dataRecords[i] ) > 0 )
            {
                i++;
            }
        }
        //
        //  Reset the starting node and go back up and try to insert this
        //  again.  Note that this "goto" actually implements the same thing
        //  as a recursive call except it's more efficient and doesn't blow
        //  out the stack.
        //
        node = node->children[i];
        goto insert;
    }
}

/**
*       Function used to insert node into a B-Tree
*       @param root Root of the B-Tree
*       @param data The address of the record to be inserted
*       @return success or failure
*
*    TODO: This function returns a value but it always returns a 0!
*/
int btree_insert ( btree_t* btree, void* data )
{
    bt_node_t* rootNode;

    TRACE ( "In btree_insert\n" );

    //
    //  Start the search at the root node.
    //
    rootNode = btree->root;

    //
    //  If the root node is full then we'll need to create a new node that
    //  will become the root and the full root will be split into 2 nodes that
    //  are each approximately half full.
    //
    if ( rootNode->keysInUse == btree->nodeFullSize )
    {
        bt_node_t* newRoot;

        //
        //  Go create a new node with the same order as the rest of the tree.
        //
        newRoot = allocate_btree_node ( btree );

        //
        //  Increment the level of this new root node to be one more than the
        //  old btree root node.  We are making the whole tree one level
        //  deeper.
        //
        newRoot->level = rootNode->level + 1;

        //
        //  Make the new node we just created the root node.
        //
        btree->root = newRoot;

        //
        //  Initialize the rest of the fields in the new root node and make
        //  the old root the first child of the new root node (the left
        //  child).
        //
        newRoot->leaf        = false;
        newRoot->keysInUse   = 0;
        newRoot->children[0] = rootNode;

        rootNode->parent     = newRoot;

        //
        //  Go split the old root node into 2 nodes and slosh the data between
        //  them.
        //
        btree_split_child ( btree, newRoot, 0, rootNode );

        //
        //  Go insert the new data we are trying to insert into the new tree
        //  that has the new node as it's root now.
        //
        btree_insert_nonfull ( btree, newRoot, data );
    }
    //
    //  If the root node is not full then just go insert the new data into
    //  this tree somewhere.
    //
    else
    {
        btree_insert_nonfull ( btree, rootNode, data );
    }
    //
    //  Increment the number of records in the btree.
    //
    ++btree->count;

    //
    //  Return the completion code to the caller.
    //
    return 0;
}

/**
*   Used to get the position of the MAX key within the subtree
*   @param btree The btree
*   @param subtree The subtree to be searched
*   @return The node containing the key and position of the key
*/
static nodePosition get_max_key_pos ( btree_t* btree, bt_node_t* subtree )
{
    nodePosition nodePosition;
    bt_node_t* node = subtree;

    TRACE ( "In get_max_key_pos\n" );

    //
    //  This loop basically runs down the right hand node pointers in each
    //  node until it gets to the leaf node at which point the value we want
    //  is the last one (right most) in that node.
    //
    while ( true )
    {
        //
        //  If we hit a NULL node pointer, it's time to quit.
        //
        if ( node == NULL )
        {
            break;
        }
        //
        //  Save the current node pointer and the index of the last record in
        //  this node.
        //
        nodePosition.node = node;
        nodePosition.index = node->keysInUse - 1;

        //
        //  If this is a leaf node...
        //
        if ( node->leaf )
        {
            //
            //  If we got to a leaf node, we are at the bottom of the tree so
            //  the value we want is the last record of this node.  We've
            //  already initialized the return structure with this value so
            //  just return the node position data for this node.
            //
            return nodePosition;
        }
        //
        //  If this is not a leaf node then we need to move down the tree to
        //  the right most child in this node.
        //
        else
        {
            //
            //  Make the current node the right most child node of the current
            //  node.
            //
            node = node->children[node->keysInUse];
        }
    }
    //
    //  Return the node position data to the caller.
    //
    return nodePosition;
}

/**
*   Used to get the position of the MIN key within the subtree
*   @param btree The btree
*   @param subtree The subtree to be searched
*   @return The node containing the key and position of the key
*/
static nodePosition get_min_key_pos ( btree_t* btree, bt_node_t* subtree )
{
    nodePosition nodePosition;
    bt_node_t* node = subtree;

    TRACE ( "In get_min_key_pos\n" );

    //
    //  This loop basically runs down the left hand node pointers in each node
    //  until it gets to the leaf node at which point the value we want is the
    //  first one (left most) in that node.
    //
    while ( true )
    {
        //
        //  If we hit a NULL node pointer, it's time to quit.
        //
        if ( node == NULL )
        {
            break;
        }
        //
        //  Save the current node pointer and the index of the first record in
        //  this node.
        //
        nodePosition.node = node;
        nodePosition.index = 0;

        //
        //  If this is a leaf node...
        //
        if ( node->leaf )
        {
            //
            //  If we got to a leaf node, we are at the bottom of the tree so
            //  the value we want is the first record of this node.  We've
            //  already initialized the return structure with this value so
            //  just return the node position data for this node.
            //
            return nodePosition;
        }
        //
        //  If this is not a leaf node then we need to move down the tree to
        //  the left most child in this node.
        //
        else
        {
            //
            //  Make the current node the left most child node of the current
            //  node.
            //
            node = node->children[0];
        }
    }
    //
    //  Return the node position data to the caller.
    //
    return nodePosition;
}

/**
*   (case 3b from Cormen)
*
*   This function will merge the 2 child nodes of the record defined by the
*   parent node and index values into a single node.  The left child of the
*   specified record will be kept and the record at the index point in the
*   parent will be moved into the left child.  The remaining records in the
*   right child will then be copied into the left child as well.
*
*   This function makes some assumptions:
*
*       The left and right children both have order - 1 records.
*       The parent has at least one record.
*
*   @param btree The btree
*   @param node The parent node
*   @param index of the child
*   @param pos left or right
*   @return none
*/
static bt_node_t* merge_siblings ( btree_t* btree, bt_node_t* parent,
                                   unsigned int index )
{
    unsigned int i;
    unsigned int j;
    bt_node_t*   leftChild;
    bt_node_t*   rightChild;

    TRACE ( "In merge_siblings\n" );

    //
    //  If the index is the last position in use in the parent, back up one
    //  position.
    //
    if ( index == ( parent->keysInUse ) )
    {
        index--;
    }
    //
    //  Get the pointers to the left and right children of the record in the
    //  parent node at the specified index.
    //
    leftChild  = parent->children[index];
    rightChild = parent->children[index + 1];

    //
    //  Move the data record from the parent to the left child at the split
    //  point.
    //
    leftChild->dataRecords[btree->order - 1] = parent->dataRecords[index];

    //
    //  Move all of the rest of the data records from the right child into the
    //  left child and move the children pointers if the leftChild is not a
    //  leaf node.
    //
    for ( j = 0; j < btree->order - 1; j++ )
    {
        leftChild->dataRecords[j + btree->order] = rightChild->dataRecords[j];

        //
        //  If this is not a leaf node, move the child pointers from the right
        //  child into the left child.
        //
        if ( ! leftChild->leaf )
        {
            //
            //  Move the left child pointer.
            //
            leftChild->children[j + btree->order] = rightChild->children[j];

            //
            //  Update the parent pointer in the child node we just moved.
            //
            leftChild->children[j + btree->order]->parent = leftChild;
        }
    }
    //
    //  Move the last child pointer from the right child to the left child, if
    //  the leftChild is not a leaf node.
    //
    if ( ! leftChild->leaf )
    {
        //
        //  Move the last right child pointer.
        //
        leftChild->children[2 * btree->order - 1] =
            rightChild->children[btree->order - 1];
        //
        //  Update the parent pointer in the child node we just moved.
        //
        leftChild->children[2 * btree->order - 1]->parent = leftChild;
    }
    //
    //  Increment the keysInUse by the number of records we just moved from
    //  the right child plus one extra for the parent node that we moved down.
    //
    leftChild->keysInUse += rightChild->keysInUse + 1;

    //
    //  If the parent node is not empty now that we moved the specified data
    //  record from it to the new merged node...
    //
    if ( ( parent->keysInUse ) > 1 )
    {
        //
        //  Decrement the number of records in the parent.
        //
        parent->keysInUse--;

        //
        //  Move all of the data records and child pointers one spot to the
        //  left in the parent node to close up the gap left when we move the
        //  data record out to the leftChild.
        //
        for ( j = index; j < parent->keysInUse; j++ )
        {
            parent->dataRecords[j] = parent->dataRecords[j + 1];
            parent->children[j + 1] = parent->children[j + 2];
        }
        //
        //  Erase all of the data records beyond the last valid record in the
        //  parent.
        //
        //  TODO: This should not be necessary.  We might want to do it for
        //  security but it's not required for the algorithm to work
        //  correctly.
        //
        for ( i = parent->keysInUse; i < 2 * btree->order - 1; i++ )
        {
            parent->dataRecords[i] = NULL;
            parent->children[i + 1] = NULL;
        }
    }
    //
    //  If the parent node is now empty, it must be the root node so make the
    //  merged node we just created (the leftChild) the new root.
    //
    else
    {
        free_btree_node ( parent );

        leftChild->parent = NULL;

        btree->root = leftChild;
    }
    //
    //  Update the node chain pointers to be correct now that we will be
    //  deleting the right child node.
    //
    leftChild->next = rightChild->next;

    //
    //  Go free up the right child node.
    //
    free_btree_node ( rightChild );

    //
    //  Return the merged left child node to the caller.
    //
    return leftChild;
}

/**
*   Move the key from node to another
*   @param btree The B-Tree
*   @param node The parent node
*   @param index of the key to be moved done
*   @param pos the position of the child to receive the key
*   @return none
*/
static void move_key ( btree_t* btree, bt_node_t* node, unsigned int index,
                       position_t pos )
{
    bt_node_t*   lchild;
    bt_node_t*   rchild;
    unsigned int i;

    TRACE ( "In move_key\n" );

    if ( pos == right )
    {
        index--;
    }
    lchild = node->children[index];
    rchild = node->children[index + 1];

    // Move the key from the parent to the left child
    if ( pos == left )
    {
        lchild->dataRecords[lchild->keysInUse] = node->dataRecords[index];
        lchild->children[lchild->keysInUse + 1] = rchild->children[0];
        rchild->children[0] = NULL;
        lchild->keysInUse++;

        node->dataRecords[index] = rchild->dataRecords[0];
        rchild->dataRecords[0] = NULL;

        for ( i = 0; i < rchild->keysInUse - 1; i++ )
        {
            rchild->dataRecords[i] = rchild->dataRecords[i + 1];
            rchild->children[i] = rchild->children[i + 1];
        }
        rchild->children[rchild->keysInUse - 1] =
            rchild->children[rchild->keysInUse];
        rchild->keysInUse--;
    }
    else
    {
        // Move the key from the parent to the right child
        for ( i = rchild->keysInUse; i > 0; i-- )
        {
            rchild->dataRecords[i] = rchild->dataRecords[i - 1];
            rchild->children[i + 1] = rchild->children[i];
        }
        rchild->children[1] = rchild->children[0];
        rchild->children[0] = NULL;

        rchild->dataRecords[0] = node->dataRecords[index];

        rchild->children[0] = lchild->children[lchild->keysInUse];
        lchild->children[lchild->keysInUse] = NULL;

        node->dataRecords[index] = lchild->dataRecords[lchild->keysInUse - 1];
        lchild->dataRecords[lchild->keysInUse - 1] = NULL;

        lchild->keysInUse--;
        rchild->keysInUse++;
    }
}

/**
*   Used to delete a key from the B-tree node
*   @param btree The btree
*   @param node The node from which the key is to be deleted
*   @param key The key to be deleted
*   @return 0 on success -1 on error
*/

int delete_key_from_node ( btree_t* btree, nodePosition* nodePosition )
{
    unsigned int maxNumberOfKeys = 2 * btree->order - 1;
    unsigned int i;
    bt_node_t*   node = nodePosition->node;

    TRACE ( "In delete_key_from_node\n" );

    //
    //  If this is not a leaf node, return an error code.  This function
    //  should never be called with anything other than a leaf node.
    //
    if ( ! node->leaf )
    {
        return -1;
    }
    //
    //  Move all of the data records down from the position of the data we
    //  want to delete to the end of the node to collapse the data list after
    //  the removal of the target record.
    //
    for ( i = nodePosition->index; i < maxNumberOfKeys - 1; i++ )
    {
        node->dataRecords[i] = node->dataRecords[i + 1];
    }
    //
    //  Decrement the number of keys in use in this node.
    //
    node->keysInUse--;

    //
    //  If the node is now empty and it is not the root node, go free it up.
    //
    //  Note that the root node is always present, even if the tree is empty.
    //
    if ( ( node->keysInUse == 0 ) && ( node != btree->root ) )
    {
        free_btree_node ( node );
    }
    //
    //  Return a good completion code to the caller.
    //
    return 0;
}

/**
*       Function used to delete a node from a  B-Tree
*       @param btree The B-Tree
*       @param key Key of the node to be deleted
*       @param value function to map the key to an unique integer value
*       @param compare Function used to compare the two nodes of the tree
*       @return success or failure
*/

int btree_delete ( btree_t* btree, bt_node_t* subtree, void* data )
{
    int             i;
    int             diff;
    unsigned int    index;
    unsigned int    splitPoint = btree->order - 1;
    bt_node_t*      node = NULL;
    bt_node_t*      rsibling;
    bt_node_t*      lsibling;
    bt_node_t*      parent;
    nodePosition    sub_nodePosition;
    nodePosition    nodePosition;

    TRACE ( "In btree_delete\n" );

    node = subtree;
    parent = NULL;

del_loop:

    //
    //  Starting at node "subtree" (x), test whether the key k is in the node.
    //  If it is, descend to the delete cases. If it's not, prepare to descend
    //  into a subtree, but ensure that it has at least t (btree->order) keys,
    //  so that we can delete 1 key without promoting a new root.
    //
    //  Remember: the minimum number of keys in a (non-root) node is
    //            btree->order - 1 (equivalently, (t - 1) or splitPoint)
    //
    while ( true )
    {
        //
        //  If there are no keys in this node, return an error
        //
        if ( !node->keysInUse )
        {
            return -ENODATA;
        }
        //
        //  Find the index of the key greater than or equal to the key that we
        //  are looking for. If our target key is greater than all of the keys
        //  in use in this node, i == node->keysInUse and we descend into the
        //  right child of the largest key in the node
        //
        i = 0;
        while ( i < node->keysInUse )
        {
            diff = btree->compareCB ( data, node->dataRecords[i] );

            //
            //  If we found the exact key we are looking for, just break out of
            //  the loop.
            //
            if ( diff <= 0 )
            {
                break;
            }
            i++;
        }
        //
        //  Save the index value of this location.
        //
        index = i;

        //
        //  If we found the exact key we are looking for, just break out of
        //  the loop.
        //
        if ( diff == 0 )
        {
            break;
        }
        //
        //  If we are in a leaf node and we didn't find the key in question
        //  then we have a "not found" condition.
        //
        if ( node->leaf )
        {
            return -ENODATA;
        }
        //
        //  Save the current node as the parent.
        //
        parent = node;

        //
        //  Get the left child of the key that we found -- since we stopped at
        //  the first record greater than our key, the key must be in the left
        //  subtree.
        //
        node = node->children[i];

        //
        //  If the child pointer is NULL then we have a "not found" condition.
        //
        if ( node == NULL )
        {
            return -ENODATA;
        }
        //
        //  If the new node has at least (splitPoint + 1) (aka, t) keys,
        //  descend directly into the subtree rooted at the new node to search
        //  for the key k.
        //
        if ( node->keysInUse > splitPoint )
        {
            continue;
        }
        //
        //  Since the node does not have at least t keys, we cannot safely
        //  descend into the subtree rooted at the node. Instead, merge the
        //  node with an immediate sibling. Perform some tests to figure out
        //  exactly how we should do this.
        //
        //  These cases correspond to case "3" from Cormen, et al., 2009
        //  (p501)
        //
        //  If we are at the last populated position in this node...
        //
        if ( index == ( parent->keysInUse ) )
        {
            //
            //  Set the left sibling to the left child of our parent and the
            //  right sibling is NULL.
            //
            lsibling = parent->children[parent->keysInUse - 1];
            rsibling = NULL;
        }
        //
        //  Otherwise, if we are at the beginning of the node, set the left
        //  sibling to NULL and the right sibling to the right child of our
        //  parent.
        //
        else if ( index == 0 )
        {
            lsibling = NULL;
            rsibling = parent->children[1];
        }
        //
        //  Otherwise, we are somewhere in the middle of the node so just set
        //  the left and right siblings to the left and right children of our
        //  parent.
        //
        else
        {
            lsibling = parent->children[i - 1];
            rsibling = parent->children[i + 1];
        }
        //
        //
        //
        if ( node->keysInUse == splitPoint && parent )
        {
            TRACE ( "In btree_delete[3a]\n" );

            //
            //  Case 3a: The target node has (t - 1) keys but the right
            //           sibling has t keys. Give the target node an extra key
            //           by moving a key from the parent into the target, then
            //           move a key from the target node's right sibling back
            //           into the parent, and update the corresponding
            //           pointers.
            //
            if ( rsibling && ( rsibling->keysInUse > splitPoint ) )
            {
                move_key ( btree, parent, i, left );
            }
            //
            //  Case 3a: The target node has (t - 1) keys but the left sibling
            //           has t keys. Just like the previous case, but move a
            //           key from the target node's left sibling into the
            //           parent.
            //
            else if ( lsibling && ( lsibling->keysInUse > splitPoint ) )
            {
                move_key ( btree, parent, i, right );
            }
            //
            //  Case 3b: The target node and its siblings have (t - 1) keys, so
            //  pick a sibling to merge. Move a key from the parent into the
            //  merged node to serve as the new median key. If the parent no
            //  longer has any kews, promote the merged node to root.
            //
            //  Case 3b -- merge LEFT sibling
            //
            else if ( lsibling && ( lsibling->keysInUse == splitPoint ) )
            {
                TRACE ( "In btree_delete[3b1]\n" );

                node = merge_siblings ( btree, parent, i );
            }
            //
            //  Case 3b -- merge RIGHT sibling
            //
            else if ( rsibling && ( rsibling->keysInUse == splitPoint ) )
            {
                TRACE ( "In btree_delete[3b2]\n" );

                node = merge_siblings ( btree, parent, i );
            }
            //
            // The current node doesn't have enough keys, but we can't perform
            // a merge, so return an error
            //
            else
            {
                TRACE ( "In btree_delete[3b3]\n" );

                return -1;
            }
        }
    }
    //
    //  Case 1 : The node containing the key is found and is a leaf node.
    //           The leaf node also has keys greater than the minimum
    //           required so we simply remove the key.
    //
    if ( node->leaf && ( node->keysInUse > splitPoint ) )
    {
        TRACE ( "In btree_delete[1a]\n" );

        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );

        //
        //  Decrement the number of records in the btree.
        //
        --btree->count;

        return 0;
    }
    //
    //  If the leaf node is the root permit deletion even if the number of
    //  keys is less than (t - 1).
    //
    if ( node->leaf && ( node == btree->root ) )
    {
        TRACE ( "In btree_delete[1a]\n" );

        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );

        //
        //  Decrement the number of records in the btree.
        //
        --btree->count;

        return 0;
    }
    //
    //  Case 2: The node x containing the key k was found and is an internal
    //  node.
    //
    if ( ! node->leaf )
    {
        //
        // Case 2a: The child y that precedes k has at least t keys
        //
        if ( node->children[index]->keysInUse > splitPoint )
        {
            TRACE ( "In btree_delete[2a]\n" );

            // Find the predecessor k' in the subtree rooted at y
            sub_nodePosition =
                get_max_key_pos ( btree, node->children[index] );

            // Replace k by k' in x
            node->dataRecords[index] =
                sub_nodePosition.node->dataRecords[sub_nodePosition.index];

            // Recursively delete k' from the subtree rooted at y
            btree_delete ( btree, node->children[index],
                    node->dataRecords[index] );
            if ( sub_nodePosition.node->leaf == false )
            {
                printf ( "Not leaf\n" );
            }
        }
        //
        // Case 2b: The child z that follows k has at least t keys
        //
        else if ( ( node->children[index + 1]->keysInUse > splitPoint ) )
        {
            TRACE ( "In btree_delete[2b]\n" );

            // Find the successor key k' in the subtree rooted at z
            sub_nodePosition =
                get_min_key_pos ( btree, node->children[index + 1] );

            // Replace k by k' in x
            node->dataRecords[index] =
                sub_nodePosition.node->dataRecords[sub_nodePosition.index];

            // Recursively delete k' from the subtree rooted at z
            btree_delete ( btree, node->children[index + 1],
                    node->dataRecords[index] );
            if ( sub_nodePosition.node->leaf == false )
            {
                printf ( "Not leaf\n" );
            }
        }
        //
        // Case 2c: both y and z have (t - 1) keys, so merge k and z into y, so
        //          x loses both k and the pointer to z, and y now contains
        //          (2t - 1) keys. Then free z, and recursively delete k from y
        //
        else if ( node->children[index]->keysInUse == splitPoint &&
                  node->children[index + 1]->keysInUse == splitPoint )
        {
            TRACE ( "In btree_delete[2c]\n" );

            //
            //  Go merge the siblings of this node at the specified index
            //  (which will remove the data record at node->dataRecords[index]
            //  from this node as well).
            //
            node = merge_siblings ( btree, node, index );

            goto del_loop;
        }
    }
    //
    //  Case 3: In this case start from the top of the tree and continue
    //          moving to the leaf node making sure that each node that we
    //          encounter on the way has at least 't' (order of the tree) keys
    //
    if ( node->leaf && ( node->keysInUse > splitPoint ) )
    {
        TRACE ( "In btree_delete[3]\n" );

        nodePosition.node = node;
        nodePosition.index = index;
        delete_key_from_node ( btree, &nodePosition );

        //
        //  Decrement the number of records in the btree.
        //
        --btree->count;

    }
    //
    //  All done so return to the caller.
    //
    return 0;
}

/**
*   Function used to get the node containing the given key
*   @param btree The btree to be searched
*   @param key The the key to be searched
*   @return The node and position of the key within the node
*/
nodePosition get_btree_node ( btree_t* btree, void* key )
{
    nodePosition nodePosition = { 0, 0 };
    bt_node_t*   node;
    unsigned int i;
    int          diff;

    TRACE ( "In get_btree_node\n" );

    //
    //  Start at the root of the tree...
    //
    node = btree->root;

    //
    //  Repeat forever...
    //
    while ( true )
    {
        //
        //  Initialize our position in this node to the first record of the
        //  node.
        //
        i = 0;

        //
        //  Search the records in this node beginning from the start until
        //  we find a record that is equal to or larger than the target...
        //
        while ( i < node->keysInUse )
        {
            //
            //  Go compare the target with the current data record.
            //
            diff = btree->compareCB ( key, node->dataRecords[i] );

            //
            //  If the comparison results in a value < 0 then we are done so
            //  just break out of this loop.
            //
            if ( diff < 0 )
            {
                break;
            }
            //
            //  If the record that we stopped on matches our target then return it
            //  to the caller.
            //
            if ( diff == 0 )
            {
                nodePosition.node  = node;
                nodePosition.index = i;
                return nodePosition;
            }
            i++;
        }
        //
        //  If the node is a leaf and if we did not find the target in it then
        //  return a NULL position to the caller for a "not found" indicator.
        //
        if ( node->leaf )
        {
            return nodePosition;
        }
        //
        //  Get the left child node and go up and try again in this node.
        //
        node = node->children[i];
    }
    //
    //  This point in the code should never be hit!
    //
    return nodePosition;
}

/**
*       Used to destory btree
*       @param btree The B-tree
*       @return none
*/
void btree_destroy ( btree_t* btree )
{
    int i = 0;

    TRACE ( "In btree_destroy\n" );

    bt_node_t* head;
    bt_node_t* tail;
    bt_node_t* child;
    bt_node_t* del_node;

    //
    //  Start at the root node...
    //
    head = tail = btree->root;

    //
    //  Repeat until the head pointer becomes NULL...
    //
    while ( head != NULL )
    {
        //
        //  If this is not a leaf node...
        //
        if ( ! head->leaf )
        {
            //
            //  For each record in this node...
            //
            //  ??? I think this is chaining all of the nodes into a list
            //  using the "next" pointer in each node before actually deleting
            //  any nodes.  I'm not sure if this is needed - Can't we just
            //  traverse the tree depth first and delete the nodes as we go?
            //
            for ( i = 0; i < head->keysInUse + 1; i++ )
            {
                child = head->children[i];
                tail->next = child;
                tail = child;
                child->next = NULL;
            }
        }
        //
        //  Grab the current list head for the delete that follows and then
        //  update the head to the next node in the list.
        //
        del_node = head;
        head = head->next;

        //
        //  Go delete the node that we saved above.
        //
        free_btree_node ( del_node );
    }
    MEM_FREE ( btree );
}

/**
*       Function used to search a node in a B-Tree
*       @param btree The B-tree to be searched
*       @param key Key of the node to be search
*       @return The key-value pair
*/
void* btree_search ( btree_t* btree, void* key )
{
    TRACE ( "In btree_search\n" );

    //
    //  Initialize the return data.
    //
    void* data = NULL;

    //
    //  Go find the given key in the tree...
    //
    nodePosition nodePos = get_btree_node ( btree, key );

    //
    //  If the target data was found in the tree, get the pointer to the data
    //  record to return to the caller.
    //
    if ( nodePos.node )
    {
        data = nodePos.node->dataRecords[nodePos.index];
    }
    //
    //  Return whatever we found to the caller.
    //
    return data;
}

/**
*   Get the max key in the btree
*   @param btree The btree
*   @return The max key
*/
void* btree_get_max ( btree_t* btree )
{
    nodePosition nodePosition;

    TRACE ( "In btree_get_max\n" );

    nodePosition = get_max_key_pos ( btree, btree->root );
    if ( nodePosition.node == NULL || nodePosition.index == (unsigned int)-1 )
    {
        return NULL;
    }
    return nodePosition.node->dataRecords[nodePosition.index];
}

/**
*   Get the min key in the btree
*   @param btree The btree
*   @return The min key
*/
void *btree_get_min ( btree_t* btree )
{
    nodePosition nodePosition;

    TRACE ( "In btree_get_min\n" );

    nodePosition = get_min_key_pos ( btree, btree->root );
    if ( nodePosition.node == NULL || nodePosition.index == (unsigned int)-1 )
    {
        return NULL;
    }
    return nodePosition.node->dataRecords[nodePosition.index];
}


//
//  This function will traverse the btree supplied and print all of the data
//  records in it in order starting with the minimum record.  This is
//  essentially a "dump" of the contents of the given btree subtree.
//
//  This function will be called by the "btree_traverse" API function with the
//  root node of the specified btree and then it will call itself recursively
//  to walk the tree.
//
static void btree_traverse_node ( bt_node_t* subtree, traverseFunc traverseCB )
{
    int        i;
    bt_node_t* node = subtree;

    TRACE ( "In btree_traverse_node\n" );

    //
    //  If this is not a leaf node...
    //
    if ( ! node->leaf )
    {
        //
        //  Loop through all of the sub nodes of the current node and process
        //  each subtree.
        //
        for ( i = 0; i < node->keysInUse; ++i )
        {
            //
            //  Go process all of the nodes in the left subtree of this node.
            //
            btree_traverse_node ( node->children[i], traverseCB );

            //
            //  Once the left subtree has been finished, display the current
            //  data record in this node.
            //
            traverseCB ( node->dataRecords[i] );
        }
        //
        //  Now that we have traversed the left subtree and the current node
        //  entry, go process the right subtree of this record in this node.
        //
        btree_traverse_node ( node->children[i], traverseCB );
    }
    //
    //  Otherwise, if this is a leaf node...
    //
    else
    {
        //
        //  Loop through all of the data records defined in this node and
        //  print them using the print callback function supplied by the user.
        //
        for ( i = 0; i < node->keysInUse; ++i )
        {
            traverseCB ( node->dataRecords[i] );
        }
    }
    return;
}


//
//  This is the user-facing API function that will walk the entire btree
//  supplied by the caller starting with the root node.  As each data record
//  is found in the tree, the user's supplied callback function will be invoked
//  with the pointer to the user data record that was just found.
//
//  The btree given will be walked in order starting with the data record that
//  has the minimum value until the entire tree has been processed.
//
extern void btree_traverse ( btree_t* tree, traverseFunc traverseCB )
{
    TRACE ( "In btree_traverse\n" );

    btree_traverse_node ( tree->root, traverseCB );
}


/*!----------------------------------------------------------------------------

    B t r e e   I t e r a t i o n   F u n c t i o n s

	This section contains the implementation of the btree iterator functions.

	Example usage (assuming the btree is populated with "userData" records):

		userData record = { 12, "" };

		btree_iter iter = btree_find ( &record );

		while ( ! btree_iter_at_end ( iter ) )
		{
            userData* returnedData = btree_iter_data ( iter );
			...
			btree_iter_next ( iter );
		}
		btree_iter_cleanup ( iter );

    The above example will find all of the records in the btree beginning with
    { 12, "" } to the end of the btree.

-----------------------------------------------------------------------------*/


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r a t o r _ n e w

	@brief Allocate and initialize a new iterator object.

	This function will allocate memory for a new iterator object and
    initialize the fields in that object appropriately.

	@param[in] btree - The address of the btree object to be operated on.
	@param[in] key - The address of the user's data object to be found in the
                     tree.

	@return The btree_iter object

-----------------------------------------------------------------------------*/
static btree_iter btree_iterator_new ( btree_t* btree, void* key )
{
    TRACE ( "In btree_iterator_new: btree[%p], key[%p]\n", btree, key );

    //
    //  Allocate a new iterator that we will return to the caller.
    //
    btree_iter iter = malloc ( sizeof(btree_iterator_t) );

    //
    //  If the memory allocation succeeded...
    //
    if ( iter != NULL )
    {
        //
        //  Initialize the fields of the iterator.
        //
        iter->btree = btree;
        iter->key   = key;
        iter->node  = 0;
        iter->index = 0;
    }
    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

	b t r e e _ f i n d

	@brief Position an iterator at the specified key location.

    The btree_find function is similar to the btree_search function except
    that it will find the smallest key that is greater than or equal to the
    specified value (rather than just the key that is equal to the specified
    value).

    This function is designed to be used for forward iterations using the
    iter->next function to initialize the user's iterator to the beginning of
    the forward range of values the user wishes to find.

    If the caller attempts to position the iterator past the end of the given
    btree, a null iterator will be returned.

    The iterator returned must be disposed of when the user has finished
    with it by calling the btree_iter_cleanup function.  If this is not done,
    there will be a memory leak in the user's program.

	@param[in] btree - The address of the btree object to be operated on.
	@param[in] key - The address of the user's data object to be found in the
                     tree.

	@return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_find ( btree_t* btree, void* key )
{
    TRACE ( "In btree_find: btree[%p], key[%p]\n", btree, key );

    bt_node_t* node  = 0;
    int        diff  = 0;
    int        i     = 0;
    int        index = 0;

    PRINT_DATA ( "  Looking up key:  ", key );

    //
    //  Go allocate and initialize a new iterator object.
    //
    btree_iter iter = btree_iterator_new ( btree, key );

    //
    //  If we were not able to allocate a new iterator, report the error and
    //  return a NULL iterator to the caller.
    //
    if ( iter == NULL )
    {
        printf ( "ERROR: Unable to allocate new iterator: %d[%s]\n", errno,
                 strerror ( errno ) );
        return iter;
    }
    //
    //  Start at the root of the tree...
    //
    node = btree->root;

    //
    //  Repeat forever...
    //
    while ( true )
    {
        //
        //  Initialize our position in this node to the first record of the
        //  node.  We will be searching the node from beginning to end.
        //
        i = 0;

        PRINT_DATA ( "  Current node is: ", node->dataRecords[i] );

        //
        //  Search the records in this node beginning from the beginning until
        //  we find a record that is less than the target...
        //
        while ( i < node->keysInUse )
        {
            //
            //  Save this node index for later processing.  Note that we do
            //  this because the value of "i" may or may not have been
            //  incremented past the end of the records array at the end of
            //  this loop depending on whether the loop terminates or we take
            //  an early out via the break condition.
            //
            index = i;

            //
            //  Go compare the target with the current data record and get the
            //  difference.  Note that this diff value cannot be used as a
            //  numeric "goodness" indicator because it is dependent on how
            //  the user has defined the comparison operator for this btree.
            //  The only thing that can be tested is the sign of this value.
            //
            diff = btree->compareCB ( key, node->dataRecords[i] );

            LOG ( "  Searching node at %d, diff: %d\n", i, diff );

            //
            //  If the record that we are looking at is less than or equal to
            //  our target then quit looking in this node.
            //
            if ( diff <= 0 )
            {
                break;
            }
            //
            //  Increment our index position and continue.
            //
            ++i;
        }
        //
        //  If we found an exact match to our key, just stop and return the
        //  iterator to the caller.
        //
        if ( diff == 0 )
        {
            LOG ( "  Diff == 0 - Returning node at i = %d\n", index );

            //
            //  Save the node and index where the match occurred in the
            //  iterator and terminate any further searching.
            //
            iter->node  = node;
            iter->index = index;

            break;
        }
        //
        //  If the difference is positive, we are going to have to go down the
        //  right subtree but we need to save the current position first.
        //
        else if ( diff > 0 )
        {
            //
            //  If this is a leaf node then we are at the end of the tree so
            //  just return the current iterator to the caller.
            //
            if ( node->leaf )
            {
                LOG ( "  Diff > 0 - Leaf - Returning node at i = %d\n", index );

                break;
            }
            //
            //  If this is not a leaf node, we will need to continue our
            //  search with the right child node of the current position.
            //
            //  If the comparison results in a diff value > 0 then the value
            //  we want might be in the right child so get the right child and
            //  continue our search.
            //
            else
            {
                LOG ( "  Diff > 0 - Not leaf - Moving to right child.\n" );
                PRINT_NODE ( btree, node, printFunction );

                node = getRightChild ( node, index );
            }
        }
        //
        //  Otherwise, if all the keys in this node are larger than our target
        //  then the node that we want must be in the left subtree.
        //
        else   // if ( diff < 0 )
        {
            //
            //  If this is a leaf node, then we are at the end of the tree so
            //  save the current node position in the iterator and stop our
            //  search.
            //
            if ( node->leaf )
            {
                LOG ( "  Diff < 0 - Leaf - Returning node at i = %d\n", index );

                iter->node  = node;
                iter->index = index;

                break;
            }
            //
            //  If this is not a leaf node, we will need to continue our
            //  search with the left child node of the current position.
            //
            //  If the comparison results in a diff value < 0 then the value
            //  we want might be in the left child so save the current
            //  position (in case nothing in the left child is less than what
            //  we found so far) get the left child and continue our search.
            //
            else
            {
                LOG ( "  Diff < 0 - Not leaf - Moving to left child.\n" );

                iter->node  = node;
                iter->index = index;

                node = getLeftChild ( node, index );
            }
        }
    }
    //
    //  We have finished our search.  If nothing in the tree is less than or
    //  equal to our target value then we need to return an "end" condition to
    //  the caller by returning a NULL iterator.
    //
    if ( iter->node == 0 )
    {
        LOG ( "  Found iterator end()\n" );

        //
        //  Go free up the memory allocated to our iterator since we won't be
        //  returning that value to the caller.
        //
        free ( iter );
        iter = NULL;
    }
    //
    //  If we have a valid iterator...
    //
    else
    {
        LOG ( "  Found index %d:\n", iter->index );
        PRINT_NODE ( btree, iter->node, printFunction );
    }
    return iter;
}


/*!----------------------------------------------------------------------------

	b t r e e _ r f i n d

	@brief Position an iterator at the specified key location.

    The btree_rfind function is similar to the btree_search function except
    that it will find the largest key that is less than or equal to the
    specified value (rather than just the key that is equal to the specified
    value).

    This function is designed to be used for reverse iterations using the
    iter->previous function to initialize the user's iterator to the beginning
    of the reverse range of values the user wishes to find.

	@param[in] btree - The address of the btree object to be operated on.
	@param[in] key - The address of the user's data object to be found in the
                     tree.

	@return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_rfind ( btree_t* btree, void* key )
{
    printf ( "Warning: btree_rfind was called but is not yet implemented!\n" );
    return NULL;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ b e g i n

	@brief Create an iterator positioned at the beginning of the btree.

	This function will find the smallest record currently defined in the
    specified btree and return an iterator to that record.  If the btree is
    empty, an iterator will be returned but it will be empty.  This iterator
    can be compared to the iter->end() iterator to determine if it empty.

    This function will return a new iterator object which the user MUST
    destroy (via btree_iter_cleanup()) when he is finished with it.

	@param[in] btree - The address of the btree object to be operated on.

	@return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_iter_begin ( btree_t* btree )
{
    btree_iter   iter;
    nodePosition nodePosition;

    TRACE ( "In btree_iter_begin\n" );

    //
    //  Go allocate and initialize a new iterator object.
    //
    iter = btree_iterator_new ( btree, 0 );

    //
    //  If we were not able to allocate a new iterator, report the error and
    //  return a NULL iterator to the caller.
    //
    if ( iter == NULL )
    {
        printf ( "ERROR: Unable to allocate new iterator: %d[%s]\n", errno,
                 strerror ( errno ) );
        return iter;
    }
    //
    //  Go get the minimum key currently defined in the btree.
    //
    nodePosition = get_min_key_pos ( btree, btree->root );

    //
    //  Save the tree position in the new iterator.
    //
    iter->index = nodePosition.index;
    iter->node  = nodePosition.node;
    iter->key   = nodePosition.node->dataRecords[nodePosition.index];

    LOG ( "  Found minimum record at index %d in node %p\n",
          nodePosition.index, nodePosition.node );

    PRINT_NODE ( btree, nodePosition.node, printFunction );

    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ e n d

	@brief Create an iterator positioned past the last record of the btree.

	This function will create a new iterator and set it to indicate that is is
    an "end" iterator.

    This function will return a new iterator object which the user MUST
    destroy (via btree_iter_cleanup()) when he is finished with it.

	@param[in] btree - The address of the btree object to be operated on.

	@return A btree_iter object

-----------------------------------------------------------------------------*/
btree_iter btree_iter_end ( btree_t* btree )
{
    btree_iter   iter;

    TRACE ( "In btree_iter_end\n" );

    //
    //  Go allocate and initialize a new iterator object.
    //
    iter = btree_iterator_new ( btree, 0 );

    //
    //  If we were not able to allocate a new iterator, report the error and
    //  return a NULL iterator to the caller.
    //
    if ( iter == NULL )
    {
        printf ( "ERROR: Unable to allocate new iterator: %d[%s]\n", errno,
                 strerror ( errno ) );
        return iter;
    }
    //
    //  Initialize the iterator to indicate the "end" position.
    //
    iter->index = -1;
    iter->node  = NULL;

    //
    //  Return the iterator to the caller.
    //
    return iter;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ n e x t

	@brief Get the "next" position in the btree.

    This function will position the specified iterator to the next higher key
    value in the btree from the current position.

    This function will use the position defined in the supplied iterator as
    the starting point in the btree and proceed to move to the "next" higher
    record in the btree.  Note that "next" is defined by the user's supplied
    comparison operator for this btree.

    TODO: This function needs to be cleaned up.  It was patched several times
    to make it work and left in a bit of a mess.

	@param[in,out] iter - The iterator to be updated

	@return None

-----------------------------------------------------------------------------*/
void btree_iter_next ( btree_iter iter )
{
    TRACE ( "In btree_iter_next: node[%p], index[%d]\n", iter->node, iter->index );

    //
    //  Set the internal variables so that we will start searching where the
    //  current iterator points.
    //
    btree_t*     btree = iter->btree;
    bt_node_t*   node  = iter->node;
    void*        key   = iter->key;
    void*        dataValue;
    int          diff  = 0;
    int          i     = iter->index;
    int          index = 0;
    nodePosition nodePos;

    //
    //  Print out the target key that we are starting at.
    //
    PRINT_DATA ( "  Looking up key:  ", key );

    //
    //  Repeat forever...
    //
    while ( true )
    {
        //
        //  If this is a leaf node...
        //
        if ( node->leaf )
        {
            //
            //  Loop through the records in this node until reach the end of
            //  the node...
            //
            i = 0;
            PRINT_DATA ( "  Current leaf node is: ", node->dataRecords[i] );
            while ( i < node->keysInUse )
            {
                //
                //  Compare the current record with our target.
                //
                index = i;
                diff = btree->compareCB ( key, node->dataRecords[i] );
                LOG ( "  Searching node at %d, diff: %d\n", i, diff );

                //
                //  If the current record is larger than the target we have
                //  found the "next" record in the btree so return it to the
                //  caller.
                //
                if ( diff < 0 )
                {
                    LOG ( "  Target record found.\n" );
                    iter->index = i;
                    iter->node  = node;
                    iter->key   = node->dataRecords[i];
                    return;
                }
                ++i;
            }
            //
            //  If none of the records in this node are larger than the
            //  target, go up one level in the btree to the parent node and
            //  try again.
            //
            //  If we are not at the root node...
            //
            if ( node->parent != NULL )
            {
                LOG ( "  Moving up to parent.\n" );
                i    = 0;
                node = node->parent;
            }
            //
            //  If we are at the root node then the target is larger than the
            //  largest record in the btree so return an "end" condition to
            //  the caller.
            //
            else
            {
                LOG ( "  End of tree found.\n" );
                iter->index = -1;
                iter->node  = NULL;
                iter->key   = NULL;
                return;
            }
            //
            //  Go back up and start looking at the records in the node...
            //
            continue;
        }
        //
        //  If the current node is an interior node (i.e. not a leaf node)...
        //
        i = 0;
        PRINT_DATA ( "  Current interior node is: ", node->dataRecords[i] );

        //
        //  Examine all of the records until we reach the end of the records
        //  in this node.
        //
        while ( i < node->keysInUse )
        {
            //
            //  Compare the current record with the target record.
            //
            index = i;
            diff = btree->compareCB ( key, node->dataRecords[i] );
            LOG ( "  Searching node at %d, diff: %d\n", i, diff );

            //
            //  If the current record is larger than the target...
            //
            if ( diff < 0 )
            {
                //
                //  Go find the smallest record in the left subtree of this
                //  record.
                //
                LOG ( "  Searching for minimum subtree key in left child.\n" );
                nodePos = get_min_key_pos ( btree, node->children[i] );

                //
                //  If this minimum value is larger than the target then that's
                //  the "next" record.
                //
                dataValue = getDataRecord ( nodePos.node, nodePos.index );
                diff = btree->compareCB ( key, dataValue );
                if ( diff < 0 )
                {
                    //
                    //  Return this record to the caller.
                    //
                    PRINT_DATA ( "    Found: ", nodePos.node );
                    iter->index = nodePos.index;
                    iter->node  = nodePos.node;
                    iter->key   = dataValue;
                    return;
                }
                //
                //  If the left subtree doesn't have the record we want then
                //  it must be the current record so return that to the
                //  caller.
                //
                else
                {
                    LOG ( "  Target record found.\n" );
                    iter->index = i;
                    iter->node  = node;
                    iter->key   = node->dataRecords[i];
                    return;
                }
            }
            ++i;
        }
        //
        //  If we have just exhausted the list of records in the current
        //  node...
        //
        if ( i == node->keysInUse )
        {
            //
            //  We need to decide if we need to go up or down the btree from
            //  here so find the largest record in the right subtree.
            //
            nodePos = get_max_key_pos ( btree, node->children[i] );
            dataValue = getDataRecord ( nodePos.node, nodePos.index );
            diff = btree->compareCB ( key, dataValue );

            //
            //  If there are no records in the right subtree that are larger
            //  than the target then we need to move up in the btree to the
            //  parent to continue our search.
            //
            if ( diff >= 0 )
            {
                //
                //  If this is not the root node then move up in the btree.
                //
                if ( node->parent != NULL )
                {
                    LOG ( "  Moving up to parent.\n" );
                    i    = 0;
                    node = node->parent;
                }
                //
                //  If this is the root node then the target record is larger
                //  than everything in the btree so return an "end" condition
                //  to the caller.
                //
                else
                {
                    LOG ( "  End of tree found.\n" );
                    iter->index = -1;
                    iter->node  = NULL;
                    iter->key   = NULL;
                    return;
                }
            }
            //
            //  There are some records in the right subtree greater than the
            //  target so go find the smallest of those records.
            //
            else
            {
                LOG ( "  Searching for minimum subtree key.\n" );
                nodePos = get_min_key_pos ( btree, node->children[i] );

                //
                //  If the smallest record in this subtree is larger than our
                //  target key then we have found the "next" record and we
                //  will return that to the caller.
                //
                dataValue = getDataRecord ( nodePos.node, nodePos.index );
                diff = btree->compareCB ( key, dataValue );
                if ( diff < 0 )
                {
                    PRINT_DATA ( "    Found: ", nodePos.node );
                    iter->index = nodePos.index;
                    iter->node  = nodePos.node;
                    iter->key   = dataValue;
                    return;
                }
                //
                //  If everything in the right subtree is smaller than our
                //  target, decend into the right tree and continue our search
                //  from there.
                //
                else
                {
                    LOG ( "  Moving down to the right child.\n" );
                    node = getRightChild ( node, index );
                    i    = 0;
                }
            }
        }
        continue;
    }
    //
    //  If we have found the "next" record, print that out in debug mode.
    //
    if ( iter->node != 0 )
    {
        LOG ( "  Found index %d:\n", iter->index );
        PRINT_NODE ( btree, iter->node, printFunction );
    }
    //
    //  Return to the caller now that we have updated the iterator to point
    //  to the "next" record in the btree.
    //
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ p r e v i o u s

	@brief Position the iterator at the previous key in the btree.

    This function will position the specified iterator to the next lower key
    value in the btree from the current position.

	@param[in,out] iter - The iterator to be updated

	@return None

-----------------------------------------------------------------------------*/
void btree_iter_previous ( btree_iter iter )
{
    printf ( "Warning: btree_iter_previous was called but is not yet "
             "implemented!\n" );
    return;
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ c m p

	@brief Compare 2 iterators.

	This function will compare the data records using the user supplied
    comparison function of the two supplied iterators.

	@param[in] iter1 - The first position to be tested.
	@param[in] iter2 - The second position to be tested.

	@return < 0  - iter2 is greater than iter1.
            == 0 - The iterators are identical.
            > 0  - iter2 is less than iter1.

-----------------------------------------------------------------------------*/
int btree_iter_cmp ( btree_iter iter1, btree_iter iter2 )
{
    return iter1->btree->compareCB ( iter1->node->dataRecords[iter1->index],
                                     iter2->node->dataRecords[iter2->index] );
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ a t _ e n d

	@brief Determine if this iterator is at the "end" condition.

	This function will determine if the input iterator is currently positioned
    at the "end" of the btree and return a "true" if it is.

	@param[in] iter - The iterator to be tested.

	@return true  - The iterator is at the "end" of the btree.
            false - The iterator is not at the "end" of the btree.

-----------------------------------------------------------------------------*/
bool btree_iter_at_end ( btree_iter iter )
{
    return ( iter == NULL || iter->node == NULL );
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ d a t a

	@brief Return the user data pointer at the current position.

	This function will return the pointer to the user data structure that is
    stored in the btree at the current iterator position.  The user data
    structure address is the "value" portion of the key/value pairs that are
    stored in the btree.

    The iterator must have already been positioned somewhere in the btree
    using one of the positioning functions like btree_iter_find or a call to
    btree_iter_next, etc.

	@param[in] iter - The current btree iterator.

	@return The address of the user data at the current iterator position.
            NULL if the iterator has not been initialized.

-----------------------------------------------------------------------------*/
void* btree_iter_data ( btree_iter iter )
{
    //
    //  If this is not a valid iterator, return a NULL to the caller.
    //
    if ( btree_iter_at_end ( iter ) )
    {
        return NULL;
    }
    //
    //  Return the user data pointer to the caller.
    //
    return iter->node->dataRecords[iter->index];
}


/*!----------------------------------------------------------------------------

    b t r e e _ i t e r _ c l e a n u p

	@brief Release all resources held by an iterator.

    This function will clean up the specified iterator and release any
    resources being used by this iterator.  This function MUST be called when
    the user is finished using an iterator or resources (such as memory
    blocks) will be leaked to the system.  Once this function has been called
    with an iterator, that iterator may not be used again for any of the btree
    functions unless it has subsequently been reinitialized using one of the
    iterator initialization functions defined here.

	@param[in] iter - The current btree iterator.

	@return None

-----------------------------------------------------------------------------*/
void btree_iter_cleanup ( btree_iter iter )
{
    //
    //  If this iterator is valid...
    //
    if ( iter != 0 )
    {
        //
        //  Set the iterator contents to "empty" just in case someone tries to
        //  use this iterator again after this call has freed it.
        //
        iter->btree = 0;
        iter->key   = 0;
        iter->node  = 0;
        iter->index = 0;

        //
        //  Go return this iterator data structure to the system memory pool.
        //
        free ( iter );
    }
    //
    //  Return to the caller.
    //
    return;
}


#ifdef BTREE_DEBUG

/**
*   Used to print the keys of the bt_node
*   @param node The node whose keys are to be printed
*   @return none
*/

//
//  This is a helper function that will generate a relatively small integer
//  value from the difference between the current tree node address and the
//  base of the tree address.  This is being done to make it easier to
//  identify nodes in the dump routines that follow.  The integer values
//  generated don't have any meaning other than just a way to identify a node.
//
//  Note that the macro that follows is just a short way of specifying the
//  pair of "printf" arguments to print the actual node address followed by
//  this node identifier.
//
static long int offset ( btree_t* btree, bt_node_t* node )
{
    if ( node == 0 )
    {
        return 0;
    }
    long int diff = (long)node - (long)btree;

    diff = diff / sizeof(struct bt_node_t);

    return diff;
}

#define NODE( node ) node, offset ( btree, node )

//
//  Print the information about a single btree node.
//
static void print_single_node ( btree_t* btree, bt_node_t* node,
                                printFunc printCB )
{
    int i;

    printf ( "\n  Node[%p:%ld]\n",        NODE ( node ) );
    printf ( "    next.......: %p:%ld\n", NODE ( node->next ) );
    printf ( "    parent.....: %p:%ld\n", NODE ( node->parent ) );
    printf ( "    leaf.......: %d\n",     node->leaf );
    printf ( "    keysInUse..: %d\n",     node->keysInUse );
    printf ( "    level......: %d\n",     node->level );
    printf ( "    dataRecords: %p\n",     node->dataRecords );
    printf ( "    children...: %p\n",     node->children );

    i = 0;
    while ( i < node->keysInUse )
    {
        printf ( "      left[%p:%ld], right[%p:%ld], ",
                 NODE ( node->children[i] ), NODE ( node->children[i + 1] ) );

        if ( printCB != NULL )
        {
            printCB ( "", node->dataRecords[i] );
        }
        ++i;
    }
    fflush ( stdout );
}
#endif      // ifdef BTREE_DEBUG

/**
*       Function used to print the B-tree
*       @param root Root of the B-Tree
*       @param print_key Function used to print the key value
*       @return none
*/

void print_subtree ( btree_t* btree, bt_node_t* node, printFunc printCB )
{
#ifdef BTREE_DEBUG
    int i = 0;
    unsigned int current_level;

    bt_node_t* head;
    bt_node_t* tail;
    bt_node_t* child;

    current_level = node->level;
    head = node;
    tail = node;

    printf ( "Btree [%p]\n", btree );
    printf ( "  order.........: %u\n", btree->order );
    printf ( "  fullSize......: %u\n", btree->nodeFullSize );
    printf ( "  sizeofKeys....: %u\n", btree->sizeofKeys );
    printf ( "  sizeofPointers: %u\n", btree->sizeofPointers );
    printf ( "  recordCount   : %u\n", btree->count );
    printf ( "  root..........: %p:%ld\n", NODE ( btree->root ) );
    printf ( "  compFunc......: %p\n", btree->compareCB );

    //
    //  Traverse the nodes of the btree displaying them as we go.
    //
    while ( head != NULL )
    {
        if ( head->level < current_level )
        {
            current_level = head->level;
        }
        print_single_node ( btree, head, printCB );

        if ( ! head->leaf )
        {
            for ( i = 0; i < head->keysInUse + 1; i++ )
            {
                child = head->children[i];
                tail->next = child;
                tail = child;
                child->next = NULL;
            }
        }
        head = head->next;
    }
    printf ( "\n" );
#endif      // ifdef BTREE_DEBUG
}


#ifdef EXAMPLE_ONLY
/*!----------------------------------------------------------------------------

    The following functions are here as an example of a set of "validation"
    functions that have been used to validate the structure of a btree.  These
    are here merely as an example.

    Note that all of these functions rely on the fact that the "domainId"
    field of the user data record is an integer value that corresponds to the
    "record number" in the btree.  In other words, when the 4th user record is
    created, the domainId is set to 3 (since the counters start at zero).

-----------------------------------------------------------------------------*/
//
//  Validation functions.
//
static int  validateCount = 0;
static int* validateIds   = 0;


static void validateReset ( void )
{
    for ( int i = 0; i < validateCount; ++i )
    {
        validateIds[i] = 0;
    }
}


static void validateInit ( int maxSize )
{
    validateCount = maxSize;
    validateIds   = malloc ( maxSize * sizeof(int) );

    validateReset();
}


static void validateRecord ( void* recordPtr )
{
    userData* data = recordPtr;
    int       item = data->domainId;

    validateIds[item]++;
}


static int validateReport ( bool reportMissing )
{
    int errorCount = 0;

    for ( int i = 0; i < validateCount; ++i )
    {
        if ( reportMissing )
        {
            if ( validateIds[i] == 0 )
            {
                printf ( "ERROR: validation found missing record %d.\n", i );
                errorCount++;
            }
        }
        else
        {
            if ( validateIds[i] != 0 )
            {
                printf ( "ERROR: validation found record %d.\n", i );
                errorCount++;
            }
        }
        if ( validateIds[i] > 1 )
        {
            printf ( "ERROR: validation found record %d duplicated %d "
                     "time(s).\n", i, validateIds[i] - 1 );
            errorCount++;
        }
    }
    return errorCount;
}


static void validateCleanup ( void )
{
    validateCount = 0;
    free ( validateIds );
    validateIds = 0;
}


static void validateBtree ( btree_t* btree, int maxSize, bool reportMissing )
{
    int errorCount = 0;

    printf ( "Validating tree: %p, size: %d, reportMissing: %d\n",
             btree, maxSize, reportMissing );

    if ( validateCount == 0 )
    {
        validateInit ( maxSize );
    }
    else
    {
        validateReset();
    }
    btree_traverse ( btree, validateRecord );

    errorCount = validateReport ( reportMissing );

    printf ( "Validation found %d error(s).\n", errorCount );

    validateCleanup();
}

#endif      // ifdef EXAMPLE_ONLY
