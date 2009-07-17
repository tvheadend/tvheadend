#!/bin/sh

gcc -O2 -Wall main.c -o muxbuilder

cat <<EOF

struct mux {
 unsigned int freq;
 unsigned int symrate;
 char fec;
 char constellation;
 char bw;
 char fechp;
 char feclp;
 char tmode;
 char guard;
 char hierarchy;
 char polarisation;
};

struct network {
const char *name;
const struct mux *muxes;
const int nmuxes;
};

struct region {
const char *name;
const struct network *networks;
const int nnetworks;
};

EOF


find $1/dvb-s -type f | sort | xargs ./muxbuilder DVBS
find $1/dvb-t -type f | sort | xargs ./muxbuilder DVBT
find $1/dvb-c -type f | sort | xargs ./muxbuilder DVBC
