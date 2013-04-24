#include "idnode.h"

#include <stdint.h>

/* Parent */
typedef struct obj_a
{
  idnode_t a_id;
  
  uint32_t a_int1;
  int      a_bool1;
  char    *a_str1;

  void    (*a_func1) (void *a);

} obj_a_t;

extern const idclass_t obj_a_class;

/* Child */
typedef struct obj_b
{
  obj_a_t;

  uint32_t b_int2;
  int      b_bool2;
  char    *b_str2;

} obj_b_t;

obj_b_t *obj_b_create ( const char *uuid );
void obj_b_load_all ( void );
