/*
  Red Black balanced tree library

    > Created (Julienne Walker): August 23, 2003
    > Modified (Julienne Walker): March 14, 2008
*/
#include "compat.h"
#include "rbtree-ec.h"

#include <assert.h>
#ifdef __cplusplus
#include <cstdlib>

using std::malloc;
using std::free;
using std::size_t;
#else
#include <stdlib.h>
#endif

#ifndef HEIGHT_LIMIT
#define HEIGHT_LIMIT 64 /* Tallest allowable tree */
#endif

static int node_cmp( const jsw_rbnode_t *n1, const jsw_rbnode_t *n2 )
{
  return n2->data.key - n1->data.key;
}

struct jsw_rbtrav {
  jsw_rbtree_t *tree;               /* Paired tree */
  jsw_rbnode_t *it;                 /* Current node */
  jsw_rbnode_t *path[HEIGHT_LIMIT]; /* Traversal path */
  size_t        top;                /* Top of stack */
};

/**
  <summary>
  Checks the color of a red black node
  <summary>
  <param name="root">The node to check</param>
  <returns>1 for a red node, 0 for a black node</returns>
  <remarks>For jsw_rbtree.c internal use only</remarks>
*/
static int is_red ( jsw_rbnode_t *root )
{
  return root != NULL && root->red == 1;
}

/**
  <summary>
  Performs a single red black rotation in the specified direction
  This function assumes that all nodes are valid for a rotation
  <summary>
  <param name="root">The original root to rotate around</param>
  <param name="dir">The direction to rotate (0 = left, 1 = right)</param>
  <returns>The new root ater rotation</returns>
  <remarks>For jsw_rbtree.c internal use only</remarks>
*/
static jsw_rbnode_t *jsw_single ( jsw_rbnode_t *root, int dir )
{
  jsw_rbnode_t *save = root->link[!dir];

  root->link[!dir] = save->link[dir];
  save->link[dir] = root;

  root->red = 1;
  save->red = 0;

  return save;
}

/**
  <summary>
  Performs a double red black rotation in the specified direction
  This function assumes that all nodes are valid for a rotation
  <summary>
  <param name="root">The original root to rotate around</param>
  <param name="dir">The direction to rotate (0 = left, 1 = right)</param>
  <returns>The new root after rotation</returns>
  <remarks>For jsw_rbtree.c internal use only</remarks>
*/
static jsw_rbnode_t *jsw_double ( jsw_rbnode_t *root, int dir )
{
  root->link[!dir] = jsw_single ( root->link[!dir], !dir );

  return jsw_single ( root, dir );
}

static jsw_rbnode_t *new_node ( jsw_rbnode_t *rn )
{
  rn->red = 1;
  rn->link[0] = rn->link[1] = NULL;

  return rn;
}

/**
  <summary>
  Search for a copy of the specified
  node data in a red black tree
  <summary>
  <param name="tree">The tree to search</param>
  <param name="data">The data value to search for</param>
  <returns>
  A pointer to the data value stored in the tree,
  or a null pointer if no data could be found
  </returns>
*/
void *jsw_rbfind ( jsw_rbtree_t *tree, jsw_rbnode_t *node )
{
  jsw_rbnode_t *it = tree->root;

  while ( it != NULL ) {
    int cmp = node_cmp( it, node );

    if ( cmp == 0 )
      break;

    /*
      If the tree supports duplicates, they should be
      chained to the right subtree for this to work
    */
    it = it->link[cmp < 0];
  }

  return it == NULL ? NULL : it;
}

