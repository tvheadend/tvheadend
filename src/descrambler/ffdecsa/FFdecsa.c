/* FFdecsa -- fast decsa algorithm
 *
 * Copyright (C) 2003-2004  fatih89r
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */


#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef NULL
#define NULL 0
#endif

//#define DEBUG
#ifdef DEBUG
#define DBG(a) a
#else
#define DBG(a)
#endif

//// parallelization stuff, large speed differences are possible
// possible choices
#define PARALLEL_32_4CHAR     320
#define PARALLEL_32_4CHARA    321
#define PARALLEL_32_INT       322
#define PARALLEL_64_8CHAR     640
#define PARALLEL_64_8CHARA    641
#define PARALLEL_64_2INT      642
#define PARALLEL_64_LONG      643
#define PARALLEL_64_MMX       644
#define PARALLEL_128_16CHAR  1280
#define PARALLEL_128_16CHARA 1281
#define PARALLEL_128_4INT    1282
#define PARALLEL_128_2LONG   1283
#define PARALLEL_128_2MMX    1284
#define PARALLEL_128_SSE     1285
#define PARALLEL_128_SSE2    1286

#include "parallel_generic.h"
//// conditionals
#if PARALLEL_MODE==PARALLEL_32_4CHAR
#include "parallel_032_4char.h"
#elif PARALLEL_MODE==PARALLEL_32_4CHARA
#include "parallel_032_4charA.h"
#elif PARALLEL_MODE==PARALLEL_32_INT
#include "parallel_032_int.h"
#define FUNC(x) (x ## _32int)
#elif PARALLEL_MODE==PARALLEL_64_8CHAR
#include "parallel_064_8char.h"
#elif PARALLEL_MODE==PARALLEL_64_8CHARA
#include "parallel_064_8charA.h"
#elif PARALLEL_MODE==PARALLEL_64_2INT
#include "parallel_064_2int.h"
#elif PARALLEL_MODE==PARALLEL_64_LONG
#include "parallel_064_long.h"
#elif PARALLEL_MODE==PARALLEL_64_MMX
#include "parallel_064_mmx.h"
#define FUNC(x) (x ## _64mmx)
#elif PARALLEL_MODE==PARALLEL_128_16CHAR
#include "parallel_128_16char.h"
#elif PARALLEL_MODE==PARALLEL_128_16CHARA
#include "parallel_128_16charA.h"
#elif PARALLEL_MODE==PARALLEL_128_4INT
#include "parallel_128_4int.h"
#elif PARALLEL_MODE==PARALLEL_128_2LONG
#include "parallel_128_2long.h"
#elif PARALLEL_MODE==PARALLEL_128_2MMX
#include "parallel_128_2mmx.h"
#elif PARALLEL_MODE==PARALLEL_128_SSE
#include "parallel_128_sse.h"
#elif PARALLEL_MODE==PARALLEL_128_SSE2
#include "parallel_128_sse2.h"
#define FUNC(x) (x ## _128sse2)
#else
#error "unknown/undefined parallel mode"
#endif


// stuff depending on conditionals

#define BYTES_PER_GROUP (GROUP_PARALLELISM/8)
#define BYPG BYTES_PER_GROUP
#define BITS_PER_GROUP GROUP_PARALLELISM
#define BIPG BITS_PER_GROUP

#ifndef MALLOC
#define MALLOC(X) malloc(X)
#endif
#ifndef FREE
#define FREE(X) free(X)
#endif
#ifndef MEMALIGN
#define MEMALIGN
#endif

//// debug tool

#if 0
static void dump_mem(const char *string, const unsigned char *p, int len, int linelen){
  int i;
  for(i=0;i<len;i++){
    if(i%linelen==0&&i) fprintf(stderr,"\n");
    if(i%linelen==0) fprintf(stderr,"%s %08x:",string,i);
    else{
      if(i%8==0) fprintf(stderr," ");
      if(i%4==0) fprintf(stderr," ");
    }
    fprintf(stderr," %02x",p[i]);
  }
  if(i%linelen==0) fprintf(stderr,"\n");
}
#endif

//////////////////////////////////////////////////////////////////////////////////

struct csa_key_t{
	unsigned char ck[8];
// used by stream
        int iA[8];  // iA[0] is for A1, iA[7] is for A8
        int iB[8];  // iB[0] is for B1, iB[7] is for B8
// used by stream (group)
        MEMALIGN group ck_g[8][8]; // [byte][bit:0=LSB,7=MSB]
        MEMALIGN group iA_g[8][4]; // [0 for A1][0 for LSB]
        MEMALIGN group iB_g[8][4]; // [0 for B1][0 for LSB]
// used by block
	unsigned char kk[56];
// used by block (group)
	MEMALIGN batch kkmulti[56]; // many times the same byte in every batch
};

