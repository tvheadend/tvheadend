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
struct mux muxlist_FE_QPSK_ABS1_75_0E[] = {
	{.freq = 12518000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12548000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12579000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12640000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12670000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12693000, .symrate = 10000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12704000, .symrate = 3900000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12740000, .symrate = 7408000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Amazonas_61_0W[] = {
	{.freq = 3957000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3966000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3975000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3993000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4137000, .symrate = 3409000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3941000, .symrate = 3480000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12092000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12132000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4174000, .symrate = 3330000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11128000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11185000, .symrate = 11800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11049000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12052000, .symrate = 27000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10975000, .symrate = 27000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3631000, .symrate = 2785000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10975000, .symrate = 26666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3677000, .symrate = 4400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3985000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11810000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11921000, .symrate = 21740000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11943000, .symrate = 4750000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4168000, .symrate = 7307000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12052000, .symrate = 26667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12172000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12092000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11175000, .symrate = 28880000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4154000, .symrate = 9615000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11099000, .symrate = 7576000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11885000, .symrate = 4890000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11882000, .symrate = 11343000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11133000, .symrate = 3111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11078000, .symrate = 1862000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11912000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11906000, .symrate = 2220000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11108000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12132000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11135000, .symrate = 26667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4144000, .symrate = 4540000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11808000, .symrate = 11111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11972000, .symrate = 26667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4156000, .symrate = 4540000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4149000, .symrate = 4540000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3948000, .symrate = 13300000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4178000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11844000, .symrate = 16600000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11095000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11790000, .symrate = 3600000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12172000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11015000, .symrate = 26666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11055000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12012000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_AMC1_103w[] = {
	{.freq = 11942000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12100000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_AMC2_85w[] = {
	{.freq = 11731000, .symrate = 13021000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11744000, .symrate = 13021000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11771000, .symrate = 13021000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_AMC3_87w[] = {
	{.freq = 11716000, .symrate = 4859000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12142000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12147000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12159000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12165000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12172000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12182000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_AMC4_101w[] = {
	{.freq = 11573000, .symrate = 7234000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11655000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11708000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11822000, .symrate = 5700000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11860000, .symrate = 28138000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12120000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12169000, .symrate = 3003000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_AMC5_79w[] = {
	{.freq = 11742000, .symrate = 11110000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12182000, .symrate = 23000000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_AMC6_72w[] = {
	{.freq = 11482000, .symrate = 2656000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11494000, .symrate = 6560000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11499000, .symrate = 2964000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11505000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11546000, .symrate = 12000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11548000, .symrate = 3002000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11552000, .symrate = 3002000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11557000, .symrate = 4392000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11563000, .symrate = 4392000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11570000, .symrate = 4392000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11576000, .symrate = 4392000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11586000, .symrate = 2652000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11603000, .symrate = 8500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11605000, .symrate = 3600000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11611000, .symrate = 3400000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11628000, .symrate = 6560000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11637000, .symrate = 2800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11641000, .symrate = 3702000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11648000, .symrate = 7500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11667000, .symrate = 7400000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11674000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11680000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11703000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11709000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11715000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11746000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11748000, .symrate = 14015000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11752000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11763000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11817000, .symrate = 5000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11874000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11986000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11995000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12004000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12013000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12025000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12031000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12046000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12055000, .symrate = 6890000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12144000, .symrate = 2573000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12188000, .symrate = 6511000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_AMC9_83w[] = {
	{.freq = 11745000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11751000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11757000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11763000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11769000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11775000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11826000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11864000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11871000, .symrate = 13000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11889000, .symrate = 13025000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11926000, .symrate = 6511000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11953000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 5000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12002000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12011000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Amos_4w[] = {
	{.freq = 11419000, .symrate = 2604000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11347000, .symrate = 2800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11423000, .symrate = 2894000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11338000, .symrate = 2960000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11133000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11631000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11132000, .symrate = 3254000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11154000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11319000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11426000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11575000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11638000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11323000, .symrate = 3350000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11328000, .symrate = 3350000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11350000, .symrate = 3350000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11359000, .symrate = 3350000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11332000, .symrate = 3500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11128000, .symrate = 3700000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11343000, .symrate = 3700000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11626000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11443000, .symrate = 4164000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11644000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11432000, .symrate = 4500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10993000, .symrate = 4850000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11191000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11183000, .symrate = 6428000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10986000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11144000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11151000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11161000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11172000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11209000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11219000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11413000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11673000, .symrate = 6900000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11435000, .symrate = 7480000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11546000, .symrate = 8050000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11177000, .symrate = 8520000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11585000, .symrate = 8888000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11235000, .symrate = 10000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11144000, .symrate = 11110000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11652000, .symrate = 11111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11560000, .symrate = 13400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11690000, .symrate = 15000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11652000, .symrate = 16100000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11303000, .symrate = 19540000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11390000, .symrate = 24000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10762000, .symrate = 26000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10720000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10723000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10758000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10803000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10805000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10841000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10842000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10889000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10889000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10924000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10924000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10972000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11008000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11260000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11474000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11510000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11558000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11592000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11677000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Anik_F1_107_3W[] = {
	{.freq = 12002000, .symrate = 19980000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12063000, .symrate = 19980000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12155000, .symrate = 22500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12185000, .symrate = 19980000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_AsiaSat3S_C_105_5E[] = {
	{.freq = 3700000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3725000, .symrate = 4450000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3743000, .symrate = 3300000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3750000, .symrate = 2820000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3755150, .symrate = 4417900, .fec = FEC_7_8, .polarisation = 'V'},
	{.freq = 3780000, .symrate = 28100000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3806000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3813000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3820000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3827000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3834000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3860000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3886000, .symrate = 4800000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3895000, .symrate = 6813000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3904000, .symrate = 4420000, .fec = FEC_7_8, .polarisation = 'V'},
	{.freq = 3914500, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3980000, .symrate = 28100000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4020000, .symrate = 27250000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4046000, .symrate = 5950000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4091000, .symrate = 13333000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4106000, .symrate = 3333300, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4115750, .symrate = 3333000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4140000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4166000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4180000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4187000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4194000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3680000, .symrate = 26670000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3706000, .symrate = 6000000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3715500, .symrate = 7000000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3729000, .symrate = 13650000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3760000, .symrate = 26000000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 3840000, .symrate = 26850000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 3920000, .symrate = 26750000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 3960000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4000000, .symrate = 26850000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 4034600, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4051000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4067000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4082000, .symrate = 4420000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4094000, .symrate = 5555000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4111000, .symrate = 13650000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4129000, .symrate = 13240000, .fec = FEC_3_4, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Astra_19_2E[] = {
	{.freq = 12551500, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Astra_28_2E[] = {
	{.freq = 11720000, .symrate = 29500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11740000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11758000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11778000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11798000, .symrate = 29500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11817000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11856000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11876000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11895000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11914000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11934000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11954000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12051000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12129000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12148000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12168000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12226000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12246000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12422000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12480000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11973000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11992000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12012000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12032000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12070000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12090000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12110000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12188000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12207000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12266000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12285000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12304000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12324000, .symrate = 29500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12344000, .symrate = 29500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12363000, .symrate = 29500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12382000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12402000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12441000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12460000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 10714000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10729000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 10744000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10758000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 10773000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10788000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 10803000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10818000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 10832000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10847000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 10862000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10876000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 10891000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10906000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 10921000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10936000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 11222170, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11223670, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11259000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11261000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11307000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11307000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11343000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11344000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11390000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11390000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11426000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11426000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11469000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11488000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11508000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11527000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11546000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11565000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11585000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11603850, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11623000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11642000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11661540, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11680770, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12524000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12524000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12560000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12560000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12596000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12607000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12629000, .symrate = 6111000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12692000, .symrate = 19532000, .fec = FEC_1_2, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Atlantic_Bird_1_12_5W[] = {
	{.freq = 11682000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11673000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11664000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11655000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11690000, .symrate = 5700000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11596000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11618000, .symrate = 2324000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11590000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11597000, .symrate = 3184000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11629000, .symrate = 6507000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11622000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11585000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11643000, .symrate = 2398000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11647000, .symrate = 3990000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11647000, .symrate = 3990000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11693000, .symrate = 4800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11644000, .symrate = 4800000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_BrasilSat_B1_75_0W[] = {
	{.freq = 3648000, .symrate = 4285000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3657000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3653000, .symrate = 4710000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3655000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3629000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3711000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3644000, .symrate = 4440000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3638000, .symrate = 4440000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_BrasilSat_B2_65_0W[] = {
	{.freq = 3745000, .symrate = 3540000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4008000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4011000, .symrate = 5000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3792000, .symrate = 3393000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4104000, .symrate = 3214000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4097000, .symrate = 6667000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3905000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3935000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3890000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3925000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4112000, .symrate = 4285000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3650000, .symrate = 4440000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3736000, .symrate = 1808000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3787000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3876000, .symrate = 2740000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4117000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3815000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3793000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3721000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3684000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3915000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3766000, .symrate = 3336000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3847000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3706000, .symrate = 2462000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3667000, .symrate = 7236000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3771000, .symrate = 1480000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3734000, .symrate = 2852000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3810000, .symrate = 13333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3762000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3834000, .symrate = 3572000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3850000, .symrate = 1570000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_BrasilSat_B3_84_0W[] = {
	{.freq = 3728000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3698000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4054000, .symrate = 1287000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3745000, .symrate = 4300000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3738000, .symrate = 4708000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3923000, .symrate = 1808000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3768000, .symrate = 8000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3955000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4167000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3791000, .symrate = 3330000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3866000, .symrate = 4425000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3932000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4132000, .symrate = 2532000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3906000, .symrate = 3928000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3858000, .symrate = 4288000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3665000, .symrate = 3177000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3732000, .symrate = 3214000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3774000, .symrate = 3330000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3927000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3710000, .symrate = 3261000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3883000, .symrate = 4278000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3770000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3754000, .symrate = 5000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3764000, .symrate = 4285000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3910000, .symrate = 3616000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3705000, .symrate = 4280000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3852000, .symrate = 3806000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3845000, .symrate = 10127000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3936000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3653000, .symrate = 3807000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3751000, .symrate = 3565000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3871000, .symrate = 4435000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3895000, .symrate = 4430000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3936000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3800000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3855000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4171000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3716000, .symrate = 4800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4068000, .symrate = 2600000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4070000, .symrate = 2964000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3949000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3684000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4169000, .symrate = 8140000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4144000, .symrate = 2734000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3736000, .symrate = 4285000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3790000, .symrate = 10444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3724000, .symrate = 2075000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4121000, .symrate = 2500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3629000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3970000, .symrate = 4445000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 3300000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3916000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4155000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3692000, .symrate = 3330000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3644000, .symrate = 4687000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4087000, .symrate = 17200000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3889000, .symrate = 4440000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4176000, .symrate = 3515000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3829000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3805000, .symrate = 2662000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3943000, .symrate = 2460000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3757000, .symrate = 3565000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3696000, .symrate = 1808000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3989000, .symrate = 2666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3690000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3657000, .symrate = 3600000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3996000, .symrate = 2300000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3926000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4136000, .symrate = 2142000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3675000, .symrate = 4285000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3931000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3786000, .symrate = 4286000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3688000, .symrate = 2308000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4075000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3911000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3824000, .symrate = 3002000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3778000, .symrate = 6850000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3710000, .symrate = 12960000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3877000, .symrate = 4450000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3940000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4064000, .symrate = 3300000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_BrasilSat_B4_70_0W[] = {
	{.freq = 3951000, .symrate = 3214000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3714000, .symrate = 4400000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3672000, .symrate = 4713000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3955000, .symrate = 4400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3965000, .symrate = 2930000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3628000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3644000, .symrate = 3214000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3650000, .symrate = 4285000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3688000, .symrate = 6000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3680000, .symrate = 6000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3662000, .symrate = 4606000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3945000, .symrate = 3214000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3640000, .symrate = 3263000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3979000, .symrate = 3617000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3752000, .symrate = 6220000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3685000, .symrate = 4500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3650000, .symrate = 4400000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4010000, .symrate = 13021000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3665000, .symrate = 4700000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3637000, .symrate = 2228000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3708000, .symrate = 3928000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4047000, .symrate = 7143000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3672000, .symrate = 8454000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3820000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3940000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3964000, .symrate = 1875000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3695000, .symrate = 3598000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3900000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3645000, .symrate = 3520000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3983000, .symrate = 1630000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3631000, .symrate = 4687000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3874000, .symrate = 5926000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3700000, .symrate = 9123000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3997000, .symrate = 2300000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3973000, .symrate = 4338000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Estrela_do_Sul_63_0W[] = {
	{.freq = 11892000, .symrate = 2964000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11987000, .symrate = 3330000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 26660000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11830000, .symrate = 6000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11603000, .symrate = 3124000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11598000, .symrate = 3124000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11803000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11958000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11610000, .symrate = 3124000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11861000, .symrate = 2964000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11879000, .symrate = 2964000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11903000, .symrate = 2362000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11817000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11577000, .symrate = 3124000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11582000, .symrate = 3124000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11982000, .symrate = 8888000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11888000, .symrate = 2392000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11898000, .symrate = 2480000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11871000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11795000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11845000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11640000, .symrate = 18100000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11875000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11543000, .symrate = 10410000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Eurobird1_28_5E[] = {
	{.freq = 11623000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11224000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11527000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_EutelsatW2_16E[] = {
	{.freq = 10957000, .symrate = 2821000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10968000, .symrate = 6400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10972000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10976000, .symrate = 6400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10989000, .symrate = 6400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10997000, .symrate = 6400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11005000, .symrate = 6400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11011000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11015000, .symrate = 6400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11025000, .symrate = 2894000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11046000, .symrate = 10555000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11057000, .symrate = 3327000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11061000, .symrate = 5722000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11078000, .symrate = 5208000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11092000, .symrate = 32000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11094000, .symrate = 2734000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11132000, .symrate = 14185000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11178000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11192000, .symrate = 2667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11267000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11276000, .symrate = 11100000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11294000, .symrate = 13333000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11304000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11324000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11428000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11449000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11471000, .symrate = 29950000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11492000, .symrate = 29950000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11513000, .symrate = 29950000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11534000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11554000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11575000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11594000, .symrate = 28800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11617000, .symrate = 29950000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11634000, .symrate = 17578000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11658000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11659000, .symrate = 17578000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11682000, .symrate = 14468000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12538000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12549000, .symrate = 2894000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12555000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12557000, .symrate = 2156000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12562000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12563000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12568000, .symrate = 3703000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12625000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12633000, .symrate = 4883000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12642000, .symrate = 3418000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12650000, .symrate = 15000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12656000, .symrate = 4883000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12677000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12683000, .symrate = 2894000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12703000, .symrate = 2748000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12716000, .symrate = 6000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12723000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12733000, .symrate = 16277000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Express_3A_11_0W[] = {
	{.freq = 3675000, .symrate = 29623000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_ExpressAM1_40_0E[] = {
	{.freq = 10967000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10995000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11097000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_ExpressAM22_53_0E[] = {
	{.freq = 11044000, .symrate = 44950000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10974000, .symrate = 8150000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11031000, .symrate = 3750000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11096000, .symrate = 6400000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11124000, .symrate = 7593000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11161000, .symrate = 5785000, .fec = FEC_3_4, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_ExpressAM2_80_0E[] = {
	{.freq = 10973000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10991000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11044000, .symrate = 44948000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11081000, .symrate = 5064000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11088000, .symrate = 4548000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11191000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11462000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11478000, .symrate = 4400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11544000, .symrate = 44950000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11606000, .symrate = 44948000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11650000, .symrate = 3500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3525000, .symrate = 31106000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3558000, .symrate = 3215000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3562000, .symrate = 3225000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3625000, .symrate = 3000000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3675000, .symrate = 33483000, .fec = FEC_7_8, .polarisation = 'V'},
	{.freq = 3929000, .symrate = 8705000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4147000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4175000, .symrate = 6510000, .fec = FEC_3_4, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Galaxy10R_123w[] = {
	{.freq = 11720000, .symrate = 27692000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11732000, .symrate = 13240000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11800000, .symrate = 26657000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11805000, .symrate = 4580000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11966000, .symrate = 13021000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12104000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12114000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Galaxy11_91w[] = {
	{.freq = 10964000, .symrate = 19850000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10994000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11024000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11806000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11815000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11825000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11925000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11930000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11935000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11940000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11945000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11950000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11950000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11955000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11955000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11965000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11970000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11975000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11980000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11985000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11985000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11990000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11990000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11995000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11995000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12000000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12005000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12010000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12010000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12015000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12020000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12025000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12030000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12035000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12066000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12075000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12083000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12086000, .symrate = 6144000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12096000, .symrate = 6144000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12104000, .symrate = 6144000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12114000, .symrate = 6144000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Galaxy25_97w[] = {
	{.freq = 11789000, .symrate = 28125000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 20770000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11867000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11874000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11898000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11966000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11991000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11999000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12053000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12084000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12090000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12115000, .symrate = 22425000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12146000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12152000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12177000, .symrate = 23000000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Galaxy26_93w[] = {
	{.freq = 11711000, .symrate = 14312000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11721000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11727000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11732000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11742000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11748000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11753000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11767000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11777000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11782000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11788000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11793000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11809000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11814000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11841000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11887000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11893000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11904000, .symrate = 3010000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11919000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11924000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11926000, .symrate = 8848000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11929000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11935000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 8848000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11944000, .symrate = 8848000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11949000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11954000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11956000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11960000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11961000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11967000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11972000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12047000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12048000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12058000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12059000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12063000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12064000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12069000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12076000, .symrate = 8681000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12089000, .symrate = 6511000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12110000, .symrate = 4104000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12116000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12121000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12126000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12132000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 5147000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Galaxy27_129w[] = {
	{.freq = 11964000, .symrate = 2920000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Galaxy28_89w[] = {
	{.freq = 11717000, .symrate = 4411000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11747000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11756000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11780000, .symrate = 29000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11800000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11825000, .symrate = 4552000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11865000, .symrate = 3700000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11882000, .symrate = 4883000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11925000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11930000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11935000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11936000, .symrate = 6000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11940000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11945000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11950000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11955000, .symrate = 19532000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 28800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11980000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11985000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11989000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11990000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11995000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12000000, .symrate = 28800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12009000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12032000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12092000, .symrate = 2314000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12114000, .symrate = 14398000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12134000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12164000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12170000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12180000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12185000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12191000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12196000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Galaxy3C_95w[] = {
	{.freq = 11780000, .symrate = 20760000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Hispasat_30_0W[] = {
	{.freq = 11539000, .symrate = 24500000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 11749000, .symrate = 3520000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11760000, .symrate = 3260000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11766000, .symrate = 4500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11776000, .symrate = 2387000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11783000, .symrate = 1200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11787000, .symrate = 2500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11807000, .symrate = 6510000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11823000, .symrate = 2387000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11884000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11907000, .symrate = 2592000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11917000, .symrate = 5681000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11931000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11931000, .symrate = 2220000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 5185000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11940000, .symrate = 1481000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11972000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11997000, .symrate = 4422000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12003000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12008000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 3492000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12040000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12052000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12085000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12131000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12135000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12137000, .symrate = 3030000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12141000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12146000, .symrate = 4200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12156000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12158000, .symrate = 2348000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12163000, .symrate = 3030000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12168000, .symrate = 5240000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12172000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 4500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12182000, .symrate = 3340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12188000, .symrate = 2583000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12192000, .symrate = 2593000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12567000, .symrate = 19850000, .fec = FEC_3_4, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Hotbird_13_0E[] = {
	{.freq = 12539000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 10719000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10723000, .symrate = 29900000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 10757000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10775000, .symrate = 28000000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 10795000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10834000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10853000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 10872000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10892000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 10910000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10930000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 10949000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 10971000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 10992000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11013000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11034000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11054000, .symrate = 27500000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 11075000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11095000, .symrate = 28000000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11117000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11137000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11158000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11178000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11200000, .symrate = 27500000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 11219000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11242000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11278000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11295000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11334000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11355000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11373000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 11393000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11411000, .symrate = 27500000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 11432000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11470000, .symrate = 27500000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 11488000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11526000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11541000, .symrate = 22000000, .fec = FEC_5_6, .polarisation = 'V'},
	{.freq = 11565000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11585000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11604000, .symrate = 27500000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 11623000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11645000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11662000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11677000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11727000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11747000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11765000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11785000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11804000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11823000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11842000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11861000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11880000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11900000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11919000, .symrate = 27500000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 11938000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 11958000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11976000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12034000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12054000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12072000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12092000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12111000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12149000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12169000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12188000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12207000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12226000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12245000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12264000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12284000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12302000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12322000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12341000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12360000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12379000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12398000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12418000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12437000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12475000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12519000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12558000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12577000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12596000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12616000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12635000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12654000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12673000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12692000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12713000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12731000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_IA5_97w[] = {
	{.freq = 11789000, .symrate = 25000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 20765000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11867000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11874000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11898000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11966000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11991000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12053000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12084000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12090000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12115000, .symrate = 22425000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12122000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12146000, .symrate = 22000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12152000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12177000, .symrate = 23000000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_IA6_93w[] = {
	{.freq = 11711000, .symrate = 14312000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11721000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11727000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11732000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11742000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11748000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11753000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11767000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11777000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11782000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11788000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11793000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11809000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11814000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11827000, .symrate = 8429000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 7179000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11841000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11865000, .symrate = 3516000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11887000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11893000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11904000, .symrate = 5000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11919000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11924000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11926000, .symrate = 8848000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11929000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11935000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 8848000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11944000, .symrate = 8848000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11949000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11954000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11956000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11960000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11961000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11967000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11972000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12047000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12048000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12058000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12059000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12063000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12064000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12069000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12076000, .symrate = 8679000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12089000, .symrate = 6511000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12110000, .symrate = 4104000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12116000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12121000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12126000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12132000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 5147000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_IA7_129w[] = {
	{.freq = 11989000, .symrate = 2821000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_IA8_89w[] = {
	{.freq = 11780000, .symrate = 29000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11925000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11930000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11935000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11940000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11945000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11945000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11950000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11950000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11955000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11955000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11965000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11965000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11970000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11975000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11980000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11985000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11989000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11990000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11995000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12009000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12164000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12170000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12180000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12185000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12191000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12196000, .symrate = 3979000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Intel4_72_0E[] = {
	{.freq = 11533000, .symrate = 4220000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11638000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12518000, .symrate = 8232000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12526000, .symrate = 3266000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Intel904_60_0E[] = {
	{.freq = 11003000, .symrate = 2975000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11011000, .symrate = 2975000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11015000, .symrate = 2975000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11093000, .symrate = 3980000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11101000, .symrate = 4105000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11142000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11152000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11157000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11515000, .symrate = 7300000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11635000, .symrate = 29700000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11675000, .symrate = 29700000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Intelsat_1002_1_0W[] = {
	{.freq = 4175000, .symrate = 28000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4180000, .symrate = 21050000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Intelsat_11_43_0W[] = {
	{.freq = 3944000, .symrate = 5945000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Intelsat_1R_45_0W[] = {
	{.freq = 4104000, .symrate = 14450000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3854000, .symrate = 2370000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11893000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3869000, .symrate = 3515000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4186000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4040000, .symrate = 4347000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4071000, .symrate = 2615000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3991000, .symrate = 4044000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3882000, .symrate = 4410000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11788000, .symrate = 10000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11728000, .symrate = 5057000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 5057000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11722000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11718000, .symrate = 24667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11833000, .symrate = 17360000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11930000, .symrate = 11790000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11808000, .symrate = 4779000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4132000, .symrate = 5749000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3780000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11705000, .symrate = 4440000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3759000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4171000, .symrate = 4410000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3899000, .symrate = 6611000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4192000, .symrate = 2075000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4096000, .symrate = 8102000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11873000, .symrate = 6000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11856000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11746000, .symrate = 6900000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11715000, .symrate = 5500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 3310000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3892000, .symrate = 6110000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4135000, .symrate = 26600000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4121000, .symrate = 3510000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3742000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3910000, .symrate = 4292000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3975000, .symrate = 3310000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4133000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3785000, .symrate = 4409000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3868000, .symrate = 10075000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3774000, .symrate = 8820000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4128000, .symrate = 3310000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Intelsat_3R_43_0W[] = {
	{.freq = 3936000, .symrate = 3310000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3901000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3891000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3935000, .symrate = 17360000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3872000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3736000, .symrate = 29270000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4106000, .symrate = 26470000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3910000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3919000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4083000, .symrate = 6599000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4106000, .symrate = 29270000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3994000, .symrate = 21090000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3867000, .symrate = 6429000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3958000, .symrate = 6500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3980000, .symrate = 3500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3828000, .symrate = 4350000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3888000, .symrate = 7813000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3785000, .symrate = 30800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3942000, .symrate = 1200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3946000, .symrate = 2592000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3988000, .symrate = 4070000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3845000, .symrate = 30800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4040000, .symrate = 30800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3865000, .symrate = 6900000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3850000, .symrate = 28800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11705000, .symrate = 3700000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11745000, .symrate = 3111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11711000, .symrate = 4687000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11754000, .symrate = 3109000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3746000, .symrate = 21261000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4150000, .symrate = 24570000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3930000, .symrate = 2812000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Intelsat_6B_43_0W[] = {
	{.freq = 10882000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10882000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10970000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10970000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11050000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11130000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11382000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11130000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10720000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10720000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11050000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11382000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10800000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10800000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Intelsat_705_50_0W[] = {
	{.freq = 3911000, .symrate = 3617000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3917000, .symrate = 4087000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3838000, .symrate = 7053000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Intelsat_707_53_0W[] = {
	{.freq = 3820000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11483000, .symrate = 5333000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Intelsat_805_55_5W[] = {
	{.freq = 4171000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4147000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3914000, .symrate = 1809000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3572000, .symrate = 11800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4135000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3737000, .symrate = 1809000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3932000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3759000, .symrate = 4167000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3750000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3850000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4010000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3715000, .symrate = 8890000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3670000, .symrate = 1374000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4158000, .symrate = 1447000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3549000, .symrate = 6510000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3451000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3667000, .symrate = 3300000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3522000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4055000, .symrate = 21703000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3698000, .symrate = 3600000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4119000, .symrate = 1631000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3446000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3675000, .symrate = 2660000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3717000, .symrate = 11574000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4030000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3767000, .symrate = 4427000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4027000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4089000, .symrate = 5540000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3685000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3751000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4127000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3980000, .symrate = 19510000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3590000, .symrate = 10127000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4000000, .symrate = 5200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3762000, .symrate = 3662000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3936000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3727000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4104000, .symrate = 5062000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3777000, .symrate = 7400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3431000, .symrate = 3500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3478000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4093000, .symrate = 2540000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3929000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4195000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4140000, .symrate = 4700000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4096000, .symrate = 5247000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3467000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3442000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4084000, .symrate = 10317000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3723000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3815000, .symrate = 26667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4006000, .symrate = 3690000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3918000, .symrate = 4400000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3735000, .symrate = 8680000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3727000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3940000, .symrate = 2575000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3792000, .symrate = 2244000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3900000, .symrate = 3612000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3677000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4127000, .symrate = 2532000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4152000, .symrate = 3600000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4177000, .symrate = 27690000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4111000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Intelsat_903_34_5W[] = {
	{.freq = 4178000, .symrate = 32555000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4045000, .symrate = 4960000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3895000, .symrate = 13021000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4004000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Intelsat_905_24_5W[] = {
	{.freq = 4171000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4181000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4194000, .symrate = 5193000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4162000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4060000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4070000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Intelsat_907_27_5W[] = {
	{.freq = 3873000, .symrate = 4687000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3935000, .symrate = 4687000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3743000, .symrate = 2900000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3732000, .symrate = 14000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3943000, .symrate = 1808000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3938000, .symrate = 3544000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Intelsat_9_58_0W[] = {
	{.freq = 4122000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4146000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4157000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4160000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3960000, .symrate = 29270000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3960000, .symrate = 29270000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 27684000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3720000, .symrate = 19510000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4131000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4155000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4173000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3995000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3986000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4175000, .symrate = 4410000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4107000, .symrate = 8850000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3925000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3915000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3925000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4005000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4015000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3720000, .symrate = 27700000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4166000, .symrate = 6200000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3794000, .symrate = 3332000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3807000, .symrate = 3428000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3880000, .symrate = 27690000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3800000, .symrate = 26470000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3760000, .symrate = 27690000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3996000, .symrate = 3330000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3935000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11852000, .symrate = 30000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11895000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11913000, .symrate = 10000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4155000, .symrate = 3310000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3934000, .symrate = 7000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3787000, .symrate = 7407000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3924000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3800000, .symrate = 4444000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3911000, .symrate = 13330000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4040000, .symrate = 16180000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 27690000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4144000, .symrate = 2205000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4151000, .symrate = 2890000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4147000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4170000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3760000, .symrate = 28500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4120000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11670000, .symrate = 16470000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4125000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4137000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3880000, .symrate = 27690000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4174000, .symrate = 2941000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3840000, .symrate = 27690000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3840000, .symrate = 27690000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3815000, .symrate = 6250000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Nahuel_1_71_8W[] = {
	{.freq = 11673000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11680000, .symrate = 3335000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11654000, .symrate = 4170000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11874000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12136000, .symrate = 2960000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11873000, .symrate = 8000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12116000, .symrate = 14396000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11997000, .symrate = 8500000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Nilesat101_102_7_0W[] = {
	{.freq = 10758000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10796000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10853000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10873000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10892000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 10911000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10930000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11747000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11766000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11785000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11804000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11823000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11843000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11862000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11881000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11900000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11919000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11938000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11958000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11996000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12034000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12073000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12130000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12149000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12207000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12226000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12284000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12303000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12341000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12360000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12399000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_NSS_10_37_5W[] = {
	{.freq = 4055000, .symrate = 2700000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3824000, .symrate = 1808000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4059000, .symrate = 3214000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3828000, .symrate = 2532000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3844000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4071000, .symrate = 3150000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4051000, .symrate = 4440000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4044000, .symrate = 3250000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4077000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_NSS_7_22_0W[] = {
	{.freq = 3926000, .symrate = 3715000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3920000, .symrate = 3715000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3954000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3929000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3915000, .symrate = 3715000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3761000, .symrate = 22650000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11825000, .symrate = 5904000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12162000, .symrate = 6510000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11777000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11860000, .symrate = 35000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12049000, .symrate = 6500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11921000, .symrate = 35000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4003000, .symrate = 6667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4011000, .symrate = 6667000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 3680000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3969000, .symrate = 1808000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3976000, .symrate = 1842000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11814000, .symrate = 5630000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4038000, .symrate = 3690000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3674000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4033000, .symrate = 3689000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4016000, .symrate = 3663000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_NSS_806_40_5W[] = {
	{.freq = 11921000, .symrate = 35000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3660000, .symrate = 4350000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3986000, .symrate = 3179000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3859000, .symrate = 2600000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4120000, .symrate = 2960000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4016000, .symrate = 5712000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3978000, .symrate = 3978000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4100000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3965000, .symrate = 2540000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3774000, .symrate = 6670000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3725000, .symrate = 26667000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3600000, .symrate = 25185000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3758000, .symrate = 26667000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3982000, .symrate = 17800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4090000, .symrate = 2515000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4042000, .symrate = 8680000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4009000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4003000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3648000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3664000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3644000, .symrate = 2534000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3695000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4082000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4143000, .symrate = 4800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3641000, .symrate = 2666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3991000, .symrate = 3578000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4000000, .symrate = 2450000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4009000, .symrate = 2450000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3960000, .symrate = 3170000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3755000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4135000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4005000, .symrate = 2450000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4003000, .symrate = 2450000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3600000, .symrate = 29185000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3868000, .symrate = 2100000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3685000, .symrate = 6500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3688000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3679000, .symrate = 2220000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3961000, .symrate = 1481000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4130000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3923000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4052000, .symrate = 2459000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4071000, .symrate = 3333000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4146000, .symrate = 2571000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3646000, .symrate = 3978000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3837000, .symrate = 19510000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4132000, .symrate = 2480000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3653000, .symrate = 5924000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4055000, .symrate = 7233000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3938000, .symrate = 4785000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4152000, .symrate = 3280000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3832000, .symrate = 13310000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4090000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4082000, .symrate = 6510000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4107000, .symrate = 2100000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3500000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 2531000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3860000, .symrate = 2713000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3983000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4065000, .symrate = 8400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4063000, .symrate = 8500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3972000, .symrate = 3330000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4024000, .symrate = 16030000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3803000, .symrate = 26860000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3920000, .symrate = 20000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4053000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3676000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3637000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3516000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3516000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4093000, .symrate = 2887000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4177000, .symrate = 4391000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4124000, .symrate = 3480000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4139000, .symrate = 2220000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3693000, .symrate = 4441000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4194000, .symrate = 6660000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3803000, .symrate = 27500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4112000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3676000, .symrate = 5900000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3652000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3725000, .symrate = 26669000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3898000, .symrate = 4195000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4162000, .symrate = 7200000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4168000, .symrate = 2400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3965000, .symrate = 3332000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4100000, .symrate = 6654000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4127000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4109000, .symrate = 6654000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3670000, .symrate = 2960000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3853000, .symrate = 5900000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3660000, .symrate = 2540000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4170000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3630000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3695000, .symrate = 2220000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4184000, .symrate = 6142000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4122000, .symrate = 1860000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4022000, .symrate = 3800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4137000, .symrate = 4400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3878000, .symrate = 22117000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4132000, .symrate = 2800000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4043000, .symrate = 7440000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4142000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3990000, .symrate = 4195000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3667000, .symrate = 3340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3673000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_OptusC1_156E[] = {
	{.freq = 12278000, .symrate = 30000000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12305000, .symrate = 30000000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12358000, .symrate = 27000000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12398000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12407000, .symrate = 30000000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12438000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12487000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12501000, .symrate = 29473000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12518000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12527000, .symrate = 30000000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12558000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12564000, .symrate = 29473000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12567000, .symrate = 27800000, .fec = FEC_2_3, .polarisation = 'V'},
	{.freq = 12598000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12607000, .symrate = 29473000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12638000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12689000, .symrate = 27800000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12720000, .symrate = 30000000, .fec = FEC_3_4, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_PAS_43_0W[] = {
	{.freq = 12578000, .symrate = 19850000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12584000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12606000, .symrate = 6616000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 12665000, .symrate = 19850000, .fec = FEC_7_8, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Satmex_5_116_8W[] = {
	{.freq = 12034000, .symrate = 2532000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 4232000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4060000, .symrate = 19510000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 2963000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4084000, .symrate = 3162000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3767000, .symrate = 1620000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 5000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11960000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4180000, .symrate = 19510000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4115000, .symrate = 3253000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3840000, .symrate = 29270000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4160000, .symrate = 29270000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3940000, .symrate = 28125000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4052000, .symrate = 4307000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12024000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4134000, .symrate = 3617000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12028000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12060000, .symrate = 3078000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4012000, .symrate = 3131000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3949000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4076000, .symrate = 2962000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3749000, .symrate = 4070000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3744000, .symrate = 4480000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3755000, .symrate = 4000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3869000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3876000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3879000, .symrate = 1984000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3957000, .symrate = 2600000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3975000, .symrate = 3131000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3982000, .symrate = 2531000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3832000, .symrate = 2500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3953000, .symrate = 2597000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3987000, .symrate = 8860000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3996000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3748000, .symrate = 2100000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3932000, .symrate = 2500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3914000, .symrate = 3223000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3888000, .symrate = 5351000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3805000, .symrate = 4679000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4001000, .symrate = 4100000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4023000, .symrate = 6400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12176000, .symrate = 3985000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4108000, .symrate = 2666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4038000, .symrate = 7675000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3809000, .symrate = 3100000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4080000, .symrate = 29270000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3910000, .symrate = 2500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4164000, .symrate = 1733000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12193000, .symrate = 7885000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12164000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12044000, .symrate = 4340000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3720000, .symrate = 27000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3922000, .symrate = 9760000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3797000, .symrate = 3200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3968000, .symrate = 7500000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4037000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4046000, .symrate = 2441000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 2441000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3773000, .symrate = 2892000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3767000, .symrate = 2893000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3935000, .symrate = 4440000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3972000, .symrate = 3364000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 2400000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4007000, .symrate = 5582000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3885000, .symrate = 3000000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3826000, .symrate = 2712000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3944000, .symrate = 3410000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3915000, .symrate = 1520000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 3876000, .symrate = 2920000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Satmex_6_113_0W[] = {
	{.freq = 4078000, .symrate = 3609000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12145000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4085000, .symrate = 2821000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12126000, .symrate = 6022000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12166000, .symrate = 17500000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 4075000, .symrate = 3782000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 4091000, .symrate = 3720000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 2300000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12126000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12157000, .symrate = 3038000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12091000, .symrate = 3337000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3947000, .symrate = 3700000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3761000, .symrate = 2120000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12107000, .symrate = 2222000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12080000, .symrate = 25635000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_SBS6_74w[] = {
	{.freq = 11744000, .symrate = 6616000, .fec = FEC_AUTO, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Sirius_5_0E[] = {
	{.freq = 11823000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11977000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 12054000, .symrate = 27500000, .fec = FEC_3_4, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Telecom2_8_0W[] = {
	{.freq = 11635000, .symrate = 6800000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 12687000, .symrate = 1879000, .fec = FEC_3_4, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Telstar_12_15_0W[] = {
	{.freq = 12180000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11895000, .symrate = 5000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11974000, .symrate = 3400000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12185000, .symrate = 4214000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12081000, .symrate = 3935000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12050000, .symrate = 3198000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11756000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12093000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11916000, .symrate = 6111000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11707000, .symrate = 3198000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11718000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11740000, .symrate = 3255000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12000000, .symrate = 6666000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12116000, .symrate = 2062000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12111000, .symrate = 2062000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 3504000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12167000, .symrate = 3502000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12129000, .symrate = 2000000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12110000, .symrate = 6620000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11724000, .symrate = 13225000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11984000, .symrate = 13570000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12125000, .symrate = 3800000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 11713000, .symrate = 9626000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11804000, .symrate = 7595000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11965000, .symrate = 14714000, .fec = FEC_AUTO, .polarisation = 'H'},
	{.freq = 12039000, .symrate = 5632000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12082000, .symrate = 6396000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12087000, .symrate = 3198000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 12097000, .symrate = 3198000, .fec = FEC_AUTO, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Telstar12_15_0W[] = {
	{.freq = 12041000, .symrate = 3256000, .fec = FEC_2_3, .polarisation = 'H'},
	{.freq = 12520000, .symrate = 8700000, .fec = FEC_1_2, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Thor_1_0W[] = {
	{.freq = 11247000, .symrate = 24500000, .fec = FEC_7_8, .polarisation = 'V'},
	{.freq = 11293000, .symrate = 24500000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 11325000, .symrate = 24500000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 28000000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 12169000, .symrate = 28000000, .fec = FEC_7_8, .polarisation = 'H'},
	{.freq = 12226000, .symrate = 28000000, .fec = FEC_7_8, .polarisation = 'V'},
};

struct mux muxlist_FE_QPSK_Turksat_42_0E[] = {
	{.freq = 11594000, .symrate = 4557000, .fec = FEC_5_6, .polarisation = 'H'},
	{.freq = 10978000, .symrate = 2344000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 11734000, .symrate = 3291000, .fec = FEC_3_4, .polarisation = 'H'},
};

struct mux muxlist_FE_QPSK_Yamal201_90_0E[] = {
	{.freq = 10990000, .symrate = 2170000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 10995000, .symrate = 4285000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11057000, .symrate = 26470000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11092000, .symrate = 26470000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11145000, .symrate = 22222000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 11671000, .symrate = 18200000, .fec = FEC_AUTO, .polarisation = 'V'},
	{.freq = 3536000, .symrate = 2532000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3553000, .symrate = 20000000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3577000, .symrate = 2626000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3588000, .symrate = 4285000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3600000, .symrate = 4285000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3603000, .symrate = 4285000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 3605000, .symrate = 2626000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3645000, .symrate = 28000000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3674000, .symrate = 17500000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3725000, .symrate = 3200000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3900000, .symrate = 4285000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3907000, .symrate = 4265000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3912000, .symrate = 4295000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3944000, .symrate = 15550000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 3980000, .symrate = 38000000, .fec = FEC_3_4, .polarisation = 'H'},
	{.freq = 4042000, .symrate = 8681000, .fec = FEC_3_4, .polarisation = 'V'},
	{.freq = 4084000, .symrate = 2500000, .fec = FEC_3_4, .polarisation = 'V'},
};

struct mux muxlist_FE_OFDM_at_Offical[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Adelaide[] = {
	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 564500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Brisbane[] = {
	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 585625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Cairns[] = {
	{.freq = 191500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 536500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_canberra[] = {
	{.freq = 205625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 543500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Canberra_Black_Mt[] = {
	{.freq = 205500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 543500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Darwin[] = {
	{.freq = 543625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 550500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 536625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 557625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_GoldCoast[] = {
	{.freq = 767500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 585500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 704500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 809500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 788500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 725500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Hobart[] = {
	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 205500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 212500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 184500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Mackay[] = {
	{.freq = 212500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 205500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 557500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 536500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Melbourne[] = {
	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 536625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Melbourne_Upwey[] = {
	{.freq = 662500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 620500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 641500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 711500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 683500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_MidNorthCoast[] = {
	{.freq = 184625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 198500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 641500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 205500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 585500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 543500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 564500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 599500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 606500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Newcastle[] = {
	{.freq = 599500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 585500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 704500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 592500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Perth[] = {
	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 536500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Perth_Roleystone[] = {
	{.freq = 704500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 725500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 767500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 788500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_SpencerGulf[] = {
	{.freq = 599500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 641500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 620500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_SunshineCoast[] = {
	{.freq = 585500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 767500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 788500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 809500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 662625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Sydney_Kings_Cross[] = {
	{.freq = 543500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 669500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 564500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 648500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 571500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Sydney_North_Shore[] = {
	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 571500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Tamworth[] = {
	{.freq = 690500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 753500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 732500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 711500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 774500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 585500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 592500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 205625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 613500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 641500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 648500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 620500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 226625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 641500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Townsville[] = {
	{.freq = 592500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 550500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 599500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 620500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 585500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_unknown[] = {
	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_WaggaWagga[] = {
	{.freq = 655500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 669500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 662500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 683500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_au_Wollongong[] = {
	{.freq = 697500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 655500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 613500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 711625000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 599500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 585500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 592500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 676500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_be_Libramont[] = {
	{.freq = 191500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_be_Schoten[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_be_St_Pieters_Leeuw[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_be_Tournai[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_ch_All[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_5_6, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_ch_Citycable[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_7_8, .feclp = FEC_7_8, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_cz_Brno[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_cz_Domazlice[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_cz_Ostrava[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_cz_Praha[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Aachen_Stadt[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Berlin[] = {
	{.freq = 177500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 191500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Bielefeld[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Braunschweig[] = {
	{.freq = 198500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Bremen[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Brocken_Magdeburg[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Chemnitz[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Dresden[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Erfurt_Weimar[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Frankfurt[] = {
	{.freq = 198500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Freiburg[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_HalleSaale[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Hamburg[] = {
	{.freq = 205500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Hannover[] = {
	{.freq = 198500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Kassel[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Kiel[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Koeln_Bonn[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Leipzig[] = {
	{.freq = 205500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Loerrach[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Luebeck[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Muenchen[] = {
	{.freq = 212500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Nuernberg[] = {
	{.freq = 184500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Osnabrueck[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Ostbayern[] = {
	{.freq = 191500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Ravensburg[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Rostock[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Ruhrgebiet[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Schwerin[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Stuttgart[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_de_Wuerzburg[] = {
	{.freq = 212500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_dk_All[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Albacete[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Alfabia[] = {
	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Alicante[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Alpicat[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Asturias[] = {
	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Bilbao[] = {
	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Carceres[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Collserola[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Donostia[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Las_Palmas[] = {
	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Lugo[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Madrid[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Malaga[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Mussara[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Rocacorba[] = {
	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Santander[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Sevilla[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Valladolid[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Vilamarxant[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_es_Zaragoza[] = {
	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Aanekoski[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Aanekoski_Konginkangas[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ahtari[] = {
	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Alajarvi[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ala_Vuokki[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ammansaari[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Anjalankoski[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Enontekio_Ahovaara_Raattama[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Espoo[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Eurajoki[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Fiskars[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Haapavesi[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hameenkyro_Kyroskoski[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hameenlinna_Painokangas[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hanko[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hartola[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Heinavesi[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Heinola[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hetta[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Houtskari[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hyrynsalmi[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hyrynsalmi_Kyparavaara[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hyrynsalmi_Paljakka[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Hyvinkaa_Musta_Mannisto[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ii_Raiskio[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Iisalmi[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ikaalinen[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ikaalinen_Riitiala[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Inari[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ivalo_Saarineitamovaara[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jalasjarvi[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jamsa_Kaipola[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jamsa_Kuorevesi_Halli[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jamsa_Matkosvuori[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jamsankoski[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jamsa_Ouninpohja[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Joensuu_Vestinkallio[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Joroinen_Puukkola[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Joutsa_Lankia[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Joutseno[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Juntusranta[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Juupajoki_Kopsamo[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jyvaskyla[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Jyvaskylan_mlk_Vaajakoski[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kaavi_Sivakkavaara_Luikonlahti[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kajaani_Pollyvaara[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kalajoki[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kangaslampi[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kangasniemi_Turkinmaki[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kankaanpaa[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Karigasniemi[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Karkkila[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Karstula[] = {
	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Karvia[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kaunispaa[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kemijarvi_Suomutunturi[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kerimaki[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Keuruu[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Keuruu_Haapamaki[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kihnio[] = {
	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kiihtelysvaara[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kilpisjarvi[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kittila_Sirkka_Levitunturi[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kolari_Vuolittaja[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Koli[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Korpilahti_Vaarunvuori[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Korppoo[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kruunupyy[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kuhmo_Iivantiira[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kuhmoinen[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kuhmoinen_Harjunsalmi[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kuhmoinen_Puukkoinen[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kuhmo_Lentiira[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kuopio[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kustavi_Viherlahti[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kuttanen[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Kyyjarvi_Noposenaho[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Lahti[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Lapua[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Laukaa[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Laukaa_Vihtavuori[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Lavia_Lavianjarvi[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Lieksa_Vieki[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Lohja[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Loimaa[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Luhanka[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Luopioinen[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Mantta[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Mantyharju[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Mikkeli[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Muonio_Olostunturi[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Nilsia[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Nilsia_Keski_Siikajarvi[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Nilsia_Pisa[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Nokia[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Nokia_Siuro_Linnavuori[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Nummi_Pusula_Hyonola[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Nurmes_Porokyla[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Orivesi_Langelmaki_Talviainen[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Oulu[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Padasjoki[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Padasjoki_Arrakoski[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Paltamo_Kivesvaara[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Parikkala[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Parkano[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pello[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pello_Ratasvaara[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Perho[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pernaja[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pieksamaki_Halkokumpu[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pihtipudas[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Porvoo_Suomenkyla[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Posio[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pudasjarvi[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pudasjarvi_Iso_Syote[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pudasjarvi_Kangasvaara[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Puolanka[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pyhatunturi[] = {
	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pyhavuori[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Pylkonmaki_Karankajarvi[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Raahe_Mestauskallio[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Raahe_Piehinki[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ranua_Haasionmaa[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ranua_Leppiaho[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rautavaara_Angervikko[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rautjarvi_Simpele[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ristijarvi[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rovaniemi[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rovaniemi_Ala_Nampa_Yli_Nampa_Rantalaki[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rovaniemi_Kaihuanvaara[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rovaniemi_Karhuvaara_Marrasjarvi[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rovaniemi_Marasenkallio[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rovaniemi_Meltaus_Sorviselka[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Rovaniemi_Sonka[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ruka[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ruovesi_Storminiemi[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Saarijarvi[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Saarijarvi_Kalmari[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Saarijarvi_Mahlu[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Salla_Hirvasvaara[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Salla_Ihistysjanka[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Salla_Naruska[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Salla_Saija[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Salla_Sallatunturi[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Salo_Isokyla[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Savukoski_Martti_Haarahonganmaa[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Savukoski_Tanhua[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Siilinjarvi[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Sipoo_Galthagen[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Sodankyla_Pittiovaara[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Sulkava_Vaatalanmaki[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Sysma_Liikola[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Taivalkoski[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Taivalkoski_Taivalvaara[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Tammela[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Tammisaari[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Tampere[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Tampere_Pyynikki[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Tervola[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Turku[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Utsjoki[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Utsjoki_Nuorgam_Njallavaara[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Utsjoki_Nuorgam_raja[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Utsjoki_Outakoski[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Utsjoki_Polvarniemi[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Utsjoki_Rovisuvanto[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Utsjoki_Tenola[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Uusikaupunki_Orivo[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vaala[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vaasa[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Valtimo[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vammala_Jyranvuori[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vammala_Roismala[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vammala_Savi[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vantaa_Hakunila[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Varpaisjarvi_Honkamaki[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Virrat_Lappavuori[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vuokatti[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Vuotso[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ylitornio_Ainiovaara[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Ylitornio_Raanujarvi[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fi_Yllas[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Abbeville[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Agen[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Ajaccio[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Albi[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Alen__on[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Ales[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Ales_Bouquet[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Amiens[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Angers[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Annecy[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Arcachon[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Argenton[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Aubenas[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Aurillac[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Autun[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Auxerre[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Avignon[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_BarleDuc[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Bastia[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Bayonne[] = {
	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Bergerac[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Besan__on[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Bordeaux[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Bordeaux_Bouliac[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Bordeaux_Cauderan[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Boulogne[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Bourges[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Brest[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Brive[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Caen[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Caen_Pincon[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Cannes[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Carcassonne[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Chambery[] = {
};

struct mux muxlist_FE_OFDM_fr_Chartres[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Chennevieres[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Cherbourg[] = {
	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_ClermontFerrand[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Cluses[] = {
};

struct mux muxlist_FE_OFDM_fr_Dieppe[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Dijon[] = {
};

struct mux muxlist_FE_OFDM_fr_Dunkerque[] = {
};

struct mux muxlist_FE_OFDM_fr_Epinal[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Evreux[] = {
	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Forbach[] = {
};

struct mux muxlist_FE_OFDM_fr_Gex[] = {
};

struct mux muxlist_FE_OFDM_fr_Grenoble[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Gueret[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Hirson[] = {
};

struct mux muxlist_FE_OFDM_fr_Hyeres[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_LaRochelle[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Laval[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_LeCreusot[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_LeHavre[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_LeMans[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_LePuyEnVelay[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Lille[] = {
};

struct mux muxlist_FE_OFDM_fr_Lille_Lambersart[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_LilleT2[] = {
	{.freq = 538167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Limoges[] = {
	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Longwy[] = {
};

struct mux muxlist_FE_OFDM_fr_Lorient[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Lyon_Fourviere[] = {
	{.freq = 754167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Lyon_Pilat[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Macon[] = {
};

struct mux muxlist_FE_OFDM_fr_Mantes[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Marseille[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Maubeuge[] = {
};

struct mux muxlist_FE_OFDM_fr_Meaux[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Mende[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Menton[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Metz[] = {
};

struct mux muxlist_FE_OFDM_fr_Mezieres[] = {
};

struct mux muxlist_FE_OFDM_fr_Montlucon[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Montpellier[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Mulhouse[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Nancy[] = {
	{.freq = 522166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Nantes[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_NeufchatelEnBray[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Nice[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Niort[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Orleans[] = {
	{.freq = 610166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Paris[] = {
	{.freq = 474166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810166000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Parthenay[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Perpignan[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Poitiers[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Privas[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Reims[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Rennes[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Roanne[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Rouen[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_SaintEtienne[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_SaintRaphael[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Sannois[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Sarrebourg[] = {
};

struct mux muxlist_FE_OFDM_fr_Sens[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Strasbourg[] = {
};

struct mux muxlist_FE_OFDM_fr_Toulon[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Toulouse[] = {
	{.freq = 754167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Toulouse_Midi[] = {
	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Tours[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Troyes[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Ussel[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Valence[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_AUTO, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_AUTO, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Valenciennes[] = {
};

struct mux muxlist_FE_OFDM_fr_Vannes[] = {
	{.freq = 674167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Villebon[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_fr_Vittel[] = {
};

struct mux muxlist_FE_OFDM_fr_Voiron[] = {
};

struct mux muxlist_FE_OFDM_gr_Athens[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_hr_Zagreb[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_AUTO, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_is_Reykjavik[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Aosta[] = {
	{.freq = 226500000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Bari[] = {
	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Bologna[] = {
	{.freq = 186000000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 203500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 212500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Bolzano[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Cagliari[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Caivano[] = {
	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Catania[] = {
	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 226500000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Conero[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Firenze[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Genova[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 219500000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Livorno[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Milano[] = {
	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Pagnacco[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 226500000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Palermo[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Pisa[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Roma[] = {
	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 186000000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_2_3, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Sassari[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 858000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 177500000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Torino[] = {
	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Trieste[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_AUTO, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Varese[] = {
	{.freq = 226500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_it_Venezia[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_lu_All[] = {
	{.freq = 191500000, .bw = BANDWIDTH_7_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_lv_Riga[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QPSK, .fechp = FEC_1_2, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_nl_All[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_nz_Waiatarua[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_3_4, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_16, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_pl_Wroclaw[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Alvdalen_Brunnsberg[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Alvdalsasen[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Alvsbyn[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Amot[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Angebo[] = {
	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Angelholm_Vegeholm[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ange_Snoberg[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Arvidsjaur_Jultrask[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Aspeboda[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Atvidaberg[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Avesta_Krylbo[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Backefors[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bankeryd[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bergsjo_Balleberget[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bergvik[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bollebygd[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bollnas[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Boras_Dalsjofors[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Boras_Sjobo[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Borlange_Idkerberget[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Borlange_Nygardarna[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bottnaryd_Ryd[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bromsebro[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Bruzaholm[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Byxelkrok[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Dadran[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Dalfors[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Dalstuga[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Degerfors[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Delary[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Djura[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Drevdagen[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Duvnas[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Duvnas_Basna[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Edsbyn[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Emmaboda_Balshult[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Enviken[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Fagersta[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Falerum_Centrum[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Falun_Lovberget[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Farila[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Faro_Ajkerstrask[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Farosund_Bunge[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Filipstad_Klockarhojden[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Finnveden[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Fredriksberg[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Fritsla[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Furudal[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gallivare[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Garpenberg_Kuppgarden[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gavle_Skogmur[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gnarp[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gnesta[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gnosjo_Marieholm[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Goteborg_Brudaremossen[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Goteborg_Slattadamm[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gullbrandstorp[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gunnarsbo[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Gusum[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hagfors_Varmullsasen[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hallaryd[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hallbo[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Halmstad_Hamnen[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Halmstad_Oskarstrom[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Harnosand_Harnon[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hassela[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Havdhem[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hedemora[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Helsingborg_Olympia[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hennan[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hestra_Aspas[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hjo_Grevback[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hofors[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hogfors[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hogsby_Virstad[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Holsbybrunn_Holsbyholm[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Horby_Sallerup[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Horken[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hudiksvall_Forsa[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Hudiksvall_Galgberget[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Huskvarna[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Idre[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ingatorp[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ingvallsbenning[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Irevik[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Jamjo[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Jarnforsen[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Jarvso[] = {
	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Jokkmokk_Tjalmejaure[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Jonkoping_Bondberget[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kalix[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Karbole[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Karlsborg_Vaberget[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Karlshamn[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Karlskrona_Vamo[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Karlstad_Sormon[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_1_2, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kaxholmen_Vistakulle[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kinnastrom[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kiruna_Kirunavaara[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kisa[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Knared[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kopmanholmen[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kopparberg[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kramfors_Lugnvik[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kristinehamn_Utsiktsberget[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kungsater[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Kungsberget_GI[] = {
	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Langshyttan[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Langshyttan_Engelsfors[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Leksand_Karingberget[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Lerdala[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Lilltjara_Digerberget[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Limedsforsen[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Lindshammar_Ramkvilla[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Linkoping_Vattentornet[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ljugarn[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Loffstrand[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Lonneberga[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Lorstrand[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ludvika_Bjorkasen[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Lumsheden_Trekanten[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Lycksele_Knaften[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Mahult[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Malmo_Jagersro[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Malung[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Mariannelund[] = {
	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Markaryd_Hualtet[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Matfors[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Molnbo_Tallstugan[] = {
};

struct mux muxlist_FE_OFDM_se_Molndal_Vasterberget[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Mora_Eldris[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Motala_Ervasteby[] = {
	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Mullsjo_Torestorp[] = {
	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Nassjo[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Navekvarn[] = {
	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Norrahammar[] = {
	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Norrkoping_Krokek[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Norrtalje_Sodra_Bergen[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Nykoping[] = {
	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Orebro_Lockhyttan[] = {
	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ornskoldsvik_As[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Oskarshamn[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ostersund_Brattasen[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Osthammar_Valo[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Overkalix[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Oxberg[] = {
	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Pajala[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Paulistom[] = {
	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Rattvik[] = {
	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Rengsjo[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Rorbacksnas[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sagmyra[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Salen[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Salfjallet[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sarna_Mickeltemplet[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Satila[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Saxdalen[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Siljansnas_Uvberget[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Skarstad[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Skattungbyn[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Skelleftea[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Skene_Nycklarberget[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Skovde[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Smedjebacken_Uvberget[] = {
	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Soderhamn[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Soderkoping[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sodertalje_Ragnhildsborg[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Solleftea_Hallsta[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Solleftea_Multra[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sorsjon[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Stockholm_Marieberg[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Stockholm_Nacka[] = {
	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Stora_Skedvi[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Storfjaten[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Storuman[] = {
	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Stromstad[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Styrsjobo[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sundborn[] = {
	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sundsbruk[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sundsvall_S_Stadsberget[] = {
	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sunne_Blabarskullen[] = {
	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Svartnas[] = {
	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Sveg_Brickan[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Taberg[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Tandadalen[] = {
	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Tasjo[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Tollsjo[] = {
	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Torsby_Bada[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Tranas_Bredkarr[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Tranemo[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Transtrand_Bolheden[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Traryd_Betas[] = {
	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Trollhattan[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Trosa[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Tystberga[] = {
	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Uddevalla_Herrestad[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ullared[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ulricehamn[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ulvshyttan_Porjus[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Uppsala_Rickomberga[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Uppsala_Vedyxa[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vaddo_Elmsta[] = {
	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Valdemarsvik[] = {
	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vannas_Granlundsberget[] = {
	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 594000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vansbro_Hummelberget[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Varberg_Grimeton[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vasteras_Lillharad[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 610000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vastervik_Farhult[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vaxbo[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vessigebro[] = {
	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vetlanda_Nye[] = {
	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vikmanshyttan[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Virserum[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Visby_Follingbo[] = {
	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Visby_Hamnen[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 586000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Visingso[] = {
	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Vislanda_Nydala[] = {
	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 602000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Voxna[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Ystad_Metallgatan[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_se_Yttermalung[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_sk_BanskaBystrica[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_sk_Bratislava[] = {
	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_sk_Kosice[] = {
	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_tw_Kaohsiung[] = {
	{.freq = 545000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 557000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 557000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_tw_Taipei[] = {
	{.freq = 533000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_1_2, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_8, .hierarchy = HIERARCHY_NONE},
 	{.freq = 557000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 581000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 	{.freq = 593000000, .bw = BANDWIDTH_6_MHZ, .constellation = QAM_16, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_8K, .guard = GUARD_INTERVAL_1_4, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Aberdare[] = {
	{.freq = 530167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 489833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 513833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Angus[] = {
	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 777833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 801833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 753833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 825833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_BeaconHill[] = {
	{.freq = 721833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 753833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Belmont[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Bilsdale[] = {
	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_BlackHill[] = {
	{.freq = 634167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Blaenplwyf[] = {
	{.freq = 530167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_BluebellHill[] = {
	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 665833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 641833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Bressay[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 497833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 521833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 553833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_BrierleyHill[] = {
	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 825833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 753833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 777833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 801833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_BristolIlchesterCres[] = {
	{.freq = 697833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_BristolKingsWeston[] = {
	{.freq = 482000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Bromsgrove[] = {
	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 537833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 569833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 489833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 513833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_BrougherMountain[] = {
	{.freq = 546167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Caldbeck[] = {
	{.freq = 506000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_CaradonHill[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 553833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 497833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Carmel[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 825833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 777833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 801833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Chatton[] = {
	{.freq = 626167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Chesterfield[] = {
	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Craigkelly[] = {
	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 489833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 513833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_CrystalPalace[] = {
	{.freq = 505833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 481833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 561833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 529833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 537833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Darvel[] = {
	{.freq = 481833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 505833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 561833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 529833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Divis[] = {
	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 569833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 489833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 513833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Dover[] = {
	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 745833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 785833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Durris[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 713833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Eitshal[] = {
	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 481833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 505833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 529833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 561833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_EmleyMoor[] = {
	{.freq = 722167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 625833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 649833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 673833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 705833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 697833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Fenham[] = {
	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Fenton[] = {
	{.freq = 577833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Ferryside[] = {
	{.freq = 474167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Guildford[] = {
	{.freq = 697833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Hannington[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Hastings[] = {
	{.freq = 553833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 521833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 497833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Heathfield[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 689833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 681833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 713833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_HemelHempstead[] = {
	{.freq = 690167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 777833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_HuntshawCross[] = {
	{.freq = 737833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 769833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 793833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 817833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 729833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 761833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Idle[] = {
	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_KeelylangHill[] = {
	{.freq = 690167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Keighley[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 729833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_KilveyHill[] = {
	{.freq = 505833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 481833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 529833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 561833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 553833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_KnockMore[] = {
	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 753833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Lancaster[] = {
	{.freq = 530167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_LarkStoke[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Limavady[] = {
	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 769833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 761833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Llanddona[] = {
	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Malvern[] = {
	{.freq = 722167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 634000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Mendip[] = {
	{.freq = 778167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Midhurst[] = {
	{.freq = 754167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 817833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Moel_y_Parc[] = {
	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Nottingham[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_OliversMount[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Oxford[] = {
	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 713833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 721833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_PendleForest[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 497833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 521833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 553833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Plympton[] = {
	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 833833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 785833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 809833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_PontopPike[] = {
	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 729833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Pontypool[] = {
	{.freq = 722000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Presely[] = {
	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 641833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 665833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 697833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Redruth[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 697833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 705833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Reigate[] = {
	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_RidgeHill[] = {
	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Rosemarkie[] = {
	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 633833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 657833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Rosneath[] = {
	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 729833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 761833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 785833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 809833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Rowridge[] = {
	{.freq = 489833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 530000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 545833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 513833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_RumsterForest[] = {
	{.freq = 530167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 482167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 506167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 562167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 802000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Saddleworth[] = {
	{.freq = 682000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 633833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 657833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 713833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Salisbury[] = {
	{.freq = 745833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 753833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 777833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 801833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 826000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 721833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_SandyHeath[] = {
	{.freq = 641833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 665833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 674167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Selkirk[] = {
	{.freq = 730167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Sheffield[] = {
	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_StocklandHill[] = {
	{.freq = 481833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 529833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 505833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 561833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 546167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Storeton[] = {
	{.freq = 546167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Sudbury[] = {
	{.freq = 698167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 738000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 754000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_SuttonColdfield[] = {
	{.freq = 634167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 658167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 682167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 714167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 722167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Tacolneston[] = {
	{.freq = 810000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 769833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 818000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_TheWrekin[] = {
	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 554000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 498167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 522167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 730000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 762000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Torosay[] = {
	{.freq = 490167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 538167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 474000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 553833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_TunbridgeWells[] = {
	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 618000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 778000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Waltham[] = {
	{.freq = 698000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 490000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 514000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 570000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 666000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 642000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_Wenvoe[] = {
	{.freq = 546000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 578000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 625833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 705833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 649833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 673833000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_WhitehawkHill[] = {
	{.freq = 834000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 706000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 746000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 690000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 770167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 794167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_OFDM_uk_WinterHill[] = {
	{.freq = 754167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 834167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 850167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_64, .fechp = FEC_2_3, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 842167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 786167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 810167000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 650000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 	{.freq = 626000000, .bw = BANDWIDTH_8_MHZ, .constellation = QAM_16, .fechp = FEC_3_4, .feclp = FEC_NONE, .tmode = TRANSMISSION_MODE_2K, .guard = GUARD_INTERVAL_1_32, .hierarchy = HIERARCHY_NONE},
 };

struct mux muxlist_FE_QAM_at_Innsbruck[] = {
	{ .freq = 450000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 490000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 442000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 546000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 554000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 562000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_at_Liwest[] = {
	{ .freq = 394000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 410000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 434000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 442000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 506000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 514000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 522000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 530000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 538000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 546000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 554000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 562000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 570000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 578000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 586000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 594000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 666000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 674000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 682000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 586000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 634000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 642000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 650000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 658000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 690000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_at_SalzburgAG[] = {
	{ .freq = 306000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 442000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 306000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_at_Vienna[] = {
	{ .freq = 377750000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_be_IN_DI_Integan[] = {
	{ .freq = 330000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 338000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 346000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 354000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 362000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 378000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 386000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 394000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 458000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 466000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 474000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 482000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 586000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_ch_unknown[] = {
	{ .freq = 530000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_ch_Video2000[] = {
	{ .freq = 306000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_ch_Zuerich_cablecom[] = {
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_de_Berlin[] = {
	{ .freq = 394000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 113000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 466000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_de_iesy[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 121000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 346000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 354000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 362000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 378000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 386000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 394000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 434000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 442000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 450000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 458000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 466000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 538000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_de_Kabel_BW[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_de_Muenchen[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 121000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 346000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 354000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 362000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 378000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 386000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 394000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 434000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 442000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 450000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 466000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 458000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_de_neftv[] = {
	{ .freq = 346000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 354000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 362000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 378000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 386000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 394000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 402000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 410000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 418000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 434000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 450000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 458000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 474000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 490000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 498000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 514000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 546000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_de_Primacom[] = {
	{ .freq = 306000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 314000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 322000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 330000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 338000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 346000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 354000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 362000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 378000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 386000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 394000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 434000000, .symrate = 6956000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 610000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 746000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 754000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 762000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 802000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 810000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 818000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 826000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 834000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 634000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_de_Unitymedia[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 121000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 338000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 346000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 354000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 362000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 378000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 386000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 394000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 434000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 442000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 450000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 458000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 466000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 474000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 522000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 530000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 538000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 554000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 562000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 570000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 610000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_64},
	{ .freq = 650000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 658000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 666000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
	{ .freq = 674000000, .symrate = 6900000, .fec = FEC_AUTO, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_dk_Odense[] = {
	{ .freq = 442000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 434000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_256},
	{ .freq = 255000000, .symrate = 5000000, .fec = FEC_2_3, .constellation = QAM_256},
	{ .freq = 506000000, .symrate = 6875000, .fec = FEC_2_3, .constellation = QAM_256},
	{ .freq = 562000000, .symrate = 6875000, .fec = FEC_2_3, .constellation = QAM_256},
	{ .freq = 610000000, .symrate = 6875000, .fec = FEC_2_3, .constellation = QAM_256},
	{ .freq = 754000000, .symrate = 6875000, .fec = FEC_2_3, .constellation = QAM_256},
	{ .freq = 770000000, .symrate = 6875000, .fec = FEC_2_3, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_es_Euskaltel[] = {
	{ .freq = 714000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 722000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 730000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 738000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 746000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 754000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 762000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 770000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 778000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 786000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 794000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 802000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 810000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
	{ .freq = 818000000, .symrate = 6875000, .fec = FEC_3_4, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_fi_3ktv[] = {
	{ .freq = 154000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 162000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 170000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 232000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 298000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 306000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 314000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 322000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 330000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 338000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 346000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 354000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 362000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 370000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 378000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 394000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 402000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 450000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_128},
};

struct mux muxlist_FE_QAM_fi_HTV[] = {
	{ .freq = 283000000, .symrate = 5900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 154000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
};

struct mux muxlist_FE_QAM_fi_jkl[] = {
	{ .freq = 514000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 162000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 490000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 498000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
};

struct mux muxlist_FE_QAM_fi_Joensuu_Tikka[] = {
	{ .freq = 154000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 162000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 170000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 434000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 458000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 466000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 474000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
};

struct mux muxlist_FE_QAM_fi_sonera[] = {
	{ .freq = 154000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 162000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 170000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 314000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 322000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 338000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 346000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 354000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
};

struct mux muxlist_FE_QAM_fi_TTV[] = {
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 346000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
};

struct mux muxlist_FE_QAM_fi_Turku[] = {
	{ .freq = 146000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 154000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 162000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 322000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 330000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 338000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 362000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 378000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 386000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 402000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 410000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 418000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 426000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 442000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_128},
	{ .freq = 354000000, .symrate = 6900000, .fec = FEC_NONE, .constellation = QAM_256},
};

struct mux muxlist_FE_QAM_fi_vaasa_oncable[] = {
	{ .freq = 386000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 394000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 143000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 434000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 362000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 418000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 314000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 410000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 442000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 306000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_fr_noos_numericable[] = {
	{ .freq = 123000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 131000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 139000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 147000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 155000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 163000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 171000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 179000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 187000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 195000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 203000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 211000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 219000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 227000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 235000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 243000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 251000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 259000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 267000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 275000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 283000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 291000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 299000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 315000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 323000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 339000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 347000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 706000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 714000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 722000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 730000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 738000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 746000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 748000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 754000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 762000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 834000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 842000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 850000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_lu_Ettelbruck_ACE[] = {
	{ .freq = 634000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 642000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 650000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 666000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 674000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 682000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 690000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 698000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 706000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 714000000, .symrate = 6900000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 656000000, .symrate = 3450000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 660000000, .symrate = 3450000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 720000000, .symrate = 3450000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 732000000, .symrate = 3450000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 724000000, .symrate = 3450000, .fec = FEC_5_6, .constellation = QAM_64},
	{ .freq = 728000000, .symrate = 3450000, .fec = FEC_5_6, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_nl_Casema[] = {
	{ .freq = 372000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_no_Oslo_CanalDigital[] = {
	{ .freq = 354000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 362000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 370000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 378000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 386000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 394000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 402000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 410000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 418000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 426000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 450000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
	{ .freq = 474000000, .symrate = 6950000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct mux muxlist_FE_QAM_se_comhem[] = {
	{ .freq = 362000000, .symrate = 6875000, .fec = FEC_NONE, .constellation = QAM_64},
};

struct {
int type;
const char *name;
struct mux *muxes;
int nmuxes;
const char *comment;
} networks[] = {
{
	.type = FE_QPSK,
	.name = "ABS1-75.0E",
	.muxes = muxlist_FE_QPSK_ABS1_75_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_ABS1_75_0E) / sizeof(struct mux),
	.comment = "ABS-1 @ 75E"
},
{
	.type = FE_QPSK,
	.name = "Amazonas-61.0W",
	.muxes = muxlist_FE_QPSK_Amazonas_61_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Amazonas_61_0W) / sizeof(struct mux),
	.comment = "Amazonas @ 61.0W"
},
{
	.type = FE_QPSK,
	.name = "AMC1-103w",
	.muxes = muxlist_FE_QPSK_AMC1_103w,
	.nmuxes = sizeof(muxlist_FE_QPSK_AMC1_103w) / sizeof(struct mux),
	.comment = "AMC 1 @ 103W"
},
{
	.type = FE_QPSK,
	.name = "AMC2-85w",
	.muxes = muxlist_FE_QPSK_AMC2_85w,
	.nmuxes = sizeof(muxlist_FE_QPSK_AMC2_85w) / sizeof(struct mux),
	.comment = "AMC 2 @ 85W"
},
{
	.type = FE_QPSK,
	.name = "AMC3-87w",
	.muxes = muxlist_FE_QPSK_AMC3_87w,
	.nmuxes = sizeof(muxlist_FE_QPSK_AMC3_87w) / sizeof(struct mux),
	.comment = "AMC 3 @ 87.0W"
},
{
	.type = FE_QPSK,
	.name = "AMC4-101w",
	.muxes = muxlist_FE_QPSK_AMC4_101w,
	.nmuxes = sizeof(muxlist_FE_QPSK_AMC4_101w) / sizeof(struct mux),
	.comment = "AMC 4 @ 101.0W"
},
{
	.type = FE_QPSK,
	.name = "AMC5-79w",
	.muxes = muxlist_FE_QPSK_AMC5_79w,
	.nmuxes = sizeof(muxlist_FE_QPSK_AMC5_79w) / sizeof(struct mux),
	.comment = "AMC 5 @ 79W"
},
{
	.type = FE_QPSK,
	.name = "AMC6-72w",
	.muxes = muxlist_FE_QPSK_AMC6_72w,
	.nmuxes = sizeof(muxlist_FE_QPSK_AMC6_72w) / sizeof(struct mux),
	.comment = "AMC 6 @ 72W"
},
{
	.type = FE_QPSK,
	.name = "AMC9-83w",
	.muxes = muxlist_FE_QPSK_AMC9_83w,
	.nmuxes = sizeof(muxlist_FE_QPSK_AMC9_83w) / sizeof(struct mux),
	.comment = "AMC 9 @ 83W"
},
{
	.type = FE_QPSK,
	.name = "Amos-4w",
	.muxes = muxlist_FE_QPSK_Amos_4w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Amos_4w) / sizeof(struct mux),
	.comment = "Amos 6 @ 4W"
},
{
	.type = FE_QPSK,
	.name = "Anik-F1-107.3W",
	.muxes = muxlist_FE_QPSK_Anik_F1_107_3W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Anik_F1_107_3W) / sizeof(struct mux),
	.comment = "Anik F1 @ 107.3W"
},
{
	.type = FE_QPSK,
	.name = "AsiaSat3S_C-105.5E",
	.muxes = muxlist_FE_QPSK_AsiaSat3S_C_105_5E,
	.nmuxes = sizeof(muxlist_FE_QPSK_AsiaSat3S_C_105_5E) / sizeof(struct mux),
	.comment = "AsiaSat 3S 105.5E C-BAND"
},
{
	.type = FE_QPSK,
	.name = "Astra-19.2E",
	.muxes = muxlist_FE_QPSK_Astra_19_2E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Astra_19_2E) / sizeof(struct mux),
	.comment = "Astra 19.2E SDT info service transponder"
},
{
	.type = FE_QPSK,
	.name = "Astra-28.2E",
	.muxes = muxlist_FE_QPSK_Astra_28_2E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Astra_28_2E) / sizeof(struct mux),
	.comment = "Astra 28.2E SDT info service transponder"
},
{
	.type = FE_QPSK,
	.name = "Atlantic-Bird-1-12.5W",
	.muxes = muxlist_FE_QPSK_Atlantic_Bird_1_12_5W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Atlantic_Bird_1_12_5W) / sizeof(struct mux),
	.comment = "Atlantic Bird 1 @ 12.5W"
},
{
	.type = FE_QPSK,
	.name = "BrasilSat-B1-75.0W",
	.muxes = muxlist_FE_QPSK_BrasilSat_B1_75_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_BrasilSat_B1_75_0W) / sizeof(struct mux),
	.comment = "Brasilsat B1 @ 75.0W"
},
{
	.type = FE_QPSK,
	.name = "BrasilSat-B2-65.0W",
	.muxes = muxlist_FE_QPSK_BrasilSat_B2_65_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_BrasilSat_B2_65_0W) / sizeof(struct mux),
	.comment = "Brasilsat B2 @ 65.0W"
},
{
	.type = FE_QPSK,
	.name = "BrasilSat-B3-84.0W",
	.muxes = muxlist_FE_QPSK_BrasilSat_B3_84_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_BrasilSat_B3_84_0W) / sizeof(struct mux),
	.comment = "Brasilsat B3 @ 84.0W"
},
{
	.type = FE_QPSK,
	.name = "BrasilSat-B4-70.0W",
	.muxes = muxlist_FE_QPSK_BrasilSat_B4_70_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_BrasilSat_B4_70_0W) / sizeof(struct mux),
	.comment = "Brasilsat B4 @ 70.0W"
},
{
	.type = FE_QPSK,
	.name = "Estrela-do-Sul-63.0W",
	.muxes = muxlist_FE_QPSK_Estrela_do_Sul_63_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Estrela_do_Sul_63_0W) / sizeof(struct mux),
	.comment = "Estrela do Sul @ 63.0W"
},
{
	.type = FE_QPSK,
	.name = "Eurobird1-28.5E",
	.muxes = muxlist_FE_QPSK_Eurobird1_28_5E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Eurobird1_28_5E) / sizeof(struct mux),
	.comment = "Eurobird 28.5E SDT info service transponder"
},
{
	.type = FE_QPSK,
	.name = "EutelsatW2-16E",
	.muxes = muxlist_FE_QPSK_EutelsatW2_16E,
	.nmuxes = sizeof(muxlist_FE_QPSK_EutelsatW2_16E) / sizeof(struct mux),
	.comment = "Eutelsat W2 @ 16E"
},
{
	.type = FE_QPSK,
	.name = "Express-3A-11.0W",
	.muxes = muxlist_FE_QPSK_Express_3A_11_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Express_3A_11_0W) / sizeof(struct mux),
	.comment = "Express 3A @ 11.0W"
},
{
	.type = FE_QPSK,
	.name = "ExpressAM1-40.0E",
	.muxes = muxlist_FE_QPSK_ExpressAM1_40_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_ExpressAM1_40_0E) / sizeof(struct mux),
	.comment = "Express AM1 @ 40E"
},
{
	.type = FE_QPSK,
	.name = "ExpressAM22-53.0E",
	.muxes = muxlist_FE_QPSK_ExpressAM22_53_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_ExpressAM22_53_0E) / sizeof(struct mux),
	.comment = "Express AM 22 @ 53E"
},
{
	.type = FE_QPSK,
	.name = "ExpressAM2-80.0E",
	.muxes = muxlist_FE_QPSK_ExpressAM2_80_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_ExpressAM2_80_0E) / sizeof(struct mux),
	.comment = "Express AM2 @ 80E"
},
{
	.type = FE_QPSK,
	.name = "Galaxy10R-123w",
	.muxes = muxlist_FE_QPSK_Galaxy10R_123w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Galaxy10R_123w) / sizeof(struct mux),
	.comment = "Galaxy 10R @ 123W"
},
{
	.type = FE_QPSK,
	.name = "Galaxy11-91w",
	.muxes = muxlist_FE_QPSK_Galaxy11_91w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Galaxy11_91w) / sizeof(struct mux),
	.comment = "Galaxy 11 @ 91W"
},
{
	.type = FE_QPSK,
	.name = "Galaxy25-97w",
	.muxes = muxlist_FE_QPSK_Galaxy25_97w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Galaxy25_97w) / sizeof(struct mux),
	.comment = "Galaxy 25 @ 97W"
},
{
	.type = FE_QPSK,
	.name = "Galaxy26-93w",
	.muxes = muxlist_FE_QPSK_Galaxy26_93w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Galaxy26_93w) / sizeof(struct mux),
	.comment = "Galaxy 26 @ 93W"
},
{
	.type = FE_QPSK,
	.name = "Galaxy27-129w",
	.muxes = muxlist_FE_QPSK_Galaxy27_129w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Galaxy27_129w) / sizeof(struct mux),
	.comment = "Galaxy 27 @ 129W"
},
{
	.type = FE_QPSK,
	.name = "Galaxy28-89w",
	.muxes = muxlist_FE_QPSK_Galaxy28_89w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Galaxy28_89w) / sizeof(struct mux),
	.comment = "Galaxy 28 @ 89W"
},
{
	.type = FE_QPSK,
	.name = "Galaxy3C-95w",
	.muxes = muxlist_FE_QPSK_Galaxy3C_95w,
	.nmuxes = sizeof(muxlist_FE_QPSK_Galaxy3C_95w) / sizeof(struct mux),
	.comment = "Galaxy 3C @ 95W"
},
{
	.type = FE_QPSK,
	.name = "Hispasat-30.0W",
	.muxes = muxlist_FE_QPSK_Hispasat_30_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Hispasat_30_0W) / sizeof(struct mux),
	.comment = "Hispasat 30.0W"
},
{
	.type = FE_QPSK,
	.name = "Hotbird-13.0E",
	.muxes = muxlist_FE_QPSK_Hotbird_13_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Hotbird_13_0E) / sizeof(struct mux),
	.comment = "EUTELSAT SkyPlex, Hotbird 13E"
},
{
	.type = FE_QPSK,
	.name = "IA5-97w",
	.muxes = muxlist_FE_QPSK_IA5_97w,
	.nmuxes = sizeof(muxlist_FE_QPSK_IA5_97w) / sizeof(struct mux),
	.comment = "Intelsat Americas 5 @ 97W"
},
{
	.type = FE_QPSK,
	.name = "IA6-93w",
	.muxes = muxlist_FE_QPSK_IA6_93w,
	.nmuxes = sizeof(muxlist_FE_QPSK_IA6_93w) / sizeof(struct mux),
	.comment = "Intelsat Americas 6 @ 93W"
},
{
	.type = FE_QPSK,
	.name = "IA7-129w",
	.muxes = muxlist_FE_QPSK_IA7_129w,
	.nmuxes = sizeof(muxlist_FE_QPSK_IA7_129w) / sizeof(struct mux),
	.comment = "Intelsat Americas 7 @ 129W"
},
{
	.type = FE_QPSK,
	.name = "IA8-89w",
	.muxes = muxlist_FE_QPSK_IA8_89w,
	.nmuxes = sizeof(muxlist_FE_QPSK_IA8_89w) / sizeof(struct mux),
	.comment = "Intelsat Americas 8 @ 89W"
},
{
	.type = FE_QPSK,
	.name = "Intel4-72.0E",
	.muxes = muxlist_FE_QPSK_Intel4_72_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intel4_72_0E) / sizeof(struct mux),
	.comment = "Intel4 @ 72E"
},
{
	.type = FE_QPSK,
	.name = "Intel904-60.0E",
	.muxes = muxlist_FE_QPSK_Intel904_60_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intel904_60_0E) / sizeof(struct mux),
	.comment = "Intel904 @ 60E"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-1002-1.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_1002_1_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_1002_1_0W) / sizeof(struct mux),
	.comment = "Intelsat 1002 @ 1.0W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-11-43.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_11_43_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_11_43_0W) / sizeof(struct mux),
	.comment = "Intelsat 11 @ 43.0W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-1R-45.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_1R_45_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_1R_45_0W) / sizeof(struct mux),
	.comment = "Intelsat 1R @ 45.0W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-3R-43.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_3R_43_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_3R_43_0W) / sizeof(struct mux),
	.comment = "Intelsat 3R @ 43.0W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-6B-43.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_6B_43_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_6B_43_0W) / sizeof(struct mux),
	.comment = "Intelsat 6B @ 43.0W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-705-50.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_705_50_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_705_50_0W) / sizeof(struct mux),
	.comment = "Intelsat 705 @ 50.0W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-707-53.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_707_53_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_707_53_0W) / sizeof(struct mux),
	.comment = "Intelsat 707 @ 53.0W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-805-55.5W",
	.muxes = muxlist_FE_QPSK_Intelsat_805_55_5W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_805_55_5W) / sizeof(struct mux),
	.comment = "Intelsat 805 @ 55.5W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-903-34.5W",
	.muxes = muxlist_FE_QPSK_Intelsat_903_34_5W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_903_34_5W) / sizeof(struct mux),
	.comment = "Intelsat 903 @ 34.5W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-905-24.5W",
	.muxes = muxlist_FE_QPSK_Intelsat_905_24_5W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_905_24_5W) / sizeof(struct mux),
	.comment = "Intelsat 905 @ 24.5W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-907-27.5W",
	.muxes = muxlist_FE_QPSK_Intelsat_907_27_5W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_907_27_5W) / sizeof(struct mux),
	.comment = "Intelsat 907 @ 27.5W"
},
{
	.type = FE_QPSK,
	.name = "Intelsat-9-58.0W",
	.muxes = muxlist_FE_QPSK_Intelsat_9_58_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Intelsat_9_58_0W) / sizeof(struct mux),
	.comment = "Intelsat 9 @ 58.0W"
},
{
	.type = FE_QPSK,
	.name = "Nahuel-1-71.8W",
	.muxes = muxlist_FE_QPSK_Nahuel_1_71_8W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Nahuel_1_71_8W) / sizeof(struct mux),
	.comment = "Nahuel 1 @ 71.8W"
},
{
	.type = FE_QPSK,
	.name = "Nilesat101+102-7.0W",
	.muxes = muxlist_FE_QPSK_Nilesat101_102_7_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Nilesat101_102_7_0W) / sizeof(struct mux),
	.comment = "Nilesat 101/102 & Atlantic Bird @ 7W"
},
{
	.type = FE_QPSK,
	.name = "NSS-10-37.5W",
	.muxes = muxlist_FE_QPSK_NSS_10_37_5W,
	.nmuxes = sizeof(muxlist_FE_QPSK_NSS_10_37_5W) / sizeof(struct mux),
	.comment = "NSS 10 @ 37.5W"
},
{
	.type = FE_QPSK,
	.name = "NSS-7-22.0W",
	.muxes = muxlist_FE_QPSK_NSS_7_22_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_NSS_7_22_0W) / sizeof(struct mux),
	.comment = "NSS 7 @ 22.0W"
},
{
	.type = FE_QPSK,
	.name = "NSS-806-40.5W",
	.muxes = muxlist_FE_QPSK_NSS_806_40_5W,
	.nmuxes = sizeof(muxlist_FE_QPSK_NSS_806_40_5W) / sizeof(struct mux),
	.comment = "NSS 806 @ 40.5W"
},
{
	.type = FE_QPSK,
	.name = "OptusC1-156E",
	.muxes = muxlist_FE_QPSK_OptusC1_156E,
	.nmuxes = sizeof(muxlist_FE_QPSK_OptusC1_156E) / sizeof(struct mux),
	.comment = "Optus C1 satellite 156E"
},
{
	.type = FE_QPSK,
	.name = "PAS-43.0W",
	.muxes = muxlist_FE_QPSK_PAS_43_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_PAS_43_0W) / sizeof(struct mux),
	.comment = "PAS 6/6B/3R at 43.0W"
},
{
	.type = FE_QPSK,
	.name = "Satmex-5-116.8W",
	.muxes = muxlist_FE_QPSK_Satmex_5_116_8W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Satmex_5_116_8W) / sizeof(struct mux),
	.comment = "Satmex 5 @ 116.8W"
},
{
	.type = FE_QPSK,
	.name = "Satmex-6-113.0W",
	.muxes = muxlist_FE_QPSK_Satmex_6_113_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Satmex_6_113_0W) / sizeof(struct mux),
	.comment = "Satmex 6 @ 113.0W"
},
{
	.type = FE_QPSK,
	.name = "SBS6-74w",
	.muxes = muxlist_FE_QPSK_SBS6_74w,
	.nmuxes = sizeof(muxlist_FE_QPSK_SBS6_74w) / sizeof(struct mux),
	.comment = "SBS 6 @ 74W"
},
{
	.type = FE_QPSK,
	.name = "Sirius-5.0E",
	.muxes = muxlist_FE_QPSK_Sirius_5_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Sirius_5_0E) / sizeof(struct mux),
	.comment = "Sirius 5.0E"
},
{
	.type = FE_QPSK,
	.name = "Telecom2-8.0W",
	.muxes = muxlist_FE_QPSK_Telecom2_8_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Telecom2_8_0W) / sizeof(struct mux),
	.comment = "Telecom2 8.0W"
},
{
	.type = FE_QPSK,
	.name = "Telstar-12-15.0W",
	.muxes = muxlist_FE_QPSK_Telstar_12_15_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Telstar_12_15_0W) / sizeof(struct mux),
	.comment = "Telstar 12 @ 15.0W"
},
{
	.type = FE_QPSK,
	.name = "Telstar12-15.0W",
	.muxes = muxlist_FE_QPSK_Telstar12_15_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Telstar12_15_0W) / sizeof(struct mux),
	.comment = "Telstar 12 15.0W"
},
{
	.type = FE_QPSK,
	.name = "Thor-1.0W",
	.muxes = muxlist_FE_QPSK_Thor_1_0W,
	.nmuxes = sizeof(muxlist_FE_QPSK_Thor_1_0W) / sizeof(struct mux),
	.comment = "Thor 1.0W"
},
{
	.type = FE_QPSK,
	.name = "Turksat-42.0E",
	.muxes = muxlist_FE_QPSK_Turksat_42_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Turksat_42_0E) / sizeof(struct mux),
	.comment = "Turksat 42.0E"
},
{
	.type = FE_QPSK,
	.name = "Yamal201-90.0E",
	.muxes = muxlist_FE_QPSK_Yamal201_90_0E,
	.nmuxes = sizeof(muxlist_FE_QPSK_Yamal201_90_0E) / sizeof(struct mux),
	.comment = "Yamal201 @ 90E"
},
{
	.type = FE_OFDM,
	.name = "at-Offical",
	.muxes = muxlist_FE_OFDM_at_Offical,
	.nmuxes = sizeof(muxlist_FE_OFDM_at_Offical) / sizeof(struct mux),
	.comment = "Austria, all DVB-T transmitters run by ORS"
},
{
	.type = FE_OFDM,
	.name = "au-Adelaide",
	.muxes = muxlist_FE_OFDM_au_Adelaide,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Adelaide) / sizeof(struct mux),
	.comment = "Australia / Adelaide / Mt Lofty"
},
{
	.type = FE_OFDM,
	.name = "au-Brisbane",
	.muxes = muxlist_FE_OFDM_au_Brisbane,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Brisbane) / sizeof(struct mux),
	.comment = "Australia / Brisbane (Mt Coot-tha transmitters)"
},
{
	.type = FE_OFDM,
	.name = "au-Cairns",
	.muxes = muxlist_FE_OFDM_au_Cairns,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Cairns) / sizeof(struct mux),
	.comment = "Australia / Cairns (Mt Bellenden-Ker transmitters)"
},
{
	.type = FE_OFDM,
	.name = "au-canberra",
	.muxes = muxlist_FE_OFDM_au_canberra,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_canberra) / sizeof(struct mux),
	.comment = "Australia / Canberra / Woden"
},
{
	.type = FE_OFDM,
	.name = "au-Canberra-Black-Mt",
	.muxes = muxlist_FE_OFDM_au_Canberra_Black_Mt,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Canberra_Black_Mt) / sizeof(struct mux),
	.comment = "Australia / Canberra / Black Mt"
},
{
	.type = FE_OFDM,
	.name = "au-Darwin",
	.muxes = muxlist_FE_OFDM_au_Darwin,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Darwin) / sizeof(struct mux),
	.comment = "ABC (UHF 30)"
},
{
	.type = FE_OFDM,
	.name = "au-GoldCoast",
	.muxes = muxlist_FE_OFDM_au_GoldCoast,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_GoldCoast) / sizeof(struct mux),
	.comment = "DVB-T frequencies & modulation for the Gold Coast, Australia (Mt Tamborine)"
},
{
	.type = FE_OFDM,
	.name = "au-Hobart",
	.muxes = muxlist_FE_OFDM_au_Hobart,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Hobart) / sizeof(struct mux),
	.comment = "Australia / Tasmania / Hobart"
},
{
	.type = FE_OFDM,
	.name = "au-Mackay",
	.muxes = muxlist_FE_OFDM_au_Mackay,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Mackay) / sizeof(struct mux),
	.comment = "Australia / Mackay (Mt Blackwood transmitters)"
},
{
	.type = FE_OFDM,
	.name = "au-Melbourne",
	.muxes = muxlist_FE_OFDM_au_Melbourne,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Melbourne) / sizeof(struct mux),
	.comment = "Australia / Melbourne (Mt Dandenong transmitters)"
},
{
	.type = FE_OFDM,
	.name = "au-Melbourne-Upwey",
	.muxes = muxlist_FE_OFDM_au_Melbourne_Upwey,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Melbourne_Upwey) / sizeof(struct mux),
	.comment = "Australia / Melbourne (Upwey Repeater)"
},
{
	.type = FE_OFDM,
	.name = "au-MidNorthCoast",
	.muxes = muxlist_FE_OFDM_au_MidNorthCoast,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_MidNorthCoast) / sizeof(struct mux),
	.comment = "Australia ABC Mid North Coast"
},
{
	.type = FE_OFDM,
	.name = "au-Newcastle",
	.muxes = muxlist_FE_OFDM_au_Newcastle,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Newcastle) / sizeof(struct mux),
	.comment = "Australia / Newcastle"
},
{
	.type = FE_OFDM,
	.name = "au-Perth",
	.muxes = muxlist_FE_OFDM_au_Perth,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Perth) / sizeof(struct mux),
	.comment = "Australia / Perth"
},
{
	.type = FE_OFDM,
	.name = "au-Perth_Roleystone",
	.muxes = muxlist_FE_OFDM_au_Perth_Roleystone,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Perth_Roleystone) / sizeof(struct mux),
	.comment = "Australia / Perth (Roleystone transmitter)"
},
{
	.type = FE_OFDM,
	.name = "au-SpencerGulf",
	.muxes = muxlist_FE_OFDM_au_SpencerGulf,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_SpencerGulf) / sizeof(struct mux),
	.comment = "Australia / South Australia / Pt Pirie (THE BLUFF)"
},
{
	.type = FE_OFDM,
	.name = "au-SunshineCoast",
	.muxes = muxlist_FE_OFDM_au_SunshineCoast,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_SunshineCoast) / sizeof(struct mux),
	.comment = "Australia / Sunshine Coast"
},
{
	.type = FE_OFDM,
	.name = "au-Sydney_Kings_Cross",
	.muxes = muxlist_FE_OFDM_au_Sydney_Kings_Cross,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Sydney_Kings_Cross) / sizeof(struct mux),
	.comment = "Australia / Sydney / Kings Cross and North Head"
},
{
	.type = FE_OFDM,
	.name = "au-Sydney_North_Shore",
	.muxes = muxlist_FE_OFDM_au_Sydney_North_Shore,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Sydney_North_Shore) / sizeof(struct mux),
	.comment = "Australia / Sydney / North Shore (aka Artarmon/Gore Hill/Willoughby)"
},
{
	.type = FE_OFDM,
	.name = "au-Tamworth",
	.muxes = muxlist_FE_OFDM_au_Tamworth,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Tamworth) / sizeof(struct mux),
	.comment = "Australia / NSW / New England / Tamworth / Mt.Soma"
},
{
	.type = FE_OFDM,
	.name = "au-Townsville",
	.muxes = muxlist_FE_OFDM_au_Townsville,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Townsville) / sizeof(struct mux),
	.comment = "Australia / Brisbane (Mt Coot-tha transmitters)"
},
{
	.type = FE_OFDM,
	.name = "au-unknown",
	.muxes = muxlist_FE_OFDM_au_unknown,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_unknown) / sizeof(struct mux),
	.comment = "Australia ABC"
},
{
	.type = FE_OFDM,
	.name = "au-WaggaWagga",
	.muxes = muxlist_FE_OFDM_au_WaggaWagga,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_WaggaWagga) / sizeof(struct mux),
	.comment = "Australia / Wagga Wagga (Mt Ulundra)"
},
{
	.type = FE_OFDM,
	.name = "au-Wollongong",
	.muxes = muxlist_FE_OFDM_au_Wollongong,
	.nmuxes = sizeof(muxlist_FE_OFDM_au_Wollongong) / sizeof(struct mux),
	.comment = "Australia / Wollongong"
},
{
	.type = FE_OFDM,
	.name = "be-Libramont",
	.muxes = muxlist_FE_OFDM_be_Libramont,
	.nmuxes = sizeof(muxlist_FE_OFDM_be_Libramont) / sizeof(struct mux),
	.comment = "Libramont - Belgique"
},
{
	.type = FE_OFDM,
	.name = "be-Schoten",
	.muxes = muxlist_FE_OFDM_be_Schoten,
	.nmuxes = sizeof(muxlist_FE_OFDM_be_Schoten) / sizeof(struct mux),
	.comment = "Schoten-Antwerpen - Belgie"
},
{
	.type = FE_OFDM,
	.name = "be-St_Pieters_Leeuw",
	.muxes = muxlist_FE_OFDM_be_St_Pieters_Leeuw,
	.nmuxes = sizeof(muxlist_FE_OFDM_be_St_Pieters_Leeuw) / sizeof(struct mux),
	.comment = "St.-Pieters-Leeuw - Belgie"
},
{
	.type = FE_OFDM,
	.name = "be-Tournai",
	.muxes = muxlist_FE_OFDM_be_Tournai,
	.nmuxes = sizeof(muxlist_FE_OFDM_be_Tournai) / sizeof(struct mux),
	.comment = "Tournai - Belgique"
},
{
	.type = FE_OFDM,
	.name = "ch-All",
	.muxes = muxlist_FE_OFDM_ch_All,
	.nmuxes = sizeof(muxlist_FE_OFDM_ch_All) / sizeof(struct mux),
	.comment = "Switzerland, whole country"
},
{
	.type = FE_OFDM,
	.name = "ch-Citycable",
	.muxes = muxlist_FE_OFDM_ch_Citycable,
	.nmuxes = sizeof(muxlist_FE_OFDM_ch_Citycable) / sizeof(struct mux),
	.comment = "Lausanne - Switzerland (DVB-T on CityCable cable network)"
},
{
	.type = FE_OFDM,
	.name = "cz-Brno",
	.muxes = muxlist_FE_OFDM_cz_Brno,
	.nmuxes = sizeof(muxlist_FE_OFDM_cz_Brno) / sizeof(struct mux),
	.comment = "DVB-T Brno (Brno, Czech Republic)"
},
{
	.type = FE_OFDM,
	.name = "cz-Domazlice",
	.muxes = muxlist_FE_OFDM_cz_Domazlice,
	.nmuxes = sizeof(muxlist_FE_OFDM_cz_Domazlice) / sizeof(struct mux),
	.comment = "DVB-T Domažlice (Domažlice, Czech Republic)"
},
{
	.type = FE_OFDM,
	.name = "cz-Ostrava",
	.muxes = muxlist_FE_OFDM_cz_Ostrava,
	.nmuxes = sizeof(muxlist_FE_OFDM_cz_Ostrava) / sizeof(struct mux),
	.comment = "DVB-T Ostrava (Ostrava, Czech Republic)"
},
{
	.type = FE_OFDM,
	.name = "cz-Praha",
	.muxes = muxlist_FE_OFDM_cz_Praha,
	.nmuxes = sizeof(muxlist_FE_OFDM_cz_Praha) / sizeof(struct mux),
	.comment = "DVB-T Praha (Prague, Czech Republic)"
},
{
	.type = FE_OFDM,
	.name = "de-Aachen_Stadt",
	.muxes = muxlist_FE_OFDM_de_Aachen_Stadt,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Aachen_Stadt) / sizeof(struct mux),
	.comment = "DVB-T Aachen-Stadt"
},
{
	.type = FE_OFDM,
	.name = "de-Berlin",
	.muxes = muxlist_FE_OFDM_de_Berlin,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Berlin) / sizeof(struct mux),
	.comment = "DVB-T Berlin"
},
{
	.type = FE_OFDM,
	.name = "de-Bielefeld",
	.muxes = muxlist_FE_OFDM_de_Bielefeld,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Bielefeld) / sizeof(struct mux),
	.comment = "DVB-T Ostwestfalen"
},
{
	.type = FE_OFDM,
	.name = "de-Braunschweig",
	.muxes = muxlist_FE_OFDM_de_Braunschweig,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Braunschweig) / sizeof(struct mux),
	.comment = "DVB-T Braunschweig -- info from http://www.skyplus.seyen.de/DVB-T.html"
},
{
	.type = FE_OFDM,
	.name = "de-Bremen",
	.muxes = muxlist_FE_OFDM_de_Bremen,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Bremen) / sizeof(struct mux),
	.comment = "DVB-T Bremen/Unterweser"
},
{
	.type = FE_OFDM,
	.name = "de-Brocken_Magdeburg",
	.muxes = muxlist_FE_OFDM_de_Brocken_Magdeburg,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Brocken_Magdeburg) / sizeof(struct mux),
	.comment = "DVB-T Brocken/Magdeburg (Germany)"
},
{
	.type = FE_OFDM,
	.name = "de-Chemnitz",
	.muxes = muxlist_FE_OFDM_de_Chemnitz,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Chemnitz) / sizeof(struct mux),
	.comment = "DVB-T Chemnitz"
},
{
	.type = FE_OFDM,
	.name = "de-Dresden",
	.muxes = muxlist_FE_OFDM_de_Dresden,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Dresden) / sizeof(struct mux),
	.comment = "DVB-T Dresden"
},
{
	.type = FE_OFDM,
	.name = "de-Erfurt-Weimar",
	.muxes = muxlist_FE_OFDM_de_Erfurt_Weimar,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Erfurt_Weimar) / sizeof(struct mux),
	.comment = "DVB-T Erfurt-Weimar"
},
{
	.type = FE_OFDM,
	.name = "de-Frankfurt",
	.muxes = muxlist_FE_OFDM_de_Frankfurt,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Frankfurt) / sizeof(struct mux),
	.comment = "#########################################"
},
{
	.type = FE_OFDM,
	.name = "de-Freiburg",
	.muxes = muxlist_FE_OFDM_de_Freiburg,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Freiburg) / sizeof(struct mux),
	.comment = "DVB-T Freiburg M/V"
},
{
	.type = FE_OFDM,
	.name = "de-HalleSaale",
	.muxes = muxlist_FE_OFDM_de_HalleSaale,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_HalleSaale) / sizeof(struct mux),
	.comment = "DVB-T Halle/Saale (Germany)"
},
{
	.type = FE_OFDM,
	.name = "de-Hamburg",
	.muxes = muxlist_FE_OFDM_de_Hamburg,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Hamburg) / sizeof(struct mux),
	.comment = "DVB-T Hamburg"
},
{
	.type = FE_OFDM,
	.name = "de-Hannover",
	.muxes = muxlist_FE_OFDM_de_Hannover,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Hannover) / sizeof(struct mux),
	.comment = "DVB-T Hannover -- info from http://www.skyplus.seyen.de/DVB-T.html"
},
{
	.type = FE_OFDM,
	.name = "de-Kassel",
	.muxes = muxlist_FE_OFDM_de_Kassel,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Kassel) / sizeof(struct mux),
	.comment = "DVB-T, Germany, Nordhessen, Region Kassel"
},
{
	.type = FE_OFDM,
	.name = "de-Kiel",
	.muxes = muxlist_FE_OFDM_de_Kiel,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Kiel) / sizeof(struct mux),
	.comment = "DVB-T Kiel"
},
{
	.type = FE_OFDM,
	.name = "de-Koeln-Bonn",
	.muxes = muxlist_FE_OFDM_de_Koeln_Bonn,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Koeln_Bonn) / sizeof(struct mux),
	.comment = "DVB-T NRW/Bonn"
},
{
	.type = FE_OFDM,
	.name = "de-Leipzig",
	.muxes = muxlist_FE_OFDM_de_Leipzig,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Leipzig) / sizeof(struct mux),
	.comment = "DVB-T Leipzig (Germany)"
},
{
	.type = FE_OFDM,
	.name = "de-Loerrach",
	.muxes = muxlist_FE_OFDM_de_Loerrach,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Loerrach) / sizeof(struct mux),
	.comment = "DVB-T transmitter of Lörrach - Germany"
},
{
	.type = FE_OFDM,
	.name = "de-Luebeck",
	.muxes = muxlist_FE_OFDM_de_Luebeck,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Luebeck) / sizeof(struct mux),
	.comment = "DVB-T Lübeck"
},
{
	.type = FE_OFDM,
	.name = "de-Muenchen",
	.muxes = muxlist_FE_OFDM_de_Muenchen,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Muenchen) / sizeof(struct mux),
	.comment = "DVB-T Muenchen/Bayern"
},
{
	.type = FE_OFDM,
	.name = "de-Nuernberg",
	.muxes = muxlist_FE_OFDM_de_Nuernberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Nuernberg) / sizeof(struct mux),
	.comment = "DVB-T Nuernberg"
},
{
	.type = FE_OFDM,
	.name = "de-Osnabrueck",
	.muxes = muxlist_FE_OFDM_de_Osnabrueck,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Osnabrueck) / sizeof(struct mux),
	.comment = "DVB-T Osnabrueck/Lingen"
},
{
	.type = FE_OFDM,
	.name = "de-Ostbayern",
	.muxes = muxlist_FE_OFDM_de_Ostbayern,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Ostbayern) / sizeof(struct mux),
	.comment = "DVB-T Ostbayern/Bayern"
},
{
	.type = FE_OFDM,
	.name = "de-Ravensburg",
	.muxes = muxlist_FE_OFDM_de_Ravensburg,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Ravensburg) / sizeof(struct mux),
	.comment = "DVB-T Ravensburg/Bodensee"
},
{
	.type = FE_OFDM,
	.name = "de-Rostock",
	.muxes = muxlist_FE_OFDM_de_Rostock,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Rostock) / sizeof(struct mux),
	.comment = "DVB-T Rostock"
},
{
	.type = FE_OFDM,
	.name = "de-Ruhrgebiet",
	.muxes = muxlist_FE_OFDM_de_Ruhrgebiet,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Ruhrgebiet) / sizeof(struct mux),
	.comment = "DVB-T Düsseldorf/Ruhrgebiet"
},
{
	.type = FE_OFDM,
	.name = "de-Schwerin",
	.muxes = muxlist_FE_OFDM_de_Schwerin,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Schwerin) / sizeof(struct mux),
	.comment = "DVB-T Schwerin M/V"
},
{
	.type = FE_OFDM,
	.name = "de-Stuttgart",
	.muxes = muxlist_FE_OFDM_de_Stuttgart,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Stuttgart) / sizeof(struct mux),
	.comment = "DVB-T Stuttgart"
},
{
	.type = FE_OFDM,
	.name = "de-Wuerzburg",
	.muxes = muxlist_FE_OFDM_de_Wuerzburg,
	.nmuxes = sizeof(muxlist_FE_OFDM_de_Wuerzburg) / sizeof(struct mux),
	.comment = "DVB-T Wuerzburg/Bayern"
},
{
	.type = FE_OFDM,
	.name = "dk-All",
	.muxes = muxlist_FE_OFDM_dk_All,
	.nmuxes = sizeof(muxlist_FE_OFDM_dk_All) / sizeof(struct mux),
	.comment = "Denmark, whole country"
},
{
	.type = FE_OFDM,
	.name = "es-Albacete",
	.muxes = muxlist_FE_OFDM_es_Albacete,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Albacete) / sizeof(struct mux),
	.comment = "Spain, Albacete"
},
{
	.type = FE_OFDM,
	.name = "es-Alfabia",
	.muxes = muxlist_FE_OFDM_es_Alfabia,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Alfabia) / sizeof(struct mux),
	.comment = "DVB-T Alfabia, Mallorca, Balearic Islands, Spain."
},
{
	.type = FE_OFDM,
	.name = "es-Alicante",
	.muxes = muxlist_FE_OFDM_es_Alicante,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Alicante) / sizeof(struct mux),
	.comment = "DVB-T Alicante, Spain"
},
{
	.type = FE_OFDM,
	.name = "es-Alpicat",
	.muxes = muxlist_FE_OFDM_es_Alpicat,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Alpicat) / sizeof(struct mux),
	.comment = "DVB-T Alpicat (Lleida)"
},
{
	.type = FE_OFDM,
	.name = "es-Asturias",
	.muxes = muxlist_FE_OFDM_es_Asturias,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Asturias) / sizeof(struct mux),
	.comment = "DVB-T Asturias"
},
{
	.type = FE_OFDM,
	.name = "es-Bilbao",
	.muxes = muxlist_FE_OFDM_es_Bilbao,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Bilbao) / sizeof(struct mux),
	.comment = ""
},
{
	.type = FE_OFDM,
	.name = "es-Carceres",
	.muxes = muxlist_FE_OFDM_es_Carceres,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Carceres) / sizeof(struct mux),
	.comment = ""
},
{
	.type = FE_OFDM,
	.name = "es-Collserola",
	.muxes = muxlist_FE_OFDM_es_Collserola,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Collserola) / sizeof(struct mux),
	.comment = "DVB-T Collserola (Barcelona)"
},
{
	.type = FE_OFDM,
	.name = "es-Donostia",
	.muxes = muxlist_FE_OFDM_es_Donostia,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Donostia) / sizeof(struct mux),
	.comment = "The channels with 1/32 guard-interval are French and should be perfectly visible"
},
{
	.type = FE_OFDM,
	.name = "es-Las_Palmas",
	.muxes = muxlist_FE_OFDM_es_Las_Palmas,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Las_Palmas) / sizeof(struct mux),
	.comment = "Funciona correctamente en Las Palmas de Gran Canaria (24-4-2007)"
},
{
	.type = FE_OFDM,
	.name = "es-Lugo",
	.muxes = muxlist_FE_OFDM_es_Lugo,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Lugo) / sizeof(struct mux),
	.comment = "DVB-T Lugo (Centro emisor Paramo) - Rev. 1.2 - 11.12.05"
},
{
	.type = FE_OFDM,
	.name = "es-Madrid",
	.muxes = muxlist_FE_OFDM_es_Madrid,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Madrid) / sizeof(struct mux),
	.comment = ""
},
{
	.type = FE_OFDM,
	.name = "es-Malaga",
	.muxes = muxlist_FE_OFDM_es_Malaga,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Malaga) / sizeof(struct mux),
	.comment = "DVB-T Malaga (Andalucia)                   by Pedro Leon 4 Mayo 2007"
},
{
	.type = FE_OFDM,
	.name = "es-Mussara",
	.muxes = muxlist_FE_OFDM_es_Mussara,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Mussara) / sizeof(struct mux),
	.comment = "DVB-T La Mussara (Reus-Tarragona)"
},
{
	.type = FE_OFDM,
	.name = "es-Rocacorba",
	.muxes = muxlist_FE_OFDM_es_Rocacorba,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Rocacorba) / sizeof(struct mux),
	.comment = "DVB-T Rocacorba (Girona)"
},
{
	.type = FE_OFDM,
	.name = "es-Santander",
	.muxes = muxlist_FE_OFDM_es_Santander,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Santander) / sizeof(struct mux),
	.comment = "file automatically generated by w_scan"
},
{
	.type = FE_OFDM,
	.name = "es-Sevilla",
	.muxes = muxlist_FE_OFDM_es_Sevilla,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Sevilla) / sizeof(struct mux),
	.comment = "DVB-T Sevilla (Andalucia)                   by x2 15 Agosto 2006"
},
{
	.type = FE_OFDM,
	.name = "es-Valladolid",
	.muxes = muxlist_FE_OFDM_es_Valladolid,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Valladolid) / sizeof(struct mux),
	.comment = "DVB-T Valladolid"
},
{
	.type = FE_OFDM,
	.name = "es-Vilamarxant",
	.muxes = muxlist_FE_OFDM_es_Vilamarxant,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Vilamarxant) / sizeof(struct mux),
	.comment = "DVB-T Vilamarxant, Valencia, C. Valenciana, Spain."
},
{
	.type = FE_OFDM,
	.name = "es-Zaragoza",
	.muxes = muxlist_FE_OFDM_es_Zaragoza,
	.nmuxes = sizeof(muxlist_FE_OFDM_es_Zaragoza) / sizeof(struct mux),
	.comment = "DVB-T Zaragoza (Aragón) [Spain] [es-Zaragoza]"
},
{
	.type = FE_OFDM,
	.name = "fi-Aanekoski",
	.muxes = muxlist_FE_OFDM_fi_Aanekoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Aanekoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Aanekoski_Konginkangas",
	.muxes = muxlist_FE_OFDM_fi_Aanekoski_Konginkangas,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Aanekoski_Konginkangas) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ahtari",
	.muxes = muxlist_FE_OFDM_fi_Ahtari,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ahtari) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Alajarvi",
	.muxes = muxlist_FE_OFDM_fi_Alajarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Alajarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ala-Vuokki",
	.muxes = muxlist_FE_OFDM_fi_Ala_Vuokki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ala_Vuokki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ammansaari",
	.muxes = muxlist_FE_OFDM_fi_Ammansaari,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ammansaari) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Anjalankoski",
	.muxes = muxlist_FE_OFDM_fi_Anjalankoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Anjalankoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Enontekio_Ahovaara_Raattama",
	.muxes = muxlist_FE_OFDM_fi_Enontekio_Ahovaara_Raattama,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Enontekio_Ahovaara_Raattama) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Espoo",
	.muxes = muxlist_FE_OFDM_fi_Espoo,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Espoo) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Eurajoki",
	.muxes = muxlist_FE_OFDM_fi_Eurajoki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Eurajoki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Fiskars",
	.muxes = muxlist_FE_OFDM_fi_Fiskars,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Fiskars) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Haapavesi",
	.muxes = muxlist_FE_OFDM_fi_Haapavesi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Haapavesi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hameenkyro_Kyroskoski",
	.muxes = muxlist_FE_OFDM_fi_Hameenkyro_Kyroskoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hameenkyro_Kyroskoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hameenlinna_Painokangas",
	.muxes = muxlist_FE_OFDM_fi_Hameenlinna_Painokangas,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hameenlinna_Painokangas) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hanko",
	.muxes = muxlist_FE_OFDM_fi_Hanko,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hanko) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hartola",
	.muxes = muxlist_FE_OFDM_fi_Hartola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hartola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Heinavesi",
	.muxes = muxlist_FE_OFDM_fi_Heinavesi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Heinavesi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Heinola",
	.muxes = muxlist_FE_OFDM_fi_Heinola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Heinola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hetta",
	.muxes = muxlist_FE_OFDM_fi_Hetta,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hetta) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Houtskari",
	.muxes = muxlist_FE_OFDM_fi_Houtskari,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Houtskari) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hyrynsalmi",
	.muxes = muxlist_FE_OFDM_fi_Hyrynsalmi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hyrynsalmi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hyrynsalmi_Kyparavaara",
	.muxes = muxlist_FE_OFDM_fi_Hyrynsalmi_Kyparavaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hyrynsalmi_Kyparavaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hyrynsalmi_Paljakka",
	.muxes = muxlist_FE_OFDM_fi_Hyrynsalmi_Paljakka,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hyrynsalmi_Paljakka) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Hyvinkaa_Musta-Mannisto",
	.muxes = muxlist_FE_OFDM_fi_Hyvinkaa_Musta_Mannisto,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Hyvinkaa_Musta_Mannisto) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ii_Raiskio",
	.muxes = muxlist_FE_OFDM_fi_Ii_Raiskio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ii_Raiskio) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Iisalmi",
	.muxes = muxlist_FE_OFDM_fi_Iisalmi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Iisalmi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ikaalinen",
	.muxes = muxlist_FE_OFDM_fi_Ikaalinen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ikaalinen) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ikaalinen_Riitiala",
	.muxes = muxlist_FE_OFDM_fi_Ikaalinen_Riitiala,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ikaalinen_Riitiala) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Inari",
	.muxes = muxlist_FE_OFDM_fi_Inari,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Inari) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ivalo_Saarineitamovaara",
	.muxes = muxlist_FE_OFDM_fi_Ivalo_Saarineitamovaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ivalo_Saarineitamovaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jalasjarvi",
	.muxes = muxlist_FE_OFDM_fi_Jalasjarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jalasjarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jamsa_Kaipola",
	.muxes = muxlist_FE_OFDM_fi_Jamsa_Kaipola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jamsa_Kaipola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jamsa_Kuorevesi_Halli",
	.muxes = muxlist_FE_OFDM_fi_Jamsa_Kuorevesi_Halli,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jamsa_Kuorevesi_Halli) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jamsa_Matkosvuori",
	.muxes = muxlist_FE_OFDM_fi_Jamsa_Matkosvuori,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jamsa_Matkosvuori) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jamsankoski",
	.muxes = muxlist_FE_OFDM_fi_Jamsankoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jamsankoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jamsa_Ouninpohja",
	.muxes = muxlist_FE_OFDM_fi_Jamsa_Ouninpohja,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jamsa_Ouninpohja) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Joensuu_Vestinkallio",
	.muxes = muxlist_FE_OFDM_fi_Joensuu_Vestinkallio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Joensuu_Vestinkallio) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Joroinen_Puukkola",
	.muxes = muxlist_FE_OFDM_fi_Joroinen_Puukkola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Joroinen_Puukkola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Joutsa_Lankia",
	.muxes = muxlist_FE_OFDM_fi_Joutsa_Lankia,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Joutsa_Lankia) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Joutseno",
	.muxes = muxlist_FE_OFDM_fi_Joutseno,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Joutseno) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Juntusranta",
	.muxes = muxlist_FE_OFDM_fi_Juntusranta,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Juntusranta) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Juupajoki_Kopsamo",
	.muxes = muxlist_FE_OFDM_fi_Juupajoki_Kopsamo,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Juupajoki_Kopsamo) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jyvaskyla",
	.muxes = muxlist_FE_OFDM_fi_Jyvaskyla,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jyvaskyla) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Jyvaskylan_mlk_Vaajakoski",
	.muxes = muxlist_FE_OFDM_fi_Jyvaskylan_mlk_Vaajakoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Jyvaskylan_mlk_Vaajakoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kaavi_Sivakkavaara_Luikonlahti",
	.muxes = muxlist_FE_OFDM_fi_Kaavi_Sivakkavaara_Luikonlahti,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kaavi_Sivakkavaara_Luikonlahti) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kajaani_Pollyvaara",
	.muxes = muxlist_FE_OFDM_fi_Kajaani_Pollyvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kajaani_Pollyvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kalajoki",
	.muxes = muxlist_FE_OFDM_fi_Kalajoki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kalajoki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kangaslampi",
	.muxes = muxlist_FE_OFDM_fi_Kangaslampi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kangaslampi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kangasniemi_Turkinmaki",
	.muxes = muxlist_FE_OFDM_fi_Kangasniemi_Turkinmaki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kangasniemi_Turkinmaki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kankaanpaa",
	.muxes = muxlist_FE_OFDM_fi_Kankaanpaa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kankaanpaa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Karigasniemi",
	.muxes = muxlist_FE_OFDM_fi_Karigasniemi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Karigasniemi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Karkkila",
	.muxes = muxlist_FE_OFDM_fi_Karkkila,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Karkkila) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Karstula",
	.muxes = muxlist_FE_OFDM_fi_Karstula,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Karstula) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Karvia",
	.muxes = muxlist_FE_OFDM_fi_Karvia,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Karvia) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kaunispaa",
	.muxes = muxlist_FE_OFDM_fi_Kaunispaa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kaunispaa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kemijarvi_Suomutunturi",
	.muxes = muxlist_FE_OFDM_fi_Kemijarvi_Suomutunturi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kemijarvi_Suomutunturi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kerimaki",
	.muxes = muxlist_FE_OFDM_fi_Kerimaki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kerimaki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Keuruu",
	.muxes = muxlist_FE_OFDM_fi_Keuruu,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Keuruu) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Keuruu_Haapamaki",
	.muxes = muxlist_FE_OFDM_fi_Keuruu_Haapamaki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Keuruu_Haapamaki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kihnio",
	.muxes = muxlist_FE_OFDM_fi_Kihnio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kihnio) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kiihtelysvaara",
	.muxes = muxlist_FE_OFDM_fi_Kiihtelysvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kiihtelysvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kilpisjarvi",
	.muxes = muxlist_FE_OFDM_fi_Kilpisjarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kilpisjarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kittila_Sirkka_Levitunturi",
	.muxes = muxlist_FE_OFDM_fi_Kittila_Sirkka_Levitunturi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kittila_Sirkka_Levitunturi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kolari_Vuolittaja",
	.muxes = muxlist_FE_OFDM_fi_Kolari_Vuolittaja,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kolari_Vuolittaja) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Koli",
	.muxes = muxlist_FE_OFDM_fi_Koli,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Koli) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Korpilahti_Vaarunvuori",
	.muxes = muxlist_FE_OFDM_fi_Korpilahti_Vaarunvuori,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Korpilahti_Vaarunvuori) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Korppoo",
	.muxes = muxlist_FE_OFDM_fi_Korppoo,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Korppoo) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kruunupyy",
	.muxes = muxlist_FE_OFDM_fi_Kruunupyy,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kruunupyy) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kuhmo_Iivantiira",
	.muxes = muxlist_FE_OFDM_fi_Kuhmo_Iivantiira,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kuhmo_Iivantiira) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kuhmoinen",
	.muxes = muxlist_FE_OFDM_fi_Kuhmoinen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kuhmoinen) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kuhmoinen_Harjunsalmi",
	.muxes = muxlist_FE_OFDM_fi_Kuhmoinen_Harjunsalmi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kuhmoinen_Harjunsalmi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kuhmoinen_Puukkoinen",
	.muxes = muxlist_FE_OFDM_fi_Kuhmoinen_Puukkoinen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kuhmoinen_Puukkoinen) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kuhmo_Lentiira",
	.muxes = muxlist_FE_OFDM_fi_Kuhmo_Lentiira,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kuhmo_Lentiira) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kuopio",
	.muxes = muxlist_FE_OFDM_fi_Kuopio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kuopio) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kustavi_Viherlahti",
	.muxes = muxlist_FE_OFDM_fi_Kustavi_Viherlahti,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kustavi_Viherlahti) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kuttanen",
	.muxes = muxlist_FE_OFDM_fi_Kuttanen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kuttanen) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Kyyjarvi_Noposenaho",
	.muxes = muxlist_FE_OFDM_fi_Kyyjarvi_Noposenaho,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Kyyjarvi_Noposenaho) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Lahti",
	.muxes = muxlist_FE_OFDM_fi_Lahti,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Lahti) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Lapua",
	.muxes = muxlist_FE_OFDM_fi_Lapua,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Lapua) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Laukaa",
	.muxes = muxlist_FE_OFDM_fi_Laukaa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Laukaa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Laukaa_Vihtavuori",
	.muxes = muxlist_FE_OFDM_fi_Laukaa_Vihtavuori,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Laukaa_Vihtavuori) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Lavia_Lavianjarvi",
	.muxes = muxlist_FE_OFDM_fi_Lavia_Lavianjarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Lavia_Lavianjarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Lieksa_Vieki",
	.muxes = muxlist_FE_OFDM_fi_Lieksa_Vieki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Lieksa_Vieki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Lohja",
	.muxes = muxlist_FE_OFDM_fi_Lohja,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Lohja) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Loimaa",
	.muxes = muxlist_FE_OFDM_fi_Loimaa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Loimaa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Luhanka",
	.muxes = muxlist_FE_OFDM_fi_Luhanka,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Luhanka) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Luopioinen",
	.muxes = muxlist_FE_OFDM_fi_Luopioinen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Luopioinen) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Mantta",
	.muxes = muxlist_FE_OFDM_fi_Mantta,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Mantta) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Mantyharju",
	.muxes = muxlist_FE_OFDM_fi_Mantyharju,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Mantyharju) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Mikkeli",
	.muxes = muxlist_FE_OFDM_fi_Mikkeli,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Mikkeli) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Muonio_Olostunturi",
	.muxes = muxlist_FE_OFDM_fi_Muonio_Olostunturi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Muonio_Olostunturi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Nilsia",
	.muxes = muxlist_FE_OFDM_fi_Nilsia,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Nilsia) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Nilsia_Keski-Siikajarvi",
	.muxes = muxlist_FE_OFDM_fi_Nilsia_Keski_Siikajarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Nilsia_Keski_Siikajarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Nilsia_Pisa",
	.muxes = muxlist_FE_OFDM_fi_Nilsia_Pisa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Nilsia_Pisa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Nokia",
	.muxes = muxlist_FE_OFDM_fi_Nokia,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Nokia) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Nokia_Siuro_Linnavuori",
	.muxes = muxlist_FE_OFDM_fi_Nokia_Siuro_Linnavuori,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Nokia_Siuro_Linnavuori) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Nummi-Pusula_Hyonola",
	.muxes = muxlist_FE_OFDM_fi_Nummi_Pusula_Hyonola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Nummi_Pusula_Hyonola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Nurmes_Porokyla",
	.muxes = muxlist_FE_OFDM_fi_Nurmes_Porokyla,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Nurmes_Porokyla) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Orivesi_Langelmaki_Talviainen",
	.muxes = muxlist_FE_OFDM_fi_Orivesi_Langelmaki_Talviainen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Orivesi_Langelmaki_Talviainen) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Oulu",
	.muxes = muxlist_FE_OFDM_fi_Oulu,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Oulu) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Padasjoki",
	.muxes = muxlist_FE_OFDM_fi_Padasjoki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Padasjoki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Padasjoki_Arrakoski",
	.muxes = muxlist_FE_OFDM_fi_Padasjoki_Arrakoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Padasjoki_Arrakoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Paltamo_Kivesvaara",
	.muxes = muxlist_FE_OFDM_fi_Paltamo_Kivesvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Paltamo_Kivesvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Parikkala",
	.muxes = muxlist_FE_OFDM_fi_Parikkala,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Parikkala) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Parkano",
	.muxes = muxlist_FE_OFDM_fi_Parkano,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Parkano) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pello",
	.muxes = muxlist_FE_OFDM_fi_Pello,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pello) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pello_Ratasvaara",
	.muxes = muxlist_FE_OFDM_fi_Pello_Ratasvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pello_Ratasvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Perho",
	.muxes = muxlist_FE_OFDM_fi_Perho,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Perho) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pernaja",
	.muxes = muxlist_FE_OFDM_fi_Pernaja,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pernaja) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pieksamaki_Halkokumpu",
	.muxes = muxlist_FE_OFDM_fi_Pieksamaki_Halkokumpu,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pieksamaki_Halkokumpu) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pihtipudas",
	.muxes = muxlist_FE_OFDM_fi_Pihtipudas,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pihtipudas) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Porvoo_Suomenkyla",
	.muxes = muxlist_FE_OFDM_fi_Porvoo_Suomenkyla,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Porvoo_Suomenkyla) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Posio",
	.muxes = muxlist_FE_OFDM_fi_Posio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Posio) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pudasjarvi",
	.muxes = muxlist_FE_OFDM_fi_Pudasjarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pudasjarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pudasjarvi_Iso-Syote",
	.muxes = muxlist_FE_OFDM_fi_Pudasjarvi_Iso_Syote,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pudasjarvi_Iso_Syote) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pudasjarvi_Kangasvaara",
	.muxes = muxlist_FE_OFDM_fi_Pudasjarvi_Kangasvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pudasjarvi_Kangasvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Puolanka",
	.muxes = muxlist_FE_OFDM_fi_Puolanka,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Puolanka) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pyhatunturi",
	.muxes = muxlist_FE_OFDM_fi_Pyhatunturi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pyhatunturi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pyhavuori",
	.muxes = muxlist_FE_OFDM_fi_Pyhavuori,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pyhavuori) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Pylkonmaki_Karankajarvi",
	.muxes = muxlist_FE_OFDM_fi_Pylkonmaki_Karankajarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Pylkonmaki_Karankajarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Raahe_Mestauskallio",
	.muxes = muxlist_FE_OFDM_fi_Raahe_Mestauskallio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Raahe_Mestauskallio) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Raahe_Piehinki",
	.muxes = muxlist_FE_OFDM_fi_Raahe_Piehinki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Raahe_Piehinki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ranua_Haasionmaa",
	.muxes = muxlist_FE_OFDM_fi_Ranua_Haasionmaa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ranua_Haasionmaa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ranua_Leppiaho",
	.muxes = muxlist_FE_OFDM_fi_Ranua_Leppiaho,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ranua_Leppiaho) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rautavaara_Angervikko",
	.muxes = muxlist_FE_OFDM_fi_Rautavaara_Angervikko,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rautavaara_Angervikko) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rautjarvi_Simpele",
	.muxes = muxlist_FE_OFDM_fi_Rautjarvi_Simpele,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rautjarvi_Simpele) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ristijarvi",
	.muxes = muxlist_FE_OFDM_fi_Ristijarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ristijarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rovaniemi",
	.muxes = muxlist_FE_OFDM_fi_Rovaniemi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rovaniemi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rovaniemi_Ala-Nampa_Yli-Nampa_Rantalaki",
	.muxes = muxlist_FE_OFDM_fi_Rovaniemi_Ala_Nampa_Yli_Nampa_Rantalaki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rovaniemi_Ala_Nampa_Yli_Nampa_Rantalaki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rovaniemi_Kaihuanvaara",
	.muxes = muxlist_FE_OFDM_fi_Rovaniemi_Kaihuanvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rovaniemi_Kaihuanvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rovaniemi_Karhuvaara_Marrasjarvi",
	.muxes = muxlist_FE_OFDM_fi_Rovaniemi_Karhuvaara_Marrasjarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rovaniemi_Karhuvaara_Marrasjarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rovaniemi_Marasenkallio",
	.muxes = muxlist_FE_OFDM_fi_Rovaniemi_Marasenkallio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rovaniemi_Marasenkallio) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rovaniemi_Meltaus_Sorviselka",
	.muxes = muxlist_FE_OFDM_fi_Rovaniemi_Meltaus_Sorviselka,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rovaniemi_Meltaus_Sorviselka) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Rovaniemi_Sonka",
	.muxes = muxlist_FE_OFDM_fi_Rovaniemi_Sonka,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Rovaniemi_Sonka) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ruka",
	.muxes = muxlist_FE_OFDM_fi_Ruka,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ruka) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ruovesi_Storminiemi",
	.muxes = muxlist_FE_OFDM_fi_Ruovesi_Storminiemi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ruovesi_Storminiemi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Saarijarvi",
	.muxes = muxlist_FE_OFDM_fi_Saarijarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Saarijarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Saarijarvi_Kalmari",
	.muxes = muxlist_FE_OFDM_fi_Saarijarvi_Kalmari,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Saarijarvi_Kalmari) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Saarijarvi_Mahlu",
	.muxes = muxlist_FE_OFDM_fi_Saarijarvi_Mahlu,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Saarijarvi_Mahlu) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Salla_Hirvasvaara",
	.muxes = muxlist_FE_OFDM_fi_Salla_Hirvasvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Salla_Hirvasvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Salla_Ihistysjanka",
	.muxes = muxlist_FE_OFDM_fi_Salla_Ihistysjanka,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Salla_Ihistysjanka) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Salla_Naruska",
	.muxes = muxlist_FE_OFDM_fi_Salla_Naruska,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Salla_Naruska) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Salla_Saija",
	.muxes = muxlist_FE_OFDM_fi_Salla_Saija,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Salla_Saija) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Salla_Sallatunturi",
	.muxes = muxlist_FE_OFDM_fi_Salla_Sallatunturi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Salla_Sallatunturi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Salo_Isokyla",
	.muxes = muxlist_FE_OFDM_fi_Salo_Isokyla,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Salo_Isokyla) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Savukoski_Martti_Haarahonganmaa",
	.muxes = muxlist_FE_OFDM_fi_Savukoski_Martti_Haarahonganmaa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Savukoski_Martti_Haarahonganmaa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Savukoski_Tanhua",
	.muxes = muxlist_FE_OFDM_fi_Savukoski_Tanhua,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Savukoski_Tanhua) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Siilinjarvi",
	.muxes = muxlist_FE_OFDM_fi_Siilinjarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Siilinjarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Sipoo_Galthagen",
	.muxes = muxlist_FE_OFDM_fi_Sipoo_Galthagen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Sipoo_Galthagen) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Sodankyla_Pittiovaara",
	.muxes = muxlist_FE_OFDM_fi_Sodankyla_Pittiovaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Sodankyla_Pittiovaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Sulkava_Vaatalanmaki",
	.muxes = muxlist_FE_OFDM_fi_Sulkava_Vaatalanmaki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Sulkava_Vaatalanmaki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Sysma_Liikola",
	.muxes = muxlist_FE_OFDM_fi_Sysma_Liikola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Sysma_Liikola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Taivalkoski",
	.muxes = muxlist_FE_OFDM_fi_Taivalkoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Taivalkoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Taivalkoski_Taivalvaara",
	.muxes = muxlist_FE_OFDM_fi_Taivalkoski_Taivalvaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Taivalkoski_Taivalvaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Tammela",
	.muxes = muxlist_FE_OFDM_fi_Tammela,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Tammela) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Tammisaari",
	.muxes = muxlist_FE_OFDM_fi_Tammisaari,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Tammisaari) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Tampere",
	.muxes = muxlist_FE_OFDM_fi_Tampere,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Tampere) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Tampere_Pyynikki",
	.muxes = muxlist_FE_OFDM_fi_Tampere_Pyynikki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Tampere_Pyynikki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Tervola",
	.muxes = muxlist_FE_OFDM_fi_Tervola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Tervola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Turku",
	.muxes = muxlist_FE_OFDM_fi_Turku,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Turku) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Utsjoki",
	.muxes = muxlist_FE_OFDM_fi_Utsjoki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Utsjoki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Utsjoki_Nuorgam_Njallavaara",
	.muxes = muxlist_FE_OFDM_fi_Utsjoki_Nuorgam_Njallavaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Utsjoki_Nuorgam_Njallavaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Utsjoki_Nuorgam_raja",
	.muxes = muxlist_FE_OFDM_fi_Utsjoki_Nuorgam_raja,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Utsjoki_Nuorgam_raja) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Utsjoki_Outakoski",
	.muxes = muxlist_FE_OFDM_fi_Utsjoki_Outakoski,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Utsjoki_Outakoski) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Utsjoki_Polvarniemi",
	.muxes = muxlist_FE_OFDM_fi_Utsjoki_Polvarniemi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Utsjoki_Polvarniemi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Utsjoki_Rovisuvanto",
	.muxes = muxlist_FE_OFDM_fi_Utsjoki_Rovisuvanto,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Utsjoki_Rovisuvanto) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Utsjoki_Tenola",
	.muxes = muxlist_FE_OFDM_fi_Utsjoki_Tenola,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Utsjoki_Tenola) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Uusikaupunki_Orivo",
	.muxes = muxlist_FE_OFDM_fi_Uusikaupunki_Orivo,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Uusikaupunki_Orivo) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vaala",
	.muxes = muxlist_FE_OFDM_fi_Vaala,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vaala) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vaasa",
	.muxes = muxlist_FE_OFDM_fi_Vaasa,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vaasa) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Valtimo",
	.muxes = muxlist_FE_OFDM_fi_Valtimo,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Valtimo) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vammala_Jyranvuori",
	.muxes = muxlist_FE_OFDM_fi_Vammala_Jyranvuori,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vammala_Jyranvuori) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vammala_Roismala",
	.muxes = muxlist_FE_OFDM_fi_Vammala_Roismala,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vammala_Roismala) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vammala_Savi",
	.muxes = muxlist_FE_OFDM_fi_Vammala_Savi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vammala_Savi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vantaa_Hakunila",
	.muxes = muxlist_FE_OFDM_fi_Vantaa_Hakunila,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vantaa_Hakunila) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Varpaisjarvi_Honkamaki",
	.muxes = muxlist_FE_OFDM_fi_Varpaisjarvi_Honkamaki,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Varpaisjarvi_Honkamaki) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Virrat_Lappavuori",
	.muxes = muxlist_FE_OFDM_fi_Virrat_Lappavuori,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Virrat_Lappavuori) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vuokatti",
	.muxes = muxlist_FE_OFDM_fi_Vuokatti,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vuokatti) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Vuotso",
	.muxes = muxlist_FE_OFDM_fi_Vuotso,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Vuotso) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ylitornio_Ainiovaara",
	.muxes = muxlist_FE_OFDM_fi_Ylitornio_Ainiovaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ylitornio_Ainiovaara) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Ylitornio_Raanujarvi",
	.muxes = muxlist_FE_OFDM_fi_Ylitornio_Raanujarvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Ylitornio_Raanujarvi) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fi-Yllas",
	.muxes = muxlist_FE_OFDM_fi_Yllas,
	.nmuxes = sizeof(muxlist_FE_OFDM_fi_Yllas) / sizeof(struct mux),
	.comment = "automatically generated from http://www.digitv.fi/sivu.asp?path=1;8224;9519"
},
{
	.type = FE_OFDM,
	.name = "fr-Abbeville",
	.muxes = muxlist_FE_OFDM_fr_Abbeville,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Abbeville) / sizeof(struct mux),
	.comment = "Abbeville - France (DVB-T transmitter of Abbeville ( LaMotte ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Agen",
	.muxes = muxlist_FE_OFDM_fr_Agen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Agen) / sizeof(struct mux),
	.comment = "Agen - France (DVB-T transmitter of Agen ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Ajaccio",
	.muxes = muxlist_FE_OFDM_fr_Ajaccio,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Ajaccio) / sizeof(struct mux),
	.comment = "Ajaccio - France (DVB-T transmitter of Ajaccio ( Baied'Ajaccio ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Albi",
	.muxes = muxlist_FE_OFDM_fr_Albi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Albi) / sizeof(struct mux),
	.comment = "Albi - France (DVB-T transmitter of Albi ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Alençon",
	.muxes = muxlist_FE_OFDM_fr_Alen__on,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Alen__on) / sizeof(struct mux),
	.comment = "Alençon - France (DVB-T transmitter of Alençon ( Montsd'Amain ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Ales",
	.muxes = muxlist_FE_OFDM_fr_Ales,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Ales) / sizeof(struct mux),
	.comment = "Alès - France (DVB-T transmitter of Alès ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Ales-Bouquet",
	.muxes = muxlist_FE_OFDM_fr_Ales_Bouquet,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Ales_Bouquet) / sizeof(struct mux),
	.comment = "Alès - France (DVB-T transmitter of Alès ( MontBouquet ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Amiens",
	.muxes = muxlist_FE_OFDM_fr_Amiens,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Amiens) / sizeof(struct mux),
	.comment = "Amiens - France (DVB-T transmitter of Amiens ( LesSaintJust ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Angers",
	.muxes = muxlist_FE_OFDM_fr_Angers,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Angers) / sizeof(struct mux),
	.comment = "Angers - France (DVB-T transmitter of Angers ( RochefortsurLoire ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Annecy",
	.muxes = muxlist_FE_OFDM_fr_Annecy,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Annecy) / sizeof(struct mux),
	.comment = "Annecy - France (DVB-T transmitter of Annecy ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Arcachon",
	.muxes = muxlist_FE_OFDM_fr_Arcachon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Arcachon) / sizeof(struct mux),
	.comment = "Arcachon - France (DVB-T transmitter of Arcachon ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Argenton",
	.muxes = muxlist_FE_OFDM_fr_Argenton,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Argenton) / sizeof(struct mux),
	.comment = "Argenton - France (DVB-T transmitter of Argenton ( Malicornay ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Aubenas",
	.muxes = muxlist_FE_OFDM_fr_Aubenas,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Aubenas) / sizeof(struct mux),
	.comment = "Aubenas - France (DVB-T transmitter of Aubenas ( Nord ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Aurillac",
	.muxes = muxlist_FE_OFDM_fr_Aurillac,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Aurillac) / sizeof(struct mux),
	.comment = "Aurillac - France (DVB-T transmitter of Aurillac ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Autun",
	.muxes = muxlist_FE_OFDM_fr_Autun,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Autun) / sizeof(struct mux),
	.comment = "Autun - France (DVB-T transmitter of Autun ( BoisduRoi ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Auxerre",
	.muxes = muxlist_FE_OFDM_fr_Auxerre,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Auxerre) / sizeof(struct mux),
	.comment = "Auxerre - France (DVB-T transmitter of Auxerre ( Molesmes ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Avignon",
	.muxes = muxlist_FE_OFDM_fr_Avignon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Avignon) / sizeof(struct mux),
	.comment = "Avignon - France (DVB-T transmitter of Avignon ( MontVentoux ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-BarleDuc",
	.muxes = muxlist_FE_OFDM_fr_BarleDuc,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_BarleDuc) / sizeof(struct mux),
	.comment = "BarleDuc - France (DVB-T transmitter of BarleDuc ( Willeroncourt ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Bastia",
	.muxes = muxlist_FE_OFDM_fr_Bastia,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Bastia) / sizeof(struct mux),
	.comment = "Bastia - France (DVB-T transmitter of Bastia ( SerradiPigno ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Bayonne",
	.muxes = muxlist_FE_OFDM_fr_Bayonne,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Bayonne) / sizeof(struct mux),
	.comment = "Bayonne - France (DVB-T transmitter of Bayonne ( LaRhune ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Bergerac",
	.muxes = muxlist_FE_OFDM_fr_Bergerac,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Bergerac) / sizeof(struct mux),
	.comment = "Bergerac - France (DVB-T transmitter of Bergerac ( Audrix ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Besançon",
	.muxes = muxlist_FE_OFDM_fr_Besan__on,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Besan__on) / sizeof(struct mux),
	.comment = "Besançon - France (DVB-T transmitter of Besançon ( Brégille ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Bordeaux",
	.muxes = muxlist_FE_OFDM_fr_Bordeaux,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Bordeaux) / sizeof(struct mux),
	.comment = "Bordeaux - France (DVB-T transmitter of Bouliac or Cauderan)"
},
{
	.type = FE_OFDM,
	.name = "fr-Bordeaux-Bouliac",
	.muxes = muxlist_FE_OFDM_fr_Bordeaux_Bouliac,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Bordeaux_Bouliac) / sizeof(struct mux),
	.comment = "Bordeaux - France (DVB-T transmitter of Bordeaux ( BordeauxEst ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Bordeaux-Cauderan",
	.muxes = muxlist_FE_OFDM_fr_Bordeaux_Cauderan,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Bordeaux_Cauderan) / sizeof(struct mux),
	.comment = "Bordeaux - France (DVB-T transmitter of Bordeaux ( Caudéran ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Boulogne",
	.muxes = muxlist_FE_OFDM_fr_Boulogne,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Boulogne) / sizeof(struct mux),
	.comment = "Boulogne - France (DVB-T transmitter of Boulogne ( MontLambert ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Bourges",
	.muxes = muxlist_FE_OFDM_fr_Bourges,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Bourges) / sizeof(struct mux),
	.comment = "Bourges - France (DVB-T transmitter of Bourges ( CollinesduSancerrois ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Brest",
	.muxes = muxlist_FE_OFDM_fr_Brest,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Brest) / sizeof(struct mux),
	.comment = "Brest - France"
},
{
	.type = FE_OFDM,
	.name = "fr-Brive",
	.muxes = muxlist_FE_OFDM_fr_Brive,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Brive) / sizeof(struct mux),
	.comment = "Brive - France (DVB-T transmitter of Brive ( Lissac ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Caen",
	.muxes = muxlist_FE_OFDM_fr_Caen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Caen) / sizeof(struct mux),
	.comment = "Caen - France (DVB-T transmitter of Caen ( CaenNord ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Caen-Pincon",
	.muxes = muxlist_FE_OFDM_fr_Caen_Pincon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Caen_Pincon) / sizeof(struct mux),
	.comment = "Caen - France (DVB-T transmitter of Caen ( MontPinçon ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Cannes",
	.muxes = muxlist_FE_OFDM_fr_Cannes,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Cannes) / sizeof(struct mux),
	.comment = "Cannes - France (DVB-T transmitter of Cannes ( Vallauris ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Carcassonne",
	.muxes = muxlist_FE_OFDM_fr_Carcassonne,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Carcassonne) / sizeof(struct mux),
	.comment = "Carcassonne - France (DVB-T transmitter of Carcassonne ( MontagneNoire ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Chambery",
	.muxes = muxlist_FE_OFDM_fr_Chambery,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Chambery) / sizeof(struct mux),
	.comment = "Chambéry - France (DVB-T transmitter of Chambéry ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Chartres",
	.muxes = muxlist_FE_OFDM_fr_Chartres,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Chartres) / sizeof(struct mux),
	.comment = "Chartres - France (DVB-T transmitter of Chartres ( Montlandon ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Chennevieres",
	.muxes = muxlist_FE_OFDM_fr_Chennevieres,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Chennevieres) / sizeof(struct mux),
	.comment = "ParisEst - France (DVB-T transmitter of ParisEst ( Chennevières ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Cherbourg",
	.muxes = muxlist_FE_OFDM_fr_Cherbourg,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Cherbourg) / sizeof(struct mux),
	.comment = "Cherbourg - France (DVB-T transmitter of Cherbourg ( Digosville ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-ClermontFerrand",
	.muxes = muxlist_FE_OFDM_fr_ClermontFerrand,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_ClermontFerrand) / sizeof(struct mux),
	.comment = "Clermont-Ferrand - France (DVB-T transmitter of Clermont-Ferrand ( PuydeDôme ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Cluses",
	.muxes = muxlist_FE_OFDM_fr_Cluses,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Cluses) / sizeof(struct mux),
	.comment = "Cluses - France (DVB-T transmitter of Cluses ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Dieppe",
	.muxes = muxlist_FE_OFDM_fr_Dieppe,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Dieppe) / sizeof(struct mux),
	.comment = "Dieppe - France (DVB-T transmitter of Dieppe ( Neuville ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Dijon",
	.muxes = muxlist_FE_OFDM_fr_Dijon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Dijon) / sizeof(struct mux),
	.comment = "Dijon - France (DVB-T transmitter of Dijon ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Dunkerque",
	.muxes = muxlist_FE_OFDM_fr_Dunkerque,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Dunkerque) / sizeof(struct mux),
	.comment = "Dunkerque - France (DVB-T transmitter of Dunkerque ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Epinal",
	.muxes = muxlist_FE_OFDM_fr_Epinal,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Epinal) / sizeof(struct mux),
	.comment = "Epinal - France (DVB-T transmitter of Epinal ( BoisdelaVierge ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Evreux",
	.muxes = muxlist_FE_OFDM_fr_Evreux,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Evreux) / sizeof(struct mux),
	.comment = "Evreux - France (DVB-T transmitter of Evreux ( Netreville ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Forbach",
	.muxes = muxlist_FE_OFDM_fr_Forbach,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Forbach) / sizeof(struct mux),
	.comment = "Forbach - France (DVB-T transmitter of Forbach ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Gex",
	.muxes = muxlist_FE_OFDM_fr_Gex,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Gex) / sizeof(struct mux),
	.comment = "Gex - France (DVB-T transmitter of Gex ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Grenoble",
	.muxes = muxlist_FE_OFDM_fr_Grenoble,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Grenoble) / sizeof(struct mux),
	.comment = "Grenoble - France (DVB-T transmitter of Grenoble ( ToursansVenin ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Gueret",
	.muxes = muxlist_FE_OFDM_fr_Gueret,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Gueret) / sizeof(struct mux),
	.comment = "Guéret - France (DVB-T transmitter of Guéret ( StLégerleGueretois ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Hirson",
	.muxes = muxlist_FE_OFDM_fr_Hirson,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Hirson) / sizeof(struct mux),
	.comment = "Hirson - France (DVB-T transmitter of Hirson ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Hyeres",
	.muxes = muxlist_FE_OFDM_fr_Hyeres,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Hyeres) / sizeof(struct mux),
	.comment = "Hyères - France (DVB-T transmitter of Hyères ( CapBenat ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-LaRochelle",
	.muxes = muxlist_FE_OFDM_fr_LaRochelle,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_LaRochelle) / sizeof(struct mux),
	.comment = "Rochelle(La) - France (DVB-T transmitter of Rochelle(La) ( Mireuil ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Laval",
	.muxes = muxlist_FE_OFDM_fr_Laval,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Laval) / sizeof(struct mux),
	.comment = "Laval - France (DVB-T transmitter of Laval ( MontRochard ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-LeCreusot",
	.muxes = muxlist_FE_OFDM_fr_LeCreusot,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_LeCreusot) / sizeof(struct mux),
	.comment = "Creusot(Le) - France (DVB-T transmitter of Creusot(Le) ( MontStVincent ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-LeHavre",
	.muxes = muxlist_FE_OFDM_fr_LeHavre,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_LeHavre) / sizeof(struct mux),
	.comment = "Havre(Le) - France (DVB-T transmitter of Havre(Le) ( Harfleur ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-LeMans",
	.muxes = muxlist_FE_OFDM_fr_LeMans,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_LeMans) / sizeof(struct mux),
	.comment = "Le Mans - France (DVB-T transmitter of Mayet)"
},
{
	.type = FE_OFDM,
	.name = "fr-LePuyEnVelay",
	.muxes = muxlist_FE_OFDM_fr_LePuyEnVelay,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_LePuyEnVelay) / sizeof(struct mux),
	.comment = "PuyenVelay(Le) - France (DVB-T transmitter of PuyenVelay(Le) ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Lille",
	.muxes = muxlist_FE_OFDM_fr_Lille,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Lille) / sizeof(struct mux),
	.comment = "Lille - France (DVB-T transmitter of Lille ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Lille-Lambersart",
	.muxes = muxlist_FE_OFDM_fr_Lille_Lambersart,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Lille_Lambersart) / sizeof(struct mux),
	.comment = "Lille - France (DVB-T transmitter of Lille ( Lambersart ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-LilleT2",
	.muxes = muxlist_FE_OFDM_fr_LilleT2,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_LilleT2) / sizeof(struct mux),
	.comment = "Lille - France (DVB-T transmitter of Lambersart)"
},
{
	.type = FE_OFDM,
	.name = "fr-Limoges",
	.muxes = muxlist_FE_OFDM_fr_Limoges,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Limoges) / sizeof(struct mux),
	.comment = "Limoges - France (DVB-T transmitter of Limoges ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Longwy",
	.muxes = muxlist_FE_OFDM_fr_Longwy,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Longwy) / sizeof(struct mux),
	.comment = "Longwy - France (DVB-T transmitter of Longwy ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Lorient",
	.muxes = muxlist_FE_OFDM_fr_Lorient,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Lorient) / sizeof(struct mux),
	.comment = "Lorient - France (DVB-T transmitter of Lorient ( Ploemer ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Lyon-Fourviere",
	.muxes = muxlist_FE_OFDM_fr_Lyon_Fourviere,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Lyon_Fourviere) / sizeof(struct mux),
	.comment = "Lyon - France (DVB-T transmitter of Fourvière)"
},
{
	.type = FE_OFDM,
	.name = "fr-Lyon-Pilat",
	.muxes = muxlist_FE_OFDM_fr_Lyon_Pilat,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Lyon_Pilat) / sizeof(struct mux),
	.comment = "Lyon - France (DVB-T transmitter of Mt Pilat)"
},
{
	.type = FE_OFDM,
	.name = "fr-Macon",
	.muxes = muxlist_FE_OFDM_fr_Macon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Macon) / sizeof(struct mux),
	.comment = "Mâcon - France (DVB-T transmitter of Mâcon ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Mantes",
	.muxes = muxlist_FE_OFDM_fr_Mantes,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Mantes) / sizeof(struct mux),
	.comment = "Mantes - France (DVB-T transmitter of Mantes ( MaudétourenVexin ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Marseille",
	.muxes = muxlist_FE_OFDM_fr_Marseille,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Marseille) / sizeof(struct mux),
	.comment = ""
},
{
	.type = FE_OFDM,
	.name = "fr-Maubeuge",
	.muxes = muxlist_FE_OFDM_fr_Maubeuge,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Maubeuge) / sizeof(struct mux),
	.comment = "Maubeuge - France (DVB-T transmitter of Maubeuge ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Meaux",
	.muxes = muxlist_FE_OFDM_fr_Meaux,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Meaux) / sizeof(struct mux),
	.comment = "Meaux - France (DVB-T transmitter of Meaux ( Mareuil ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Mende",
	.muxes = muxlist_FE_OFDM_fr_Mende,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Mende) / sizeof(struct mux),
	.comment = "Mende - France (DVB-T transmitter of Mende ( TrucdeFortunio ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Menton",
	.muxes = muxlist_FE_OFDM_fr_Menton,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Menton) / sizeof(struct mux),
	.comment = "Menton - France (DVB-T transmitter of Menton ( CapMartin ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Metz",
	.muxes = muxlist_FE_OFDM_fr_Metz,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Metz) / sizeof(struct mux),
	.comment = "Metz - France (DVB-T transmitter of Metz ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Mezieres",
	.muxes = muxlist_FE_OFDM_fr_Mezieres,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Mezieres) / sizeof(struct mux),
	.comment = "Mézières - France (DVB-T transmitter of Mézières ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Montlucon",
	.muxes = muxlist_FE_OFDM_fr_Montlucon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Montlucon) / sizeof(struct mux),
	.comment = "Montluçon - France (DVB-T transmitter of Montluçon ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Montpellier",
	.muxes = muxlist_FE_OFDM_fr_Montpellier,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Montpellier) / sizeof(struct mux),
	.comment = "Montpellier - France (DVB-T transmitter of Montpellier ( SaintBaudille ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Mulhouse",
	.muxes = muxlist_FE_OFDM_fr_Mulhouse,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Mulhouse) / sizeof(struct mux),
	.comment = "Mulhouse - France (DVB-T transmitter of Mulhouse ( Belvédère ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Nancy",
	.muxes = muxlist_FE_OFDM_fr_Nancy,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Nancy) / sizeof(struct mux),
	.comment = "Nancy - France (DVB-T transmitter of Nancy ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Nantes",
	.muxes = muxlist_FE_OFDM_fr_Nantes,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Nantes) / sizeof(struct mux),
	.comment = "Nantes - France"
},
{
	.type = FE_OFDM,
	.name = "fr-NeufchatelEnBray",
	.muxes = muxlist_FE_OFDM_fr_NeufchatelEnBray,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_NeufchatelEnBray) / sizeof(struct mux),
	.comment = "Neufchatel-en-Bray - France (DVB-T transmitter of Neufchatel-en-Bray ( Croixdalle ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Nice",
	.muxes = muxlist_FE_OFDM_fr_Nice,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Nice) / sizeof(struct mux),
	.comment = "Nice - France (DVB-T transmitter of Nice ( MontAlban ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Niort",
	.muxes = muxlist_FE_OFDM_fr_Niort,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Niort) / sizeof(struct mux),
	.comment = "Niort - France (DVB-T transmitter of Niort-Maisonnay)"
},
{
	.type = FE_OFDM,
	.name = "fr-Orleans",
	.muxes = muxlist_FE_OFDM_fr_Orleans,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Orleans) / sizeof(struct mux),
	.comment = "Orléans / France"
},
{
	.type = FE_OFDM,
	.name = "fr-Paris",
	.muxes = muxlist_FE_OFDM_fr_Paris,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Paris) / sizeof(struct mux),
	.comment = "Paris - France - various DVB-T transmitters"
},
{
	.type = FE_OFDM,
	.name = "fr-Parthenay",
	.muxes = muxlist_FE_OFDM_fr_Parthenay,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Parthenay) / sizeof(struct mux),
	.comment = "Parthenay - France (DVB-T transmitter of Parthenay ( Amailloux ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Perpignan",
	.muxes = muxlist_FE_OFDM_fr_Perpignan,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Perpignan) / sizeof(struct mux),
	.comment = "Perpignan - France (DVB-T transmitter of Perpignan ( PicdeNeulos ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Poitiers",
	.muxes = muxlist_FE_OFDM_fr_Poitiers,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Poitiers) / sizeof(struct mux),
	.comment = "Poitiers - France (DVB-T transmitter of Poitiers ( Agglomération ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Privas",
	.muxes = muxlist_FE_OFDM_fr_Privas,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Privas) / sizeof(struct mux),
	.comment = "Privas - France (DVB-T transmitter of Privas ( Sud ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Reims",
	.muxes = muxlist_FE_OFDM_fr_Reims,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Reims) / sizeof(struct mux),
	.comment = "Reims - France (DVB-T transmitter of Reims ( Hautvillers ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Rennes",
	.muxes = muxlist_FE_OFDM_fr_Rennes,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Rennes) / sizeof(struct mux),
	.comment = "Rennes - France"
},
{
	.type = FE_OFDM,
	.name = "fr-Roanne",
	.muxes = muxlist_FE_OFDM_fr_Roanne,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Roanne) / sizeof(struct mux),
	.comment = "Roanne - France (DVB-T transmitter of Roanne ( Perreux ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Rouen",
	.muxes = muxlist_FE_OFDM_fr_Rouen,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Rouen) / sizeof(struct mux),
	.comment = "Rouen - France"
},
{
	.type = FE_OFDM,
	.name = "fr-SaintEtienne",
	.muxes = muxlist_FE_OFDM_fr_SaintEtienne,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_SaintEtienne) / sizeof(struct mux),
	.comment = "Saint-Etienne - France (DVB-T transmitter of Saint-Etienne ( CroixduGuisay ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-SaintRaphael",
	.muxes = muxlist_FE_OFDM_fr_SaintRaphael,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_SaintRaphael) / sizeof(struct mux),
	.comment = "Saint-Raphaël - France (DVB-T transmitter of Saint-Raphaël ( Picdel'Ours ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Sannois",
	.muxes = muxlist_FE_OFDM_fr_Sannois,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Sannois) / sizeof(struct mux),
	.comment = "ParisNord - France (DVB-T transmitter of ParisNord ( Sannois ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Sarrebourg",
	.muxes = muxlist_FE_OFDM_fr_Sarrebourg,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Sarrebourg) / sizeof(struct mux),
	.comment = "Sarrebourg - France (DVB-T transmitter of Sarrebourg ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Sens",
	.muxes = muxlist_FE_OFDM_fr_Sens,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Sens) / sizeof(struct mux),
	.comment = "Sens - France (DVB-T transmitter of Sens ( GisylesNobles ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Strasbourg",
	.muxes = muxlist_FE_OFDM_fr_Strasbourg,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Strasbourg) / sizeof(struct mux),
	.comment = "Strasbourg - France (DVB-T transmitter of Strasbourg ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Toulon",
	.muxes = muxlist_FE_OFDM_fr_Toulon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Toulon) / sizeof(struct mux),
	.comment = "Toulon - France (DVB-T transmitter of Toulon ( CapSicié ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Toulouse",
	.muxes = muxlist_FE_OFDM_fr_Toulouse,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Toulouse) / sizeof(struct mux),
	.comment = "Toulouse - France (DVB-T transmitter of Bohnoure)"
},
{
	.type = FE_OFDM,
	.name = "fr-Toulouse-Midi",
	.muxes = muxlist_FE_OFDM_fr_Toulouse_Midi,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Toulouse_Midi) / sizeof(struct mux),
	.comment = "Toulouse - France (DVB-T transmitter of Toulouse ( PicduMidi ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Tours",
	.muxes = muxlist_FE_OFDM_fr_Tours,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Tours) / sizeof(struct mux),
	.comment = "Tours - France (DVB-T transmitter of Tours ( Chissay ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Troyes",
	.muxes = muxlist_FE_OFDM_fr_Troyes,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Troyes) / sizeof(struct mux),
	.comment = "Troyes - France (DVB-T transmitter of Troyes ( LesRiceys ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Ussel",
	.muxes = muxlist_FE_OFDM_fr_Ussel,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Ussel) / sizeof(struct mux),
	.comment = "Ussel - France (DVB-T transmitter of Ussel ( Meymac ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Valence",
	.muxes = muxlist_FE_OFDM_fr_Valence,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Valence) / sizeof(struct mux),
	.comment = "Valence - France (DVB-T transmitter of Valence ( StRomaindeLerps ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Valenciennes",
	.muxes = muxlist_FE_OFDM_fr_Valenciennes,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Valenciennes) / sizeof(struct mux),
	.comment = "Valenciennes - France (DVB-T transmitter of Valenciennes ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Vannes",
	.muxes = muxlist_FE_OFDM_fr_Vannes,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Vannes) / sizeof(struct mux),
	.comment = "Vannes / France"
},
{
	.type = FE_OFDM,
	.name = "fr-Villebon",
	.muxes = muxlist_FE_OFDM_fr_Villebon,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Villebon) / sizeof(struct mux),
	.comment = "Paris - France (DVB-T transmitter of Villebon )"
},
{
	.type = FE_OFDM,
	.name = "fr-Vittel",
	.muxes = muxlist_FE_OFDM_fr_Vittel,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Vittel) / sizeof(struct mux),
	.comment = "Vittel - France (DVB-T transmitter of Vittel ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "fr-Voiron",
	.muxes = muxlist_FE_OFDM_fr_Voiron,
	.nmuxes = sizeof(muxlist_FE_OFDM_fr_Voiron) / sizeof(struct mux),
	.comment = "Voiron - France (DVB-T transmitter of Voiron ( Nondéfini ) )"
},
{
	.type = FE_OFDM,
	.name = "gr-Athens",
	.muxes = muxlist_FE_OFDM_gr_Athens,
	.nmuxes = sizeof(muxlist_FE_OFDM_gr_Athens) / sizeof(struct mux),
	.comment = "Initial scan config for Digital DVB-T (Ert) in Athens Greece"
},
{
	.type = FE_OFDM,
	.name = "hr-Zagreb",
	.muxes = muxlist_FE_OFDM_hr_Zagreb,
	.nmuxes = sizeof(muxlist_FE_OFDM_hr_Zagreb) / sizeof(struct mux),
	.comment = "DVB-T Hamburg"
},
{
	.type = FE_OFDM,
	.name = "is-Reykjavik",
	.muxes = muxlist_FE_OFDM_is_Reykjavik,
	.nmuxes = sizeof(muxlist_FE_OFDM_is_Reykjavik) / sizeof(struct mux),
	.comment = "Initial scan config for Digital Ísland in Iceland"
},
{
	.type = FE_OFDM,
	.name = "it-Aosta",
	.muxes = muxlist_FE_OFDM_it_Aosta,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Aosta) / sizeof(struct mux),
	.comment = "Italia / Aosta (it-Aosta) -- mailto: Marco <lovebuzz@email.it>"
},
{
	.type = FE_OFDM,
	.name = "it-Bari",
	.muxes = muxlist_FE_OFDM_it_Bari,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Bari) / sizeof(struct mux),
	.comment = "Italy, Bari"
},
{
	.type = FE_OFDM,
	.name = "it-Bologna",
	.muxes = muxlist_FE_OFDM_it_Bologna,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Bologna) / sizeof(struct mux),
	.comment = "DVB-T Collserola (Barcelona)"
},
{
	.type = FE_OFDM,
	.name = "it-Bolzano",
	.muxes = muxlist_FE_OFDM_it_Bolzano,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Bolzano) / sizeof(struct mux),
	.comment = "DVB-T Bolzano"
},
{
	.type = FE_OFDM,
	.name = "it-Cagliari",
	.muxes = muxlist_FE_OFDM_it_Cagliari,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Cagliari) / sizeof(struct mux),
	.comment = "DVB-T Cagliari"
},
{
	.type = FE_OFDM,
	.name = "it-Caivano",
	.muxes = muxlist_FE_OFDM_it_Caivano,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Caivano) / sizeof(struct mux),
	.comment = "###############################"
},
{
	.type = FE_OFDM,
	.name = "it-Catania",
	.muxes = muxlist_FE_OFDM_it_Catania,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Catania) / sizeof(struct mux),
	.comment = "it-Catania"
},
{
	.type = FE_OFDM,
	.name = "it-Conero",
	.muxes = muxlist_FE_OFDM_it_Conero,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Conero) / sizeof(struct mux),
	.comment = "Italia / Conero (it-Conero) -- mailto: simon <f.simon@email.it>"
},
{
	.type = FE_OFDM,
	.name = "it-Firenze",
	.muxes = muxlist_FE_OFDM_it_Firenze,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Firenze) / sizeof(struct mux),
	.comment = "This channel list is made by Michele Ficarra"
},
{
	.type = FE_OFDM,
	.name = "it-Genova",
	.muxes = muxlist_FE_OFDM_it_Genova,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Genova) / sizeof(struct mux),
	.comment = "Italia / Genova (it-Genova) - Angelo Conforti <angeloxx@angeloxx.it>"
},
{
	.type = FE_OFDM,
	.name = "it-Livorno",
	.muxes = muxlist_FE_OFDM_it_Livorno,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Livorno) / sizeof(struct mux),
	.comment = "This channel list is made by G.U.L.LI. LIvorno's Linux Users Group"
},
{
	.type = FE_OFDM,
	.name = "it-Milano",
	.muxes = muxlist_FE_OFDM_it_Milano,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Milano) / sizeof(struct mux),
	.comment = "MUX-A RAI"
},
{
	.type = FE_OFDM,
	.name = "it-Pagnacco",
	.muxes = muxlist_FE_OFDM_it_Pagnacco,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Pagnacco) / sizeof(struct mux),
	.comment = "Italia / Pagnacco (it-Pagnacco)"
},
{
	.type = FE_OFDM,
	.name = "it-Palermo",
	.muxes = muxlist_FE_OFDM_it_Palermo,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Palermo) / sizeof(struct mux),
	.comment = "Palermo, Italy"
},
{
	.type = FE_OFDM,
	.name = "it-Pisa",
	.muxes = muxlist_FE_OFDM_it_Pisa,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Pisa) / sizeof(struct mux),
	.comment = "This channel list is made by G.U.L.LI. LIvorno's Linux Users Group"
},
{
	.type = FE_OFDM,
	.name = "it-Roma",
	.muxes = muxlist_FE_OFDM_it_Roma,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Roma) / sizeof(struct mux),
	.comment = "DVB-T Roma"
},
{
	.type = FE_OFDM,
	.name = "it-Sassari",
	.muxes = muxlist_FE_OFDM_it_Sassari,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Sassari) / sizeof(struct mux),
	.comment = "DVB-T Sassari Channels List by frippertronics@alice.it ;)"
},
{
	.type = FE_OFDM,
	.name = "it-Torino",
	.muxes = muxlist_FE_OFDM_it_Torino,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Torino) / sizeof(struct mux),
	.comment = "DVB-T Torino (Italia)"
},
{
	.type = FE_OFDM,
	.name = "it-Trieste",
	.muxes = muxlist_FE_OFDM_it_Trieste,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Trieste) / sizeof(struct mux),
	.comment = "Trieste, Italy"
},
{
	.type = FE_OFDM,
	.name = "it-Varese",
	.muxes = muxlist_FE_OFDM_it_Varese,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Varese) / sizeof(struct mux),
	.comment = "Italia / Varese -- mailto: b.gabriele <gb.dvbch@dveprojects.com>"
},
{
	.type = FE_OFDM,
	.name = "it-Venezia",
	.muxes = muxlist_FE_OFDM_it_Venezia,
	.nmuxes = sizeof(muxlist_FE_OFDM_it_Venezia) / sizeof(struct mux),
	.comment = "Italia / Venzia (it-Venezia) -- mailto: Rob <rob.davis@libero.it>"
},
{
	.type = FE_OFDM,
	.name = "lu-All",
	.muxes = muxlist_FE_OFDM_lu_All,
	.nmuxes = sizeof(muxlist_FE_OFDM_lu_All) / sizeof(struct mux),
	.comment = "DVB-T Luxembourg [2007-11-18]"
},
{
	.type = FE_OFDM,
	.name = "lv-Riga",
	.muxes = muxlist_FE_OFDM_lv_Riga,
	.nmuxes = sizeof(muxlist_FE_OFDM_lv_Riga) / sizeof(struct mux),
	.comment = "Latvia - Riga (lv-Riga)"
},
{
	.type = FE_OFDM,
	.name = "nl-All",
	.muxes = muxlist_FE_OFDM_nl_All,
	.nmuxes = sizeof(muxlist_FE_OFDM_nl_All) / sizeof(struct mux),
	.comment = "The Netherlands, whole country"
},
{
	.type = FE_OFDM,
	.name = "nz-Waiatarua",
	.muxes = muxlist_FE_OFDM_nz_Waiatarua,
	.nmuxes = sizeof(muxlist_FE_OFDM_nz_Waiatarua) / sizeof(struct mux),
	.comment = "Waiatarua, Auckland NZ"
},
{
	.type = FE_OFDM,
	.name = "pl-Wroclaw",
	.muxes = muxlist_FE_OFDM_pl_Wroclaw,
	.nmuxes = sizeof(muxlist_FE_OFDM_pl_Wroclaw) / sizeof(struct mux),
	.comment = "Wroclaw / Zorawina, South-West Poland"
},
{
	.type = FE_OFDM,
	.name = "se-Alvdalen_Brunnsberg",
	.muxes = muxlist_FE_OFDM_se_Alvdalen_Brunnsberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Alvdalen_Brunnsberg) / sizeof(struct mux),
	.comment = "Sweden - Älvdalen/Brunnsberg"
},
{
	.type = FE_OFDM,
	.name = "se-Alvdalsasen",
	.muxes = muxlist_FE_OFDM_se_Alvdalsasen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Alvdalsasen) / sizeof(struct mux),
	.comment = "Sweden - Älvdalsåsen"
},
{
	.type = FE_OFDM,
	.name = "se-Alvsbyn",
	.muxes = muxlist_FE_OFDM_se_Alvsbyn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Alvsbyn) / sizeof(struct mux),
	.comment = "Sweden - Älvsbyn"
},
{
	.type = FE_OFDM,
	.name = "se-Amot",
	.muxes = muxlist_FE_OFDM_se_Amot,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Amot) / sizeof(struct mux),
	.comment = "Sweden - Åmot"
},
{
	.type = FE_OFDM,
	.name = "se-Angebo",
	.muxes = muxlist_FE_OFDM_se_Angebo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Angebo) / sizeof(struct mux),
	.comment = "Sweden - Ängebo"
},
{
	.type = FE_OFDM,
	.name = "se-Angelholm_Vegeholm",
	.muxes = muxlist_FE_OFDM_se_Angelholm_Vegeholm,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Angelholm_Vegeholm) / sizeof(struct mux),
	.comment = "Sweden - Ängelholm/Vegeholm"
},
{
	.type = FE_OFDM,
	.name = "se-Ange_Snoberg",
	.muxes = muxlist_FE_OFDM_se_Ange_Snoberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ange_Snoberg) / sizeof(struct mux),
	.comment = "Sweden - Ånge/Snöberg"
},
{
	.type = FE_OFDM,
	.name = "se-Arvidsjaur_Jultrask",
	.muxes = muxlist_FE_OFDM_se_Arvidsjaur_Jultrask,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Arvidsjaur_Jultrask) / sizeof(struct mux),
	.comment = "Sweden - Arvidsjaur/Julträsk"
},
{
	.type = FE_OFDM,
	.name = "se-Aspeboda",
	.muxes = muxlist_FE_OFDM_se_Aspeboda,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Aspeboda) / sizeof(struct mux),
	.comment = "Sweden - Aspeboda"
},
{
	.type = FE_OFDM,
	.name = "se-Atvidaberg",
	.muxes = muxlist_FE_OFDM_se_Atvidaberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Atvidaberg) / sizeof(struct mux),
	.comment = "Sweden - Åtvidaberg"
},
{
	.type = FE_OFDM,
	.name = "se-Avesta_Krylbo",
	.muxes = muxlist_FE_OFDM_se_Avesta_Krylbo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Avesta_Krylbo) / sizeof(struct mux),
	.comment = "Sweden - Avesta/Krylbo"
},
{
	.type = FE_OFDM,
	.name = "se-Backefors",
	.muxes = muxlist_FE_OFDM_se_Backefors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Backefors) / sizeof(struct mux),
	.comment = "Sweden - Bäckefors"
},
{
	.type = FE_OFDM,
	.name = "se-Bankeryd",
	.muxes = muxlist_FE_OFDM_se_Bankeryd,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bankeryd) / sizeof(struct mux),
	.comment = "Sweden - Bankeryd"
},
{
	.type = FE_OFDM,
	.name = "se-Bergsjo_Balleberget",
	.muxes = muxlist_FE_OFDM_se_Bergsjo_Balleberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bergsjo_Balleberget) / sizeof(struct mux),
	.comment = "Sweden - Bergsjö/Bålleberget"
},
{
	.type = FE_OFDM,
	.name = "se-Bergvik",
	.muxes = muxlist_FE_OFDM_se_Bergvik,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bergvik) / sizeof(struct mux),
	.comment = "Sweden - Bergvik"
},
{
	.type = FE_OFDM,
	.name = "se-Bollebygd",
	.muxes = muxlist_FE_OFDM_se_Bollebygd,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bollebygd) / sizeof(struct mux),
	.comment = "Sweden - Bollebygd"
},
{
	.type = FE_OFDM,
	.name = "se-Bollnas",
	.muxes = muxlist_FE_OFDM_se_Bollnas,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bollnas) / sizeof(struct mux),
	.comment = "Sweden - Bollnäs"
},
{
	.type = FE_OFDM,
	.name = "se-Boras_Dalsjofors",
	.muxes = muxlist_FE_OFDM_se_Boras_Dalsjofors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Boras_Dalsjofors) / sizeof(struct mux),
	.comment = "Sweden - Borås/Dalsjöfors"
},
{
	.type = FE_OFDM,
	.name = "se-Boras_Sjobo",
	.muxes = muxlist_FE_OFDM_se_Boras_Sjobo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Boras_Sjobo) / sizeof(struct mux),
	.comment = "Sweden - Borås/Sjöbo"
},
{
	.type = FE_OFDM,
	.name = "se-Borlange_Idkerberget",
	.muxes = muxlist_FE_OFDM_se_Borlange_Idkerberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Borlange_Idkerberget) / sizeof(struct mux),
	.comment = "Sweden - Borlänge/Idkerberget"
},
{
	.type = FE_OFDM,
	.name = "se-Borlange_Nygardarna",
	.muxes = muxlist_FE_OFDM_se_Borlange_Nygardarna,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Borlange_Nygardarna) / sizeof(struct mux),
	.comment = "Sweden - Borlänge/Nygårdarna"
},
{
	.type = FE_OFDM,
	.name = "se-Bottnaryd_Ryd",
	.muxes = muxlist_FE_OFDM_se_Bottnaryd_Ryd,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bottnaryd_Ryd) / sizeof(struct mux),
	.comment = "Sweden - Bottnaryd/Ryd"
},
{
	.type = FE_OFDM,
	.name = "se-Bromsebro",
	.muxes = muxlist_FE_OFDM_se_Bromsebro,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bromsebro) / sizeof(struct mux),
	.comment = "Sweden - Brömsebro"
},
{
	.type = FE_OFDM,
	.name = "se-Bruzaholm",
	.muxes = muxlist_FE_OFDM_se_Bruzaholm,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Bruzaholm) / sizeof(struct mux),
	.comment = "Sweden - Bruzaholm"
},
{
	.type = FE_OFDM,
	.name = "se-Byxelkrok",
	.muxes = muxlist_FE_OFDM_se_Byxelkrok,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Byxelkrok) / sizeof(struct mux),
	.comment = "Sweden - Byxelkrok"
},
{
	.type = FE_OFDM,
	.name = "se-Dadran",
	.muxes = muxlist_FE_OFDM_se_Dadran,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Dadran) / sizeof(struct mux),
	.comment = "Sweden - Dådran"
},
{
	.type = FE_OFDM,
	.name = "se-Dalfors",
	.muxes = muxlist_FE_OFDM_se_Dalfors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Dalfors) / sizeof(struct mux),
	.comment = "Sweden - Dalfors"
},
{
	.type = FE_OFDM,
	.name = "se-Dalstuga",
	.muxes = muxlist_FE_OFDM_se_Dalstuga,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Dalstuga) / sizeof(struct mux),
	.comment = "Sweden - Dalstuga"
},
{
	.type = FE_OFDM,
	.name = "se-Degerfors",
	.muxes = muxlist_FE_OFDM_se_Degerfors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Degerfors) / sizeof(struct mux),
	.comment = "Sweden - Degerfors"
},
{
	.type = FE_OFDM,
	.name = "se-Delary",
	.muxes = muxlist_FE_OFDM_se_Delary,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Delary) / sizeof(struct mux),
	.comment = "Sweden - Delary"
},
{
	.type = FE_OFDM,
	.name = "se-Djura",
	.muxes = muxlist_FE_OFDM_se_Djura,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Djura) / sizeof(struct mux),
	.comment = "Sweden - Djura"
},
{
	.type = FE_OFDM,
	.name = "se-Drevdagen",
	.muxes = muxlist_FE_OFDM_se_Drevdagen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Drevdagen) / sizeof(struct mux),
	.comment = "Sweden - Drevdagen"
},
{
	.type = FE_OFDM,
	.name = "se-Duvnas",
	.muxes = muxlist_FE_OFDM_se_Duvnas,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Duvnas) / sizeof(struct mux),
	.comment = "Sweden - Duvnäs"
},
{
	.type = FE_OFDM,
	.name = "se-Duvnas_Basna",
	.muxes = muxlist_FE_OFDM_se_Duvnas_Basna,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Duvnas_Basna) / sizeof(struct mux),
	.comment = "Sweden - Duvnäs/Bäsna"
},
{
	.type = FE_OFDM,
	.name = "se-Edsbyn",
	.muxes = muxlist_FE_OFDM_se_Edsbyn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Edsbyn) / sizeof(struct mux),
	.comment = "Sweden - Edsbyn"
},
{
	.type = FE_OFDM,
	.name = "se-Emmaboda_Balshult",
	.muxes = muxlist_FE_OFDM_se_Emmaboda_Balshult,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Emmaboda_Balshult) / sizeof(struct mux),
	.comment = "Sweden - Emmaboda/Bälshult"
},
{
	.type = FE_OFDM,
	.name = "se-Enviken",
	.muxes = muxlist_FE_OFDM_se_Enviken,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Enviken) / sizeof(struct mux),
	.comment = "Sweden - Enviken"
},
{
	.type = FE_OFDM,
	.name = "se-Fagersta",
	.muxes = muxlist_FE_OFDM_se_Fagersta,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Fagersta) / sizeof(struct mux),
	.comment = "Sweden - Fagersta"
},
{
	.type = FE_OFDM,
	.name = "se-Falerum_Centrum",
	.muxes = muxlist_FE_OFDM_se_Falerum_Centrum,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Falerum_Centrum) / sizeof(struct mux),
	.comment = "Sweden - Falerum/Centrum"
},
{
	.type = FE_OFDM,
	.name = "se-Falun_Lovberget",
	.muxes = muxlist_FE_OFDM_se_Falun_Lovberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Falun_Lovberget) / sizeof(struct mux),
	.comment = "Sweden - Falun/Lövberget"
},
{
	.type = FE_OFDM,
	.name = "se-Farila",
	.muxes = muxlist_FE_OFDM_se_Farila,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Farila) / sizeof(struct mux),
	.comment = "Sweden - Färila"
},
{
	.type = FE_OFDM,
	.name = "se-Faro_Ajkerstrask",
	.muxes = muxlist_FE_OFDM_se_Faro_Ajkerstrask,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Faro_Ajkerstrask) / sizeof(struct mux),
	.comment = "Sweden - Fårö/Ajkersträsk"
},
{
	.type = FE_OFDM,
	.name = "se-Farosund_Bunge",
	.muxes = muxlist_FE_OFDM_se_Farosund_Bunge,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Farosund_Bunge) / sizeof(struct mux),
	.comment = "Sweden - Fårösund/Bunge"
},
{
	.type = FE_OFDM,
	.name = "se-Filipstad_Klockarhojden",
	.muxes = muxlist_FE_OFDM_se_Filipstad_Klockarhojden,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Filipstad_Klockarhojden) / sizeof(struct mux),
	.comment = "Sweden - Filipstad/Klockarhöjden"
},
{
	.type = FE_OFDM,
	.name = "se-Finnveden",
	.muxes = muxlist_FE_OFDM_se_Finnveden,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Finnveden) / sizeof(struct mux),
	.comment = "Sweden - Finnveden"
},
{
	.type = FE_OFDM,
	.name = "se-Fredriksberg",
	.muxes = muxlist_FE_OFDM_se_Fredriksberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Fredriksberg) / sizeof(struct mux),
	.comment = "Sweden - Fredriksberg"
},
{
	.type = FE_OFDM,
	.name = "se-Fritsla",
	.muxes = muxlist_FE_OFDM_se_Fritsla,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Fritsla) / sizeof(struct mux),
	.comment = "Sweden - Fritsla"
},
{
	.type = FE_OFDM,
	.name = "se-Furudal",
	.muxes = muxlist_FE_OFDM_se_Furudal,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Furudal) / sizeof(struct mux),
	.comment = "Sweden - Furudal"
},
{
	.type = FE_OFDM,
	.name = "se-Gallivare",
	.muxes = muxlist_FE_OFDM_se_Gallivare,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gallivare) / sizeof(struct mux),
	.comment = "Sweden - Gällivare"
},
{
	.type = FE_OFDM,
	.name = "se-Garpenberg_Kuppgarden",
	.muxes = muxlist_FE_OFDM_se_Garpenberg_Kuppgarden,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Garpenberg_Kuppgarden) / sizeof(struct mux),
	.comment = "Sweden - Garpenberg/Kuppgården"
},
{
	.type = FE_OFDM,
	.name = "se-Gavle_Skogmur",
	.muxes = muxlist_FE_OFDM_se_Gavle_Skogmur,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gavle_Skogmur) / sizeof(struct mux),
	.comment = "Sweden - Gävle/Skogmur"
},
{
	.type = FE_OFDM,
	.name = "se-Gnarp",
	.muxes = muxlist_FE_OFDM_se_Gnarp,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gnarp) / sizeof(struct mux),
	.comment = "Sweden - Gnarp"
},
{
	.type = FE_OFDM,
	.name = "se-Gnesta",
	.muxes = muxlist_FE_OFDM_se_Gnesta,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gnesta) / sizeof(struct mux),
	.comment = "Sweden - Gnesta"
},
{
	.type = FE_OFDM,
	.name = "se-Gnosjo_Marieholm",
	.muxes = muxlist_FE_OFDM_se_Gnosjo_Marieholm,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gnosjo_Marieholm) / sizeof(struct mux),
	.comment = "Sweden - Gnosjö/Marieholm"
},
{
	.type = FE_OFDM,
	.name = "se-Goteborg_Brudaremossen",
	.muxes = muxlist_FE_OFDM_se_Goteborg_Brudaremossen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Goteborg_Brudaremossen) / sizeof(struct mux),
	.comment = "Sweden - Göteborg/Brudaremossen"
},
{
	.type = FE_OFDM,
	.name = "se-Goteborg_Slattadamm",
	.muxes = muxlist_FE_OFDM_se_Goteborg_Slattadamm,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Goteborg_Slattadamm) / sizeof(struct mux),
	.comment = "Sweden - Göteborg/Slättadamm"
},
{
	.type = FE_OFDM,
	.name = "se-Gullbrandstorp",
	.muxes = muxlist_FE_OFDM_se_Gullbrandstorp,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gullbrandstorp) / sizeof(struct mux),
	.comment = "Sweden - Gullbrandstorp"
},
{
	.type = FE_OFDM,
	.name = "se-Gunnarsbo",
	.muxes = muxlist_FE_OFDM_se_Gunnarsbo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gunnarsbo) / sizeof(struct mux),
	.comment = "Sweden - Gunnarsbo"
},
{
	.type = FE_OFDM,
	.name = "se-Gusum",
	.muxes = muxlist_FE_OFDM_se_Gusum,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Gusum) / sizeof(struct mux),
	.comment = "Sweden - Gusum"
},
{
	.type = FE_OFDM,
	.name = "se-Hagfors_Varmullsasen",
	.muxes = muxlist_FE_OFDM_se_Hagfors_Varmullsasen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hagfors_Varmullsasen) / sizeof(struct mux),
	.comment = "Sweden - Hagfors/Värmullsåsen"
},
{
	.type = FE_OFDM,
	.name = "se-Hallaryd",
	.muxes = muxlist_FE_OFDM_se_Hallaryd,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hallaryd) / sizeof(struct mux),
	.comment = "Sweden - Hallaryd"
},
{
	.type = FE_OFDM,
	.name = "se-Hallbo",
	.muxes = muxlist_FE_OFDM_se_Hallbo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hallbo) / sizeof(struct mux),
	.comment = "Sweden - Hällbo"
},
{
	.type = FE_OFDM,
	.name = "se-Halmstad_Hamnen",
	.muxes = muxlist_FE_OFDM_se_Halmstad_Hamnen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Halmstad_Hamnen) / sizeof(struct mux),
	.comment = "Sweden - Halmstad/Hamnen"
},
{
	.type = FE_OFDM,
	.name = "se-Halmstad_Oskarstrom",
	.muxes = muxlist_FE_OFDM_se_Halmstad_Oskarstrom,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Halmstad_Oskarstrom) / sizeof(struct mux),
	.comment = "Sweden - Halmstad/Oskarström"
},
{
	.type = FE_OFDM,
	.name = "se-Harnosand_Harnon",
	.muxes = muxlist_FE_OFDM_se_Harnosand_Harnon,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Harnosand_Harnon) / sizeof(struct mux),
	.comment = "Sweden - Härnösand/Härnön"
},
{
	.type = FE_OFDM,
	.name = "se-Hassela",
	.muxes = muxlist_FE_OFDM_se_Hassela,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hassela) / sizeof(struct mux),
	.comment = "Sweden - Hassela"
},
{
	.type = FE_OFDM,
	.name = "se-Havdhem",
	.muxes = muxlist_FE_OFDM_se_Havdhem,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Havdhem) / sizeof(struct mux),
	.comment = "Sweden - Havdhem"
},
{
	.type = FE_OFDM,
	.name = "se-Hedemora",
	.muxes = muxlist_FE_OFDM_se_Hedemora,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hedemora) / sizeof(struct mux),
	.comment = "Sweden - Hedemora"
},
{
	.type = FE_OFDM,
	.name = "se-Helsingborg_Olympia",
	.muxes = muxlist_FE_OFDM_se_Helsingborg_Olympia,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Helsingborg_Olympia) / sizeof(struct mux),
	.comment = "Sweden - Helsingborg/Olympia"
},
{
	.type = FE_OFDM,
	.name = "se-Hennan",
	.muxes = muxlist_FE_OFDM_se_Hennan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hennan) / sizeof(struct mux),
	.comment = "Sweden - Hennan"
},
{
	.type = FE_OFDM,
	.name = "se-Hestra_Aspas",
	.muxes = muxlist_FE_OFDM_se_Hestra_Aspas,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hestra_Aspas) / sizeof(struct mux),
	.comment = "Sweden - Hestra/Äspås"
},
{
	.type = FE_OFDM,
	.name = "se-Hjo_Grevback",
	.muxes = muxlist_FE_OFDM_se_Hjo_Grevback,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hjo_Grevback) / sizeof(struct mux),
	.comment = "Sweden - Hjo/Grevbäck"
},
{
	.type = FE_OFDM,
	.name = "se-Hofors",
	.muxes = muxlist_FE_OFDM_se_Hofors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hofors) / sizeof(struct mux),
	.comment = "Sweden - Hofors"
},
{
	.type = FE_OFDM,
	.name = "se-Hogfors",
	.muxes = muxlist_FE_OFDM_se_Hogfors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hogfors) / sizeof(struct mux),
	.comment = "Sweden - Högfors"
},
{
	.type = FE_OFDM,
	.name = "se-Hogsby_Virstad",
	.muxes = muxlist_FE_OFDM_se_Hogsby_Virstad,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hogsby_Virstad) / sizeof(struct mux),
	.comment = "Sweden - Högsby/Virstad"
},
{
	.type = FE_OFDM,
	.name = "se-Holsbybrunn_Holsbyholm",
	.muxes = muxlist_FE_OFDM_se_Holsbybrunn_Holsbyholm,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Holsbybrunn_Holsbyholm) / sizeof(struct mux),
	.comment = "Sweden - Holsbybrunn/Holsbyholm"
},
{
	.type = FE_OFDM,
	.name = "se-Horby_Sallerup",
	.muxes = muxlist_FE_OFDM_se_Horby_Sallerup,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Horby_Sallerup) / sizeof(struct mux),
	.comment = "Sweden - Hörby/Sallerup"
},
{
	.type = FE_OFDM,
	.name = "se-Horken",
	.muxes = muxlist_FE_OFDM_se_Horken,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Horken) / sizeof(struct mux),
	.comment = "Sweden - Hörken"
},
{
	.type = FE_OFDM,
	.name = "se-Hudiksvall_Forsa",
	.muxes = muxlist_FE_OFDM_se_Hudiksvall_Forsa,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hudiksvall_Forsa) / sizeof(struct mux),
	.comment = "Sweden - Hudiksvall/Forsa"
},
{
	.type = FE_OFDM,
	.name = "se-Hudiksvall_Galgberget",
	.muxes = muxlist_FE_OFDM_se_Hudiksvall_Galgberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Hudiksvall_Galgberget) / sizeof(struct mux),
	.comment = "Sweden - Hudiksvall/Galgberget"
},
{
	.type = FE_OFDM,
	.name = "se-Huskvarna",
	.muxes = muxlist_FE_OFDM_se_Huskvarna,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Huskvarna) / sizeof(struct mux),
	.comment = "Sweden - Huskvarna"
},
{
	.type = FE_OFDM,
	.name = "se-Idre",
	.muxes = muxlist_FE_OFDM_se_Idre,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Idre) / sizeof(struct mux),
	.comment = "Sweden - Idre"
},
{
	.type = FE_OFDM,
	.name = "se-Ingatorp",
	.muxes = muxlist_FE_OFDM_se_Ingatorp,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ingatorp) / sizeof(struct mux),
	.comment = "Sweden - Ingatorp"
},
{
	.type = FE_OFDM,
	.name = "se-Ingvallsbenning",
	.muxes = muxlist_FE_OFDM_se_Ingvallsbenning,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ingvallsbenning) / sizeof(struct mux),
	.comment = "Sweden - Ingvallsbenning"
},
{
	.type = FE_OFDM,
	.name = "se-Irevik",
	.muxes = muxlist_FE_OFDM_se_Irevik,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Irevik) / sizeof(struct mux),
	.comment = "Sweden - Irevik"
},
{
	.type = FE_OFDM,
	.name = "se-Jamjo",
	.muxes = muxlist_FE_OFDM_se_Jamjo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Jamjo) / sizeof(struct mux),
	.comment = "Sweden - Jämj"
},
{
	.type = FE_OFDM,
	.name = "se-Jarnforsen",
	.muxes = muxlist_FE_OFDM_se_Jarnforsen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Jarnforsen) / sizeof(struct mux),
	.comment = "Sweden - Järnforsen"
},
{
	.type = FE_OFDM,
	.name = "se-Jarvso",
	.muxes = muxlist_FE_OFDM_se_Jarvso,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Jarvso) / sizeof(struct mux),
	.comment = "Sweden - Järvs"
},
{
	.type = FE_OFDM,
	.name = "se-Jokkmokk_Tjalmejaure",
	.muxes = muxlist_FE_OFDM_se_Jokkmokk_Tjalmejaure,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Jokkmokk_Tjalmejaure) / sizeof(struct mux),
	.comment = "Sweden - Jokkmokk/Tjalmejaure"
},
{
	.type = FE_OFDM,
	.name = "se-Jonkoping_Bondberget",
	.muxes = muxlist_FE_OFDM_se_Jonkoping_Bondberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Jonkoping_Bondberget) / sizeof(struct mux),
	.comment = "Sweden - Jönköping/Bondberget"
},
{
	.type = FE_OFDM,
	.name = "se-Kalix",
	.muxes = muxlist_FE_OFDM_se_Kalix,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kalix) / sizeof(struct mux),
	.comment = "Sweden - Kalix"
},
{
	.type = FE_OFDM,
	.name = "se-Karbole",
	.muxes = muxlist_FE_OFDM_se_Karbole,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Karbole) / sizeof(struct mux),
	.comment = "Sweden - Kårböle"
},
{
	.type = FE_OFDM,
	.name = "se-Karlsborg_Vaberget",
	.muxes = muxlist_FE_OFDM_se_Karlsborg_Vaberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Karlsborg_Vaberget) / sizeof(struct mux),
	.comment = "Sweden - Karlsborg/Vaberget"
},
{
	.type = FE_OFDM,
	.name = "se-Karlshamn",
	.muxes = muxlist_FE_OFDM_se_Karlshamn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Karlshamn) / sizeof(struct mux),
	.comment = "Sweden - Karlshamn"
},
{
	.type = FE_OFDM,
	.name = "se-Karlskrona_Vamo",
	.muxes = muxlist_FE_OFDM_se_Karlskrona_Vamo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Karlskrona_Vamo) / sizeof(struct mux),
	.comment = "Sweden - Karlskrona/Väm"
},
{
	.type = FE_OFDM,
	.name = "se-Karlstad_Sormon",
	.muxes = muxlist_FE_OFDM_se_Karlstad_Sormon,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Karlstad_Sormon) / sizeof(struct mux),
	.comment = "Sweden - Karlstad Sörmon Valid from 2007 09 26. Ver. 2 Correct FEC"
},
{
	.type = FE_OFDM,
	.name = "se-Kaxholmen_Vistakulle",
	.muxes = muxlist_FE_OFDM_se_Kaxholmen_Vistakulle,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kaxholmen_Vistakulle) / sizeof(struct mux),
	.comment = "Sweden - Kaxholmen/Vistakulle"
},
{
	.type = FE_OFDM,
	.name = "se-Kinnastrom",
	.muxes = muxlist_FE_OFDM_se_Kinnastrom,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kinnastrom) / sizeof(struct mux),
	.comment = "Sweden - Kinnaström"
},
{
	.type = FE_OFDM,
	.name = "se-Kiruna_Kirunavaara",
	.muxes = muxlist_FE_OFDM_se_Kiruna_Kirunavaara,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kiruna_Kirunavaara) / sizeof(struct mux),
	.comment = "Sweden - Kiruna/Kirunavaara"
},
{
	.type = FE_OFDM,
	.name = "se-Kisa",
	.muxes = muxlist_FE_OFDM_se_Kisa,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kisa) / sizeof(struct mux),
	.comment = "Sweden - Kisa"
},
{
	.type = FE_OFDM,
	.name = "se-Knared",
	.muxes = muxlist_FE_OFDM_se_Knared,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Knared) / sizeof(struct mux),
	.comment = "Sweden - Knäred"
},
{
	.type = FE_OFDM,
	.name = "se-Kopmanholmen",
	.muxes = muxlist_FE_OFDM_se_Kopmanholmen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kopmanholmen) / sizeof(struct mux),
	.comment = "Sweden - Köpmanholmen"
},
{
	.type = FE_OFDM,
	.name = "se-Kopparberg",
	.muxes = muxlist_FE_OFDM_se_Kopparberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kopparberg) / sizeof(struct mux),
	.comment = "Sweden - Kopparberg"
},
{
	.type = FE_OFDM,
	.name = "se-Kramfors_Lugnvik",
	.muxes = muxlist_FE_OFDM_se_Kramfors_Lugnvik,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kramfors_Lugnvik) / sizeof(struct mux),
	.comment = "Sweden - Kramfors/Lugnvik"
},
{
	.type = FE_OFDM,
	.name = "se-Kristinehamn_Utsiktsberget",
	.muxes = muxlist_FE_OFDM_se_Kristinehamn_Utsiktsberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kristinehamn_Utsiktsberget) / sizeof(struct mux),
	.comment = "Sweden - Kristinehamn/Utsiktsberget"
},
{
	.type = FE_OFDM,
	.name = "se-Kungsater",
	.muxes = muxlist_FE_OFDM_se_Kungsater,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kungsater) / sizeof(struct mux),
	.comment = "Sweden - Kungsäter"
},
{
	.type = FE_OFDM,
	.name = "se-Kungsberget_GI",
	.muxes = muxlist_FE_OFDM_se_Kungsberget_GI,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Kungsberget_GI) / sizeof(struct mux),
	.comment = "Sweden - Kungsberget/GI"
},
{
	.type = FE_OFDM,
	.name = "se-Langshyttan",
	.muxes = muxlist_FE_OFDM_se_Langshyttan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Langshyttan) / sizeof(struct mux),
	.comment = "Sweden - Långshyttan"
},
{
	.type = FE_OFDM,
	.name = "se-Langshyttan_Engelsfors",
	.muxes = muxlist_FE_OFDM_se_Langshyttan_Engelsfors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Langshyttan_Engelsfors) / sizeof(struct mux),
	.comment = "Sweden - Långshyttan/Engelsfors"
},
{
	.type = FE_OFDM,
	.name = "se-Leksand_Karingberget",
	.muxes = muxlist_FE_OFDM_se_Leksand_Karingberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Leksand_Karingberget) / sizeof(struct mux),
	.comment = "Sweden - Leksand/Käringberget"
},
{
	.type = FE_OFDM,
	.name = "se-Lerdala",
	.muxes = muxlist_FE_OFDM_se_Lerdala,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Lerdala) / sizeof(struct mux),
	.comment = "Sweden - Lerdala"
},
{
	.type = FE_OFDM,
	.name = "se-Lilltjara_Digerberget",
	.muxes = muxlist_FE_OFDM_se_Lilltjara_Digerberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Lilltjara_Digerberget) / sizeof(struct mux),
	.comment = "Sweden - Lilltjära/Digerberget"
},
{
	.type = FE_OFDM,
	.name = "se-Limedsforsen",
	.muxes = muxlist_FE_OFDM_se_Limedsforsen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Limedsforsen) / sizeof(struct mux),
	.comment = "Sweden - Limedsforsen"
},
{
	.type = FE_OFDM,
	.name = "se-Lindshammar_Ramkvilla",
	.muxes = muxlist_FE_OFDM_se_Lindshammar_Ramkvilla,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Lindshammar_Ramkvilla) / sizeof(struct mux),
	.comment = "Sweden - Lindshammar/Ramkvilla"
},
{
	.type = FE_OFDM,
	.name = "se-Linkoping_Vattentornet",
	.muxes = muxlist_FE_OFDM_se_Linkoping_Vattentornet,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Linkoping_Vattentornet) / sizeof(struct mux),
	.comment = "Sweden - Linköping/Vattentornet"
},
{
	.type = FE_OFDM,
	.name = "se-Ljugarn",
	.muxes = muxlist_FE_OFDM_se_Ljugarn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ljugarn) / sizeof(struct mux),
	.comment = "Sweden - Ljugarn"
},
{
	.type = FE_OFDM,
	.name = "se-Loffstrand",
	.muxes = muxlist_FE_OFDM_se_Loffstrand,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Loffstrand) / sizeof(struct mux),
	.comment = "Sweden - Loffstrand"
},
{
	.type = FE_OFDM,
	.name = "se-Lonneberga",
	.muxes = muxlist_FE_OFDM_se_Lonneberga,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Lonneberga) / sizeof(struct mux),
	.comment = "Sweden - Lönneberga"
},
{
	.type = FE_OFDM,
	.name = "se-Lorstrand",
	.muxes = muxlist_FE_OFDM_se_Lorstrand,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Lorstrand) / sizeof(struct mux),
	.comment = "Sweden - Lörstrand"
},
{
	.type = FE_OFDM,
	.name = "se-Ludvika_Bjorkasen",
	.muxes = muxlist_FE_OFDM_se_Ludvika_Bjorkasen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ludvika_Bjorkasen) / sizeof(struct mux),
	.comment = "Sweden - Ludvika/Björkåsen"
},
{
	.type = FE_OFDM,
	.name = "se-Lumsheden_Trekanten",
	.muxes = muxlist_FE_OFDM_se_Lumsheden_Trekanten,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Lumsheden_Trekanten) / sizeof(struct mux),
	.comment = "Sweden - Lumsheden/Trekanten"
},
{
	.type = FE_OFDM,
	.name = "se-Lycksele_Knaften",
	.muxes = muxlist_FE_OFDM_se_Lycksele_Knaften,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Lycksele_Knaften) / sizeof(struct mux),
	.comment = "Sweden - Lycksele/Knaften"
},
{
	.type = FE_OFDM,
	.name = "se-Mahult",
	.muxes = muxlist_FE_OFDM_se_Mahult,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Mahult) / sizeof(struct mux),
	.comment = "Sweden - Mahult"
},
{
	.type = FE_OFDM,
	.name = "se-Malmo_Jagersro",
	.muxes = muxlist_FE_OFDM_se_Malmo_Jagersro,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Malmo_Jagersro) / sizeof(struct mux),
	.comment = "Sweden - Malmö/Jägersro"
},
{
	.type = FE_OFDM,
	.name = "se-Malung",
	.muxes = muxlist_FE_OFDM_se_Malung,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Malung) / sizeof(struct mux),
	.comment = "Sweden - Malung"
},
{
	.type = FE_OFDM,
	.name = "se-Mariannelund",
	.muxes = muxlist_FE_OFDM_se_Mariannelund,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Mariannelund) / sizeof(struct mux),
	.comment = "Sweden - Mariannelund"
},
{
	.type = FE_OFDM,
	.name = "se-Markaryd_Hualtet",
	.muxes = muxlist_FE_OFDM_se_Markaryd_Hualtet,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Markaryd_Hualtet) / sizeof(struct mux),
	.comment = "Sweden - Markaryd/Hualtet"
},
{
	.type = FE_OFDM,
	.name = "se-Matfors",
	.muxes = muxlist_FE_OFDM_se_Matfors,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Matfors) / sizeof(struct mux),
	.comment = "Sweden - Matfors"
},
{
	.type = FE_OFDM,
	.name = "se-Molnbo_Tallstugan",
	.muxes = muxlist_FE_OFDM_se_Molnbo_Tallstugan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Molnbo_Tallstugan) / sizeof(struct mux),
	.comment = "Sweden - Mölnbo/Tallstugan"
},
{
	.type = FE_OFDM,
	.name = "se-Molndal_Vasterberget",
	.muxes = muxlist_FE_OFDM_se_Molndal_Vasterberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Molndal_Vasterberget) / sizeof(struct mux),
	.comment = "Sweden - Mölndal/Västerberget"
},
{
	.type = FE_OFDM,
	.name = "se-Mora_Eldris",
	.muxes = muxlist_FE_OFDM_se_Mora_Eldris,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Mora_Eldris) / sizeof(struct mux),
	.comment = "Sweden - Mora/Eldris"
},
{
	.type = FE_OFDM,
	.name = "se-Motala_Ervasteby",
	.muxes = muxlist_FE_OFDM_se_Motala_Ervasteby,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Motala_Ervasteby) / sizeof(struct mux),
	.comment = "Sweden - Motala/Ervasteby"
},
{
	.type = FE_OFDM,
	.name = "se-Mullsjo_Torestorp",
	.muxes = muxlist_FE_OFDM_se_Mullsjo_Torestorp,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Mullsjo_Torestorp) / sizeof(struct mux),
	.comment = "Sweden - Mullsjö/Torestorp"
},
{
	.type = FE_OFDM,
	.name = "se-Nassjo",
	.muxes = muxlist_FE_OFDM_se_Nassjo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Nassjo) / sizeof(struct mux),
	.comment = "Sweden - Nässj"
},
{
	.type = FE_OFDM,
	.name = "se-Navekvarn",
	.muxes = muxlist_FE_OFDM_se_Navekvarn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Navekvarn) / sizeof(struct mux),
	.comment = "Sweden - Nävekvarn"
},
{
	.type = FE_OFDM,
	.name = "se-Norrahammar",
	.muxes = muxlist_FE_OFDM_se_Norrahammar,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Norrahammar) / sizeof(struct mux),
	.comment = "Sweden - Norrahammar"
},
{
	.type = FE_OFDM,
	.name = "se-Norrkoping_Krokek",
	.muxes = muxlist_FE_OFDM_se_Norrkoping_Krokek,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Norrkoping_Krokek) / sizeof(struct mux),
	.comment = "Sweden - Norrköping/Krokek"
},
{
	.type = FE_OFDM,
	.name = "se-Norrtalje_Sodra_Bergen",
	.muxes = muxlist_FE_OFDM_se_Norrtalje_Sodra_Bergen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Norrtalje_Sodra_Bergen) / sizeof(struct mux),
	.comment = "Sweden - Norrtälje/Södra Bergen"
},
{
	.type = FE_OFDM,
	.name = "se-Nykoping",
	.muxes = muxlist_FE_OFDM_se_Nykoping,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Nykoping) / sizeof(struct mux),
	.comment = "Sweden - Nyköping"
},
{
	.type = FE_OFDM,
	.name = "se-Orebro_Lockhyttan",
	.muxes = muxlist_FE_OFDM_se_Orebro_Lockhyttan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Orebro_Lockhyttan) / sizeof(struct mux),
	.comment = "Sweden - Örebro/Lockhyttan"
},
{
	.type = FE_OFDM,
	.name = "se-Ornskoldsvik_As",
	.muxes = muxlist_FE_OFDM_se_Ornskoldsvik_As,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ornskoldsvik_As) / sizeof(struct mux),
	.comment = "Sweden - Örnsköldsvik/Ås"
},
{
	.type = FE_OFDM,
	.name = "se-Oskarshamn",
	.muxes = muxlist_FE_OFDM_se_Oskarshamn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Oskarshamn) / sizeof(struct mux),
	.comment = "Sweden - Oskarshamn"
},
{
	.type = FE_OFDM,
	.name = "se-Ostersund_Brattasen",
	.muxes = muxlist_FE_OFDM_se_Ostersund_Brattasen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ostersund_Brattasen) / sizeof(struct mux),
	.comment = "Sweden - Östersund/Brattåsen"
},
{
	.type = FE_OFDM,
	.name = "se-Osthammar_Valo",
	.muxes = muxlist_FE_OFDM_se_Osthammar_Valo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Osthammar_Valo) / sizeof(struct mux),
	.comment = "Sweden - Östhammar/Val"
},
{
	.type = FE_OFDM,
	.name = "se-Overkalix",
	.muxes = muxlist_FE_OFDM_se_Overkalix,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Overkalix) / sizeof(struct mux),
	.comment = "Sweden - Överkalix"
},
{
	.type = FE_OFDM,
	.name = "se-Oxberg",
	.muxes = muxlist_FE_OFDM_se_Oxberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Oxberg) / sizeof(struct mux),
	.comment = "Sweden - Oxberg"
},
{
	.type = FE_OFDM,
	.name = "se-Pajala",
	.muxes = muxlist_FE_OFDM_se_Pajala,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Pajala) / sizeof(struct mux),
	.comment = "Sweden - Pajala"
},
{
	.type = FE_OFDM,
	.name = "se-Paulistom",
	.muxes = muxlist_FE_OFDM_se_Paulistom,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Paulistom) / sizeof(struct mux),
	.comment = "Sweden - Paulistöm"
},
{
	.type = FE_OFDM,
	.name = "se-Rattvik",
	.muxes = muxlist_FE_OFDM_se_Rattvik,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Rattvik) / sizeof(struct mux),
	.comment = "Sweden - Rättvik"
},
{
	.type = FE_OFDM,
	.name = "se-Rengsjo",
	.muxes = muxlist_FE_OFDM_se_Rengsjo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Rengsjo) / sizeof(struct mux),
	.comment = "Sweden - Rengsj"
},
{
	.type = FE_OFDM,
	.name = "se-Rorbacksnas",
	.muxes = muxlist_FE_OFDM_se_Rorbacksnas,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Rorbacksnas) / sizeof(struct mux),
	.comment = "Sweden - Rörbäcksnäs"
},
{
	.type = FE_OFDM,
	.name = "se-Sagmyra",
	.muxes = muxlist_FE_OFDM_se_Sagmyra,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sagmyra) / sizeof(struct mux),
	.comment = "Sweden - Sågmyra"
},
{
	.type = FE_OFDM,
	.name = "se-Salen",
	.muxes = muxlist_FE_OFDM_se_Salen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Salen) / sizeof(struct mux),
	.comment = "Sweden - Sälen"
},
{
	.type = FE_OFDM,
	.name = "se-Salfjallet",
	.muxes = muxlist_FE_OFDM_se_Salfjallet,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Salfjallet) / sizeof(struct mux),
	.comment = "Sweden - Sälfjället"
},
{
	.type = FE_OFDM,
	.name = "se-Sarna_Mickeltemplet",
	.muxes = muxlist_FE_OFDM_se_Sarna_Mickeltemplet,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sarna_Mickeltemplet) / sizeof(struct mux),
	.comment = "Sweden - Särna/Mickeltemplet"
},
{
	.type = FE_OFDM,
	.name = "se-Satila",
	.muxes = muxlist_FE_OFDM_se_Satila,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Satila) / sizeof(struct mux),
	.comment = "Sweden - Sätila"
},
{
	.type = FE_OFDM,
	.name = "se-Saxdalen",
	.muxes = muxlist_FE_OFDM_se_Saxdalen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Saxdalen) / sizeof(struct mux),
	.comment = "Sweden - Saxdalen"
},
{
	.type = FE_OFDM,
	.name = "se-Siljansnas_Uvberget",
	.muxes = muxlist_FE_OFDM_se_Siljansnas_Uvberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Siljansnas_Uvberget) / sizeof(struct mux),
	.comment = "Sweden - Siljansnäs/Uvberget"
},
{
	.type = FE_OFDM,
	.name = "se-Skarstad",
	.muxes = muxlist_FE_OFDM_se_Skarstad,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Skarstad) / sizeof(struct mux),
	.comment = "Sweden - Skärstad"
},
{
	.type = FE_OFDM,
	.name = "se-Skattungbyn",
	.muxes = muxlist_FE_OFDM_se_Skattungbyn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Skattungbyn) / sizeof(struct mux),
	.comment = "Sweden - Skattungbyn"
},
{
	.type = FE_OFDM,
	.name = "se-Skelleftea",
	.muxes = muxlist_FE_OFDM_se_Skelleftea,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Skelleftea) / sizeof(struct mux),
	.comment = "Sweden - Skellefte"
},
{
	.type = FE_OFDM,
	.name = "se-Skene_Nycklarberget",
	.muxes = muxlist_FE_OFDM_se_Skene_Nycklarberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Skene_Nycklarberget) / sizeof(struct mux),
	.comment = "Sweden - Skene/Nycklarberget"
},
{
	.type = FE_OFDM,
	.name = "se-Skovde",
	.muxes = muxlist_FE_OFDM_se_Skovde,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Skovde) / sizeof(struct mux),
	.comment = "Sweden - Skövde"
},
{
	.type = FE_OFDM,
	.name = "se-Smedjebacken_Uvberget",
	.muxes = muxlist_FE_OFDM_se_Smedjebacken_Uvberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Smedjebacken_Uvberget) / sizeof(struct mux),
	.comment = "Sweden - Smedjebacken/Uvberget"
},
{
	.type = FE_OFDM,
	.name = "se-Soderhamn",
	.muxes = muxlist_FE_OFDM_se_Soderhamn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Soderhamn) / sizeof(struct mux),
	.comment = "Sweden - Söderhamn"
},
{
	.type = FE_OFDM,
	.name = "se-Soderkoping",
	.muxes = muxlist_FE_OFDM_se_Soderkoping,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Soderkoping) / sizeof(struct mux),
	.comment = "Sweden - Söderköping"
},
{
	.type = FE_OFDM,
	.name = "se-Sodertalje_Ragnhildsborg",
	.muxes = muxlist_FE_OFDM_se_Sodertalje_Ragnhildsborg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sodertalje_Ragnhildsborg) / sizeof(struct mux),
	.comment = "Sweden - Södertälje/Ragnhildsborg"
},
{
	.type = FE_OFDM,
	.name = "se-Solleftea_Hallsta",
	.muxes = muxlist_FE_OFDM_se_Solleftea_Hallsta,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Solleftea_Hallsta) / sizeof(struct mux),
	.comment = "Sweden - Sollefteå/Hallsta"
},
{
	.type = FE_OFDM,
	.name = "se-Solleftea_Multra",
	.muxes = muxlist_FE_OFDM_se_Solleftea_Multra,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Solleftea_Multra) / sizeof(struct mux),
	.comment = "Sweden - Sollefteå/Multr"
},
{
	.type = FE_OFDM,
	.name = "se-Sorsjon",
	.muxes = muxlist_FE_OFDM_se_Sorsjon,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sorsjon) / sizeof(struct mux),
	.comment = "Sweden - Sörsjön"
},
{
	.type = FE_OFDM,
	.name = "se-Stockholm_Marieberg",
	.muxes = muxlist_FE_OFDM_se_Stockholm_Marieberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Stockholm_Marieberg) / sizeof(struct mux),
	.comment = "Sweden - Stockholm/Marieberg"
},
{
	.type = FE_OFDM,
	.name = "se-Stockholm_Nacka",
	.muxes = muxlist_FE_OFDM_se_Stockholm_Nacka,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Stockholm_Nacka) / sizeof(struct mux),
	.comment = "Sweden - Stockholm/Nacka"
},
{
	.type = FE_OFDM,
	.name = "se-Stora_Skedvi",
	.muxes = muxlist_FE_OFDM_se_Stora_Skedvi,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Stora_Skedvi) / sizeof(struct mux),
	.comment = "Sweden - Stora Skedvi"
},
{
	.type = FE_OFDM,
	.name = "se-Storfjaten",
	.muxes = muxlist_FE_OFDM_se_Storfjaten,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Storfjaten) / sizeof(struct mux),
	.comment = "Sweden - Storfjäten"
},
{
	.type = FE_OFDM,
	.name = "se-Storuman",
	.muxes = muxlist_FE_OFDM_se_Storuman,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Storuman) / sizeof(struct mux),
	.comment = "Sweden - Storuman"
},
{
	.type = FE_OFDM,
	.name = "se-Stromstad",
	.muxes = muxlist_FE_OFDM_se_Stromstad,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Stromstad) / sizeof(struct mux),
	.comment = "Sweden - Strömstad"
},
{
	.type = FE_OFDM,
	.name = "se-Styrsjobo",
	.muxes = muxlist_FE_OFDM_se_Styrsjobo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Styrsjobo) / sizeof(struct mux),
	.comment = "Sweden - Styrsjöbo"
},
{
	.type = FE_OFDM,
	.name = "se-Sundborn",
	.muxes = muxlist_FE_OFDM_se_Sundborn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sundborn) / sizeof(struct mux),
	.comment = "Sweden - Sundborn"
},
{
	.type = FE_OFDM,
	.name = "se-Sundsbruk",
	.muxes = muxlist_FE_OFDM_se_Sundsbruk,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sundsbruk) / sizeof(struct mux),
	.comment = "Sweden - Sundsbruk"
},
{
	.type = FE_OFDM,
	.name = "se-Sundsvall_S_Stadsberget",
	.muxes = muxlist_FE_OFDM_se_Sundsvall_S_Stadsberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sundsvall_S_Stadsberget) / sizeof(struct mux),
	.comment = "Sweden - Sundsvall/S Stadsberget"
},
{
	.type = FE_OFDM,
	.name = "se-Sunne_Blabarskullen",
	.muxes = muxlist_FE_OFDM_se_Sunne_Blabarskullen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sunne_Blabarskullen) / sizeof(struct mux),
	.comment = "Sweden - Sunne/Blåbärskullen"
},
{
	.type = FE_OFDM,
	.name = "se-Svartnas",
	.muxes = muxlist_FE_OFDM_se_Svartnas,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Svartnas) / sizeof(struct mux),
	.comment = "Sweden - Svartnäs"
},
{
	.type = FE_OFDM,
	.name = "se-Sveg_Brickan",
	.muxes = muxlist_FE_OFDM_se_Sveg_Brickan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Sveg_Brickan) / sizeof(struct mux),
	.comment = "Sweden - Sveg/Brickan"
},
{
	.type = FE_OFDM,
	.name = "se-Taberg",
	.muxes = muxlist_FE_OFDM_se_Taberg,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Taberg) / sizeof(struct mux),
	.comment = "Sweden - Taberg"
},
{
	.type = FE_OFDM,
	.name = "se-Tandadalen",
	.muxes = muxlist_FE_OFDM_se_Tandadalen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Tandadalen) / sizeof(struct mux),
	.comment = "Sweden - Tandådalen"
},
{
	.type = FE_OFDM,
	.name = "se-Tasjo",
	.muxes = muxlist_FE_OFDM_se_Tasjo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Tasjo) / sizeof(struct mux),
	.comment = "Sweden - Tåsj"
},
{
	.type = FE_OFDM,
	.name = "se-Tollsjo",
	.muxes = muxlist_FE_OFDM_se_Tollsjo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Tollsjo) / sizeof(struct mux),
	.comment = "Sweden - Töllsj"
},
{
	.type = FE_OFDM,
	.name = "se-Torsby_Bada",
	.muxes = muxlist_FE_OFDM_se_Torsby_Bada,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Torsby_Bada) / sizeof(struct mux),
	.comment = "Sweden - Torsby/Bada"
},
{
	.type = FE_OFDM,
	.name = "se-Tranas_Bredkarr",
	.muxes = muxlist_FE_OFDM_se_Tranas_Bredkarr,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Tranas_Bredkarr) / sizeof(struct mux),
	.comment = "Sweden - Tranås/Bredkärr"
},
{
	.type = FE_OFDM,
	.name = "se-Tranemo",
	.muxes = muxlist_FE_OFDM_se_Tranemo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Tranemo) / sizeof(struct mux),
	.comment = "Sweden - Tranemo"
},
{
	.type = FE_OFDM,
	.name = "se-Transtrand_Bolheden",
	.muxes = muxlist_FE_OFDM_se_Transtrand_Bolheden,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Transtrand_Bolheden) / sizeof(struct mux),
	.comment = "Sweden - Transtrand/Bolheden"
},
{
	.type = FE_OFDM,
	.name = "se-Traryd_Betas",
	.muxes = muxlist_FE_OFDM_se_Traryd_Betas,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Traryd_Betas) / sizeof(struct mux),
	.comment = "Sweden - Traryd/Betås"
},
{
	.type = FE_OFDM,
	.name = "se-Trollhattan",
	.muxes = muxlist_FE_OFDM_se_Trollhattan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Trollhattan) / sizeof(struct mux),
	.comment = "Sweden - Trollhättan"
},
{
	.type = FE_OFDM,
	.name = "se-Trosa",
	.muxes = muxlist_FE_OFDM_se_Trosa,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Trosa) / sizeof(struct mux),
	.comment = "Sweden - Trosa"
},
{
	.type = FE_OFDM,
	.name = "se-Tystberga",
	.muxes = muxlist_FE_OFDM_se_Tystberga,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Tystberga) / sizeof(struct mux),
	.comment = "Sweden - Tystberga"
},
{
	.type = FE_OFDM,
	.name = "se-Uddevalla_Herrestad",
	.muxes = muxlist_FE_OFDM_se_Uddevalla_Herrestad,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Uddevalla_Herrestad) / sizeof(struct mux),
	.comment = "Sweden - Uddevalla/Herrestad"
},
{
	.type = FE_OFDM,
	.name = "se-Ullared",
	.muxes = muxlist_FE_OFDM_se_Ullared,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ullared) / sizeof(struct mux),
	.comment = "Sweden - Ullared"
},
{
	.type = FE_OFDM,
	.name = "se-Ulricehamn",
	.muxes = muxlist_FE_OFDM_se_Ulricehamn,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ulricehamn) / sizeof(struct mux),
	.comment = "Sweden - Ulricehamn"
},
{
	.type = FE_OFDM,
	.name = "se-Ulvshyttan_Porjus",
	.muxes = muxlist_FE_OFDM_se_Ulvshyttan_Porjus,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ulvshyttan_Porjus) / sizeof(struct mux),
	.comment = "Sweden - Ulvshyttan/Porjus"
},
{
	.type = FE_OFDM,
	.name = "se-Uppsala_Rickomberga",
	.muxes = muxlist_FE_OFDM_se_Uppsala_Rickomberga,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Uppsala_Rickomberga) / sizeof(struct mux),
	.comment = "Sweden - Uppsala/Rickomberga"
},
{
	.type = FE_OFDM,
	.name = "se-Uppsala_Vedyxa",
	.muxes = muxlist_FE_OFDM_se_Uppsala_Vedyxa,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Uppsala_Vedyxa) / sizeof(struct mux),
	.comment = "Sweden - Uppsala/Vedyxa"
},
{
	.type = FE_OFDM,
	.name = "se-Vaddo_Elmsta",
	.muxes = muxlist_FE_OFDM_se_Vaddo_Elmsta,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vaddo_Elmsta) / sizeof(struct mux),
	.comment = "Sweden - Väddö/Elmsta"
},
{
	.type = FE_OFDM,
	.name = "se-Valdemarsvik",
	.muxes = muxlist_FE_OFDM_se_Valdemarsvik,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Valdemarsvik) / sizeof(struct mux),
	.comment = "Sweden - Valdemarsvik"
},
{
	.type = FE_OFDM,
	.name = "se-Vannas_Granlundsberget",
	.muxes = muxlist_FE_OFDM_se_Vannas_Granlundsberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vannas_Granlundsberget) / sizeof(struct mux),
	.comment = "Sweden - Vännäs/Granlundsberget"
},
{
	.type = FE_OFDM,
	.name = "se-Vansbro_Hummelberget",
	.muxes = muxlist_FE_OFDM_se_Vansbro_Hummelberget,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vansbro_Hummelberget) / sizeof(struct mux),
	.comment = "Sweden - Vansbro/Hummelberget"
},
{
	.type = FE_OFDM,
	.name = "se-Varberg_Grimeton",
	.muxes = muxlist_FE_OFDM_se_Varberg_Grimeton,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Varberg_Grimeton) / sizeof(struct mux),
	.comment = "Sweden - Varberg/Grimeton"
},
{
	.type = FE_OFDM,
	.name = "se-Vasteras_Lillharad",
	.muxes = muxlist_FE_OFDM_se_Vasteras_Lillharad,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vasteras_Lillharad) / sizeof(struct mux),
	.comment = "Sweden - Västerås/Lillhärad"
},
{
	.type = FE_OFDM,
	.name = "se-Vastervik_Farhult",
	.muxes = muxlist_FE_OFDM_se_Vastervik_Farhult,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vastervik_Farhult) / sizeof(struct mux),
	.comment = "Sweden - Västervik/Fårhult"
},
{
	.type = FE_OFDM,
	.name = "se-Vaxbo",
	.muxes = muxlist_FE_OFDM_se_Vaxbo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vaxbo) / sizeof(struct mux),
	.comment = "Sweden - Växbo"
},
{
	.type = FE_OFDM,
	.name = "se-Vessigebro",
	.muxes = muxlist_FE_OFDM_se_Vessigebro,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vessigebro) / sizeof(struct mux),
	.comment = "Sweden - Vessigebro"
},
{
	.type = FE_OFDM,
	.name = "se-Vetlanda_Nye",
	.muxes = muxlist_FE_OFDM_se_Vetlanda_Nye,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vetlanda_Nye) / sizeof(struct mux),
	.comment = "Sweden - Vetlanda/Nye"
},
{
	.type = FE_OFDM,
	.name = "se-Vikmanshyttan",
	.muxes = muxlist_FE_OFDM_se_Vikmanshyttan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vikmanshyttan) / sizeof(struct mux),
	.comment = "Sweden - Vikmanshyttan"
},
{
	.type = FE_OFDM,
	.name = "se-Virserum",
	.muxes = muxlist_FE_OFDM_se_Virserum,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Virserum) / sizeof(struct mux),
	.comment = "Sweden - Virserum"
},
{
	.type = FE_OFDM,
	.name = "se-Visby_Follingbo",
	.muxes = muxlist_FE_OFDM_se_Visby_Follingbo,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Visby_Follingbo) / sizeof(struct mux),
	.comment = "Sweden - Visby/Follingbo"
},
{
	.type = FE_OFDM,
	.name = "se-Visby_Hamnen",
	.muxes = muxlist_FE_OFDM_se_Visby_Hamnen,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Visby_Hamnen) / sizeof(struct mux),
	.comment = "Sweden - Visby/Hamnen"
},
{
	.type = FE_OFDM,
	.name = "se-Visingso",
	.muxes = muxlist_FE_OFDM_se_Visingso,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Visingso) / sizeof(struct mux),
	.comment = "Sweden - Visings"
},
{
	.type = FE_OFDM,
	.name = "se-Vislanda_Nydala",
	.muxes = muxlist_FE_OFDM_se_Vislanda_Nydala,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Vislanda_Nydala) / sizeof(struct mux),
	.comment = "Sweden - Vislanda/Nydala"
},
{
	.type = FE_OFDM,
	.name = "se-Voxna",
	.muxes = muxlist_FE_OFDM_se_Voxna,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Voxna) / sizeof(struct mux),
	.comment = "Sweden - Voxna"
},
{
	.type = FE_OFDM,
	.name = "se-Ystad_Metallgatan",
	.muxes = muxlist_FE_OFDM_se_Ystad_Metallgatan,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Ystad_Metallgatan) / sizeof(struct mux),
	.comment = "Sweden - Ystad/Metallgatan"
},
{
	.type = FE_OFDM,
	.name = "se-Yttermalung",
	.muxes = muxlist_FE_OFDM_se_Yttermalung,
	.nmuxes = sizeof(muxlist_FE_OFDM_se_Yttermalung) / sizeof(struct mux),
	.comment = "Sweden - Yttermalung"
},
{
	.type = FE_OFDM,
	.name = "sk-BanskaBystrica",
	.muxes = muxlist_FE_OFDM_sk_BanskaBystrica,
	.nmuxes = sizeof(muxlist_FE_OFDM_sk_BanskaBystrica) / sizeof(struct mux),
	.comment = "DVB-T Banska Bystrica (Banska Bystrica, Slovak Republic)"
},
{
	.type = FE_OFDM,
	.name = "sk-Bratislava",
	.muxes = muxlist_FE_OFDM_sk_Bratislava,
	.nmuxes = sizeof(muxlist_FE_OFDM_sk_Bratislava) / sizeof(struct mux),
	.comment = "DVB-T Bratislava (Bratislava, Slovak Republic)"
},
{
	.type = FE_OFDM,
	.name = "sk-Kosice",
	.muxes = muxlist_FE_OFDM_sk_Kosice,
	.nmuxes = sizeof(muxlist_FE_OFDM_sk_Kosice) / sizeof(struct mux),
	.comment = "DVB-T Kosice (Kosice, Slovak Republic)"
},
{
	.type = FE_OFDM,
	.name = "tw-Kaohsiung",
	.muxes = muxlist_FE_OFDM_tw_Kaohsiung,
	.nmuxes = sizeof(muxlist_FE_OFDM_tw_Kaohsiung) / sizeof(struct mux),
	.comment = "Taiwan - Kaohsiung, southern Taiwan"
},
{
	.type = FE_OFDM,
	.name = "tw-Taipei",
	.muxes = muxlist_FE_OFDM_tw_Taipei,
	.nmuxes = sizeof(muxlist_FE_OFDM_tw_Taipei) / sizeof(struct mux),
	.comment = "Taiwan - Taipei, northern Taiwan"
},
{
	.type = FE_OFDM,
	.name = "uk-Aberdare",
	.muxes = muxlist_FE_OFDM_uk_Aberdare,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Aberdare) / sizeof(struct mux),
	.comment = "UK, Aberdare"
},
{
	.type = FE_OFDM,
	.name = "uk-Angus",
	.muxes = muxlist_FE_OFDM_uk_Angus,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Angus) / sizeof(struct mux),
	.comment = "UK, Angus"
},
{
	.type = FE_OFDM,
	.name = "uk-BeaconHill",
	.muxes = muxlist_FE_OFDM_uk_BeaconHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_BeaconHill) / sizeof(struct mux),
	.comment = "UK, Beacon Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-Belmont",
	.muxes = muxlist_FE_OFDM_uk_Belmont,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Belmont) / sizeof(struct mux),
	.comment = "UK, Belmont"
},
{
	.type = FE_OFDM,
	.name = "uk-Bilsdale",
	.muxes = muxlist_FE_OFDM_uk_Bilsdale,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Bilsdale) / sizeof(struct mux),
	.comment = "UK, Bilsdale"
},
{
	.type = FE_OFDM,
	.name = "uk-BlackHill",
	.muxes = muxlist_FE_OFDM_uk_BlackHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_BlackHill) / sizeof(struct mux),
	.comment = "UK, Black Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-Blaenplwyf",
	.muxes = muxlist_FE_OFDM_uk_Blaenplwyf,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Blaenplwyf) / sizeof(struct mux),
	.comment = "UK, Blaenplwyf"
},
{
	.type = FE_OFDM,
	.name = "uk-BluebellHill",
	.muxes = muxlist_FE_OFDM_uk_BluebellHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_BluebellHill) / sizeof(struct mux),
	.comment = "UK, Bluebell Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-Bressay",
	.muxes = muxlist_FE_OFDM_uk_Bressay,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Bressay) / sizeof(struct mux),
	.comment = "UK, Bressay"
},
{
	.type = FE_OFDM,
	.name = "uk-BrierleyHill",
	.muxes = muxlist_FE_OFDM_uk_BrierleyHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_BrierleyHill) / sizeof(struct mux),
	.comment = "UK, Brierley Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-BristolIlchesterCres",
	.muxes = muxlist_FE_OFDM_uk_BristolIlchesterCres,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_BristolIlchesterCres) / sizeof(struct mux),
	.comment = "UK, Bristol Ilchester Cres."
},
{
	.type = FE_OFDM,
	.name = "uk-BristolKingsWeston",
	.muxes = muxlist_FE_OFDM_uk_BristolKingsWeston,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_BristolKingsWeston) / sizeof(struct mux),
	.comment = "UK, Bristol King's Weston"
},
{
	.type = FE_OFDM,
	.name = "uk-Bromsgrove",
	.muxes = muxlist_FE_OFDM_uk_Bromsgrove,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Bromsgrove) / sizeof(struct mux),
	.comment = "UK, Bromsgrove"
},
{
	.type = FE_OFDM,
	.name = "uk-BrougherMountain",
	.muxes = muxlist_FE_OFDM_uk_BrougherMountain,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_BrougherMountain) / sizeof(struct mux),
	.comment = "UK, Brougher Mountain"
},
{
	.type = FE_OFDM,
	.name = "uk-Caldbeck",
	.muxes = muxlist_FE_OFDM_uk_Caldbeck,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Caldbeck) / sizeof(struct mux),
	.comment = "UK, Caldbeck"
},
{
	.type = FE_OFDM,
	.name = "uk-CaradonHill",
	.muxes = muxlist_FE_OFDM_uk_CaradonHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_CaradonHill) / sizeof(struct mux),
	.comment = "UK, Caradon Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-Carmel",
	.muxes = muxlist_FE_OFDM_uk_Carmel,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Carmel) / sizeof(struct mux),
	.comment = "UK, Carmel"
},
{
	.type = FE_OFDM,
	.name = "uk-Chatton",
	.muxes = muxlist_FE_OFDM_uk_Chatton,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Chatton) / sizeof(struct mux),
	.comment = "UK, Chatton"
},
{
	.type = FE_OFDM,
	.name = "uk-Chesterfield",
	.muxes = muxlist_FE_OFDM_uk_Chesterfield,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Chesterfield) / sizeof(struct mux),
	.comment = "UK, Chesterfield"
},
{
	.type = FE_OFDM,
	.name = "uk-Craigkelly",
	.muxes = muxlist_FE_OFDM_uk_Craigkelly,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Craigkelly) / sizeof(struct mux),
	.comment = "UK, Craigkelly"
},
{
	.type = FE_OFDM,
	.name = "uk-CrystalPalace",
	.muxes = muxlist_FE_OFDM_uk_CrystalPalace,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_CrystalPalace) / sizeof(struct mux),
	.comment = "UK, Crystal Palace"
},
{
	.type = FE_OFDM,
	.name = "uk-Darvel",
	.muxes = muxlist_FE_OFDM_uk_Darvel,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Darvel) / sizeof(struct mux),
	.comment = "UK, Darvel"
},
{
	.type = FE_OFDM,
	.name = "uk-Divis",
	.muxes = muxlist_FE_OFDM_uk_Divis,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Divis) / sizeof(struct mux),
	.comment = "UK, Divis"
},
{
	.type = FE_OFDM,
	.name = "uk-Dover",
	.muxes = muxlist_FE_OFDM_uk_Dover,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Dover) / sizeof(struct mux),
	.comment = "UK, Dover"
},
{
	.type = FE_OFDM,
	.name = "uk-Durris",
	.muxes = muxlist_FE_OFDM_uk_Durris,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Durris) / sizeof(struct mux),
	.comment = "UK, Durris"
},
{
	.type = FE_OFDM,
	.name = "uk-Eitshal",
	.muxes = muxlist_FE_OFDM_uk_Eitshal,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Eitshal) / sizeof(struct mux),
	.comment = "UK, Eitshal"
},
{
	.type = FE_OFDM,
	.name = "uk-EmleyMoor",
	.muxes = muxlist_FE_OFDM_uk_EmleyMoor,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_EmleyMoor) / sizeof(struct mux),
	.comment = "UK, Emley Moor"
},
{
	.type = FE_OFDM,
	.name = "uk-Fenham",
	.muxes = muxlist_FE_OFDM_uk_Fenham,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Fenham) / sizeof(struct mux),
	.comment = "UK, Fenham"
},
{
	.type = FE_OFDM,
	.name = "uk-Fenton",
	.muxes = muxlist_FE_OFDM_uk_Fenton,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Fenton) / sizeof(struct mux),
	.comment = "UK, Fenton"
},
{
	.type = FE_OFDM,
	.name = "uk-Ferryside",
	.muxes = muxlist_FE_OFDM_uk_Ferryside,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Ferryside) / sizeof(struct mux),
	.comment = "UK, Ferryside"
},
{
	.type = FE_OFDM,
	.name = "uk-Guildford",
	.muxes = muxlist_FE_OFDM_uk_Guildford,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Guildford) / sizeof(struct mux),
	.comment = "UK, Guildford"
},
{
	.type = FE_OFDM,
	.name = "uk-Hannington",
	.muxes = muxlist_FE_OFDM_uk_Hannington,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Hannington) / sizeof(struct mux),
	.comment = "UK, Hannington"
},
{
	.type = FE_OFDM,
	.name = "uk-Hastings",
	.muxes = muxlist_FE_OFDM_uk_Hastings,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Hastings) / sizeof(struct mux),
	.comment = "UK, Hastings"
},
{
	.type = FE_OFDM,
	.name = "uk-Heathfield",
	.muxes = muxlist_FE_OFDM_uk_Heathfield,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Heathfield) / sizeof(struct mux),
	.comment = "UK, Heathfield"
},
{
	.type = FE_OFDM,
	.name = "uk-HemelHempstead",
	.muxes = muxlist_FE_OFDM_uk_HemelHempstead,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_HemelHempstead) / sizeof(struct mux),
	.comment = "UK, Hemel Hempstead"
},
{
	.type = FE_OFDM,
	.name = "uk-HuntshawCross",
	.muxes = muxlist_FE_OFDM_uk_HuntshawCross,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_HuntshawCross) / sizeof(struct mux),
	.comment = "UK, Huntshaw Cross"
},
{
	.type = FE_OFDM,
	.name = "uk-Idle",
	.muxes = muxlist_FE_OFDM_uk_Idle,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Idle) / sizeof(struct mux),
	.comment = "UK, Idle"
},
{
	.type = FE_OFDM,
	.name = "uk-KeelylangHill",
	.muxes = muxlist_FE_OFDM_uk_KeelylangHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_KeelylangHill) / sizeof(struct mux),
	.comment = "UK, Keelylang Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-Keighley",
	.muxes = muxlist_FE_OFDM_uk_Keighley,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Keighley) / sizeof(struct mux),
	.comment = "UK, Keighley"
},
{
	.type = FE_OFDM,
	.name = "uk-KilveyHill",
	.muxes = muxlist_FE_OFDM_uk_KilveyHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_KilveyHill) / sizeof(struct mux),
	.comment = "UK, Kilvey Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-KnockMore",
	.muxes = muxlist_FE_OFDM_uk_KnockMore,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_KnockMore) / sizeof(struct mux),
	.comment = "UK, Knock More"
},
{
	.type = FE_OFDM,
	.name = "uk-Lancaster",
	.muxes = muxlist_FE_OFDM_uk_Lancaster,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Lancaster) / sizeof(struct mux),
	.comment = "UK, Lancaster"
},
{
	.type = FE_OFDM,
	.name = "uk-LarkStoke",
	.muxes = muxlist_FE_OFDM_uk_LarkStoke,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_LarkStoke) / sizeof(struct mux),
	.comment = "UK, Lark Stoke"
},
{
	.type = FE_OFDM,
	.name = "uk-Limavady",
	.muxes = muxlist_FE_OFDM_uk_Limavady,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Limavady) / sizeof(struct mux),
	.comment = "UK, Limavady"
},
{
	.type = FE_OFDM,
	.name = "uk-Llanddona",
	.muxes = muxlist_FE_OFDM_uk_Llanddona,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Llanddona) / sizeof(struct mux),
	.comment = "UK, Llanddona"
},
{
	.type = FE_OFDM,
	.name = "uk-Malvern",
	.muxes = muxlist_FE_OFDM_uk_Malvern,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Malvern) / sizeof(struct mux),
	.comment = "UK, Malvern"
},
{
	.type = FE_OFDM,
	.name = "uk-Mendip",
	.muxes = muxlist_FE_OFDM_uk_Mendip,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Mendip) / sizeof(struct mux),
	.comment = "UK, Mendip"
},
{
	.type = FE_OFDM,
	.name = "uk-Midhurst",
	.muxes = muxlist_FE_OFDM_uk_Midhurst,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Midhurst) / sizeof(struct mux),
	.comment = "UK, Midhurst"
},
{
	.type = FE_OFDM,
	.name = "uk-Moel-y-Parc",
	.muxes = muxlist_FE_OFDM_uk_Moel_y_Parc,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Moel_y_Parc) / sizeof(struct mux),
	.comment = "UK, Moel-y-Parc"
},
{
	.type = FE_OFDM,
	.name = "uk-Nottingham",
	.muxes = muxlist_FE_OFDM_uk_Nottingham,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Nottingham) / sizeof(struct mux),
	.comment = "UK, Nottingham"
},
{
	.type = FE_OFDM,
	.name = "uk-OliversMount",
	.muxes = muxlist_FE_OFDM_uk_OliversMount,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_OliversMount) / sizeof(struct mux),
	.comment = "UK, Oliver's Mount"
},
{
	.type = FE_OFDM,
	.name = "uk-Oxford",
	.muxes = muxlist_FE_OFDM_uk_Oxford,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Oxford) / sizeof(struct mux),
	.comment = "UK, Oxford"
},
{
	.type = FE_OFDM,
	.name = "uk-PendleForest",
	.muxes = muxlist_FE_OFDM_uk_PendleForest,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_PendleForest) / sizeof(struct mux),
	.comment = "UK, Pendle Forest"
},
{
	.type = FE_OFDM,
	.name = "uk-Plympton",
	.muxes = muxlist_FE_OFDM_uk_Plympton,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Plympton) / sizeof(struct mux),
	.comment = "UK, Plympton"
},
{
	.type = FE_OFDM,
	.name = "uk-PontopPike",
	.muxes = muxlist_FE_OFDM_uk_PontopPike,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_PontopPike) / sizeof(struct mux),
	.comment = "UK, Pontop Pike"
},
{
	.type = FE_OFDM,
	.name = "uk-Pontypool",
	.muxes = muxlist_FE_OFDM_uk_Pontypool,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Pontypool) / sizeof(struct mux),
	.comment = "UK, Pontypool"
},
{
	.type = FE_OFDM,
	.name = "uk-Presely",
	.muxes = muxlist_FE_OFDM_uk_Presely,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Presely) / sizeof(struct mux),
	.comment = "UK, Presely"
},
{
	.type = FE_OFDM,
	.name = "uk-Redruth",
	.muxes = muxlist_FE_OFDM_uk_Redruth,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Redruth) / sizeof(struct mux),
	.comment = "UK, Redruth"
},
{
	.type = FE_OFDM,
	.name = "uk-Reigate",
	.muxes = muxlist_FE_OFDM_uk_Reigate,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Reigate) / sizeof(struct mux),
	.comment = "UK, Reigate"
},
{
	.type = FE_OFDM,
	.name = "uk-RidgeHill",
	.muxes = muxlist_FE_OFDM_uk_RidgeHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_RidgeHill) / sizeof(struct mux),
	.comment = "UK, Ridge Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-Rosemarkie",
	.muxes = muxlist_FE_OFDM_uk_Rosemarkie,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Rosemarkie) / sizeof(struct mux),
	.comment = "UK, Rosemarkie"
},
{
	.type = FE_OFDM,
	.name = "uk-Rosneath",
	.muxes = muxlist_FE_OFDM_uk_Rosneath,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Rosneath) / sizeof(struct mux),
	.comment = "UK, Rosneath"
},
{
	.type = FE_OFDM,
	.name = "uk-Rowridge",
	.muxes = muxlist_FE_OFDM_uk_Rowridge,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Rowridge) / sizeof(struct mux),
	.comment = "UK, Rowridge"
},
{
	.type = FE_OFDM,
	.name = "uk-RumsterForest",
	.muxes = muxlist_FE_OFDM_uk_RumsterForest,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_RumsterForest) / sizeof(struct mux),
	.comment = "UK, Rumster Forest"
},
{
	.type = FE_OFDM,
	.name = "uk-Saddleworth",
	.muxes = muxlist_FE_OFDM_uk_Saddleworth,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Saddleworth) / sizeof(struct mux),
	.comment = "UK, Saddleworth"
},
{
	.type = FE_OFDM,
	.name = "uk-Salisbury",
	.muxes = muxlist_FE_OFDM_uk_Salisbury,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Salisbury) / sizeof(struct mux),
	.comment = "UK, Salisbury"
},
{
	.type = FE_OFDM,
	.name = "uk-SandyHeath",
	.muxes = muxlist_FE_OFDM_uk_SandyHeath,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_SandyHeath) / sizeof(struct mux),
	.comment = "UK, Sandy Heath"
},
{
	.type = FE_OFDM,
	.name = "uk-Selkirk",
	.muxes = muxlist_FE_OFDM_uk_Selkirk,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Selkirk) / sizeof(struct mux),
	.comment = "UK, Selkirk"
},
{
	.type = FE_OFDM,
	.name = "uk-Sheffield",
	.muxes = muxlist_FE_OFDM_uk_Sheffield,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Sheffield) / sizeof(struct mux),
	.comment = "UK, Sheffield"
},
{
	.type = FE_OFDM,
	.name = "uk-StocklandHill",
	.muxes = muxlist_FE_OFDM_uk_StocklandHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_StocklandHill) / sizeof(struct mux),
	.comment = "UK, Stockland Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-Storeton",
	.muxes = muxlist_FE_OFDM_uk_Storeton,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Storeton) / sizeof(struct mux),
	.comment = "UK, Storeton"
},
{
	.type = FE_OFDM,
	.name = "uk-Sudbury",
	.muxes = muxlist_FE_OFDM_uk_Sudbury,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Sudbury) / sizeof(struct mux),
	.comment = "UK, Sudbury"
},
{
	.type = FE_OFDM,
	.name = "uk-SuttonColdfield",
	.muxes = muxlist_FE_OFDM_uk_SuttonColdfield,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_SuttonColdfield) / sizeof(struct mux),
	.comment = "UK, Sutton Coldfield"
},
{
	.type = FE_OFDM,
	.name = "uk-Tacolneston",
	.muxes = muxlist_FE_OFDM_uk_Tacolneston,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Tacolneston) / sizeof(struct mux),
	.comment = "UK, Tacolneston"
},
{
	.type = FE_OFDM,
	.name = "uk-TheWrekin",
	.muxes = muxlist_FE_OFDM_uk_TheWrekin,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_TheWrekin) / sizeof(struct mux),
	.comment = "UK, The Wrekin"
},
{
	.type = FE_OFDM,
	.name = "uk-Torosay",
	.muxes = muxlist_FE_OFDM_uk_Torosay,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Torosay) / sizeof(struct mux),
	.comment = "UK, Torosay"
},
{
	.type = FE_OFDM,
	.name = "uk-TunbridgeWells",
	.muxes = muxlist_FE_OFDM_uk_TunbridgeWells,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_TunbridgeWells) / sizeof(struct mux),
	.comment = "UK, Tunbridge Wells"
},
{
	.type = FE_OFDM,
	.name = "uk-Waltham",
	.muxes = muxlist_FE_OFDM_uk_Waltham,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Waltham) / sizeof(struct mux),
	.comment = "UK, Waltham"
},
{
	.type = FE_OFDM,
	.name = "uk-Wenvoe",
	.muxes = muxlist_FE_OFDM_uk_Wenvoe,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_Wenvoe) / sizeof(struct mux),
	.comment = "UK, Wenvoe"
},
{
	.type = FE_OFDM,
	.name = "uk-WhitehawkHill",
	.muxes = muxlist_FE_OFDM_uk_WhitehawkHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_WhitehawkHill) / sizeof(struct mux),
	.comment = "UK, Whitehawk Hill"
},
{
	.type = FE_OFDM,
	.name = "uk-WinterHill",
	.muxes = muxlist_FE_OFDM_uk_WinterHill,
	.nmuxes = sizeof(muxlist_FE_OFDM_uk_WinterHill) / sizeof(struct mux),
	.comment = "UK, Winter Hill"
},
{
	.type = FE_QAM,
	.name = "at-Innsbruck",
	.muxes = muxlist_FE_QAM_at_Innsbruck,
	.nmuxes = sizeof(muxlist_FE_QAM_at_Innsbruck) / sizeof(struct mux),
	.comment = "scan config for Innsbruck Telesystem cable provider"
},
{
	.type = FE_QAM,
	.name = "at-Liwest",
	.muxes = muxlist_FE_QAM_at_Liwest,
	.nmuxes = sizeof(muxlist_FE_QAM_at_Liwest) / sizeof(struct mux),
	.comment = "Kabel Linz/AT Liwest"
},
{
	.type = FE_QAM,
	.name = "at-SalzburgAG",
	.muxes = muxlist_FE_QAM_at_SalzburgAG,
	.nmuxes = sizeof(muxlist_FE_QAM_at_SalzburgAG) / sizeof(struct mux),
	.comment = "scan config for Salzburg AG cable provider"
},
{
	.type = FE_QAM,
	.name = "at-Vienna",
	.muxes = muxlist_FE_QAM_at_Vienna,
	.nmuxes = sizeof(muxlist_FE_QAM_at_Vienna) / sizeof(struct mux),
	.comment = "Kabel Vienna"
},
{
	.type = FE_QAM,
	.name = "be-IN.DI-Integan",
	.muxes = muxlist_FE_QAM_be_IN_DI_Integan,
	.nmuxes = sizeof(muxlist_FE_QAM_be_IN_DI_Integan) / sizeof(struct mux),
	.comment = "Integan DVB-C (Belgium, IN.DI region)"
},
{
	.type = FE_QAM,
	.name = "ch-unknown",
	.muxes = muxlist_FE_QAM_ch_unknown,
	.nmuxes = sizeof(muxlist_FE_QAM_ch_unknown) / sizeof(struct mux),
	.comment = "Kabel Suisse"
},
{
	.type = FE_QAM,
	.name = "ch-Video2000",
	.muxes = muxlist_FE_QAM_ch_Video2000,
	.nmuxes = sizeof(muxlist_FE_QAM_ch_Video2000) / sizeof(struct mux),
	.comment = "Cable Video2000"
},
{
	.type = FE_QAM,
	.name = "ch-Zuerich-cablecom",
	.muxes = muxlist_FE_QAM_ch_Zuerich_cablecom,
	.nmuxes = sizeof(muxlist_FE_QAM_ch_Zuerich_cablecom) / sizeof(struct mux),
	.comment = "Kabel cablecom.ch Zuerich"
},
{
	.type = FE_QAM,
	.name = "de-Berlin",
	.muxes = muxlist_FE_QAM_de_Berlin,
	.nmuxes = sizeof(muxlist_FE_QAM_de_Berlin) / sizeof(struct mux),
	.comment = "Kabel Berlin"
},
{
	.type = FE_QAM,
	.name = "de-iesy",
	.muxes = muxlist_FE_QAM_de_iesy,
	.nmuxes = sizeof(muxlist_FE_QAM_de_iesy) / sizeof(struct mux),
	.comment = "Unity Media (iesy Hessen, ish Nordrhein-Westfalen)"
},
{
	.type = FE_QAM,
	.name = "de-Kabel_BW",
	.muxes = muxlist_FE_QAM_de_Kabel_BW,
	.nmuxes = sizeof(muxlist_FE_QAM_de_Kabel_BW) / sizeof(struct mux),
	.comment = "Kabel-BW, Stand 04/2007"
},
{
	.type = FE_QAM,
	.name = "de-Muenchen",
	.muxes = muxlist_FE_QAM_de_Muenchen,
	.nmuxes = sizeof(muxlist_FE_QAM_de_Muenchen) / sizeof(struct mux),
	.comment = "2008-04-28"
},
{
	.type = FE_QAM,
	.name = "de-neftv",
	.muxes = muxlist_FE_QAM_de_neftv,
	.nmuxes = sizeof(muxlist_FE_QAM_de_neftv) / sizeof(struct mux),
	.comment = "Cable conf for NEFtv"
},
{
	.type = FE_QAM,
	.name = "de-Primacom",
	.muxes = muxlist_FE_QAM_de_Primacom,
	.nmuxes = sizeof(muxlist_FE_QAM_de_Primacom) / sizeof(struct mux),
	.comment = "Primacom"
},
{
	.type = FE_QAM,
	.name = "de-Unitymedia",
	.muxes = muxlist_FE_QAM_de_Unitymedia,
	.nmuxes = sizeof(muxlist_FE_QAM_de_Unitymedia) / sizeof(struct mux),
	.comment = "Unitymedia"
},
{
	.type = FE_QAM,
	.name = "dk-Odense",
	.muxes = muxlist_FE_QAM_dk_Odense,
	.nmuxes = sizeof(muxlist_FE_QAM_dk_Odense) / sizeof(struct mux),
	.comment = "Glentevejs Antennelaug (Denmark / Odense)"
},
{
	.type = FE_QAM,
	.name = "es-Euskaltel",
	.muxes = muxlist_FE_QAM_es_Euskaltel,
	.nmuxes = sizeof(muxlist_FE_QAM_es_Euskaltel) / sizeof(struct mux),
	.comment = "Scan config for Euskaltel (DVB-C)"
},
{
	.type = FE_QAM,
	.name = "fi-3ktv",
	.muxes = muxlist_FE_QAM_fi_3ktv,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_3ktv) / sizeof(struct mux),
	.comment = "3KTV network reference channels"
},
{
	.type = FE_QAM,
	.name = "fi-HTV",
	.muxes = muxlist_FE_QAM_fi_HTV,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_HTV) / sizeof(struct mux),
	.comment = "HTV"
},
{
	.type = FE_QAM,
	.name = "fi-jkl",
	.muxes = muxlist_FE_QAM_fi_jkl,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_jkl) / sizeof(struct mux),
	.comment = "OnCable (Finland / Jyväskylä)"
},
{
	.type = FE_QAM,
	.name = "fi-Joensuu-Tikka",
	.muxes = muxlist_FE_QAM_fi_Joensuu_Tikka,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_Joensuu_Tikka) / sizeof(struct mux),
	.comment = "DVB-C, Tikka Media, Joensuu, Finland"
},
{
	.type = FE_QAM,
	.name = "fi-sonera",
	.muxes = muxlist_FE_QAM_fi_sonera,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_sonera) / sizeof(struct mux),
	.comment = "Sonera kaapeli-tv (Finland)"
},
{
	.type = FE_QAM,
	.name = "fi-TTV",
	.muxes = muxlist_FE_QAM_fi_TTV,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_TTV) / sizeof(struct mux),
	.comment = "TTV"
},
{
	.type = FE_QAM,
	.name = "fi-Turku",
	.muxes = muxlist_FE_QAM_fi_Turku,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_Turku) / sizeof(struct mux),
	.comment = "Turun Kaapelitelevisio Oy (Turku)"
},
{
	.type = FE_QAM,
	.name = "fi-vaasa-oncable",
	.muxes = muxlist_FE_QAM_fi_vaasa_oncable,
	.nmuxes = sizeof(muxlist_FE_QAM_fi_vaasa_oncable) / sizeof(struct mux),
	.comment = "OnCable (Finland / Vaasa)"
},
{
	.type = FE_QAM,
	.name = "fr-noos-numericable",
	.muxes = muxlist_FE_QAM_fr_noos_numericable,
	.nmuxes = sizeof(muxlist_FE_QAM_fr_noos_numericable) / sizeof(struct mux),
	.comment = "Cable en France"
},
{
	.type = FE_QAM,
	.name = "lu-Ettelbruck-ACE",
	.muxes = muxlist_FE_QAM_lu_Ettelbruck_ACE,
	.nmuxes = sizeof(muxlist_FE_QAM_lu_Ettelbruck_ACE) / sizeof(struct mux),
	.comment = "Scan config for Antenne Collective Ettelbruck a.s.b.l."
},
{
	.type = FE_QAM,
	.name = "nl-Casema",
	.muxes = muxlist_FE_QAM_nl_Casema,
	.nmuxes = sizeof(muxlist_FE_QAM_nl_Casema) / sizeof(struct mux),
	.comment = "Casema Netherlands"
},
{
	.type = FE_QAM,
	.name = "no-Oslo-CanalDigital",
	.muxes = muxlist_FE_QAM_no_Oslo_CanalDigital,
	.nmuxes = sizeof(muxlist_FE_QAM_no_Oslo_CanalDigital) / sizeof(struct mux),
	.comment = "no-oslo-CanalDigital (cable)"
},
{
	.type = FE_QAM,
	.name = "se-comhem",
	.muxes = muxlist_FE_QAM_se_comhem,
	.nmuxes = sizeof(muxlist_FE_QAM_se_comhem) / sizeof(struct mux),
	.comment = "com hem"
},
};
