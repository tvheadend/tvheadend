#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

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




/**
 *
 */
typedef struct network {
  char *structname; 
  char *displayname;
  char *comment;
  struct network *next;
} network_t;


typedef struct region {
  char *shortname;
  char *longname;
  struct network *networks;
  struct region *next;
} region_t;

region_t *regions;


static region_t *
find_region(const char *name, const char *longname)
{
  region_t *c, *n, **p;

  for(c = regions; c != NULL; c = c->next)
    if(!strcmp(name, c->shortname))
      return c;

  n = malloc(sizeof(region_t));
  n->shortname = strdup(name);
  n->longname = strdup(longname);
  n->networks = NULL;

  if(regions == NULL)
    regions = n;
  else {
    p = &regions;
    while((c = *p) != NULL) {
      if(strcmp(c->longname, longname) > 0)
	break;
      p = &c->next;
    }
    n->next = *p;
    *p = n;
  }
  return n;
}



static network_t *
add_network(region_t *x, const char *name)
{
  network_t *m, *n, **p;

  n = calloc(1, sizeof(network_t));
  n->structname = strdup(name);

  if(x->networks == NULL)
    x->networks = n;
  else {
    p = &x->networks;
    while((m = *p) != NULL) {
      if(strcmp(m->structname, name) > 0)
	break;
      p = &m->next;
    }
    n->next = *p;
    *p = n;
  }
  return n;
}


static const struct {
  const char *code;
  const char *name;

} tldlist[] = {
  {"ad", "Andorra"},
  {"at", "Austria"},
  {"au", "Australia"},
  {"be", "Belgium"},
  {"ch", "Switzerland"},
  {"cz", "Czech Republic"},
  {"de", "Germany"},
  {"dk", "Denmark"},
  {"es", "Spain"},
  {"fi", "Finland"},
  {"fr", "France"},
  {"gr", "Greece"},
  {"hk", "Hong Kong"},
  {"hr", "Croatia"},
  {"is", "Iceland"},
  {"it", "Italy"},
  {"lu", "Luxembourg"},
  {"lv", "Latvia"},
  {"nl", "Netherlands"},
  {"no", "Norway"},
  {"nz", "New Zealand"},
  {"pl", "Poland"},
  {"se", "Sweden"},
  {"sk", "Slovakia"},
  {"tw", "Taiwan"},
  {"uk", "United Kingdom"},
};

static const char *
tldcode2longname(const char *tld)
{
  int i;
  for(i = 0; i < sizeof(tldlist) / sizeof(tldlist[0]); i++)
    if(!strcmp(tld, tldlist[i].code))
      return tldlist[i].name;
  
  fprintf(stderr, "Unable to translate tld %s\n", tld);
  exit(1);
}




static void
scan_file(char *fname)
{
  FILE *fp;
  char line[200];
  int l;
  char c, *s;
  char smartname[200];
  char *bn;
  int gotcomment = 0;
  region_t *co;
  network_t *ne;
  char *name, *displayname;

  char buf[100];

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

  name = basename(fname);

  if(!strcmp(type, "DVBS")) {
    displayname = name;
    co = find_region("geo", "Geosynchronous Orbit");

  } else {
    displayname = name + 3;
    buf[0] = name[0];
    buf[1] = name[1];
    buf[2] = 0;
    co = find_region(buf, tldcode2longname(buf));
  }

  snprintf(buf, sizeof(buf), "%s_%s", type, smartname);

  printf("static const struct mux muxes_%s[] = {\n", buf);

  ne = add_network(co, buf);
  bn = ne->displayname = strdup(displayname);

  while(*bn) {
    if(*bn == '_') *bn = ' ';
    bn++;
  }

  //  ne->muxlistname = strdup(buf);

#if 0
  if(pass == 2) {
    printf("{\n");
    printf("\t.type = %s,\n", type);
    printf("\t.name = \"%s\",\n", basename(fname));
    printf("\t.muxes = muxlist_%s_%s,\n", type, smartname);
    printf("\t.nmuxes = sizeof(muxlist_%s_%s) / sizeof(struct mux),\n",
	   type, smartname);
    printf("\t.comment = ");
  }
#endif

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

      s = line + 2;
      if(strstr(s, " freq "))
	break;

      // ne->comment = strdup(s);
      gotcomment = 1;
      break;

    case 'C':
      dvb_c_config(line + 1);
      break;

    case 'T':
      dvb_t_config(line + 1);
      break;

    case 'S':
      dvb_s_config(line + 1);
      break;

    default:
      break;
    }
  }
  printf("};\n\n");

  fclose(fp);
}


static void
dump_networks(region_t *c)
{
  network_t *n;

  printf("static const struct network networks_%s_%s[] = {\n",
	 type, c->shortname);

  for(n = c->networks; n != NULL; n = n->next) {

    printf("\t{\n");
    printf("\t\t.name = \"%s\",\n", n->displayname);
    printf("\t\t.muxes = muxes_%s,\n", n->structname);
    printf("\t\t.nmuxes = sizeof(muxes_%s) / sizeof(struct mux),\n",
	   n->structname);
    if(n->comment)
      printf("\t\t.comment = \"%s\",\n", n->comment);
    printf("\t},\n");
  }
  printf("};\n\n");

}



static void
dump_regions(void)
{
  region_t *r;

  printf("static const struct region regions_%s[] = {\n", type);

  for(r = regions; r != NULL; r = r->next) {

    printf("\t{\n");
    printf("\t\t.name = \"%s\",\n", r->longname);
    printf("\t\t.networks = networks_%s_%s,\n", type, r->shortname);
    printf("\t\t.nnetworks = sizeof(networks_%s_%s) / sizeof(struct network),\n",
	   type, r->shortname);
    printf("\t},\n");
  }
  printf("};\n\n");
  
}



int 
main(int argc, char **argv)
{
  int i;
  region_t *c;

  if(argc < 2)
    return 1;

  type = argv[1];
#if 0
  printf("struct mux {\n"
	 "unsigned int freq;\n"
	 "unsigned int symrate;\n"
	 "char fec;\n"
	 "char constellation;\n"
	 "char bw;\n"
	 "char fechp;\n"
	 "char feclp;\n"
	 "char tmode;\n"
	 "char guard;\n"
	 "char hierarchy;\n"
	 "char polarisation;\n"
	 "}\n");
#endif

  for(i = 2; i < argc; i++)
    scan_file(argv[i]);

  for(c = regions; c != NULL; c = c->next) {
    dump_networks(c);
  }
  dump_regions();

  return 0;
}