struct csa_keys_t{
  struct csa_key_t even;
  struct csa_key_t odd;
};

//-----stream cypher

//-----key schedule for stream decypher
static void key_schedule_stream(
  unsigned char *ck,    // [In]  ck[0]-ck[7]   8 bytes   | Key.
  int *iA,              // [Out] iA[0]-iA[7]   8 nibbles | Key schedule.
  int *iB)              // [Out] iB[0]-iB[7]   8 nibbles | Key schedule.
{
    iA[0]=(ck[0]>>4)&0xf;
    iA[1]=(ck[0]   )&0xf;
    iA[2]=(ck[1]>>4)&0xf;
    iA[3]=(ck[1]   )&0xf;
    iA[4]=(ck[2]>>4)&0xf;
    iA[5]=(ck[2]   )&0xf;
    iA[6]=(ck[3]>>4)&0xf;
    iA[7]=(ck[3]   )&0xf;
    iB[0]=(ck[4]>>4)&0xf;
    iB[1]=(ck[4]   )&0xf;
    iB[2]=(ck[5]>>4)&0xf;
    iB[3]=(ck[5]   )&0xf;
    iB[4]=(ck[6]>>4)&0xf;
    iB[5]=(ck[6]   )&0xf;
    iB[6]=(ck[7]>>4)&0xf;
    iB[7]=(ck[7]   )&0xf;
}

//----- stream main function

#define STREAM_INIT
#include "stream.c"
#undef STREAM_INIT

#define STREAM_NORMAL
#include "stream.c"
#undef STREAM_NORMAL


//-----block decypher

//-----key schedule for block decypher

static void key_schedule_block(
  unsigned char *ck,    // [In]  ck[0]-ck[7]   8 bytes | Key.
  unsigned char *kk)    // [Out] kk[0]-kk[55] 56 bytes | Key schedule.
{
  static const unsigned char key_perm[0x40] = {
    0x12,0x24,0x09,0x07,0x2A,0x31,0x1D,0x15, 0x1C,0x36,0x3E,0x32,0x13,0x21,0x3B,0x40,
    0x18,0x14,0x25,0x27,0x02,0x35,0x1B,0x01, 0x22,0x04,0x0D,0x0E,0x39,0x28,0x1A,0x29,
    0x33,0x23,0x34,0x0C,0x16,0x30,0x1E,0x3A, 0x2D,0x1F,0x08,0x19,0x17,0x2F,0x3D,0x11,
    0x3C,0x05,0x38,0x2B,0x0B,0x06,0x0A,0x2C, 0x20,0x3F,0x2E,0x0F,0x03,0x26,0x10,0x37,
  };

  int i,j,k;
  int bit[64];
  int newbit[64];
  int kb[7][8];

  // 56 steps
  // 56 key bytes kk(55)..kk(0) by key schedule from ck

  // kb(6,0) .. kb(6,7) = ck(0) .. ck(7)
  kb[6][0] = ck[0];
  kb[6][1] = ck[1];
  kb[6][2] = ck[2];
  kb[6][3] = ck[3];
  kb[6][4] = ck[4];
  kb[6][5] = ck[5];
  kb[6][6] = ck[6];
  kb[6][7] = ck[7];

  // calculate kb[5] .. kb[0]
  for(i=5; i>=0; i--){
    // 64 bit perm on kb
    for(j=0; j<8; j++){
      for(k=0; k<8; k++){
        bit[j*8+k] = (kb[i+1][j] >> (7-k)) & 1;
        newbit[key_perm[j*8+k]-1] = bit[j*8+k];
      }
    }
    for(j=0; j<8; j++){
      kb[i][j] = 0;
      for(k=0; k<8; k++){
        kb[i][j] |= newbit[j*8+k] << (7-k);
      }
    }
  }

  // xor to give kk
  for(i=0; i<7; i++){
    for(j=0; j<8; j++){
      kk[i*8+j] = kb[i][j] ^ i;
    }
  }

}

//-----block utils