/**
  <summary>
  Insert a copy of the user-specified
  data into a red black tree
  <summary>
  <param name="tree">The tree to insert into</param>
  <param name="data">The data value to insert</param>
  <returns>
  1 if the value was inserted successfully,
  0 if the insertion failed for any reason
  </returns>
*/
int jsw_rbinsert ( jsw_rbtree_t *tree, jsw_rbnode_t *node )
{
  if ( tree->root == NULL ) {
    /*
      We have an empty tree; attach the
      new node directly to the root
    */
    tree->root = new_node ( node );

    if ( tree->root == NULL )
      return 0;
  }
  else {
    jsw_rbnode_t head = { .red = 0 }; /* False tree root */
    jsw_rbnode_t *g, *t;     /* Grandparent & parent */
    jsw_rbnode_t *p, *q;     /* Iterator & parent */
    int dir = 0, last = 0;

    /* Set up our helpers */
    t = &head;
    g = p = NULL;
    q = t->link[1] = tree->root;

    /* Search down the tree for a place to insert */
    for ( ; ; ) {
      if ( q == NULL ) {
        /* Insert a new node at the first null link */
        p->link[dir] = q = new_node ( node );

        if ( q == NULL )
          return 0;
      }
      else if ( is_red ( q->link[0] ) && is_red ( q->link[1] ) ) {
        /* Simple red violation: color flip */
        q->red = 1;
        q->link[0]->red = 0;
        q->link[1]->red = 0;
      }

      if ( is_red ( q ) && is_red ( p ) ) {
        /* Hard red violation: rotations necessary */
        int dir2 = t->link[1] == g;

        if ( q == p->link[last] )
          t->link[dir2] = jsw_single ( g, !last );
        else
          t->link[dir2] = jsw_double ( g, !last );
      }

      /*
        Stop working if we inserted a node. This
        check also disallows duplicates in the tree
      */
      if ( node_cmp ( q, node ) == 0 )
        break;

      last = dir;
      dir = node_cmp ( q, node ) < 0;

      /* Move the helpers down */
      if ( g != NULL )
        t = g;

      g = p, p = q;
      q = q->link[dir];
    }

    /* Update the root (it may be different) */
    tree->root = head.link[1];
  }

  /* Make the root black for simplified logic */
  tree->root->red = 0;

  return 1;
}

/**
  <summary>
  Remove a node from a red black tree
  that matches the user-specified data
  <summary>
  <param name="tree">The tree to remove from</param>
  <param name="data">The data value to search for</param>
  <returns>
  1 if the value was removed successfully,
  0 if the removal failed for any reason
  </returns>
  <remarks>
  The most common failure reason should be
  that the data was not found in the tree
  </remarks>
*/
int jsw_rberase ( jsw_rbtree_t *tree, jsw_rbnode_t *node )
{
  if ( tree->root != NULL ) {
    jsw_rbnode_t head = { .red = 0 }; /* False tree root */
    jsw_rbnode_t *q, *p, *g; /* Helpers */
    jsw_rbnode_t *f = NULL;  /* Found item */
    int dir = 1;

    /* Set up our helpers */
    q = &head;
    g = p = NULL;
    q->link[1] = tree->root;

    /*
      Search and push a red node down
      to fix red violations as we go
    */
    while ( q->link[dir] != NULL ) {
      int last = dir;

      /* Move the helpers down */
      g = p, p = q;
      q = q->link[dir];
      dir = node_cmp ( q, node ) < 0;

      /*
        Save the node with matching data and keep
        going; we'll do removal tasks at the end
      */
      if ( node_cmp ( q, node ) == 0 )
        f = q;

      /* Push the red node down with rotations and color flips */
      if ( !is_red ( q ) && !is_red ( q->link[dir] ) ) {
        if ( is_red ( q->link[!dir] ) )
          p = p->link[last] = jsw_single ( q, dir );
        else if ( !is_red ( q->link[!dir] ) ) {
          jsw_rbnode_t *s = p->link[!last];

          if ( s != NULL ) {
            if ( !is_red ( s->link[!last] ) && !is_red ( s->link[last] ) ) {
              /* Color flip */
              p->red = 0;
              s->red = 1;
              q->red = 1;
            }
            else {
              int dir2 = g->link[1] == p;

              if ( is_red ( s->link[last] ) )
                g->link[dir2] = jsw_double ( p, last );
              else if ( is_red ( s->link[!last] ) )
                g->link[dir2] = jsw_single ( p, last );

              /* Ensure correct coloring */
              q->red = g->link[dir2]->red = 1;
              g->link[dir2]->link[0]->red = 0;
              g->link[dir2]->link[1]->red = 0;
            }
          }
        }
      }
    }

    /* Replace and remove the saved node */
    if ( f != NULL ) {
      test_data tmp = f->data;

      f->data = q->data;
      q->data = tmp;

      p->link[p->link[1] == q] =
        q->link[q->link[0] == NULL];
    }

    /* Update the root (it may be different) */
    tree->root = head.link[1];

    /* Make the root black for simplified logic */
    if ( tree->root != NULL )
      tree->root->red = 0;
  }

  return 1;
}

