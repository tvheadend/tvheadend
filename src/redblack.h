/*****************************************************************************
 *
* Copyright (C) 2001 Mark Edel
* Copyright (C) 2008 Andreas Öman

* This is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 2 of the License, or (at your option) any later
* version.
*
* This software is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
* for more details.
*
* You should have received a copy of the GNU General Public License along with
* software; if not, write to the Free Software Foundation, Inc., 51 Franklin
* Street, Fifth Floor, Boston, MA 02110-1301 USA
*
* Written by Mark Edel
* Macroified + additional support functions by Andreas Öman
*
*****************************************************************************/

#ifndef REDBLACK_H_
#define REDBLACK_H_

#define RB_TREE_NODE_RED      0
#define RB_TREE_NODE_BLACK    1


#define RB_HEAD(name, type)			\
struct name {					\
  struct type *first, *last, *root;		\
  int entries;					\
}

#define RB_ENTRY(type)				\
struct {					\
  struct type *left, *right, *parent;		\
  int color;					\
}

#define RB_INIT(head)				\
do {						\
  (head)->first   = NULL;			\
  (head)->last    = NULL;			\
  (head)->root    = NULL;			\
  (head)->entries = 0;				\
} while(0)



#define RB_ROTATE_LEFT(x, field, root)		\
do {						\
  typeof(x) xx = x;                             \
  typeof(x) yy = xx->field.right;		\
						\
  xx->field.right = yy->field.left;		\
  if(yy->field.left != NULL)			\
    yy->field.left->field.parent = xx;		\
						\
  yy->field.parent = xx->field.parent;		\
						\
  if(xx == root)				\
    root = yy;					\
  else if(xx == xx->field.parent->field.left) 	\
    xx->field.parent->field.left = yy;		\
  else  					\
    xx->field.parent->field.right = yy;		\
  yy->field.left = xx;				\
  xx->field.parent = yy;			\
} while(0)
  

#define RB_ROTATE_RIGHT(x, field, root)			\
do {							\
  typeof(x) xx = x;					\
  typeof(x) yy = xx->field.left;			\
							\
  xx->field.left = yy->field.right;			\
  if (yy->field.right != NULL)   			\
    yy->field.right->field.parent = xx;			\
  yy->field.parent = xx->field.parent;			\
							\
  if (xx == root)    					\
    root = yy;						\
  else if (xx == xx->field.parent->field.right)  	\
    xx->field.parent->field.right = yy;			\
  else  						\
    xx->field.parent->field.left = yy;			\
  yy->field.right = xx;					\
  xx->field.parent = yy;				\
} while(0)



#define RB_INSERT_BALANCE(x, field, root)				  \
do {									  \
  typeof(x) y;								  \
									  \
  x->field.color = RB_TREE_NODE_RED;					  \
  while (x != root && x->field.parent->field.color == RB_TREE_NODE_RED) { \
    if (x->field.parent == x->field.parent->field.parent->field.left) {	  \
      y = x->field.parent->field.parent->field.right;			  \
      if (y && y->field.color == RB_TREE_NODE_RED) {			  \
        x->field.parent->field.color = RB_TREE_NODE_BLACK;		  \
        y->field.color = RB_TREE_NODE_BLACK;				  \
        x->field.parent->field.parent->field.color = RB_TREE_NODE_RED;	  \
        x = x->field.parent->field.parent;				  \
      }									  \
      else {								  \
        if (x == x->field.parent->field.right) {			  \
          x = x->field.parent;						  \
          RB_ROTATE_LEFT(x, field, root);				  \
        }								  \
        x->field.parent->field.color = RB_TREE_NODE_BLACK;		  \
        x->field.parent->field.parent->field.color = RB_TREE_NODE_RED;	  \
        RB_ROTATE_RIGHT(x->field.parent->field.parent, field, root);	  \
      }									  \
    }									  \
    else {								  \
      y = x->field.parent->field.parent->field.left;			  \
      if (y && y->field.color == RB_TREE_NODE_RED) {			  \
        x->field.parent->field.color = RB_TREE_NODE_BLACK;		  \
        y->field.color = RB_TREE_NODE_BLACK;				  \
        x->field.parent->field.parent->field.color = RB_TREE_NODE_RED;	  \
        x = x->field.parent->field.parent;				  \
      }									  \
      else {								  \
        if (x == x->field.parent->field.left) {				  \
          x = x->field.parent;						  \
          RB_ROTATE_RIGHT(x, field, root);				  \
        }								  \
        x->field.parent->field.color = RB_TREE_NODE_BLACK;		  \
        x->field.parent->field.parent->field.color = RB_TREE_NODE_RED;	  \
        RB_ROTATE_LEFT(x->field.parent->field.parent, field, root);	  \
      }									  \
    }									  \
  }									  \
  root->field.color = RB_TREE_NODE_BLACK;				  \
} while(0);