static inline __attribute__((always_inline)) void trasp_N_8 (unsigned char *in,unsigned char* out,int count){
  int *ri=(int *)in;
  int *ibi=(int *)out;
  int j,i,k,g;
  // copy and first step
  for(g=0;g<count;g++){
    ri[g]=ibi[2*g];
    ri[GROUP_PARALLELISM+g]=ibi[2*g+1];
  }
//dump_mem("NE1 r[roff]",&r[roff],GROUP_PARALLELISM*8,GROUP_PARALLELISM);
// now 01230123
#define INTS_PER_ROW (GROUP_PARALLELISM/8*2)
  for(j=0;j<8;j+=4){
    for(i=0;i<2;i++){
      for(k=0;k<INTS_PER_ROW;k++){
        unsigned int t,b;
        t=ri[INTS_PER_ROW*(j+i)+k];
        b=ri[INTS_PER_ROW*(j+i+2)+k];
        ri[INTS_PER_ROW*(j+i)+k]=     (t&0x0000ffff)      | ((b           )<<16);
        ri[INTS_PER_ROW*(j+i+2)+k]=  ((t           )>>16) |  (b&0xffff0000) ;
      }
    }
  }
//dump_mem("NE2 r[roff]",&r[roff],GROUP_PARALLELISM*8,GROUP_PARALLELISM);
// now 01010101
  for(j=0;j<8;j+=2){
    for(i=0;i<1;i++){
      for(k=0;k<INTS_PER_ROW;k++){
        unsigned int t,b;
        t=ri[INTS_PER_ROW*(j+i)+k];
        b=ri[INTS_PER_ROW*(j+i+1)+k];
        ri[INTS_PER_ROW*(j+i)+k]=     (t&0x00ff00ff)     | ((b&0x00ff00ff)<<8);
        ri[INTS_PER_ROW*(j+i+1)+k]=  ((t&0xff00ff00)>>8) |  (b&0xff00ff00);
      }
    }
  }
//dump_mem("NE3 r[roff]",&r[roff],GROUP_PARALLELISM*8,GROUP_PARALLELISM);
// now 00000000
}

static inline __attribute__((always_inline)) void trasp_8_N (unsigned char *in,unsigned char* out,int count){
  int *ri=(int *)in;
  int *bdi=(int *)out;
  int j,i,k,g;
#define INTS_PER_ROW (GROUP_PARALLELISM/8*2)
//dump_mem("NE1 r[roff]",&r[roff],GROUP_PARALLELISM*8,GROUP_PARALLELISM);
// now 00000000
  for(j=0;j<8;j+=2){
    for(i=0;i<1;i++){
      for(k=0;k<INTS_PER_ROW;k++){
        unsigned int t,b;
        t=ri[INTS_PER_ROW*(j+i)+k];
        b=ri[INTS_PER_ROW*(j+i+1)+k];
        ri[INTS_PER_ROW*(j+i)+k]=     (t&0x00ff00ff)     | ((b&0x00ff00ff)<<8);
        ri[INTS_PER_ROW*(j+i+1)+k]=  ((t&0xff00ff00)>>8) |  (b&0xff00ff00);
      }
    }
  }
//dump_mem("NE2 r[roff]",&r[roff],GROUP_PARALLELISM*8,GROUP_PARALLELISM);
// now 01010101
  for(j=0;j<8;j+=4){
    for(i=0;i<2;i++){
      for(k=0;k<INTS_PER_ROW;k++){
        unsigned int t,b;
        t=ri[INTS_PER_ROW*(j+i)+k];
        b=ri[INTS_PER_ROW*(j+i+2)+k];
        ri[INTS_PER_ROW*(j+i)+k]=     (t&0x0000ffff)      | ((b           )<<16);
        ri[INTS_PER_ROW*(j+i+2)+k]=  ((t           )>>16) |  (b&0xffff0000) ;
      }
    }
  }
//dump_mem("NE3 r[roff]",&r[roff],GROUP_PARALLELISM*8,GROUP_PARALLELISM);
// now 01230123
  for(g=0;g<count;g++){
    bdi[2*g]=ri[g];
    bdi[2*g+1]=ri[GROUP_PARALLELISM+g];
  }
}

//-----block main function

