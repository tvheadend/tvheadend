/*
 * sys/queue.h wrappers and helpers
 */

#ifndef HTSQ_H
#define HTSQ_H

#include "../vendor/include/sys/queue.h"

/*
 * Extra LIST-ops
 */

#define LIST_LAST(headname, head, field) ({ \
        headname *r; \
        for (r = LIST_FIRST(head); \
             LIST_NEXT(r, field); \
             r = LIST_NEXT(r, field)); \
        r; \
})

#define LIST_PREV(elm, headname, head, field) ({ \
        headname *r; \
        for (r = LIST_FIRST(head); \
             r && LIST_NEXT(r, field) != (elm); \
             r = LIST_NEXT(r, field)); \
        r; \
})

#define LIST_ENTRY_INIT(elm, field) \
        (elm)->field.le_next = NULL, (elm)->field.le_prev = NULL

#define LIST_SAFE_REMOVE(elm, field) \
        if ((elm)->field.le_next != NULL || (elm)->field.le_prev != NULL) { \
                LIST_REMOVE(elm, field); \
                LIST_ENTRY_INIT(elm, field); \
        }

#define LIST_SAFE_ENTRY(elm, field) \
        ((elm)->field.le_next == NULL && (elm)->field.le_prev == NULL)

#define LIST_MOVE(newhead, oldhead, field) do {			        \
        if((oldhead)->lh_first) {					\
           (oldhead)->lh_first->field.le_prev = &(newhead)->lh_first;	\
	}								\
        (newhead)->lh_first = (oldhead)->lh_first;			\
} while (0)

#define LIST_INSERT_SORTED(head, elm, field, cmpfunc) do {	\
        if(LIST_EMPTY(head)) {					\
           LIST_INSERT_HEAD(head, elm, field);			\
        } else {						\
           typeof(elm) _tmp;					\
           LIST_FOREACH(_tmp,head,field) {			\
              if(cmpfunc(elm,_tmp) <= 0) {			\
                LIST_INSERT_BEFORE(_tmp,elm,field);		\
                break;						\
              }							\
              if(!LIST_NEXT(_tmp,field)) {			\
                 LIST_INSERT_AFTER(_tmp,elm,field);		\
                 break;						\
              }							\
           }							\
        }							\
} while(0)

/*
 * Extra TAILQ-ops
 */

#define TAILQ_SAFE_ENTRY(elm, field) \
        ((elm)->field.tqe_next == NULL && (elm)->field.tqe_prev == NULL)

#define TAILQ_SAFE_REMOVE(head, elm, field)			\
        if ((elm)->field.tqe_next != NULL || (elm)->field.tqe_prev != NULL) { \
           TAILQ_REMOVE(head, elm, field);			\
           (elm)->field.tqe_next = NULL;			\
           (elm)->field.tqe_prev = NULL;			\
        }

#define TAILQ_INSERT_SORTED(head, elm, field, cmpfunc) do {	\
        if(TAILQ_FIRST(head) == NULL) {				\
           TAILQ_INSERT_HEAD(head, elm, field);			\
        } else {						\
           typeof(elm) _tmp;					\
           TAILQ_FOREACH(_tmp,head,field) {			\
              if(cmpfunc(elm,_tmp) <= 0) {			\
                TAILQ_INSERT_BEFORE(_tmp,elm,field);		\
                break;						\
              }							\
              if(!TAILQ_NEXT(_tmp,field)) {			\
                 TAILQ_INSERT_AFTER(head,_tmp,elm,field);	\
                 break;						\
              }							\
           }							\
        }							\
} while(0)

#define TAILQ_INSERT_SORTED_R(head, headname, elm, field, cmpfunc) do { \
        if(TAILQ_FIRST(head) == NULL) {				\
           TAILQ_INSERT_HEAD(head, elm, field);			\
        } else {						\
           typeof(elm) _tmp;					\
           TAILQ_FOREACH_REVERSE(_tmp,head,headname,field) {			\
              if(cmpfunc(elm,_tmp) >= 0) {			\
                TAILQ_INSERT_AFTER(head,_tmp,elm,field);		\
                break;						\
              }							\
              if(!TAILQ_PREV(_tmp,headname,field)) {			\
                 TAILQ_INSERT_BEFORE(_tmp,elm,field);	\
                 break;						\
              }							\
           }							\
        }							\
} while(0)

#define TAILQ_MOVE(newhead, oldhead, field) do { \
        if(TAILQ_FIRST(oldhead)) { \
           TAILQ_FIRST(oldhead)->field.tqe_prev = &(newhead)->tqh_first;  \
          (newhead)->tqh_first = (oldhead)->tqh_first;                   \
          (newhead)->tqh_last = (oldhead)->tqh_last;                     \
          TAILQ_INIT(oldhead);\
        } else { \
          TAILQ_INIT(newhead);\
        }\
} while (/*CONSTCOND*/0) 
 
#endif /* HTSQ_H */
