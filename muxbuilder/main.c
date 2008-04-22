#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

int pass;
char *type;

struct strtab {
  const char *v1;
  const char *v2;
};

static struct strtab fectab[] = {
  { "NONE", "FEC_NONE" },
  { "1/1",  "FEC_AUTO" },
  { "1/2",  "FEC_1_2" },
  { "2/3",  "FEC_2_3" },
  { "3/4",  "FEC_3_4" },
  { "4/5",  "FEC_4_5" },
  { "5/6",  "FEC_5_6" },
  { "6/7",  "FEC_6_7" },
  { "7/8",  "FEC_7_8" },
  { "8/9",  "FEC_8_9" },
  { "AUTO", "FEC_AUTO" }
};

static struct strtab qamtab[] = {
  { "QPSK",   "QPSK" },
  { "QAM16",  "QAM_16" },
  { "QAM32",  "QAM_32" },
  { "QAM64",  "QAM_64" },
  { "QAM128", "QAM_128" },
  { "QAM256", "QAM_256" },
  { "AUTO",   "QAM_AUTO" },
  { "8VSB",   "VSB_8" },
  { "16VSB",  "VSB_16" }
};

static struct strtab bwtab[] = {
  { "8MHz", "BANDWIDTH_8_MHZ" },
  { "7MHz", "BANDWIDTH_7_MHZ" },
  { "6MHz", "BANDWIDTH_6_MHZ" },
  { "AUTO", "BANDWIDTH_AUTO" }
};

static struct strtab modetab[] = {
  { "2k",   "TRANSMISSION_MODE_2K" },
  { "8k",   "TRANSMISSION_MODE_8K" },
  { "AUTO", "TRANSMISSION_MODE_AUTO" }
};

static struct strtab guardtab[] = {
  { "1/32", "GUARD_INTERVAL_1_32" },
  { "1/16", "GUARD_INTERVAL_1_16" },
  { "1/8",  "GUARD_INTERVAL_1_8" },
  { "1/4",  "GUARD_INTERVAL_1_4" },
  { "AUTO", "GUARD_INTERVAL_AUTO" },
};

static struct strtab hiertab[] = {
  { "NONE", "HIERARCHY_NONE" },
  { "1",    "HIERARCHY_1" },
  { "2",    "HIERARCHY_2" },
  { "4",    "HIERARCHY_4" },
  { "AUTO", "HIERARCHY_AUTO" }
};


static const char *
str2str0(const char *str, struct strtab tab[], int l)
{
  int i;
  for(i = 0; i < l; i++)
    if(!strcasecmp(str, tab[i].v1))
      return tab[i].v2;
  fprintf(stderr, "Warning, cannot translate %s\n", str);
  return "#error";
}

#define str2str(str, tab) str2str0(str, tab, sizeof(tab) / sizeof(tab[0]))




static void
dvb_t_config(const char *l)
{
  unsigned long freq;
  char bw[20], fec[20], fec2[20], qam[20], mode[20], guard[20], hier[20];
  int r;

  r = sscanf(l, "%lu %10s %10s %10s %10s %10s %10s %10s",
	     &freq, bw, fec, fec2, qam, mode, guard, hier);

  if(r != 8)
    return;

  printf("\t{.freq = %lu, "
	 ".bw = %s, "
	 ".constellation = %s, "
	 ".fechp = %s, "
	 ".feclp = %s, "
	 ".tmode = %s, "
	 ".guard = %s, "
	 ".hierarchy = %s},\n "
	 ,
	 freq, 
	 str2str(bw, bwtab), 
	 str2str(qam, qamtab),
	 str2str(fec, fectab),
	 str2str(fec2, fectab),
	 str2str(mode, modetab),
	 str2str(guard, guardtab),
	 str2str(hier, hiertab));
}




static void
dvb_s_config(const char *l)
{
  unsigned long freq, symrate;
  char fec[20], polarisation;
  int r;

  r = sscanf(l, "%lu %c %lu %s",
	     &freq, &polarisation, &symrate, fec);

  if(r != 4)
    return;

  printf("\t{"
	 ".freq = %lu, .symrate = %lu, .fec = %s, .polarisation = '%c'},\n",
	 freq, symrate, str2str(fec, fectab), polarisation);
}




static void
dvb_c_config(const char *l)
{
  unsigned long freq, symrate;
  char fec[20], qam[20];
  int r;

  r = sscanf(l, "%lu %lu %s %s",
	     &freq, &symrate, fec, qam);

  if(r != 4)
    return;

  printf("\t{ "
	 ".freq = %lu, .symrate = %lu, .fec = %s, .constellation = %s},\n",
	 freq, symrate, str2str(fec, fectab), str2str(qam, qamtab));
}




static void
convert_file(char *fname)
{
  FILE *fp;
  char line[200];
  int l;
  char c, *s;
  char smartname[200];
  const char *bn;
  int gotcomment = 0;

  fp = fopen(fname, "r");
  if(fp == NULL) {
    fprintf(stderr, "Unable to open file %s -- %s", fname, strerror(errno));
    return;
  }

  l = 0;
  bn = basename(fname);
  while(*bn) {
    c = *bn++;
    smartname[l++] = isalnum(c) ? c : '_';
  }
  smartname[l] = 0;

  if(pass == 1) {
    printf("struct mux muxlist_%s_%s[] = {\n", type, smartname);
  }

  if(pass == 2) {
    printf("{\n");
    printf("\t.type = %s,\n", type);
    printf("\t.name = \"%s\",\n", basename(fname));
    printf("\t.muxes = muxlist_%s_%s,\n", type, smartname);
    printf("\t.nmuxes = sizeof(muxlist_%s_%s) / sizeof(struct mux),\n",
	   type, smartname);
    printf("\t.comment = ");
  }

  while(!feof(fp)) {
    memset(line, 0, sizeof(line));

    if(fgets(line, sizeof(line) - 1, fp) == NULL)
      break;

    l = strlen(line);
    while(l > 0 && line[l - 1] < 32)
      line[--l] = 0;

    switch(line[0]) {
    case '#':
      if(gotcomment)
	break;

      if(pass != 2)
	break;
      s = line + 2;
      if(strstr(s, " freq "))
	break;

      printf("\"%s\"\n", s);
      gotcomment = 1;
      break;

    case 'C':
      if(pass == 1)
	dvb_c_config(line + 1);
      break;

    case 'T':
      if(pass == 1)
	dvb_t_config(line + 1);
      break;

    case 'S':
      if(pass == 1)
	dvb_s_config(line + 1);
      break;

    default:
      break;
    }
  }
  if(pass == 2) {
    if(gotcomment == 0)
      printf("\"\"\n");
    printf("},\n");
  }

  if(pass == 1) {
    printf("};\n\n");
  }

  fclose(fp);
}

int 
main(int argc, char **argv)
{
  int i;

  if(argc < 3)
    return 1;

  pass = atoi(argv[1]);
  if(pass < 1 || pass > 2)
    return 2;

  type = argv[2];

  for(i = 3; i < argc; i++) {
    convert_file(argv[i]);
  }

  return 0;
}