// block group
static void block_decypher_group (
  batch *kkmulti,       // [In]  kkmulti[0]-kkmulti[55] 56 batches | Key schedule (each batch has repeated equal bytes).
  unsigned char *ib,    // [In]  (ib0,ib1,...ib7)...x32 32*8 bytes | Initialization vector.
  unsigned char *bd,    // [Out] (bd0,bd1,...bd7)...x32 32*8 bytes | Block decipher.
  int count)
{
  // int is faster than unsigned char. apparently not
  static const unsigned char block_sbox[0x100] = {
    0x3A,0xEA,0x68,0xFE,0x33,0xE9,0x88,0x1A, 0x83,0xCF,0xE1,0x7F,0xBA,0xE2,0x38,0x12,
    0xE8,0x27,0x61,0x95,0x0C,0x36,0xE5,0x70, 0xA2,0x06,0x82,0x7C,0x17,0xA3,0x26,0x49,
    0xBE,0x7A,0x6D,0x47,0xC1,0x51,0x8F,0xF3, 0xCC,0x5B,0x67,0xBD,0xCD,0x18,0x08,0xC9,
    0xFF,0x69,0xEF,0x03,0x4E,0x48,0x4A,0x84, 0x3F,0xB4,0x10,0x04,0xDC,0xF5,0x5C,0xC6,
    0x16,0xAB,0xAC,0x4C,0xF1,0x6A,0x2F,0x3C, 0x3B,0xD4,0xD5,0x94,0xD0,0xC4,0x63,0x62,
    0x71,0xA1,0xF9,0x4F,0x2E,0xAA,0xC5,0x56, 0xE3,0x39,0x93,0xCE,0x65,0x64,0xE4,0x58,
    0x6C,0x19,0x42,0x79,0xDD,0xEE,0x96,0xF6, 0x8A,0xEC,0x1E,0x85,0x53,0x45,0xDE,0xBB,
    0x7E,0x0A,0x9A,0x13,0x2A,0x9D,0xC2,0x5E, 0x5A,0x1F,0x32,0x35,0x9C,0xA8,0x73,0x30,

    0x29,0x3D,0xE7,0x92,0x87,0x1B,0x2B,0x4B, 0xA5,0x57,0x97,0x40,0x15,0xE6,0xBC,0x0E,
    0xEB,0xC3,0x34,0x2D,0xB8,0x44,0x25,0xA4, 0x1C,0xC7,0x23,0xED,0x90,0x6E,0x50,0x00,
    0x99,0x9E,0x4D,0xD9,0xDA,0x8D,0x6F,0x5F, 0x3E,0xD7,0x21,0x74,0x86,0xDF,0x6B,0x05,
    0x8E,0x5D,0x37,0x11,0xD2,0x28,0x75,0xD6, 0xA7,0x77,0x24,0xBF,0xF0,0xB0,0x02,0xB7,
    0xF8,0xFC,0x81,0x09,0xB1,0x01,0x76,0x91, 0x7D,0x0F,0xC8,0xA0,0xF2,0xCB,0x78,0x60,
    0xD1,0xF7,0xE0,0xB5,0x98,0x22,0xB3,0x20, 0x1D,0xA6,0xDB,0x7B,0x59,0x9F,0xAE,0x31,
    0xFB,0xD3,0xB6,0xCA,0x43,0x72,0x07,0xF4, 0xD8,0x41,0x14,0x55,0x0D,0x54,0x8B,0xB9,
    0xAD,0x46,0x0B,0xAF,0x80,0x52,0x2C,0xFA, 0x8C,0x89,0x66,0xFD,0xB2,0xA9,0x9B,0xC0,
  };
  MEMALIGN unsigned char r[GROUP_PARALLELISM*(8+56)];  /* 56 because we will move back in memory while looping */
  MEMALIGN unsigned char sbox_in[GROUP_PARALLELISM],sbox_out[GROUP_PARALLELISM],perm_out[GROUP_PARALLELISM];
  int roff;
  int i,g,count_all=GROUP_PARALLELISM;

  roff=GROUP_PARALLELISM*56;
  memset(r + roff, 0, sizeof(r) - roff);

#define FASTTRASP1
#ifndef FASTTRASP1
  for(g=0;g<count;g++){
    // Init registers 
    int j;
    for(j=0;j<8;j++){
      r[roff+GROUP_PARALLELISM*j+g]=ib[8*g+j];
    }
  }
#else
  trasp_N_8((unsigned char *)&r[roff],(unsigned char *)ib,count);
#endif
//dump_mem("OLD r[roff]",&r[roff],GROUP_PARALLELISM*8,GROUP_PARALLELISM);

  // loop over kk[55]..kk[0]
  for(i=55;i>=0;i--){
    {
      MEMALIGN batch tkkmulti=kkmulti[i];
      batch *si=(batch *)sbox_in;
      batch *r6_N=(batch *)(r+roff+GROUP_PARALLELISM*6);
      for(g=0;g<count_all/BYTES_PER_BATCH;g++){
        si[g]=B_FFXOR(tkkmulti,r6_N[g]);              //FIXME: introduce FASTBATCH?
      }
    }

    // table lookup, this works on only one byte at a time
    // most difficult part of all
    // - can't be parallelized
    // - can't be synthetized through boolean terms (8 input bits are too many)
    for(g=0;g<count_all;g++){
      sbox_out[g]=block_sbox[sbox_in[g]];
    }

    // bit permutation
    {
      unsigned char *po=(unsigned char *)perm_out;
      unsigned char *so=(unsigned char *)sbox_out;
//dump_mem("pre perm ",(unsigned char *)so,GROUP_PARALLELISM,GROUP_PARALLELISM);
      for(g=0;g<count_all;g+=BYTES_PER_BATCH){
        MEMALIGN batch in,out;
        in=*(batch *)&so[g];

        out=B_FFOR(
	    B_FFOR(
	    B_FFOR(
	    B_FFOR(
	    B_FFOR(
	           B_FFSH8L(B_FFAND(in,B_FFN_ALL_29()),1),
	           B_FFSH8L(B_FFAND(in,B_FFN_ALL_02()),6)),
	           B_FFSH8L(B_FFAND(in,B_FFN_ALL_04()),3)),
	           B_FFSH8R(B_FFAND(in,B_FFN_ALL_10()),2)),
	           B_FFSH8R(B_FFAND(in,B_FFN_ALL_40()),6)),
	           B_FFSH8R(B_FFAND(in,B_FFN_ALL_80()),4));

        *(batch *)&po[g]=out;
      }
//dump_mem("post perm",(unsigned char *)po,GROUP_PARALLELISM,GROUP_PARALLELISM);
    }

    roff-=GROUP_PARALLELISM; /* virtual shift of registers */

#if 0
/* one by one */
    for(g=0;g<count_all;g++){
      r[roff+GROUP_PARALLELISM*0+g]=r[roff+GROUP_PARALLELISM*8+g]^sbox_out[g];
      r[roff+GROUP_PARALLELISM*6+g]^=perm_out[g];
      r[roff+GROUP_PARALLELISM*4+g]^=r[roff+GROUP_PARALLELISM*0+g];
      r[roff+GROUP_PARALLELISM*3+g]^=r[roff+GROUP_PARALLELISM*0+g];
      r[roff+GROUP_PARALLELISM*2+g]^=r[roff+GROUP_PARALLELISM*0+g];
    }
#else
    for(g=0;g<count_all;g+=BEST_SPAN){
      XOR_BEST_BY(&r[roff+GROUP_PARALLELISM*0+g],&r[roff+GROUP_PARALLELISM*8+g],&sbox_out[g]);
      XOREQ_BEST_BY(&r[roff+GROUP_PARALLELISM*6+g],&perm_out[g]);
      XOREQ_BEST_BY(&r[roff+GROUP_PARALLELISM*4+g],&r[roff+GROUP_PARALLELISM*0+g]);
      XOREQ_BEST_BY(&r[roff+GROUP_PARALLELISM*3+g],&r[roff+GROUP_PARALLELISM*0+g]);
      XOREQ_BEST_BY(&r[roff+GROUP_PARALLELISM*2+g],&r[roff+GROUP_PARALLELISM*0+g]);
    }
#endif
  }

#define FASTTRASP2
#ifndef FASTTRASP2
  for(g=0;g<count;g++){
    // Copy results
    int j;
    for(j=0;j<8;j++){
      bd[8*g+j]=r[roff+GROUP_PARALLELISM*j+g];
    }
  }
#else
  trasp_8_N((unsigned char *)&r[roff],(unsigned char *)bd,count);
#endif
}

