/* A splay-tree datatype.
   Copyright (C) 1998-2020 Free Software Foundation, Inc.
   Contributed by Mark Mitchell (mark@markmitchell.com).
   This file is part of the GNU Offloading and Multi Processing Library
   (libgomp).
   Libgomp is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3, or (at your option)
   any later version.
   Libgomp is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
   more details.
   Under Section 7 of GPL version 3, you are granted additional
   permissions described in the GCC Runtime Library Exception, version
   3.1, as published by the Free Software Foundation.
   You should have received a copy of the GNU General Public License and
   a copy of the GCC Runtime Library Exception along with this program;
   see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
   <http://www.gnu.org/licenses/>.  */

/* The splay tree code copied from include/splay-tree.h and adjusted,
   so that all the data lives directly in splay_tree_node_s structure
   and no extra allocations are needed.  */

/* For an easily readable description of splay-trees, see:
     Lewis, Harry R. and Denenberg, Larry.  Data Structures and Their
     Algorithms.  Harper-Collins, Inc.  1991.
   The major feature of splay trees is that all basic tree operations
   are amortized O(log n) time for a tree with n nodes.  */



#include <nautilus/nautilus.h>
#include "mm_splay_tree.h"


/* Rotate the edge joining the left child N with its parent P.  PP is the
   grandparents' pointer to P.  */

int mm_overlap_helper(nk_aspace_region_t * regionA, nk_aspace_region_t * regionB){
    void * VA_start_A = regionA->va_start;
    void * VA_start_B = regionB->va_start;
    void * VA_end_A = regionA->va_start + regionA->len_bytes;
    void * VA_end_B = regionB->va_start + regionB->len_bytes;

    if (VA_start_A <= VA_start_B && VA_start_B < VA_end_A) {
        return 1;
    }
    if (VA_start_B <= VA_start_A && VA_start_A < VA_end_B) {
        return 1;
    }

    return 0;
}

static inline void
mm_rotate_left (mm_splay_tree_node_t *pp, mm_splay_tree_node_t *p, mm_splay_tree_node_t *n)
{
  mm_splay_tree_node_t tmp;
  tmp = n->right;
  n->right = p;
  p->left = tmp;
  pp = n;
}

/* Rotate the edge joining the right child N with its parent P.  PP is the
   grandparents' pointer to P.  */

static inline void
mm_rotate_right (mm_splay_tree_node_t *pp, mm_splay_tree_node_t *p, mm_splay_tree_node_t *n)
{
  mm_splay_tree_node_t tmp;
  tmp = n->left;
  n->left = p;
  p->right = tmp;
  pp = n;
}

/* Bottom up splay of KEY.  */

static void
mm_splay_tree_splay (mm_splay_tree_t *sp, mm_splay_tree_key_t *key)
{
  if (sp->root == NULL)
    return;

  do {
    int cmp1, cmp2;
    mm_splay_tree_node_t *n, *c;

    n = sp->root;
    cmp1 = mm_splay_compare (key, n->key);

    /* Found.  */
    if (cmp1 == 0)
      return;

    /* Left or right?  If no child, then we're done.  */
    if (cmp1 < 0)
      c = n->left;
    else
      c = n->right;
    if (!c)
      return;

    /* Next one left or right?  If found or no child, we're done
       after one rotation.  */
    cmp2 = splay_compare (key, &c->key);
    if (cmp2 == 0
	|| (cmp2 < 0 && !c->left)
	|| (cmp2 > 0 && !c->right))
      {
	if (cmp1 < 0)
	  rotate_left (&sp->root, n, c);
	else
	  rotate_right (&sp->root, n, c);
	return;
      }

    /* Now we have the four cases of double-rotation.  */
    if (cmp1 < 0 && cmp2 < 0)
      {
	rotate_left (&n->left, c, c->left);
	rotate_left (&sp->root, n, n->left);
      }
    else if (cmp1 > 0 && cmp2 > 0)
      {
	rotate_right (&n->right, c, c->right);
	rotate_right (&sp->root, n, n->right);
      }
    else if (cmp1 < 0 && cmp2 > 0)
      {
	rotate_right (&n->left, c, c->right);
	rotate_left (&sp->root, n, n->left);
      }
    else if (cmp1 > 0 && cmp2 < 0)
      {
	rotate_left (&n->right, c, c->left);
	rotate_right (&sp->root, n, n->right);
      }
  } while (1);
}