/**
 * Insert a new node, if a collision occurs the colliding node is returned
 */
#define RB_INSERT_SORTED(head, skel, field, cmpfunc)			 \
({									 \
  int res, fromleft = 0;						 \
  typeof(skel) x = skel, c, parent = NULL;				 \
									 \
  c = (head)->root;							 \
  while(c != NULL) {							 \
    res = cmpfunc(x, c);						 \
    if(res < 0) {							 \
      parent = c;							 \
      c = c->field.left;						 \
      fromleft = 1;							 \
    } else if(res > 0) {						 \
      parent = c;							 \
      c = c->field.right;						 \
      fromleft = 0;							 \
    } else {								 \
      break;								 \
    }									 \
  }									 \
  if(c == NULL) {							 \
    x->field.parent = parent;						 \
    x->field.left = NULL;						 \
    x->field.right = NULL;						 \
    x->field.color = RB_TREE_NODE_RED;					 \
									 \
    if(parent) {							 \
      if(fromleft)							 \
	parent->field.left = x;						 \
      else								 \
	parent->field.right = x;					 \
    } else {								 \
      (head)->root = x;							 \
    }									 \
    (head)->entries++;							 \
									 \
    if(x->field.parent == (head)->first &&				 \
       (x->field.parent == NULL || x->field.parent->field.left == x)) {	 \
      (head)->first = x;						 \
    }									 \
									 \
    if(x->field.parent == (head)->last &&				 \
       (x->field.parent == NULL || x->field.parent->field.right == x)) { \
      (head)->last = x;							 \
    }									 \
    RB_INSERT_BALANCE(x, field, (head)->root);                           \
  }									 \
  c;									 \
})


/**
 * Returns next node
 */
#define RB_NEXT(e, field)			\
({						\
  typeof(e) xx = e, f;				\
  if (xx->field.right != NULL) {		\
    xx = xx->field.right;			\
    while (xx->field.left != NULL) {		\
      xx = xx->field.left;			\
    }						\
  }						\
  else {					\
    do {					\
      f = xx;					\
      xx = xx->field.parent;			\
    } while (xx && f == xx->field.right);	\
  }						\
  xx;						\
})


/**
 * Returns previous node
 */
#define RB_PREV(e, field)			\
({						\
  typeof(e) xx = e, f;				\
  if (xx->field.left != NULL) {			\
    xx = xx->field.left;			\
    while (xx->field.right != NULL) {		\
      xx = xx->field.right;			\
    }						\
  }						\
  else {					\
    do {					\
      f = xx;					\
      xx = xx->field.parent;			\
    } while (xx && f == xx->field.left);	\
  }						\
  xx;						\
})


/**
 * Returns first node
 */
#define RB_FIRST(head) ((head)->first)

/**
 * Returns last node
 */
#define RB_LAST(head)  ((head)->last)

/**
 * Iterate thru all nodes
 */
#define RB_FOREACH(e, head, field)		\
 for(e = (head)->first; e != NULL; 		\
       ({					\
	 if(e->field.right != NULL) {		\
	   e = e->field.right;			\
	   while(e->field.left != NULL) {	\
	     e = e->field.left;			\
	   }					\
	 } else {				\
	   typeof(e) f;				\
	   do {					\
	     f = e;				\
	     e = e->field.parent;		\
	   } while(e && f == e->field.right);	\
	 }					\
       }))


/**
 * Iterate thru all nodes in reverse order
 */
#define RB_FOREACH_REVERSE(e, head, field)	\
 for(e = (head)->last; e != NULL; 		\
       ({					\
	 if(e->field.left != NULL) {		\
	   e = e->field.left;			\
	   while(e->field.right != NULL) {	\
	     e = e->field.right;		\
	   }					\
	 } else {				\
	   typeof(e) f;				\
	   do {					\
	     f = e;				\
	     e = e->field.parent;		\
	   } while(e && f == e->field.left);	\
	 }					\
       }))

/**
 * Remove the given node
 */