//-----------------------------------EXTERNAL INTERFACE


//-----set control words

static void schedule_key(struct csa_key_t *key, const unsigned char *pk){
  // could be made faster, but is not run often
  int bi,by;
  int i,j;
// key
  memcpy(key->ck,pk,8);
// precalculations for stream
  key_schedule_stream(key->ck,key->iA,key->iB);
  for(by=0;by<8;by++){
    for(bi=0;bi<8;bi++){
      key->ck_g[by][bi]=(key->ck[by]&(1<<bi))?FF1():FF0();
    }
  }
  for(by=0;by<8;by++){
    for(bi=0;bi<4;bi++){
      key->iA_g[by][bi]=(key->iA[by]&(1<<bi))?FF1():FF0();
      key->iB_g[by][bi]=(key->iB[by]&(1<<bi))?FF1():FF0();
    }
  }
// precalculations for block
  key_schedule_block(key->ck,key->kk);
  for(i=0;i<56;i++){
    for(j=0;j<BYTES_PER_BATCH;j++){
      *(((unsigned char *)&key->kkmulti[i])+j)=key->kk[i];
    }
  }
}

extern void FUNC(set_control_words)(void *keys, const unsigned char *ev, const unsigned char *od);

void FUNC(set_control_words)(void *keys, const unsigned char *ev, const unsigned char *od)
{
  schedule_key(&((struct csa_keys_t *)keys)->even,ev);
  schedule_key(&((struct csa_keys_t *)keys)->odd,od);
}

extern void FUNC(set_even_control_word)(void *keys, const unsigned char *pk);

void FUNC(set_even_control_word)(void *keys, const unsigned char *pk)
{
  schedule_key(&((struct csa_keys_t *)keys)->even,pk);
}

extern void FUNC(set_odd_control_word)(void *keys, const unsigned char *pk);

void FUNC(set_odd_control_word)(void *keys, const unsigned char *pk){
  schedule_key(&((struct csa_keys_t *)keys)->odd,pk);
}

//-----get internal parallelism

extern int FUNC(get_internal_parallelism)(void);

int FUNC(get_internal_parallelism)(void)
{
  return GROUP_PARALLELISM;
}

//-----get suggested cluster size

extern int FUNC(get_suggested_cluster_size)(void);

int FUNC(get_suggested_cluster_size)(void)
{
  int r;
  r=GROUP_PARALLELISM+GROUP_PARALLELISM/10;
  if(r<GROUP_PARALLELISM+5) r=GROUP_PARALLELISM+5;
  return r;
}

