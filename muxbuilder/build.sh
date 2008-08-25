#!/bin/sh

gcc -O2 -Wall main.c -o muxbuilder

echo struct mux {
echo  unsigned int freq\;
echo  unsigned int symrate\;
echo  char fec\;
echo  char constellation\;
echo  char bw\;
echo  char fechp\;
echo  char feclp\;
echo  char tmode\;
echo  char guard\;
echo  char hierarchy\;
echo  char polarisation\;
echo }\;

echo struct network {
echo const char *name\;
echo const struct mux *muxes\;
echo const int nmuxes\;
echo }\;

echo struct region {
echo const char *name\;
echo const struct network *networks\;
echo const int nnetworks\;
echo }\;

find $1/dvb-s -type f | sort | xargs ./muxbuilder DVBS
find $1/dvb-t -type f | sort | xargs ./muxbuilder DVBT
find $1/dvb-c -type f | sort | xargs ./muxbuilder DVBC