/* Insert a new NODE into SP.  The NODE shouldn't exist in the tree.  */

attribute_hidden void
mm_splay_tree_insert (mm_splay_tree_t * sp, mm_splay_tree_node_t * node)
{
  int comparison = 0;

  mm_splay_tree_splay (sp, node->key);

  if (sp->root)
    comparison = mm_splay_compare (&sp->root->key, &node->key);

  if (sp->root && comparison == 0) {
    DEBUG_PRINT("Duplicate node\n");
    return -1;
  } else
    {
      /* Insert it at the root.  */
      if (sp->root == NULL)
	node->left = node->right = NULL;
      else if (comparison < 0)
	{
	  node->left = sp->root;
	  node->right = node->left->right;
	  node->left->right = NULL;
	}
      else
	{
	  node->right = sp->root;
	  node->left = node->right->left;
	  node->right->left = NULL;
	}

      sp->root = node;
    }
}

/* Remove node with KEY from SP.  It is not an error if it did not exist.  */

attribute_hidden void
mm_splay_tree_remove (mm_splay_tree_t *sp, mm_splay_tree_key_t * key)
{
  splay_tree_splay (sp, key);

  if (sp->root && splay_compare (sp->root->key, key) == 0)
    {
      mm_splay_tree_node_t * left, * right;

      left = sp->root->left;
      right = sp->root->right;

      /* One of the children is now the root.  Doesn't matter much
	 which, so long as we preserve the properties of the tree.  */
      if (left)
	{
	  sp->root = left;

	  /* If there was a right child as well, hang it off the
	     right-most leaf of the left child.  */
	  if (right)
	    {
	      while (left->right)
		left = left->right;
	      left->right = right;
	    }
	}
      else
	sp->root = right;
    }
}

/* Lookup KEY in SP, returning NODE if present, and NULL
   otherwise.  */

attribute_hidden nk_aspace_region_t*
mm_splay_tree_lookup (mm_splay_tree_t * sp, mm_splay_tree_key_t * key)
{
  mm_splay_tree_splay (sp, key);

  if (sp->root && mm_splay_compare (sp->root->key, key) == 0)
    return  &(sp->root->region);
  else
    return NULL;
}

/* Helper function for splay_tree_foreach.
   Run FUNC on every node in KEY.  */

static void
splay_tree_foreach_internal (mm_splay_tree_node_t node, mm_splay_tree_callback_t func,
			     void *data)
{
  if (!node)
    return;
  func (&node->key, data);
  splay_tree_foreach_internal (node->left, func, data);
  /* Yeah, whatever.  GCC can fix my tail recursion.  */
  splay_tree_foreach_internal (node->right, func, data);
}

/* Run FUNC on each of the nodes in SP.  */

attribute_hidden void
splay_tree_foreach (mm_splay_tree_t sp, mm_splay_tree_callback_t func, void *data)
{
  splay_tree_foreach_internal (sp->root, func, data);
}


int mm_splay_tree_init(mm_splay_tree_t * spt){
    mm_struct_init(& (spt -> super));
    
    vtbl * vptr = (vtbl *) malloc (sizeof(vtbl));
    vptr->insert = &mm_splay_tree_insert;
    vptr->show = &mm_llist_show;
    vptr->check_overlap = &mm_llist_check_overlap;
    vptr->remove = &mm_llist_remove;
    vptr->contains = &mm_llist_contains;
    vptr->find_reg_at_addr = &mm_llist_find_reg_at_addr;
    vptr->update_region = &mm_llist_update_region;

    spt->super.vptr = vptr;
    spt->region_head = NULL;

    return 0;
}


