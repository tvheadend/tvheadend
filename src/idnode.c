#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "idnode.h"

static int randfd = 0;

RB_HEAD(idnode_tree, idnode);

static struct idnode_tree idnodes;

/**
 *
 */
static int
hexnibble(char c)
{
  switch(c) {
  case '0' ... '9':    return c - '0';
  case 'a' ... 'f':    return c - 'a' + 10;
  case 'A' ... 'F':    return c - 'A' + 10;
  default:
    return -1;
  }
}


/**
 *
 */
static int
hex2bin(uint8_t *buf, size_t buflen, const char *str)
{
  int hi, lo;

  while(*str) {
    if(buflen == 0)
      return -1;
    if((hi = hexnibble(*str++)) == -1)
      return -1;
    if((lo = hexnibble(*str++)) == -1)
      return -1;

    *buf++ = hi << 4 | lo;
    buflen--;
  }
  return 0;
}


/**
 *
 */
static void
bin2hex(char *dst, size_t dstlen, const uint8_t *src, size_t srclen)
{
  while(dstlen > 2 && srclen > 0) {
    *dst++ = "0123456789abcdef"[*src >> 4];
    *dst++ = "0123456789abcdef"[*src & 0xf];
    src++;
    srclen--;
    dstlen -= 2;
  }
  *dst = 0;
}


/**
 *
 */
void
idnode_init(void)
{
  randfd = open("/dev/urandom", O_RDONLY);
  if(randfd == -1)
    exit(1);
}


/**
 *
 */
static int
in_cmp(const idnode_t *a, const idnode_t *b)
{
  return memcmp(a->in_uuid, b->in_uuid, 16);
}


/**
 *
 */
int
idnode_insert(idnode_t *in, const char *uuid, const idclass_t *class)
{
  idnode_t *c;
  if(uuid == NULL) {
    if(read(randfd, in->in_uuid, 16) != 16) {
      perror("read(random for uuid)");
      exit(1);
    }
  } else {
    if(hex2bin(in->in_uuid, 16, uuid))
      return -1;
  }

  in->in_class = class;

  c = RB_INSERT_SORTED(&idnodes, in, in_link, in_cmp);
  if(c != NULL) {
    fprintf(stderr, "Id node collision\n");
    abort();
  }
  return 0;
}


/**
 *
 */
const char *
idnode_uuid_as_str(const idnode_t *in)
{
  static char ret[16][33];
  static int p;
  char *b = ret[p];
  bin2hex(b, 33, in->in_uuid, 16);
  p = (p + 1) & 15;
  return b;
}


/**
 *
 */
void *
idnode_find(const char *uuid, const idclass_t *idc)
{
  idnode_t skel, *r;

  if(hex2bin(skel.in_uuid, 16, uuid))
    return NULL;
  r = RB_FIND(&idnodes, &skel, in_link, in_cmp);
  if(r != NULL && idc != NULL) {
    const idclass_t *c = r->in_class;
    for(;c != NULL; c = c->ic_super) {
      if(idc == c)
        return r;
    }
    return NULL;
  }
  return r;
}


/**
 *
 */
void
idnode_unlink(idnode_t *in)
{
  RB_REMOVE(&idnodes, in, in_link);
}


/**
 * Recursive to get superclass nodes first
 */
static void
add_descriptors(struct idnode *self, const idclass_t *ic, htsmsg_t *p)
{
  if(ic->ic_super != NULL)
    add_descriptors(self, ic->ic_super, p);

  if(TAILQ_FIRST(&p->hm_fields) != NULL) {
    // Only add separator if not empty
    htsmsg_t *m = htsmsg_create_map();
    htsmsg_add_str(m, "caption", ic->ic_caption ?: ic->ic_class);
    htsmsg_add_str(m, "type", "separator");
    htsmsg_add_msg(p, NULL, m);
  }

  prop_add_descriptors_to_msg(self, ic->ic_properties, p);
}


/**
 *
 */
htsmsg_t *
idnode_serialize(struct idnode *self)
{
  const idclass_t *c = self->in_class;
  htsmsg_t *m;
  if(c->ic_serialize != NULL) {
    m = c->ic_serialize(self);
  } else {
    m = htsmsg_create_map();

    if(c->ic_get_title != NULL) {
      htsmsg_add_str(m, "text", c->ic_get_title(self));
    } else {
      htsmsg_add_str(m, "text", idnode_uuid_as_str(self));
    }

    htsmsg_t *p  = htsmsg_create_list();
    add_descriptors(self, c, p);

    htsmsg_add_msg(m, "descriptors", p);

    htsmsg_add_str(m, "id", idnode_uuid_as_str(self));
  }
  return m;
}

/**
 *
 */
static void
idnode_save(idnode_t *in)
{
  const idclass_t *ic = in->in_class;
  for(; ic != NULL; ic = ic->ic_super) {
    if(ic->ic_save != NULL) {
      ic->ic_save(in);
      return;
    }
  }
}


/**
 *
 */
void
idnode_set_prop(idnode_t *in, const char *key, const char *value)
{
  const idclass_t *ic = in->in_class;
  int do_save = 0;
  for(;ic != NULL; ic = ic->ic_super) {
    int x = prop_set(in, ic->ic_properties, key, value);
    if(x == -1)
      continue;
    do_save |= x;
    break;
  }
  if(do_save)
    idnode_save(in);
}


/**
 *
 */
void
idnode_update_all_props(idnode_t *in,
                        const char *(*getvalue)(void *opaque, const char *key),
                        void *opaque)
{
  const idclass_t *ic = in->in_class;
  int do_save = 0;
  for(;ic != NULL; ic = ic->ic_super)
    do_save |= prop_update_all(in, ic->ic_properties, getvalue, opaque);
  if(do_save)
    idnode_save(in);
}