#define RB_REMOVE(head, e, field)					 \
do {									 \
  int swapColor;							 \
  typeof(e) x, y, z = e, x_parent, w;					 \
									 \
  y = z;								 \
  if (y == (head)->first) {						 \
    (head)->first = RB_NEXT(y, field);				         \
  }									 \
  if (y == (head)->last) {						 \
    (head)->last = RB_PREV(y, field);				         \
  }									 \
  if (y->field.left == NULL) {						 \
    x = y->field.right;							 \
  }									 \
  else {								 \
    if (y->field.right == NULL) {					 \
      x = y->field.left;						 \
    }									 \
    else {								 \
      y = y->field.right;						 \
      while (y->field.left != NULL) {					 \
	y = y->field.left;						 \
      }									 \
      x = y->field.right;						 \
    }									 \
  }									 \
  if (y != z) {								 \
    z->field.left->field.parent = y;					 \
    y->field.left = z->field.left;					 \
    if (y != z->field.right) {						 \
      x_parent = y->field.parent;					 \
      if (x != NULL) {							 \
	x->field.parent = y->field.parent;				 \
      }									 \
      y->field.parent->field.left = x;					 \
      y->field.right = z->field.right;					 \
      z->field.right->field.parent = y;					 \
    }									 \
    else {								 \
      x_parent = y;							 \
    }									 \
    if ((head)->root == z) {						 \
      (head)->root = y;							 \
    }									 \
    else if (z->field.parent->field.left == z) {			 \
      z->field.parent->field.left = y;					 \
    }									 \
    else {								 \
      z->field.parent->field.right = y;					 \
    }									 \
    y->field.parent = z->field.parent;					 \
									 \
    swapColor = y->field.color;						 \
    y->field.color = z->field.color;					 \
    z->field.color = swapColor;						 \
									 \
    y = z;								 \
  }									 \
  else {								 \
    x_parent = y->field.parent;						 \
    if (x != NULL) {							 \
      x->field.parent = y->field.parent;				 \
    }									 \
    if ((head)->root == z) {						 \
      (head)->root = x;							 \
    }									 \
    else {								 \
      if (z->field.parent->field.left == z) {				 \
	z->field.parent->field.left = x;				 \
      }									 \
      else {								 \
	z->field.parent->field.right = x;				 \
      }									 \
    }									 \
  }									 \
									 \
  (head)->entries--;							 \
									 \
  if (y->field.color != RB_TREE_NODE_RED) { 				 \
    while (x != (head)->root && 					 \
	   (x == NULL || x->field.color == RB_TREE_NODE_BLACK)) {	 \
      if (x == x_parent->field.left) {					 \
	w = x_parent->field.right;					 \
	if (w->field.color == RB_TREE_NODE_RED) {			 \
	  w->field.color = RB_TREE_NODE_BLACK;				 \
	  x_parent->field.color = RB_TREE_NODE_RED;			 \
	  RB_ROTATE_LEFT(x_parent, field, (head)->root);		 \
	  w = x_parent->field.right;					 \
	}								 \
	if ((w->field.left == NULL || 					 \
	     w->field.left->field.color == RB_TREE_NODE_BLACK) &&	 \
	    (w->field.right == NULL || 					 \
	     w->field.right->field.color == RB_TREE_NODE_BLACK)) {	 \
									 \
	  w->field.color = RB_TREE_NODE_RED;				 \
	  x = x_parent;							 \
	  x_parent = x_parent->field.parent;				 \
	} else {							 \
	  if (w->field.right == NULL || 				 \
	      w->field.right->field.color == RB_TREE_NODE_BLACK) {	 \
									 \
	    if (w->field.left) {					 \
	      w->field.left->field.color = RB_TREE_NODE_BLACK;		 \
	    }								 \
	    w->field.color = RB_TREE_NODE_RED;				 \
	    RB_ROTATE_RIGHT(w, field, (head)->root);			 \
	    w = x_parent->field.right;					 \
	  }								 \
	  w->field.color = x_parent->field.color;			 \
	  x_parent->field.color = RB_TREE_NODE_BLACK;			 \
	  if (w->field.right) {						 \
	    w->field.right->field.color = RB_TREE_NODE_BLACK;		 \
	  }								 \
	  RB_ROTATE_LEFT(x_parent, field, (head)->root);		 \
	  break;							 \
	}								 \
      }									 \
      else {								 \
	w = x_parent->field.left;					 \
	if (w->field.color == RB_TREE_NODE_RED) {			 \
	  w->field.color = RB_TREE_NODE_BLACK;				 \
	  x_parent->field.color = RB_TREE_NODE_RED;			 \
	  RB_ROTATE_RIGHT(x_parent, field, (head)->root);		 \
	  w = x_parent->field.left;					 \
	}								 \
	if ((w->field.right == NULL || 					 \
	     w->field.right->field.color == RB_TREE_NODE_BLACK) &&	 \
	    (w->field.left == NULL || 					 \
	     w->field.left->field.color == RB_TREE_NODE_BLACK)) {	 \
									 \
	  w->field.color = RB_TREE_NODE_RED;				 \
	  x = x_parent;							 \
	  x_parent = x_parent->field.parent;				 \
	}								 \
	else {								 \
	  if (w->field.left == NULL || 					 \
	      w->field.left->field.color == RB_TREE_NODE_BLACK) {	 \
									 \
	    if (w->field.right) {					 \
	      w->field.right->field.color = RB_TREE_NODE_BLACK;		 \
	    }								 \
	    w->field.color = RB_TREE_NODE_RED;				 \
	    RB_ROTATE_LEFT(w, field, (head)->root);			 \
	    w = x_parent->field.left;					 \
	  }								 \
	  w->field.color = x_parent->field.color;			 \
	  x_parent->field.color = RB_TREE_NODE_BLACK;			 \
	  if (w->field.left) {						 \
	    w->field.left->field.color = RB_TREE_NODE_BLACK;		 \
	  }								 \
	  RB_ROTATE_RIGHT(x_parent, field, (head)->root);		 \
	  break;							 \
	}								 \
      }									 \
    }									 \
    if (x) {								 \
      x->field.color = RB_TREE_NODE_BLACK;				 \
    }									 \
  }									 \
} while(0)



