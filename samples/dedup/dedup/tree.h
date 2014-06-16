/* Based on Data Structures and Algorithm Analysis in C (Second Edition)
 * by Mark Allen Weiss.
 *
 * Modified by Minlan Yu.
 */
        #ifndef _Tree_H
        #define _Tree_H

#include "dedupdef.h"
#include "binheap.h"

struct tree_element {
  u_int32 aid;
  PriorityQueue queue;
};

        typedef struct tree_element *TreeElementType;

        struct TreeNode;
        typedef struct TreeNode *Position;
        typedef struct TreeNode *SearchTree;
        struct TreeNode
        {
            TreeElementType Element;
            SearchTree  Left;
            SearchTree  Right;
        };

        SearchTree TreeMakeEmpty( SearchTree T );
        Position TreeFind( int value, SearchTree T );
        Position TreeFindMin( SearchTree T );
        Position TreeFindMax( SearchTree T );
        SearchTree TreeInsert( TreeElementType X, SearchTree T );
        SearchTree TreeDelete( TreeElementType X, SearchTree T );
        TreeElementType TreeRetrieve( Position P );

        #endif  /* _Tree_H */