/**
  <summary>
  Create a new traversal object
  <summary>
  <returns>A pointer to the new object</returns>
  <remarks>
  The traversal object is not initialized until
  jsw_rbtfirst or jsw_rbtlast are called.
  The pointer must be released with jsw_rbtdelete
  </remarks>
*/
jsw_rbtrav_t *jsw_rbtnew ( void )
{
  return (jsw_rbtrav_t*)malloc ( sizeof ( jsw_rbtrav_t ) );
}

/**
  <summary>
  Release a traversal object
  <summary>
  <param name="trav">The object to release</param>
  <remarks>
  The object must have been created with jsw_rbtnew
  </remarks>
*/
void jsw_rbtdelete ( jsw_rbtrav_t *trav )
{
  free ( trav );
}

/**
  <summary>
  Initialize a traversal object. The user-specified
  direction determines whether to begin traversal at the
  smallest or largest valued node
  <summary>
  <param name="trav">The traversal object to initialize</param>
  <param name="tree">The tree that the object will be attached to</param>
  <param name="dir">
  The direction to traverse (0 = ascending, 1 = descending)
  </param>
  <returns>A pointer to the smallest or largest data value</returns>
  <remarks>For jsw_rbtree.c internal use only</remarks>
*/
static void *start ( jsw_rbtrav_t *trav, jsw_rbtree_t *tree, int dir )
{
  trav->tree = tree;
  trav->it = tree->root;
  trav->top = 0;

  /* Save the path for later traversal */
  if ( trav->it != NULL ) {
    while ( trav->it->link[dir] != NULL ) {
      trav->path[trav->top++] = trav->it;
      trav->it = trav->it->link[dir];
    }
  }

  return trav->it == NULL ? NULL : trav->it;
}

/**
  <summary>
  Traverse a red black tree in the user-specified direction
  <summary>
  <param name="trav">The initialized traversal object</param>
  <param name="dir">
  The direction to traverse (0 = ascending, 1 = descending)
  </param>
  <returns>
  A pointer to the next data value in the specified direction
  </returns>
  <remarks>For jsw_rbtree.c internal use only</remarks>
*/
static void *move ( jsw_rbtrav_t *trav, int dir )
{
  if ( trav->it->link[dir] != NULL ) {
    /* Continue down this branch */
    trav->path[trav->top++] = trav->it;
    trav->it = trav->it->link[dir];

    while ( trav->it->link[!dir] != NULL ) {
      trav->path[trav->top++] = trav->it;
      trav->it = trav->it->link[!dir];
    }
  }
  else {
    /* Move to the next branch */
    jsw_rbnode_t *last;

    do {
      if ( trav->top == 0 ) {
        trav->it = NULL;
        break;
      }

      last = trav->it;
      trav->it = trav->path[--trav->top];
    } while ( last == trav->it->link[dir] );
  }

  return trav->it == NULL ? NULL : trav->it;
}

/**
  <summary>
  Initialize a traversal object to the smallest valued node
  <summary>
  <param name="trav">The traversal object to initialize</param>
  <param name="tree">The tree that the object will be attached to</param>
  <returns>A pointer to the smallest data value</returns>
*/
void *jsw_rbtfirst ( jsw_rbtrav_t *trav, jsw_rbtree_t *tree )
{
  return start ( trav, tree, 0 ); /* Min value */
}

/**
  <summary>
  Initialize a traversal object to the largest valued node
  <summary>
  <param name="trav">The traversal object to initialize</param>
  <param name="tree">The tree that the object will be attached to</param>
  <returns>A pointer to the largest data value</returns>
*/
void *jsw_rbtlast ( jsw_rbtrav_t *trav, jsw_rbtree_t *tree )
{
  return start ( trav, tree, 1 ); /* Max value */
}

/**
  <summary>
  Traverse to the next value in ascending order
  <summary>
  <param name="trav">The initialized traversal object</param>
  <returns>A pointer to the next value in ascending order</returns>
*/
void *jsw_rbtnext ( jsw_rbtrav_t *trav )
{
  return move ( trav, 1 ); /* Toward larger items */
}

/**
  <summary>
  Traverse to the next value in descending order
  <summary>
  <param name="trav">The initialized traversal object</param>
  <returns>A pointer to the next value in descending order</returns>
*/
void *jsw_rbtprev ( jsw_rbtrav_t *trav )
{
  return move ( trav, 0 ); /* Toward smaller items */
}
