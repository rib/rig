/*
 * Copyright (c) 2004, 2007 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file rbtree.h
 * @brief RedBlack tree library
 *
 * This library implements the algorithm for red-black trees, as
 * described at http://en.wikipedia.org/wiki/Red%E2%80%93black_tree
 * Red-Black trees provide insert, find and delete in O(log n) time
 * in O(n) space. Inorder, preorder and postorder iteration is done
 * in O(n) time.
 *
 * This implementation was originally written by Todd C. Miller
 * <todd.Miller@courtesan.com> and was taken from Apple's "sudo"
 * program. It has been modified to match the libnaemon naming
 * scheme and naemon's (future) usage of it as a replacement for
 * the horrible skiplists, which provide the same operations as
 * red-black trees, but are an order of magnitude slower in the
 * worst case; in reality, skiplists are 50% slower than red-black
 * trees, but use more memory than red-black trees to guarantee
 * that (worse) performance.
 */

#ifndef _RB_TREE_H_
#define _RB_TREE_H_

#include <clib.h>

enum c_rbcolor {
    c_rbtree_red,
    c_rbtree_black
};

enum c_rbtree_traversal {
    c_rbpreorder,
    c_rbinorder,
    c_rbpostorder
};

struct c_rbnode {
	struct c_rbnode *left, *right, *parent;
	void *data;
	enum c_rbcolor color;
};

struct c_rbtree {
	int (*compar)(const void *, const void *);
	struct c_rbnode root;
	struct c_rbnode nil;
	unsigned int num_nodes;
};

/** Obtain the first node of the tree */
#define c_rbtree_first(t)		((t)->root.left)
/** Traverse an entire tree */
#define c_rbtree_traverse(t, f, c, o)	c_rbtree_traverse_node((t), c_rbtree_first(t), (f), (c), (o))
/** Determine if a tree is empty */
#define c_rbtree_isempty(t)		((t)->root.left == &(t)->nil && (t)->root.right == &(t)->nil)
/** Obtain the root of the tree */
#define c_rbtree_root(t)		(&(t)->root)

C_BEGIN_DECLS

/**
 * Delete a node from the tree
 */
void *c_rbtree_delete(struct c_rbtree *, struct c_rbnode *);

/**
 * Traverse a tree from the given node
 *
 * "func" is called once for each node under the given node in
 * the given tree, being passed "cookie" every time. The order
 * is selected from c_rbtree_traversal enum and can be any of
 * c_rbpreorder, c_rbinorder and c_rbpostorder. Traversal stops when
 * "func" returns non-zero.
 *
 * @param tree Tree to traverse
 * @param node Node to start from
 * @param func Function to call for each node under "node"
 * @param void cookie Cookie to pass to "func"
 * @param rbtraversal_order c_rbpreorder, c_rbinorder or c_rbpostorder
 * @return Whatever the last called "func" returns.
 */
int c_rbtree_traverse_node(struct c_rbtree *tree, struct c_rbnode *node,
                           int (*func)(void *, void *),
                           void *cookie, enum c_rbtree_traversal);
/**
 * Find a node in the tree
 *
 * @param tree Tree to look in
 * @param key What to look for
 * @return The redblack node containing the data searched for
 */
struct c_rbnode *c_rbtree_find_node(struct c_rbtree *tree, void *key);

/**
 * Insert a node in the tree
 *
 * @param tree Tree to insert in
 * @param data Data to insert
 */
struct c_rbnode *c_rbtree_insert(struct c_rbtree *tree, void *data);

/**
 * Create a new red-black tree
 *
 * @param compar Comparison function to order the tree
 * @return The newly created tree
 */
struct c_rbtree *c_rbtree_create(int (*compar)(const void *, const void *));

/**
 * Find data in the tree
 *
 * @param tree Tree to look in
 * @param key Key to find
 * @return Found data, if any, or NULL if none can be found
 */
void *c_rbtree_find(struct c_rbtree *tree, void *key);

/**
 * Destroy a red-black tree, calling "func" for each data item
 *
 * @param tree Tree to destroy
 * @param func Destructor for the data ("free" would work")
 */
void c_rbtree_destroy(struct c_rbtree *tree, void (*func)(void *));

/**
 * Get number of data-containing nodes in the tree
 * @param tree Tree to operate on
 * @return Number of data-containing nodes
 */
unsigned int c_rbtree_num_nodes(struct c_rbtree *tree);

C_END_DECLS

#endif /* _RB_TREE_H_ */
