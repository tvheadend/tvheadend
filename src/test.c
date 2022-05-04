#include "test.h"

const idclass_t obj_a_class = {
  .ic_class      = "obj_a",
  .ic_caption    = N_("Object A"),
  .ic_properties = (const property_t[]){
    { PROPDEF1("int1",  "Integer 1", PT_INT,  obj_a_t, a_int1)  },
    { PROPDEF1("bool1", "Boolean 1", PT_BOOL, obj_a_t, a_bool1) },
    { PROPDEF1("str1",  "String 1",  PT_STR,  obj_a_t, a_str1)  },
  }
};

static void obj_b_save ( idnode_t *self );

const idclass_t obj_b_class = {
  .ic_super      = &obj_a_class,
  .ic_class      = "obj_b",
  .ic_caption    = N_("Object B"),
  .ic_save       = obj_b_save,
  .ic_properties = (const property_t[]){
    { PROPDEF1("int2",  "Integer 2", PT_INT,  obj_b_t, b_int2)  },
    { PROPDEF1("bool2", "Boolean 2", PT_BOOL, obj_b_t, b_bool2) },
    { PROPDEF1("str2",  "String 2",  PT_STR,  obj_b_t, b_str2)  },
  }
};

static void
obj_a_func1 ( void *self )
{
  obj_a_t *a = self;
  printf("a->a_int1 = %d\n", a->a_int1);
}

static obj_a_t *
obj_a_create1 ( size_t alloc, const idclass_t *class, const char *uuid )
{
  obj_a_t *a = idnode_create(alloc, class, uuid);
  a->a_func1 = obj_a_func1;
  return a;
}

#define obj_a_create(type, uuid)\
  (struct type*)obj_a_create1(sizeof(struct type), &type##_class, uuid)

static void
obj_b_func1 ( void *self )
{
  obj_b_t *b = self;
  printf("b->a_int1 = %d\n", b->a_int1);
  printf("b->b_int2 = %d\n", b->b_int2);
}

obj_b_t *
obj_b_create ( const char *uuid )
{
  obj_b_t *b = obj_a_create(obj_b, uuid);
  b->a_func1 = obj_b_func1;
  b->a_bool1 = 0;
  b->b_bool2 = 1;
  b->a_int1  = 2;
  b->b_int2  = 3;
  b->a_str1  = strdup("HELLO");
  b->b_str2  = strdup("WORLD");
  return b;
}

void obj_b_save ( idnode_t *self )
{
  idnode_save(self, "test");
}

void obj_b_load_all ( void )
{
  idnode_load_all("test", (void*(*)(const char*))obj_b_create);
}
