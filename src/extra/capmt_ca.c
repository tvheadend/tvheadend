/* Compile as follows:
 * gcc -O -fbuiltin -fomit-frame-pointer -fPIC -shared -o capmt_ca.so capmt_ca.c -ldl
 *
 * Will then send CWs to Tvheadend
 *
 * Taken from the VDR Sources
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

//#define DEBUG

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)

#include <dlfcn.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/dvb/ca.h>
#include <linux/dvb/dmx.h>
#include <linux/dvb/frontend.h>
#include <linux/ioctl.h>
#include <stdio.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <syslog.h>
#include <errno.h>

#if defined(RTLD_NEXT)
#define REAL_LIBC RTLD_NEXT
#else
#define REAL_LIBC ((void *) -1L)
#endif

#define PORT 9000
#define MAX_CA  4

static int cafd[MAX_CA]  = {-1,-1,-1,-1};
static int cafdc[MAX_CA] = {0,0,0,0};

#define MAX_INDEX 64
#define KEY_SIZE  8
#define INFO_SIZE (2+KEY_SIZE+KEY_SIZE)
#define EVEN_OFF  (2)
#define ODD_OFF   (2+KEY_SIZE)

static unsigned char ca_info[MAX_CA][MAX_INDEX][INFO_SIZE];

#define CA_BASE		"/dev/dvb/adapter0/ca"
#define DEMUX_BASE	"/dev/dvb/adapter0/demux"
#define DEMUX_MAP	"/dev/dvb/adapter%d/demux0"
#define CPUINFO_BASE	"/proc/cpuinfo"
#define CPUINFO_MAP	"/tmp/cpuinfo"

#if 1
#define DBG(x...)
#define INF(x...) {syslog(LOG_INFO, x);}
#define ERR(x...) {syslog(LOG_ERR, x);}
#else
#define DBG(...) { printf(__VA_ARGS__); printf("\n"); }
#define INF(...) { printf(__VA_ARGS__); printf("\n"); }
#define ERR(...) { printf(__VA_ARGS__); printf("\n"); }
#endif
int proginfo=0;
#define PROGINFO() {if(!proginfo){syslog(LOG_INFO,"capmt_ca.so for camd clients");proginfo=1;}}

int sendcw(int fd, unsigned char *buff) {
  DBG(">>>>>>>> sendcw %02x%02x %02x%02x%02x%02x%02x%02x%02x%02x %02x%02x%02x%02x%02x%02x%02x%02x", buff[0], buff[1], buff[2], buff[3], buff[4], buff[5], buff[6], buff[7], buff[8], buff[9], buff[10], buff[11], buff[12], buff[13], buff[14], buff[15], buff[16], buff[17], buff[18]);
  if ( send(fd,buff, 18, 0) == -1 ) { 
  	ERR("ca.so: Send cw failed %d", errno);
    return 0;
  } // if
  return 1;
} // sendcw

static int cactl (int fd, int cai, int request, void *argp) {
  ca_descr_t *ca = (ca_descr_t *)argp;
  ca_pid_t  *cpd = (ca_pid_t  *)argp;
  switch (request) {
    case CA_SET_DESCR:
      DBG(">>>>>>>> cactl CA_SET_DESCR fd %d cai %d req %d par %d idx %d %02x%02x%02x%02x%02x%02x%02x%02x", fd, cai, request, ca->parity, ca->index, ca->cw[0], ca->cw[1], ca->cw[2], ca->cw[3], ca->cw[4], ca->cw[5], ca->cw[6], ca->cw[7]);
      if(ca->parity==0)
        memcpy(&ca_info[cai][ca->index][EVEN_OFF],ca->cw,KEY_SIZE); // even key
      else if(ca->parity==1)
        memcpy(&ca_info[cai][ca->index][ODD_OFF],ca->cw,KEY_SIZE); // odd key
      else
        ERR("ca.so: Invalid parity %d in CA_SET_DESCR for ca id %d", ca->parity, cai);
      sendcw(fd, ca_info[cai][ca->index]);
      break;
    case CA_SET_PID:
      DBG(">>>>>>>> cactl CA_SET_PID fd %d cai %d req %d (%d %04x)", fd, cai, request, cpd->index, cpd->pid);
      if (cpd->index >=0 && cpd->index < MAX_INDEX) { 
        ca_info[cai][cpd->index][0] = (cpd->pid >> 0) & 0xff;
				ca_info[cai][cpd->index][1] = (cpd->pid >> 8) & 0xff;
      } else if (cpd->index == -1) {
        memset(&ca_info[cai], 0, sizeof(ca_info[cai]));
      } else
        ERR("ca.so: Invalid index %d in CA_SET_PID (%d) for ca id %d", cpd->index, MAX_INDEX, cai);
      return 1;
    case CA_RESET:
      DBG(">>>>>>>> cactl CA_RESET cai %d", cai);
      break;
    case CA_GET_CAP:
      DBG(">>>>>>>> cactl CA_GET_CAP cai %d", cai);
      break;
    case CA_GET_SLOT_INFO:
      DBG(">>>>>>>> cactl CA_GET_SLOT_INFO cai %d", cai);
      break;
    default:
      DBG(">>>>>>>> cactl unhandled req %d cai %d", request, cai);
      //errno = EINVAL;
      //return -1;
  } // switch
  return 0;
} // cactl

/* 
    CPU Model
    Brcm4380 V4.2  // DM8000
    Brcm7401 V0.0  // DM800
    MIPS 4KEc V4.8 // DM7025
*/

