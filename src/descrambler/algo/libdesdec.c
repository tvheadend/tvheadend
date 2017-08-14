/*
 * libaesdec.c
 */

#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "openssl/des.h"

#include "libdesdec.h"

/* key structure */
typedef struct des_priv {
  DES_key_schedule sched[2]; /* 0 = even, 1 = odd */
} des_priv_t;

/* even cw represents one 64-bit DES key */
void des_set_even_control_word(void *priv, const uint8_t *pk)
{
  DES_set_key_unchecked((const_DES_cblock *)pk, &((des_priv_t *)priv)->sched[0]);
}

/* odd cw represents one 64-bit DES key */
void des_set_odd_control_word(void *priv, const uint8_t *pk)
{
  DES_set_key_unchecked((const_DES_cblock *)pk, &((des_priv_t *)priv)->sched[1]);
}

/* set control words */
void des_set_control_words(void *priv,
                           const uint8_t *ev,
                           const uint8_t *od)
{
  DES_set_key_unchecked((const_DES_cblock *)ev, &((des_priv_t *)priv)->sched[0]);
  DES_set_key_unchecked((const_DES_cblock *)od, &((des_priv_t *)priv)->sched[1]);
}

/* allocate key structure */
void * des_get_priv_struct(void)
{
  des_priv_t *priv;

  priv = (des_priv_t *) malloc(sizeof(des_priv_t));
  if (priv) {
    static const uint8_t pk[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    des_set_control_words(priv, pk, pk);
  }
  return priv;
}

/* free key structure */
void des_free_priv_struct(void *priv)
{
  free(priv);
}

/* decrypt */
void des_decrypt_packet(void *priv, const uint8_t *pkt)
{
  uint_fast8_t ev_od = 0;
  uint_fast8_t xc0, offset, offset2, offset3;
  DES_key_schedule *sched;
  uint8_t buf[188];

  // skip reserved and not encrypted pkt
  if (((xc0 = pkt[3]) & 0x80) == 0)
    return;

  ev_od = (xc0 & 0x40) >> 6; // 0 even, 1 odd
  ((uint8_t *)pkt)[3] = xc0 & 0x3f;  // consider it decrypted now
  if (xc0 & 0x20) { // incomplete packet
    offset = 4 + pkt[4] + 1;
    if (offset + 8 > 188) { // decrypted==encrypted!
      return;  // this doesn't need more processing
    }
  } else {
    offset = 4;
  }

  sched = &((des_priv_t *)priv)->sched[ev_od];
  if (offset & 3) {
    /* data must be aligned for DES_encrypt1() */
    offset2 = (offset + 3) & ~3;
    memcpy(buf + offset2, pkt + offset, 188 - offset2);
    for (offset3 = offset2; offset3 <= (188 - 8); offset3 += 8) {
      DES_encrypt1((DES_LONG *)(buf + offset3), sched, 0);
    }
    memcpy((uint8_t *)(pkt + offset), buf + offset2, 188 - offset2);
  } else {
    for (; offset <= (188 - 8); offset += 8) {
      DES_encrypt1((DES_LONG *)(pkt + offset), sched, 0);
    }
  }
}
