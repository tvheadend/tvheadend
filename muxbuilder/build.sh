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

find $1/dvb-s -type f | sort | xargs ./muxbuilder 1 FE_QPSK
find $1/dvb-t -type f | sort | xargs ./muxbuilder 1 FE_OFDM
find $1/dvb-c -type f | sort | xargs ./muxbuilder 1 FE_QAM


echo struct {
echo int type\;
echo const char *name\;
echo struct mux *muxes\;
echo int nmuxes\;
echo const char *comment\;
echo } networks[] = {
find $1/dvb-s -type f | sort | xargs ./muxbuilder 2 FE_QPSK
find $1/dvb-t -type f | sort | xargs ./muxbuilder 2 FE_OFDM
find $1/dvb-c -type f | sort | xargs ./muxbuilder 2 FE_QAM
echo }\;