/**
 * Finds a node
 */
#define RB_FIND(head, skel, field, cmpfunc)	\
({						\
  int res;                                        \
  typeof(skel) c = (head)->root;		\
  while(c != NULL) {				\
    res = cmpfunc(skel, c);			\
    if(res < 0) {				\
      c = c->field.left;			\
    } else if(res > 0) {			\
      c = c->field.right;			\
    } else {					\
      break;					\
    }						\
  }						\
 c;						\
}) 



/**
 * Finds first node greater than 'skel'
 */
#define RB_FIND_GT(head, skel, field, cmpfunc)	  \
({						  \
  int res;                                        \
  typeof(skel) c = (head)->root, x = NULL;	  \
  while(c != NULL) {				  \
    res = cmpfunc(skel, c);			  \
    if(res < 0) {				  \
      x = c;					  \
      c = c->field.left;			  \
    } else if(res > 0) {			  \
      c = c->field.right;			  \
    } else {					  \
      x = RB_NEXT(c, field);			  \
      break;					  \
    }						  \
  }						  \
 x;						  \
})

/**
 * Finds a node greater or equal to 'skel'
 */
#define RB_FIND_GE(head, skel, field, cmpfunc)	  \
({						  \
  int res;                                        \
  typeof(skel) c = (head)->root, x = NULL;	  \
  while(c != NULL) {				  \
    res = cmpfunc(skel, c);			  \
    if(res < 0) {				  \
      x = c;					  \
      c = c->field.left;			  \
    } else if(res > 0) {			  \
      c = c->field.right;			  \
    } else {					  \
      x = c;                   			  \
      break;					  \
    }						  \
  }						  \
 x;						  \
})


/**
 * Finds first node lesser than 'skel'
 */
#define RB_FIND_LT(head, skel, field, cmpfunc)	  \
({						  \
  int res;                                        \
  typeof(skel) c = (head)->root, x = NULL;	  \
  while(c != NULL) {				  \
    res = cmpfunc(skel, c);			  \
    if(res < 0) {				  \
      c = c->field.left;			  \
    } else if(res > 0) {			  \
      x = c;					  \
      c = c->field.right;			  \
    } else {					  \
      x = RB_PREV(c, field);			  \
      break;					  \
    }						  \
  }						  \
 x;						  \
})

/**
 * Finds a node lesser or equal to 'skel'
 */
#define RB_FIND_LE(head, skel, field, cmpfunc)	  \
({						  \
  int res;                                        \
  typeof(skel) c = (head)->root, x = NULL;	  \
  while(c != NULL) {				  \
    res = cmpfunc(skel, c);			  \
    if(res < 0) {				  \
      c = c->field.left;			  \
    } else if(res > 0) {			  \
      x = c;					  \
      c = c->field.right;			  \
    } else {					  \
      x = c;                   			  \
      break;					  \
    }						  \
  }						  \
 x;						  \
})

#endif /* REDBLACK_H_ */