//-----key structure

extern void *FUNC(get_key_struct)(void);
void *FUNC(get_key_struct)(void)
{
  struct csa_keys_t *keys=(struct csa_keys_t *)MALLOC(sizeof(struct csa_keys_t));
  if(keys) {
    static const unsigned char pk[8] = { 0,0,0,0,0,0,0,0 };
    FUNC(set_control_words)(keys,pk,pk);
    }
  return keys;
}

extern void FUNC(free_key_struct)(void *keys);
void FUNC(free_key_struct)(void *keys)
{
  return FREE(keys);
}



//-----get control words
#if 0
void get_control_words(void *keys, unsigned char *even, unsigned char *odd){
  memcpy(even,&((struct csa_keys_t *)keys)->even.ck,8);
  memcpy(odd,&((struct csa_keys_t *)keys)->odd.ck,8);
}
#endif

//----- decrypt

extern int FUNC(decrypt_packets)(void *keys, unsigned char **cluster);
int FUNC(decrypt_packets)(void *keys, unsigned char **cluster)
{
  // statistics, currently unused
  int stat_no_scramble=0;
  int stat_reserved=0;
  int stat_decrypted[2]={0,0};
  int stat_decrypted_mini=0;
  unsigned char **clst;
  unsigned char **clst2;
  int grouped;
  int group_ev_od;
  int advanced;
  int can_advance;
  unsigned char *g_pkt[GROUP_PARALLELISM];
  int g_len[GROUP_PARALLELISM];
  int g_offset[GROUP_PARALLELISM];
  int g_n[GROUP_PARALLELISM];
  int g_residue[GROUP_PARALLELISM];
  unsigned char *pkt;
  int xc0,ev_od,len,offset,n,residue;
  struct csa_key_t* k;
  int i,j,iter,g;
  int t23,tsmall;
  int alive[24];
//icc craziness  int pad1=0; //////////align! FIXME
  unsigned char *encp[GROUP_PARALLELISM];
  MEMALIGN unsigned char stream_in[GROUP_PARALLELISM*8];
  MEMALIGN unsigned char stream_out[GROUP_PARALLELISM*8];
  MEMALIGN unsigned char ib[GROUP_PARALLELISM*8];
  MEMALIGN unsigned char block_out[GROUP_PARALLELISM*8];
  struct stream_regs regs;

//icc craziness  i=(int)&pad1;//////////align!!! FIXME

  // build a list of packets to be processed
  clst=cluster;
  grouped=0;
  advanced=0;
  can_advance=1;
  group_ev_od=-1; // silence incorrect compiler warning
  pkt=*clst;
  do{ // find a new packet
    if(grouped==GROUP_PARALLELISM){
      // full
      break;
    }
    if(pkt==NULL){
      // no more ranges
      break;
    }
    if(pkt>=*(clst+1)){
      // out of this range, try next
      clst++;clst++;
      pkt=*clst;
      continue;
    }

    do{ // handle this packet
      xc0=pkt[3]&0xc0;
      DBG(fprintf(stderr,"   exam pkt=%p, xc0=%02x, can_adv=%i\n",pkt,xc0,can_advance));
      if(xc0==0x00){
        DBG(fprintf(stderr,"skip clear pkt %p (can_advance is %i)\n",pkt,can_advance));
        advanced+=can_advance;
        stat_no_scramble++;
        break;
      }
      if(xc0==0x40){
        DBG(fprintf(stderr,"skip reserved pkt %p (can_advance is %i)\n",pkt,can_advance));
        advanced+=can_advance;
        stat_reserved++;
        break;
      }
      if(xc0==0x80||xc0==0xc0){ // encrypted
        ev_od=(xc0&0x40)>>6; // 0 even, 1 odd
        if(grouped==0) group_ev_od=ev_od; // this group will be all even (or odd)
        if(group_ev_od==ev_od){ // could be added to group
          pkt[3]&=0x3f;  // consider it decrypted now
          if(pkt[3]&0x20){ // incomplete packet
            offset=4+pkt[4]+1;
            len=188-offset;
            n=len>>3;
            residue=len-(n<<3);
            if(n==0){ // decrypted==encrypted!
              DBG(fprintf(stderr,"DECRYPTED MINI! (can_advance is %i)\n",can_advance));
              advanced+=can_advance;
              stat_decrypted_mini++;
              break; // this doesn't need more processing
            }
          }else{
            len=184;
            offset=4;
            n=23;
            residue=0;
          }
          g_pkt[grouped]=pkt;
          g_len[grouped]=len;
          g_offset[grouped]=offset;
          g_n[grouped]=n;
          g_residue[grouped]=residue;
          DBG(fprintf(stderr,"%2i: eo=%i pkt=%p len=%03i n=%2i residue=%i\n",grouped,ev_od,pkt,len,n,residue));
          grouped++;
          advanced+=can_advance;
          stat_decrypted[ev_od]++;
        }
        else{
          can_advance=0;
          DBG(fprintf(stderr,"skip pkt %p and can_advance set to 0\n",pkt));
          break; // skip and go on
        }
      }
    } while(0);

    if(can_advance){
      // move range start forward
      *clst+=188;
    }
    // next packet, if there is one
    pkt+=188;
  } while(1);
  DBG(fprintf(stderr,"-- result: grouped %i pkts, advanced %i pkts\n",grouped,advanced));

  // delete empty ranges and compact list
  clst2=cluster;
  for(clst=cluster;*clst!=NULL;clst+=2){
    // if not empty
    if(*clst<*(clst+1)){
      // it will remain 
      *clst2=*clst;
      *(clst2+1)=*(clst+1);
      clst2+=2;
    }
  }
  *clst2=NULL;

  if(grouped==0){
    // no processing needed
    return advanced;
  }

  //  sort them, longest payload first
  //  we expect many n=23 packets and a few n<23
  DBG(fprintf(stderr,"PRESORTING\n"));
  for(i=0;i<grouped;i++){
    DBG(fprintf(stderr,"%2i of %2i: pkt=%p len=%03i n=%2i residue=%i\n",i,grouped,g_pkt[i],g_len[i],g_n[i],g_residue[i]));
    }
  // grouped is always <= GROUP_PARALLELISM

#define g_swap(a,b) \
    pkt=g_pkt[a]; \
    g_pkt[a]=g_pkt[b]; \
    g_pkt[b]=pkt; \
\
    len=g_len[a]; \
    g_len[a]=g_len[b]; \
    g_len[b]=len; \
\
    offset=g_offset[a]; \
    g_offset[a]=g_offset[b]; \
    g_offset[b]=offset; \
\
    n=g_n[a]; \
    g_n[a]=g_n[b]; \
    g_n[b]=n; \
\
    residue=g_residue[a]; \
    g_residue[a]=g_residue[b]; \
    g_residue[b]=residue;

  // step 1: move n=23 packets before small packets
  t23=0;
  tsmall=grouped-1;
  for(;;){
    for(;t23<grouped;t23++){
      if(g_n[t23]!=23) break;
    }
DBG(fprintf(stderr,"t23 after for =%i\n",t23));
    
    for(;tsmall>=0;tsmall--){
      if(g_n[tsmall]==23) break;
    }
DBG(fprintf(stderr,"tsmall after for =%i\n",tsmall));
    
    if(tsmall-t23<1) break;
    
DBG(fprintf(stderr,"swap t23=%i,tsmall=%i\n",t23,tsmall));

    g_swap(t23,tsmall);

    t23++;
    tsmall--;
DBG(fprintf(stderr,"new t23=%i,tsmall=%i\n\n",t23,tsmall));
  }
  DBG(fprintf(stderr,"packets with n=23, t23=%i   grouped=%i\n",t23,grouped));
  DBG(fprintf(stderr,"MIDSORTING\n"));
  for(i=0;i<grouped;i++){
    DBG(fprintf(stderr,"%2i of %2i: pkt=%p len=%03i n=%2i residue=%i\n",i,grouped,g_pkt[i],g_len[i],g_n[i],g_residue[i]));
    }

  // step 2: sort small packets in decreasing order of n (bubble sort is enough)
  for(i=t23;i<grouped;i++){
    for(j=i+1;j<grouped;j++){
      if(g_n[j]>g_n[i]){
        g_swap(i,j);
      }
    }
  }
  DBG(fprintf(stderr,"POSTSORTING\n"));
  for(i=0;i<grouped;i++){
    DBG(fprintf(stderr,"%2i of %2i: pkt=%p len=%03i n=%2i residue=%i\n",i,grouped,g_pkt[i],g_len[i],g_n[i],g_residue[i]));
    }

  // we need to know how many packets need 23 iterations, how many 22...
  for(i=0;i<=23;i++){
    alive[i]=0;
  }
  // count
  alive[23-1]=t23;
  for(i=t23;i<grouped;i++){
    alive[g_n[i]-1]++;
  }
  // integrate
  for(i=22;i>=0;i--){
    alive[i]+=alive[i+1];
  }
  DBG(fprintf(stderr,"ALIVE\n"));
  for(i=0;i<=23;i++){
    DBG(fprintf(stderr,"alive%2i=%i\n",i,alive[i]));
    }

  // choose key
  if(group_ev_od==0){
    k=&((struct csa_keys_t *)keys)->even;
  }
  else{
    k=&((struct csa_keys_t *)keys)->odd;
  }

  //INIT
//#define INITIALIZE_UNUSED_INPUT
#ifdef INITIALIZE_UNUSED_INPUT
// unnecessary zeroing.
// without this, we operate on uninitialized memory
// when grouped<GROUP_PARALLELISM, but it's not a problem,
// as final results will be discarded.
// random data makes debugging sessions difficult.
  for(j=0;j<GROUP_PARALLELISM*8;j++) stream_in[j]=0;
DBG(fprintf(stderr,"--- WARNING: you could gain speed by not initializing unused memory ---\n"));
#else
DBG(fprintf(stderr,"--- WARNING: DEBUGGING IS MORE DIFFICULT WHEN PROCESSING RANDOM DATA CHANGING AT EVERY RUN! ---\n"));
#endif

  for(g=0;g<grouped;g++){
    encp[g]=g_pkt[g];
    DBG(fprintf(stderr,"header[%i]=%p (%02x)\n",g,encp[g],*(encp[g])));
    encp[g]+=g_offset[g]; // skip header
    FFTABLEIN(stream_in,g,encp[g]);
  }
//dump_mem("stream_in",stream_in,GROUP_PARALLELISM*8,BYPG);


  // ITER 0
DBG(fprintf(stderr,">>>>>ITER 0\n"));
  iter=0;
  stream_cypher_group_init(&regs,k->iA_g,k->iB_g,stream_in);
  // fill first ib
  for(g=0;g<alive[iter];g++){
    COPY_8_BY(ib+8*g,encp[g]);
  }
DBG(dump_mem("IB ",ib,8*alive[iter],8));
  // ITER 1..N-1
  for (iter=1;iter<23&&alive[iter-1]>0;iter++){
DBG(fprintf(stderr,">>>>>ITER %i\n",iter));
    // alive and just dead packets: calc block
    block_decypher_group(k->kkmulti,ib,block_out,alive[iter-1]);
DBG(dump_mem("BLO_ib ",block_out,8*alive[iter-1],8));
    // all packets (dead too): calc stream
    stream_cypher_group_normal(&regs,stream_out);
//dump_mem("stream_out",stream_out,GROUP_PARALLELISM*8,BYPG);

    // alive packets: calc ib
    for(g=0;g<alive[iter];g++){
      FFTABLEOUT(ib+8*g,stream_out,g);
DBG(dump_mem("stream_out_ib ",ib+8*g,8,8));
// XOREQ8BY gcc bug? 2x4 ok, 8 ko    UPDATE: result ok but speed 1-2% slower (!!!???)
#if 1
      XOREQ_4_BY(ib+8*g,encp[g]+8);
      XOREQ_4_BY(ib+8*g+4,encp[g]+8+4);
#else
      XOREQ_8_BY(ib+8*g,encp[g]+8);
#endif
DBG(dump_mem("after_stream_xor_ib ",ib+8*g,8,8));
    }
    // alive packets: decrypt data
    for(g=0;g<alive[iter];g++){
DBG(dump_mem("before_ib_decrypt_data ",encp[g],8,8));
      XOR_8_BY(encp[g],ib+8*g,block_out+8*g);
DBG(dump_mem("after_ib_decrypt_data ",encp[g],8,8));
    }
    // just dead packets: write decrypted data
    for(g=alive[iter];g<alive[iter-1];g++){
DBG(dump_mem("jd_before_ib_decrypt_data ",encp[g],8,8));
      COPY_8_BY(encp[g],block_out+8*g);
DBG(dump_mem("jd_after_ib_decrypt_data ",encp[g],8,8));
    }
    // just dead packets: decrypt residue
    for(g=alive[iter];g<alive[iter-1];g++){
DBG(dump_mem("jd_before_decrypt_residue ",encp[g]+8,g_residue[g],g_residue[g]));
      FFTABLEOUTXORNBY(g_residue[g],encp[g]+8,stream_out,g);
DBG(dump_mem("jd_after_decrypt_residue ",encp[g]+8,g_residue[g],g_residue[g]));
    }
    // alive packets: pointers++
    for(g=0;g<alive[iter];g++) encp[g]+=8;
  };
  // ITER N
DBG(fprintf(stderr,">>>>>ITER 23\n"));
  iter=23;
  // calc block
  block_decypher_group(k->kkmulti,ib,block_out,alive[iter-1]);
DBG(dump_mem("23BLO_ib ",block_out,8*alive[iter-1],8));
  // just dead packets: write decrypted data
  for(g=alive[iter];g<alive[iter-1];g++){
DBG(dump_mem("23jd_before_ib_decrypt_data ",encp[g],8,8));
    COPY_8_BY(encp[g],block_out+8*g);
DBG(dump_mem("23jd_after_ib_decrypt_data ",encp[g],8,8));
  }
  // no residue possible
  // so do nothing

  DBG(fprintf(stderr,"returning advanced=%i\n",advanced));

  M_EMPTY(); // restore CPU multimedia state

  return advanced;
}
