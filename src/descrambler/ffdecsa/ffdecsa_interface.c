/*
 * CPU detection code, extracted from mmx.h
 * (c)1997-99 by H. Dietz and R. Fisher
 * Converted to C and improved by Fabrice Bellard.
 *
 * This file is part of Tvheadend.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "tvheadend.h"
#include "FFdecsa.h"



typedef struct {
  int (*get_internal_parallelism)(void);
  int (*get_suggested_cluster_size)(void);
  void *(*get_key_struct)(void);
  void (*free_key_struct)(void *keys);
  void (*set_control_words)(void *keys, const unsigned char *even, const unsigned char *odd);

  void (*set_even_control_word)(void *keys, const unsigned char *even);
  void (*set_odd_control_word)(void *keys, const unsigned char *odd);
  int (*decrypt_packets)(void *keys, unsigned char **cluster);

} csafuncs_t;


#define MAKEFUNCS(x) \
extern int get_internal_parallelism_##x(void);\
extern int get_suggested_cluster_size_##x(void);\
extern void *get_key_struct_##x(void);\
extern void free_key_struct_##x(void *keys);\
extern void set_control_words_##x(void *keys, const unsigned char *even, const unsigned char *odd);\
extern void set_even_control_word_##x(void *keys, const unsigned char *even);\
extern void set_odd_control_word_##x(void *keys, const unsigned char *odd);\
extern int decrypt_packets_##x(void *keys, unsigned char **cluster);\
static csafuncs_t funcs_##x = { \
  &get_internal_parallelism_##x,\
  &get_suggested_cluster_size_##x,\
  &get_key_struct_##x,\
  &free_key_struct_##x,\
  &set_control_words_##x,\
  &set_even_control_word_##x,\
  &set_odd_control_word_##x,\
  &decrypt_packets_##x\
};

MAKEFUNCS(32int);
#ifdef CONFIG_MMX
MAKEFUNCS(64mmx);
#endif

#ifdef CONFIG_SSE2
MAKEFUNCS(128sse2);
#endif

static csafuncs_t current;




#if defined(__x86_64__)
#    define REG_a "rax"
#    define REG_b "rbx"
#    define REG_c "rcx"
#    define REG_d "rdx"
#    define REG_D "rdi"
#    define REG_S "rsi"
#    define PTR_SIZE "8"
typedef int64_t x86_reg;

#    define REG_SP "rsp"
#    define REG_BP "rbp"
#    define REGBP   rbp
#    define REGa    rax
#    define REGb    rbx
#    define REGc    rcx
#    define REGd    rdx
#    define REGSP   rsp

#elif defined(__i386__)

#    define REG_a "eax"
#    define REG_b "ebx"
#    define REG_c "ecx"
#    define REG_d "edx"
#    define REG_D "edi"
#    define REG_S "esi"
#    define PTR_SIZE "4"
typedef int32_t x86_reg;

#    define REG_SP "esp"
#    define REG_BP "ebp"
#    define REGBP   ebp
#    define REGa    eax
#    define REGb    ebx
#    define REGc    ecx
#    define REGd    edx
#    define REGSP   esp
#else
typedef int x86_reg;
#endif

#if defined(__i386__) || defined(__x86_64__)
static inline void
native_cpuid(unsigned int *eax, unsigned int *ebx,
             unsigned int *ecx, unsigned int *edx)
{
  /* saving ebx is necessary for PIC compatibility */
  asm volatile("mov %%"REG_b", %%"REG_S"\n\t"
               "cpuid\n\t"
               "xchg %%"REG_b", %%"REG_S
               : "=a" (*eax),
                 "=S" (*ebx),
                 "=c" (*ecx),
                 "=d" (*edx)
               : "0" (*eax), "2" (*ecx));
}
#endif

void
ffdecsa_init(void)
{
  current = funcs_32int;


#if defined(__i386__) || defined(__x86_64__)

  unsigned int eax, ebx, ecx, edx;
  unsigned int max_std_level, std_caps;
  
#if defined(__i386__)

  x86_reg a, c;
  __asm__ volatile (
		    /* See if CPUID instruction is supported ... */
		    /* ... Get copies of EFLAGS into eax and ecx */
		    "pushfl\n\t"
		    "pop %0\n\t"
		    "mov %0, %1\n\t"

		    /* ... Toggle the ID bit in one copy and store */
		    /*     to the EFLAGS reg */
		    "xor $0x200000, %0\n\t"
		    "push %0\n\t"
		    "popfl\n\t"

		    /* ... Get the (hopefully modified) EFLAGS */
		    "pushfl\n\t"
		    "pop %0\n\t"
		    : "=a" (a), "=c" (c)
		    :
		    : "cc"
		    );

  if (a != c) {
#endif
    eax = ebx = ecx = edx = 0;
    native_cpuid(&eax, &ebx, &ecx, &edx);
    max_std_level = eax;

    if(max_std_level >= 1){
      eax = 1;
      native_cpuid(&eax, &ebx, &ecx, &edx);
      std_caps = edx;

#ifdef CONFIG_SSE2
      if (std_caps & (1<<26)) {
	current = funcs_128sse2;
	tvhlog(LOG_INFO, "CSA", "Using SSE2 128bit parallel descrambling");
	return;
      }
#endif

#ifdef CONFIG_MMX
      if (std_caps & (1<<23)) {
	current = funcs_64mmx;
	tvhlog(LOG_INFO, "CSA", "Using MMX 64bit parallel descrambling");
	return;
      }
#endif
    }
#if defined(__i386__)
  }
#endif
#endif

  tvhlog(LOG_INFO, "CSA", "Using 32bit parallel descrambling");
}


int
get_internal_parallelism(void)
{
  return current.get_internal_parallelism();
}
int
get_suggested_cluster_size(void)
{
  return current.get_suggested_cluster_size();
}

void *
get_key_struct(void)
{
  return current.get_key_struct();
}
void
free_key_struct(void *keys)
{
  current.free_key_struct(keys);
}

void
set_even_control_word(void *keys, const unsigned char *even)
{
  current.set_even_control_word(keys, even);
}

void
set_odd_control_word(void *keys, const unsigned char *odd)
{
  current.set_odd_control_word(keys, odd);
}

int
decrypt_packets(void *keys, unsigned char **cluster)
{
  return current.decrypt_packets(keys, cluster);
}