static const char *cpuinfo_file = "\
system type             : ATI XILLEON HDTV SUPERTOLL\n\
processor               : 0\n\
cpu model               : Brcm4380 V4.2\n\
BogoMIPS                : 297.98\n\
wait instruction        : yes\n\
microsecond timers      : yes\n\
tlb_entries             : 16\n\
extra interrupt vector  : yes\n\
hardware watchpoint     : yes\n\
VCED exceptions         : not available\n\
VCEI exceptions         : not available\n\
";

int write_cpuinfo (void) {
  static FILE* (*func) (const char *, const char *) = NULL;
  func = (FILE* (*) (const char *, const char *)) dlsym (REAL_LIBC, "fopen");
  FILE* file = func(CPUINFO_MAP, "w");
  
  
  if(file == NULL) {
     printf("error \"%s\" opening file\n", strerror(errno));
     return -1;
  }
  int ret = fwrite(cpuinfo_file, strlen(cpuinfo_file), 1, file);
  fclose(file);
  return 0;
}

FILE *fopen(const char *path, const char *mode){
  static FILE* (*func) (const char *, const char *) = NULL;
  write_cpuinfo();
  if (!func) func = (FILE* (*) (const char *, const char *)) dlsym (REAL_LIBC, "fopen");
  if (!strcmp(path, CPUINFO_BASE)) {
    INF("ca.so fopen %s mapped to %s", path, CPUINFO_MAP);
    return (*func) (CPUINFO_MAP, mode);
  } // if
  DBG(">>>>>>>> fopen %s", path);
  return (*func) (path, mode);
} // fopen
						  
int open (const char *pathname, int flags, ...) {
  static int (*func) (const char *, int, mode_t) = NULL;
  if (!func) func = (int (*) (const char *, int, mode_t)) dlsym (REAL_LIBC, "open");
  PROGINFO();
  
  va_list args;
  mode_t mode;

  va_start (args, flags);
  mode = va_arg (args, mode_t);
  va_end (args);

  if(strstr(pathname, CA_BASE)) {
    int cai=pathname[strlen(CA_BASE)]-'0';
    if((cai >= 0) && (cai < MAX_CA)) {
      DBG(">>>>>>>> open cafd[%d] flags %d %s (%d)", cai, flags, pathname, cafdc[cai]);
      if(cafd[cai]==-1) {
        cafd[cai] = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(cafd[cai]==-1) {
          ERR("Failed to open socket (%d)", errno);
        } else {
          struct sockaddr_in saddr;
          fcntl(cafd[cai],F_SETFL,O_NONBLOCK);
          bzero(&saddr,sizeof(saddr));
          saddr.sin_family = AF_INET;
          saddr.sin_port = htons(PORT);
          saddr.sin_addr.s_addr = inet_addr("127.0.0.1");
          int r = connect(cafd[cai], (struct sockaddr *) &saddr, sizeof(saddr));
          if (r<0) {
            ERR("Failed to connect socket (%d)", errno);
            close(cafd[cai]);
            cafd[cai]=-1;
          } // if
        } // if
      } // if
      if(cafd[cai]!=-1) 
        cafdc[cai]++;
      else 
        cafdc[cai] = 0;
      return (cafd[cai]);
    } // if      
  } else if (strstr(pathname, DEMUX_BASE)) {
    int cai=pathname[strlen(DEMUX_BASE)]-'0';
    if((cai >= 0) && (cai < MAX_CA)) {
      char dest[256];
      sprintf(dest, DEMUX_MAP, cai);
      int r = (*func) (dest, flags, mode);
      DBG(">>>>>>>> open %s mapped to %s fd %d", pathname, dest, r);
      return r;
    } // if
  }

  int r = (*func) (pathname, flags, mode);
  DBG(">>>>>>>> open %s", pathname);
  DBG(">>>>>>>> open fd %d", r);
  return r;
} // open

ssize_t read(int fd, void *buf, size_t count) {
  static ssize_t (*func)(int, void*, size_t) = NULL;
  if (!func) func = (ssize_t (*)(int, void*, size_t)) dlsym( REAL_LIBC, "read");

  DBG(">>>>>>>> read(fd=%d, buf=%x, count=%d)", fd, buf, count);

  ssize_t r = (*func)(fd, buf, count);
  DBG(">>>>>>>> read %lld bytes", r);

  return r;
}

int ioctl (int fd, int request, void *a) {
  static int (*func) (int, int, void *) = NULL;
  if (!func) func = (int (*) (int, int, void *)) dlsym (REAL_LIBC, "ioctl");
  int i;
  for(i=0;i<MAX_CA;i++)
    if (fd == cafd[i])
      return cactl (fd, i, request, a);
  return (*func) (fd, request, a); 
} // ioctl

int close (int fd) {
  static int (*func) (int) = NULL;
  if (!func) func = (int (*) (int)) dlsym (REAL_LIBC, "close");
  int i;
  for(i=0;i<MAX_CA;i++) {
    if (fd == cafd[i]) {
      DBG(">>>>>>>> close cafd[%d] (%d)", i, cafdc[i]);
      if(--cafdc[i]) return 0;
      cafd[i] = -1;
    } // if
  } // for
  return (*func) (fd);
} // close

#endif

