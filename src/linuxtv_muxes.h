
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

static const struct mux muxes_DVBS_ABS1_75_0E[] = {
	{.freq = 12518000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12548000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12579000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12640000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12670000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12693000, .symrate = 10000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12704000, .symrate = 3900000, .fec = 9, .polarisation = 'V'},
	{.freq = 12740000, .symrate = 7408000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Agila2_C_146_0E[] = {
	{.freq = 3725000, .symrate = 2345000, .fec = 3, .polarisation = 'H'},
	{.freq = 3774000, .symrate = 3300000, .fec = 3, .polarisation = 'V'},
	{.freq = 3791000, .symrate = 3100000, .fec = 3, .polarisation = 'H'},
	{.freq = 3791000, .symrate = 10091000, .fec = 5, .polarisation = 'V'},
	{.freq = 3796000, .symrate = 3360000, .fec = 3, .polarisation = 'H'},
	{.freq = 3846000, .symrate = 6110000, .fec = 3, .polarisation = 'H'},
	{.freq = 3854000, .symrate = 2603000, .fec = 5, .polarisation = 'H'},
	{.freq = 3873000, .symrate = 2532000, .fec = 3, .polarisation = 'V'},
	{.freq = 3875000, .symrate = 3000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3880000, .symrate = 2000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3887000, .symrate = 6716000, .fec = 3, .polarisation = 'H'},
	{.freq = 3896000, .symrate = 3000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3979000, .symrate = 9628000, .fec = 3, .polarisation = 'V'},
	{.freq = 3990000, .symrate = 13330000, .fec = 3, .polarisation = 'V'},
	{.freq = 4164000, .symrate = 3735000, .fec = 7, .polarisation = 'V'},
	{.freq = 4171000, .symrate = 3213000, .fec = 3, .polarisation = 'V'},
	{.freq = 4183000, .symrate = 2170000, .fec = 3, .polarisation = 'V'},
	{.freq = 4188000, .symrate = 2170000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Amazonas_61_0W[] = {
	{.freq = 3957000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3966000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3975000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3993000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 4137000, .symrate = 3409000, .fec = 9, .polarisation = 'H'},
	{.freq = 3941000, .symrate = 3480000, .fec = 9, .polarisation = 'V'},
	{.freq = 12092000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12132000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 4174000, .symrate = 3330000, .fec = 9, .polarisation = 'H'},
	{.freq = 11128000, .symrate = 6666000, .fec = 9, .polarisation = 'V'},
	{.freq = 11185000, .symrate = 11800000, .fec = 9, .polarisation = 'V'},
	{.freq = 11049000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12052000, .symrate = 27000000, .fec = 9, .polarisation = 'V'},
	{.freq = 10975000, .symrate = 27000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3631000, .symrate = 2785000, .fec = 9, .polarisation = 'H'},
	{.freq = 10975000, .symrate = 26666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3677000, .symrate = 4400000, .fec = 9, .polarisation = 'H'},
	{.freq = 3985000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 11810000, .symrate = 6666000, .fec = 9, .polarisation = 'V'},
	{.freq = 11921000, .symrate = 21740000, .fec = 9, .polarisation = 'V'},
	{.freq = 11943000, .symrate = 4750000, .fec = 9, .polarisation = 'V'},
	{.freq = 4168000, .symrate = 7307000, .fec = 9, .polarisation = 'H'},
	{.freq = 12052000, .symrate = 26667000, .fec = 9, .polarisation = 'H'},
	{.freq = 12172000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 12092000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 11175000, .symrate = 28880000, .fec = 9, .polarisation = 'H'},
	{.freq = 4154000, .symrate = 9615000, .fec = 9, .polarisation = 'H'},
	{.freq = 11099000, .symrate = 7576000, .fec = 9, .polarisation = 'V'},
	{.freq = 11885000, .symrate = 4890000, .fec = 9, .polarisation = 'H'},
	{.freq = 11882000, .symrate = 11343000, .fec = 9, .polarisation = 'V'},
	{.freq = 11133000, .symrate = 3111000, .fec = 9, .polarisation = 'V'},
	{.freq = 11078000, .symrate = 1862000, .fec = 9, .polarisation = 'V'},
	{.freq = 11912000, .symrate = 2222000, .fec = 9, .polarisation = 'H'},
	{.freq = 11906000, .symrate = 2220000, .fec = 9, .polarisation = 'H'},
	{.freq = 11108000, .symrate = 2170000, .fec = 9, .polarisation = 'V'},
	{.freq = 12132000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 11135000, .symrate = 26667000, .fec = 9, .polarisation = 'H'},
	{.freq = 4144000, .symrate = 4540000, .fec = 9, .polarisation = 'V'},
	{.freq = 11808000, .symrate = 11111000, .fec = 9, .polarisation = 'H'},
	{.freq = 11972000, .symrate = 26667000, .fec = 9, .polarisation = 'H'},
	{.freq = 4156000, .symrate = 4540000, .fec = 9, .polarisation = 'V'},
	{.freq = 4149000, .symrate = 4540000, .fec = 9, .polarisation = 'V'},
	{.freq = 3948000, .symrate = 13300000, .fec = 9, .polarisation = 'H'},
	{.freq = 4178000, .symrate = 3333000, .fec = 9, .polarisation = 'H'},
	{.freq = 11844000, .symrate = 16600000, .fec = 9, .polarisation = 'V'},
	{.freq = 11095000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11790000, .symrate = 3600000, .fec = 9, .polarisation = 'H'},
	{.freq = 12172000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11015000, .symrate = 26666000, .fec = 9, .polarisation = 'H'},
	{.freq = 11055000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 12012000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_AMC1_103w[] = {
	{.freq = 11942000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12100000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_AMC2_85w[] = {
	{.freq = 11731000, .symrate = 13021000, .fec = 9, .polarisation = 'H'},
	{.freq = 11744000, .symrate = 13021000, .fec = 9, .polarisation = 'H'},
	{.freq = 11771000, .symrate = 13021000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_AMC3_87w[] = {
	{.freq = 11716000, .symrate = 4859000, .fec = 9, .polarisation = 'H'},
	{.freq = 12142000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12147000, .symrate = 4340000, .fec = 9, .polarisation = 'H'},
	{.freq = 12159000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 12165000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 12172000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 12182000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_AMC4_101w[] = {
	{.freq = 11573000, .symrate = 7234000, .fec = 9, .polarisation = 'V'},
	{.freq = 11655000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11708000, .symrate = 2170000, .fec = 9, .polarisation = 'V'},
	{.freq = 11822000, .symrate = 5700000, .fec = 9, .polarisation = 'H'},
	{.freq = 11860000, .symrate = 28138000, .fec = 9, .polarisation = 'H'},
	{.freq = 12120000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12169000, .symrate = 3003000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_AMC5_79w[] = {
	{.freq = 11742000, .symrate = 11110000, .fec = 9, .polarisation = 'V'},
	{.freq = 12182000, .symrate = 23000000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_AMC6_72w[] = {
	{.freq = 11482000, .symrate = 2656000, .fec = 9, .polarisation = 'V'},
	{.freq = 11494000, .symrate = 6560000, .fec = 9, .polarisation = 'V'},
	{.freq = 11499000, .symrate = 2964000, .fec = 9, .polarisation = 'V'},
	{.freq = 11505000, .symrate = 2963000, .fec = 9, .polarisation = 'V'},
	{.freq = 11546000, .symrate = 12000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11548000, .symrate = 3002000, .fec = 9, .polarisation = 'H'},
	{.freq = 11552000, .symrate = 3002000, .fec = 9, .polarisation = 'H'},
	{.freq = 11557000, .symrate = 4392000, .fec = 9, .polarisation = 'H'},
	{.freq = 11563000, .symrate = 4392000, .fec = 9, .polarisation = 'H'},
	{.freq = 11570000, .symrate = 4392000, .fec = 9, .polarisation = 'H'},
	{.freq = 11576000, .symrate = 4392000, .fec = 9, .polarisation = 'H'},
	{.freq = 11586000, .symrate = 2652000, .fec = 9, .polarisation = 'H'},
	{.freq = 11603000, .symrate = 8500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11605000, .symrate = 3600000, .fec = 9, .polarisation = 'V'},
	{.freq = 11611000, .symrate = 3400000, .fec = 9, .polarisation = 'V'},
	{.freq = 11628000, .symrate = 6560000, .fec = 9, .polarisation = 'V'},
	{.freq = 11637000, .symrate = 2800000, .fec = 9, .polarisation = 'V'},
	{.freq = 11641000, .symrate = 3702000, .fec = 9, .polarisation = 'V'},
	{.freq = 11648000, .symrate = 7500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11667000, .symrate = 7400000, .fec = 9, .polarisation = 'V'},
	{.freq = 11674000, .symrate = 4000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11680000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 11703000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11709000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11715000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11746000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11748000, .symrate = 14015000, .fec = 9, .polarisation = 'H'},
	{.freq = 11752000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11763000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11817000, .symrate = 5000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11874000, .symrate = 4000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11986000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11995000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12004000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12013000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12025000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12031000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12046000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 12055000, .symrate = 6890000, .fec = 9, .polarisation = 'V'},
	{.freq = 12144000, .symrate = 2573000, .fec = 9, .polarisation = 'V'},
	{.freq = 12188000, .symrate = 6511000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_AMC9_83w[] = {
	{.freq = 11745000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 11751000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 11757000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 11763000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 11769000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 11775000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 11826000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 11864000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11871000, .symrate = 13000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11889000, .symrate = 13025000, .fec = 9, .polarisation = 'H'},
	{.freq = 11926000, .symrate = 6511000, .fec = 9, .polarisation = 'V'},
	{.freq = 11953000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 5000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12002000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12011000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Amos_4w[] = {
	{.freq = 10722000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10722000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10722000, .symrate = 30000000, .fec = 2, .polarisation = 'V'},
	{.freq = 10758000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10758000, .symrate = 30000000, .fec = 2, .polarisation = 'V'},
	{.freq = 10759000, .symrate = 30000000, .fec = 3, .polarisation = 'H'},
	{.freq = 10806000, .symrate = 27500000, .fec = 5, .polarisation = 'H'},
	{.freq = 10806000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10842000, .symrate = 27500000, .fec = 7, .polarisation = 'H'},
	{.freq = 10842000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10842000, .symrate = 30000000, .fec = 2, .polarisation = 'V'},
	{.freq = 10890000, .symrate = 27500000, .fec = 7, .polarisation = 'H'},
	{.freq = 10890000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10925000, .symrate = 27500000, .fec = 7, .polarisation = 'H'},
	{.freq = 10925000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10972000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11008000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11015000, .symrate = 2295000, .fec = 3, .polarisation = 'H'},
	{.freq = 11123000, .symrate = 1850000, .fec = 7, .polarisation = 'H'},
	{.freq = 11167000, .symrate = 12500000, .fec = 5, .polarisation = 'H'},
	{.freq = 11179000, .symrate = 6666000, .fec = 3, .polarisation = 'H'},
	{.freq = 11260000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11304000, .symrate = 19540000, .fec = 3, .polarisation = 'H'},
	{.freq = 11319000, .symrate = 2750000, .fec = 3, .polarisation = 'H'},
	{.freq = 11329000, .symrate = 3333000, .fec = 3, .polarisation = 'H'},
	{.freq = 11333000, .symrate = 3500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11347000, .symrate = 3350000, .fec = 3, .polarisation = 'H'},
	{.freq = 11384000, .symrate = 19000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11411000, .symrate = 7925000, .fec = 5, .polarisation = 'H'},
	{.freq = 11429000, .symrate = 5925000, .fec = 3, .polarisation = 'H'},
	{.freq = 11435000, .symrate = 2089000, .fec = 3, .polarisation = 'H'},
	{.freq = 11474000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11510000, .symrate = 30000000, .fec = 2, .polarisation = 'V'},
	{.freq = 11558000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11559000, .symrate = 13400000, .fec = 7, .polarisation = 'H'},
	{.freq = 11572000, .symrate = 8888000, .fec = 3, .polarisation = 'H'},
	{.freq = 11592000, .symrate = 21350000, .fec = 3, .polarisation = 'H'},
	{.freq = 11593000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11625000, .symrate = 3000000, .fec = 3, .polarisation = 'V'},
	{.freq = 11630000, .symrate = 2963000, .fec = 3, .polarisation = 'H'},
	{.freq = 11630000, .symrate = 3000000, .fec = 3, .polarisation = 'V'},
	{.freq = 11637000, .symrate = 1480000, .fec = 3, .polarisation = 'V'},
	{.freq = 11647000, .symrate = 9167000, .fec = 3, .polarisation = 'H'},
	{.freq = 11647000, .symrate = 8518000, .fec = 3, .polarisation = 'V'},
	{.freq = 11654000, .symrate = 2000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11658000, .symrate = 8520000, .fec = 5, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Anik_F1_107_3W[] = {
	{.freq = 12002000, .symrate = 19980000, .fec = 9, .polarisation = 'H'},
	{.freq = 12063000, .symrate = 19980000, .fec = 9, .polarisation = 'H'},
	{.freq = 12155000, .symrate = 22500000, .fec = 9, .polarisation = 'H'},
	{.freq = 12185000, .symrate = 19980000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Apstar6_C_134_0E[] = {
	{.freq = 3766000, .symrate = 4285000, .fec = 3, .polarisation = 'H'},
	{.freq = 3840000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 3250000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_AsiaSat2_C_100_5E[] = {
	{.freq = 3660000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3706000, .symrate = 4167000, .fec = 5, .polarisation = 'H'},
	{.freq = 3732000, .symrate = 8889000, .fec = 2, .polarisation = 'H'},
	{.freq = 3767000, .symrate = 6527000, .fec = 1, .polarisation = 'V'},
	{.freq = 3778000, .symrate = 3333000, .fec = 3, .polarisation = 'V'},
	{.freq = 3786000, .symrate = 6000000, .fec = 7, .polarisation = 'H'},
	{.freq = 3790000, .symrate = 2444000, .fec = 3, .polarisation = 'V'},
	{.freq = 3794000, .symrate = 4640000, .fec = 3, .polarisation = 'H'},
	{.freq = 3820000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3840000, .symrate = 30000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3880000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3960000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3980000, .symrate = 28100000, .fec = 3, .polarisation = 'V'},
	{.freq = 4000000, .symrate = 28125000, .fec = 3, .polarisation = 'H'},
	{.freq = 4148000, .symrate = 11852000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_AsiaSat3S_C_105_5E[] = {
	{.freq = 3680000, .symrate = 26670000, .fec = 3, .polarisation = 'H'},
	{.freq = 3706000, .symrate = 6000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3725000, .symrate = 4450000, .fec = 3, .polarisation = 'V'},
	{.freq = 3729000, .symrate = 13650000, .fec = 3, .polarisation = 'H'},
	{.freq = 3733000, .symrate = 6500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3739000, .symrate = 2815000, .fec = 3, .polarisation = 'V'},
	{.freq = 3746000, .symrate = 2625000, .fec = 3, .polarisation = 'V'},
	{.freq = 3750000, .symrate = 2821000, .fec = 3, .polarisation = 'V'},
	{.freq = 3755000, .symrate = 4418000, .fec = 7, .polarisation = 'V'},
	{.freq = 3760000, .symrate = 26000000, .fec = 7, .polarisation = 'H'},
	{.freq = 3780000, .symrate = 28100000, .fec = 3, .polarisation = 'V'},
	{.freq = 3786000, .symrate = 5700000, .fec = 3, .polarisation = 'H'},
	{.freq = 3820000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3840000, .symrate = 26850000, .fec = 7, .polarisation = 'H'},
	{.freq = 3860000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3880000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3940000, .symrate = 28100000, .fec = 3, .polarisation = 'V'},
	{.freq = 3960000, .symrate = 28100000, .fec = 3, .polarisation = 'H'},
	{.freq = 3980000, .symrate = 28100000, .fec = 3, .polarisation = 'V'},
	{.freq = 4000000, .symrate = 26850000, .fec = 7, .polarisation = 'H'},
	{.freq = 4020000, .symrate = 27250000, .fec = 3, .polarisation = 'V'},
	{.freq = 4040000, .symrate = 20400000, .fec = 1, .polarisation = 'H'},
	{.freq = 4071000, .symrate = 14240000, .fec = 3, .polarisation = 'H'},
	{.freq = 4091000, .symrate = 13333000, .fec = 3, .polarisation = 'V'},
	{.freq = 4095000, .symrate = 5554000, .fec = 3, .polarisation = 'H'},
	{.freq = 4102000, .symrate = 2894000, .fec = 3, .polarisation = 'V'},
	{.freq = 4106000, .symrate = 2894000, .fec = 3, .polarisation = 'V'},
	{.freq = 4111000, .symrate = 13650000, .fec = 3, .polarisation = 'H'},
	{.freq = 4114000, .symrate = 7000000, .fec = 3, .polarisation = 'V'},
	{.freq = 4129000, .symrate = 13600000, .fec = 3, .polarisation = 'H'},
	{.freq = 4140000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 4180000, .symrate = 26666000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Asiasat4_C_122_2E[] = {
	{.freq = 3760000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3820000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3856000, .symrate = 2170000, .fec = 3, .polarisation = 'H'},
	{.freq = 3880000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3920000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3960000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 4180000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Astra_19_2E[] = {
	{.freq = 12551500, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Astra_23_5E[] = {
	{.freq = 10758000, .symrate = 22000000, .fec = 7, .polarisation = 'V'},
	{.freq = 10788000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10802000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10842000, .symrate = 13333000, .fec = 5, .polarisation = 'V'},
	{.freq = 10862000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11475000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11515000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11675000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11739000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11798000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11836000, .symrate = 29900000, .fec = 3, .polarisation = 'H'},
	{.freq = 11875000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11914000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11992000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12012000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12070000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12168000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12525000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12525000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12565000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12565000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12630000, .symrate = 6000000, .fec = 3, .polarisation = 'V'},
	{.freq = 12631000, .symrate = 6666000, .fec = 7, .polarisation = 'H'},
	{.freq = 12636000, .symrate = 2277000, .fec = 5, .polarisation = 'V'},
	{.freq = 12641000, .symrate = 2220000, .fec = 7, .polarisation = 'H'},
	{.freq = 12644000, .symrate = 2170000, .fec = 3, .polarisation = 'H'},
	{.freq = 12649000, .symrate = 5600000, .fec = 5, .polarisation = 'H'},
	{.freq = 12661000, .symrate = 2170000, .fec = 5, .polarisation = 'H'},
	{.freq = 12685000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12696000, .symrate = 3255000, .fec = 1, .polarisation = 'H'},
	{.freq = 12711000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 12725000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Astra_28_2E[] = {
	{.freq = 11720000, .symrate = 29500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11740000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11758000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11778000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11798000, .symrate = 29500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11817000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11856000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11876000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11895000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11914000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11934000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11954000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12051000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12129000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12148000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12168000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12226000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12246000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12422000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12480000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11973000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11992000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12012000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12032000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12070000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12090000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12110000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12188000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12207000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12266000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12285000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12304000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12324000, .symrate = 29500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12344000, .symrate = 29500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12363000, .symrate = 29500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12382000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12402000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12441000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12460000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 10714000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10729000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10744000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10758000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10773000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10788000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10803000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10818000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10832000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10847000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10862000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10876000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10891000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10906000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 10921000, .symrate = 22000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10936000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 11222170, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11223670, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11259000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11261000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11307000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11307000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11343000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11344000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11390000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11390000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11426000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11426000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11469000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11488000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11508000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11527000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11546000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11565000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11585000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11603850, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11623000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11642000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11661540, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11680770, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12524000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12524000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12560000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 12560000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12596000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12607000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12629000, .symrate = 6111000, .fec = 3, .polarisation = 'V'},
	{.freq = 12692000, .symrate = 19532000, .fec = 1, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Atlantic_Bird_1_12_5W[] = {
	{.freq = 11099000, .symrate = 2000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11132000, .symrate = 3255000, .fec = 3, .polarisation = 'H'},
	{.freq = 11136000, .symrate = 3700000, .fec = 5, .polarisation = 'V'},
	{.freq = 11174000, .symrate = 15190000, .fec = 3, .polarisation = 'H'},
	{.freq = 11179000, .symrate = 22400000, .fec = 2, .polarisation = 'V'},
	{.freq = 11188000, .symrate = 5722000, .fec = 5, .polarisation = 'H'},
	{.freq = 11332000, .symrate = 6428000, .fec = 2, .polarisation = 'H'},
	{.freq = 11337000, .symrate = 1923000, .fec = 7, .polarisation = 'H'},
	{.freq = 11340000, .symrate = 2279000, .fec = 5, .polarisation = 'H'},
	{.freq = 11355000, .symrate = 11781000, .fec = 7, .polarisation = 'H'},
	{.freq = 11408000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11428000, .symrate = 30000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11595000, .symrate = 27500000, .fec = 5, .polarisation = 'V'},
	{.freq = 11622000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 11643000, .symrate = 2398000, .fec = 9, .polarisation = 'H'},
	{.freq = 11645000, .symrate = 4790000, .fec = 9, .polarisation = 'V'},
	{.freq = 11651000, .symrate = 3688000, .fec = 9, .polarisation = 'V'},
	{.freq = 12515000, .symrate = 17455000, .fec = 3, .polarisation = 'H'},
	{.freq = 12535000, .symrate = 2000000, .fec = 3, .polarisation = 'V'},
	{.freq = 12545000, .symrate = 17455000, .fec = 3, .polarisation = 'H'},
	{.freq = 12594000, .symrate = 2170000, .fec = 3, .polarisation = 'H'},
	{.freq = 12597000, .symrate = 1730000, .fec = 3, .polarisation = 'H'},
	{.freq = 12604000, .symrate = 1481000, .fec = 3, .polarisation = 'H'},
	{.freq = 12655000, .symrate = 4285000, .fec = 3, .polarisation = 'H'},
	{.freq = 12659000, .symrate = 2141000, .fec = 3, .polarisation = 'H'},
	{.freq = 12662000, .symrate = 3928000, .fec = 3, .polarisation = 'V'},
	{.freq = 12720000, .symrate = 1808000, .fec = 3, .polarisation = 'V'},
	{.freq = 12743000, .symrate = 3214000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Atlantic_Bird_3_5_0W[] = {
	{.freq = 11096000, .symrate = 29950000, .fec = 7, .polarisation = 'V'},
	{.freq = 11591000, .symrate = 20000000, .fec = 2, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_BrasilSat_B1_75_0W[] = {
	{.freq = 3648000, .symrate = 4285000, .fec = 9, .polarisation = 'V'},
	{.freq = 3657000, .symrate = 6620000, .fec = 9, .polarisation = 'V'},
	{.freq = 3653000, .symrate = 4710000, .fec = 9, .polarisation = 'V'},
	{.freq = 3655000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3629000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3711000, .symrate = 3200000, .fec = 9, .polarisation = 'H'},
	{.freq = 3644000, .symrate = 4440000, .fec = 9, .polarisation = 'V'},
	{.freq = 3638000, .symrate = 4440000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_BrasilSat_B2_65_0W[] = {
	{.freq = 3745000, .symrate = 3540000, .fec = 9, .polarisation = 'H'},
	{.freq = 4008000, .symrate = 3333000, .fec = 9, .polarisation = 'H'},
	{.freq = 4011000, .symrate = 5000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3792000, .symrate = 3393000, .fec = 9, .polarisation = 'V'},
	{.freq = 4104000, .symrate = 3214000, .fec = 9, .polarisation = 'V'},
	{.freq = 4097000, .symrate = 6667000, .fec = 9, .polarisation = 'V'},
	{.freq = 3905000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3935000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3890000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3925000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 4112000, .symrate = 4285000, .fec = 9, .polarisation = 'V'},
	{.freq = 3650000, .symrate = 4440000, .fec = 9, .polarisation = 'V'},
	{.freq = 3736000, .symrate = 1808000, .fec = 9, .polarisation = 'H'},
	{.freq = 3787000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3876000, .symrate = 2740000, .fec = 9, .polarisation = 'V'},
	{.freq = 4117000, .symrate = 2963000, .fec = 9, .polarisation = 'V'},
	{.freq = 3815000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3793000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3721000, .symrate = 2963000, .fec = 9, .polarisation = 'H'},
	{.freq = 3684000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3915000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3766000, .symrate = 3336000, .fec = 9, .polarisation = 'H'},
	{.freq = 3847000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 3706000, .symrate = 2462000, .fec = 9, .polarisation = 'H'},
	{.freq = 3667000, .symrate = 7236000, .fec = 9, .polarisation = 'H'},
	{.freq = 3771000, .symrate = 1480000, .fec = 9, .polarisation = 'V'},
	{.freq = 3734000, .symrate = 2852000, .fec = 9, .polarisation = 'V'},
	{.freq = 3810000, .symrate = 13333000, .fec = 9, .polarisation = 'H'},
	{.freq = 3762000, .symrate = 2222000, .fec = 9, .polarisation = 'H'},
	{.freq = 3834000, .symrate = 3572000, .fec = 9, .polarisation = 'H'},
	{.freq = 3850000, .symrate = 1570000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_BrasilSat_B3_84_0W[] = {
	{.freq = 3728000, .symrate = 4340000, .fec = 9, .polarisation = 'H'},
	{.freq = 3698000, .symrate = 3333000, .fec = 9, .polarisation = 'V'},
	{.freq = 4054000, .symrate = 1287000, .fec = 9, .polarisation = 'V'},
	{.freq = 3745000, .symrate = 4300000, .fec = 9, .polarisation = 'H'},
	{.freq = 3738000, .symrate = 4708000, .fec = 9, .polarisation = 'V'},
	{.freq = 3923000, .symrate = 1808000, .fec = 9, .polarisation = 'V'},
	{.freq = 3768000, .symrate = 8000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3955000, .symrate = 4340000, .fec = 9, .polarisation = 'V'},
	{.freq = 4167000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 3791000, .symrate = 3330000, .fec = 9, .polarisation = 'V'},
	{.freq = 3866000, .symrate = 4425000, .fec = 9, .polarisation = 'H'},
	{.freq = 3932000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 4132000, .symrate = 2532000, .fec = 9, .polarisation = 'V'},
	{.freq = 3906000, .symrate = 3928000, .fec = 9, .polarisation = 'H'},
	{.freq = 3858000, .symrate = 4288000, .fec = 9, .polarisation = 'V'},
	{.freq = 3665000, .symrate = 3177000, .fec = 9, .polarisation = 'V'},
	{.freq = 3732000, .symrate = 3214000, .fec = 9, .polarisation = 'V'},
	{.freq = 3774000, .symrate = 3330000, .fec = 9, .polarisation = 'H'},
	{.freq = 3927000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 3710000, .symrate = 3261000, .fec = 9, .polarisation = 'H'},
	{.freq = 3883000, .symrate = 4278000, .fec = 9, .polarisation = 'H'},
	{.freq = 3770000, .symrate = 3333000, .fec = 9, .polarisation = 'H'},
	{.freq = 3754000, .symrate = 5000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3764000, .symrate = 4285000, .fec = 9, .polarisation = 'H'},
	{.freq = 3910000, .symrate = 3616000, .fec = 9, .polarisation = 'H'},
	{.freq = 3705000, .symrate = 4280000, .fec = 9, .polarisation = 'H'},
	{.freq = 3852000, .symrate = 3806000, .fec = 9, .polarisation = 'V'},
	{.freq = 3845000, .symrate = 10127000, .fec = 9, .polarisation = 'H'},
	{.freq = 3936000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 3653000, .symrate = 3807000, .fec = 9, .polarisation = 'V'},
	{.freq = 3751000, .symrate = 3565000, .fec = 9, .polarisation = 'H'},
	{.freq = 3871000, .symrate = 4435000, .fec = 9, .polarisation = 'H'},
	{.freq = 3895000, .symrate = 4430000, .fec = 9, .polarisation = 'H'},
	{.freq = 3936000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 3800000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 3855000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4171000, .symrate = 2170000, .fec = 9, .polarisation = 'H'},
	{.freq = 3716000, .symrate = 4800000, .fec = 9, .polarisation = 'H'},
	{.freq = 4068000, .symrate = 2600000, .fec = 9, .polarisation = 'H'},
	{.freq = 4070000, .symrate = 2964000, .fec = 9, .polarisation = 'V'},
	{.freq = 3949000, .symrate = 4340000, .fec = 9, .polarisation = 'V'},
	{.freq = 3684000, .symrate = 3200000, .fec = 9, .polarisation = 'V'},
	{.freq = 4169000, .symrate = 8140000, .fec = 9, .polarisation = 'V'},
	{.freq = 4144000, .symrate = 2734000, .fec = 9, .polarisation = 'H'},
	{.freq = 3736000, .symrate = 4285000, .fec = 9, .polarisation = 'H'},
	{.freq = 3790000, .symrate = 10444000, .fec = 9, .polarisation = 'H'},
	{.freq = 3724000, .symrate = 2075000, .fec = 9, .polarisation = 'V'},
	{.freq = 4121000, .symrate = 2500000, .fec = 9, .polarisation = 'H'},
	{.freq = 3629000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3970000, .symrate = 4445000, .fec = 9, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 3300000, .fec = 9, .polarisation = 'V'},
	{.freq = 3916000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 4155000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 3692000, .symrate = 3330000, .fec = 9, .polarisation = 'V'},
	{.freq = 3644000, .symrate = 4687000, .fec = 9, .polarisation = 'V'},
	{.freq = 4087000, .symrate = 17200000, .fec = 9, .polarisation = 'H'},
	{.freq = 3889000, .symrate = 4440000, .fec = 9, .polarisation = 'H'},
	{.freq = 4176000, .symrate = 3515000, .fec = 9, .polarisation = 'V'},
	{.freq = 3829000, .symrate = 4340000, .fec = 9, .polarisation = 'V'},
	{.freq = 3805000, .symrate = 2662000, .fec = 9, .polarisation = 'V'},
	{.freq = 3943000, .symrate = 2460000, .fec = 9, .polarisation = 'V'},
	{.freq = 3757000, .symrate = 3565000, .fec = 9, .polarisation = 'H'},
	{.freq = 3696000, .symrate = 1808000, .fec = 9, .polarisation = 'H'},
	{.freq = 3989000, .symrate = 2666000, .fec = 9, .polarisation = 'V'},
	{.freq = 3690000, .symrate = 3200000, .fec = 9, .polarisation = 'V'},
	{.freq = 3657000, .symrate = 3600000, .fec = 9, .polarisation = 'V'},
	{.freq = 3996000, .symrate = 2300000, .fec = 9, .polarisation = 'V'},
	{.freq = 3926000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4136000, .symrate = 2142000, .fec = 9, .polarisation = 'H'},
	{.freq = 3675000, .symrate = 4285000, .fec = 9, .polarisation = 'V'},
	{.freq = 3931000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3786000, .symrate = 4286000, .fec = 9, .polarisation = 'V'},
	{.freq = 3688000, .symrate = 2308000, .fec = 9, .polarisation = 'V'},
	{.freq = 4075000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 3911000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 3824000, .symrate = 3002000, .fec = 9, .polarisation = 'H'},
	{.freq = 3778000, .symrate = 6850000, .fec = 9, .polarisation = 'V'},
	{.freq = 3710000, .symrate = 12960000, .fec = 9, .polarisation = 'V'},
	{.freq = 3877000, .symrate = 4450000, .fec = 9, .polarisation = 'H'},
	{.freq = 3940000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 4064000, .symrate = 3300000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_BrasilSat_B4_70_0W[] = {
	{.freq = 3951000, .symrate = 3214000, .fec = 9, .polarisation = 'H'},
	{.freq = 3714000, .symrate = 4400000, .fec = 9, .polarisation = 'V'},
	{.freq = 3672000, .symrate = 4713000, .fec = 9, .polarisation = 'H'},
	{.freq = 3955000, .symrate = 4400000, .fec = 9, .polarisation = 'H'},
	{.freq = 3965000, .symrate = 2930000, .fec = 9, .polarisation = 'V'},
	{.freq = 3628000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3644000, .symrate = 3214000, .fec = 9, .polarisation = 'V'},
	{.freq = 3650000, .symrate = 4285000, .fec = 9, .polarisation = 'H'},
	{.freq = 3688000, .symrate = 6000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3680000, .symrate = 6000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3662000, .symrate = 4606000, .fec = 9, .polarisation = 'V'},
	{.freq = 3945000, .symrate = 3214000, .fec = 9, .polarisation = 'H'},
	{.freq = 3640000, .symrate = 3263000, .fec = 9, .polarisation = 'H'},
	{.freq = 3979000, .symrate = 3617000, .fec = 9, .polarisation = 'V'},
	{.freq = 3752000, .symrate = 6220000, .fec = 9, .polarisation = 'V'},
	{.freq = 3685000, .symrate = 4500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3650000, .symrate = 4400000, .fec = 9, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 2170000, .fec = 9, .polarisation = 'H'},
	{.freq = 4010000, .symrate = 13021000, .fec = 9, .polarisation = 'H'},
	{.freq = 3665000, .symrate = 4700000, .fec = 9, .polarisation = 'H'},
	{.freq = 3637000, .symrate = 2228000, .fec = 9, .polarisation = 'H'},
	{.freq = 3708000, .symrate = 3928000, .fec = 9, .polarisation = 'V'},
	{.freq = 4047000, .symrate = 7143000, .fec = 9, .polarisation = 'V'},
	{.freq = 3672000, .symrate = 8454000, .fec = 9, .polarisation = 'V'},
	{.freq = 3820000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3940000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3964000, .symrate = 1875000, .fec = 9, .polarisation = 'H'},
	{.freq = 3695000, .symrate = 3598000, .fec = 9, .polarisation = 'H'},
	{.freq = 3900000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3645000, .symrate = 3520000, .fec = 9, .polarisation = 'H'},
	{.freq = 3983000, .symrate = 1630000, .fec = 9, .polarisation = 'V'},
	{.freq = 3631000, .symrate = 4687000, .fec = 9, .polarisation = 'H'},
	{.freq = 3874000, .symrate = 5926000, .fec = 9, .polarisation = 'V'},
	{.freq = 3700000, .symrate = 9123000, .fec = 9, .polarisation = 'V'},
	{.freq = 3997000, .symrate = 2300000, .fec = 9, .polarisation = 'V'},
	{.freq = 3973000, .symrate = 4338000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Chinastar1_C_87_5E[] = {
	{.freq = 3734000, .symrate = 5925000, .fec = 3, .polarisation = 'H'},
	{.freq = 4076000, .symrate = 2532000, .fec = 3, .polarisation = 'H'},
	{.freq = 4081000, .symrate = 4687000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Estrela_do_Sul_63_0W[] = {
	{.freq = 11892000, .symrate = 2964000, .fec = 9, .polarisation = 'V'},
	{.freq = 11987000, .symrate = 3330000, .fec = 9, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 26660000, .fec = 9, .polarisation = 'H'},
	{.freq = 11830000, .symrate = 6000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11603000, .symrate = 3124000, .fec = 9, .polarisation = 'V'},
	{.freq = 11598000, .symrate = 3124000, .fec = 9, .polarisation = 'V'},
	{.freq = 11803000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 11958000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 11610000, .symrate = 3124000, .fec = 9, .polarisation = 'V'},
	{.freq = 11861000, .symrate = 2964000, .fec = 9, .polarisation = 'V'},
	{.freq = 11879000, .symrate = 2964000, .fec = 9, .polarisation = 'V'},
	{.freq = 11903000, .symrate = 2362000, .fec = 9, .polarisation = 'V'},
	{.freq = 11817000, .symrate = 6666000, .fec = 9, .polarisation = 'V'},
	{.freq = 11577000, .symrate = 3124000, .fec = 9, .polarisation = 'V'},
	{.freq = 11582000, .symrate = 3124000, .fec = 9, .polarisation = 'V'},
	{.freq = 11982000, .symrate = 8888000, .fec = 9, .polarisation = 'H'},
	{.freq = 11888000, .symrate = 2392000, .fec = 9, .polarisation = 'V'},
	{.freq = 11898000, .symrate = 2480000, .fec = 9, .polarisation = 'V'},
	{.freq = 11871000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11795000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 11845000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 11640000, .symrate = 18100000, .fec = 9, .polarisation = 'V'},
	{.freq = 11875000, .symrate = 3333000, .fec = 9, .polarisation = 'V'},
	{.freq = 11543000, .symrate = 10410000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Eurobird1_28_5E[] = {
	{.freq = 11623000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11224000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11527000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Eurobird9_9_0E[] = {
	{.freq = 11727000, .symrate = 27500000, .fec = 5, .polarisation = 'V'},
	{.freq = 11747000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11766000, .symrate = 27500000, .fec = 5, .polarisation = 'V'},
	{.freq = 11785000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11804000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11823000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11843000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11881000, .symrate = 26700000, .fec = 3, .polarisation = 'V'},
	{.freq = 11919000, .symrate = 27500000, .fec = 5, .polarisation = 'V'},
	{.freq = 11938000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11996000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12054000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12092000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_EutelsatW2_16E[] = {
	{.freq = 10957000, .symrate = 2821000, .fec = 9, .polarisation = 'H'},
	{.freq = 10968000, .symrate = 6400000, .fec = 9, .polarisation = 'H'},
	{.freq = 10972000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 10976000, .symrate = 6400000, .fec = 9, .polarisation = 'H'},
	{.freq = 10989000, .symrate = 6400000, .fec = 9, .polarisation = 'H'},
	{.freq = 10997000, .symrate = 6400000, .fec = 9, .polarisation = 'H'},
	{.freq = 11005000, .symrate = 6400000, .fec = 9, .polarisation = 'H'},
	{.freq = 11011000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11015000, .symrate = 6400000, .fec = 9, .polarisation = 'H'},
	{.freq = 11025000, .symrate = 2894000, .fec = 9, .polarisation = 'H'},
	{.freq = 11046000, .symrate = 10555000, .fec = 9, .polarisation = 'H'},
	{.freq = 11057000, .symrate = 3327000, .fec = 9, .polarisation = 'H'},
	{.freq = 11061000, .symrate = 5722000, .fec = 9, .polarisation = 'H'},
	{.freq = 11078000, .symrate = 5208000, .fec = 9, .polarisation = 'H'},
	{.freq = 11092000, .symrate = 32000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11094000, .symrate = 2734000, .fec = 9, .polarisation = 'H'},
	{.freq = 11132000, .symrate = 14185000, .fec = 9, .polarisation = 'V'},
	{.freq = 11178000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11192000, .symrate = 2667000, .fec = 9, .polarisation = 'H'},
	{.freq = 11267000, .symrate = 2170000, .fec = 9, .polarisation = 'H'},
	{.freq = 11276000, .symrate = 11100000, .fec = 9, .polarisation = 'H'},
	{.freq = 11294000, .symrate = 13333000, .fec = 9, .polarisation = 'H'},
	{.freq = 11304000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11324000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 11428000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11449000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 11471000, .symrate = 29950000, .fec = 9, .polarisation = 'V'},
	{.freq = 11492000, .symrate = 29950000, .fec = 9, .polarisation = 'H'},
	{.freq = 11513000, .symrate = 29950000, .fec = 9, .polarisation = 'V'},
	{.freq = 11534000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11554000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11575000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11594000, .symrate = 28800000, .fec = 9, .polarisation = 'V'},
	{.freq = 11617000, .symrate = 29950000, .fec = 9, .polarisation = 'H'},
	{.freq = 11634000, .symrate = 17578000, .fec = 9, .polarisation = 'V'},
	{.freq = 11658000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11659000, .symrate = 17578000, .fec = 9, .polarisation = 'V'},
	{.freq = 11682000, .symrate = 14468000, .fec = 9, .polarisation = 'V'},
	{.freq = 12538000, .symrate = 4340000, .fec = 9, .polarisation = 'V'},
	{.freq = 12549000, .symrate = 2894000, .fec = 9, .polarisation = 'V'},
	{.freq = 12555000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12557000, .symrate = 2156000, .fec = 9, .polarisation = 'V'},
	{.freq = 12562000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12563000, .symrate = 2222000, .fec = 9, .polarisation = 'V'},
	{.freq = 12568000, .symrate = 3703000, .fec = 9, .polarisation = 'H'},
	{.freq = 12625000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 12633000, .symrate = 4883000, .fec = 9, .polarisation = 'V'},
	{.freq = 12642000, .symrate = 3418000, .fec = 9, .polarisation = 'V'},
	{.freq = 12650000, .symrate = 15000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12656000, .symrate = 4883000, .fec = 9, .polarisation = 'V'},
	{.freq = 12677000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 12683000, .symrate = 2894000, .fec = 9, .polarisation = 'H'},
	{.freq = 12703000, .symrate = 2748000, .fec = 9, .polarisation = 'H'},
	{.freq = 12716000, .symrate = 6000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12723000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12733000, .symrate = 16277000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Express_3A_11_0W[] = {
	{.freq = 3675000, .symrate = 29623000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_ExpressAM1_40_0E[] = {
	{.freq = 10967000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
	{.freq = 10995000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11097000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_ExpressAM22_53_0E[] = {
	{.freq = 11044000, .symrate = 44950000, .fec = 3, .polarisation = 'V'},
	{.freq = 10974000, .symrate = 8150000, .fec = 3, .polarisation = 'H'},
	{.freq = 11031000, .symrate = 3750000, .fec = 3, .polarisation = 'H'},
	{.freq = 11096000, .symrate = 6400000, .fec = 3, .polarisation = 'V'},
	{.freq = 11124000, .symrate = 7593000, .fec = 3, .polarisation = 'V'},
	{.freq = 11161000, .symrate = 5785000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_ExpressAM2_80_0E[] = {
	{.freq = 10973000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 10991000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 11044000, .symrate = 44948000, .fec = 9, .polarisation = 'H'},
	{.freq = 11081000, .symrate = 5064000, .fec = 9, .polarisation = 'V'},
	{.freq = 11088000, .symrate = 4548000, .fec = 9, .polarisation = 'V'},
	{.freq = 11191000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 11462000, .symrate = 3200000, .fec = 9, .polarisation = 'V'},
	{.freq = 11478000, .symrate = 4400000, .fec = 9, .polarisation = 'H'},
	{.freq = 11544000, .symrate = 44950000, .fec = 9, .polarisation = 'V'},
	{.freq = 11606000, .symrate = 44948000, .fec = 9, .polarisation = 'V'},
	{.freq = 11650000, .symrate = 3500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3525000, .symrate = 31106000, .fec = 3, .polarisation = 'V'},
	{.freq = 3558000, .symrate = 3215000, .fec = 3, .polarisation = 'V'},
	{.freq = 3562000, .symrate = 3225000, .fec = 3, .polarisation = 'V'},
	{.freq = 3625000, .symrate = 3000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3675000, .symrate = 33483000, .fec = 7, .polarisation = 'V'},
	{.freq = 3929000, .symrate = 8705000, .fec = 3, .polarisation = 'H'},
	{.freq = 4147000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 4175000, .symrate = 6510000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Galaxy10R_123w[] = {
	{.freq = 11720000, .symrate = 27692000, .fec = 9, .polarisation = 'V'},
	{.freq = 11732000, .symrate = 13240000, .fec = 9, .polarisation = 'H'},
	{.freq = 11800000, .symrate = 26657000, .fec = 9, .polarisation = 'V'},
	{.freq = 11805000, .symrate = 4580000, .fec = 9, .polarisation = 'H'},
	{.freq = 11966000, .symrate = 13021000, .fec = 9, .polarisation = 'V'},
	{.freq = 12104000, .symrate = 2222000, .fec = 9, .polarisation = 'V'},
	{.freq = 12114000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Galaxy11_91w[] = {
	{.freq = 10964000, .symrate = 19850000, .fec = 9, .polarisation = 'V'},
	{.freq = 10994000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11024000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11806000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11815000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11825000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 11925000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11930000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11935000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11940000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11945000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11950000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11950000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11955000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11955000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11965000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11970000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11975000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11980000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11985000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11985000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11990000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11990000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11995000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11995000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12000000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12005000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12010000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12010000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12015000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12020000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12025000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12030000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12035000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12066000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12075000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12083000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12086000, .symrate = 6144000, .fec = 9, .polarisation = 'V'},
	{.freq = 12096000, .symrate = 6144000, .fec = 9, .polarisation = 'V'},
	{.freq = 12104000, .symrate = 6144000, .fec = 9, .polarisation = 'V'},
	{.freq = 12114000, .symrate = 6144000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Galaxy25_97w[] = {
	{.freq = 11789000, .symrate = 28125000, .fec = 9, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 20770000, .fec = 9, .polarisation = 'V'},
	{.freq = 11867000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11874000, .symrate = 22000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11898000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11966000, .symrate = 22000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11991000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11999000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12053000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12084000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12090000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12115000, .symrate = 22425000, .fec = 9, .polarisation = 'V'},
	{.freq = 12146000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12152000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12177000, .symrate = 23000000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Galaxy26_93w[] = {
	{.freq = 11711000, .symrate = 14312000, .fec = 9, .polarisation = 'V'},
	{.freq = 11721000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11727000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11732000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11742000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11748000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11753000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11767000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11777000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11782000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11788000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11793000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11809000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11814000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11841000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11887000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11893000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11904000, .symrate = 3010000, .fec = 9, .polarisation = 'H'},
	{.freq = 11919000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11924000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11926000, .symrate = 8848000, .fec = 9, .polarisation = 'H'},
	{.freq = 11929000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11935000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 8848000, .fec = 9, .polarisation = 'H'},
	{.freq = 11944000, .symrate = 8848000, .fec = 9, .polarisation = 'H'},
	{.freq = 11949000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11954000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11956000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11960000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11961000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11967000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11972000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12047000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12048000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12058000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12059000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12063000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12064000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12069000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12076000, .symrate = 8681000, .fec = 9, .polarisation = 'V'},
	{.freq = 12089000, .symrate = 6511000, .fec = 9, .polarisation = 'V'},
	{.freq = 12110000, .symrate = 4104000, .fec = 9, .polarisation = 'H'},
	{.freq = 12116000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12121000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12126000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12132000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 5147000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Galaxy27_129w[] = {
	{.freq = 11964000, .symrate = 2920000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Galaxy28_89w[] = {
	{.freq = 11717000, .symrate = 4411000, .fec = 9, .polarisation = 'H'},
	{.freq = 11747000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 11756000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 11780000, .symrate = 29000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11800000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 11825000, .symrate = 4552000, .fec = 9, .polarisation = 'H'},
	{.freq = 11865000, .symrate = 3700000, .fec = 9, .polarisation = 'H'},
	{.freq = 11882000, .symrate = 4883000, .fec = 9, .polarisation = 'H'},
	{.freq = 11925000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11930000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11935000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11936000, .symrate = 6000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11940000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11945000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11950000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11955000, .symrate = 19532000, .fec = 9, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 28800000, .fec = 9, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11980000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11985000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11989000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 11990000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11995000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12000000, .symrate = 28800000, .fec = 9, .polarisation = 'H'},
	{.freq = 12009000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 12032000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 12092000, .symrate = 2314000, .fec = 9, .polarisation = 'H'},
	{.freq = 12114000, .symrate = 14398000, .fec = 9, .polarisation = 'H'},
	{.freq = 12134000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12164000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12170000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12180000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12185000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12191000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12196000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Galaxy3C_95w[] = {
	{.freq = 11780000, .symrate = 20760000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Hispasat_30_0W[] = {
	{.freq = 11539000, .symrate = 24500000, .fec = 5, .polarisation = 'V'},
	{.freq = 11749000, .symrate = 3520000, .fec = 9, .polarisation = 'V'},
	{.freq = 11760000, .symrate = 3260000, .fec = 9, .polarisation = 'V'},
	{.freq = 11766000, .symrate = 4500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11776000, .symrate = 2387000, .fec = 9, .polarisation = 'V'},
	{.freq = 11783000, .symrate = 1200000, .fec = 9, .polarisation = 'V'},
	{.freq = 11787000, .symrate = 2500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11807000, .symrate = 6510000, .fec = 9, .polarisation = 'V'},
	{.freq = 11823000, .symrate = 2387000, .fec = 9, .polarisation = 'V'},
	{.freq = 11884000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11907000, .symrate = 2592000, .fec = 9, .polarisation = 'V'},
	{.freq = 11917000, .symrate = 5681000, .fec = 9, .polarisation = 'V'},
	{.freq = 11931000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11931000, .symrate = 2220000, .fec = 9, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 5185000, .fec = 9, .polarisation = 'V'},
	{.freq = 11940000, .symrate = 1481000, .fec = 9, .polarisation = 'V'},
	{.freq = 11972000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 11997000, .symrate = 4422000, .fec = 9, .polarisation = 'H'},
	{.freq = 12003000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12008000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 3492000, .fec = 9, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12040000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12052000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 12085000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12131000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 12135000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 12137000, .symrate = 3030000, .fec = 9, .polarisation = 'V'},
	{.freq = 12141000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 12146000, .symrate = 4200000, .fec = 9, .polarisation = 'V'},
	{.freq = 12156000, .symrate = 2222000, .fec = 9, .polarisation = 'V'},
	{.freq = 12158000, .symrate = 2348000, .fec = 9, .polarisation = 'V'},
	{.freq = 12163000, .symrate = 3030000, .fec = 9, .polarisation = 'V'},
	{.freq = 12168000, .symrate = 5240000, .fec = 9, .polarisation = 'V'},
	{.freq = 12172000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 4500000, .fec = 9, .polarisation = 'V'},
	{.freq = 12182000, .symrate = 3340000, .fec = 9, .polarisation = 'V'},
	{.freq = 12188000, .symrate = 2583000, .fec = 9, .polarisation = 'V'},
	{.freq = 12192000, .symrate = 2593000, .fec = 9, .polarisation = 'V'},
	{.freq = 12567000, .symrate = 19850000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Hotbird_13_0E[] = {
	{.freq = 12539000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10719000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10723000, .symrate = 29900000, .fec = 3, .polarisation = 'H'},
	{.freq = 10757000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10775000, .symrate = 28000000, .fec = 3, .polarisation = 'H'},
	{.freq = 10795000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10834000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10853000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10872000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10892000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10910000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10930000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10949000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10971000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10992000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11013000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11034000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11054000, .symrate = 27500000, .fec = 5, .polarisation = 'H'},
	{.freq = 11075000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11095000, .symrate = 28000000, .fec = 3, .polarisation = 'H'},
	{.freq = 11117000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11137000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11158000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11178000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11200000, .symrate = 27500000, .fec = 5, .polarisation = 'V'},
	{.freq = 11219000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11242000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11278000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11295000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11334000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11355000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11373000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 11393000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11411000, .symrate = 27500000, .fec = 5, .polarisation = 'H'},
	{.freq = 11432000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11470000, .symrate = 27500000, .fec = 5, .polarisation = 'V'},
	{.freq = 11488000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11526000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11541000, .symrate = 22000000, .fec = 5, .polarisation = 'V'},
	{.freq = 11565000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11585000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11604000, .symrate = 27500000, .fec = 5, .polarisation = 'H'},
	{.freq = 11623000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11645000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11662000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11677000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11727000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11747000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11765000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11785000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11804000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11823000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11842000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11861000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11880000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11900000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11919000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 11938000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11958000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11976000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12034000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12054000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12072000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12092000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12111000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12149000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12169000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12188000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12207000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12226000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12245000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12264000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12284000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12302000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12322000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12341000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12360000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12379000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12398000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12418000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12437000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12475000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12519000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12558000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12577000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12596000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12616000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12635000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12654000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12673000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12692000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12713000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12731000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_IA5_97w[] = {
	{.freq = 11789000, .symrate = 25000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 20765000, .fec = 9, .polarisation = 'V'},
	{.freq = 11867000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11874000, .symrate = 22000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11898000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11966000, .symrate = 22000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11991000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12053000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12084000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12090000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12115000, .symrate = 22425000, .fec = 9, .polarisation = 'V'},
	{.freq = 12122000, .symrate = 22000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12146000, .symrate = 22000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12152000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12177000, .symrate = 23000000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_IA6_93w[] = {
	{.freq = 11711000, .symrate = 14312000, .fec = 9, .polarisation = 'V'},
	{.freq = 11721000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11727000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11732000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11742000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11748000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11753000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11767000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11772000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11777000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11782000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11788000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11793000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11809000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11814000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11827000, .symrate = 8429000, .fec = 9, .polarisation = 'V'},
	{.freq = 11836000, .symrate = 7179000, .fec = 9, .polarisation = 'H'},
	{.freq = 11841000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11865000, .symrate = 3516000, .fec = 9, .polarisation = 'V'},
	{.freq = 11887000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11893000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11904000, .symrate = 5000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11919000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11924000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11926000, .symrate = 8848000, .fec = 9, .polarisation = 'H'},
	{.freq = 11929000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11935000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11936000, .symrate = 8848000, .fec = 9, .polarisation = 'H'},
	{.freq = 11944000, .symrate = 8848000, .fec = 9, .polarisation = 'H'},
	{.freq = 11949000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11954000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11956000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11960000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11961000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11965000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11967000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11972000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12047000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12048000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12058000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12059000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12063000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 12064000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12069000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12076000, .symrate = 8679000, .fec = 9, .polarisation = 'V'},
	{.freq = 12089000, .symrate = 6511000, .fec = 9, .polarisation = 'V'},
	{.freq = 12110000, .symrate = 4104000, .fec = 9, .polarisation = 'H'},
	{.freq = 12116000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12121000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12126000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12132000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 5147000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_IA7_129w[] = {
	{.freq = 11989000, .symrate = 2821000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_IA8_89w[] = {
	{.freq = 11780000, .symrate = 29000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11925000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11930000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11935000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11940000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11945000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11945000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11950000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11950000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11955000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11955000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11960000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11965000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11965000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11970000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11970000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 3979000, .fec = 9, .polarisation = 'V'},
	{.freq = 11975000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11980000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11985000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11989000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 11990000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 11995000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12009000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 12164000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12170000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12180000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12185000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12191000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
	{.freq = 12196000, .symrate = 3979000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Insat2E_C_83_0E[] = {
	{.freq = 3521000, .symrate = 8640000, .fec = 3, .polarisation = 'V'},
	{.freq = 3537000, .symrate = 10800000, .fec = 3, .polarisation = 'V'},
	{.freq = 3550000, .symrate = 3255000, .fec = 3, .polarisation = 'V'},
	{.freq = 3559000, .symrate = 3366000, .fec = 3, .polarisation = 'V'},
	{.freq = 3581000, .symrate = 3255000, .fec = 3, .polarisation = 'V'},
	{.freq = 3593000, .symrate = 8680000, .fec = 3, .polarisation = 'V'},
	{.freq = 3593000, .symrate = 9629000, .fec = 3, .polarisation = 'V'},
	{.freq = 3643000, .symrate = 19531000, .fec = 3, .polarisation = 'V'},
	{.freq = 3678000, .symrate = 5926000, .fec = 3, .polarisation = 'V'},
	{.freq = 3689000, .symrate = 2893000, .fec = 3, .polarisation = 'V'},
	{.freq = 3696000, .symrate = 6111000, .fec = 3, .polarisation = 'V'},
	{.freq = 3760000, .symrate = 6500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3774000, .symrate = 13021000, .fec = 3, .polarisation = 'V'},
	{.freq = 3794000, .symrate = 3500000, .fec = 10, .polarisation = 'V'},
	{.freq = 3815000, .symrate = 6000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3845000, .symrate = 26043000, .fec = 3, .polarisation = 'V'},
	{.freq = 3914000, .symrate = 3000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3919000, .symrate = 3000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3923000, .symrate = 3000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3934000, .symrate = 13021000, .fec = 3, .polarisation = 'V'},
	{.freq = 3961000, .symrate = 6656000, .fec = 3, .polarisation = 'V'},
	{.freq = 3974000, .symrate = 13333000, .fec = 3, .polarisation = 'V'},
	{.freq = 4010000, .symrate = 19531000, .fec = 3, .polarisation = 'V'},
	{.freq = 4031000, .symrate = 7440000, .fec = 7, .polarisation = 'V'},
	{.freq = 4042000, .symrate = 7420000, .fec = 7, .polarisation = 'V'},
	{.freq = 4050000, .symrate = 5084000, .fec = 7, .polarisation = 'V'},
	{.freq = 4070000, .symrate = 5000000, .fec = 3, .polarisation = 'V'},
	{.freq = 4091000, .symrate = 9230000, .fec = 7, .polarisation = 'V'},
	{.freq = 4100000, .symrate = 4700000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Insat3A_C_93_5E[] = {
	{.freq = 3732000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3740000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3750000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3759000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3772000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3782000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3791000, .symrate = 8600000, .fec = 3, .polarisation = 'V'},
	{.freq = 3812000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3820000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3831000, .symrate = 8600000, .fec = 3, .polarisation = 'V'},
	{.freq = 3932000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3940000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3950000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 3958000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 4092000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 4101000, .symrate = 6250000, .fec = 3, .polarisation = 'V'},
	{.freq = 4109000, .symrate = 4250000, .fec = 3, .polarisation = 'V'},
	{.freq = 4132000, .symrate = 4000000, .fec = 3, .polarisation = 'V'},
	{.freq = 4136000, .symrate = 2000000, .fec = 3, .polarisation = 'V'},
	{.freq = 4141000, .symrate = 5150000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Insat4A_C_83_0E[] = {
	{.freq = 3725000, .symrate = 26665000, .fec = 3, .polarisation = 'H'},
	{.freq = 3756000, .symrate = 13333000, .fec = 3, .polarisation = 'H'},
	{.freq = 3777000, .symrate = 10900000, .fec = 3, .polarisation = 'H'},
	{.freq = 3805000, .symrate = 28500000, .fec = 7, .polarisation = 'H'},
	{.freq = 3841000, .symrate = 6920000, .fec = 3, .polarisation = 'H'},
	{.freq = 3853000, .symrate = 3265000, .fec = 3, .polarisation = 'H'},
	{.freq = 3857000, .symrate = 3462000, .fec = 3, .polarisation = 'H'},
	{.freq = 3874000, .symrate = 3403000, .fec = 3, .polarisation = 'H'},
	{.freq = 3892000, .symrate = 3300000, .fec = 3, .polarisation = 'H'},
	{.freq = 3898000, .symrate = 6800000, .fec = 3, .polarisation = 'H'},
	{.freq = 3910000, .symrate = 3600000, .fec = 7, .polarisation = 'H'},
	{.freq = 3923000, .symrate = 13000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3949000, .symrate = 3225000, .fec = 3, .polarisation = 'H'},
	{.freq = 3958000, .symrate = 9500000, .fec = 7, .polarisation = 'H'},
	{.freq = 3976000, .symrate = 3200000, .fec = 3, .polarisation = 'H'},
	{.freq = 4004000, .symrate = 22220000, .fec = 5, .polarisation = 'H'},
	{.freq = 4040000, .symrate = 7500000, .fec = 7, .polarisation = 'H'},
	{.freq = 4054000, .symrate = 13230000, .fec = 3, .polarisation = 'H'},
	{.freq = 4072000, .symrate = 6500000, .fec = 3, .polarisation = 'H'},
	{.freq = 4078000, .symrate = 3250000, .fec = 3, .polarisation = 'H'},
	{.freq = 4087000, .symrate = 3300000, .fec = 3, .polarisation = 'H'},
	{.freq = 4091000, .symrate = 3000000, .fec = 3, .polarisation = 'H'},
	{.freq = 4100000, .symrate = 4750000, .fec = 3, .polarisation = 'H'},
	{.freq = 4115000, .symrate = 5555000, .fec = 3, .polarisation = 'H'},
	{.freq = 4134000, .symrate = 10600000, .fec = 3, .polarisation = 'H'},
	{.freq = 4151000, .symrate = 6500000, .fec = 3, .polarisation = 'H'},
	{.freq = 4161000, .symrate = 6500000, .fec = 3, .polarisation = 'H'},
	{.freq = 4170000, .symrate = 6500000, .fec = 3, .polarisation = 'H'},
	{.freq = 4180000, .symrate = 3233000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Insat4B_C_93_5E[] = {
	{.freq = 3725000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3750000, .symrate = 4250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3762000, .symrate = 4250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3768000, .symrate = 4250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3774000, .symrate = 4250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3802000, .symrate = 4250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3808000, .symrate = 4250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3822000, .symrate = 4250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3832000, .symrate = 6250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3841000, .symrate = 6250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3885000, .symrate = 28000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3925000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3950000, .symrate = 4440000, .fec = 3, .polarisation = 'H'},
	{.freq = 4005000, .symrate = 25422000, .fec = 7, .polarisation = 'H'},
	{.freq = 4045000, .symrate = 28000000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Intel4_72_0E[] = {
	{.freq = 11533000, .symrate = 4220000, .fec = 9, .polarisation = 'V'},
	{.freq = 11638000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 12518000, .symrate = 8232000, .fec = 9, .polarisation = 'V'},
	{.freq = 12526000, .symrate = 3266000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intel904_60_0E[] = {
	{.freq = 11003000, .symrate = 2975000, .fec = 9, .polarisation = 'H'},
	{.freq = 11011000, .symrate = 2975000, .fec = 9, .polarisation = 'H'},
	{.freq = 11015000, .symrate = 2975000, .fec = 9, .polarisation = 'H'},
	{.freq = 11093000, .symrate = 3980000, .fec = 9, .polarisation = 'V'},
	{.freq = 11101000, .symrate = 4105000, .fec = 9, .polarisation = 'V'},
	{.freq = 11142000, .symrate = 2963000, .fec = 9, .polarisation = 'H'},
	{.freq = 11152000, .symrate = 2963000, .fec = 9, .polarisation = 'H'},
	{.freq = 11157000, .symrate = 2963000, .fec = 9, .polarisation = 'H'},
	{.freq = 11515000, .symrate = 7300000, .fec = 9, .polarisation = 'V'},
	{.freq = 11635000, .symrate = 29700000, .fec = 9, .polarisation = 'V'},
	{.freq = 11675000, .symrate = 29700000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_1002_1_0W[] = {
	{.freq = 4175000, .symrate = 28000000, .fec = 9, .polarisation = 'V'},
	{.freq = 4180000, .symrate = 21050000, .fec = 9, .polarisation = 'H'},
	{.freq = 11093000, .symrate = 19191000, .fec = 7, .polarisation = 'H'},
	{.freq = 11166000, .symrate = 1450000, .fec = 7, .polarisation = 'V'},
	{.freq = 11182000, .symrate = 3100000, .fec = 7, .polarisation = 'V'},
	{.freq = 11606000, .symrate = 13356000, .fec = 7, .polarisation = 'H'},
	{.freq = 12527000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12563000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12563000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12607000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12607000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12643000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12643000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12687000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12687000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12718000, .symrate = 18400000, .fec = 3, .polarisation = 'V'},
	{.freq = 12723000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12735000, .symrate = 8800000, .fec = 5, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_11_43_0W[] = {
	{.freq = 3944000, .symrate = 5945000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Intelsat_1R_45_0W[] = {
	{.freq = 4104000, .symrate = 14450000, .fec = 9, .polarisation = 'V'},
	{.freq = 3854000, .symrate = 2370000, .fec = 9, .polarisation = 'H'},
	{.freq = 11893000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3869000, .symrate = 3515000, .fec = 9, .polarisation = 'H'},
	{.freq = 4186000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 4040000, .symrate = 4347000, .fec = 9, .polarisation = 'V'},
	{.freq = 4071000, .symrate = 2615000, .fec = 9, .polarisation = 'V'},
	{.freq = 3991000, .symrate = 4044000, .fec = 9, .polarisation = 'V'},
	{.freq = 3882000, .symrate = 4410000, .fec = 9, .polarisation = 'V'},
	{.freq = 11788000, .symrate = 10000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11728000, .symrate = 5057000, .fec = 9, .polarisation = 'V'},
	{.freq = 11737000, .symrate = 5057000, .fec = 9, .polarisation = 'V'},
	{.freq = 11722000, .symrate = 3000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11718000, .symrate = 24667000, .fec = 9, .polarisation = 'H'},
	{.freq = 11833000, .symrate = 17360000, .fec = 9, .polarisation = 'H'},
	{.freq = 11930000, .symrate = 11790000, .fec = 9, .polarisation = 'V'},
	{.freq = 11808000, .symrate = 4779000, .fec = 9, .polarisation = 'H'},
	{.freq = 4132000, .symrate = 5749000, .fec = 9, .polarisation = 'H'},
	{.freq = 3780000, .symrate = 2941000, .fec = 9, .polarisation = 'V'},
	{.freq = 11705000, .symrate = 4440000, .fec = 9, .polarisation = 'V'},
	{.freq = 3759000, .symrate = 2941000, .fec = 9, .polarisation = 'V'},
	{.freq = 4171000, .symrate = 4410000, .fec = 9, .polarisation = 'V'},
	{.freq = 3899000, .symrate = 6611000, .fec = 9, .polarisation = 'V'},
	{.freq = 4192000, .symrate = 2075000, .fec = 9, .polarisation = 'H'},
	{.freq = 4096000, .symrate = 8102000, .fec = 9, .polarisation = 'H'},
	{.freq = 11873000, .symrate = 6000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11856000, .symrate = 3000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11746000, .symrate = 6900000, .fec = 9, .polarisation = 'V'},
	{.freq = 11715000, .symrate = 5500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 3310000, .fec = 9, .polarisation = 'V'},
	{.freq = 3892000, .symrate = 6110000, .fec = 9, .polarisation = 'V'},
	{.freq = 4135000, .symrate = 26600000, .fec = 9, .polarisation = 'H'},
	{.freq = 4121000, .symrate = 3510000, .fec = 9, .polarisation = 'H'},
	{.freq = 3742000, .symrate = 4444000, .fec = 9, .polarisation = 'V'},
	{.freq = 3910000, .symrate = 4292000, .fec = 9, .polarisation = 'V'},
	{.freq = 3975000, .symrate = 3310000, .fec = 9, .polarisation = 'V'},
	{.freq = 4133000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 3785000, .symrate = 4409000, .fec = 9, .polarisation = 'V'},
	{.freq = 3868000, .symrate = 10075000, .fec = 9, .polarisation = 'V'},
	{.freq = 3774000, .symrate = 8820000, .fec = 9, .polarisation = 'V'},
	{.freq = 4128000, .symrate = 3310000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_3R_43_0W[] = {
	{.freq = 3936000, .symrate = 3310000, .fec = 9, .polarisation = 'H'},
	{.freq = 3901000, .symrate = 6620000, .fec = 9, .polarisation = 'V'},
	{.freq = 3891000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 3935000, .symrate = 17360000, .fec = 9, .polarisation = 'V'},
	{.freq = 3872000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3736000, .symrate = 29270000, .fec = 9, .polarisation = 'H'},
	{.freq = 4106000, .symrate = 26470000, .fec = 9, .polarisation = 'V'},
	{.freq = 3910000, .symrate = 5632000, .fec = 9, .polarisation = 'V'},
	{.freq = 3919000, .symrate = 6620000, .fec = 9, .polarisation = 'V'},
	{.freq = 4083000, .symrate = 6599000, .fec = 9, .polarisation = 'V'},
	{.freq = 4106000, .symrate = 29270000, .fec = 9, .polarisation = 'H'},
	{.freq = 3994000, .symrate = 21090000, .fec = 9, .polarisation = 'H'},
	{.freq = 3867000, .symrate = 6429000, .fec = 9, .polarisation = 'V'},
	{.freq = 3958000, .symrate = 6500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3980000, .symrate = 3500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3828000, .symrate = 4350000, .fec = 9, .polarisation = 'V'},
	{.freq = 3888000, .symrate = 7813000, .fec = 9, .polarisation = 'H'},
	{.freq = 3785000, .symrate = 30800000, .fec = 9, .polarisation = 'H'},
	{.freq = 3942000, .symrate = 1200000, .fec = 9, .polarisation = 'V'},
	{.freq = 3946000, .symrate = 2592000, .fec = 9, .polarisation = 'V'},
	{.freq = 3988000, .symrate = 4070000, .fec = 9, .polarisation = 'V'},
	{.freq = 3845000, .symrate = 30800000, .fec = 9, .polarisation = 'H'},
	{.freq = 4040000, .symrate = 30800000, .fec = 9, .polarisation = 'H'},
	{.freq = 3865000, .symrate = 6900000, .fec = 9, .polarisation = 'V'},
	{.freq = 3850000, .symrate = 28800000, .fec = 9, .polarisation = 'V'},
	{.freq = 11705000, .symrate = 3700000, .fec = 9, .polarisation = 'H'},
	{.freq = 11745000, .symrate = 3111000, .fec = 9, .polarisation = 'H'},
	{.freq = 11711000, .symrate = 4687000, .fec = 9, .polarisation = 'H'},
	{.freq = 11754000, .symrate = 3109000, .fec = 9, .polarisation = 'H'},
	{.freq = 3746000, .symrate = 21261000, .fec = 9, .polarisation = 'V'},
	{.freq = 4150000, .symrate = 24570000, .fec = 9, .polarisation = 'H'},
	{.freq = 3930000, .symrate = 2812000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Intelsat_6B_43_0W[] = {
	{.freq = 10882000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 10882000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 10970000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 10970000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11050000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11130000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11382000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11130000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 10720000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 10720000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11050000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11382000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 10800000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 10800000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_705_50_0W[] = {
	{.freq = 3911000, .symrate = 3617000, .fec = 9, .polarisation = 'V'},
	{.freq = 3917000, .symrate = 4087000, .fec = 9, .polarisation = 'V'},
	{.freq = 3838000, .symrate = 7053000, .fec = 9, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Intelsat_707_53_0W[] = {
	{.freq = 3820000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 11483000, .symrate = 5333000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_805_55_5W[] = {
	{.freq = 4171000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 4147000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 3914000, .symrate = 1809000, .fec = 9, .polarisation = 'V'},
	{.freq = 3572000, .symrate = 11800000, .fec = 9, .polarisation = 'V'},
	{.freq = 4135000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 3737000, .symrate = 1809000, .fec = 9, .polarisation = 'H'},
	{.freq = 3932000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 3759000, .symrate = 4167000, .fec = 9, .polarisation = 'V'},
	{.freq = 3750000, .symrate = 5632000, .fec = 9, .polarisation = 'V'},
	{.freq = 3850000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4010000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 3715000, .symrate = 8890000, .fec = 9, .polarisation = 'H'},
	{.freq = 3670000, .symrate = 1374000, .fec = 9, .polarisation = 'H'},
	{.freq = 4158000, .symrate = 1447000, .fec = 9, .polarisation = 'H'},
	{.freq = 3549000, .symrate = 6510000, .fec = 9, .polarisation = 'V'},
	{.freq = 3451000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 3667000, .symrate = 3300000, .fec = 9, .polarisation = 'H'},
	{.freq = 3522000, .symrate = 30000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4055000, .symrate = 21703000, .fec = 9, .polarisation = 'V'},
	{.freq = 3698000, .symrate = 3600000, .fec = 9, .polarisation = 'V'},
	{.freq = 4119000, .symrate = 1631000, .fec = 9, .polarisation = 'V'},
	{.freq = 3446000, .symrate = 3200000, .fec = 9, .polarisation = 'H'},
	{.freq = 3675000, .symrate = 2660000, .fec = 9, .polarisation = 'V'},
	{.freq = 3717000, .symrate = 11574000, .fec = 9, .polarisation = 'V'},
	{.freq = 4030000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 3767000, .symrate = 4427000, .fec = 9, .polarisation = 'H'},
	{.freq = 4027000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 4089000, .symrate = 5540000, .fec = 9, .polarisation = 'V'},
	{.freq = 3685000, .symrate = 5632000, .fec = 9, .polarisation = 'V'},
	{.freq = 3751000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 4127000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3980000, .symrate = 19510000, .fec = 9, .polarisation = 'V'},
	{.freq = 3590000, .symrate = 10127000, .fec = 9, .polarisation = 'V'},
	{.freq = 4000000, .symrate = 5200000, .fec = 9, .polarisation = 'V'},
	{.freq = 3762000, .symrate = 3662000, .fec = 9, .polarisation = 'H'},
	{.freq = 3936000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 3727000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4104000, .symrate = 5062000, .fec = 9, .polarisation = 'H'},
	{.freq = 3777000, .symrate = 7400000, .fec = 9, .polarisation = 'H'},
	{.freq = 3431000, .symrate = 3500000, .fec = 9, .polarisation = 'H'},
	{.freq = 3478000, .symrate = 5632000, .fec = 9, .polarisation = 'V'},
	{.freq = 4093000, .symrate = 2540000, .fec = 9, .polarisation = 'H'},
	{.freq = 3929000, .symrate = 2941000, .fec = 9, .polarisation = 'H'},
	{.freq = 4195000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 4140000, .symrate = 4700000, .fec = 9, .polarisation = 'V'},
	{.freq = 4096000, .symrate = 5247000, .fec = 9, .polarisation = 'H'},
	{.freq = 3467000, .symrate = 4340000, .fec = 9, .polarisation = 'V'},
	{.freq = 3442000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 4340000, .fec = 9, .polarisation = 'V'},
	{.freq = 4084000, .symrate = 10317000, .fec = 9, .polarisation = 'H'},
	{.freq = 3723000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3815000, .symrate = 26667000, .fec = 9, .polarisation = 'H'},
	{.freq = 4006000, .symrate = 3690000, .fec = 9, .polarisation = 'V'},
	{.freq = 3918000, .symrate = 4400000, .fec = 9, .polarisation = 'V'},
	{.freq = 3735000, .symrate = 8680000, .fec = 9, .polarisation = 'V'},
	{.freq = 3727000, .symrate = 3000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3940000, .symrate = 2575000, .fec = 9, .polarisation = 'H'},
	{.freq = 3792000, .symrate = 2244000, .fec = 9, .polarisation = 'H'},
	{.freq = 3900000, .symrate = 3612000, .fec = 9, .polarisation = 'H'},
	{.freq = 3677000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 4127000, .symrate = 2532000, .fec = 9, .polarisation = 'H'},
	{.freq = 4152000, .symrate = 3600000, .fec = 9, .polarisation = 'V'},
	{.freq = 4177000, .symrate = 27690000, .fec = 9, .polarisation = 'V'},
	{.freq = 4111000, .symrate = 3333000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Intelsat8_C_166_0E[] = {
	{.freq = 3740000, .symrate = 27500000, .fec = 2, .polarisation = 'H'},
	{.freq = 3790000, .symrate = 6510000, .fec = 3, .polarisation = 'V'},
	{.freq = 3815000, .symrate = 4400000, .fec = 3, .polarisation = 'V'},
	{.freq = 3829000, .symrate = 13238000, .fec = 3, .polarisation = 'H'},
	{.freq = 3833000, .symrate = 17500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3900000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3940000, .symrate = 27690000, .fec = 7, .polarisation = 'H'},
	{.freq = 3959000, .symrate = 3428000, .fec = 3, .polarisation = 'V'},
	{.freq = 4060000, .symrate = 16180000, .fec = 1, .polarisation = 'H'},
	{.freq = 4122000, .symrate = 5554000, .fec = 3, .polarisation = 'V'},
	{.freq = 4130000, .symrate = 5800000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_903_34_5W[] = {
	{.freq = 4178000, .symrate = 32555000, .fec = 9, .polarisation = 'V'},
	{.freq = 4045000, .symrate = 4960000, .fec = 9, .polarisation = 'H'},
	{.freq = 3895000, .symrate = 13021000, .fec = 9, .polarisation = 'H'},
	{.freq = 4004000, .symrate = 2170000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_905_24_5W[] = {
	{.freq = 4171000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 4181000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 4194000, .symrate = 5193000, .fec = 9, .polarisation = 'V'},
	{.freq = 4162000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 4060000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 4070000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Intelsat_907_27_5W[] = {
	{.freq = 3873000, .symrate = 4687000, .fec = 9, .polarisation = 'H'},
	{.freq = 3935000, .symrate = 4687000, .fec = 9, .polarisation = 'H'},
	{.freq = 3743000, .symrate = 2900000, .fec = 9, .polarisation = 'V'},
	{.freq = 3732000, .symrate = 14000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3943000, .symrate = 1808000, .fec = 9, .polarisation = 'H'},
	{.freq = 3938000, .symrate = 3544000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Intelsat_9_58_0W[] = {
	{.freq = 4122000, .symrate = 2222000, .fec = 9, .polarisation = 'H'},
	{.freq = 4146000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 4157000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 4160000, .symrate = 3000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3960000, .symrate = 29270000, .fec = 9, .polarisation = 'V'},
	{.freq = 3960000, .symrate = 29270000, .fec = 9, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 27684000, .fec = 9, .polarisation = 'H'},
	{.freq = 3720000, .symrate = 19510000, .fec = 9, .polarisation = 'H'},
	{.freq = 4131000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 4155000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 4173000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3995000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 3986000, .symrate = 6111000, .fec = 9, .polarisation = 'H'},
	{.freq = 4175000, .symrate = 4410000, .fec = 9, .polarisation = 'H'},
	{.freq = 4107000, .symrate = 8850000, .fec = 9, .polarisation = 'H'},
	{.freq = 3925000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3915000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3925000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 4005000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 4015000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 3720000, .symrate = 27700000, .fec = 9, .polarisation = 'V'},
	{.freq = 4166000, .symrate = 6200000, .fec = 9, .polarisation = 'H'},
	{.freq = 3794000, .symrate = 3332000, .fec = 9, .polarisation = 'H'},
	{.freq = 3807000, .symrate = 3428000, .fec = 9, .polarisation = 'H'},
	{.freq = 3880000, .symrate = 27690000, .fec = 9, .polarisation = 'V'},
	{.freq = 3800000, .symrate = 26470000, .fec = 9, .polarisation = 'V'},
	{.freq = 3760000, .symrate = 27690000, .fec = 9, .polarisation = 'V'},
	{.freq = 3996000, .symrate = 3330000, .fec = 9, .polarisation = 'H'},
	{.freq = 3935000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 11852000, .symrate = 30000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11895000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11913000, .symrate = 10000000, .fec = 9, .polarisation = 'V'},
	{.freq = 4155000, .symrate = 3310000, .fec = 9, .polarisation = 'V'},
	{.freq = 3934000, .symrate = 7000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3787000, .symrate = 7407000, .fec = 9, .polarisation = 'H'},
	{.freq = 3924000, .symrate = 6620000, .fec = 9, .polarisation = 'V'},
	{.freq = 3800000, .symrate = 4444000, .fec = 9, .polarisation = 'H'},
	{.freq = 3911000, .symrate = 13330000, .fec = 9, .polarisation = 'V'},
	{.freq = 4040000, .symrate = 16180000, .fec = 9, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 27690000, .fec = 9, .polarisation = 'V'},
	{.freq = 4144000, .symrate = 2205000, .fec = 9, .polarisation = 'V'},
	{.freq = 4151000, .symrate = 2890000, .fec = 9, .polarisation = 'V'},
	{.freq = 4147000, .symrate = 2941000, .fec = 9, .polarisation = 'V'},
	{.freq = 4170000, .symrate = 2941000, .fec = 9, .polarisation = 'V'},
	{.freq = 3760000, .symrate = 28500000, .fec = 9, .polarisation = 'H'},
	{.freq = 4120000, .symrate = 27500000, .fec = 9, .polarisation = 'V'},
	{.freq = 11670000, .symrate = 16470000, .fec = 9, .polarisation = 'V'},
	{.freq = 4125000, .symrate = 2941000, .fec = 9, .polarisation = 'H'},
	{.freq = 4137000, .symrate = 2941000, .fec = 9, .polarisation = 'H'},
	{.freq = 3880000, .symrate = 27690000, .fec = 9, .polarisation = 'H'},
	{.freq = 4174000, .symrate = 2941000, .fec = 9, .polarisation = 'V'},
	{.freq = 3840000, .symrate = 27690000, .fec = 9, .polarisation = 'V'},
	{.freq = 3840000, .symrate = 27690000, .fec = 9, .polarisation = 'H'},
	{.freq = 3815000, .symrate = 6250000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_JCSat3A_C_128_0E[] = {
	{.freq = 3960000, .symrate = 30000000, .fec = 5, .polarisation = 'V'},
	{.freq = 4000000, .symrate = 30000000, .fec = 5, .polarisation = 'V'},
	{.freq = 4055000, .symrate = 3332000, .fec = 3, .polarisation = 'V'},
	{.freq = 4120000, .symrate = 30000000, .fec = 5, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Measat3_C_91_5E[] = {
	{.freq = 3629000, .symrate = 2200000, .fec = 3, .polarisation = 'V'},
	{.freq = 3644000, .symrate = 22320000, .fec = 3, .polarisation = 'H'},
	{.freq = 3645000, .symrate = 19510000, .fec = 3, .polarisation = 'V'},
	{.freq = 3718000, .symrate = 1700000, .fec = 7, .polarisation = 'V'},
	{.freq = 3721000, .symrate = 2170000, .fec = 3, .polarisation = 'V'},
	{.freq = 3751000, .symrate = 13330000, .fec = 2, .polarisation = 'V'},
	{.freq = 3769000, .symrate = 14000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3796000, .symrate = 7030000, .fec = 3, .polarisation = 'V'},
	{.freq = 3814000, .symrate = 6666000, .fec = 3, .polarisation = 'V'},
	{.freq = 3865000, .symrate = 3835000, .fec = 3, .polarisation = 'V'},
	{.freq = 3877000, .symrate = 12525000, .fec = 7, .polarisation = 'V'},
	{.freq = 3887000, .symrate = 3984000, .fec = 3, .polarisation = 'V'},
	{.freq = 3920000, .symrate = 28000000, .fec = 7, .polarisation = 'V'},
	{.freq = 4113000, .symrate = 2960000, .fec = 3, .polarisation = 'V'},
	{.freq = 4117000, .symrate = 2963000, .fec = 3, .polarisation = 'V'},
	{.freq = 4134000, .symrate = 7200000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Nahuel_1_71_8W[] = {
	{.freq = 11673000, .symrate = 4000000, .fec = 9, .polarisation = 'V'},
	{.freq = 11680000, .symrate = 3335000, .fec = 9, .polarisation = 'V'},
	{.freq = 11654000, .symrate = 4170000, .fec = 9, .polarisation = 'V'},
	{.freq = 11874000, .symrate = 4000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12136000, .symrate = 2960000, .fec = 9, .polarisation = 'V'},
	{.freq = 11873000, .symrate = 8000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12116000, .symrate = 14396000, .fec = 9, .polarisation = 'H'},
	{.freq = 11997000, .symrate = 8500000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Nilesat101_102_7_0W[] = {
	{.freq = 10719000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10723000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10758000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10775000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10796000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10892000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 10911000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10930000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11317000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11747000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11766000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11785000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11804000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11823000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11843000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11862000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11881000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11900000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11919000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11938000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11958000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 27600000, .fec = 5, .polarisation = 'V'},
	{.freq = 11996000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12034000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12073000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12130000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12149000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12207000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12226000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12284000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12303000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12341000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12360000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12380000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12399000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12418000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12476000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_NSS_10_37_5W[] = {
	{.freq = 4055000, .symrate = 2700000, .fec = 9, .polarisation = 'V'},
	{.freq = 3824000, .symrate = 1808000, .fec = 9, .polarisation = 'H'},
	{.freq = 4059000, .symrate = 3214000, .fec = 9, .polarisation = 'V'},
	{.freq = 3828000, .symrate = 2532000, .fec = 9, .polarisation = 'H'},
	{.freq = 3844000, .symrate = 4340000, .fec = 9, .polarisation = 'H'},
	{.freq = 4071000, .symrate = 3150000, .fec = 9, .polarisation = 'V'},
	{.freq = 4051000, .symrate = 4440000, .fec = 9, .polarisation = 'V'},
	{.freq = 4044000, .symrate = 3250000, .fec = 9, .polarisation = 'V'},
	{.freq = 4077000, .symrate = 3200000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_NSS_7_22_0W[] = {
	{.freq = 3926000, .symrate = 3715000, .fec = 9, .polarisation = 'H'},
	{.freq = 3920000, .symrate = 3715000, .fec = 9, .polarisation = 'H'},
	{.freq = 3954000, .symrate = 5632000, .fec = 9, .polarisation = 'V'},
	{.freq = 3929000, .symrate = 5632000, .fec = 9, .polarisation = 'V'},
	{.freq = 3915000, .symrate = 3715000, .fec = 9, .polarisation = 'H'},
	{.freq = 3761000, .symrate = 22650000, .fec = 9, .polarisation = 'V'},
	{.freq = 11825000, .symrate = 5904000, .fec = 9, .polarisation = 'H'},
	{.freq = 12162000, .symrate = 6510000, .fec = 9, .polarisation = 'H'},
	{.freq = 11777000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11860000, .symrate = 35000000, .fec = 9, .polarisation = 'H'},
	{.freq = 12049000, .symrate = 6500000, .fec = 9, .polarisation = 'H'},
	{.freq = 11921000, .symrate = 35000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4003000, .symrate = 6667000, .fec = 9, .polarisation = 'H'},
	{.freq = 4011000, .symrate = 6667000, .fec = 9, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 3680000, .fec = 9, .polarisation = 'H'},
	{.freq = 3969000, .symrate = 1808000, .fec = 9, .polarisation = 'H'},
	{.freq = 3976000, .symrate = 1842000, .fec = 9, .polarisation = 'H'},
	{.freq = 11814000, .symrate = 5630000, .fec = 9, .polarisation = 'H'},
	{.freq = 4038000, .symrate = 3690000, .fec = 9, .polarisation = 'V'},
	{.freq = 3674000, .symrate = 2222000, .fec = 9, .polarisation = 'V'},
	{.freq = 4033000, .symrate = 3689000, .fec = 9, .polarisation = 'V'},
	{.freq = 4016000, .symrate = 3663000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_NSS_806_40_5W[] = {
	{.freq = 11921000, .symrate = 35000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3660000, .symrate = 4350000, .fec = 9, .polarisation = 'H'},
	{.freq = 3986000, .symrate = 3179000, .fec = 9, .polarisation = 'H'},
	{.freq = 3859000, .symrate = 2600000, .fec = 9, .polarisation = 'H'},
	{.freq = 4120000, .symrate = 2960000, .fec = 9, .polarisation = 'V'},
	{.freq = 4016000, .symrate = 5712000, .fec = 9, .polarisation = 'V'},
	{.freq = 3978000, .symrate = 3978000, .fec = 9, .polarisation = 'H'},
	{.freq = 4100000, .symrate = 6111000, .fec = 9, .polarisation = 'V'},
	{.freq = 3965000, .symrate = 2540000, .fec = 9, .polarisation = 'V'},
	{.freq = 3774000, .symrate = 6670000, .fec = 9, .polarisation = 'H'},
	{.freq = 3725000, .symrate = 26667000, .fec = 9, .polarisation = 'V'},
	{.freq = 3600000, .symrate = 25185000, .fec = 9, .polarisation = 'V'},
	{.freq = 3758000, .symrate = 26667000, .fec = 9, .polarisation = 'V'},
	{.freq = 3982000, .symrate = 17800000, .fec = 9, .polarisation = 'V'},
	{.freq = 4090000, .symrate = 2515000, .fec = 9, .polarisation = 'V'},
	{.freq = 4042000, .symrate = 8680000, .fec = 9, .polarisation = 'H'},
	{.freq = 4009000, .symrate = 6666000, .fec = 9, .polarisation = 'V'},
	{.freq = 4003000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 3648000, .symrate = 2000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3664000, .symrate = 2170000, .fec = 9, .polarisation = 'V'},
	{.freq = 3644000, .symrate = 2534000, .fec = 9, .polarisation = 'H'},
	{.freq = 3695000, .symrate = 2963000, .fec = 9, .polarisation = 'H'},
	{.freq = 4082000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 4143000, .symrate = 4800000, .fec = 9, .polarisation = 'V'},
	{.freq = 3641000, .symrate = 2666000, .fec = 9, .polarisation = 'V'},
	{.freq = 3991000, .symrate = 3578000, .fec = 9, .polarisation = 'H'},
	{.freq = 4000000, .symrate = 2450000, .fec = 9, .polarisation = 'H'},
	{.freq = 4009000, .symrate = 2450000, .fec = 9, .polarisation = 'H'},
	{.freq = 3960000, .symrate = 3170000, .fec = 9, .polarisation = 'V'},
	{.freq = 3755000, .symrate = 20000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4135000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 4005000, .symrate = 2450000, .fec = 9, .polarisation = 'H'},
	{.freq = 4003000, .symrate = 2450000, .fec = 9, .polarisation = 'H'},
	{.freq = 3600000, .symrate = 29185000, .fec = 9, .polarisation = 'H'},
	{.freq = 3868000, .symrate = 2100000, .fec = 9, .polarisation = 'V'},
	{.freq = 3685000, .symrate = 6500000, .fec = 9, .polarisation = 'H'},
	{.freq = 3688000, .symrate = 6666000, .fec = 9, .polarisation = 'V'},
	{.freq = 3679000, .symrate = 2220000, .fec = 9, .polarisation = 'V'},
	{.freq = 3961000, .symrate = 1481000, .fec = 9, .polarisation = 'H'},
	{.freq = 4130000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3923000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 4052000, .symrate = 2459000, .fec = 9, .polarisation = 'V'},
	{.freq = 4071000, .symrate = 3333000, .fec = 9, .polarisation = 'V'},
	{.freq = 4146000, .symrate = 2571000, .fec = 9, .polarisation = 'H'},
	{.freq = 3646000, .symrate = 3978000, .fec = 9, .polarisation = 'V'},
	{.freq = 3837000, .symrate = 19510000, .fec = 9, .polarisation = 'H'},
	{.freq = 4132000, .symrate = 2480000, .fec = 9, .polarisation = 'H'},
	{.freq = 3653000, .symrate = 5924000, .fec = 9, .polarisation = 'V'},
	{.freq = 4055000, .symrate = 7233000, .fec = 9, .polarisation = 'H'},
	{.freq = 3938000, .symrate = 4785000, .fec = 9, .polarisation = 'V'},
	{.freq = 4152000, .symrate = 3280000, .fec = 9, .polarisation = 'V'},
	{.freq = 3832000, .symrate = 13310000, .fec = 9, .polarisation = 'V'},
	{.freq = 4090000, .symrate = 6620000, .fec = 9, .polarisation = 'H'},
	{.freq = 4082000, .symrate = 6510000, .fec = 9, .polarisation = 'V'},
	{.freq = 4107000, .symrate = 2100000, .fec = 9, .polarisation = 'V'},
	{.freq = 3500000, .symrate = 6666000, .fec = 9, .polarisation = 'H'},
	{.freq = 4126000, .symrate = 2531000, .fec = 9, .polarisation = 'V'},
	{.freq = 3860000, .symrate = 2713000, .fec = 9, .polarisation = 'V'},
	{.freq = 3983000, .symrate = 2222000, .fec = 9, .polarisation = 'H'},
	{.freq = 4065000, .symrate = 8400000, .fec = 9, .polarisation = 'H'},
	{.freq = 4063000, .symrate = 8500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3972000, .symrate = 3330000, .fec = 9, .polarisation = 'H'},
	{.freq = 4024000, .symrate = 16030000, .fec = 9, .polarisation = 'H'},
	{.freq = 3803000, .symrate = 26860000, .fec = 9, .polarisation = 'V'},
	{.freq = 3920000, .symrate = 20000000, .fec = 9, .polarisation = 'V'},
	{.freq = 4053000, .symrate = 6666000, .fec = 9, .polarisation = 'V'},
	{.freq = 3676000, .symrate = 3000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3637000, .symrate = 2963000, .fec = 9, .polarisation = 'H'},
	{.freq = 3516000, .symrate = 5632000, .fec = 9, .polarisation = 'V'},
	{.freq = 3516000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 4093000, .symrate = 2887000, .fec = 9, .polarisation = 'V'},
	{.freq = 4177000, .symrate = 4391000, .fec = 9, .polarisation = 'H'},
	{.freq = 4124000, .symrate = 3480000, .fec = 9, .polarisation = 'H'},
	{.freq = 4139000, .symrate = 2220000, .fec = 9, .polarisation = 'V'},
	{.freq = 3693000, .symrate = 4441000, .fec = 9, .polarisation = 'V'},
	{.freq = 4194000, .symrate = 6660000, .fec = 9, .polarisation = 'H'},
	{.freq = 3803000, .symrate = 27500000, .fec = 9, .polarisation = 'H'},
	{.freq = 4112000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3676000, .symrate = 5900000, .fec = 9, .polarisation = 'H'},
	{.freq = 3652000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3725000, .symrate = 26669000, .fec = 9, .polarisation = 'H'},
	{.freq = 3898000, .symrate = 4195000, .fec = 9, .polarisation = 'V'},
	{.freq = 4162000, .symrate = 7200000, .fec = 9, .polarisation = 'H'},
	{.freq = 4168000, .symrate = 2400000, .fec = 9, .polarisation = 'H'},
	{.freq = 3965000, .symrate = 3332000, .fec = 9, .polarisation = 'H'},
	{.freq = 4100000, .symrate = 6654000, .fec = 9, .polarisation = 'H'},
	{.freq = 4127000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4109000, .symrate = 6654000, .fec = 9, .polarisation = 'H'},
	{.freq = 3670000, .symrate = 2960000, .fec = 9, .polarisation = 'H'},
	{.freq = 3853000, .symrate = 5900000, .fec = 9, .polarisation = 'H'},
	{.freq = 3660000, .symrate = 2540000, .fec = 9, .polarisation = 'V'},
	{.freq = 4170000, .symrate = 2222000, .fec = 9, .polarisation = 'H'},
	{.freq = 3630000, .symrate = 5632000, .fec = 9, .polarisation = 'H'},
	{.freq = 3695000, .symrate = 2220000, .fec = 9, .polarisation = 'V'},
	{.freq = 4184000, .symrate = 6142000, .fec = 9, .polarisation = 'H'},
	{.freq = 4122000, .symrate = 1860000, .fec = 9, .polarisation = 'V'},
	{.freq = 4022000, .symrate = 3800000, .fec = 9, .polarisation = 'V'},
	{.freq = 4137000, .symrate = 4400000, .fec = 9, .polarisation = 'H'},
	{.freq = 3878000, .symrate = 22117000, .fec = 9, .polarisation = 'V'},
	{.freq = 4132000, .symrate = 2800000, .fec = 9, .polarisation = 'V'},
	{.freq = 4043000, .symrate = 7440000, .fec = 9, .polarisation = 'V'},
	{.freq = 4142000, .symrate = 2222000, .fec = 9, .polarisation = 'H'},
	{.freq = 3990000, .symrate = 4195000, .fec = 9, .polarisation = 'V'},
	{.freq = 3667000, .symrate = 3340000, .fec = 9, .polarisation = 'V'},
	{.freq = 3673000, .symrate = 3000000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_OptusC1_156E[] = {
	{.freq = 12278000, .symrate = 30000000, .fec = 2, .polarisation = 'H'},
	{.freq = 12305000, .symrate = 30000000, .fec = 2, .polarisation = 'H'},
	{.freq = 12358000, .symrate = 27000000, .fec = 2, .polarisation = 'H'},
	{.freq = 12398000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12407000, .symrate = 30000000, .fec = 2, .polarisation = 'V'},
	{.freq = 12438000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12487000, .symrate = 27800000, .fec = 3, .polarisation = 'V'},
	{.freq = 12501000, .symrate = 29473000, .fec = 3, .polarisation = 'H'},
	{.freq = 12518000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12527000, .symrate = 30000000, .fec = 3, .polarisation = 'V'},
	{.freq = 12558000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12564000, .symrate = 29473000, .fec = 3, .polarisation = 'H'},
	{.freq = 12567000, .symrate = 27800000, .fec = 2, .polarisation = 'V'},
	{.freq = 12598000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12607000, .symrate = 29473000, .fec = 3, .polarisation = 'V'},
	{.freq = 12638000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12689000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12720000, .symrate = 30000000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_OptusD1_160_0E[] = {
	{.freq = 12456000, .symrate = 22500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12483000, .symrate = 22500000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_PalapaC2_C_113_0E[] = {
	{.freq = 3745000, .symrate = 3125000, .fec = 3, .polarisation = 'H'},
	{.freq = 3758000, .symrate = 4470000, .fec = 3, .polarisation = 'H'},
	{.freq = 3765000, .symrate = 5555000, .fec = 3, .polarisation = 'H'},
	{.freq = 3774000, .symrate = 6520000, .fec = 3, .polarisation = 'H'},
	{.freq = 3836000, .symrate = 2097000, .fec = 3, .polarisation = 'V'},
	{.freq = 3926000, .symrate = 4208000, .fec = 3, .polarisation = 'H'},
	{.freq = 4054000, .symrate = 5632000, .fec = 3, .polarisation = 'H'},
	{.freq = 4055000, .symrate = 6510000, .fec = 3, .polarisation = 'V'},
	{.freq = 4074000, .symrate = 6500000, .fec = 3, .polarisation = 'V'},
	{.freq = 4080000, .symrate = 28125000, .fec = 3, .polarisation = 'H'},
	{.freq = 4184000, .symrate = 6700000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_PAS_43_0W[] = {
	{.freq = 12578000, .symrate = 19850000, .fec = 3, .polarisation = 'H'},
	{.freq = 12584000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12606000, .symrate = 6616000, .fec = 3, .polarisation = 'H'},
	{.freq = 12665000, .symrate = 19850000, .fec = 7, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Satmex_5_116_8W[] = {
	{.freq = 12034000, .symrate = 2532000, .fec = 9, .polarisation = 'H'},
	{.freq = 12175000, .symrate = 4232000, .fec = 9, .polarisation = 'H'},
	{.freq = 4060000, .symrate = 19510000, .fec = 9, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 2963000, .fec = 9, .polarisation = 'V'},
	{.freq = 4084000, .symrate = 3162000, .fec = 9, .polarisation = 'H'},
	{.freq = 3767000, .symrate = 1620000, .fec = 9, .polarisation = 'H'},
	{.freq = 11975000, .symrate = 5000000, .fec = 9, .polarisation = 'H'},
	{.freq = 11960000, .symrate = 2000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4180000, .symrate = 19510000, .fec = 9, .polarisation = 'H'},
	{.freq = 4115000, .symrate = 3253000, .fec = 9, .polarisation = 'H'},
	{.freq = 3840000, .symrate = 29270000, .fec = 9, .polarisation = 'V'},
	{.freq = 4160000, .symrate = 29270000, .fec = 9, .polarisation = 'V'},
	{.freq = 3940000, .symrate = 28125000, .fec = 9, .polarisation = 'H'},
	{.freq = 4052000, .symrate = 4307000, .fec = 9, .polarisation = 'V'},
	{.freq = 12024000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 4134000, .symrate = 3617000, .fec = 9, .polarisation = 'H'},
	{.freq = 12028000, .symrate = 3255000, .fec = 9, .polarisation = 'H'},
	{.freq = 12060000, .symrate = 3078000, .fec = 9, .polarisation = 'V'},
	{.freq = 4012000, .symrate = 3131000, .fec = 9, .polarisation = 'V'},
	{.freq = 3949000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 4076000, .symrate = 2962000, .fec = 9, .polarisation = 'H'},
	{.freq = 3749000, .symrate = 4070000, .fec = 9, .polarisation = 'H'},
	{.freq = 3744000, .symrate = 4480000, .fec = 9, .polarisation = 'H'},
	{.freq = 3755000, .symrate = 4000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3869000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3876000, .symrate = 2170000, .fec = 9, .polarisation = 'H'},
	{.freq = 3879000, .symrate = 1984000, .fec = 9, .polarisation = 'H'},
	{.freq = 3957000, .symrate = 2600000, .fec = 9, .polarisation = 'V'},
	{.freq = 3975000, .symrate = 3131000, .fec = 9, .polarisation = 'V'},
	{.freq = 3982000, .symrate = 2531000, .fec = 9, .polarisation = 'H'},
	{.freq = 3832000, .symrate = 2500000, .fec = 9, .polarisation = 'H'},
	{.freq = 3953000, .symrate = 2597000, .fec = 9, .polarisation = 'V'},
	{.freq = 3987000, .symrate = 8860000, .fec = 9, .polarisation = 'V'},
	{.freq = 3996000, .symrate = 2170000, .fec = 9, .polarisation = 'V'},
	{.freq = 3748000, .symrate = 2100000, .fec = 9, .polarisation = 'V'},
	{.freq = 3932000, .symrate = 2500000, .fec = 9, .polarisation = 'V'},
	{.freq = 3914000, .symrate = 3223000, .fec = 9, .polarisation = 'V'},
	{.freq = 3888000, .symrate = 5351000, .fec = 9, .polarisation = 'H'},
	{.freq = 3805000, .symrate = 4679000, .fec = 9, .polarisation = 'V'},
	{.freq = 4001000, .symrate = 4100000, .fec = 9, .polarisation = 'V'},
	{.freq = 4023000, .symrate = 6400000, .fec = 9, .polarisation = 'H'},
	{.freq = 12176000, .symrate = 3985000, .fec = 9, .polarisation = 'H'},
	{.freq = 4108000, .symrate = 2666000, .fec = 9, .polarisation = 'H'},
	{.freq = 4038000, .symrate = 7675000, .fec = 9, .polarisation = 'V'},
	{.freq = 3809000, .symrate = 3100000, .fec = 9, .polarisation = 'V'},
	{.freq = 4080000, .symrate = 29270000, .fec = 9, .polarisation = 'V'},
	{.freq = 3910000, .symrate = 2500000, .fec = 9, .polarisation = 'H'},
	{.freq = 4164000, .symrate = 1733000, .fec = 9, .polarisation = 'H'},
	{.freq = 12193000, .symrate = 7885000, .fec = 9, .polarisation = 'V'},
	{.freq = 12164000, .symrate = 2000000, .fec = 9, .polarisation = 'V'},
	{.freq = 12044000, .symrate = 4340000, .fec = 9, .polarisation = 'V'},
	{.freq = 3720000, .symrate = 27000000, .fec = 9, .polarisation = 'V'},
	{.freq = 3922000, .symrate = 9760000, .fec = 9, .polarisation = 'V'},
	{.freq = 3797000, .symrate = 3200000, .fec = 9, .polarisation = 'V'},
	{.freq = 3968000, .symrate = 7500000, .fec = 9, .polarisation = 'V'},
	{.freq = 4037000, .symrate = 2222000, .fec = 9, .polarisation = 'H'},
	{.freq = 4046000, .symrate = 2441000, .fec = 9, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 2441000, .fec = 9, .polarisation = 'H'},
	{.freq = 3773000, .symrate = 2892000, .fec = 9, .polarisation = 'V'},
	{.freq = 3767000, .symrate = 2893000, .fec = 9, .polarisation = 'V'},
	{.freq = 3935000, .symrate = 4440000, .fec = 9, .polarisation = 'V'},
	{.freq = 3972000, .symrate = 3364000, .fec = 9, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 2400000, .fec = 9, .polarisation = 'H'},
	{.freq = 4007000, .symrate = 5582000, .fec = 9, .polarisation = 'V'},
	{.freq = 3885000, .symrate = 3000000, .fec = 9, .polarisation = 'H'},
	{.freq = 3826000, .symrate = 2712000, .fec = 9, .polarisation = 'H'},
	{.freq = 3944000, .symrate = 3410000, .fec = 9, .polarisation = 'V'},
	{.freq = 3915000, .symrate = 1520000, .fec = 9, .polarisation = 'H'},
	{.freq = 3876000, .symrate = 2920000, .fec = 9, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Satmex_6_113_0W[] = {
	{.freq = 4078000, .symrate = 3609000, .fec = 9, .polarisation = 'H'},
	{.freq = 4080000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 12145000, .symrate = 3255000, .fec = 9, .polarisation = 'V'},
	{.freq = 4085000, .symrate = 2821000, .fec = 9, .polarisation = 'V'},
	{.freq = 12126000, .symrate = 6022000, .fec = 9, .polarisation = 'V'},
	{.freq = 12166000, .symrate = 17500000, .fec = 9, .polarisation = 'H'},
	{.freq = 4075000, .symrate = 3782000, .fec = 9, .polarisation = 'V'},
	{.freq = 4091000, .symrate = 3720000, .fec = 9, .polarisation = 'V'},
	{.freq = 3985000, .symrate = 2300000, .fec = 9, .polarisation = 'V'},
	{.freq = 12126000, .symrate = 2170000, .fec = 9, .polarisation = 'H'},
	{.freq = 12157000, .symrate = 3038000, .fec = 9, .polarisation = 'V'},
	{.freq = 12091000, .symrate = 3337000, .fec = 9, .polarisation = 'V'},
	{.freq = 3947000, .symrate = 3700000, .fec = 9, .polarisation = 'V'},
	{.freq = 3761000, .symrate = 2120000, .fec = 9, .polarisation = 'V'},
	{.freq = 12107000, .symrate = 2222000, .fec = 9, .polarisation = 'V'},
	{.freq = 12080000, .symrate = 25635000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_SBS6_74w[] = {
	{.freq = 11744000, .symrate = 6616000, .fec = 9, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Sirius_5_0E[] = {
	{.freq = 11215000, .symrate = 6666000, .fec = 1, .polarisation = 'V'},
	{.freq = 11227000, .symrate = 23145000, .fec = 3, .polarisation = 'H'},
	{.freq = 11247000, .symrate = 23145000, .fec = 3, .polarisation = 'V'},
	{.freq = 11420000, .symrate = 23145000, .fec = 3, .polarisation = 'H'},
	{.freq = 11727000, .symrate = 27500000, .fec = 5, .polarisation = 'H'},
	{.freq = 11747000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11766000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11785000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11804000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11823000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11843000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11862000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11881000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11900000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11919000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11938000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11958000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11977000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11996000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 27500000, .fec = 5, .polarisation = 'V'},
	{.freq = 12034000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12054000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12073000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12092000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12111000, .symrate = 27500000, .fec = 5, .polarisation = 'H'},
	{.freq = 12130000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12149000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12169000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12188000, .symrate = 27500000, .fec = 7, .polarisation = 'H'},
	{.freq = 12207000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12226000, .symrate = 25540000, .fec = 7, .polarisation = 'H'},
	{.freq = 12245000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12265000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12284000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12303000, .symrate = 25547000, .fec = 7, .polarisation = 'H'},
	{.freq = 12322000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12341000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12360000, .symrate = 27500000, .fec = 7, .polarisation = 'V'},
	{.freq = 12379000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12399000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12418000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12437000, .symrate = 27500000, .fec = 2, .polarisation = 'V'},
	{.freq = 12456000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12476000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12608000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 12637000, .symrate = 14465000, .fec = 3, .polarisation = 'H'},
	{.freq = 12668000, .symrate = 6666000, .fec = 1, .polarisation = 'V'},
	{.freq = 12672000, .symrate = 3300000, .fec = 3, .polarisation = 'H'},
	{.freq = 12674000, .symrate = 10000000, .fec = 3, .polarisation = 'V'},
	{.freq = 12678000, .symrate = 13333000, .fec = 5, .polarisation = 'V'},
	{.freq = 12680000, .symrate = 9404000, .fec = 3, .polarisation = 'H'},
	{.freq = 12685000, .symrate = 4444000, .fec = 3, .polarisation = 'V'},
	{.freq = 12690000, .symrate = 3330000, .fec = 3, .polarisation = 'H'},
	{.freq = 12693000, .symrate = 3333000, .fec = 5, .polarisation = 'V'},
	{.freq = 12701000, .symrate = 6111000, .fec = 3, .polarisation = 'H'},
	{.freq = 12715000, .symrate = 3330000, .fec = 3, .polarisation = 'H'},
	{.freq = 12718000, .symrate = 23500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12724000, .symrate = 1772000, .fec = 3, .polarisation = 'H'},
	{.freq = 12728000, .symrate = 19720000, .fec = 3, .polarisation = 'V'},
	{.freq = 12737000, .symrate = 6150000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_ST1_C_80_0E[] = {
	{.freq = 3469000, .symrate = 30000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3521000, .symrate = 8332000, .fec = 5, .polarisation = 'H'},
	{.freq = 3550000, .symrate = 30000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3595000, .symrate = 30000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3632000, .symrate = 30000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3671000, .symrate = 9256000, .fec = 3, .polarisation = 'H'},
	{.freq = 3689000, .symrate = 3000000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Telecom2_8_0W[] = {
	{.freq = 10972000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 10974000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11010000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11011000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11042000, .symrate = 7860000, .fec = 3, .polarisation = 'H'},
	{.freq = 11053000, .symrate = 2850000, .fec = 3, .polarisation = 'H'},
	{.freq = 11064000, .symrate = 12667000, .fec = 3, .polarisation = 'H'},
	{.freq = 11387000, .symrate = 24740000, .fec = 5, .polarisation = 'H'},
	{.freq = 11428000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 11500000, .symrate = 3460000, .fec = 9, .polarisation = 'H'},
	{.freq = 11554000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 11595000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12545000, .symrate = 3038000, .fec = 3, .polarisation = 'H'},
	{.freq = 12566000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
	{.freq = 12649000, .symrate = 27500000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Telkom1_C_108_0E[] = {
	{.freq = 3580000, .symrate = 28000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3620000, .symrate = 28000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3640000, .symrate = 28000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3776000, .symrate = 4285000, .fec = 3, .polarisation = 'H'},
	{.freq = 3785000, .symrate = 4000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3793000, .symrate = 3000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3797000, .symrate = 3900000, .fec = 3, .polarisation = 'H'},
	{.freq = 3807000, .symrate = 4000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3990000, .symrate = 6000000, .fec = 3, .polarisation = 'H'},
	{.freq = 4015000, .symrate = 6000000, .fec = 3, .polarisation = 'H'},
	{.freq = 4025000, .symrate = 3000000, .fec = 3, .polarisation = 'H'},
	{.freq = 4084000, .symrate = 6000000, .fec = 3, .polarisation = 'H'},
	{.freq = 4092000, .symrate = 3570000, .fec = 3, .polarisation = 'H'},
	{.freq = 4097000, .symrate = 3125000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Telstar10_C_76_5E[] = {
	{.freq = 3646000, .symrate = 5064000, .fec = 3, .polarisation = 'H'},
	{.freq = 3655000, .symrate = 3214000, .fec = 3, .polarisation = 'H'},
	{.freq = 3680000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3760000, .symrate = 28066000, .fec = 2, .polarisation = 'H'},
	{.freq = 3780000, .symrate = 28066000, .fec = 2, .polarisation = 'V'},
	{.freq = 3787000, .symrate = 4600000, .fec = 2, .polarisation = 'H'},
	{.freq = 3793000, .symrate = 4340000, .fec = 3, .polarisation = 'H'},
	{.freq = 3840000, .symrate = 27500000, .fec = 3, .polarisation = 'H'},
	{.freq = 4003000, .symrate = 4080000, .fec = 3, .polarisation = 'H'},
	{.freq = 4009000, .symrate = 4300000, .fec = 3, .polarisation = 'H'},
	{.freq = 4030000, .symrate = 6312000, .fec = 1, .polarisation = 'V'},
	{.freq = 4034000, .symrate = 4300000, .fec = 3, .polarisation = 'H'},
	{.freq = 4049000, .symrate = 3600000, .fec = 3, .polarisation = 'H'},
	{.freq = 4056000, .symrate = 3214000, .fec = 3, .polarisation = 'H'},
	{.freq = 4073000, .symrate = 13020000, .fec = 1, .polarisation = 'H'},
	{.freq = 4105000, .symrate = 3125000, .fec = 3, .polarisation = 'H'},
	{.freq = 4137000, .symrate = 2893000, .fec = 3, .polarisation = 'H'},
	{.freq = 4146000, .symrate = 3600000, .fec = 3, .polarisation = 'H'},
	{.freq = 4173000, .symrate = 7400000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Telstar12_15_0W[] = {
	{.freq = 10988000, .symrate = 2893000, .fec = 3, .polarisation = 'H'},
	{.freq = 10992000, .symrate = 2893000, .fec = 3, .polarisation = 'H'},
	{.freq = 11060000, .symrate = 19279000, .fec = 3, .polarisation = 'H'},
	{.freq = 11123000, .symrate = 19279000, .fec = 3, .polarisation = 'V'},
	{.freq = 11124000, .symrate = 18386000, .fec = 3, .polarisation = 'H'},
	{.freq = 11150000, .symrate = 18386000, .fec = 3, .polarisation = 'H'},
	{.freq = 11150000, .symrate = 19279000, .fec = 3, .polarisation = 'V'},
	{.freq = 11531000, .symrate = 3198000, .fec = 7, .polarisation = 'H'},
	{.freq = 11566000, .symrate = 4284000, .fec = 3, .polarisation = 'H'},
	{.freq = 11677000, .symrate = 2200000, .fec = 1, .polarisation = 'V'},
	{.freq = 11709000, .symrate = 3195000, .fec = 9, .polarisation = 'V'},
	{.freq = 11806000, .symrate = 7596000, .fec = 3, .polarisation = 'V'},
	{.freq = 11963000, .symrate = 14714000, .fec = 3, .polarisation = 'H'},
	{.freq = 12082000, .symrate = 3935000, .fec = 3, .polarisation = 'H'},
	{.freq = 12117000, .symrate = 4772000, .fec = 9, .polarisation = 'H'},
	{.freq = 12159000, .symrate = 7777000, .fec = 2, .polarisation = 'H'},
	{.freq = 12166000, .symrate = 3503000, .fec = 3, .polarisation = 'H'},
	{.freq = 12173000, .symrate = 3503000, .fec = 3, .polarisation = 'H'},
	{.freq = 12524000, .symrate = 26470000, .fec = 3, .polarisation = 'V'},
	{.freq = 12547000, .symrate = 11900000, .fec = 2, .polarisation = 'H'},
	{.freq = 12589000, .symrate = 4503000, .fec = 1, .polarisation = 'V'},
	{.freq = 12608000, .symrate = 19279000, .fec = 2, .polarisation = 'H'},
	{.freq = 12615000, .symrate = 11939000, .fec = 2, .polarisation = 'V'},
	{.freq = 12645000, .symrate = 3255000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Telstar18_C_138_0E[] = {
	{.freq = 3599000, .symrate = 30000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3660000, .symrate = 30000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3704000, .symrate = 3034000, .fec = 3, .polarisation = 'V'},
	{.freq = 4124000, .symrate = 3255000, .fec = 1, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Thaicom1A_C_120_0E[] = {
	{.freq = 3725000, .symrate = 3255000, .fec = 3, .polarisation = 'V'},
	{.freq = 3745000, .symrate = 2850000, .fec = 5, .polarisation = 'V'},
	{.freq = 3854000, .symrate = 9628000, .fec = 3, .polarisation = 'V'},
	{.freq = 3881000, .symrate = 2000000, .fec = 3, .polarisation = 'V'},
	{.freq = 3916000, .symrate = 3359000, .fec = 3, .polarisation = 'V'},
	{.freq = 4135000, .symrate = 1808000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Thaicom2_C_78_5E[] = {
	{.freq = 3764000, .symrate = 5900000, .fec = 3, .polarisation = 'H'},
	{.freq = 3803000, .symrate = 1445000, .fec = 7, .polarisation = 'H'},
	{.freq = 3888000, .symrate = 15625000, .fec = 3, .polarisation = 'H'},
	{.freq = 3905000, .symrate = 6250000, .fec = 3, .polarisation = 'H'},
	{.freq = 3967000, .symrate = 4551000, .fec = 3, .polarisation = 'H'},
	{.freq = 4003000, .symrate = 1085000, .fec = 3, .polarisation = 'H'},
	{.freq = 4016000, .symrate = 2305000, .fec = 7, .polarisation = 'H'},
	{.freq = 4106000, .symrate = 4688000, .fec = 3, .polarisation = 'H'},
	{.freq = 4145000, .symrate = 4815000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Thaicom5_C_78_5E[] = {
	{.freq = 3421000, .symrate = 6667000, .fec = 3, .polarisation = 'V'},
	{.freq = 3431000, .symrate = 13333000, .fec = 3, .polarisation = 'H'},
	{.freq = 3432000, .symrate = 6667000, .fec = 3, .polarisation = 'V'},
	{.freq = 3448000, .symrate = 2222000, .fec = 3, .polarisation = 'V'},
	{.freq = 3450000, .symrate = 13333000, .fec = 3, .polarisation = 'H'},
	{.freq = 3454000, .symrate = 3333000, .fec = 3, .polarisation = 'V'},
	{.freq = 3460000, .symrate = 1538000, .fec = 3, .polarisation = 'V'},
	{.freq = 3489000, .symrate = 3333000, .fec = 3, .polarisation = 'V'},
	{.freq = 3502000, .symrate = 4444000, .fec = 2, .polarisation = 'V'},
	{.freq = 3504000, .symrate = 3367000, .fec = 2, .polarisation = 'H'},
	{.freq = 3505000, .symrate = 1852000, .fec = 3, .polarisation = 'V'},
	{.freq = 3521000, .symrate = 2917000, .fec = 3, .polarisation = 'V'},
	{.freq = 3545000, .symrate = 30000000, .fec = 5, .polarisation = 'V'},
	{.freq = 3551000, .symrate = 13333000, .fec = 3, .polarisation = 'H'},
	{.freq = 3562000, .symrate = 4444000, .fec = 1, .polarisation = 'H'},
	{.freq = 3574000, .symrate = 7492000, .fec = 2, .polarisation = 'H'},
	{.freq = 3585000, .symrate = 26667000, .fec = 3, .polarisation = 'V'},
	{.freq = 3600000, .symrate = 26667000, .fec = 3, .polarisation = 'H'},
	{.freq = 3617000, .symrate = 12500000, .fec = 3, .polarisation = 'V'},
	{.freq = 3640000, .symrate = 28066000, .fec = 3, .polarisation = 'H'},
	{.freq = 3664000, .symrate = 3704000, .fec = 3, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Thor_1_0W[] = {
	{.freq = 11216000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11229000, .symrate = 24500000, .fec = 7, .polarisation = 'H'},
	{.freq = 11247000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11261000, .symrate = 24500000, .fec = 7, .polarisation = 'H'},
	{.freq = 11278000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11293000, .symrate = 24500000, .fec = 7, .polarisation = 'H'},
	{.freq = 11309000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11325000, .symrate = 24500000, .fec = 7, .polarisation = 'H'},
	{.freq = 11325000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11325000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11341000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11357000, .symrate = 24500000, .fec = 7, .polarisation = 'H'},
	{.freq = 11372000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11389000, .symrate = 24500000, .fec = 7, .polarisation = 'H'},
	{.freq = 11403000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11421000, .symrate = 24500000, .fec = 7, .polarisation = 'H'},
	{.freq = 11434000, .symrate = 24500000, .fec = 7, .polarisation = 'V'},
	{.freq = 11747000, .symrate = 28000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11785000, .symrate = 28000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11823000, .symrate = 28000000, .fec = 7, .polarisation = 'H'},
	{.freq = 11938000, .symrate = 25000000, .fec = 3, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 30000000, .fec = 3, .polarisation = 'H'},
	{.freq = 12130000, .symrate = 30000000, .fec = 3, .polarisation = 'H'},
	{.freq = 12149000, .symrate = 28000000, .fec = 5, .polarisation = 'V'},
	{.freq = 12169000, .symrate = 28000000, .fec = 7, .polarisation = 'H'},
	{.freq = 12188000, .symrate = 25000000, .fec = 3, .polarisation = 'V'},
	{.freq = 12226000, .symrate = 28000000, .fec = 3, .polarisation = 'V'},
	{.freq = 12245000, .symrate = 28000000, .fec = 5, .polarisation = 'H'},
	{.freq = 12303000, .symrate = 28000000, .fec = 5, .polarisation = 'V'},
	{.freq = 12322000, .symrate = 27800000, .fec = 3, .polarisation = 'H'},
	{.freq = 12399000, .symrate = 28000000, .fec = 7, .polarisation = 'H'},
	{.freq = 12418000, .symrate = 28000000, .fec = 7, .polarisation = 'V'},
	{.freq = 12456000, .symrate = 28000000, .fec = 3, .polarisation = 'V'},
	{.freq = 12476000, .symrate = 28000000, .fec = 5, .polarisation = 'H'},
};

static const struct mux muxes_DVBS_Turksat_42_0E[] = {
	{.freq = 10968000, .symrate = 4557000, .fec = 3, .polarisation = 'V'},
	{.freq = 10970000, .symrate = 30000000, .fec = 5, .polarisation = 'H'},
	{.freq = 10999000, .symrate = 2222000, .fec = 3, .polarisation = 'V'},
	{.freq = 11003000, .symrate = 2175000, .fec = 3, .polarisation = 'V'},
	{.freq = 11007000, .symrate = 2950000, .fec = 5, .polarisation = 'V'},
	{.freq = 11011000, .symrate = 2125000, .fec = 5, .polarisation = 'V'},
	{.freq = 11014000, .symrate = 2050000, .fec = 3, .polarisation = 'V'},
	{.freq = 11018000, .symrate = 2150000, .fec = 3, .polarisation = 'V'},
	{.freq = 11028000, .symrate = 2400000, .fec = 3, .polarisation = 'V'},
	{.freq = 11083000, .symrate = 8888000, .fec = 3, .polarisation = 'V'},
	{.freq = 11136000, .symrate = 2170000, .fec = 3, .polarisation = 'V'},
	{.freq = 11143000, .symrate = 2200000, .fec = 3, .polarisation = 'V'},
	{.freq = 11159000, .symrate = 2596000, .fec = 3, .polarisation = 'V'},
	{.freq = 11162000, .symrate = 2222000, .fec = 5, .polarisation = 'V'},
	{.freq = 11166000, .symrate = 2960000, .fec = 3, .polarisation = 'V'},
	{.freq = 11177000, .symrate = 2200000, .fec = 3, .polarisation = 'V'},
	{.freq = 11554000, .symrate = 2916000, .fec = 2, .polarisation = 'H'},
	{.freq = 11576000, .symrate = 2400000, .fec = 5, .polarisation = 'H'},
	{.freq = 11581000, .symrate = 4444000, .fec = 3, .polarisation = 'H'},
	{.freq = 11607000, .symrate = 3750000, .fec = 2, .polarisation = 'H'},
	{.freq = 11712000, .symrate = 2963000, .fec = 3, .polarisation = 'V'},
	{.freq = 11716000, .symrate = 2222000, .fec = 3, .polarisation = 'V'},
	{.freq = 11729000, .symrate = 15555000, .fec = 5, .polarisation = 'V'},
	{.freq = 11734000, .symrate = 3291000, .fec = 3, .polarisation = 'H'},
	{.freq = 11739000, .symrate = 3125000, .fec = 3, .polarisation = 'H'},
	{.freq = 11743000, .symrate = 2222000, .fec = 3, .polarisation = 'H'},
	{.freq = 11743000, .symrate = 2222000, .fec = 3, .polarisation = 'V'},
	{.freq = 11748000, .symrate = 4444000, .fec = 5, .polarisation = 'H'},
	{.freq = 11753000, .symrate = 3000000, .fec = 7, .polarisation = 'H'},
	{.freq = 11754000, .symrate = 3900000, .fec = 5, .polarisation = 'V'},
	{.freq = 11758000, .symrate = 2962000, .fec = 3, .polarisation = 'V'},
	{.freq = 11760000, .symrate = 5925000, .fec = 3, .polarisation = 'H'},
	{.freq = 11762000, .symrate = 2155000, .fec = 3, .polarisation = 'V'},
	{.freq = 11765000, .symrate = 2222000, .fec = 3, .polarisation = 'V'},
	{.freq = 11770000, .symrate = 2177000, .fec = 3, .polarisation = 'V'},
	{.freq = 11773000, .symrate = 2420000, .fec = 5, .polarisation = 'V'},
	{.freq = 11775000, .symrate = 2222000, .fec = 3, .polarisation = 'H'},
	{.freq = 11777000, .symrate = 3150000, .fec = 5, .polarisation = 'V'},
	{.freq = 11781000, .symrate = 2815000, .fec = 5, .polarisation = 'V'},
	{.freq = 11794000, .symrate = 5632000, .fec = 3, .polarisation = 'H'},
	{.freq = 11800000, .symrate = 2400000, .fec = 5, .polarisation = 'H'},
	{.freq = 11804000, .symrate = 24444000, .fec = 5, .polarisation = 'V'},
	{.freq = 11830000, .symrate = 6666000, .fec = 3, .polarisation = 'V'},
	{.freq = 11839000, .symrate = 4444000, .fec = 3, .polarisation = 'V'},
	{.freq = 11846000, .symrate = 3333000, .fec = 5, .polarisation = 'V'},
	{.freq = 11852000, .symrate = 4444000, .fec = 5, .polarisation = 'V'},
	{.freq = 11858000, .symrate = 2400000, .fec = 7, .polarisation = 'V'},
	{.freq = 11867000, .symrate = 4444000, .fec = 5, .polarisation = 'V'},
	{.freq = 11874000, .symrate = 3400000, .fec = 7, .polarisation = 'V'},
	{.freq = 11878000, .symrate = 3750000, .fec = 5, .polarisation = 'V'},
	{.freq = 11882000, .symrate = 2965000, .fec = 5, .polarisation = 'V'},
	{.freq = 11887000, .symrate = 3333000, .fec = 7, .polarisation = 'V'},
	{.freq = 11892000, .symrate = 12800000, .fec = 5, .polarisation = 'H'},
	{.freq = 11892000, .symrate = 2960000, .fec = 5, .polarisation = 'V'},
	{.freq = 11896000, .symrate = 2222000, .fec = 3, .polarisation = 'V'},
	{.freq = 11905000, .symrate = 6666000, .fec = 3, .polarisation = 'H'},
	{.freq = 11912000, .symrate = 3333000, .fec = 5, .polarisation = 'H'},
	{.freq = 11919000, .symrate = 24444000, .fec = 3, .polarisation = 'V'},
	{.freq = 11951000, .symrate = 8800000, .fec = 5, .polarisation = 'V'},
	{.freq = 11959000, .symrate = 2960000, .fec = 3, .polarisation = 'V'},
	{.freq = 11963000, .symrate = 2300000, .fec = 5, .polarisation = 'V'},
	{.freq = 11967000, .symrate = 4340000, .fec = 3, .polarisation = 'V'},
	{.freq = 11970000, .symrate = 17900000, .fec = 5, .polarisation = 'H'},
	{.freq = 11973000, .symrate = 2240000, .fec = 5, .polarisation = 'V'},
	{.freq = 11984000, .symrate = 4000000, .fec = 5, .polarisation = 'H'},
	{.freq = 11996000, .symrate = 26000000, .fec = 5, .polarisation = 'V'},
	{.freq = 12002000, .symrate = 4800000, .fec = 5, .polarisation = 'H'},
	{.freq = 12008000, .symrate = 4400000, .fec = 5, .polarisation = 'H'},
	{.freq = 12015000, .symrate = 4800000, .fec = 5, .polarisation = 'H'},
	{.freq = 12022000, .symrate = 5380000, .fec = 5, .polarisation = 'H'},
	{.freq = 12028000, .symrate = 4557000, .fec = 5, .polarisation = 'H'},
	{.freq = 12126000, .symrate = 6666000, .fec = 5, .polarisation = 'V'},
	{.freq = 12127000, .symrate = 7400000, .fec = 5, .polarisation = 'H'},
	{.freq = 12140000, .symrate = 2222000, .fec = 3, .polarisation = 'H'},
	{.freq = 12140000, .symrate = 4444000, .fec = 3, .polarisation = 'V'},
	{.freq = 12513000, .symrate = 4400000, .fec = 3, .polarisation = 'H'},
	{.freq = 12518000, .symrate = 3125000, .fec = 3, .polarisation = 'H'},
	{.freq = 12524000, .symrate = 4250000, .fec = 5, .polarisation = 'H'},
	{.freq = 12530000, .symrate = 4444000, .fec = 5, .polarisation = 'H'},
	{.freq = 12536000, .symrate = 2962000, .fec = 3, .polarisation = 'H'},
	{.freq = 12540000, .symrate = 3125000, .fec = 3, .polarisation = 'H'},
	{.freq = 12563000, .symrate = 24000000, .fec = 7, .polarisation = 'H'},
	{.freq = 12590000, .symrate = 3000000, .fec = 5, .polarisation = 'V'},
	{.freq = 12595000, .symrate = 2500000, .fec = 5, .polarisation = 'V'},
	{.freq = 12605000, .symrate = 2961000, .fec = 3, .polarisation = 'V'},
	{.freq = 12609000, .symrate = 3700000, .fec = 5, .polarisation = 'V'},
	{.freq = 12614000, .symrate = 3333000, .fec = 5, .polarisation = 'V'},
	{.freq = 12633000, .symrate = 4800000, .fec = 5, .polarisation = 'V'},
	{.freq = 12636000, .symrate = 4800000, .fec = 5, .polarisation = 'H'},
	{.freq = 12638000, .symrate = 2400000, .fec = 5, .polarisation = 'V'},
	{.freq = 12647000, .symrate = 3333000, .fec = 5, .polarisation = 'V'},
	{.freq = 12652000, .symrate = 22500000, .fec = 5, .polarisation = 'H'},
	{.freq = 12652000, .symrate = 3900000, .fec = 5, .polarisation = 'V'},
	{.freq = 12660000, .symrate = 9150000, .fec = 5, .polarisation = 'V'},
	{.freq = 12672000, .symrate = 2222000, .fec = 5, .polarisation = 'H'},
	{.freq = 12680000, .symrate = 8888000, .fec = 3, .polarisation = 'H'},
	{.freq = 12692000, .symrate = 2800000, .fec = 5, .polarisation = 'H'},
	{.freq = 12696000, .symrate = 2222000, .fec = 3, .polarisation = 'H'},
	{.freq = 12699000, .symrate = 2400000, .fec = 3, .polarisation = 'H'},
	{.freq = 12702000, .symrate = 2285000, .fec = 7, .polarisation = 'H'},
	{.freq = 12717000, .symrate = 5925000, .fec = 5, .polarisation = 'V'},
	{.freq = 12731000, .symrate = 3333000, .fec = 3, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Vinasat1_C_132_0E[] = {
	{.freq = 3413000, .symrate = 9766000, .fec = 3, .polarisation = 'V'},
	{.freq = 3529000, .symrate = 2500000, .fec = 2, .polarisation = 'V'},
};

static const struct mux muxes_DVBS_Yamal201_90_0E[] = {
	{.freq = 10990000, .symrate = 2170000, .fec = 9, .polarisation = 'V'},
	{.freq = 10995000, .symrate = 4285000, .fec = 9, .polarisation = 'V'},
	{.freq = 11057000, .symrate = 26470000, .fec = 9, .polarisation = 'V'},
	{.freq = 11092000, .symrate = 26470000, .fec = 9, .polarisation = 'V'},
	{.freq = 11145000, .symrate = 22222000, .fec = 9, .polarisation = 'V'},
	{.freq = 11671000, .symrate = 18200000, .fec = 9, .polarisation = 'V'},
	{.freq = 3536000, .symrate = 2532000, .fec = 3, .polarisation = 'H'},
	{.freq = 3553000, .symrate = 20000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3577000, .symrate = 2626000, .fec = 3, .polarisation = 'H'},
	{.freq = 3588000, .symrate = 4285000, .fec = 3, .polarisation = 'H'},
	{.freq = 3600000, .symrate = 4285000, .fec = 3, .polarisation = 'H'},
	{.freq = 3603000, .symrate = 4285000, .fec = 3, .polarisation = 'V'},
	{.freq = 3605000, .symrate = 2626000, .fec = 3, .polarisation = 'H'},
	{.freq = 3645000, .symrate = 28000000, .fec = 3, .polarisation = 'H'},
	{.freq = 3674000, .symrate = 17500000, .fec = 3, .polarisation = 'H'},
	{.freq = 3725000, .symrate = 3200000, .fec = 3, .polarisation = 'H'},
	{.freq = 3900000, .symrate = 4285000, .fec = 3, .polarisation = 'H'},
	{.freq = 3907000, .symrate = 4265000, .fec = 3, .polarisation = 'H'},
	{.freq = 3912000, .symrate = 4295000, .fec = 3, .polarisation = 'H'},
	{.freq = 3944000, .symrate = 15550000, .fec = 3, .polarisation = 'H'},
	{.freq = 3980000, .symrate = 38000000, .fec = 3, .polarisation = 'H'},
	{.freq = 4042000, .symrate = 8681000, .fec = 3, .polarisation = 'V'},
	{.freq = 4084000, .symrate = 2500000, .fec = 3, .polarisation = 'V'},
};

static const struct network networks_DVBS_geo[] = {
	{
		.name = "ABS1-75.0E",
		.muxes = muxes_DVBS_ABS1_75_0E,
		.nmuxes = sizeof(muxes_DVBS_ABS1_75_0E) / sizeof(struct mux),
	},
	{
		.name = "AMC1-103w",
		.muxes = muxes_DVBS_AMC1_103w,
		.nmuxes = sizeof(muxes_DVBS_AMC1_103w) / sizeof(struct mux),
	},
	{
		.name = "AMC2-85w",
		.muxes = muxes_DVBS_AMC2_85w,
		.nmuxes = sizeof(muxes_DVBS_AMC2_85w) / sizeof(struct mux),
	},
	{
		.name = "AMC3-87w",
		.muxes = muxes_DVBS_AMC3_87w,
		.nmuxes = sizeof(muxes_DVBS_AMC3_87w) / sizeof(struct mux),
	},
	{
		.name = "AMC4-101w",
		.muxes = muxes_DVBS_AMC4_101w,
		.nmuxes = sizeof(muxes_DVBS_AMC4_101w) / sizeof(struct mux),
	},
	{
		.name = "AMC5-79w",
		.muxes = muxes_DVBS_AMC5_79w,
		.nmuxes = sizeof(muxes_DVBS_AMC5_79w) / sizeof(struct mux),
	},
	{
		.name = "AMC6-72w",
		.muxes = muxes_DVBS_AMC6_72w,
		.nmuxes = sizeof(muxes_DVBS_AMC6_72w) / sizeof(struct mux),
	},
	{
		.name = "AMC9-83w",
		.muxes = muxes_DVBS_AMC9_83w,
		.nmuxes = sizeof(muxes_DVBS_AMC9_83w) / sizeof(struct mux),
	},
	{
		.name = "Agila2 C-146.0E",
		.muxes = muxes_DVBS_Agila2_C_146_0E,
		.nmuxes = sizeof(muxes_DVBS_Agila2_C_146_0E) / sizeof(struct mux),
	},
	{
		.name = "Amazonas-61.0W",
		.muxes = muxes_DVBS_Amazonas_61_0W,
		.nmuxes = sizeof(muxes_DVBS_Amazonas_61_0W) / sizeof(struct mux),
	},
	{
		.name = "Amos-4w",
		.muxes = muxes_DVBS_Amos_4w,
		.nmuxes = sizeof(muxes_DVBS_Amos_4w) / sizeof(struct mux),
	},
	{
		.name = "Anik-F1-107.3W",
		.muxes = muxes_DVBS_Anik_F1_107_3W,
		.nmuxes = sizeof(muxes_DVBS_Anik_F1_107_3W) / sizeof(struct mux),
	},
	{
		.name = "Apstar6 C-134.0E",
		.muxes = muxes_DVBS_Apstar6_C_134_0E,
		.nmuxes = sizeof(muxes_DVBS_Apstar6_C_134_0E) / sizeof(struct mux),
	},
	{
		.name = "AsiaSat2 C-100.5E",
		.muxes = muxes_DVBS_AsiaSat2_C_100_5E,
		.nmuxes = sizeof(muxes_DVBS_AsiaSat2_C_100_5E) / sizeof(struct mux),
	},
	{
		.name = "AsiaSat3S C-105.5E",
		.muxes = muxes_DVBS_AsiaSat3S_C_105_5E,
		.nmuxes = sizeof(muxes_DVBS_AsiaSat3S_C_105_5E) / sizeof(struct mux),
	},
	{
		.name = "Asiasat4 C-122.2E",
		.muxes = muxes_DVBS_Asiasat4_C_122_2E,
		.nmuxes = sizeof(muxes_DVBS_Asiasat4_C_122_2E) / sizeof(struct mux),
	},
	{
		.name = "Astra-19.2E",
		.muxes = muxes_DVBS_Astra_19_2E,
		.nmuxes = sizeof(muxes_DVBS_Astra_19_2E) / sizeof(struct mux),
	},
	{
		.name = "Astra-23.5E",
		.muxes = muxes_DVBS_Astra_23_5E,
		.nmuxes = sizeof(muxes_DVBS_Astra_23_5E) / sizeof(struct mux),
	},
	{
		.name = "Astra-28.2E",
		.muxes = muxes_DVBS_Astra_28_2E,
		.nmuxes = sizeof(muxes_DVBS_Astra_28_2E) / sizeof(struct mux),
	},
	{
		.name = "Atlantic-Bird-1-12.5W",
		.muxes = muxes_DVBS_Atlantic_Bird_1_12_5W,
		.nmuxes = sizeof(muxes_DVBS_Atlantic_Bird_1_12_5W) / sizeof(struct mux),
	},
	{
		.name = "Atlantic-Bird-3-5.0W",
		.muxes = muxes_DVBS_Atlantic_Bird_3_5_0W,
		.nmuxes = sizeof(muxes_DVBS_Atlantic_Bird_3_5_0W) / sizeof(struct mux),
	},
	{
		.name = "BrasilSat-B1-75.0W",
		.muxes = muxes_DVBS_BrasilSat_B1_75_0W,
		.nmuxes = sizeof(muxes_DVBS_BrasilSat_B1_75_0W) / sizeof(struct mux),
	},
	{
		.name = "BrasilSat-B2-65.0W",
		.muxes = muxes_DVBS_BrasilSat_B2_65_0W,
		.nmuxes = sizeof(muxes_DVBS_BrasilSat_B2_65_0W) / sizeof(struct mux),
	},
	{
		.name = "BrasilSat-B3-84.0W",
		.muxes = muxes_DVBS_BrasilSat_B3_84_0W,
		.nmuxes = sizeof(muxes_DVBS_BrasilSat_B3_84_0W) / sizeof(struct mux),
	},
	{
		.name = "BrasilSat-B4-70.0W",
		.muxes = muxes_DVBS_BrasilSat_B4_70_0W,
		.nmuxes = sizeof(muxes_DVBS_BrasilSat_B4_70_0W) / sizeof(struct mux),
	},
	{
		.name = "Chinastar1 C-87.5E",
		.muxes = muxes_DVBS_Chinastar1_C_87_5E,
		.nmuxes = sizeof(muxes_DVBS_Chinastar1_C_87_5E) / sizeof(struct mux),
	},
	{
		.name = "Estrela-do-Sul-63.0W",
		.muxes = muxes_DVBS_Estrela_do_Sul_63_0W,
		.nmuxes = sizeof(muxes_DVBS_Estrela_do_Sul_63_0W) / sizeof(struct mux),
	},
	{
		.name = "Eurobird1-28.5E",
		.muxes = muxes_DVBS_Eurobird1_28_5E,
		.nmuxes = sizeof(muxes_DVBS_Eurobird1_28_5E) / sizeof(struct mux),
	},
	{
		.name = "Eurobird9-9.0E",
		.muxes = muxes_DVBS_Eurobird9_9_0E,
		.nmuxes = sizeof(muxes_DVBS_Eurobird9_9_0E) / sizeof(struct mux),
	},
	{
		.name = "EutelsatW2-16E",
		.muxes = muxes_DVBS_EutelsatW2_16E,
		.nmuxes = sizeof(muxes_DVBS_EutelsatW2_16E) / sizeof(struct mux),
	},
	{
		.name = "ExpressAM1-40.0E",
		.muxes = muxes_DVBS_ExpressAM1_40_0E,
		.nmuxes = sizeof(muxes_DVBS_ExpressAM1_40_0E) / sizeof(struct mux),
	},
	{
		.name = "ExpressAM22-53.0E",
		.muxes = muxes_DVBS_ExpressAM22_53_0E,
		.nmuxes = sizeof(muxes_DVBS_ExpressAM22_53_0E) / sizeof(struct mux),
	},
	{
		.name = "ExpressAM2-80.0E",
		.muxes = muxes_DVBS_ExpressAM2_80_0E,
		.nmuxes = sizeof(muxes_DVBS_ExpressAM2_80_0E) / sizeof(struct mux),
	},
	{
		.name = "Express-3A-11.0W",
		.muxes = muxes_DVBS_Express_3A_11_0W,
		.nmuxes = sizeof(muxes_DVBS_Express_3A_11_0W) / sizeof(struct mux),
	},
	{
		.name = "Galaxy10R-123w",
		.muxes = muxes_DVBS_Galaxy10R_123w,
		.nmuxes = sizeof(muxes_DVBS_Galaxy10R_123w) / sizeof(struct mux),
	},
	{
		.name = "Galaxy11-91w",
		.muxes = muxes_DVBS_Galaxy11_91w,
		.nmuxes = sizeof(muxes_DVBS_Galaxy11_91w) / sizeof(struct mux),
	},
	{
		.name = "Galaxy25-97w",
		.muxes = muxes_DVBS_Galaxy25_97w,
		.nmuxes = sizeof(muxes_DVBS_Galaxy25_97w) / sizeof(struct mux),
	},
	{
		.name = "Galaxy26-93w",
		.muxes = muxes_DVBS_Galaxy26_93w,
		.nmuxes = sizeof(muxes_DVBS_Galaxy26_93w) / sizeof(struct mux),
	},
	{
		.name = "Galaxy27-129w",
		.muxes = muxes_DVBS_Galaxy27_129w,
		.nmuxes = sizeof(muxes_DVBS_Galaxy27_129w) / sizeof(struct mux),
	},
	{
		.name = "Galaxy28-89w",
		.muxes = muxes_DVBS_Galaxy28_89w,
		.nmuxes = sizeof(muxes_DVBS_Galaxy28_89w) / sizeof(struct mux),
	},
	{
		.name = "Galaxy3C-95w",
		.muxes = muxes_DVBS_Galaxy3C_95w,
		.nmuxes = sizeof(muxes_DVBS_Galaxy3C_95w) / sizeof(struct mux),
	},
	{
		.name = "Hispasat-30.0W",
		.muxes = muxes_DVBS_Hispasat_30_0W,
		.nmuxes = sizeof(muxes_DVBS_Hispasat_30_0W) / sizeof(struct mux),
	},
	{
		.name = "Hotbird-13.0E",
		.muxes = muxes_DVBS_Hotbird_13_0E,
		.nmuxes = sizeof(muxes_DVBS_Hotbird_13_0E) / sizeof(struct mux),
	},
	{
		.name = "IA5-97w",
		.muxes = muxes_DVBS_IA5_97w,
		.nmuxes = sizeof(muxes_DVBS_IA5_97w) / sizeof(struct mux),
	},
	{
		.name = "IA6-93w",
		.muxes = muxes_DVBS_IA6_93w,
		.nmuxes = sizeof(muxes_DVBS_IA6_93w) / sizeof(struct mux),
	},
	{
		.name = "IA7-129w",
		.muxes = muxes_DVBS_IA7_129w,
		.nmuxes = sizeof(muxes_DVBS_IA7_129w) / sizeof(struct mux),
	},
	{
		.name = "IA8-89w",
		.muxes = muxes_DVBS_IA8_89w,
		.nmuxes = sizeof(muxes_DVBS_IA8_89w) / sizeof(struct mux),
	},
	{
		.name = "Insat2E C-83.0E",
		.muxes = muxes_DVBS_Insat2E_C_83_0E,
		.nmuxes = sizeof(muxes_DVBS_Insat2E_C_83_0E) / sizeof(struct mux),
	},
	{
		.name = "Insat3A C-93.5E",
		.muxes = muxes_DVBS_Insat3A_C_93_5E,
		.nmuxes = sizeof(muxes_DVBS_Insat3A_C_93_5E) / sizeof(struct mux),
	},
	{
		.name = "Insat4A C-83.0E",
		.muxes = muxes_DVBS_Insat4A_C_83_0E,
		.nmuxes = sizeof(muxes_DVBS_Insat4A_C_83_0E) / sizeof(struct mux),
	},
	{
		.name = "Insat4B C-93.5E",
		.muxes = muxes_DVBS_Insat4B_C_93_5E,
		.nmuxes = sizeof(muxes_DVBS_Insat4B_C_93_5E) / sizeof(struct mux),
	},
	{
		.name = "Intel4-72.0E",
		.muxes = muxes_DVBS_Intel4_72_0E,
		.nmuxes = sizeof(muxes_DVBS_Intel4_72_0E) / sizeof(struct mux),
	},
	{
		.name = "Intel904-60.0E",
		.muxes = muxes_DVBS_Intel904_60_0E,
		.nmuxes = sizeof(muxes_DVBS_Intel904_60_0E) / sizeof(struct mux),
	},
	{
		.name = "Intelsat8 C-166.0E",
		.muxes = muxes_DVBS_Intelsat8_C_166_0E,
		.nmuxes = sizeof(muxes_DVBS_Intelsat8_C_166_0E) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-1002-1.0W",
		.muxes = muxes_DVBS_Intelsat_1002_1_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_1002_1_0W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-11-43.0W",
		.muxes = muxes_DVBS_Intelsat_11_43_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_11_43_0W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-1R-45.0W",
		.muxes = muxes_DVBS_Intelsat_1R_45_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_1R_45_0W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-3R-43.0W",
		.muxes = muxes_DVBS_Intelsat_3R_43_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_3R_43_0W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-6B-43.0W",
		.muxes = muxes_DVBS_Intelsat_6B_43_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_6B_43_0W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-705-50.0W",
		.muxes = muxes_DVBS_Intelsat_705_50_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_705_50_0W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-707-53.0W",
		.muxes = muxes_DVBS_Intelsat_707_53_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_707_53_0W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-805-55.5W",
		.muxes = muxes_DVBS_Intelsat_805_55_5W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_805_55_5W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-903-34.5W",
		.muxes = muxes_DVBS_Intelsat_903_34_5W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_903_34_5W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-905-24.5W",
		.muxes = muxes_DVBS_Intelsat_905_24_5W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_905_24_5W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-907-27.5W",
		.muxes = muxes_DVBS_Intelsat_907_27_5W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_907_27_5W) / sizeof(struct mux),
	},
	{
		.name = "Intelsat-9-58.0W",
		.muxes = muxes_DVBS_Intelsat_9_58_0W,
		.nmuxes = sizeof(muxes_DVBS_Intelsat_9_58_0W) / sizeof(struct mux),
	},
	{
		.name = "JCSat3A C-128.0E",
		.muxes = muxes_DVBS_JCSat3A_C_128_0E,
		.nmuxes = sizeof(muxes_DVBS_JCSat3A_C_128_0E) / sizeof(struct mux),
	},
	{
		.name = "Measat3 C-91.5E",
		.muxes = muxes_DVBS_Measat3_C_91_5E,
		.nmuxes = sizeof(muxes_DVBS_Measat3_C_91_5E) / sizeof(struct mux),
	},
	{
		.name = "NSS-10-37.5W",
		.muxes = muxes_DVBS_NSS_10_37_5W,
		.nmuxes = sizeof(muxes_DVBS_NSS_10_37_5W) / sizeof(struct mux),
	},
	{
		.name = "NSS-7-22.0W",
		.muxes = muxes_DVBS_NSS_7_22_0W,
		.nmuxes = sizeof(muxes_DVBS_NSS_7_22_0W) / sizeof(struct mux),
	},
	{
		.name = "NSS-806-40.5W",
		.muxes = muxes_DVBS_NSS_806_40_5W,
		.nmuxes = sizeof(muxes_DVBS_NSS_806_40_5W) / sizeof(struct mux),
	},
	{
		.name = "Nahuel-1-71.8W",
		.muxes = muxes_DVBS_Nahuel_1_71_8W,
		.nmuxes = sizeof(muxes_DVBS_Nahuel_1_71_8W) / sizeof(struct mux),
	},
	{
		.name = "Nilesat101+102-7.0W",
		.muxes = muxes_DVBS_Nilesat101_102_7_0W,
		.nmuxes = sizeof(muxes_DVBS_Nilesat101_102_7_0W) / sizeof(struct mux),
	},
	{
		.name = "OptusC1-156E",
		.muxes = muxes_DVBS_OptusC1_156E,
		.nmuxes = sizeof(muxes_DVBS_OptusC1_156E) / sizeof(struct mux),
	},
	{
		.name = "OptusD1-160.0E",
		.muxes = muxes_DVBS_OptusD1_160_0E,
		.nmuxes = sizeof(muxes_DVBS_OptusD1_160_0E) / sizeof(struct mux),
	},
	{
		.name = "PAS-43.0W",
		.muxes = muxes_DVBS_PAS_43_0W,
		.nmuxes = sizeof(muxes_DVBS_PAS_43_0W) / sizeof(struct mux),
	},
	{
		.name = "PalapaC2 C-113.0E",
		.muxes = muxes_DVBS_PalapaC2_C_113_0E,
		.nmuxes = sizeof(muxes_DVBS_PalapaC2_C_113_0E) / sizeof(struct mux),
	},
	{
		.name = "SBS6-74w",
		.muxes = muxes_DVBS_SBS6_74w,
		.nmuxes = sizeof(muxes_DVBS_SBS6_74w) / sizeof(struct mux),
	},
	{
		.name = "ST1 C-80.0E",
		.muxes = muxes_DVBS_ST1_C_80_0E,
		.nmuxes = sizeof(muxes_DVBS_ST1_C_80_0E) / sizeof(struct mux),
	},
	{
		.name = "Satmex-5-116.8W",
		.muxes = muxes_DVBS_Satmex_5_116_8W,
		.nmuxes = sizeof(muxes_DVBS_Satmex_5_116_8W) / sizeof(struct mux),
	},
	{
		.name = "Satmex-6-113.0W",
		.muxes = muxes_DVBS_Satmex_6_113_0W,
		.nmuxes = sizeof(muxes_DVBS_Satmex_6_113_0W) / sizeof(struct mux),
	},
	{
		.name = "Sirius-5.0E",
		.muxes = muxes_DVBS_Sirius_5_0E,
		.nmuxes = sizeof(muxes_DVBS_Sirius_5_0E) / sizeof(struct mux),
	},
	{
		.name = "Telecom2-8.0W",
		.muxes = muxes_DVBS_Telecom2_8_0W,
		.nmuxes = sizeof(muxes_DVBS_Telecom2_8_0W) / sizeof(struct mux),
	},
	{
		.name = "Telkom1 C-108.0E",
		.muxes = muxes_DVBS_Telkom1_C_108_0E,
		.nmuxes = sizeof(muxes_DVBS_Telkom1_C_108_0E) / sizeof(struct mux),
	},
	{
		.name = "Telstar10 C-76.5E",
		.muxes = muxes_DVBS_Telstar10_C_76_5E,
		.nmuxes = sizeof(muxes_DVBS_Telstar10_C_76_5E) / sizeof(struct mux),
	},
	{
		.name = "Telstar12-15.0W",
		.muxes = muxes_DVBS_Telstar12_15_0W,
		.nmuxes = sizeof(muxes_DVBS_Telstar12_15_0W) / sizeof(struct mux),
	},
	{
		.name = "Telstar18 C-138.0E",
		.muxes = muxes_DVBS_Telstar18_C_138_0E,
		.nmuxes = sizeof(muxes_DVBS_Telstar18_C_138_0E) / sizeof(struct mux),
	},
	{
		.name = "Thaicom1A C-120.0E",
		.muxes = muxes_DVBS_Thaicom1A_C_120_0E,
		.nmuxes = sizeof(muxes_DVBS_Thaicom1A_C_120_0E) / sizeof(struct mux),
	},
	{
		.name = "Thaicom2 C-78.5E",
		.muxes = muxes_DVBS_Thaicom2_C_78_5E,
		.nmuxes = sizeof(muxes_DVBS_Thaicom2_C_78_5E) / sizeof(struct mux),
	},
	{
		.name = "Thaicom5 C-78.5E",
		.muxes = muxes_DVBS_Thaicom5_C_78_5E,
		.nmuxes = sizeof(muxes_DVBS_Thaicom5_C_78_5E) / sizeof(struct mux),
	},
	{
		.name = "Thor-1.0W",
		.muxes = muxes_DVBS_Thor_1_0W,
		.nmuxes = sizeof(muxes_DVBS_Thor_1_0W) / sizeof(struct mux),
	},
	{
		.name = "Turksat-42.0E",
		.muxes = muxes_DVBS_Turksat_42_0E,
		.nmuxes = sizeof(muxes_DVBS_Turksat_42_0E) / sizeof(struct mux),
	},
	{
		.name = "Vinasat1 C-132.0E",
		.muxes = muxes_DVBS_Vinasat1_C_132_0E,
		.nmuxes = sizeof(muxes_DVBS_Vinasat1_C_132_0E) / sizeof(struct mux),
	},
	{
		.name = "Yamal201-90.0E",
		.muxes = muxes_DVBS_Yamal201_90_0E,
		.nmuxes = sizeof(muxes_DVBS_Yamal201_90_0E) / sizeof(struct mux),
	},
};

static const struct region regions_DVBS[] = {
	{
		.name = "Geosynchronous Orbit",
		.networks = networks_DVBS_geo,
		.nnetworks = sizeof(networks_DVBS_geo) / sizeof(struct network),
	},
};

static const struct mux muxes_DVBT_ad_Andorra[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_at_Official[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Adelaide[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 564500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_AdelaideFoothills[] = {
	{.freq = 781625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 711500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 795500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 732500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 760500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Ballarat[] = {
	{.freq = 620625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 655500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 592625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 613500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 634625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Bendigo[] = {
	{.freq = 669500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 620500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 572500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 690500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 655500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 555250000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 576250000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 592500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 618250000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 529500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 534250000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 676500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 571500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 585625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 564500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 543500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 529500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 205500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 564500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 690500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 711500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 550500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Brisbane[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 585625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 599500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Cairns[] = {
	{.freq = 191500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_canberra[] = {
	{.freq = 205625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 543500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Canberra_Black_Mt[] = {
	{.freq = 205500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 543500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Coonabarabran[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 655500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 648500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 641500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Darwin[] = {
	{.freq = 543625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 550500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 557625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Devonport[] = {
	{.freq = 730250000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 648500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 709250000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 606500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 613498000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 627498000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 585498000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_FraserCoast_Bundaberg[] = {
	{.freq = 205625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 529500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 184625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 198525000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_GoldCoast[] = {
	{.freq = 767500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 585500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 704500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 809500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 788500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 634500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 746500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 725500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Goulburn_Rocky_Hill[] = {
	{.freq = 725625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 746625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 767625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 788625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 809500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Hervey_Bay_Ghost_Hill[] = {
	{.freq = 725500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 746500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 767625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 809500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 529500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 704500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Hobart[] = {
	{.freq = 191500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 205500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 184625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Mackay[] = {
	{.freq = 212500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 205500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 557500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Melbourne[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Melbourne_Upwey[] = {
	{.freq = 662500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 620500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 641500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 711500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 683500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_MidNorthCoast[] = {
	{.freq = 184625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 198500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 641500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 205500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 585500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 543500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 564500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 599500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 606500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Newcastle[] = {
	{.freq = 599500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 585500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 704500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 592500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 690500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Perth[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Perth_Roleystone[] = {
	{.freq = 704500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 725500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 746500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 767500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 788500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_SpencerGulf[] = {
	{.freq = 599500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 641500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 620500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_SunshineCoast[] = {
	{.freq = 585625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 662625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 767625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 788625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 809500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Sutherland[] = {
	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 571500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Sydney_Kings_Cross[] = {
	{.freq = 543500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 669500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 564500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 648500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 571500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Sydney_North_Shore[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 571500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Tamworth[] = {
	{.freq = 690500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 753500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 732500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 711500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 774500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 585500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 592500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 205625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 613500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 641500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 648500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 620500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 226625000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 641500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_auto_Australia[] = {
	{.freq = 177500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 177625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 184500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 184625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 191500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 191625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 198500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 198625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 205500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 205625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 212625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 219625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 226625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 529500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 529625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 536500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 536625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 543500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 543625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 550500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 550625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 557500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 557625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 564500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 564625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 571500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 571625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 578500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 578625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 585500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 585625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 592500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 592625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 599500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 599625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 606500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 606625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 613500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 613625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 620500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 620625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 627500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 627625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 634500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 634625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 641500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 641625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 648500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 648625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 655500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 655625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 662500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 662625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 669500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 669625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 676500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 676625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 683500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 683625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 690500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 690625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 697500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 697625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 704500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 704625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 711500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 711625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 718500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 718625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 725500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 725625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 732500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 732625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 739500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 739625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 746500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 746625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 753500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 753625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 760500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 760625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 767500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 767625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 774500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 774625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 781500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 781625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 788500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 788625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 795500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 795625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 802500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 802625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 809500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 809625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 816500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 816625000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_auto_Default[] = {
	{.freq = 177500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 184500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 191500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 198500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 205500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_auto_Italy[] = {
	{.freq = 177500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 177500000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 186000000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 186000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 194500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 194500000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 203500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 203500000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 212500000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 219500000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 226500000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_auto_Taiwan[] = {
	{.freq = 527000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 533000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 539000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 545000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 551000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 557000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 563000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 569000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 575000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 581000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 587000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 593000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 599000000, .bw = 2, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_auto_With167kHzOffsets[] = {
	{.freq = 177500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 184500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 191500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 198500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 205500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 473833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 474167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 481833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 489833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 490167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 497833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 498167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 505833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 513833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 514167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 521833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 522167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 529833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 530167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 537833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 538167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 546167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 553833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 554167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 561833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 562167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 569833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 570167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 577833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 578167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 585833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 586167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 593833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 594167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 601833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 602167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 609833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 610167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 617833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 618167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 625833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 626167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 633833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 634167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 641833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 642167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 649833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 650167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 657833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 665833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 666167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 673833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 674167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 681833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 682167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 689833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 690167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 697833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 698167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 705833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 706167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 713833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 721833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 722167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 729833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 730167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 737833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 738167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 745833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 746167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 753833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 754167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 761833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 762167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 769833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 770167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 777833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 778167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 785833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 786167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 793833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 794167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 801833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 802167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 809833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 810167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 817833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 818167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 825833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 826167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 833833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 834167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 841833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 842167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 849833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 850167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 857833000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 	{.freq = 858167000, .bw = 0, .constellation = 6, .fechp = 9, .feclp = 0, .tmode = 2, .guard = 4, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Townsville[] = {
	{.freq = 592500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 550500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 599500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 620500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 585500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_unknown[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_WaggaWagga[] = {
	{.freq = 655500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 669500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 662500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 690500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 683500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_au_Wollongong[] = {
	{.freq = 697500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 655500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 613500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 634500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 711625000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 599500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 585500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 592500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 676500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_ax_Smedsbole[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_be_All[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_ch_All[] = {
	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_ch_Citycable[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 7, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_ch_Geneva[] = {
	{.freq = 570166000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658166000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666166000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682166000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746166000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 0, .fechp = 1, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 770166000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 794166000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 818166000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 850166000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_cz_All[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Baden_Wuerttemberg[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Bayern[] = {
	{.freq = 191500000, .bw = 1, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Berlin[] = {
	{.freq = 177500000, .bw = 1, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 191500000, .bw = 1, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Brandenburg[] = {
	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Bremen[] = {
	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Hamburg[] = {
	{.freq = 490000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Hessen[] = {
	{.freq = 198500000, .bw = 1, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Mecklenburg_Vorpommern[] = {
	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Niedersachsen[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Nordrhein_Westfalen[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Rheinland_Pfalz[] = {
	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Saarland[] = {
	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Sachsen[] = {
	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Sachsen_Anhalt[] = {
	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Schleswig_Holstein[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_de_Thueringen[] = {
	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_dk_All[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Albacete[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Alfabia[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Alicante[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Alpicat[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Asturias[] = {
	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_BaixoMinho[] = {
	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Bilbao[] = {
	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Cadiz[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Carceres[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Collserola[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Donostia[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Granada[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Huesca[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Las_Palmas[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Lugo[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Madrid[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Malaga[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Muros_Noia[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Mussara[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Pamplona[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Rocacorba[] = {
	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Santander[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Santiago_de_Compostela[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_SC_Tenerife[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Sevilla[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Tenerife[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Valencia[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Valladolid[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Vilamarxant[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Vitoria_Gasteiz[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_es_Zaragoza[] = {
	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Aanekoski[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Aanekoski_Konginkangas[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ahtari[] = {
	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Alajarvi[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ala_Vuokki[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ammansaari[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Anjalankoski[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Enontekio_Raattama[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Espoo[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Eurajoki[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Fiskars[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Haapavesi[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hameenkyro_Kyroskoski[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hameenlinna_Painokangas[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hanko[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hartola[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Haukela[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Heinavesi[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Heinola[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hetta[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hossa[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Houtskari[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hyrynsalmi[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hyrynsalmi_Kyparavaara[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hyrynsalmi_Paljakka[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Hyvinkaa_Musta_Mannisto[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ii_Raiskio[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Iisalmi[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ikaalinen[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ikaalinen_Riitiala[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Inari[] = {
	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Inari_Janispaa[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Inari_Naatamo[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ivalo_Saarineitamovaara[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Jalasjarvi[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Jamsa_Kaipola[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Jamsa_Kuorevesi_Halli[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Jamsa_Matkosvuori[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Jamsankoski[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Jamsa_Ouninpohja[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Joensuu_Vestinkallio[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Joroinen_Puukkola_Huutokoski[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Joutsa_Lankia[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Joutseno[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Juntusranta[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Juupajoki_Kopsamo[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Jyvaskyla[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kaavi_Luikonlahti[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kajaani_Pollyvaara[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kalajoki[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kangaslampi[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kangasniemi_Turkinmaki[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kankaanpaa[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Karigasniemi[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Karkkila[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Karstula[] = {
	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Karvia[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kaunispaa[] = {
	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kemijarvi_Suomutunturi[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kerimaki[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Keuruu[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Keuruu_Haapamaki[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kihnio[] = {
	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kiihtelysvaara[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kilpisjarvi[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kittila_Sirkka_Levitunturi[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kolari_Vuolittaja[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Koli[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Korpilahti_Vaarunvuori[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Korppoo[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kruunupyy[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kuhmo_Iivantiira[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kuhmoinen[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kuhmoinen_Harjunsalmi[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kuhmoinen_Puukkoinen[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kuhmo_Lentiira[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kuopio[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kustavi_Viherlahti[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kuttanen[] = {
	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Kyyjarvi_Noposenaho[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Lahti[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Lapua[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Laukaa[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Laukaa_Vihtavuori[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Lavia_Lavianjarvi[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Lieksa_Konnanvaara[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Lohja[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Loimaa[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Luhanka[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Luopioinen[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Mantta[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Mantyharju[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Mikkeli[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Muonio_Olostunturi[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Mustasaari[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Myllylahti[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Nilsia[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Nilsia_Keski_Siikajarvi[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Nilsia_Pisa[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Nokia[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Nokia_Siuro_Linnavuori[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Nummi_Pusula_Hyonola[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Nurmes_Kortevaara[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Orivesi_Langelmaki_Talviainen[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Oulu[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Padasjoki[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Padasjoki_Arrakoski[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Paltamo_Kivesvaara[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Parikkala[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Parkano_Sopukallio[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pello[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pello_Ratasvaara[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Perho[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pernaja[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pieksamaki_Halkokumpu[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pihtipudas[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Porvoo_Suomenkyla[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Posio[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pudasjarvi[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pudasjarvi_Hirvaskoski[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pudasjarvi_Iso_Syote[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Puolanka[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pyhatunturi[] = {
	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pyhavuori[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Pylkonmaki_Karankajarvi[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Raahe_Mestauskallio[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Raahe_Piehinki[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ranua_Haasionmaa[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ranua_Leppiaho[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rautavaara_Angervikko[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rautjarvi_Simpele[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ristijarvi[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rovaniemi[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rovaniemi_Ala_Nampa_Yli_Nampa_Rantalaki[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rovaniemi_Kaihuanvaara[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rovaniemi_Marasenkallio[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rovaniemi_Marrasjarvi[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rovaniemi_Meltaus_Sorviselka[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Rovaniemi_Sonka[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ruka[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ruovesi_Storminiemi[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Saarijarvi[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Saarijarvi_Kalmari[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Saarijarvi_Mahlu[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Salla_Hirvasvaara[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Salla_Ihistysjanka[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Salla_Naruska[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Salla_Sallatunturi[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Salla_Sarivaara[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Salo_Isokyla[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Savukoski_Martti[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Savukoski_Tanhua[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Siilinjarvi[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Sipoo_Norrkulla[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Sodankyla_Pittiovaara[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Sulkava_Vaatalanmaki[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Sysma_Liikola[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Taivalkoski[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Taivalkoski_Taivalvaara[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Tammela[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Tammisaari[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Tampere[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Tampere_Pyynikki[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Tervola[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Turku[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki_Nuorgam_Njallavaara[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki_Nuorgam_raja[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki_Nuvvus[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki_Outakoski[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki_Polvarniemi[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki_Rovisuvanto[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Utsjoki_Tenola[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Uusikaupunki_Orivo[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vaajakoski[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vaala[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vaasa[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Valtimo[] = {
	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vammala_Jyranvuori[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vammala_Roismala[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vammala_Savi[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vantaa_Hakunila[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Varpaisjarvi_Honkamaki[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Virrat_Lappavuori[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vuokatti[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Vuotso[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ylitornio_Ainiovaara[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Ylitornio_Raanujarvi[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_fi_Yllas[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_gr_Athens[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hk_HongKong[] = {
	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 628000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hr_All[] = {
	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Bekescsaba[] = {
	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Budapest[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Csavoly_Kiskoros[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Gerecse[] = {
	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Gyor_Tatabanya[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Kabhegy_Mor_Siofok[] = {
	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Karcag[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Kekes_Ozd[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Kiskoros[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Komadi_Debrecen[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Nagykanizsa_Kaposvar[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Pecs_Uzd[] = {
	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Salgotarjan[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Sopron[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Szentes_Szeged[] = {
	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Tokaj_Kazincbarcika[] = {
	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_hu_Vasvar[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_il_All[] = {
	{.freq = 514000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_ir_Tehran[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_is_Reykjavik[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 0, .fechp = 1, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Aosta[] = {
	{.freq = 226500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Bari[] = {
	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Bologna[] = {
	{.freq = 186000000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 203500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 212500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 219500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Bolzano[] = {
	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Cagliari[] = {
	{.freq = 205500000, .bw = 1, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Caivano[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Catania[] = {
	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 226500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Conero[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Firenze[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 219500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Genova[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 219500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Livorno[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Milano[] = {
	{.freq = 800000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 184500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 191500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 219500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 495250000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 735250000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 783250000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 807250000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 847250000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Modena[] = {
	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 184500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Montevergine[] = {
	{.freq = 177500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Pagnacco[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 226500000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Palermo[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Pescara[] = {
	{.freq = 186000000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 194500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Pisa[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Roma[] = {
	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 186000000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Sassari[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 177500000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_S_Stefano_al_mare[] = {
	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Torino[] = {
	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Trieste[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Varese[] = {
	{.freq = 226500000, .bw = 1, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Venezia[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_it_Verona[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_lt_All[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 9, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_lu_All[] = {
	{.freq = 191500000, .bw = 1, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_lv_Riga[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 0, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nl_All[] = {
	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_no_Trondelag_Stjordal[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_AucklandInfill[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_AucklandWaiatarua[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_Christchurch[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_Dunedin[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_Hamilton[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_HawkesBayMtErin[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_HawkesBayNapier[] = {
	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_Manawatu[] = {
	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_Tauranga[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_Waikato[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_WellingtonInfill[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_WellingtonKaukau[] = {
	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_nz_WellingtonNgarara[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_pl_Rzeszow[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_pl_Warszawa[] = {
	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 5, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_pl_Wroclaw[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_ro_Bucharest[] = {
	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 1, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Alvdalen_Brunnsberg[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Alvdalsasen[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Alvsbyn[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Amot[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Angebo[] = {
	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Angelholm_Vegeholm[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ange_Snoberg[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Arvidsjaur_Jultrask[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Aspeboda[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Atvidaberg[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Avesta_Krylbo[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Backefors[] = {
	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bankeryd[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bergsjo_Balleberget[] = {
	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bergvik[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bollebygd[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bollnas[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Boras_Dalsjofors[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Boras_Sjobo[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Borlange_Idkerberget[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Borlange_Nygardarna[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bottnaryd_Ryd[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bromsebro[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Bruzaholm[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Byxelkrok[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Dadran[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Dalfors[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Dalstuga[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Degerfors[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Delary[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Djura[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Drevdagen[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Duvnas[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Duvnas_Basna[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Edsbyn[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Emmaboda_Balshult[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Enviken[] = {
	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Fagersta[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Falerum_Centrum[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Falun_Lovberget[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Farila[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Faro_Ajkerstrask[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Farosund_Bunge[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Filipstad_Klockarhojden[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Finnveden[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Fredriksberg[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Fritsla[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Furudal[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gallivare[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Garpenberg_Kuppgarden[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gavle_Skogmur[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gnarp[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gnesta[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gnosjo_Marieholm[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Goteborg_Brudaremossen[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Goteborg_Slattadamm[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gullbrandstorp[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gunnarsbo[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Gusum[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hagfors_Varmullsasen[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hallaryd[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hallbo[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Halmstad_Hamnen[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Halmstad_Oskarstrom[] = {
	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Harnosand_Harnon[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hassela[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Havdhem[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hedemora[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Helsingborg_Olympia[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hennan[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hestra_Aspas[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hjo_Grevback[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hofors[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hogfors[] = {
	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hogsby_Virstad[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Holsbybrunn_Holsbyholm[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Horby_Sallerup[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Horken[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hudiksvall_Forsa[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Hudiksvall_Galgberget[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Huskvarna[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Idre[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ingatorp[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ingvallsbenning[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Irevik[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Jamjo[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Jarnforsen[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Jarvso[] = {
	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Jokkmokk_Tjalmejaure[] = {
	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Jonkoping_Bondberget[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kalix[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Karbole[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Karlsborg_Vaberget[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Karlshamn[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Karlskrona_Vamo[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Karlstad_Sormon[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kaxholmen_Vistakulle[] = {
	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kinnastrom[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kiruna_Kirunavaara[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kisa[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Knared[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kopmanholmen[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kopparberg[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kramfors_Lugnvik[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kristinehamn_Utsiktsberget[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kungsater[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Kungsberget_GI[] = {
	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Langshyttan[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Langshyttan_Engelsfors[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Leksand_Karingberget[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Lerdala[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Lilltjara_Digerberget[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Limedsforsen[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Lindshammar_Ramkvilla[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Linkoping_Vattentornet[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ljugarn[] = {
	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Loffstrand[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Lonneberga[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Lorstrand[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ludvika_Bjorkasen[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Lumsheden_Trekanten[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Lycksele_Knaften[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Mahult[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Malmo_Jagersro[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Malung[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Mariannelund[] = {
	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Markaryd_Hualtet[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Matfors[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Molndal_Vasterberget[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Mora_Eldris[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Motala_Ervasteby[] = {
	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Mullsjo_Torestorp[] = {
	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Nassjo[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Navekvarn[] = {
	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Norrahammar[] = {
	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Norrkoping_Krokek[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Norrtalje_Sodra_Bergen[] = {
	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Nykoping[] = {
	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Orebro_Lockhyttan[] = {
	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ornskoldsvik_As[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Oskarshamn[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ostersund_Brattasen[] = {
	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Osthammar_Valo[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Overkalix[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Oxberg[] = {
	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Pajala[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Paulistom[] = {
	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Rattvik[] = {
	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Rengsjo[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Rorbacksnas[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sagmyra[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Salen[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Salfjallet[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sarna_Mickeltemplet[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Satila[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Saxdalen[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Siljansnas_Uvberget[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Skarstad[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Skattungbyn[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Skelleftea[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Skene_Nycklarberget[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Skovde[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Smedjebacken_Uvberget[] = {
	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Soderhamn[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Soderkoping[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sodertalje_Ragnhildsborg[] = {
	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Solleftea_Hallsta[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Solleftea_Multra[] = {
	{.freq = 658000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sorsjon[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Stockholm_Marieberg[] = {
	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Stockholm_Nacka[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Stora_Skedvi[] = {
	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Storfjaten[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Storuman[] = {
	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Stromstad[] = {
	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Styrsjobo[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sundborn[] = {
	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sundsbruk[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sundsvall_S_Stadsberget[] = {
	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sunne_Blabarskullen[] = {
	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Svartnas[] = {
	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Sveg_Brickan[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Taberg[] = {
	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Tandadalen[] = {
	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Tasjo[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Tollsjo[] = {
	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Torsby_Bada[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Tranas_Bredkarr[] = {
	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Tranemo[] = {
	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Transtrand_Bolheden[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Traryd_Betas[] = {
	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Trollhattan[] = {
	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Trosa[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Tystberga[] = {
	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Uddevalla_Herrestad[] = {
	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ullared[] = {
	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ulricehamn[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ulvshyttan_Porjus[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Uppsala_Rickomberga[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Uppsala_Vedyxa[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vaddo_Elmsta[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Valdemarsvik[] = {
	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vannas_Granlundsberget[] = {
	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 594000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vansbro_Hummelberget[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Varberg_Grimeton[] = {
	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vasteras_Lillharad[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 610000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vastervik_Farhult[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vaxbo[] = {
	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vessigebro[] = {
	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vetlanda_Nye[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vikmanshyttan[] = {
	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Virserum[] = {
	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Visby_Follingbo[] = {
	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Visby_Hamnen[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 586000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Visingso[] = {
	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Vislanda_Nydala[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Voxna[] = {
	{.freq = 666000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Ystad_Metallgatan[] = {
	{.freq = 482000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_se_Yttermalung[] = {
	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_si_Ljubljana[] = {
	{.freq = 602000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_BanskaBystrica[] = {
	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Bardejov[] = {
	{.freq = 802000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Bratislava[] = {
	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Kosice[] = {
	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Michalovce[] = {
	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Namestovo[] = {
	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Poprad[] = {
	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Presov[] = {
	{.freq = 818000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_RimavskaSobota[] = {
	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Trencin[] = {
	{.freq = 858000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_VelkyKrtis[] = {
	{.freq = 826000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_sk_Zilina[] = {
	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_tw_Kaohsiung[] = {
	{.freq = 545000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 545000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 557000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 557000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_tw_Taipei[] = {
	{.freq = 533000000, .bw = 2, .constellation = 1, .fechp = 1, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 545000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 2, .hierarchy = 0},
 	{.freq = 557000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 581000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 	{.freq = 593000000, .bw = 2, .constellation = 1, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 3, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Aberdare[] = {
	{.freq = 474167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 497833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 521833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 530167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Angus[] = {
	{.freq = 850000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 777833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 801833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 753833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 825833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_BeaconHill[] = {
	{.freq = 721833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 770167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 738167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 753833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 818167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Belmont[] = {
	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Bilsdale[] = {
	{.freq = 578167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 498167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 522167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_BlackHill[] = {
	{.freq = 634167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Blaenplwyf[] = {
	{.freq = 530167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 562167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 538167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 570167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_BluebellHill[] = {
	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 665833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 641833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Bressay[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 497833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 521833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 553833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_BrierleyHill[] = {
	{.freq = 850000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 825833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 753833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 777833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 801833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_BristolIlchesterCres[] = {
	{.freq = 697833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 634167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_BristolKingsWeston[] = {
	{.freq = 482000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 562000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Bromsgrove[] = {
	{.freq = 578167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 537833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 569833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 489833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 513833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_BrougherMountain[] = {
	{.freq = 546167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 490167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 514167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 538167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 570167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Caldbeck[] = {
	{.freq = 506000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 514167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_CaradonHill[] = {
	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 553833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 497833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Carmel[] = {
	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 825833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 777833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 801833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Chatton[] = {
	{.freq = 626167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 706167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 650167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 674167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Chesterfield[] = {
	{.freq = 578167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 650000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Craigkelly[] = {
	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 489833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 513833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_CrystalPalace[] = {
	{.freq = 505833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 481833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 561833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 529833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 537833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Darvel[] = {
	{.freq = 481833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 505833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 561833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 529833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 546167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Divis[] = {
	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 569833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 489833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 513833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Dover[] = {
	{.freq = 850000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 745833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 770167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 785833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Durris[] = {
	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 722167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 713833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Eitshal[] = {
	{.freq = 578167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 546167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 481833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 505833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 529833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 561833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_EmleyMoor[] = {
	{.freq = 722167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 625833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 649833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 673833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 705833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 697833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Fenham[] = {
	{.freq = 545833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 530167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 562167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Fenton[] = {
	{.freq = 577833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 530167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 562167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Ferryside[] = {
	{.freq = 474167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Guildford[] = {
	{.freq = 697833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 634167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Hannington[] = {
	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 650167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 626167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 674167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 634167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Hastings[] = {
	{.freq = 553833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 521833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 497833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Heathfield[] = {
	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 689833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 681833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 713833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_HemelHempstead[] = {
	{.freq = 690167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 777833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_HuntshawCross[] = {
	{.freq = 737833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 769833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 793833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 817833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 729833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 761833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Idle[] = {
	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_KeelylangHill[] = {
	{.freq = 690167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 722167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 634167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Keighley[] = {
	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 729833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_KilveyHill[] = {
	{.freq = 505833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 481833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 529833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 561833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 553833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_KnockMore[] = {
	{.freq = 578167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 546167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 753833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Lancaster[] = {
	{.freq = 530167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 562167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_LarkStoke[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 498167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 522167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Limavady[] = {
	{.freq = 842000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 769833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 761833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 810167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Llanddona[] = {
	{.freq = 706000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 738167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 770167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 818167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 674000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Malvern[] = {
	{.freq = 722167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Mendip[] = {
	{.freq = 778167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 746167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 802167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 826167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 754167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Midhurst[] = {
	{.freq = 754167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 826167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 802167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 778167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 817833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Moel_y_Parc[] = {
	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Nottingham[] = {
	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_OliversMount[] = {
	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 842167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 738167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 770167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 818167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Oxford[] = {
	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 713833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 721833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_PendleForest[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 497833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 521833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 553833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Plympton[] = {
	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 842167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 833833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 785833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 809833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_PontopPike[] = {
	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 746167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 778167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 802167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 826167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 729833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Pontypool[] = {
	{.freq = 722000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 530167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Presely[] = {
	{.freq = 682167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 641833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 665833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 697833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Redruth[] = {
	{.freq = 474167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 497833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 521833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 634167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 650167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 714000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Reigate[] = {
	{.freq = 554000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 498000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 522000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_RidgeHill[] = {
	{.freq = 730000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 810000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Rosemarkie[] = {
	{.freq = 682167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 633833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 657833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 674167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 706167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Rosneath[] = {
	{.freq = 842000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 729833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 761833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 785833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 809833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Rowridge[] = {
	{.freq = 489833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 545833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 562167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 513833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 570167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_RumsterForest[] = {
	{.freq = 530167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 482167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 506167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 562167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 802000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Saddleworth[] = {
	{.freq = 682000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 633833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 657833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 713833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Salisbury[] = {
	{.freq = 745833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 753833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 777833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 801833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 826000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 721833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_SandyHeath[] = {
	{.freq = 641833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 665833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 650167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 842000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 626167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 674167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Selkirk[] = {
	{.freq = 730167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 810167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 834167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 754167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Sheffield[] = {
	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_StocklandHill[] = {
	{.freq = 514167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 490167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 505833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 481833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 529833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Storeton[] = {
	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 506000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 530000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 538000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 546000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Sudbury[] = {
	{.freq = 698167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 850000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 738000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 706167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 754000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_SuttonColdfield[] = {
	{.freq = 634167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 658167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 682167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 714167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 722167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Tacolneston[] = {
	{.freq = 810000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 786000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 730167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 769833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 818000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_TheWrekin[] = {
	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 554000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 498167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 522167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 730000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 762000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 698000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Torosay[] = {
	{.freq = 490167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 514167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 538167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 570167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 474000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 553833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_TunbridgeWells[] = {
	{.freq = 746000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642167000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 618000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Waltham[] = {
	{.freq = 698000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 490000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 570000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 666000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 642000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_Wenvoe[] = {
	{.freq = 546000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 625833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 705833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 649833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 673833000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_WhitehawkHill[] = {
	{.freq = 834000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 706000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 690000000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 770167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 794167000, .bw = 0, .constellation = 1, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_uk_WinterHill[] = {
	{.freq = 746000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 770000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 778000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 794000000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 	{.freq = 801833000, .bw = 0, .constellation = 3, .fechp = 2, .feclp = 0, .tmode = 1, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_vn_Hanoi[] = {
	{.freq = 514000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 578000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct mux muxes_DVBT_vn_Thaibinh[] = {
	{.freq = 626000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 	{.freq = 634000000, .bw = 0, .constellation = 3, .fechp = 3, .feclp = 0, .tmode = 0, .guard = 0, .hierarchy = 0},
 };

static const struct network networks_DVBT_auto[] = {
	{
		.name = "Australia",
		.muxes = muxes_DVBT_auto_Australia,
		.nmuxes = sizeof(muxes_DVBT_auto_Australia) / sizeof(struct mux),
	},
	{
		.name = "Default",
		.muxes = muxes_DVBT_auto_Default,
		.nmuxes = sizeof(muxes_DVBT_auto_Default) / sizeof(struct mux),
	},
	{
		.name = "Italy",
		.muxes = muxes_DVBT_auto_Italy,
		.nmuxes = sizeof(muxes_DVBT_auto_Italy) / sizeof(struct mux),
	},
	{
		.name = "Taiwan",
		.muxes = muxes_DVBT_auto_Taiwan,
		.nmuxes = sizeof(muxes_DVBT_auto_Taiwan) / sizeof(struct mux),
	},
	{
		.name = "With167kHzOffsets",
		.muxes = muxes_DVBT_auto_With167kHzOffsets,
		.nmuxes = sizeof(muxes_DVBT_auto_With167kHzOffsets) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_ax[] = {
	{
		.name = "Smedsbole",
		.muxes = muxes_DVBT_ax_Smedsbole,
		.nmuxes = sizeof(muxes_DVBT_ax_Smedsbole) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_ad[] = {
	{
		.name = "Andorra",
		.muxes = muxes_DVBT_ad_Andorra,
		.nmuxes = sizeof(muxes_DVBT_ad_Andorra) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_au[] = {
	{
		.name = "Adelaide",
		.muxes = muxes_DVBT_au_Adelaide,
		.nmuxes = sizeof(muxes_DVBT_au_Adelaide) / sizeof(struct mux),
	},
	{
		.name = "AdelaideFoothills",
		.muxes = muxes_DVBT_au_AdelaideFoothills,
		.nmuxes = sizeof(muxes_DVBT_au_AdelaideFoothills) / sizeof(struct mux),
	},
	{
		.name = "Ballarat",
		.muxes = muxes_DVBT_au_Ballarat,
		.nmuxes = sizeof(muxes_DVBT_au_Ballarat) / sizeof(struct mux),
	},
	{
		.name = "Bendigo",
		.muxes = muxes_DVBT_au_Bendigo,
		.nmuxes = sizeof(muxes_DVBT_au_Bendigo) / sizeof(struct mux),
	},
	{
		.name = "Brisbane",
		.muxes = muxes_DVBT_au_Brisbane,
		.nmuxes = sizeof(muxes_DVBT_au_Brisbane) / sizeof(struct mux),
	},
	{
		.name = "Cairns",
		.muxes = muxes_DVBT_au_Cairns,
		.nmuxes = sizeof(muxes_DVBT_au_Cairns) / sizeof(struct mux),
	},
	{
		.name = "Canberra-Black-Mt",
		.muxes = muxes_DVBT_au_Canberra_Black_Mt,
		.nmuxes = sizeof(muxes_DVBT_au_Canberra_Black_Mt) / sizeof(struct mux),
	},
	{
		.name = "Coonabarabran",
		.muxes = muxes_DVBT_au_Coonabarabran,
		.nmuxes = sizeof(muxes_DVBT_au_Coonabarabran) / sizeof(struct mux),
	},
	{
		.name = "Darwin",
		.muxes = muxes_DVBT_au_Darwin,
		.nmuxes = sizeof(muxes_DVBT_au_Darwin) / sizeof(struct mux),
	},
	{
		.name = "Devonport",
		.muxes = muxes_DVBT_au_Devonport,
		.nmuxes = sizeof(muxes_DVBT_au_Devonport) / sizeof(struct mux),
	},
	{
		.name = "FraserCoast-Bundaberg",
		.muxes = muxes_DVBT_au_FraserCoast_Bundaberg,
		.nmuxes = sizeof(muxes_DVBT_au_FraserCoast_Bundaberg) / sizeof(struct mux),
	},
	{
		.name = "GoldCoast",
		.muxes = muxes_DVBT_au_GoldCoast,
		.nmuxes = sizeof(muxes_DVBT_au_GoldCoast) / sizeof(struct mux),
	},
	{
		.name = "Goulburn-Rocky Hill",
		.muxes = muxes_DVBT_au_Goulburn_Rocky_Hill,
		.nmuxes = sizeof(muxes_DVBT_au_Goulburn_Rocky_Hill) / sizeof(struct mux),
	},
	{
		.name = "Hervey Bay-Ghost Hill",
		.muxes = muxes_DVBT_au_Hervey_Bay_Ghost_Hill,
		.nmuxes = sizeof(muxes_DVBT_au_Hervey_Bay_Ghost_Hill) / sizeof(struct mux),
	},
	{
		.name = "Hobart",
		.muxes = muxes_DVBT_au_Hobart,
		.nmuxes = sizeof(muxes_DVBT_au_Hobart) / sizeof(struct mux),
	},
	{
		.name = "Mackay",
		.muxes = muxes_DVBT_au_Mackay,
		.nmuxes = sizeof(muxes_DVBT_au_Mackay) / sizeof(struct mux),
	},
	{
		.name = "Melbourne",
		.muxes = muxes_DVBT_au_Melbourne,
		.nmuxes = sizeof(muxes_DVBT_au_Melbourne) / sizeof(struct mux),
	},
	{
		.name = "Melbourne-Upwey",
		.muxes = muxes_DVBT_au_Melbourne_Upwey,
		.nmuxes = sizeof(muxes_DVBT_au_Melbourne_Upwey) / sizeof(struct mux),
	},
	{
		.name = "MidNorthCoast",
		.muxes = muxes_DVBT_au_MidNorthCoast,
		.nmuxes = sizeof(muxes_DVBT_au_MidNorthCoast) / sizeof(struct mux),
	},
	{
		.name = "Newcastle",
		.muxes = muxes_DVBT_au_Newcastle,
		.nmuxes = sizeof(muxes_DVBT_au_Newcastle) / sizeof(struct mux),
	},
	{
		.name = "Perth",
		.muxes = muxes_DVBT_au_Perth,
		.nmuxes = sizeof(muxes_DVBT_au_Perth) / sizeof(struct mux),
	},
	{
		.name = "Perth Roleystone",
		.muxes = muxes_DVBT_au_Perth_Roleystone,
		.nmuxes = sizeof(muxes_DVBT_au_Perth_Roleystone) / sizeof(struct mux),
	},
	{
		.name = "SpencerGulf",
		.muxes = muxes_DVBT_au_SpencerGulf,
		.nmuxes = sizeof(muxes_DVBT_au_SpencerGulf) / sizeof(struct mux),
	},
	{
		.name = "SunshineCoast",
		.muxes = muxes_DVBT_au_SunshineCoast,
		.nmuxes = sizeof(muxes_DVBT_au_SunshineCoast) / sizeof(struct mux),
	},
	{
		.name = "Sutherland",
		.muxes = muxes_DVBT_au_Sutherland,
		.nmuxes = sizeof(muxes_DVBT_au_Sutherland) / sizeof(struct mux),
	},
	{
		.name = "Sydney Kings Cross",
		.muxes = muxes_DVBT_au_Sydney_Kings_Cross,
		.nmuxes = sizeof(muxes_DVBT_au_Sydney_Kings_Cross) / sizeof(struct mux),
	},
	{
		.name = "Sydney North Shore",
		.muxes = muxes_DVBT_au_Sydney_North_Shore,
		.nmuxes = sizeof(muxes_DVBT_au_Sydney_North_Shore) / sizeof(struct mux),
	},
	{
		.name = "Tamworth",
		.muxes = muxes_DVBT_au_Tamworth,
		.nmuxes = sizeof(muxes_DVBT_au_Tamworth) / sizeof(struct mux),
	},
	{
		.name = "Townsville",
		.muxes = muxes_DVBT_au_Townsville,
		.nmuxes = sizeof(muxes_DVBT_au_Townsville) / sizeof(struct mux),
	},
	{
		.name = "WaggaWagga",
		.muxes = muxes_DVBT_au_WaggaWagga,
		.nmuxes = sizeof(muxes_DVBT_au_WaggaWagga) / sizeof(struct mux),
	},
	{
		.name = "Wollongong",
		.muxes = muxes_DVBT_au_Wollongong,
		.nmuxes = sizeof(muxes_DVBT_au_Wollongong) / sizeof(struct mux),
	},
	{
		.name = "canberra",
		.muxes = muxes_DVBT_au_canberra,
		.nmuxes = sizeof(muxes_DVBT_au_canberra) / sizeof(struct mux),
	},
	{
		.name = "unknown",
		.muxes = muxes_DVBT_au_unknown,
		.nmuxes = sizeof(muxes_DVBT_au_unknown) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_at[] = {
	{
		.name = "Official",
		.muxes = muxes_DVBT_at_Official,
		.nmuxes = sizeof(muxes_DVBT_at_Official) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_be[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_be_All,
		.nmuxes = sizeof(muxes_DVBT_be_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_hr[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_hr_All,
		.nmuxes = sizeof(muxes_DVBT_hr_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_cz[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_cz_All,
		.nmuxes = sizeof(muxes_DVBT_cz_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_dk[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_dk_All,
		.nmuxes = sizeof(muxes_DVBT_dk_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_fi[] = {
	{
		.name = "Aanekoski",
		.muxes = muxes_DVBT_fi_Aanekoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Aanekoski) / sizeof(struct mux),
	},
	{
		.name = "Aanekoski Konginkangas",
		.muxes = muxes_DVBT_fi_Aanekoski_Konginkangas,
		.nmuxes = sizeof(muxes_DVBT_fi_Aanekoski_Konginkangas) / sizeof(struct mux),
	},
	{
		.name = "Ahtari",
		.muxes = muxes_DVBT_fi_Ahtari,
		.nmuxes = sizeof(muxes_DVBT_fi_Ahtari) / sizeof(struct mux),
	},
	{
		.name = "Ala-Vuokki",
		.muxes = muxes_DVBT_fi_Ala_Vuokki,
		.nmuxes = sizeof(muxes_DVBT_fi_Ala_Vuokki) / sizeof(struct mux),
	},
	{
		.name = "Alajarvi",
		.muxes = muxes_DVBT_fi_Alajarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Alajarvi) / sizeof(struct mux),
	},
	{
		.name = "Ammansaari",
		.muxes = muxes_DVBT_fi_Ammansaari,
		.nmuxes = sizeof(muxes_DVBT_fi_Ammansaari) / sizeof(struct mux),
	},
	{
		.name = "Anjalankoski",
		.muxes = muxes_DVBT_fi_Anjalankoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Anjalankoski) / sizeof(struct mux),
	},
	{
		.name = "Enontekio Raattama",
		.muxes = muxes_DVBT_fi_Enontekio_Raattama,
		.nmuxes = sizeof(muxes_DVBT_fi_Enontekio_Raattama) / sizeof(struct mux),
	},
	{
		.name = "Espoo",
		.muxes = muxes_DVBT_fi_Espoo,
		.nmuxes = sizeof(muxes_DVBT_fi_Espoo) / sizeof(struct mux),
	},
	{
		.name = "Eurajoki",
		.muxes = muxes_DVBT_fi_Eurajoki,
		.nmuxes = sizeof(muxes_DVBT_fi_Eurajoki) / sizeof(struct mux),
	},
	{
		.name = "Fiskars",
		.muxes = muxes_DVBT_fi_Fiskars,
		.nmuxes = sizeof(muxes_DVBT_fi_Fiskars) / sizeof(struct mux),
	},
	{
		.name = "Haapavesi",
		.muxes = muxes_DVBT_fi_Haapavesi,
		.nmuxes = sizeof(muxes_DVBT_fi_Haapavesi) / sizeof(struct mux),
	},
	{
		.name = "Hameenkyro Kyroskoski",
		.muxes = muxes_DVBT_fi_Hameenkyro_Kyroskoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Hameenkyro_Kyroskoski) / sizeof(struct mux),
	},
	{
		.name = "Hameenlinna Painokangas",
		.muxes = muxes_DVBT_fi_Hameenlinna_Painokangas,
		.nmuxes = sizeof(muxes_DVBT_fi_Hameenlinna_Painokangas) / sizeof(struct mux),
	},
	{
		.name = "Hanko",
		.muxes = muxes_DVBT_fi_Hanko,
		.nmuxes = sizeof(muxes_DVBT_fi_Hanko) / sizeof(struct mux),
	},
	{
		.name = "Hartola",
		.muxes = muxes_DVBT_fi_Hartola,
		.nmuxes = sizeof(muxes_DVBT_fi_Hartola) / sizeof(struct mux),
	},
	{
		.name = "Haukela",
		.muxes = muxes_DVBT_fi_Haukela,
		.nmuxes = sizeof(muxes_DVBT_fi_Haukela) / sizeof(struct mux),
	},
	{
		.name = "Heinavesi",
		.muxes = muxes_DVBT_fi_Heinavesi,
		.nmuxes = sizeof(muxes_DVBT_fi_Heinavesi) / sizeof(struct mux),
	},
	{
		.name = "Heinola",
		.muxes = muxes_DVBT_fi_Heinola,
		.nmuxes = sizeof(muxes_DVBT_fi_Heinola) / sizeof(struct mux),
	},
	{
		.name = "Hetta",
		.muxes = muxes_DVBT_fi_Hetta,
		.nmuxes = sizeof(muxes_DVBT_fi_Hetta) / sizeof(struct mux),
	},
	{
		.name = "Hossa",
		.muxes = muxes_DVBT_fi_Hossa,
		.nmuxes = sizeof(muxes_DVBT_fi_Hossa) / sizeof(struct mux),
	},
	{
		.name = "Houtskari",
		.muxes = muxes_DVBT_fi_Houtskari,
		.nmuxes = sizeof(muxes_DVBT_fi_Houtskari) / sizeof(struct mux),
	},
	{
		.name = "Hyrynsalmi",
		.muxes = muxes_DVBT_fi_Hyrynsalmi,
		.nmuxes = sizeof(muxes_DVBT_fi_Hyrynsalmi) / sizeof(struct mux),
	},
	{
		.name = "Hyrynsalmi Kyparavaara",
		.muxes = muxes_DVBT_fi_Hyrynsalmi_Kyparavaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Hyrynsalmi_Kyparavaara) / sizeof(struct mux),
	},
	{
		.name = "Hyrynsalmi Paljakka",
		.muxes = muxes_DVBT_fi_Hyrynsalmi_Paljakka,
		.nmuxes = sizeof(muxes_DVBT_fi_Hyrynsalmi_Paljakka) / sizeof(struct mux),
	},
	{
		.name = "Hyvinkaa Musta-Mannisto",
		.muxes = muxes_DVBT_fi_Hyvinkaa_Musta_Mannisto,
		.nmuxes = sizeof(muxes_DVBT_fi_Hyvinkaa_Musta_Mannisto) / sizeof(struct mux),
	},
	{
		.name = "Ii Raiskio",
		.muxes = muxes_DVBT_fi_Ii_Raiskio,
		.nmuxes = sizeof(muxes_DVBT_fi_Ii_Raiskio) / sizeof(struct mux),
	},
	{
		.name = "Iisalmi",
		.muxes = muxes_DVBT_fi_Iisalmi,
		.nmuxes = sizeof(muxes_DVBT_fi_Iisalmi) / sizeof(struct mux),
	},
	{
		.name = "Ikaalinen",
		.muxes = muxes_DVBT_fi_Ikaalinen,
		.nmuxes = sizeof(muxes_DVBT_fi_Ikaalinen) / sizeof(struct mux),
	},
	{
		.name = "Ikaalinen Riitiala",
		.muxes = muxes_DVBT_fi_Ikaalinen_Riitiala,
		.nmuxes = sizeof(muxes_DVBT_fi_Ikaalinen_Riitiala) / sizeof(struct mux),
	},
	{
		.name = "Inari",
		.muxes = muxes_DVBT_fi_Inari,
		.nmuxes = sizeof(muxes_DVBT_fi_Inari) / sizeof(struct mux),
	},
	{
		.name = "Inari Janispaa",
		.muxes = muxes_DVBT_fi_Inari_Janispaa,
		.nmuxes = sizeof(muxes_DVBT_fi_Inari_Janispaa) / sizeof(struct mux),
	},
	{
		.name = "Inari Naatamo",
		.muxes = muxes_DVBT_fi_Inari_Naatamo,
		.nmuxes = sizeof(muxes_DVBT_fi_Inari_Naatamo) / sizeof(struct mux),
	},
	{
		.name = "Ivalo Saarineitamovaara",
		.muxes = muxes_DVBT_fi_Ivalo_Saarineitamovaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Ivalo_Saarineitamovaara) / sizeof(struct mux),
	},
	{
		.name = "Jalasjarvi",
		.muxes = muxes_DVBT_fi_Jalasjarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Jalasjarvi) / sizeof(struct mux),
	},
	{
		.name = "Jamsa Kaipola",
		.muxes = muxes_DVBT_fi_Jamsa_Kaipola,
		.nmuxes = sizeof(muxes_DVBT_fi_Jamsa_Kaipola) / sizeof(struct mux),
	},
	{
		.name = "Jamsa Kuorevesi Halli",
		.muxes = muxes_DVBT_fi_Jamsa_Kuorevesi_Halli,
		.nmuxes = sizeof(muxes_DVBT_fi_Jamsa_Kuorevesi_Halli) / sizeof(struct mux),
	},
	{
		.name = "Jamsa Matkosvuori",
		.muxes = muxes_DVBT_fi_Jamsa_Matkosvuori,
		.nmuxes = sizeof(muxes_DVBT_fi_Jamsa_Matkosvuori) / sizeof(struct mux),
	},
	{
		.name = "Jamsa Ouninpohja",
		.muxes = muxes_DVBT_fi_Jamsa_Ouninpohja,
		.nmuxes = sizeof(muxes_DVBT_fi_Jamsa_Ouninpohja) / sizeof(struct mux),
	},
	{
		.name = "Jamsankoski",
		.muxes = muxes_DVBT_fi_Jamsankoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Jamsankoski) / sizeof(struct mux),
	},
	{
		.name = "Joensuu Vestinkallio",
		.muxes = muxes_DVBT_fi_Joensuu_Vestinkallio,
		.nmuxes = sizeof(muxes_DVBT_fi_Joensuu_Vestinkallio) / sizeof(struct mux),
	},
	{
		.name = "Joroinen Puukkola-Huutokoski",
		.muxes = muxes_DVBT_fi_Joroinen_Puukkola_Huutokoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Joroinen_Puukkola_Huutokoski) / sizeof(struct mux),
	},
	{
		.name = "Joutsa Lankia",
		.muxes = muxes_DVBT_fi_Joutsa_Lankia,
		.nmuxes = sizeof(muxes_DVBT_fi_Joutsa_Lankia) / sizeof(struct mux),
	},
	{
		.name = "Joutseno",
		.muxes = muxes_DVBT_fi_Joutseno,
		.nmuxes = sizeof(muxes_DVBT_fi_Joutseno) / sizeof(struct mux),
	},
	{
		.name = "Juntusranta",
		.muxes = muxes_DVBT_fi_Juntusranta,
		.nmuxes = sizeof(muxes_DVBT_fi_Juntusranta) / sizeof(struct mux),
	},
	{
		.name = "Juupajoki Kopsamo",
		.muxes = muxes_DVBT_fi_Juupajoki_Kopsamo,
		.nmuxes = sizeof(muxes_DVBT_fi_Juupajoki_Kopsamo) / sizeof(struct mux),
	},
	{
		.name = "Jyvaskyla",
		.muxes = muxes_DVBT_fi_Jyvaskyla,
		.nmuxes = sizeof(muxes_DVBT_fi_Jyvaskyla) / sizeof(struct mux),
	},
	{
		.name = "Kaavi Luikonlahti",
		.muxes = muxes_DVBT_fi_Kaavi_Luikonlahti,
		.nmuxes = sizeof(muxes_DVBT_fi_Kaavi_Luikonlahti) / sizeof(struct mux),
	},
	{
		.name = "Kajaani Pollyvaara",
		.muxes = muxes_DVBT_fi_Kajaani_Pollyvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Kajaani_Pollyvaara) / sizeof(struct mux),
	},
	{
		.name = "Kalajoki",
		.muxes = muxes_DVBT_fi_Kalajoki,
		.nmuxes = sizeof(muxes_DVBT_fi_Kalajoki) / sizeof(struct mux),
	},
	{
		.name = "Kangaslampi",
		.muxes = muxes_DVBT_fi_Kangaslampi,
		.nmuxes = sizeof(muxes_DVBT_fi_Kangaslampi) / sizeof(struct mux),
	},
	{
		.name = "Kangasniemi Turkinmaki",
		.muxes = muxes_DVBT_fi_Kangasniemi_Turkinmaki,
		.nmuxes = sizeof(muxes_DVBT_fi_Kangasniemi_Turkinmaki) / sizeof(struct mux),
	},
	{
		.name = "Kankaanpaa",
		.muxes = muxes_DVBT_fi_Kankaanpaa,
		.nmuxes = sizeof(muxes_DVBT_fi_Kankaanpaa) / sizeof(struct mux),
	},
	{
		.name = "Karigasniemi",
		.muxes = muxes_DVBT_fi_Karigasniemi,
		.nmuxes = sizeof(muxes_DVBT_fi_Karigasniemi) / sizeof(struct mux),
	},
	{
		.name = "Karkkila",
		.muxes = muxes_DVBT_fi_Karkkila,
		.nmuxes = sizeof(muxes_DVBT_fi_Karkkila) / sizeof(struct mux),
	},
	{
		.name = "Karstula",
		.muxes = muxes_DVBT_fi_Karstula,
		.nmuxes = sizeof(muxes_DVBT_fi_Karstula) / sizeof(struct mux),
	},
	{
		.name = "Karvia",
		.muxes = muxes_DVBT_fi_Karvia,
		.nmuxes = sizeof(muxes_DVBT_fi_Karvia) / sizeof(struct mux),
	},
	{
		.name = "Kaunispaa",
		.muxes = muxes_DVBT_fi_Kaunispaa,
		.nmuxes = sizeof(muxes_DVBT_fi_Kaunispaa) / sizeof(struct mux),
	},
	{
		.name = "Kemijarvi Suomutunturi",
		.muxes = muxes_DVBT_fi_Kemijarvi_Suomutunturi,
		.nmuxes = sizeof(muxes_DVBT_fi_Kemijarvi_Suomutunturi) / sizeof(struct mux),
	},
	{
		.name = "Kerimaki",
		.muxes = muxes_DVBT_fi_Kerimaki,
		.nmuxes = sizeof(muxes_DVBT_fi_Kerimaki) / sizeof(struct mux),
	},
	{
		.name = "Keuruu",
		.muxes = muxes_DVBT_fi_Keuruu,
		.nmuxes = sizeof(muxes_DVBT_fi_Keuruu) / sizeof(struct mux),
	},
	{
		.name = "Keuruu Haapamaki",
		.muxes = muxes_DVBT_fi_Keuruu_Haapamaki,
		.nmuxes = sizeof(muxes_DVBT_fi_Keuruu_Haapamaki) / sizeof(struct mux),
	},
	{
		.name = "Kihnio",
		.muxes = muxes_DVBT_fi_Kihnio,
		.nmuxes = sizeof(muxes_DVBT_fi_Kihnio) / sizeof(struct mux),
	},
	{
		.name = "Kiihtelysvaara",
		.muxes = muxes_DVBT_fi_Kiihtelysvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Kiihtelysvaara) / sizeof(struct mux),
	},
	{
		.name = "Kilpisjarvi",
		.muxes = muxes_DVBT_fi_Kilpisjarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Kilpisjarvi) / sizeof(struct mux),
	},
	{
		.name = "Kittila Sirkka Levitunturi",
		.muxes = muxes_DVBT_fi_Kittila_Sirkka_Levitunturi,
		.nmuxes = sizeof(muxes_DVBT_fi_Kittila_Sirkka_Levitunturi) / sizeof(struct mux),
	},
	{
		.name = "Kolari Vuolittaja",
		.muxes = muxes_DVBT_fi_Kolari_Vuolittaja,
		.nmuxes = sizeof(muxes_DVBT_fi_Kolari_Vuolittaja) / sizeof(struct mux),
	},
	{
		.name = "Koli",
		.muxes = muxes_DVBT_fi_Koli,
		.nmuxes = sizeof(muxes_DVBT_fi_Koli) / sizeof(struct mux),
	},
	{
		.name = "Korpilahti Vaarunvuori",
		.muxes = muxes_DVBT_fi_Korpilahti_Vaarunvuori,
		.nmuxes = sizeof(muxes_DVBT_fi_Korpilahti_Vaarunvuori) / sizeof(struct mux),
	},
	{
		.name = "Korppoo",
		.muxes = muxes_DVBT_fi_Korppoo,
		.nmuxes = sizeof(muxes_DVBT_fi_Korppoo) / sizeof(struct mux),
	},
	{
		.name = "Kruunupyy",
		.muxes = muxes_DVBT_fi_Kruunupyy,
		.nmuxes = sizeof(muxes_DVBT_fi_Kruunupyy) / sizeof(struct mux),
	},
	{
		.name = "Kuhmo Iivantiira",
		.muxes = muxes_DVBT_fi_Kuhmo_Iivantiira,
		.nmuxes = sizeof(muxes_DVBT_fi_Kuhmo_Iivantiira) / sizeof(struct mux),
	},
	{
		.name = "Kuhmo Lentiira",
		.muxes = muxes_DVBT_fi_Kuhmo_Lentiira,
		.nmuxes = sizeof(muxes_DVBT_fi_Kuhmo_Lentiira) / sizeof(struct mux),
	},
	{
		.name = "Kuhmoinen",
		.muxes = muxes_DVBT_fi_Kuhmoinen,
		.nmuxes = sizeof(muxes_DVBT_fi_Kuhmoinen) / sizeof(struct mux),
	},
	{
		.name = "Kuhmoinen Harjunsalmi",
		.muxes = muxes_DVBT_fi_Kuhmoinen_Harjunsalmi,
		.nmuxes = sizeof(muxes_DVBT_fi_Kuhmoinen_Harjunsalmi) / sizeof(struct mux),
	},
	{
		.name = "Kuhmoinen Puukkoinen",
		.muxes = muxes_DVBT_fi_Kuhmoinen_Puukkoinen,
		.nmuxes = sizeof(muxes_DVBT_fi_Kuhmoinen_Puukkoinen) / sizeof(struct mux),
	},
	{
		.name = "Kuopio",
		.muxes = muxes_DVBT_fi_Kuopio,
		.nmuxes = sizeof(muxes_DVBT_fi_Kuopio) / sizeof(struct mux),
	},
	{
		.name = "Kustavi Viherlahti",
		.muxes = muxes_DVBT_fi_Kustavi_Viherlahti,
		.nmuxes = sizeof(muxes_DVBT_fi_Kustavi_Viherlahti) / sizeof(struct mux),
	},
	{
		.name = "Kuttanen",
		.muxes = muxes_DVBT_fi_Kuttanen,
		.nmuxes = sizeof(muxes_DVBT_fi_Kuttanen) / sizeof(struct mux),
	},
	{
		.name = "Kyyjarvi Noposenaho",
		.muxes = muxes_DVBT_fi_Kyyjarvi_Noposenaho,
		.nmuxes = sizeof(muxes_DVBT_fi_Kyyjarvi_Noposenaho) / sizeof(struct mux),
	},
	{
		.name = "Lahti",
		.muxes = muxes_DVBT_fi_Lahti,
		.nmuxes = sizeof(muxes_DVBT_fi_Lahti) / sizeof(struct mux),
	},
	{
		.name = "Lapua",
		.muxes = muxes_DVBT_fi_Lapua,
		.nmuxes = sizeof(muxes_DVBT_fi_Lapua) / sizeof(struct mux),
	},
	{
		.name = "Laukaa",
		.muxes = muxes_DVBT_fi_Laukaa,
		.nmuxes = sizeof(muxes_DVBT_fi_Laukaa) / sizeof(struct mux),
	},
	{
		.name = "Laukaa Vihtavuori",
		.muxes = muxes_DVBT_fi_Laukaa_Vihtavuori,
		.nmuxes = sizeof(muxes_DVBT_fi_Laukaa_Vihtavuori) / sizeof(struct mux),
	},
	{
		.name = "Lavia Lavianjarvi",
		.muxes = muxes_DVBT_fi_Lavia_Lavianjarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Lavia_Lavianjarvi) / sizeof(struct mux),
	},
	{
		.name = "Lieksa Konnanvaara",
		.muxes = muxes_DVBT_fi_Lieksa_Konnanvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Lieksa_Konnanvaara) / sizeof(struct mux),
	},
	{
		.name = "Lohja",
		.muxes = muxes_DVBT_fi_Lohja,
		.nmuxes = sizeof(muxes_DVBT_fi_Lohja) / sizeof(struct mux),
	},
	{
		.name = "Loimaa",
		.muxes = muxes_DVBT_fi_Loimaa,
		.nmuxes = sizeof(muxes_DVBT_fi_Loimaa) / sizeof(struct mux),
	},
	{
		.name = "Luhanka",
		.muxes = muxes_DVBT_fi_Luhanka,
		.nmuxes = sizeof(muxes_DVBT_fi_Luhanka) / sizeof(struct mux),
	},
	{
		.name = "Luopioinen",
		.muxes = muxes_DVBT_fi_Luopioinen,
		.nmuxes = sizeof(muxes_DVBT_fi_Luopioinen) / sizeof(struct mux),
	},
	{
		.name = "Mantta",
		.muxes = muxes_DVBT_fi_Mantta,
		.nmuxes = sizeof(muxes_DVBT_fi_Mantta) / sizeof(struct mux),
	},
	{
		.name = "Mantyharju",
		.muxes = muxes_DVBT_fi_Mantyharju,
		.nmuxes = sizeof(muxes_DVBT_fi_Mantyharju) / sizeof(struct mux),
	},
	{
		.name = "Mikkeli",
		.muxes = muxes_DVBT_fi_Mikkeli,
		.nmuxes = sizeof(muxes_DVBT_fi_Mikkeli) / sizeof(struct mux),
	},
	{
		.name = "Muonio Olostunturi",
		.muxes = muxes_DVBT_fi_Muonio_Olostunturi,
		.nmuxes = sizeof(muxes_DVBT_fi_Muonio_Olostunturi) / sizeof(struct mux),
	},
	{
		.name = "Mustasaari",
		.muxes = muxes_DVBT_fi_Mustasaari,
		.nmuxes = sizeof(muxes_DVBT_fi_Mustasaari) / sizeof(struct mux),
	},
	{
		.name = "Myllylahti",
		.muxes = muxes_DVBT_fi_Myllylahti,
		.nmuxes = sizeof(muxes_DVBT_fi_Myllylahti) / sizeof(struct mux),
	},
	{
		.name = "Nilsia",
		.muxes = muxes_DVBT_fi_Nilsia,
		.nmuxes = sizeof(muxes_DVBT_fi_Nilsia) / sizeof(struct mux),
	},
	{
		.name = "Nilsia Keski-Siikajarvi",
		.muxes = muxes_DVBT_fi_Nilsia_Keski_Siikajarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Nilsia_Keski_Siikajarvi) / sizeof(struct mux),
	},
	{
		.name = "Nilsia Pisa",
		.muxes = muxes_DVBT_fi_Nilsia_Pisa,
		.nmuxes = sizeof(muxes_DVBT_fi_Nilsia_Pisa) / sizeof(struct mux),
	},
	{
		.name = "Nokia",
		.muxes = muxes_DVBT_fi_Nokia,
		.nmuxes = sizeof(muxes_DVBT_fi_Nokia) / sizeof(struct mux),
	},
	{
		.name = "Nokia Siuro Linnavuori",
		.muxes = muxes_DVBT_fi_Nokia_Siuro_Linnavuori,
		.nmuxes = sizeof(muxes_DVBT_fi_Nokia_Siuro_Linnavuori) / sizeof(struct mux),
	},
	{
		.name = "Nummi-Pusula Hyonola",
		.muxes = muxes_DVBT_fi_Nummi_Pusula_Hyonola,
		.nmuxes = sizeof(muxes_DVBT_fi_Nummi_Pusula_Hyonola) / sizeof(struct mux),
	},
	{
		.name = "Nurmes Kortevaara",
		.muxes = muxes_DVBT_fi_Nurmes_Kortevaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Nurmes_Kortevaara) / sizeof(struct mux),
	},
	{
		.name = "Orivesi Langelmaki Talviainen",
		.muxes = muxes_DVBT_fi_Orivesi_Langelmaki_Talviainen,
		.nmuxes = sizeof(muxes_DVBT_fi_Orivesi_Langelmaki_Talviainen) / sizeof(struct mux),
	},
	{
		.name = "Oulu",
		.muxes = muxes_DVBT_fi_Oulu,
		.nmuxes = sizeof(muxes_DVBT_fi_Oulu) / sizeof(struct mux),
	},
	{
		.name = "Padasjoki",
		.muxes = muxes_DVBT_fi_Padasjoki,
		.nmuxes = sizeof(muxes_DVBT_fi_Padasjoki) / sizeof(struct mux),
	},
	{
		.name = "Padasjoki Arrakoski",
		.muxes = muxes_DVBT_fi_Padasjoki_Arrakoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Padasjoki_Arrakoski) / sizeof(struct mux),
	},
	{
		.name = "Paltamo Kivesvaara",
		.muxes = muxes_DVBT_fi_Paltamo_Kivesvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Paltamo_Kivesvaara) / sizeof(struct mux),
	},
	{
		.name = "Parikkala",
		.muxes = muxes_DVBT_fi_Parikkala,
		.nmuxes = sizeof(muxes_DVBT_fi_Parikkala) / sizeof(struct mux),
	},
	{
		.name = "Parkano Sopukallio",
		.muxes = muxes_DVBT_fi_Parkano_Sopukallio,
		.nmuxes = sizeof(muxes_DVBT_fi_Parkano_Sopukallio) / sizeof(struct mux),
	},
	{
		.name = "Pello",
		.muxes = muxes_DVBT_fi_Pello,
		.nmuxes = sizeof(muxes_DVBT_fi_Pello) / sizeof(struct mux),
	},
	{
		.name = "Pello Ratasvaara",
		.muxes = muxes_DVBT_fi_Pello_Ratasvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Pello_Ratasvaara) / sizeof(struct mux),
	},
	{
		.name = "Perho",
		.muxes = muxes_DVBT_fi_Perho,
		.nmuxes = sizeof(muxes_DVBT_fi_Perho) / sizeof(struct mux),
	},
	{
		.name = "Pernaja",
		.muxes = muxes_DVBT_fi_Pernaja,
		.nmuxes = sizeof(muxes_DVBT_fi_Pernaja) / sizeof(struct mux),
	},
	{
		.name = "Pieksamaki Halkokumpu",
		.muxes = muxes_DVBT_fi_Pieksamaki_Halkokumpu,
		.nmuxes = sizeof(muxes_DVBT_fi_Pieksamaki_Halkokumpu) / sizeof(struct mux),
	},
	{
		.name = "Pihtipudas",
		.muxes = muxes_DVBT_fi_Pihtipudas,
		.nmuxes = sizeof(muxes_DVBT_fi_Pihtipudas) / sizeof(struct mux),
	},
	{
		.name = "Porvoo Suomenkyla",
		.muxes = muxes_DVBT_fi_Porvoo_Suomenkyla,
		.nmuxes = sizeof(muxes_DVBT_fi_Porvoo_Suomenkyla) / sizeof(struct mux),
	},
	{
		.name = "Posio",
		.muxes = muxes_DVBT_fi_Posio,
		.nmuxes = sizeof(muxes_DVBT_fi_Posio) / sizeof(struct mux),
	},
	{
		.name = "Pudasjarvi",
		.muxes = muxes_DVBT_fi_Pudasjarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Pudasjarvi) / sizeof(struct mux),
	},
	{
		.name = "Pudasjarvi Hirvaskoski",
		.muxes = muxes_DVBT_fi_Pudasjarvi_Hirvaskoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Pudasjarvi_Hirvaskoski) / sizeof(struct mux),
	},
	{
		.name = "Pudasjarvi Iso-Syote",
		.muxes = muxes_DVBT_fi_Pudasjarvi_Iso_Syote,
		.nmuxes = sizeof(muxes_DVBT_fi_Pudasjarvi_Iso_Syote) / sizeof(struct mux),
	},
	{
		.name = "Puolanka",
		.muxes = muxes_DVBT_fi_Puolanka,
		.nmuxes = sizeof(muxes_DVBT_fi_Puolanka) / sizeof(struct mux),
	},
	{
		.name = "Pyhatunturi",
		.muxes = muxes_DVBT_fi_Pyhatunturi,
		.nmuxes = sizeof(muxes_DVBT_fi_Pyhatunturi) / sizeof(struct mux),
	},
	{
		.name = "Pyhavuori",
		.muxes = muxes_DVBT_fi_Pyhavuori,
		.nmuxes = sizeof(muxes_DVBT_fi_Pyhavuori) / sizeof(struct mux),
	},
	{
		.name = "Pylkonmaki Karankajarvi",
		.muxes = muxes_DVBT_fi_Pylkonmaki_Karankajarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Pylkonmaki_Karankajarvi) / sizeof(struct mux),
	},
	{
		.name = "Raahe Mestauskallio",
		.muxes = muxes_DVBT_fi_Raahe_Mestauskallio,
		.nmuxes = sizeof(muxes_DVBT_fi_Raahe_Mestauskallio) / sizeof(struct mux),
	},
	{
		.name = "Raahe Piehinki",
		.muxes = muxes_DVBT_fi_Raahe_Piehinki,
		.nmuxes = sizeof(muxes_DVBT_fi_Raahe_Piehinki) / sizeof(struct mux),
	},
	{
		.name = "Ranua Haasionmaa",
		.muxes = muxes_DVBT_fi_Ranua_Haasionmaa,
		.nmuxes = sizeof(muxes_DVBT_fi_Ranua_Haasionmaa) / sizeof(struct mux),
	},
	{
		.name = "Ranua Leppiaho",
		.muxes = muxes_DVBT_fi_Ranua_Leppiaho,
		.nmuxes = sizeof(muxes_DVBT_fi_Ranua_Leppiaho) / sizeof(struct mux),
	},
	{
		.name = "Rautavaara Angervikko",
		.muxes = muxes_DVBT_fi_Rautavaara_Angervikko,
		.nmuxes = sizeof(muxes_DVBT_fi_Rautavaara_Angervikko) / sizeof(struct mux),
	},
	{
		.name = "Rautjarvi Simpele",
		.muxes = muxes_DVBT_fi_Rautjarvi_Simpele,
		.nmuxes = sizeof(muxes_DVBT_fi_Rautjarvi_Simpele) / sizeof(struct mux),
	},
	{
		.name = "Ristijarvi",
		.muxes = muxes_DVBT_fi_Ristijarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Ristijarvi) / sizeof(struct mux),
	},
	{
		.name = "Rovaniemi",
		.muxes = muxes_DVBT_fi_Rovaniemi,
		.nmuxes = sizeof(muxes_DVBT_fi_Rovaniemi) / sizeof(struct mux),
	},
	{
		.name = "Rovaniemi Ala-Nampa Yli-Nampa Rantalaki",
		.muxes = muxes_DVBT_fi_Rovaniemi_Ala_Nampa_Yli_Nampa_Rantalaki,
		.nmuxes = sizeof(muxes_DVBT_fi_Rovaniemi_Ala_Nampa_Yli_Nampa_Rantalaki) / sizeof(struct mux),
	},
	{
		.name = "Rovaniemi Kaihuanvaara",
		.muxes = muxes_DVBT_fi_Rovaniemi_Kaihuanvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Rovaniemi_Kaihuanvaara) / sizeof(struct mux),
	},
	{
		.name = "Rovaniemi Marasenkallio",
		.muxes = muxes_DVBT_fi_Rovaniemi_Marasenkallio,
		.nmuxes = sizeof(muxes_DVBT_fi_Rovaniemi_Marasenkallio) / sizeof(struct mux),
	},
	{
		.name = "Rovaniemi Marrasjarvi",
		.muxes = muxes_DVBT_fi_Rovaniemi_Marrasjarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Rovaniemi_Marrasjarvi) / sizeof(struct mux),
	},
	{
		.name = "Rovaniemi Meltaus Sorviselka",
		.muxes = muxes_DVBT_fi_Rovaniemi_Meltaus_Sorviselka,
		.nmuxes = sizeof(muxes_DVBT_fi_Rovaniemi_Meltaus_Sorviselka) / sizeof(struct mux),
	},
	{
		.name = "Rovaniemi Sonka",
		.muxes = muxes_DVBT_fi_Rovaniemi_Sonka,
		.nmuxes = sizeof(muxes_DVBT_fi_Rovaniemi_Sonka) / sizeof(struct mux),
	},
	{
		.name = "Ruka",
		.muxes = muxes_DVBT_fi_Ruka,
		.nmuxes = sizeof(muxes_DVBT_fi_Ruka) / sizeof(struct mux),
	},
	{
		.name = "Ruovesi Storminiemi",
		.muxes = muxes_DVBT_fi_Ruovesi_Storminiemi,
		.nmuxes = sizeof(muxes_DVBT_fi_Ruovesi_Storminiemi) / sizeof(struct mux),
	},
	{
		.name = "Saarijarvi",
		.muxes = muxes_DVBT_fi_Saarijarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Saarijarvi) / sizeof(struct mux),
	},
	{
		.name = "Saarijarvi Kalmari",
		.muxes = muxes_DVBT_fi_Saarijarvi_Kalmari,
		.nmuxes = sizeof(muxes_DVBT_fi_Saarijarvi_Kalmari) / sizeof(struct mux),
	},
	{
		.name = "Saarijarvi Mahlu",
		.muxes = muxes_DVBT_fi_Saarijarvi_Mahlu,
		.nmuxes = sizeof(muxes_DVBT_fi_Saarijarvi_Mahlu) / sizeof(struct mux),
	},
	{
		.name = "Salla Hirvasvaara",
		.muxes = muxes_DVBT_fi_Salla_Hirvasvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Salla_Hirvasvaara) / sizeof(struct mux),
	},
	{
		.name = "Salla Ihistysjanka",
		.muxes = muxes_DVBT_fi_Salla_Ihistysjanka,
		.nmuxes = sizeof(muxes_DVBT_fi_Salla_Ihistysjanka) / sizeof(struct mux),
	},
	{
		.name = "Salla Naruska",
		.muxes = muxes_DVBT_fi_Salla_Naruska,
		.nmuxes = sizeof(muxes_DVBT_fi_Salla_Naruska) / sizeof(struct mux),
	},
	{
		.name = "Salla Sallatunturi",
		.muxes = muxes_DVBT_fi_Salla_Sallatunturi,
		.nmuxes = sizeof(muxes_DVBT_fi_Salla_Sallatunturi) / sizeof(struct mux),
	},
	{
		.name = "Salla Sarivaara",
		.muxes = muxes_DVBT_fi_Salla_Sarivaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Salla_Sarivaara) / sizeof(struct mux),
	},
	{
		.name = "Salo Isokyla",
		.muxes = muxes_DVBT_fi_Salo_Isokyla,
		.nmuxes = sizeof(muxes_DVBT_fi_Salo_Isokyla) / sizeof(struct mux),
	},
	{
		.name = "Savukoski Martti",
		.muxes = muxes_DVBT_fi_Savukoski_Martti,
		.nmuxes = sizeof(muxes_DVBT_fi_Savukoski_Martti) / sizeof(struct mux),
	},
	{
		.name = "Savukoski Tanhua",
		.muxes = muxes_DVBT_fi_Savukoski_Tanhua,
		.nmuxes = sizeof(muxes_DVBT_fi_Savukoski_Tanhua) / sizeof(struct mux),
	},
	{
		.name = "Siilinjarvi",
		.muxes = muxes_DVBT_fi_Siilinjarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Siilinjarvi) / sizeof(struct mux),
	},
	{
		.name = "Sipoo Norrkulla",
		.muxes = muxes_DVBT_fi_Sipoo_Norrkulla,
		.nmuxes = sizeof(muxes_DVBT_fi_Sipoo_Norrkulla) / sizeof(struct mux),
	},
	{
		.name = "Sodankyla Pittiovaara",
		.muxes = muxes_DVBT_fi_Sodankyla_Pittiovaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Sodankyla_Pittiovaara) / sizeof(struct mux),
	},
	{
		.name = "Sulkava Vaatalanmaki",
		.muxes = muxes_DVBT_fi_Sulkava_Vaatalanmaki,
		.nmuxes = sizeof(muxes_DVBT_fi_Sulkava_Vaatalanmaki) / sizeof(struct mux),
	},
	{
		.name = "Sysma Liikola",
		.muxes = muxes_DVBT_fi_Sysma_Liikola,
		.nmuxes = sizeof(muxes_DVBT_fi_Sysma_Liikola) / sizeof(struct mux),
	},
	{
		.name = "Taivalkoski",
		.muxes = muxes_DVBT_fi_Taivalkoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Taivalkoski) / sizeof(struct mux),
	},
	{
		.name = "Taivalkoski Taivalvaara",
		.muxes = muxes_DVBT_fi_Taivalkoski_Taivalvaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Taivalkoski_Taivalvaara) / sizeof(struct mux),
	},
	{
		.name = "Tammela",
		.muxes = muxes_DVBT_fi_Tammela,
		.nmuxes = sizeof(muxes_DVBT_fi_Tammela) / sizeof(struct mux),
	},
	{
		.name = "Tammisaari",
		.muxes = muxes_DVBT_fi_Tammisaari,
		.nmuxes = sizeof(muxes_DVBT_fi_Tammisaari) / sizeof(struct mux),
	},
	{
		.name = "Tampere",
		.muxes = muxes_DVBT_fi_Tampere,
		.nmuxes = sizeof(muxes_DVBT_fi_Tampere) / sizeof(struct mux),
	},
	{
		.name = "Tampere Pyynikki",
		.muxes = muxes_DVBT_fi_Tampere_Pyynikki,
		.nmuxes = sizeof(muxes_DVBT_fi_Tampere_Pyynikki) / sizeof(struct mux),
	},
	{
		.name = "Tervola",
		.muxes = muxes_DVBT_fi_Tervola,
		.nmuxes = sizeof(muxes_DVBT_fi_Tervola) / sizeof(struct mux),
	},
	{
		.name = "Turku",
		.muxes = muxes_DVBT_fi_Turku,
		.nmuxes = sizeof(muxes_DVBT_fi_Turku) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki",
		.muxes = muxes_DVBT_fi_Utsjoki,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki Nuorgam Njallavaara",
		.muxes = muxes_DVBT_fi_Utsjoki_Nuorgam_Njallavaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki_Nuorgam_Njallavaara) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki Nuorgam raja",
		.muxes = muxes_DVBT_fi_Utsjoki_Nuorgam_raja,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki_Nuorgam_raja) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki Nuvvus",
		.muxes = muxes_DVBT_fi_Utsjoki_Nuvvus,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki_Nuvvus) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki Outakoski",
		.muxes = muxes_DVBT_fi_Utsjoki_Outakoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki_Outakoski) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki Polvarniemi",
		.muxes = muxes_DVBT_fi_Utsjoki_Polvarniemi,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki_Polvarniemi) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki Rovisuvanto",
		.muxes = muxes_DVBT_fi_Utsjoki_Rovisuvanto,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki_Rovisuvanto) / sizeof(struct mux),
	},
	{
		.name = "Utsjoki Tenola",
		.muxes = muxes_DVBT_fi_Utsjoki_Tenola,
		.nmuxes = sizeof(muxes_DVBT_fi_Utsjoki_Tenola) / sizeof(struct mux),
	},
	{
		.name = "Uusikaupunki Orivo",
		.muxes = muxes_DVBT_fi_Uusikaupunki_Orivo,
		.nmuxes = sizeof(muxes_DVBT_fi_Uusikaupunki_Orivo) / sizeof(struct mux),
	},
	{
		.name = "Vaajakoski",
		.muxes = muxes_DVBT_fi_Vaajakoski,
		.nmuxes = sizeof(muxes_DVBT_fi_Vaajakoski) / sizeof(struct mux),
	},
	{
		.name = "Vaala",
		.muxes = muxes_DVBT_fi_Vaala,
		.nmuxes = sizeof(muxes_DVBT_fi_Vaala) / sizeof(struct mux),
	},
	{
		.name = "Vaasa",
		.muxes = muxes_DVBT_fi_Vaasa,
		.nmuxes = sizeof(muxes_DVBT_fi_Vaasa) / sizeof(struct mux),
	},
	{
		.name = "Valtimo",
		.muxes = muxes_DVBT_fi_Valtimo,
		.nmuxes = sizeof(muxes_DVBT_fi_Valtimo) / sizeof(struct mux),
	},
	{
		.name = "Vammala Jyranvuori",
		.muxes = muxes_DVBT_fi_Vammala_Jyranvuori,
		.nmuxes = sizeof(muxes_DVBT_fi_Vammala_Jyranvuori) / sizeof(struct mux),
	},
	{
		.name = "Vammala Roismala",
		.muxes = muxes_DVBT_fi_Vammala_Roismala,
		.nmuxes = sizeof(muxes_DVBT_fi_Vammala_Roismala) / sizeof(struct mux),
	},
	{
		.name = "Vammala Savi",
		.muxes = muxes_DVBT_fi_Vammala_Savi,
		.nmuxes = sizeof(muxes_DVBT_fi_Vammala_Savi) / sizeof(struct mux),
	},
	{
		.name = "Vantaa Hakunila",
		.muxes = muxes_DVBT_fi_Vantaa_Hakunila,
		.nmuxes = sizeof(muxes_DVBT_fi_Vantaa_Hakunila) / sizeof(struct mux),
	},
	{
		.name = "Varpaisjarvi Honkamaki",
		.muxes = muxes_DVBT_fi_Varpaisjarvi_Honkamaki,
		.nmuxes = sizeof(muxes_DVBT_fi_Varpaisjarvi_Honkamaki) / sizeof(struct mux),
	},
	{
		.name = "Virrat Lappavuori",
		.muxes = muxes_DVBT_fi_Virrat_Lappavuori,
		.nmuxes = sizeof(muxes_DVBT_fi_Virrat_Lappavuori) / sizeof(struct mux),
	},
	{
		.name = "Vuokatti",
		.muxes = muxes_DVBT_fi_Vuokatti,
		.nmuxes = sizeof(muxes_DVBT_fi_Vuokatti) / sizeof(struct mux),
	},
	{
		.name = "Vuotso",
		.muxes = muxes_DVBT_fi_Vuotso,
		.nmuxes = sizeof(muxes_DVBT_fi_Vuotso) / sizeof(struct mux),
	},
	{
		.name = "Ylitornio Ainiovaara",
		.muxes = muxes_DVBT_fi_Ylitornio_Ainiovaara,
		.nmuxes = sizeof(muxes_DVBT_fi_Ylitornio_Ainiovaara) / sizeof(struct mux),
	},
	{
		.name = "Ylitornio Raanujarvi",
		.muxes = muxes_DVBT_fi_Ylitornio_Raanujarvi,
		.nmuxes = sizeof(muxes_DVBT_fi_Ylitornio_Raanujarvi) / sizeof(struct mux),
	},
	{
		.name = "Yllas",
		.muxes = muxes_DVBT_fi_Yllas,
		.nmuxes = sizeof(muxes_DVBT_fi_Yllas) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_de[] = {
	{
		.name = "Baden-Wuerttemberg",
		.muxes = muxes_DVBT_de_Baden_Wuerttemberg,
		.nmuxes = sizeof(muxes_DVBT_de_Baden_Wuerttemberg) / sizeof(struct mux),
	},
	{
		.name = "Bayern",
		.muxes = muxes_DVBT_de_Bayern,
		.nmuxes = sizeof(muxes_DVBT_de_Bayern) / sizeof(struct mux),
	},
	{
		.name = "Berlin",
		.muxes = muxes_DVBT_de_Berlin,
		.nmuxes = sizeof(muxes_DVBT_de_Berlin) / sizeof(struct mux),
	},
	{
		.name = "Brandenburg",
		.muxes = muxes_DVBT_de_Brandenburg,
		.nmuxes = sizeof(muxes_DVBT_de_Brandenburg) / sizeof(struct mux),
	},
	{
		.name = "Bremen",
		.muxes = muxes_DVBT_de_Bremen,
		.nmuxes = sizeof(muxes_DVBT_de_Bremen) / sizeof(struct mux),
	},
	{
		.name = "Hamburg",
		.muxes = muxes_DVBT_de_Hamburg,
		.nmuxes = sizeof(muxes_DVBT_de_Hamburg) / sizeof(struct mux),
	},
	{
		.name = "Hessen",
		.muxes = muxes_DVBT_de_Hessen,
		.nmuxes = sizeof(muxes_DVBT_de_Hessen) / sizeof(struct mux),
	},
	{
		.name = "Mecklenburg-Vorpommern",
		.muxes = muxes_DVBT_de_Mecklenburg_Vorpommern,
		.nmuxes = sizeof(muxes_DVBT_de_Mecklenburg_Vorpommern) / sizeof(struct mux),
	},
	{
		.name = "Niedersachsen",
		.muxes = muxes_DVBT_de_Niedersachsen,
		.nmuxes = sizeof(muxes_DVBT_de_Niedersachsen) / sizeof(struct mux),
	},
	{
		.name = "Nordrhein-Westfalen",
		.muxes = muxes_DVBT_de_Nordrhein_Westfalen,
		.nmuxes = sizeof(muxes_DVBT_de_Nordrhein_Westfalen) / sizeof(struct mux),
	},
	{
		.name = "Rheinland-Pfalz",
		.muxes = muxes_DVBT_de_Rheinland_Pfalz,
		.nmuxes = sizeof(muxes_DVBT_de_Rheinland_Pfalz) / sizeof(struct mux),
	},
	{
		.name = "Saarland",
		.muxes = muxes_DVBT_de_Saarland,
		.nmuxes = sizeof(muxes_DVBT_de_Saarland) / sizeof(struct mux),
	},
	{
		.name = "Sachsen",
		.muxes = muxes_DVBT_de_Sachsen,
		.nmuxes = sizeof(muxes_DVBT_de_Sachsen) / sizeof(struct mux),
	},
	{
		.name = "Sachsen-Anhalt",
		.muxes = muxes_DVBT_de_Sachsen_Anhalt,
		.nmuxes = sizeof(muxes_DVBT_de_Sachsen_Anhalt) / sizeof(struct mux),
	},
	{
		.name = "Schleswig-Holstein",
		.muxes = muxes_DVBT_de_Schleswig_Holstein,
		.nmuxes = sizeof(muxes_DVBT_de_Schleswig_Holstein) / sizeof(struct mux),
	},
	{
		.name = "Thueringen",
		.muxes = muxes_DVBT_de_Thueringen,
		.nmuxes = sizeof(muxes_DVBT_de_Thueringen) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_gr[] = {
	{
		.name = "Athens",
		.muxes = muxes_DVBT_gr_Athens,
		.nmuxes = sizeof(muxes_DVBT_gr_Athens) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_hk[] = {
	{
		.name = "HongKong",
		.muxes = muxes_DVBT_hk_HongKong,
		.nmuxes = sizeof(muxes_DVBT_hk_HongKong) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_hu[] = {
	{
		.name = "Bekescsaba",
		.muxes = muxes_DVBT_hu_Bekescsaba,
		.nmuxes = sizeof(muxes_DVBT_hu_Bekescsaba) / sizeof(struct mux),
	},
	{
		.name = "Budapest",
		.muxes = muxes_DVBT_hu_Budapest,
		.nmuxes = sizeof(muxes_DVBT_hu_Budapest) / sizeof(struct mux),
	},
	{
		.name = "Csavoly-Kiskoros",
		.muxes = muxes_DVBT_hu_Csavoly_Kiskoros,
		.nmuxes = sizeof(muxes_DVBT_hu_Csavoly_Kiskoros) / sizeof(struct mux),
	},
	{
		.name = "Gerecse",
		.muxes = muxes_DVBT_hu_Gerecse,
		.nmuxes = sizeof(muxes_DVBT_hu_Gerecse) / sizeof(struct mux),
	},
	{
		.name = "Gyor-Tatabanya",
		.muxes = muxes_DVBT_hu_Gyor_Tatabanya,
		.nmuxes = sizeof(muxes_DVBT_hu_Gyor_Tatabanya) / sizeof(struct mux),
	},
	{
		.name = "Kabhegy-Mor-Siofok",
		.muxes = muxes_DVBT_hu_Kabhegy_Mor_Siofok,
		.nmuxes = sizeof(muxes_DVBT_hu_Kabhegy_Mor_Siofok) / sizeof(struct mux),
	},
	{
		.name = "Karcag",
		.muxes = muxes_DVBT_hu_Karcag,
		.nmuxes = sizeof(muxes_DVBT_hu_Karcag) / sizeof(struct mux),
	},
	{
		.name = "Kekes-Ozd",
		.muxes = muxes_DVBT_hu_Kekes_Ozd,
		.nmuxes = sizeof(muxes_DVBT_hu_Kekes_Ozd) / sizeof(struct mux),
	},
	{
		.name = "Kiskoros",
		.muxes = muxes_DVBT_hu_Kiskoros,
		.nmuxes = sizeof(muxes_DVBT_hu_Kiskoros) / sizeof(struct mux),
	},
	{
		.name = "Komadi-Debrecen",
		.muxes = muxes_DVBT_hu_Komadi_Debrecen,
		.nmuxes = sizeof(muxes_DVBT_hu_Komadi_Debrecen) / sizeof(struct mux),
	},
	{
		.name = "Nagykanizsa-Kaposvar",
		.muxes = muxes_DVBT_hu_Nagykanizsa_Kaposvar,
		.nmuxes = sizeof(muxes_DVBT_hu_Nagykanizsa_Kaposvar) / sizeof(struct mux),
	},
	{
		.name = "Pecs-Uzd",
		.muxes = muxes_DVBT_hu_Pecs_Uzd,
		.nmuxes = sizeof(muxes_DVBT_hu_Pecs_Uzd) / sizeof(struct mux),
	},
	{
		.name = "Salgotarjan",
		.muxes = muxes_DVBT_hu_Salgotarjan,
		.nmuxes = sizeof(muxes_DVBT_hu_Salgotarjan) / sizeof(struct mux),
	},
	{
		.name = "Sopron",
		.muxes = muxes_DVBT_hu_Sopron,
		.nmuxes = sizeof(muxes_DVBT_hu_Sopron) / sizeof(struct mux),
	},
	{
		.name = "Szentes-Szeged",
		.muxes = muxes_DVBT_hu_Szentes_Szeged,
		.nmuxes = sizeof(muxes_DVBT_hu_Szentes_Szeged) / sizeof(struct mux),
	},
	{
		.name = "Tokaj-Kazincbarcika",
		.muxes = muxes_DVBT_hu_Tokaj_Kazincbarcika,
		.nmuxes = sizeof(muxes_DVBT_hu_Tokaj_Kazincbarcika) / sizeof(struct mux),
	},
	{
		.name = "Vasvar",
		.muxes = muxes_DVBT_hu_Vasvar,
		.nmuxes = sizeof(muxes_DVBT_hu_Vasvar) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_is[] = {
	{
		.name = "Reykjavik",
		.muxes = muxes_DVBT_is_Reykjavik,
		.nmuxes = sizeof(muxes_DVBT_is_Reykjavik) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_ir[] = {
	{
		.name = "Tehran",
		.muxes = muxes_DVBT_ir_Tehran,
		.nmuxes = sizeof(muxes_DVBT_ir_Tehran) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_il[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_il_All,
		.nmuxes = sizeof(muxes_DVBT_il_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_it[] = {
	{
		.name = "Aosta",
		.muxes = muxes_DVBT_it_Aosta,
		.nmuxes = sizeof(muxes_DVBT_it_Aosta) / sizeof(struct mux),
	},
	{
		.name = "Bari",
		.muxes = muxes_DVBT_it_Bari,
		.nmuxes = sizeof(muxes_DVBT_it_Bari) / sizeof(struct mux),
	},
	{
		.name = "Bologna",
		.muxes = muxes_DVBT_it_Bologna,
		.nmuxes = sizeof(muxes_DVBT_it_Bologna) / sizeof(struct mux),
	},
	{
		.name = "Bolzano",
		.muxes = muxes_DVBT_it_Bolzano,
		.nmuxes = sizeof(muxes_DVBT_it_Bolzano) / sizeof(struct mux),
	},
	{
		.name = "Cagliari",
		.muxes = muxes_DVBT_it_Cagliari,
		.nmuxes = sizeof(muxes_DVBT_it_Cagliari) / sizeof(struct mux),
	},
	{
		.name = "Caivano",
		.muxes = muxes_DVBT_it_Caivano,
		.nmuxes = sizeof(muxes_DVBT_it_Caivano) / sizeof(struct mux),
	},
	{
		.name = "Catania",
		.muxes = muxes_DVBT_it_Catania,
		.nmuxes = sizeof(muxes_DVBT_it_Catania) / sizeof(struct mux),
	},
	{
		.name = "Conero",
		.muxes = muxes_DVBT_it_Conero,
		.nmuxes = sizeof(muxes_DVBT_it_Conero) / sizeof(struct mux),
	},
	{
		.name = "Firenze",
		.muxes = muxes_DVBT_it_Firenze,
		.nmuxes = sizeof(muxes_DVBT_it_Firenze) / sizeof(struct mux),
	},
	{
		.name = "Genova",
		.muxes = muxes_DVBT_it_Genova,
		.nmuxes = sizeof(muxes_DVBT_it_Genova) / sizeof(struct mux),
	},
	{
		.name = "Livorno",
		.muxes = muxes_DVBT_it_Livorno,
		.nmuxes = sizeof(muxes_DVBT_it_Livorno) / sizeof(struct mux),
	},
	{
		.name = "Milano",
		.muxes = muxes_DVBT_it_Milano,
		.nmuxes = sizeof(muxes_DVBT_it_Milano) / sizeof(struct mux),
	},
	{
		.name = "Modena",
		.muxes = muxes_DVBT_it_Modena,
		.nmuxes = sizeof(muxes_DVBT_it_Modena) / sizeof(struct mux),
	},
	{
		.name = "Montevergine",
		.muxes = muxes_DVBT_it_Montevergine,
		.nmuxes = sizeof(muxes_DVBT_it_Montevergine) / sizeof(struct mux),
	},
	{
		.name = "Pagnacco",
		.muxes = muxes_DVBT_it_Pagnacco,
		.nmuxes = sizeof(muxes_DVBT_it_Pagnacco) / sizeof(struct mux),
	},
	{
		.name = "Palermo",
		.muxes = muxes_DVBT_it_Palermo,
		.nmuxes = sizeof(muxes_DVBT_it_Palermo) / sizeof(struct mux),
	},
	{
		.name = "Pescara",
		.muxes = muxes_DVBT_it_Pescara,
		.nmuxes = sizeof(muxes_DVBT_it_Pescara) / sizeof(struct mux),
	},
	{
		.name = "Pisa",
		.muxes = muxes_DVBT_it_Pisa,
		.nmuxes = sizeof(muxes_DVBT_it_Pisa) / sizeof(struct mux),
	},
	{
		.name = "Roma",
		.muxes = muxes_DVBT_it_Roma,
		.nmuxes = sizeof(muxes_DVBT_it_Roma) / sizeof(struct mux),
	},
	{
		.name = "S-Stefano al mare",
		.muxes = muxes_DVBT_it_S_Stefano_al_mare,
		.nmuxes = sizeof(muxes_DVBT_it_S_Stefano_al_mare) / sizeof(struct mux),
	},
	{
		.name = "Sassari",
		.muxes = muxes_DVBT_it_Sassari,
		.nmuxes = sizeof(muxes_DVBT_it_Sassari) / sizeof(struct mux),
	},
	{
		.name = "Torino",
		.muxes = muxes_DVBT_it_Torino,
		.nmuxes = sizeof(muxes_DVBT_it_Torino) / sizeof(struct mux),
	},
	{
		.name = "Trieste",
		.muxes = muxes_DVBT_it_Trieste,
		.nmuxes = sizeof(muxes_DVBT_it_Trieste) / sizeof(struct mux),
	},
	{
		.name = "Varese",
		.muxes = muxes_DVBT_it_Varese,
		.nmuxes = sizeof(muxes_DVBT_it_Varese) / sizeof(struct mux),
	},
	{
		.name = "Venezia",
		.muxes = muxes_DVBT_it_Venezia,
		.nmuxes = sizeof(muxes_DVBT_it_Venezia) / sizeof(struct mux),
	},
	{
		.name = "Verona",
		.muxes = muxes_DVBT_it_Verona,
		.nmuxes = sizeof(muxes_DVBT_it_Verona) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_lv[] = {
	{
		.name = "Riga",
		.muxes = muxes_DVBT_lv_Riga,
		.nmuxes = sizeof(muxes_DVBT_lv_Riga) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_lt[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_lt_All,
		.nmuxes = sizeof(muxes_DVBT_lt_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_lu[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_lu_All,
		.nmuxes = sizeof(muxes_DVBT_lu_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_nl[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_nl_All,
		.nmuxes = sizeof(muxes_DVBT_nl_All) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_nz[] = {
	{
		.name = "AucklandInfill",
		.muxes = muxes_DVBT_nz_AucklandInfill,
		.nmuxes = sizeof(muxes_DVBT_nz_AucklandInfill) / sizeof(struct mux),
	},
	{
		.name = "AucklandWaiatarua",
		.muxes = muxes_DVBT_nz_AucklandWaiatarua,
		.nmuxes = sizeof(muxes_DVBT_nz_AucklandWaiatarua) / sizeof(struct mux),
	},
	{
		.name = "Christchurch",
		.muxes = muxes_DVBT_nz_Christchurch,
		.nmuxes = sizeof(muxes_DVBT_nz_Christchurch) / sizeof(struct mux),
	},
	{
		.name = "Dunedin",
		.muxes = muxes_DVBT_nz_Dunedin,
		.nmuxes = sizeof(muxes_DVBT_nz_Dunedin) / sizeof(struct mux),
	},
	{
		.name = "Hamilton",
		.muxes = muxes_DVBT_nz_Hamilton,
		.nmuxes = sizeof(muxes_DVBT_nz_Hamilton) / sizeof(struct mux),
	},
	{
		.name = "HawkesBayMtErin",
		.muxes = muxes_DVBT_nz_HawkesBayMtErin,
		.nmuxes = sizeof(muxes_DVBT_nz_HawkesBayMtErin) / sizeof(struct mux),
	},
	{
		.name = "HawkesBayNapier",
		.muxes = muxes_DVBT_nz_HawkesBayNapier,
		.nmuxes = sizeof(muxes_DVBT_nz_HawkesBayNapier) / sizeof(struct mux),
	},
	{
		.name = "Manawatu",
		.muxes = muxes_DVBT_nz_Manawatu,
		.nmuxes = sizeof(muxes_DVBT_nz_Manawatu) / sizeof(struct mux),
	},
	{
		.name = "Tauranga",
		.muxes = muxes_DVBT_nz_Tauranga,
		.nmuxes = sizeof(muxes_DVBT_nz_Tauranga) / sizeof(struct mux),
	},
	{
		.name = "Waikato",
		.muxes = muxes_DVBT_nz_Waikato,
		.nmuxes = sizeof(muxes_DVBT_nz_Waikato) / sizeof(struct mux),
	},
	{
		.name = "WellingtonInfill",
		.muxes = muxes_DVBT_nz_WellingtonInfill,
		.nmuxes = sizeof(muxes_DVBT_nz_WellingtonInfill) / sizeof(struct mux),
	},
	{
		.name = "WellingtonKaukau",
		.muxes = muxes_DVBT_nz_WellingtonKaukau,
		.nmuxes = sizeof(muxes_DVBT_nz_WellingtonKaukau) / sizeof(struct mux),
	},
	{
		.name = "WellingtonNgarara",
		.muxes = muxes_DVBT_nz_WellingtonNgarara,
		.nmuxes = sizeof(muxes_DVBT_nz_WellingtonNgarara) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_no[] = {
	{
		.name = "Trondelag Stjordal",
		.muxes = muxes_DVBT_no_Trondelag_Stjordal,
		.nmuxes = sizeof(muxes_DVBT_no_Trondelag_Stjordal) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_pl[] = {
	{
		.name = "Rzeszow",
		.muxes = muxes_DVBT_pl_Rzeszow,
		.nmuxes = sizeof(muxes_DVBT_pl_Rzeszow) / sizeof(struct mux),
	},
	{
		.name = "Warszawa",
		.muxes = muxes_DVBT_pl_Warszawa,
		.nmuxes = sizeof(muxes_DVBT_pl_Warszawa) / sizeof(struct mux),
	},
	{
		.name = "Wroclaw",
		.muxes = muxes_DVBT_pl_Wroclaw,
		.nmuxes = sizeof(muxes_DVBT_pl_Wroclaw) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_ro[] = {
	{
		.name = "Bucharest",
		.muxes = muxes_DVBT_ro_Bucharest,
		.nmuxes = sizeof(muxes_DVBT_ro_Bucharest) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_sk[] = {
	{
		.name = "BanskaBystrica",
		.muxes = muxes_DVBT_sk_BanskaBystrica,
		.nmuxes = sizeof(muxes_DVBT_sk_BanskaBystrica) / sizeof(struct mux),
	},
	{
		.name = "Bardejov",
		.muxes = muxes_DVBT_sk_Bardejov,
		.nmuxes = sizeof(muxes_DVBT_sk_Bardejov) / sizeof(struct mux),
	},
	{
		.name = "Bratislava",
		.muxes = muxes_DVBT_sk_Bratislava,
		.nmuxes = sizeof(muxes_DVBT_sk_Bratislava) / sizeof(struct mux),
	},
	{
		.name = "Kosice",
		.muxes = muxes_DVBT_sk_Kosice,
		.nmuxes = sizeof(muxes_DVBT_sk_Kosice) / sizeof(struct mux),
	},
	{
		.name = "Michalovce",
		.muxes = muxes_DVBT_sk_Michalovce,
		.nmuxes = sizeof(muxes_DVBT_sk_Michalovce) / sizeof(struct mux),
	},
	{
		.name = "Namestovo",
		.muxes = muxes_DVBT_sk_Namestovo,
		.nmuxes = sizeof(muxes_DVBT_sk_Namestovo) / sizeof(struct mux),
	},
	{
		.name = "Poprad",
		.muxes = muxes_DVBT_sk_Poprad,
		.nmuxes = sizeof(muxes_DVBT_sk_Poprad) / sizeof(struct mux),
	},
	{
		.name = "Presov",
		.muxes = muxes_DVBT_sk_Presov,
		.nmuxes = sizeof(muxes_DVBT_sk_Presov) / sizeof(struct mux),
	},
	{
		.name = "RimavskaSobota",
		.muxes = muxes_DVBT_sk_RimavskaSobota,
		.nmuxes = sizeof(muxes_DVBT_sk_RimavskaSobota) / sizeof(struct mux),
	},
	{
		.name = "Trencin",
		.muxes = muxes_DVBT_sk_Trencin,
		.nmuxes = sizeof(muxes_DVBT_sk_Trencin) / sizeof(struct mux),
	},
	{
		.name = "VelkyKrtis",
		.muxes = muxes_DVBT_sk_VelkyKrtis,
		.nmuxes = sizeof(muxes_DVBT_sk_VelkyKrtis) / sizeof(struct mux),
	},
	{
		.name = "Zilina",
		.muxes = muxes_DVBT_sk_Zilina,
		.nmuxes = sizeof(muxes_DVBT_sk_Zilina) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_si[] = {
	{
		.name = "Ljubljana",
		.muxes = muxes_DVBT_si_Ljubljana,
		.nmuxes = sizeof(muxes_DVBT_si_Ljubljana) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_es[] = {
	{
		.name = "Albacete",
		.muxes = muxes_DVBT_es_Albacete,
		.nmuxes = sizeof(muxes_DVBT_es_Albacete) / sizeof(struct mux),
	},
	{
		.name = "Alfabia",
		.muxes = muxes_DVBT_es_Alfabia,
		.nmuxes = sizeof(muxes_DVBT_es_Alfabia) / sizeof(struct mux),
	},
	{
		.name = "Alicante",
		.muxes = muxes_DVBT_es_Alicante,
		.nmuxes = sizeof(muxes_DVBT_es_Alicante) / sizeof(struct mux),
	},
	{
		.name = "Alpicat",
		.muxes = muxes_DVBT_es_Alpicat,
		.nmuxes = sizeof(muxes_DVBT_es_Alpicat) / sizeof(struct mux),
	},
	{
		.name = "Asturias",
		.muxes = muxes_DVBT_es_Asturias,
		.nmuxes = sizeof(muxes_DVBT_es_Asturias) / sizeof(struct mux),
	},
	{
		.name = "BaixoMinho",
		.muxes = muxes_DVBT_es_BaixoMinho,
		.nmuxes = sizeof(muxes_DVBT_es_BaixoMinho) / sizeof(struct mux),
	},
	{
		.name = "Bilbao",
		.muxes = muxes_DVBT_es_Bilbao,
		.nmuxes = sizeof(muxes_DVBT_es_Bilbao) / sizeof(struct mux),
	},
	{
		.name = "Cadiz",
		.muxes = muxes_DVBT_es_Cadiz,
		.nmuxes = sizeof(muxes_DVBT_es_Cadiz) / sizeof(struct mux),
	},
	{
		.name = "Carceres",
		.muxes = muxes_DVBT_es_Carceres,
		.nmuxes = sizeof(muxes_DVBT_es_Carceres) / sizeof(struct mux),
	},
	{
		.name = "Collserola",
		.muxes = muxes_DVBT_es_Collserola,
		.nmuxes = sizeof(muxes_DVBT_es_Collserola) / sizeof(struct mux),
	},
	{
		.name = "Donostia",
		.muxes = muxes_DVBT_es_Donostia,
		.nmuxes = sizeof(muxes_DVBT_es_Donostia) / sizeof(struct mux),
	},
	{
		.name = "Granada",
		.muxes = muxes_DVBT_es_Granada,
		.nmuxes = sizeof(muxes_DVBT_es_Granada) / sizeof(struct mux),
	},
	{
		.name = "Huesca",
		.muxes = muxes_DVBT_es_Huesca,
		.nmuxes = sizeof(muxes_DVBT_es_Huesca) / sizeof(struct mux),
	},
	{
		.name = "Las Palmas",
		.muxes = muxes_DVBT_es_Las_Palmas,
		.nmuxes = sizeof(muxes_DVBT_es_Las_Palmas) / sizeof(struct mux),
	},
	{
		.name = "Lugo",
		.muxes = muxes_DVBT_es_Lugo,
		.nmuxes = sizeof(muxes_DVBT_es_Lugo) / sizeof(struct mux),
	},
	{
		.name = "Madrid",
		.muxes = muxes_DVBT_es_Madrid,
		.nmuxes = sizeof(muxes_DVBT_es_Madrid) / sizeof(struct mux),
	},
	{
		.name = "Malaga",
		.muxes = muxes_DVBT_es_Malaga,
		.nmuxes = sizeof(muxes_DVBT_es_Malaga) / sizeof(struct mux),
	},
	{
		.name = "Muros-Noia",
		.muxes = muxes_DVBT_es_Muros_Noia,
		.nmuxes = sizeof(muxes_DVBT_es_Muros_Noia) / sizeof(struct mux),
	},
	{
		.name = "Mussara",
		.muxes = muxes_DVBT_es_Mussara,
		.nmuxes = sizeof(muxes_DVBT_es_Mussara) / sizeof(struct mux),
	},
	{
		.name = "Pamplona",
		.muxes = muxes_DVBT_es_Pamplona,
		.nmuxes = sizeof(muxes_DVBT_es_Pamplona) / sizeof(struct mux),
	},
	{
		.name = "Rocacorba",
		.muxes = muxes_DVBT_es_Rocacorba,
		.nmuxes = sizeof(muxes_DVBT_es_Rocacorba) / sizeof(struct mux),
	},
	{
		.name = "SC Tenerife",
		.muxes = muxes_DVBT_es_SC_Tenerife,
		.nmuxes = sizeof(muxes_DVBT_es_SC_Tenerife) / sizeof(struct mux),
	},
	{
		.name = "Santander",
		.muxes = muxes_DVBT_es_Santander,
		.nmuxes = sizeof(muxes_DVBT_es_Santander) / sizeof(struct mux),
	},
	{
		.name = "Santiago de Compostela",
		.muxes = muxes_DVBT_es_Santiago_de_Compostela,
		.nmuxes = sizeof(muxes_DVBT_es_Santiago_de_Compostela) / sizeof(struct mux),
	},
	{
		.name = "Sevilla",
		.muxes = muxes_DVBT_es_Sevilla,
		.nmuxes = sizeof(muxes_DVBT_es_Sevilla) / sizeof(struct mux),
	},
	{
		.name = "Tenerife",
		.muxes = muxes_DVBT_es_Tenerife,
		.nmuxes = sizeof(muxes_DVBT_es_Tenerife) / sizeof(struct mux),
	},
	{
		.name = "Valencia",
		.muxes = muxes_DVBT_es_Valencia,
		.nmuxes = sizeof(muxes_DVBT_es_Valencia) / sizeof(struct mux),
	},
	{
		.name = "Valladolid",
		.muxes = muxes_DVBT_es_Valladolid,
		.nmuxes = sizeof(muxes_DVBT_es_Valladolid) / sizeof(struct mux),
	},
	{
		.name = "Vilamarxant",
		.muxes = muxes_DVBT_es_Vilamarxant,
		.nmuxes = sizeof(muxes_DVBT_es_Vilamarxant) / sizeof(struct mux),
	},
	{
		.name = "Vitoria-Gasteiz",
		.muxes = muxes_DVBT_es_Vitoria_Gasteiz,
		.nmuxes = sizeof(muxes_DVBT_es_Vitoria_Gasteiz) / sizeof(struct mux),
	},
	{
		.name = "Zaragoza",
		.muxes = muxes_DVBT_es_Zaragoza,
		.nmuxes = sizeof(muxes_DVBT_es_Zaragoza) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_se[] = {
	{
		.name = "Alvdalen Brunnsberg",
		.muxes = muxes_DVBT_se_Alvdalen_Brunnsberg,
		.nmuxes = sizeof(muxes_DVBT_se_Alvdalen_Brunnsberg) / sizeof(struct mux),
	},
	{
		.name = "Alvdalsasen",
		.muxes = muxes_DVBT_se_Alvdalsasen,
		.nmuxes = sizeof(muxes_DVBT_se_Alvdalsasen) / sizeof(struct mux),
	},
	{
		.name = "Alvsbyn",
		.muxes = muxes_DVBT_se_Alvsbyn,
		.nmuxes = sizeof(muxes_DVBT_se_Alvsbyn) / sizeof(struct mux),
	},
	{
		.name = "Amot",
		.muxes = muxes_DVBT_se_Amot,
		.nmuxes = sizeof(muxes_DVBT_se_Amot) / sizeof(struct mux),
	},
	{
		.name = "Ange Snoberg",
		.muxes = muxes_DVBT_se_Ange_Snoberg,
		.nmuxes = sizeof(muxes_DVBT_se_Ange_Snoberg) / sizeof(struct mux),
	},
	{
		.name = "Angebo",
		.muxes = muxes_DVBT_se_Angebo,
		.nmuxes = sizeof(muxes_DVBT_se_Angebo) / sizeof(struct mux),
	},
	{
		.name = "Angelholm Vegeholm",
		.muxes = muxes_DVBT_se_Angelholm_Vegeholm,
		.nmuxes = sizeof(muxes_DVBT_se_Angelholm_Vegeholm) / sizeof(struct mux),
	},
	{
		.name = "Arvidsjaur Jultrask",
		.muxes = muxes_DVBT_se_Arvidsjaur_Jultrask,
		.nmuxes = sizeof(muxes_DVBT_se_Arvidsjaur_Jultrask) / sizeof(struct mux),
	},
	{
		.name = "Aspeboda",
		.muxes = muxes_DVBT_se_Aspeboda,
		.nmuxes = sizeof(muxes_DVBT_se_Aspeboda) / sizeof(struct mux),
	},
	{
		.name = "Atvidaberg",
		.muxes = muxes_DVBT_se_Atvidaberg,
		.nmuxes = sizeof(muxes_DVBT_se_Atvidaberg) / sizeof(struct mux),
	},
	{
		.name = "Avesta Krylbo",
		.muxes = muxes_DVBT_se_Avesta_Krylbo,
		.nmuxes = sizeof(muxes_DVBT_se_Avesta_Krylbo) / sizeof(struct mux),
	},
	{
		.name = "Backefors",
		.muxes = muxes_DVBT_se_Backefors,
		.nmuxes = sizeof(muxes_DVBT_se_Backefors) / sizeof(struct mux),
	},
	{
		.name = "Bankeryd",
		.muxes = muxes_DVBT_se_Bankeryd,
		.nmuxes = sizeof(muxes_DVBT_se_Bankeryd) / sizeof(struct mux),
	},
	{
		.name = "Bergsjo Balleberget",
		.muxes = muxes_DVBT_se_Bergsjo_Balleberget,
		.nmuxes = sizeof(muxes_DVBT_se_Bergsjo_Balleberget) / sizeof(struct mux),
	},
	{
		.name = "Bergvik",
		.muxes = muxes_DVBT_se_Bergvik,
		.nmuxes = sizeof(muxes_DVBT_se_Bergvik) / sizeof(struct mux),
	},
	{
		.name = "Bollebygd",
		.muxes = muxes_DVBT_se_Bollebygd,
		.nmuxes = sizeof(muxes_DVBT_se_Bollebygd) / sizeof(struct mux),
	},
	{
		.name = "Bollnas",
		.muxes = muxes_DVBT_se_Bollnas,
		.nmuxes = sizeof(muxes_DVBT_se_Bollnas) / sizeof(struct mux),
	},
	{
		.name = "Boras Dalsjofors",
		.muxes = muxes_DVBT_se_Boras_Dalsjofors,
		.nmuxes = sizeof(muxes_DVBT_se_Boras_Dalsjofors) / sizeof(struct mux),
	},
	{
		.name = "Boras Sjobo",
		.muxes = muxes_DVBT_se_Boras_Sjobo,
		.nmuxes = sizeof(muxes_DVBT_se_Boras_Sjobo) / sizeof(struct mux),
	},
	{
		.name = "Borlange Idkerberget",
		.muxes = muxes_DVBT_se_Borlange_Idkerberget,
		.nmuxes = sizeof(muxes_DVBT_se_Borlange_Idkerberget) / sizeof(struct mux),
	},
	{
		.name = "Borlange Nygardarna",
		.muxes = muxes_DVBT_se_Borlange_Nygardarna,
		.nmuxes = sizeof(muxes_DVBT_se_Borlange_Nygardarna) / sizeof(struct mux),
	},
	{
		.name = "Bottnaryd Ryd",
		.muxes = muxes_DVBT_se_Bottnaryd_Ryd,
		.nmuxes = sizeof(muxes_DVBT_se_Bottnaryd_Ryd) / sizeof(struct mux),
	},
	{
		.name = "Bromsebro",
		.muxes = muxes_DVBT_se_Bromsebro,
		.nmuxes = sizeof(muxes_DVBT_se_Bromsebro) / sizeof(struct mux),
	},
	{
		.name = "Bruzaholm",
		.muxes = muxes_DVBT_se_Bruzaholm,
		.nmuxes = sizeof(muxes_DVBT_se_Bruzaholm) / sizeof(struct mux),
	},
	{
		.name = "Byxelkrok",
		.muxes = muxes_DVBT_se_Byxelkrok,
		.nmuxes = sizeof(muxes_DVBT_se_Byxelkrok) / sizeof(struct mux),
	},
	{
		.name = "Dadran",
		.muxes = muxes_DVBT_se_Dadran,
		.nmuxes = sizeof(muxes_DVBT_se_Dadran) / sizeof(struct mux),
	},
	{
		.name = "Dalfors",
		.muxes = muxes_DVBT_se_Dalfors,
		.nmuxes = sizeof(muxes_DVBT_se_Dalfors) / sizeof(struct mux),
	},
	{
		.name = "Dalstuga",
		.muxes = muxes_DVBT_se_Dalstuga,
		.nmuxes = sizeof(muxes_DVBT_se_Dalstuga) / sizeof(struct mux),
	},
	{
		.name = "Degerfors",
		.muxes = muxes_DVBT_se_Degerfors,
		.nmuxes = sizeof(muxes_DVBT_se_Degerfors) / sizeof(struct mux),
	},
	{
		.name = "Delary",
		.muxes = muxes_DVBT_se_Delary,
		.nmuxes = sizeof(muxes_DVBT_se_Delary) / sizeof(struct mux),
	},
	{
		.name = "Djura",
		.muxes = muxes_DVBT_se_Djura,
		.nmuxes = sizeof(muxes_DVBT_se_Djura) / sizeof(struct mux),
	},
	{
		.name = "Drevdagen",
		.muxes = muxes_DVBT_se_Drevdagen,
		.nmuxes = sizeof(muxes_DVBT_se_Drevdagen) / sizeof(struct mux),
	},
	{
		.name = "Duvnas",
		.muxes = muxes_DVBT_se_Duvnas,
		.nmuxes = sizeof(muxes_DVBT_se_Duvnas) / sizeof(struct mux),
	},
	{
		.name = "Duvnas Basna",
		.muxes = muxes_DVBT_se_Duvnas_Basna,
		.nmuxes = sizeof(muxes_DVBT_se_Duvnas_Basna) / sizeof(struct mux),
	},
	{
		.name = "Edsbyn",
		.muxes = muxes_DVBT_se_Edsbyn,
		.nmuxes = sizeof(muxes_DVBT_se_Edsbyn) / sizeof(struct mux),
	},
	{
		.name = "Emmaboda Balshult",
		.muxes = muxes_DVBT_se_Emmaboda_Balshult,
		.nmuxes = sizeof(muxes_DVBT_se_Emmaboda_Balshult) / sizeof(struct mux),
	},
	{
		.name = "Enviken",
		.muxes = muxes_DVBT_se_Enviken,
		.nmuxes = sizeof(muxes_DVBT_se_Enviken) / sizeof(struct mux),
	},
	{
		.name = "Fagersta",
		.muxes = muxes_DVBT_se_Fagersta,
		.nmuxes = sizeof(muxes_DVBT_se_Fagersta) / sizeof(struct mux),
	},
	{
		.name = "Falerum Centrum",
		.muxes = muxes_DVBT_se_Falerum_Centrum,
		.nmuxes = sizeof(muxes_DVBT_se_Falerum_Centrum) / sizeof(struct mux),
	},
	{
		.name = "Falun Lovberget",
		.muxes = muxes_DVBT_se_Falun_Lovberget,
		.nmuxes = sizeof(muxes_DVBT_se_Falun_Lovberget) / sizeof(struct mux),
	},
	{
		.name = "Farila",
		.muxes = muxes_DVBT_se_Farila,
		.nmuxes = sizeof(muxes_DVBT_se_Farila) / sizeof(struct mux),
	},
	{
		.name = "Faro Ajkerstrask",
		.muxes = muxes_DVBT_se_Faro_Ajkerstrask,
		.nmuxes = sizeof(muxes_DVBT_se_Faro_Ajkerstrask) / sizeof(struct mux),
	},
	{
		.name = "Farosund Bunge",
		.muxes = muxes_DVBT_se_Farosund_Bunge,
		.nmuxes = sizeof(muxes_DVBT_se_Farosund_Bunge) / sizeof(struct mux),
	},
	{
		.name = "Filipstad Klockarhojden",
		.muxes = muxes_DVBT_se_Filipstad_Klockarhojden,
		.nmuxes = sizeof(muxes_DVBT_se_Filipstad_Klockarhojden) / sizeof(struct mux),
	},
	{
		.name = "Finnveden",
		.muxes = muxes_DVBT_se_Finnveden,
		.nmuxes = sizeof(muxes_DVBT_se_Finnveden) / sizeof(struct mux),
	},
	{
		.name = "Fredriksberg",
		.muxes = muxes_DVBT_se_Fredriksberg,
		.nmuxes = sizeof(muxes_DVBT_se_Fredriksberg) / sizeof(struct mux),
	},
	{
		.name = "Fritsla",
		.muxes = muxes_DVBT_se_Fritsla,
		.nmuxes = sizeof(muxes_DVBT_se_Fritsla) / sizeof(struct mux),
	},
	{
		.name = "Furudal",
		.muxes = muxes_DVBT_se_Furudal,
		.nmuxes = sizeof(muxes_DVBT_se_Furudal) / sizeof(struct mux),
	},
	{
		.name = "Gallivare",
		.muxes = muxes_DVBT_se_Gallivare,
		.nmuxes = sizeof(muxes_DVBT_se_Gallivare) / sizeof(struct mux),
	},
	{
		.name = "Garpenberg Kuppgarden",
		.muxes = muxes_DVBT_se_Garpenberg_Kuppgarden,
		.nmuxes = sizeof(muxes_DVBT_se_Garpenberg_Kuppgarden) / sizeof(struct mux),
	},
	{
		.name = "Gavle Skogmur",
		.muxes = muxes_DVBT_se_Gavle_Skogmur,
		.nmuxes = sizeof(muxes_DVBT_se_Gavle_Skogmur) / sizeof(struct mux),
	},
	{
		.name = "Gnarp",
		.muxes = muxes_DVBT_se_Gnarp,
		.nmuxes = sizeof(muxes_DVBT_se_Gnarp) / sizeof(struct mux),
	},
	{
		.name = "Gnesta",
		.muxes = muxes_DVBT_se_Gnesta,
		.nmuxes = sizeof(muxes_DVBT_se_Gnesta) / sizeof(struct mux),
	},
	{
		.name = "Gnosjo Marieholm",
		.muxes = muxes_DVBT_se_Gnosjo_Marieholm,
		.nmuxes = sizeof(muxes_DVBT_se_Gnosjo_Marieholm) / sizeof(struct mux),
	},
	{
		.name = "Goteborg Brudaremossen",
		.muxes = muxes_DVBT_se_Goteborg_Brudaremossen,
		.nmuxes = sizeof(muxes_DVBT_se_Goteborg_Brudaremossen) / sizeof(struct mux),
	},
	{
		.name = "Goteborg Slattadamm",
		.muxes = muxes_DVBT_se_Goteborg_Slattadamm,
		.nmuxes = sizeof(muxes_DVBT_se_Goteborg_Slattadamm) / sizeof(struct mux),
	},
	{
		.name = "Gullbrandstorp",
		.muxes = muxes_DVBT_se_Gullbrandstorp,
		.nmuxes = sizeof(muxes_DVBT_se_Gullbrandstorp) / sizeof(struct mux),
	},
	{
		.name = "Gunnarsbo",
		.muxes = muxes_DVBT_se_Gunnarsbo,
		.nmuxes = sizeof(muxes_DVBT_se_Gunnarsbo) / sizeof(struct mux),
	},
	{
		.name = "Gusum",
		.muxes = muxes_DVBT_se_Gusum,
		.nmuxes = sizeof(muxes_DVBT_se_Gusum) / sizeof(struct mux),
	},
	{
		.name = "Hagfors Varmullsasen",
		.muxes = muxes_DVBT_se_Hagfors_Varmullsasen,
		.nmuxes = sizeof(muxes_DVBT_se_Hagfors_Varmullsasen) / sizeof(struct mux),
	},
	{
		.name = "Hallaryd",
		.muxes = muxes_DVBT_se_Hallaryd,
		.nmuxes = sizeof(muxes_DVBT_se_Hallaryd) / sizeof(struct mux),
	},
	{
		.name = "Hallbo",
		.muxes = muxes_DVBT_se_Hallbo,
		.nmuxes = sizeof(muxes_DVBT_se_Hallbo) / sizeof(struct mux),
	},
	{
		.name = "Halmstad Hamnen",
		.muxes = muxes_DVBT_se_Halmstad_Hamnen,
		.nmuxes = sizeof(muxes_DVBT_se_Halmstad_Hamnen) / sizeof(struct mux),
	},
	{
		.name = "Halmstad Oskarstrom",
		.muxes = muxes_DVBT_se_Halmstad_Oskarstrom,
		.nmuxes = sizeof(muxes_DVBT_se_Halmstad_Oskarstrom) / sizeof(struct mux),
	},
	{
		.name = "Harnosand Harnon",
		.muxes = muxes_DVBT_se_Harnosand_Harnon,
		.nmuxes = sizeof(muxes_DVBT_se_Harnosand_Harnon) / sizeof(struct mux),
	},
	{
		.name = "Hassela",
		.muxes = muxes_DVBT_se_Hassela,
		.nmuxes = sizeof(muxes_DVBT_se_Hassela) / sizeof(struct mux),
	},
	{
		.name = "Havdhem",
		.muxes = muxes_DVBT_se_Havdhem,
		.nmuxes = sizeof(muxes_DVBT_se_Havdhem) / sizeof(struct mux),
	},
	{
		.name = "Hedemora",
		.muxes = muxes_DVBT_se_Hedemora,
		.nmuxes = sizeof(muxes_DVBT_se_Hedemora) / sizeof(struct mux),
	},
	{
		.name = "Helsingborg Olympia",
		.muxes = muxes_DVBT_se_Helsingborg_Olympia,
		.nmuxes = sizeof(muxes_DVBT_se_Helsingborg_Olympia) / sizeof(struct mux),
	},
	{
		.name = "Hennan",
		.muxes = muxes_DVBT_se_Hennan,
		.nmuxes = sizeof(muxes_DVBT_se_Hennan) / sizeof(struct mux),
	},
	{
		.name = "Hestra Aspas",
		.muxes = muxes_DVBT_se_Hestra_Aspas,
		.nmuxes = sizeof(muxes_DVBT_se_Hestra_Aspas) / sizeof(struct mux),
	},
	{
		.name = "Hjo Grevback",
		.muxes = muxes_DVBT_se_Hjo_Grevback,
		.nmuxes = sizeof(muxes_DVBT_se_Hjo_Grevback) / sizeof(struct mux),
	},
	{
		.name = "Hofors",
		.muxes = muxes_DVBT_se_Hofors,
		.nmuxes = sizeof(muxes_DVBT_se_Hofors) / sizeof(struct mux),
	},
	{
		.name = "Hogfors",
		.muxes = muxes_DVBT_se_Hogfors,
		.nmuxes = sizeof(muxes_DVBT_se_Hogfors) / sizeof(struct mux),
	},
	{
		.name = "Hogsby Virstad",
		.muxes = muxes_DVBT_se_Hogsby_Virstad,
		.nmuxes = sizeof(muxes_DVBT_se_Hogsby_Virstad) / sizeof(struct mux),
	},
	{
		.name = "Holsbybrunn Holsbyholm",
		.muxes = muxes_DVBT_se_Holsbybrunn_Holsbyholm,
		.nmuxes = sizeof(muxes_DVBT_se_Holsbybrunn_Holsbyholm) / sizeof(struct mux),
	},
	{
		.name = "Horby Sallerup",
		.muxes = muxes_DVBT_se_Horby_Sallerup,
		.nmuxes = sizeof(muxes_DVBT_se_Horby_Sallerup) / sizeof(struct mux),
	},
	{
		.name = "Horken",
		.muxes = muxes_DVBT_se_Horken,
		.nmuxes = sizeof(muxes_DVBT_se_Horken) / sizeof(struct mux),
	},
	{
		.name = "Hudiksvall Forsa",
		.muxes = muxes_DVBT_se_Hudiksvall_Forsa,
		.nmuxes = sizeof(muxes_DVBT_se_Hudiksvall_Forsa) / sizeof(struct mux),
	},
	{
		.name = "Hudiksvall Galgberget",
		.muxes = muxes_DVBT_se_Hudiksvall_Galgberget,
		.nmuxes = sizeof(muxes_DVBT_se_Hudiksvall_Galgberget) / sizeof(struct mux),
	},
	{
		.name = "Huskvarna",
		.muxes = muxes_DVBT_se_Huskvarna,
		.nmuxes = sizeof(muxes_DVBT_se_Huskvarna) / sizeof(struct mux),
	},
	{
		.name = "Idre",
		.muxes = muxes_DVBT_se_Idre,
		.nmuxes = sizeof(muxes_DVBT_se_Idre) / sizeof(struct mux),
	},
	{
		.name = "Ingatorp",
		.muxes = muxes_DVBT_se_Ingatorp,
		.nmuxes = sizeof(muxes_DVBT_se_Ingatorp) / sizeof(struct mux),
	},
	{
		.name = "Ingvallsbenning",
		.muxes = muxes_DVBT_se_Ingvallsbenning,
		.nmuxes = sizeof(muxes_DVBT_se_Ingvallsbenning) / sizeof(struct mux),
	},
	{
		.name = "Irevik",
		.muxes = muxes_DVBT_se_Irevik,
		.nmuxes = sizeof(muxes_DVBT_se_Irevik) / sizeof(struct mux),
	},
	{
		.name = "Jamjo",
		.muxes = muxes_DVBT_se_Jamjo,
		.nmuxes = sizeof(muxes_DVBT_se_Jamjo) / sizeof(struct mux),
	},
	{
		.name = "Jarnforsen",
		.muxes = muxes_DVBT_se_Jarnforsen,
		.nmuxes = sizeof(muxes_DVBT_se_Jarnforsen) / sizeof(struct mux),
	},
	{
		.name = "Jarvso",
		.muxes = muxes_DVBT_se_Jarvso,
		.nmuxes = sizeof(muxes_DVBT_se_Jarvso) / sizeof(struct mux),
	},
	{
		.name = "Jokkmokk Tjalmejaure",
		.muxes = muxes_DVBT_se_Jokkmokk_Tjalmejaure,
		.nmuxes = sizeof(muxes_DVBT_se_Jokkmokk_Tjalmejaure) / sizeof(struct mux),
	},
	{
		.name = "Jonkoping Bondberget",
		.muxes = muxes_DVBT_se_Jonkoping_Bondberget,
		.nmuxes = sizeof(muxes_DVBT_se_Jonkoping_Bondberget) / sizeof(struct mux),
	},
	{
		.name = "Kalix",
		.muxes = muxes_DVBT_se_Kalix,
		.nmuxes = sizeof(muxes_DVBT_se_Kalix) / sizeof(struct mux),
	},
	{
		.name = "Karbole",
		.muxes = muxes_DVBT_se_Karbole,
		.nmuxes = sizeof(muxes_DVBT_se_Karbole) / sizeof(struct mux),
	},
	{
		.name = "Karlsborg Vaberget",
		.muxes = muxes_DVBT_se_Karlsborg_Vaberget,
		.nmuxes = sizeof(muxes_DVBT_se_Karlsborg_Vaberget) / sizeof(struct mux),
	},
	{
		.name = "Karlshamn",
		.muxes = muxes_DVBT_se_Karlshamn,
		.nmuxes = sizeof(muxes_DVBT_se_Karlshamn) / sizeof(struct mux),
	},
	{
		.name = "Karlskrona Vamo",
		.muxes = muxes_DVBT_se_Karlskrona_Vamo,
		.nmuxes = sizeof(muxes_DVBT_se_Karlskrona_Vamo) / sizeof(struct mux),
	},
	{
		.name = "Karlstad Sormon",
		.muxes = muxes_DVBT_se_Karlstad_Sormon,
		.nmuxes = sizeof(muxes_DVBT_se_Karlstad_Sormon) / sizeof(struct mux),
	},
	{
		.name = "Kaxholmen Vistakulle",
		.muxes = muxes_DVBT_se_Kaxholmen_Vistakulle,
		.nmuxes = sizeof(muxes_DVBT_se_Kaxholmen_Vistakulle) / sizeof(struct mux),
	},
	{
		.name = "Kinnastrom",
		.muxes = muxes_DVBT_se_Kinnastrom,
		.nmuxes = sizeof(muxes_DVBT_se_Kinnastrom) / sizeof(struct mux),
	},
	{
		.name = "Kiruna Kirunavaara",
		.muxes = muxes_DVBT_se_Kiruna_Kirunavaara,
		.nmuxes = sizeof(muxes_DVBT_se_Kiruna_Kirunavaara) / sizeof(struct mux),
	},
	{
		.name = "Kisa",
		.muxes = muxes_DVBT_se_Kisa,
		.nmuxes = sizeof(muxes_DVBT_se_Kisa) / sizeof(struct mux),
	},
	{
		.name = "Knared",
		.muxes = muxes_DVBT_se_Knared,
		.nmuxes = sizeof(muxes_DVBT_se_Knared) / sizeof(struct mux),
	},
	{
		.name = "Kopmanholmen",
		.muxes = muxes_DVBT_se_Kopmanholmen,
		.nmuxes = sizeof(muxes_DVBT_se_Kopmanholmen) / sizeof(struct mux),
	},
	{
		.name = "Kopparberg",
		.muxes = muxes_DVBT_se_Kopparberg,
		.nmuxes = sizeof(muxes_DVBT_se_Kopparberg) / sizeof(struct mux),
	},
	{
		.name = "Kramfors Lugnvik",
		.muxes = muxes_DVBT_se_Kramfors_Lugnvik,
		.nmuxes = sizeof(muxes_DVBT_se_Kramfors_Lugnvik) / sizeof(struct mux),
	},
	{
		.name = "Kristinehamn Utsiktsberget",
		.muxes = muxes_DVBT_se_Kristinehamn_Utsiktsberget,
		.nmuxes = sizeof(muxes_DVBT_se_Kristinehamn_Utsiktsberget) / sizeof(struct mux),
	},
	{
		.name = "Kungsater",
		.muxes = muxes_DVBT_se_Kungsater,
		.nmuxes = sizeof(muxes_DVBT_se_Kungsater) / sizeof(struct mux),
	},
	{
		.name = "Kungsberget GI",
		.muxes = muxes_DVBT_se_Kungsberget_GI,
		.nmuxes = sizeof(muxes_DVBT_se_Kungsberget_GI) / sizeof(struct mux),
	},
	{
		.name = "Langshyttan",
		.muxes = muxes_DVBT_se_Langshyttan,
		.nmuxes = sizeof(muxes_DVBT_se_Langshyttan) / sizeof(struct mux),
	},
	{
		.name = "Langshyttan Engelsfors",
		.muxes = muxes_DVBT_se_Langshyttan_Engelsfors,
		.nmuxes = sizeof(muxes_DVBT_se_Langshyttan_Engelsfors) / sizeof(struct mux),
	},
	{
		.name = "Leksand Karingberget",
		.muxes = muxes_DVBT_se_Leksand_Karingberget,
		.nmuxes = sizeof(muxes_DVBT_se_Leksand_Karingberget) / sizeof(struct mux),
	},
	{
		.name = "Lerdala",
		.muxes = muxes_DVBT_se_Lerdala,
		.nmuxes = sizeof(muxes_DVBT_se_Lerdala) / sizeof(struct mux),
	},
	{
		.name = "Lilltjara Digerberget",
		.muxes = muxes_DVBT_se_Lilltjara_Digerberget,
		.nmuxes = sizeof(muxes_DVBT_se_Lilltjara_Digerberget) / sizeof(struct mux),
	},
	{
		.name = "Limedsforsen",
		.muxes = muxes_DVBT_se_Limedsforsen,
		.nmuxes = sizeof(muxes_DVBT_se_Limedsforsen) / sizeof(struct mux),
	},
	{
		.name = "Lindshammar Ramkvilla",
		.muxes = muxes_DVBT_se_Lindshammar_Ramkvilla,
		.nmuxes = sizeof(muxes_DVBT_se_Lindshammar_Ramkvilla) / sizeof(struct mux),
	},
	{
		.name = "Linkoping Vattentornet",
		.muxes = muxes_DVBT_se_Linkoping_Vattentornet,
		.nmuxes = sizeof(muxes_DVBT_se_Linkoping_Vattentornet) / sizeof(struct mux),
	},
	{
		.name = "Ljugarn",
		.muxes = muxes_DVBT_se_Ljugarn,
		.nmuxes = sizeof(muxes_DVBT_se_Ljugarn) / sizeof(struct mux),
	},
	{
		.name = "Loffstrand",
		.muxes = muxes_DVBT_se_Loffstrand,
		.nmuxes = sizeof(muxes_DVBT_se_Loffstrand) / sizeof(struct mux),
	},
	{
		.name = "Lonneberga",
		.muxes = muxes_DVBT_se_Lonneberga,
		.nmuxes = sizeof(muxes_DVBT_se_Lonneberga) / sizeof(struct mux),
	},
	{
		.name = "Lorstrand",
		.muxes = muxes_DVBT_se_Lorstrand,
		.nmuxes = sizeof(muxes_DVBT_se_Lorstrand) / sizeof(struct mux),
	},
	{
		.name = "Ludvika Bjorkasen",
		.muxes = muxes_DVBT_se_Ludvika_Bjorkasen,
		.nmuxes = sizeof(muxes_DVBT_se_Ludvika_Bjorkasen) / sizeof(struct mux),
	},
	{
		.name = "Lumsheden Trekanten",
		.muxes = muxes_DVBT_se_Lumsheden_Trekanten,
		.nmuxes = sizeof(muxes_DVBT_se_Lumsheden_Trekanten) / sizeof(struct mux),
	},
	{
		.name = "Lycksele Knaften",
		.muxes = muxes_DVBT_se_Lycksele_Knaften,
		.nmuxes = sizeof(muxes_DVBT_se_Lycksele_Knaften) / sizeof(struct mux),
	},
	{
		.name = "Mahult",
		.muxes = muxes_DVBT_se_Mahult,
		.nmuxes = sizeof(muxes_DVBT_se_Mahult) / sizeof(struct mux),
	},
	{
		.name = "Malmo Jagersro",
		.muxes = muxes_DVBT_se_Malmo_Jagersro,
		.nmuxes = sizeof(muxes_DVBT_se_Malmo_Jagersro) / sizeof(struct mux),
	},
	{
		.name = "Malung",
		.muxes = muxes_DVBT_se_Malung,
		.nmuxes = sizeof(muxes_DVBT_se_Malung) / sizeof(struct mux),
	},
	{
		.name = "Mariannelund",
		.muxes = muxes_DVBT_se_Mariannelund,
		.nmuxes = sizeof(muxes_DVBT_se_Mariannelund) / sizeof(struct mux),
	},
	{
		.name = "Markaryd Hualtet",
		.muxes = muxes_DVBT_se_Markaryd_Hualtet,
		.nmuxes = sizeof(muxes_DVBT_se_Markaryd_Hualtet) / sizeof(struct mux),
	},
	{
		.name = "Matfors",
		.muxes = muxes_DVBT_se_Matfors,
		.nmuxes = sizeof(muxes_DVBT_se_Matfors) / sizeof(struct mux),
	},
	{
		.name = "Molndal Vasterberget",
		.muxes = muxes_DVBT_se_Molndal_Vasterberget,
		.nmuxes = sizeof(muxes_DVBT_se_Molndal_Vasterberget) / sizeof(struct mux),
	},
	{
		.name = "Mora Eldris",
		.muxes = muxes_DVBT_se_Mora_Eldris,
		.nmuxes = sizeof(muxes_DVBT_se_Mora_Eldris) / sizeof(struct mux),
	},
	{
		.name = "Motala Ervasteby",
		.muxes = muxes_DVBT_se_Motala_Ervasteby,
		.nmuxes = sizeof(muxes_DVBT_se_Motala_Ervasteby) / sizeof(struct mux),
	},
	{
		.name = "Mullsjo Torestorp",
		.muxes = muxes_DVBT_se_Mullsjo_Torestorp,
		.nmuxes = sizeof(muxes_DVBT_se_Mullsjo_Torestorp) / sizeof(struct mux),
	},
	{
		.name = "Nassjo",
		.muxes = muxes_DVBT_se_Nassjo,
		.nmuxes = sizeof(muxes_DVBT_se_Nassjo) / sizeof(struct mux),
	},
	{
		.name = "Navekvarn",
		.muxes = muxes_DVBT_se_Navekvarn,
		.nmuxes = sizeof(muxes_DVBT_se_Navekvarn) / sizeof(struct mux),
	},
	{
		.name = "Norrahammar",
		.muxes = muxes_DVBT_se_Norrahammar,
		.nmuxes = sizeof(muxes_DVBT_se_Norrahammar) / sizeof(struct mux),
	},
	{
		.name = "Norrkoping Krokek",
		.muxes = muxes_DVBT_se_Norrkoping_Krokek,
		.nmuxes = sizeof(muxes_DVBT_se_Norrkoping_Krokek) / sizeof(struct mux),
	},
	{
		.name = "Norrtalje Sodra Bergen",
		.muxes = muxes_DVBT_se_Norrtalje_Sodra_Bergen,
		.nmuxes = sizeof(muxes_DVBT_se_Norrtalje_Sodra_Bergen) / sizeof(struct mux),
	},
	{
		.name = "Nykoping",
		.muxes = muxes_DVBT_se_Nykoping,
		.nmuxes = sizeof(muxes_DVBT_se_Nykoping) / sizeof(struct mux),
	},
	{
		.name = "Orebro Lockhyttan",
		.muxes = muxes_DVBT_se_Orebro_Lockhyttan,
		.nmuxes = sizeof(muxes_DVBT_se_Orebro_Lockhyttan) / sizeof(struct mux),
	},
	{
		.name = "Ornskoldsvik As",
		.muxes = muxes_DVBT_se_Ornskoldsvik_As,
		.nmuxes = sizeof(muxes_DVBT_se_Ornskoldsvik_As) / sizeof(struct mux),
	},
	{
		.name = "Oskarshamn",
		.muxes = muxes_DVBT_se_Oskarshamn,
		.nmuxes = sizeof(muxes_DVBT_se_Oskarshamn) / sizeof(struct mux),
	},
	{
		.name = "Ostersund Brattasen",
		.muxes = muxes_DVBT_se_Ostersund_Brattasen,
		.nmuxes = sizeof(muxes_DVBT_se_Ostersund_Brattasen) / sizeof(struct mux),
	},
	{
		.name = "Osthammar Valo",
		.muxes = muxes_DVBT_se_Osthammar_Valo,
		.nmuxes = sizeof(muxes_DVBT_se_Osthammar_Valo) / sizeof(struct mux),
	},
	{
		.name = "Overkalix",
		.muxes = muxes_DVBT_se_Overkalix,
		.nmuxes = sizeof(muxes_DVBT_se_Overkalix) / sizeof(struct mux),
	},
	{
		.name = "Oxberg",
		.muxes = muxes_DVBT_se_Oxberg,
		.nmuxes = sizeof(muxes_DVBT_se_Oxberg) / sizeof(struct mux),
	},
	{
		.name = "Pajala",
		.muxes = muxes_DVBT_se_Pajala,
		.nmuxes = sizeof(muxes_DVBT_se_Pajala) / sizeof(struct mux),
	},
	{
		.name = "Paulistom",
		.muxes = muxes_DVBT_se_Paulistom,
		.nmuxes = sizeof(muxes_DVBT_se_Paulistom) / sizeof(struct mux),
	},
	{
		.name = "Rattvik",
		.muxes = muxes_DVBT_se_Rattvik,
		.nmuxes = sizeof(muxes_DVBT_se_Rattvik) / sizeof(struct mux),
	},
	{
		.name = "Rengsjo",
		.muxes = muxes_DVBT_se_Rengsjo,
		.nmuxes = sizeof(muxes_DVBT_se_Rengsjo) / sizeof(struct mux),
	},
	{
		.name = "Rorbacksnas",
		.muxes = muxes_DVBT_se_Rorbacksnas,
		.nmuxes = sizeof(muxes_DVBT_se_Rorbacksnas) / sizeof(struct mux),
	},
	{
		.name = "Sagmyra",
		.muxes = muxes_DVBT_se_Sagmyra,
		.nmuxes = sizeof(muxes_DVBT_se_Sagmyra) / sizeof(struct mux),
	},
	{
		.name = "Salen",
		.muxes = muxes_DVBT_se_Salen,
		.nmuxes = sizeof(muxes_DVBT_se_Salen) / sizeof(struct mux),
	},
	{
		.name = "Salfjallet",
		.muxes = muxes_DVBT_se_Salfjallet,
		.nmuxes = sizeof(muxes_DVBT_se_Salfjallet) / sizeof(struct mux),
	},
	{
		.name = "Sarna Mickeltemplet",
		.muxes = muxes_DVBT_se_Sarna_Mickeltemplet,
		.nmuxes = sizeof(muxes_DVBT_se_Sarna_Mickeltemplet) / sizeof(struct mux),
	},
	{
		.name = "Satila",
		.muxes = muxes_DVBT_se_Satila,
		.nmuxes = sizeof(muxes_DVBT_se_Satila) / sizeof(struct mux),
	},
	{
		.name = "Saxdalen",
		.muxes = muxes_DVBT_se_Saxdalen,
		.nmuxes = sizeof(muxes_DVBT_se_Saxdalen) / sizeof(struct mux),
	},
	{
		.name = "Siljansnas Uvberget",
		.muxes = muxes_DVBT_se_Siljansnas_Uvberget,
		.nmuxes = sizeof(muxes_DVBT_se_Siljansnas_Uvberget) / sizeof(struct mux),
	},
	{
		.name = "Skarstad",
		.muxes = muxes_DVBT_se_Skarstad,
		.nmuxes = sizeof(muxes_DVBT_se_Skarstad) / sizeof(struct mux),
	},
	{
		.name = "Skattungbyn",
		.muxes = muxes_DVBT_se_Skattungbyn,
		.nmuxes = sizeof(muxes_DVBT_se_Skattungbyn) / sizeof(struct mux),
	},
	{
		.name = "Skelleftea",
		.muxes = muxes_DVBT_se_Skelleftea,
		.nmuxes = sizeof(muxes_DVBT_se_Skelleftea) / sizeof(struct mux),
	},
	{
		.name = "Skene Nycklarberget",
		.muxes = muxes_DVBT_se_Skene_Nycklarberget,
		.nmuxes = sizeof(muxes_DVBT_se_Skene_Nycklarberget) / sizeof(struct mux),
	},
	{
		.name = "Skovde",
		.muxes = muxes_DVBT_se_Skovde,
		.nmuxes = sizeof(muxes_DVBT_se_Skovde) / sizeof(struct mux),
	},
	{
		.name = "Smedjebacken Uvberget",
		.muxes = muxes_DVBT_se_Smedjebacken_Uvberget,
		.nmuxes = sizeof(muxes_DVBT_se_Smedjebacken_Uvberget) / sizeof(struct mux),
	},
	{
		.name = "Soderhamn",
		.muxes = muxes_DVBT_se_Soderhamn,
		.nmuxes = sizeof(muxes_DVBT_se_Soderhamn) / sizeof(struct mux),
	},
	{
		.name = "Soderkoping",
		.muxes = muxes_DVBT_se_Soderkoping,
		.nmuxes = sizeof(muxes_DVBT_se_Soderkoping) / sizeof(struct mux),
	},
	{
		.name = "Sodertalje Ragnhildsborg",
		.muxes = muxes_DVBT_se_Sodertalje_Ragnhildsborg,
		.nmuxes = sizeof(muxes_DVBT_se_Sodertalje_Ragnhildsborg) / sizeof(struct mux),
	},
	{
		.name = "Solleftea Hallsta",
		.muxes = muxes_DVBT_se_Solleftea_Hallsta,
		.nmuxes = sizeof(muxes_DVBT_se_Solleftea_Hallsta) / sizeof(struct mux),
	},
	{
		.name = "Solleftea Multra",
		.muxes = muxes_DVBT_se_Solleftea_Multra,
		.nmuxes = sizeof(muxes_DVBT_se_Solleftea_Multra) / sizeof(struct mux),
	},
	{
		.name = "Sorsjon",
		.muxes = muxes_DVBT_se_Sorsjon,
		.nmuxes = sizeof(muxes_DVBT_se_Sorsjon) / sizeof(struct mux),
	},
	{
		.name = "Stockholm Marieberg",
		.muxes = muxes_DVBT_se_Stockholm_Marieberg,
		.nmuxes = sizeof(muxes_DVBT_se_Stockholm_Marieberg) / sizeof(struct mux),
	},
	{
		.name = "Stockholm Nacka",
		.muxes = muxes_DVBT_se_Stockholm_Nacka,
		.nmuxes = sizeof(muxes_DVBT_se_Stockholm_Nacka) / sizeof(struct mux),
	},
	{
		.name = "Stora Skedvi",
		.muxes = muxes_DVBT_se_Stora_Skedvi,
		.nmuxes = sizeof(muxes_DVBT_se_Stora_Skedvi) / sizeof(struct mux),
	},
	{
		.name = "Storfjaten",
		.muxes = muxes_DVBT_se_Storfjaten,
		.nmuxes = sizeof(muxes_DVBT_se_Storfjaten) / sizeof(struct mux),
	},
	{
		.name = "Storuman",
		.muxes = muxes_DVBT_se_Storuman,
		.nmuxes = sizeof(muxes_DVBT_se_Storuman) / sizeof(struct mux),
	},
	{
		.name = "Stromstad",
		.muxes = muxes_DVBT_se_Stromstad,
		.nmuxes = sizeof(muxes_DVBT_se_Stromstad) / sizeof(struct mux),
	},
	{
		.name = "Styrsjobo",
		.muxes = muxes_DVBT_se_Styrsjobo,
		.nmuxes = sizeof(muxes_DVBT_se_Styrsjobo) / sizeof(struct mux),
	},
	{
		.name = "Sundborn",
		.muxes = muxes_DVBT_se_Sundborn,
		.nmuxes = sizeof(muxes_DVBT_se_Sundborn) / sizeof(struct mux),
	},
	{
		.name = "Sundsbruk",
		.muxes = muxes_DVBT_se_Sundsbruk,
		.nmuxes = sizeof(muxes_DVBT_se_Sundsbruk) / sizeof(struct mux),
	},
	{
		.name = "Sundsvall S Stadsberget",
		.muxes = muxes_DVBT_se_Sundsvall_S_Stadsberget,
		.nmuxes = sizeof(muxes_DVBT_se_Sundsvall_S_Stadsberget) / sizeof(struct mux),
	},
	{
		.name = "Sunne Blabarskullen",
		.muxes = muxes_DVBT_se_Sunne_Blabarskullen,
		.nmuxes = sizeof(muxes_DVBT_se_Sunne_Blabarskullen) / sizeof(struct mux),
	},
	{
		.name = "Svartnas",
		.muxes = muxes_DVBT_se_Svartnas,
		.nmuxes = sizeof(muxes_DVBT_se_Svartnas) / sizeof(struct mux),
	},
	{
		.name = "Sveg Brickan",
		.muxes = muxes_DVBT_se_Sveg_Brickan,
		.nmuxes = sizeof(muxes_DVBT_se_Sveg_Brickan) / sizeof(struct mux),
	},
	{
		.name = "Taberg",
		.muxes = muxes_DVBT_se_Taberg,
		.nmuxes = sizeof(muxes_DVBT_se_Taberg) / sizeof(struct mux),
	},
	{
		.name = "Tandadalen",
		.muxes = muxes_DVBT_se_Tandadalen,
		.nmuxes = sizeof(muxes_DVBT_se_Tandadalen) / sizeof(struct mux),
	},
	{
		.name = "Tasjo",
		.muxes = muxes_DVBT_se_Tasjo,
		.nmuxes = sizeof(muxes_DVBT_se_Tasjo) / sizeof(struct mux),
	},
	{
		.name = "Tollsjo",
		.muxes = muxes_DVBT_se_Tollsjo,
		.nmuxes = sizeof(muxes_DVBT_se_Tollsjo) / sizeof(struct mux),
	},
	{
		.name = "Torsby Bada",
		.muxes = muxes_DVBT_se_Torsby_Bada,
		.nmuxes = sizeof(muxes_DVBT_se_Torsby_Bada) / sizeof(struct mux),
	},
	{
		.name = "Tranas Bredkarr",
		.muxes = muxes_DVBT_se_Tranas_Bredkarr,
		.nmuxes = sizeof(muxes_DVBT_se_Tranas_Bredkarr) / sizeof(struct mux),
	},
	{
		.name = "Tranemo",
		.muxes = muxes_DVBT_se_Tranemo,
		.nmuxes = sizeof(muxes_DVBT_se_Tranemo) / sizeof(struct mux),
	},
	{
		.name = "Transtrand Bolheden",
		.muxes = muxes_DVBT_se_Transtrand_Bolheden,
		.nmuxes = sizeof(muxes_DVBT_se_Transtrand_Bolheden) / sizeof(struct mux),
	},
	{
		.name = "Traryd Betas",
		.muxes = muxes_DVBT_se_Traryd_Betas,
		.nmuxes = sizeof(muxes_DVBT_se_Traryd_Betas) / sizeof(struct mux),
	},
	{
		.name = "Trollhattan",
		.muxes = muxes_DVBT_se_Trollhattan,
		.nmuxes = sizeof(muxes_DVBT_se_Trollhattan) / sizeof(struct mux),
	},
	{
		.name = "Trosa",
		.muxes = muxes_DVBT_se_Trosa,
		.nmuxes = sizeof(muxes_DVBT_se_Trosa) / sizeof(struct mux),
	},
	{
		.name = "Tystberga",
		.muxes = muxes_DVBT_se_Tystberga,
		.nmuxes = sizeof(muxes_DVBT_se_Tystberga) / sizeof(struct mux),
	},
	{
		.name = "Uddevalla Herrestad",
		.muxes = muxes_DVBT_se_Uddevalla_Herrestad,
		.nmuxes = sizeof(muxes_DVBT_se_Uddevalla_Herrestad) / sizeof(struct mux),
	},
	{
		.name = "Ullared",
		.muxes = muxes_DVBT_se_Ullared,
		.nmuxes = sizeof(muxes_DVBT_se_Ullared) / sizeof(struct mux),
	},
	{
		.name = "Ulricehamn",
		.muxes = muxes_DVBT_se_Ulricehamn,
		.nmuxes = sizeof(muxes_DVBT_se_Ulricehamn) / sizeof(struct mux),
	},
	{
		.name = "Ulvshyttan Porjus",
		.muxes = muxes_DVBT_se_Ulvshyttan_Porjus,
		.nmuxes = sizeof(muxes_DVBT_se_Ulvshyttan_Porjus) / sizeof(struct mux),
	},
	{
		.name = "Uppsala Rickomberga",
		.muxes = muxes_DVBT_se_Uppsala_Rickomberga,
		.nmuxes = sizeof(muxes_DVBT_se_Uppsala_Rickomberga) / sizeof(struct mux),
	},
	{
		.name = "Uppsala Vedyxa",
		.muxes = muxes_DVBT_se_Uppsala_Vedyxa,
		.nmuxes = sizeof(muxes_DVBT_se_Uppsala_Vedyxa) / sizeof(struct mux),
	},
	{
		.name = "Vaddo Elmsta",
		.muxes = muxes_DVBT_se_Vaddo_Elmsta,
		.nmuxes = sizeof(muxes_DVBT_se_Vaddo_Elmsta) / sizeof(struct mux),
	},
	{
		.name = "Valdemarsvik",
		.muxes = muxes_DVBT_se_Valdemarsvik,
		.nmuxes = sizeof(muxes_DVBT_se_Valdemarsvik) / sizeof(struct mux),
	},
	{
		.name = "Vannas Granlundsberget",
		.muxes = muxes_DVBT_se_Vannas_Granlundsberget,
		.nmuxes = sizeof(muxes_DVBT_se_Vannas_Granlundsberget) / sizeof(struct mux),
	},
	{
		.name = "Vansbro Hummelberget",
		.muxes = muxes_DVBT_se_Vansbro_Hummelberget,
		.nmuxes = sizeof(muxes_DVBT_se_Vansbro_Hummelberget) / sizeof(struct mux),
	},
	{
		.name = "Varberg Grimeton",
		.muxes = muxes_DVBT_se_Varberg_Grimeton,
		.nmuxes = sizeof(muxes_DVBT_se_Varberg_Grimeton) / sizeof(struct mux),
	},
	{
		.name = "Vasteras Lillharad",
		.muxes = muxes_DVBT_se_Vasteras_Lillharad,
		.nmuxes = sizeof(muxes_DVBT_se_Vasteras_Lillharad) / sizeof(struct mux),
	},
	{
		.name = "Vastervik Farhult",
		.muxes = muxes_DVBT_se_Vastervik_Farhult,
		.nmuxes = sizeof(muxes_DVBT_se_Vastervik_Farhult) / sizeof(struct mux),
	},
	{
		.name = "Vaxbo",
		.muxes = muxes_DVBT_se_Vaxbo,
		.nmuxes = sizeof(muxes_DVBT_se_Vaxbo) / sizeof(struct mux),
	},
	{
		.name = "Vessigebro",
		.muxes = muxes_DVBT_se_Vessigebro,
		.nmuxes = sizeof(muxes_DVBT_se_Vessigebro) / sizeof(struct mux),
	},
	{
		.name = "Vetlanda Nye",
		.muxes = muxes_DVBT_se_Vetlanda_Nye,
		.nmuxes = sizeof(muxes_DVBT_se_Vetlanda_Nye) / sizeof(struct mux),
	},
	{
		.name = "Vikmanshyttan",
		.muxes = muxes_DVBT_se_Vikmanshyttan,
		.nmuxes = sizeof(muxes_DVBT_se_Vikmanshyttan) / sizeof(struct mux),
	},
	{
		.name = "Virserum",
		.muxes = muxes_DVBT_se_Virserum,
		.nmuxes = sizeof(muxes_DVBT_se_Virserum) / sizeof(struct mux),
	},
	{
		.name = "Visby Follingbo",
		.muxes = muxes_DVBT_se_Visby_Follingbo,
		.nmuxes = sizeof(muxes_DVBT_se_Visby_Follingbo) / sizeof(struct mux),
	},
	{
		.name = "Visby Hamnen",
		.muxes = muxes_DVBT_se_Visby_Hamnen,
		.nmuxes = sizeof(muxes_DVBT_se_Visby_Hamnen) / sizeof(struct mux),
	},
	{
		.name = "Visingso",
		.muxes = muxes_DVBT_se_Visingso,
		.nmuxes = sizeof(muxes_DVBT_se_Visingso) / sizeof(struct mux),
	},
	{
		.name = "Vislanda Nydala",
		.muxes = muxes_DVBT_se_Vislanda_Nydala,
		.nmuxes = sizeof(muxes_DVBT_se_Vislanda_Nydala) / sizeof(struct mux),
	},
	{
		.name = "Voxna",
		.muxes = muxes_DVBT_se_Voxna,
		.nmuxes = sizeof(muxes_DVBT_se_Voxna) / sizeof(struct mux),
	},
	{
		.name = "Ystad Metallgatan",
		.muxes = muxes_DVBT_se_Ystad_Metallgatan,
		.nmuxes = sizeof(muxes_DVBT_se_Ystad_Metallgatan) / sizeof(struct mux),
	},
	{
		.name = "Yttermalung",
		.muxes = muxes_DVBT_se_Yttermalung,
		.nmuxes = sizeof(muxes_DVBT_se_Yttermalung) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_ch[] = {
	{
		.name = "All",
		.muxes = muxes_DVBT_ch_All,
		.nmuxes = sizeof(muxes_DVBT_ch_All) / sizeof(struct mux),
	},
	{
		.name = "Citycable",
		.muxes = muxes_DVBT_ch_Citycable,
		.nmuxes = sizeof(muxes_DVBT_ch_Citycable) / sizeof(struct mux),
	},
	{
		.name = "Geneva",
		.muxes = muxes_DVBT_ch_Geneva,
		.nmuxes = sizeof(muxes_DVBT_ch_Geneva) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_tw[] = {
	{
		.name = "Kaohsiung",
		.muxes = muxes_DVBT_tw_Kaohsiung,
		.nmuxes = sizeof(muxes_DVBT_tw_Kaohsiung) / sizeof(struct mux),
	},
	{
		.name = "Taipei",
		.muxes = muxes_DVBT_tw_Taipei,
		.nmuxes = sizeof(muxes_DVBT_tw_Taipei) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_uk[] = {
	{
		.name = "Aberdare",
		.muxes = muxes_DVBT_uk_Aberdare,
		.nmuxes = sizeof(muxes_DVBT_uk_Aberdare) / sizeof(struct mux),
	},
	{
		.name = "Angus",
		.muxes = muxes_DVBT_uk_Angus,
		.nmuxes = sizeof(muxes_DVBT_uk_Angus) / sizeof(struct mux),
	},
	{
		.name = "BeaconHill",
		.muxes = muxes_DVBT_uk_BeaconHill,
		.nmuxes = sizeof(muxes_DVBT_uk_BeaconHill) / sizeof(struct mux),
	},
	{
		.name = "Belmont",
		.muxes = muxes_DVBT_uk_Belmont,
		.nmuxes = sizeof(muxes_DVBT_uk_Belmont) / sizeof(struct mux),
	},
	{
		.name = "Bilsdale",
		.muxes = muxes_DVBT_uk_Bilsdale,
		.nmuxes = sizeof(muxes_DVBT_uk_Bilsdale) / sizeof(struct mux),
	},
	{
		.name = "BlackHill",
		.muxes = muxes_DVBT_uk_BlackHill,
		.nmuxes = sizeof(muxes_DVBT_uk_BlackHill) / sizeof(struct mux),
	},
	{
		.name = "Blaenplwyf",
		.muxes = muxes_DVBT_uk_Blaenplwyf,
		.nmuxes = sizeof(muxes_DVBT_uk_Blaenplwyf) / sizeof(struct mux),
	},
	{
		.name = "BluebellHill",
		.muxes = muxes_DVBT_uk_BluebellHill,
		.nmuxes = sizeof(muxes_DVBT_uk_BluebellHill) / sizeof(struct mux),
	},
	{
		.name = "Bressay",
		.muxes = muxes_DVBT_uk_Bressay,
		.nmuxes = sizeof(muxes_DVBT_uk_Bressay) / sizeof(struct mux),
	},
	{
		.name = "BrierleyHill",
		.muxes = muxes_DVBT_uk_BrierleyHill,
		.nmuxes = sizeof(muxes_DVBT_uk_BrierleyHill) / sizeof(struct mux),
	},
	{
		.name = "BristolIlchesterCres",
		.muxes = muxes_DVBT_uk_BristolIlchesterCres,
		.nmuxes = sizeof(muxes_DVBT_uk_BristolIlchesterCres) / sizeof(struct mux),
	},
	{
		.name = "BristolKingsWeston",
		.muxes = muxes_DVBT_uk_BristolKingsWeston,
		.nmuxes = sizeof(muxes_DVBT_uk_BristolKingsWeston) / sizeof(struct mux),
	},
	{
		.name = "Bromsgrove",
		.muxes = muxes_DVBT_uk_Bromsgrove,
		.nmuxes = sizeof(muxes_DVBT_uk_Bromsgrove) / sizeof(struct mux),
	},
	{
		.name = "BrougherMountain",
		.muxes = muxes_DVBT_uk_BrougherMountain,
		.nmuxes = sizeof(muxes_DVBT_uk_BrougherMountain) / sizeof(struct mux),
	},
	{
		.name = "Caldbeck",
		.muxes = muxes_DVBT_uk_Caldbeck,
		.nmuxes = sizeof(muxes_DVBT_uk_Caldbeck) / sizeof(struct mux),
	},
	{
		.name = "CaradonHill",
		.muxes = muxes_DVBT_uk_CaradonHill,
		.nmuxes = sizeof(muxes_DVBT_uk_CaradonHill) / sizeof(struct mux),
	},
	{
		.name = "Carmel",
		.muxes = muxes_DVBT_uk_Carmel,
		.nmuxes = sizeof(muxes_DVBT_uk_Carmel) / sizeof(struct mux),
	},
	{
		.name = "Chatton",
		.muxes = muxes_DVBT_uk_Chatton,
		.nmuxes = sizeof(muxes_DVBT_uk_Chatton) / sizeof(struct mux),
	},
	{
		.name = "Chesterfield",
		.muxes = muxes_DVBT_uk_Chesterfield,
		.nmuxes = sizeof(muxes_DVBT_uk_Chesterfield) / sizeof(struct mux),
	},
	{
		.name = "Craigkelly",
		.muxes = muxes_DVBT_uk_Craigkelly,
		.nmuxes = sizeof(muxes_DVBT_uk_Craigkelly) / sizeof(struct mux),
	},
	{
		.name = "CrystalPalace",
		.muxes = muxes_DVBT_uk_CrystalPalace,
		.nmuxes = sizeof(muxes_DVBT_uk_CrystalPalace) / sizeof(struct mux),
	},
	{
		.name = "Darvel",
		.muxes = muxes_DVBT_uk_Darvel,
		.nmuxes = sizeof(muxes_DVBT_uk_Darvel) / sizeof(struct mux),
	},
	{
		.name = "Divis",
		.muxes = muxes_DVBT_uk_Divis,
		.nmuxes = sizeof(muxes_DVBT_uk_Divis) / sizeof(struct mux),
	},
	{
		.name = "Dover",
		.muxes = muxes_DVBT_uk_Dover,
		.nmuxes = sizeof(muxes_DVBT_uk_Dover) / sizeof(struct mux),
	},
	{
		.name = "Durris",
		.muxes = muxes_DVBT_uk_Durris,
		.nmuxes = sizeof(muxes_DVBT_uk_Durris) / sizeof(struct mux),
	},
	{
		.name = "Eitshal",
		.muxes = muxes_DVBT_uk_Eitshal,
		.nmuxes = sizeof(muxes_DVBT_uk_Eitshal) / sizeof(struct mux),
	},
	{
		.name = "EmleyMoor",
		.muxes = muxes_DVBT_uk_EmleyMoor,
		.nmuxes = sizeof(muxes_DVBT_uk_EmleyMoor) / sizeof(struct mux),
	},
	{
		.name = "Fenham",
		.muxes = muxes_DVBT_uk_Fenham,
		.nmuxes = sizeof(muxes_DVBT_uk_Fenham) / sizeof(struct mux),
	},
	{
		.name = "Fenton",
		.muxes = muxes_DVBT_uk_Fenton,
		.nmuxes = sizeof(muxes_DVBT_uk_Fenton) / sizeof(struct mux),
	},
	{
		.name = "Ferryside",
		.muxes = muxes_DVBT_uk_Ferryside,
		.nmuxes = sizeof(muxes_DVBT_uk_Ferryside) / sizeof(struct mux),
	},
	{
		.name = "Guildford",
		.muxes = muxes_DVBT_uk_Guildford,
		.nmuxes = sizeof(muxes_DVBT_uk_Guildford) / sizeof(struct mux),
	},
	{
		.name = "Hannington",
		.muxes = muxes_DVBT_uk_Hannington,
		.nmuxes = sizeof(muxes_DVBT_uk_Hannington) / sizeof(struct mux),
	},
	{
		.name = "Hastings",
		.muxes = muxes_DVBT_uk_Hastings,
		.nmuxes = sizeof(muxes_DVBT_uk_Hastings) / sizeof(struct mux),
	},
	{
		.name = "Heathfield",
		.muxes = muxes_DVBT_uk_Heathfield,
		.nmuxes = sizeof(muxes_DVBT_uk_Heathfield) / sizeof(struct mux),
	},
	{
		.name = "HemelHempstead",
		.muxes = muxes_DVBT_uk_HemelHempstead,
		.nmuxes = sizeof(muxes_DVBT_uk_HemelHempstead) / sizeof(struct mux),
	},
	{
		.name = "HuntshawCross",
		.muxes = muxes_DVBT_uk_HuntshawCross,
		.nmuxes = sizeof(muxes_DVBT_uk_HuntshawCross) / sizeof(struct mux),
	},
	{
		.name = "Idle",
		.muxes = muxes_DVBT_uk_Idle,
		.nmuxes = sizeof(muxes_DVBT_uk_Idle) / sizeof(struct mux),
	},
	{
		.name = "KeelylangHill",
		.muxes = muxes_DVBT_uk_KeelylangHill,
		.nmuxes = sizeof(muxes_DVBT_uk_KeelylangHill) / sizeof(struct mux),
	},
	{
		.name = "Keighley",
		.muxes = muxes_DVBT_uk_Keighley,
		.nmuxes = sizeof(muxes_DVBT_uk_Keighley) / sizeof(struct mux),
	},
	{
		.name = "KilveyHill",
		.muxes = muxes_DVBT_uk_KilveyHill,
		.nmuxes = sizeof(muxes_DVBT_uk_KilveyHill) / sizeof(struct mux),
	},
	{
		.name = "KnockMore",
		.muxes = muxes_DVBT_uk_KnockMore,
		.nmuxes = sizeof(muxes_DVBT_uk_KnockMore) / sizeof(struct mux),
	},
	{
		.name = "Lancaster",
		.muxes = muxes_DVBT_uk_Lancaster,
		.nmuxes = sizeof(muxes_DVBT_uk_Lancaster) / sizeof(struct mux),
	},
	{
		.name = "LarkStoke",
		.muxes = muxes_DVBT_uk_LarkStoke,
		.nmuxes = sizeof(muxes_DVBT_uk_LarkStoke) / sizeof(struct mux),
	},
	{
		.name = "Limavady",
		.muxes = muxes_DVBT_uk_Limavady,
		.nmuxes = sizeof(muxes_DVBT_uk_Limavady) / sizeof(struct mux),
	},
	{
		.name = "Llanddona",
		.muxes = muxes_DVBT_uk_Llanddona,
		.nmuxes = sizeof(muxes_DVBT_uk_Llanddona) / sizeof(struct mux),
	},
	{
		.name = "Malvern",
		.muxes = muxes_DVBT_uk_Malvern,
		.nmuxes = sizeof(muxes_DVBT_uk_Malvern) / sizeof(struct mux),
	},
	{
		.name = "Mendip",
		.muxes = muxes_DVBT_uk_Mendip,
		.nmuxes = sizeof(muxes_DVBT_uk_Mendip) / sizeof(struct mux),
	},
	{
		.name = "Midhurst",
		.muxes = muxes_DVBT_uk_Midhurst,
		.nmuxes = sizeof(muxes_DVBT_uk_Midhurst) / sizeof(struct mux),
	},
	{
		.name = "Moel-y-Parc",
		.muxes = muxes_DVBT_uk_Moel_y_Parc,
		.nmuxes = sizeof(muxes_DVBT_uk_Moel_y_Parc) / sizeof(struct mux),
	},
	{
		.name = "Nottingham",
		.muxes = muxes_DVBT_uk_Nottingham,
		.nmuxes = sizeof(muxes_DVBT_uk_Nottingham) / sizeof(struct mux),
	},
	{
		.name = "OliversMount",
		.muxes = muxes_DVBT_uk_OliversMount,
		.nmuxes = sizeof(muxes_DVBT_uk_OliversMount) / sizeof(struct mux),
	},
	{
		.name = "Oxford",
		.muxes = muxes_DVBT_uk_Oxford,
		.nmuxes = sizeof(muxes_DVBT_uk_Oxford) / sizeof(struct mux),
	},
	{
		.name = "PendleForest",
		.muxes = muxes_DVBT_uk_PendleForest,
		.nmuxes = sizeof(muxes_DVBT_uk_PendleForest) / sizeof(struct mux),
	},
	{
		.name = "Plympton",
		.muxes = muxes_DVBT_uk_Plympton,
		.nmuxes = sizeof(muxes_DVBT_uk_Plympton) / sizeof(struct mux),
	},
	{
		.name = "PontopPike",
		.muxes = muxes_DVBT_uk_PontopPike,
		.nmuxes = sizeof(muxes_DVBT_uk_PontopPike) / sizeof(struct mux),
	},
	{
		.name = "Pontypool",
		.muxes = muxes_DVBT_uk_Pontypool,
		.nmuxes = sizeof(muxes_DVBT_uk_Pontypool) / sizeof(struct mux),
	},
	{
		.name = "Presely",
		.muxes = muxes_DVBT_uk_Presely,
		.nmuxes = sizeof(muxes_DVBT_uk_Presely) / sizeof(struct mux),
	},
	{
		.name = "Redruth",
		.muxes = muxes_DVBT_uk_Redruth,
		.nmuxes = sizeof(muxes_DVBT_uk_Redruth) / sizeof(struct mux),
	},
	{
		.name = "Reigate",
		.muxes = muxes_DVBT_uk_Reigate,
		.nmuxes = sizeof(muxes_DVBT_uk_Reigate) / sizeof(struct mux),
	},
	{
		.name = "RidgeHill",
		.muxes = muxes_DVBT_uk_RidgeHill,
		.nmuxes = sizeof(muxes_DVBT_uk_RidgeHill) / sizeof(struct mux),
	},
	{
		.name = "Rosemarkie",
		.muxes = muxes_DVBT_uk_Rosemarkie,
		.nmuxes = sizeof(muxes_DVBT_uk_Rosemarkie) / sizeof(struct mux),
	},
	{
		.name = "Rosneath",
		.muxes = muxes_DVBT_uk_Rosneath,
		.nmuxes = sizeof(muxes_DVBT_uk_Rosneath) / sizeof(struct mux),
	},
	{
		.name = "Rowridge",
		.muxes = muxes_DVBT_uk_Rowridge,
		.nmuxes = sizeof(muxes_DVBT_uk_Rowridge) / sizeof(struct mux),
	},
	{
		.name = "RumsterForest",
		.muxes = muxes_DVBT_uk_RumsterForest,
		.nmuxes = sizeof(muxes_DVBT_uk_RumsterForest) / sizeof(struct mux),
	},
	{
		.name = "Saddleworth",
		.muxes = muxes_DVBT_uk_Saddleworth,
		.nmuxes = sizeof(muxes_DVBT_uk_Saddleworth) / sizeof(struct mux),
	},
	{
		.name = "Salisbury",
		.muxes = muxes_DVBT_uk_Salisbury,
		.nmuxes = sizeof(muxes_DVBT_uk_Salisbury) / sizeof(struct mux),
	},
	{
		.name = "SandyHeath",
		.muxes = muxes_DVBT_uk_SandyHeath,
		.nmuxes = sizeof(muxes_DVBT_uk_SandyHeath) / sizeof(struct mux),
	},
	{
		.name = "Selkirk",
		.muxes = muxes_DVBT_uk_Selkirk,
		.nmuxes = sizeof(muxes_DVBT_uk_Selkirk) / sizeof(struct mux),
	},
	{
		.name = "Sheffield",
		.muxes = muxes_DVBT_uk_Sheffield,
		.nmuxes = sizeof(muxes_DVBT_uk_Sheffield) / sizeof(struct mux),
	},
	{
		.name = "StocklandHill",
		.muxes = muxes_DVBT_uk_StocklandHill,
		.nmuxes = sizeof(muxes_DVBT_uk_StocklandHill) / sizeof(struct mux),
	},
	{
		.name = "Storeton",
		.muxes = muxes_DVBT_uk_Storeton,
		.nmuxes = sizeof(muxes_DVBT_uk_Storeton) / sizeof(struct mux),
	},
	{
		.name = "Sudbury",
		.muxes = muxes_DVBT_uk_Sudbury,
		.nmuxes = sizeof(muxes_DVBT_uk_Sudbury) / sizeof(struct mux),
	},
	{
		.name = "SuttonColdfield",
		.muxes = muxes_DVBT_uk_SuttonColdfield,
		.nmuxes = sizeof(muxes_DVBT_uk_SuttonColdfield) / sizeof(struct mux),
	},
	{
		.name = "Tacolneston",
		.muxes = muxes_DVBT_uk_Tacolneston,
		.nmuxes = sizeof(muxes_DVBT_uk_Tacolneston) / sizeof(struct mux),
	},
	{
		.name = "TheWrekin",
		.muxes = muxes_DVBT_uk_TheWrekin,
		.nmuxes = sizeof(muxes_DVBT_uk_TheWrekin) / sizeof(struct mux),
	},
	{
		.name = "Torosay",
		.muxes = muxes_DVBT_uk_Torosay,
		.nmuxes = sizeof(muxes_DVBT_uk_Torosay) / sizeof(struct mux),
	},
	{
		.name = "TunbridgeWells",
		.muxes = muxes_DVBT_uk_TunbridgeWells,
		.nmuxes = sizeof(muxes_DVBT_uk_TunbridgeWells) / sizeof(struct mux),
	},
	{
		.name = "Waltham",
		.muxes = muxes_DVBT_uk_Waltham,
		.nmuxes = sizeof(muxes_DVBT_uk_Waltham) / sizeof(struct mux),
	},
	{
		.name = "Wenvoe",
		.muxes = muxes_DVBT_uk_Wenvoe,
		.nmuxes = sizeof(muxes_DVBT_uk_Wenvoe) / sizeof(struct mux),
	},
	{
		.name = "WhitehawkHill",
		.muxes = muxes_DVBT_uk_WhitehawkHill,
		.nmuxes = sizeof(muxes_DVBT_uk_WhitehawkHill) / sizeof(struct mux),
	},
	{
		.name = "WinterHill",
		.muxes = muxes_DVBT_uk_WinterHill,
		.nmuxes = sizeof(muxes_DVBT_uk_WinterHill) / sizeof(struct mux),
	},
};

static const struct network networks_DVBT_vn[] = {
	{
		.name = "Hanoi",
		.muxes = muxes_DVBT_vn_Hanoi,
		.nmuxes = sizeof(muxes_DVBT_vn_Hanoi) / sizeof(struct mux),
	},
	{
		.name = "Thaibinh",
		.muxes = muxes_DVBT_vn_Thaibinh,
		.nmuxes = sizeof(muxes_DVBT_vn_Thaibinh) / sizeof(struct mux),
	},
};

static const struct region regions_DVBT[] = {
	{
		.name = "--Generic--",
		.networks = networks_DVBT_auto,
		.nnetworks = sizeof(networks_DVBT_auto) / sizeof(struct network),
	},
	{
		.name = "Aland Islands",
		.networks = networks_DVBT_ax,
		.nnetworks = sizeof(networks_DVBT_ax) / sizeof(struct network),
	},
	{
		.name = "Andorra",
		.networks = networks_DVBT_ad,
		.nnetworks = sizeof(networks_DVBT_ad) / sizeof(struct network),
	},
	{
		.name = "Australia",
		.networks = networks_DVBT_au,
		.nnetworks = sizeof(networks_DVBT_au) / sizeof(struct network),
	},
	{
		.name = "Austria",
		.networks = networks_DVBT_at,
		.nnetworks = sizeof(networks_DVBT_at) / sizeof(struct network),
	},
	{
		.name = "Belgium",
		.networks = networks_DVBT_be,
		.nnetworks = sizeof(networks_DVBT_be) / sizeof(struct network),
	},
	{
		.name = "Croatia",
		.networks = networks_DVBT_hr,
		.nnetworks = sizeof(networks_DVBT_hr) / sizeof(struct network),
	},
	{
		.name = "Czech Republic",
		.networks = networks_DVBT_cz,
		.nnetworks = sizeof(networks_DVBT_cz) / sizeof(struct network),
	},
	{
		.name = "Denmark",
		.networks = networks_DVBT_dk,
		.nnetworks = sizeof(networks_DVBT_dk) / sizeof(struct network),
	},
	{
		.name = "Finland",
		.networks = networks_DVBT_fi,
		.nnetworks = sizeof(networks_DVBT_fi) / sizeof(struct network),
	},
	{
		.name = "Germany",
		.networks = networks_DVBT_de,
		.nnetworks = sizeof(networks_DVBT_de) / sizeof(struct network),
	},
	{
		.name = "Greece",
		.networks = networks_DVBT_gr,
		.nnetworks = sizeof(networks_DVBT_gr) / sizeof(struct network),
	},
	{
		.name = "Hong Kong",
		.networks = networks_DVBT_hk,
		.nnetworks = sizeof(networks_DVBT_hk) / sizeof(struct network),
	},
	{
		.name = "Hungary",
		.networks = networks_DVBT_hu,
		.nnetworks = sizeof(networks_DVBT_hu) / sizeof(struct network),
	},
	{
		.name = "Iceland",
		.networks = networks_DVBT_is,
		.nnetworks = sizeof(networks_DVBT_is) / sizeof(struct network),
	},
	{
		.name = "Iran",
		.networks = networks_DVBT_ir,
		.nnetworks = sizeof(networks_DVBT_ir) / sizeof(struct network),
	},
	{
		.name = "Israel",
		.networks = networks_DVBT_il,
		.nnetworks = sizeof(networks_DVBT_il) / sizeof(struct network),
	},
	{
		.name = "Italy",
		.networks = networks_DVBT_it,
		.nnetworks = sizeof(networks_DVBT_it) / sizeof(struct network),
	},
	{
		.name = "Latvia",
		.networks = networks_DVBT_lv,
		.nnetworks = sizeof(networks_DVBT_lv) / sizeof(struct network),
	},
	{
		.name = "Lithuania",
		.networks = networks_DVBT_lt,
		.nnetworks = sizeof(networks_DVBT_lt) / sizeof(struct network),
	},
	{
		.name = "Luxembourg",
		.networks = networks_DVBT_lu,
		.nnetworks = sizeof(networks_DVBT_lu) / sizeof(struct network),
	},
	{
		.name = "Netherlands",
		.networks = networks_DVBT_nl,
		.nnetworks = sizeof(networks_DVBT_nl) / sizeof(struct network),
	},
	{
		.name = "New Zealand",
		.networks = networks_DVBT_nz,
		.nnetworks = sizeof(networks_DVBT_nz) / sizeof(struct network),
	},
	{
		.name = "Norway",
		.networks = networks_DVBT_no,
		.nnetworks = sizeof(networks_DVBT_no) / sizeof(struct network),
	},
	{
		.name = "Poland",
		.networks = networks_DVBT_pl,
		.nnetworks = sizeof(networks_DVBT_pl) / sizeof(struct network),
	},
	{
		.name = "Romania",
		.networks = networks_DVBT_ro,
		.nnetworks = sizeof(networks_DVBT_ro) / sizeof(struct network),
	},
	{
		.name = "Slovakia",
		.networks = networks_DVBT_sk,
		.nnetworks = sizeof(networks_DVBT_sk) / sizeof(struct network),
	},
	{
		.name = "Slovenia",
		.networks = networks_DVBT_si,
		.nnetworks = sizeof(networks_DVBT_si) / sizeof(struct network),
	},
	{
		.name = "Spain",
		.networks = networks_DVBT_es,
		.nnetworks = sizeof(networks_DVBT_es) / sizeof(struct network),
	},
	{
		.name = "Sweden",
		.networks = networks_DVBT_se,
		.nnetworks = sizeof(networks_DVBT_se) / sizeof(struct network),
	},
	{
		.name = "Switzerland",
		.networks = networks_DVBT_ch,
		.nnetworks = sizeof(networks_DVBT_ch) / sizeof(struct network),
	},
	{
		.name = "Taiwan",
		.networks = networks_DVBT_tw,
		.nnetworks = sizeof(networks_DVBT_tw) / sizeof(struct network),
	},
	{
		.name = "United Kingdom",
		.networks = networks_DVBT_uk,
		.nnetworks = sizeof(networks_DVBT_uk) / sizeof(struct network),
	},
	{
		.name = "Vietnam",
		.networks = networks_DVBT_vn,
		.nnetworks = sizeof(networks_DVBT_vn) / sizeof(struct network),
	},
};

static const struct mux muxes_DVBC_at_Innsbruck[] = {
	{ .freq = 450000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 490000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 546000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 554000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 562000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_at_Kabel_Braunau[] = {
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 474000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 482000000, .symrate = 6111000, .fec = 0, .constellation = 3},
	{ .freq = 490000000, .symrate = 6111000, .fec = 0, .constellation = 3},
	{ .freq = 498000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 506000000, .symrate = 6111000, .fec = 0, .constellation = 3},
	{ .freq = 514000000, .symrate = 6111000, .fec = 0, .constellation = 3},
	{ .freq = 522000000, .symrate = 6428000, .fec = 0, .constellation = 4},
	{ .freq = 530000000, .symrate = 6428000, .fec = 0, .constellation = 4},
	{ .freq = 546000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 554000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 562000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 570000000, .symrate = 6111000, .fec = 0, .constellation = 3},
	{ .freq = 578000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 586000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 594000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_at_Kabelsignal[] = {
	{ .freq = 314000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 322000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 330000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 338000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 666000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 674000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 682000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 690000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 698000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 706000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 714000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 722000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 730000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 738000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 746000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 762000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 770000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 778000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 786000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 794000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 802000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 810000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_at_KarrerNet[] = {
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 474000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 482000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 490000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 498000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 506000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 514000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 522000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 530000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 538000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 546000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 554000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 562000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 570000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 578000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 586000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 610000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_at_Liwest[] = {
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 506000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 514000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 522000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 530000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 538000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 546000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 554000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 562000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 570000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 578000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 586000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 594000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 666000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 674000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 682000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 586000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 634000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 642000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 650000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 658000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 690000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_at_SalzburgAG[] = {
	{ .freq = 306000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_at_Vienna[] = {
	{ .freq = 377750000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_be_IN_DI_Integan[] = {
	{ .freq = 330000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 338000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 346000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 354000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 362000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 378000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 386000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 394000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 458000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 466000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 474000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 482000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 586000000, .symrate = 6875000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_br_Net_Digital[] = {
	{ .freq = 573000000, .symrate = 5217000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_ch_Rega_Sense[] = {
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 714000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 722000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 125000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 474000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 482000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 514000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 522000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 578000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 586000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 634000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 642000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 650000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 658000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 666000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 682000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 698000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 730000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 618000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 674000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 642000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 690000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_ch_unknown[] = {
	{ .freq = 530000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_ch_Video2000[] = {
	{ .freq = 306000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_ch_Zuerich_cablecom[] = {
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_cz_Moravianet[] = {
	{ .freq = 218000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 226000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 234000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 242000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 250000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 258000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 266000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 274000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 282000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 290000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 298000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 306000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_de_Berlin[] = {
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 113000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_de_Brandenburg_Havel[] = {
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 490000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 498000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 506000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 530000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 538000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 546000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 554000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 562000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 570000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 578000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 586000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 594000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 602000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 610000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 618000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 626000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_de_iesy[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 121000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 538000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_de_Kabel_BW[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_de_komro_Rosenheim[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 121000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 338000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 522000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 530000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 602000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 610000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 626000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 642000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 650000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 658000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 666000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 682000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 690000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 698000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 706000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 714000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 722000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 730000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 738000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 746000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 754000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 762000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 778000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_de_Muenchen[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 121000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 482000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 322000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 490000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_de_neftv[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 346000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 354000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 378000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 458000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 474000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 490000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 498000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 514000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 522000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 546000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 618000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 634000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 786000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 794000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 802000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 810000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_de_Primacom[] = {
	{ .freq = 121000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 129000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 137000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 145000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 306000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 314000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 322000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 330000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 338000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 610000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 746000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_de_Unitymedia[] = {
	{ .freq = 113000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 121000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 450000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 522000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 530000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 538000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 546000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 554000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 562000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 570000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 650000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 658000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 666000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 674000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 682000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 690000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 698000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_dk_Aarhus_Antenneforening[] = {
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_dk_Copenhagen_AFDK[] = {
	{ .freq = 386000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 490000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 602000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 610000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 618000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 794000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_dk_Odense[] = {
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 255000000, .symrate = 5000000, .fec = 0, .constellation = 5},
	{ .freq = 506000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 562000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 610000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 754000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 770000000, .symrate = 6875000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_es_Euskaltel[] = {
	{ .freq = 714000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 722000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 730000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 738000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 746000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 754000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 762000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 770000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 778000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 786000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 794000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 802000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 810000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 818000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_fi_dna[] = {
	{ .freq = 154000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 162000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 162000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 170000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 226000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 234000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 242000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 242000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 250000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 258000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 266000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 274000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 282000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 290000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 298000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 298000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 306000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 314000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 322000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 330000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 338000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 346000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 354000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 362000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 362000000, .symrate = 6875000, .fec = 0, .constellation = 5},
	{ .freq = 370000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 378000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 386000000, .symrate = 6875000, .fec = 0, .constellation = 4},
	{ .freq = 394000000, .symrate = 6875000, .fec = 0, .constellation = 4},
};

static const struct mux muxes_DVBC_fi_HTV[] = {
	{ .freq = 283000000, .symrate = 5900000, .fec = 0, .constellation = 4},
	{ .freq = 154000000, .symrate = 6900000, .fec = 0, .constellation = 4},
};

static const struct mux muxes_DVBC_fi_jkl[] = {
	{ .freq = 514000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 162000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 490000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 498000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 4},
};

static const struct mux muxes_DVBC_fi_Joensuu_Tikka[] = {
	{ .freq = 154000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 162000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 170000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 434000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 458000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 466000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 474000000, .symrate = 6900000, .fec = 0, .constellation = 4},
};

static const struct mux muxes_DVBC_fi_sonera[] = {
	{ .freq = 154000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 162000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 170000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 314000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 322000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 338000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 4},
};

static const struct mux muxes_DVBC_fi_TTV[] = {
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 4},
};

static const struct mux muxes_DVBC_fi_Turku[] = {
	{ .freq = 146000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 154000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 162000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 234000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 242000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 250000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 258000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 266000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 290000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 298000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 322000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 330000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 338000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 418000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 426000000, .symrate = 6900000, .fec = 0, .constellation = 4},
	{ .freq = 442000000, .symrate = 6900000, .fec = 0, .constellation = 4},
};

static const struct mux muxes_DVBC_fi_vaasa_oncable[] = {
	{ .freq = 386000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 143000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 314000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 306000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_fr_noos_numericable[] = {
	{ .freq = 123000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 131000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 139000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 147000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 155000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 163000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 171000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 179000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 187000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 195000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 203000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 211000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 219000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 227000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 235000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 243000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 251000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 259000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 267000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 275000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 283000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 291000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 299000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 315000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 323000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 339000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 347000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 706000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 714000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 722000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 730000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 738000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 746000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 748000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 754000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 762000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 834000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 842000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 850000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_hu_Digikabel[] = {
	{ .freq = 121000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 402000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 746000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 754000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 762000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 770000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 778000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 786000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 794000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 850000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_lu_Ettelbruck_ACE[] = {
	{ .freq = 634000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 642000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 650000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 666000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 674000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 682000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 690000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 698000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 706000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 714000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 656000000, .symrate = 3450000, .fec = 0, .constellation = 3},
	{ .freq = 660000000, .symrate = 3450000, .fec = 0, .constellation = 3},
	{ .freq = 720000000, .symrate = 3450000, .fec = 0, .constellation = 3},
	{ .freq = 732000000, .symrate = 3450000, .fec = 0, .constellation = 3},
	{ .freq = 724000000, .symrate = 3450000, .fec = 0, .constellation = 3},
	{ .freq = 728000000, .symrate = 3450000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_nl_REKAM_CAIW_Gouda[] = {
	{ .freq = 163000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 770000000, .symrate = 6900000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_nl_Ziggo[] = {
	{ .freq = 372000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 514000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 356000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 369000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_nl_Ziggo_Zwolle[] = {
	{ .freq = 313000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 361000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 353000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 345000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 818000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 329000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 810000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 305000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 762000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 618000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 610000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 337000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 321000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 385000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 393000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 401000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 369000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 297000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 377000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 754000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 642000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 650000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 794000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 409000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 425000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 417000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_no_Oslo_CanalDigital[] = {
	{ .freq = 354000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 378000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 410000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 426000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 450000000, .symrate = 6950000, .fec = 0, .constellation = 3},
	{ .freq = 474000000, .symrate = 6950000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_no_Oslo_Get[] = {
	{ .freq = 241000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 272000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 280000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 290000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 298000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 306000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 314000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 322000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 330000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 338000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 346000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 354000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 362000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 370000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 378000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 386000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 394000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 410000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 442000000, .symrate = 6952000, .fec = 0, .constellation = 5},
	{ .freq = 482000000, .symrate = 6900000, .fec = 0, .constellation = 5},
	{ .freq = 498000000, .symrate = 6900000, .fec = 0, .constellation = 5},
};

static const struct mux muxes_DVBC_se_comhem[] = {
	{ .freq = 289500000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 298000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 306000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 314000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 322000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 338000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 346000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 354000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 378000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 386000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 394000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 402000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 418000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 434000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 442000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 450000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 458000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 714000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 722000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 738000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 786000000, .symrate = 6875000, .fec = 0, .constellation = 3},
};

static const struct mux muxes_DVBC_se_Gothnet[] = {
	{ .freq = 490000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 498000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 506000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 514000000, .symrate = 6875000, .fec = 0, .constellation = 3},
	{ .freq = 682000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 690000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 698000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 706000000, .symrate = 6900000, .fec = 0, .constellation = 3},
	{ .freq = 314000000, .symrate = 7000000, .fec = 0, .constellation = 3},
	{ .freq = 322000000, .symrate = 7000000, .fec = 0, .constellation = 3},
	{ .freq = 346000000, .symrate = 7000000, .fec = 0, .constellation = 3},
	{ .freq = 354000000, .symrate = 7000000, .fec = 0, .constellation = 3},
	{ .freq = 362000000, .symrate = 7000000, .fec = 0, .constellation = 3},
	{ .freq = 370000000, .symrate = 7000000, .fec = 0, .constellation = 3},
};

static const struct network networks_DVBC_at[] = {
	{
		.name = "Innsbruck",
		.muxes = muxes_DVBC_at_Innsbruck,
		.nmuxes = sizeof(muxes_DVBC_at_Innsbruck) / sizeof(struct mux),
	},
	{
		.name = "Kabel-Braunau",
		.muxes = muxes_DVBC_at_Kabel_Braunau,
		.nmuxes = sizeof(muxes_DVBC_at_Kabel_Braunau) / sizeof(struct mux),
	},
	{
		.name = "Kabelsignal",
		.muxes = muxes_DVBC_at_Kabelsignal,
		.nmuxes = sizeof(muxes_DVBC_at_Kabelsignal) / sizeof(struct mux),
	},
	{
		.name = "KarrerNet",
		.muxes = muxes_DVBC_at_KarrerNet,
		.nmuxes = sizeof(muxes_DVBC_at_KarrerNet) / sizeof(struct mux),
	},
	{
		.name = "Liwest",
		.muxes = muxes_DVBC_at_Liwest,
		.nmuxes = sizeof(muxes_DVBC_at_Liwest) / sizeof(struct mux),
	},
	{
		.name = "SalzburgAG",
		.muxes = muxes_DVBC_at_SalzburgAG,
		.nmuxes = sizeof(muxes_DVBC_at_SalzburgAG) / sizeof(struct mux),
	},
	{
		.name = "Vienna",
		.muxes = muxes_DVBC_at_Vienna,
		.nmuxes = sizeof(muxes_DVBC_at_Vienna) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_be[] = {
	{
		.name = "IN.DI-Integan",
		.muxes = muxes_DVBC_be_IN_DI_Integan,
		.nmuxes = sizeof(muxes_DVBC_be_IN_DI_Integan) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_br[] = {
	{
		.name = "Net-Digital",
		.muxes = muxes_DVBC_br_Net_Digital,
		.nmuxes = sizeof(muxes_DVBC_br_Net_Digital) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_cz[] = {
	{
		.name = "Moravianet",
		.muxes = muxes_DVBC_cz_Moravianet,
		.nmuxes = sizeof(muxes_DVBC_cz_Moravianet) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_dk[] = {
	{
		.name = "Aarhus-Antenneforening",
		.muxes = muxes_DVBC_dk_Aarhus_Antenneforening,
		.nmuxes = sizeof(muxes_DVBC_dk_Aarhus_Antenneforening) / sizeof(struct mux),
	},
	{
		.name = "Copenhagen-AFDK",
		.muxes = muxes_DVBC_dk_Copenhagen_AFDK,
		.nmuxes = sizeof(muxes_DVBC_dk_Copenhagen_AFDK) / sizeof(struct mux),
	},
	{
		.name = "Odense",
		.muxes = muxes_DVBC_dk_Odense,
		.nmuxes = sizeof(muxes_DVBC_dk_Odense) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_fi[] = {
	{
		.name = "HTV",
		.muxes = muxes_DVBC_fi_HTV,
		.nmuxes = sizeof(muxes_DVBC_fi_HTV) / sizeof(struct mux),
	},
	{
		.name = "Joensuu-Tikka",
		.muxes = muxes_DVBC_fi_Joensuu_Tikka,
		.nmuxes = sizeof(muxes_DVBC_fi_Joensuu_Tikka) / sizeof(struct mux),
	},
	{
		.name = "TTV",
		.muxes = muxes_DVBC_fi_TTV,
		.nmuxes = sizeof(muxes_DVBC_fi_TTV) / sizeof(struct mux),
	},
	{
		.name = "Turku",
		.muxes = muxes_DVBC_fi_Turku,
		.nmuxes = sizeof(muxes_DVBC_fi_Turku) / sizeof(struct mux),
	},
	{
		.name = "dna",
		.muxes = muxes_DVBC_fi_dna,
		.nmuxes = sizeof(muxes_DVBC_fi_dna) / sizeof(struct mux),
	},
	{
		.name = "jkl",
		.muxes = muxes_DVBC_fi_jkl,
		.nmuxes = sizeof(muxes_DVBC_fi_jkl) / sizeof(struct mux),
	},
	{
		.name = "sonera",
		.muxes = muxes_DVBC_fi_sonera,
		.nmuxes = sizeof(muxes_DVBC_fi_sonera) / sizeof(struct mux),
	},
	{
		.name = "vaasa-oncable",
		.muxes = muxes_DVBC_fi_vaasa_oncable,
		.nmuxes = sizeof(muxes_DVBC_fi_vaasa_oncable) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_fr[] = {
	{
		.name = "noos-numericable",
		.muxes = muxes_DVBC_fr_noos_numericable,
		.nmuxes = sizeof(muxes_DVBC_fr_noos_numericable) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_de[] = {
	{
		.name = "Berlin",
		.muxes = muxes_DVBC_de_Berlin,
		.nmuxes = sizeof(muxes_DVBC_de_Berlin) / sizeof(struct mux),
	},
	{
		.name = "Brandenburg-Havel",
		.muxes = muxes_DVBC_de_Brandenburg_Havel,
		.nmuxes = sizeof(muxes_DVBC_de_Brandenburg_Havel) / sizeof(struct mux),
	},
	{
		.name = "Kabel BW",
		.muxes = muxes_DVBC_de_Kabel_BW,
		.nmuxes = sizeof(muxes_DVBC_de_Kabel_BW) / sizeof(struct mux),
	},
	{
		.name = "Muenchen",
		.muxes = muxes_DVBC_de_Muenchen,
		.nmuxes = sizeof(muxes_DVBC_de_Muenchen) / sizeof(struct mux),
	},
	{
		.name = "Primacom",
		.muxes = muxes_DVBC_de_Primacom,
		.nmuxes = sizeof(muxes_DVBC_de_Primacom) / sizeof(struct mux),
	},
	{
		.name = "Unitymedia",
		.muxes = muxes_DVBC_de_Unitymedia,
		.nmuxes = sizeof(muxes_DVBC_de_Unitymedia) / sizeof(struct mux),
	},
	{
		.name = "iesy",
		.muxes = muxes_DVBC_de_iesy,
		.nmuxes = sizeof(muxes_DVBC_de_iesy) / sizeof(struct mux),
	},
	{
		.name = "komro-Rosenheim",
		.muxes = muxes_DVBC_de_komro_Rosenheim,
		.nmuxes = sizeof(muxes_DVBC_de_komro_Rosenheim) / sizeof(struct mux),
	},
	{
		.name = "neftv",
		.muxes = muxes_DVBC_de_neftv,
		.nmuxes = sizeof(muxes_DVBC_de_neftv) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_hu[] = {
	{
		.name = "Digikabel",
		.muxes = muxes_DVBC_hu_Digikabel,
		.nmuxes = sizeof(muxes_DVBC_hu_Digikabel) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_lu[] = {
	{
		.name = "Ettelbruck-ACE",
		.muxes = muxes_DVBC_lu_Ettelbruck_ACE,
		.nmuxes = sizeof(muxes_DVBC_lu_Ettelbruck_ACE) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_nl[] = {
	{
		.name = "REKAM-CAIW Gouda",
		.muxes = muxes_DVBC_nl_REKAM_CAIW_Gouda,
		.nmuxes = sizeof(muxes_DVBC_nl_REKAM_CAIW_Gouda) / sizeof(struct mux),
	},
	{
		.name = "Ziggo",
		.muxes = muxes_DVBC_nl_Ziggo,
		.nmuxes = sizeof(muxes_DVBC_nl_Ziggo) / sizeof(struct mux),
	},
	{
		.name = "Ziggo-Zwolle",
		.muxes = muxes_DVBC_nl_Ziggo_Zwolle,
		.nmuxes = sizeof(muxes_DVBC_nl_Ziggo_Zwolle) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_no[] = {
	{
		.name = "Oslo-CanalDigital",
		.muxes = muxes_DVBC_no_Oslo_CanalDigital,
		.nmuxes = sizeof(muxes_DVBC_no_Oslo_CanalDigital) / sizeof(struct mux),
	},
	{
		.name = "Oslo-Get",
		.muxes = muxes_DVBC_no_Oslo_Get,
		.nmuxes = sizeof(muxes_DVBC_no_Oslo_Get) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_es[] = {
	{
		.name = "Euskaltel",
		.muxes = muxes_DVBC_es_Euskaltel,
		.nmuxes = sizeof(muxes_DVBC_es_Euskaltel) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_se[] = {
	{
		.name = "Gothnet",
		.muxes = muxes_DVBC_se_Gothnet,
		.nmuxes = sizeof(muxes_DVBC_se_Gothnet) / sizeof(struct mux),
	},
	{
		.name = "comhem",
		.muxes = muxes_DVBC_se_comhem,
		.nmuxes = sizeof(muxes_DVBC_se_comhem) / sizeof(struct mux),
	},
};

static const struct network networks_DVBC_ch[] = {
	{
		.name = "Rega-Sense",
		.muxes = muxes_DVBC_ch_Rega_Sense,
		.nmuxes = sizeof(muxes_DVBC_ch_Rega_Sense) / sizeof(struct mux),
	},
	{
		.name = "Video2000",
		.muxes = muxes_DVBC_ch_Video2000,
		.nmuxes = sizeof(muxes_DVBC_ch_Video2000) / sizeof(struct mux),
	},
	{
		.name = "Zuerich-cablecom",
		.muxes = muxes_DVBC_ch_Zuerich_cablecom,
		.nmuxes = sizeof(muxes_DVBC_ch_Zuerich_cablecom) / sizeof(struct mux),
	},
	{
		.name = "unknown",
		.muxes = muxes_DVBC_ch_unknown,
		.nmuxes = sizeof(muxes_DVBC_ch_unknown) / sizeof(struct mux),
	},
};

static const struct region regions_DVBC[] = {
	{
		.name = "Austria",
		.networks = networks_DVBC_at,
		.nnetworks = sizeof(networks_DVBC_at) / sizeof(struct network),
	},
	{
		.name = "Belgium",
		.networks = networks_DVBC_be,
		.nnetworks = sizeof(networks_DVBC_be) / sizeof(struct network),
	},
	{
		.name = "Brazil",
		.networks = networks_DVBC_br,
		.nnetworks = sizeof(networks_DVBC_br) / sizeof(struct network),
	},
	{
		.name = "Czech Republic",
		.networks = networks_DVBC_cz,
		.nnetworks = sizeof(networks_DVBC_cz) / sizeof(struct network),
	},
	{
		.name = "Denmark",
		.networks = networks_DVBC_dk,
		.nnetworks = sizeof(networks_DVBC_dk) / sizeof(struct network),
	},
	{
		.name = "Finland",
		.networks = networks_DVBC_fi,
		.nnetworks = sizeof(networks_DVBC_fi) / sizeof(struct network),
	},
	{
		.name = "France",
		.networks = networks_DVBC_fr,
		.nnetworks = sizeof(networks_DVBC_fr) / sizeof(struct network),
	},
	{
		.name = "Germany",
		.networks = networks_DVBC_de,
		.nnetworks = sizeof(networks_DVBC_de) / sizeof(struct network),
	},
	{
		.name = "Hungary",
		.networks = networks_DVBC_hu,
		.nnetworks = sizeof(networks_DVBC_hu) / sizeof(struct network),
	},
	{
		.name = "Luxembourg",
		.networks = networks_DVBC_lu,
		.nnetworks = sizeof(networks_DVBC_lu) / sizeof(struct network),
	},
	{
		.name = "Netherlands",
		.networks = networks_DVBC_nl,
		.nnetworks = sizeof(networks_DVBC_nl) / sizeof(struct network),
	},
	{
		.name = "Norway",
		.networks = networks_DVBC_no,
		.nnetworks = sizeof(networks_DVBC_no) / sizeof(struct network),
	},
	{
		.name = "Spain",
		.networks = networks_DVBC_es,
		.nnetworks = sizeof(networks_DVBC_es) / sizeof(struct network),
	},
	{
		.name = "Sweden",
		.networks = networks_DVBC_se,
		.nnetworks = sizeof(networks_DVBC_se) / sizeof(struct network),
	},
	{
		.name = "Switzerland",
		.networks = networks_DVBC_ch,
		.nnetworks = sizeof(networks_DVBC_ch) / sizeof(struct network),
	},
};

static const struct mux muxes_ATSC_ca_AB_Calgary[] = {
	{ .freq = 515000000, .constellation = 7},
	{ .freq = 563000000, .constellation = 7},
	{ .freq = 635000000, .constellation = 7},
	{ .freq = 683000000, .constellation = 7},
};

static const struct mux muxes_ATSC_us_ATSC_center_frequencies_8VSB[] = {
	{ .freq = 57028615, .constellation = 7},
	{ .freq = 63028615, .constellation = 7},
	{ .freq = 69028615, .constellation = 7},
	{ .freq = 79028615, .constellation = 7},
	{ .freq = 85028615, .constellation = 7},
	{ .freq = 177028615, .constellation = 7},
	{ .freq = 183028615, .constellation = 7},
	{ .freq = 189028615, .constellation = 7},
	{ .freq = 195028615, .constellation = 7},
	{ .freq = 201028615, .constellation = 7},
	{ .freq = 207028615, .constellation = 7},
	{ .freq = 213028615, .constellation = 7},
	{ .freq = 473028615, .constellation = 7},
	{ .freq = 479028615, .constellation = 7},
	{ .freq = 485028615, .constellation = 7},
	{ .freq = 491028615, .constellation = 7},
	{ .freq = 497028615, .constellation = 7},
	{ .freq = 503028615, .constellation = 7},
	{ .freq = 509028615, .constellation = 7},
	{ .freq = 515028615, .constellation = 7},
	{ .freq = 521028615, .constellation = 7},
	{ .freq = 527028615, .constellation = 7},
	{ .freq = 533028615, .constellation = 7},
	{ .freq = 539028615, .constellation = 7},
	{ .freq = 545028615, .constellation = 7},
	{ .freq = 551028615, .constellation = 7},
	{ .freq = 557028615, .constellation = 7},
	{ .freq = 563028615, .constellation = 7},
	{ .freq = 569028615, .constellation = 7},
	{ .freq = 575028615, .constellation = 7},
	{ .freq = 581028615, .constellation = 7},
	{ .freq = 587028615, .constellation = 7},
	{ .freq = 593028615, .constellation = 7},
	{ .freq = 599028615, .constellation = 7},
	{ .freq = 605028615, .constellation = 7},
	{ .freq = 611028615, .constellation = 7},
	{ .freq = 617028615, .constellation = 7},
	{ .freq = 623028615, .constellation = 7},
	{ .freq = 629028615, .constellation = 7},
	{ .freq = 635028615, .constellation = 7},
	{ .freq = 641028615, .constellation = 7},
	{ .freq = 647028615, .constellation = 7},
	{ .freq = 653028615, .constellation = 7},
	{ .freq = 659028615, .constellation = 7},
	{ .freq = 665028615, .constellation = 7},
	{ .freq = 671028615, .constellation = 7},
	{ .freq = 677028615, .constellation = 7},
	{ .freq = 683028615, .constellation = 7},
	{ .freq = 689028615, .constellation = 7},
	{ .freq = 695028615, .constellation = 7},
	{ .freq = 701028615, .constellation = 7},
	{ .freq = 707028615, .constellation = 7},
	{ .freq = 713028615, .constellation = 7},
	{ .freq = 719028615, .constellation = 7},
	{ .freq = 725028615, .constellation = 7},
	{ .freq = 731028615, .constellation = 7},
	{ .freq = 737028615, .constellation = 7},
	{ .freq = 743028615, .constellation = 7},
	{ .freq = 749028615, .constellation = 7},
	{ .freq = 755028615, .constellation = 7},
	{ .freq = 761028615, .constellation = 7},
	{ .freq = 767028615, .constellation = 7},
	{ .freq = 773028615, .constellation = 7},
	{ .freq = 779028615, .constellation = 7},
	{ .freq = 785028615, .constellation = 7},
	{ .freq = 791028615, .constellation = 7},
	{ .freq = 797028615, .constellation = 7},
	{ .freq = 803028615, .constellation = 7},
};

static const struct mux muxes_ATSC_us_Cable_EIA_542_HRC_center_frequencies_QAM256[] = {
	{ .freq = 73753600, .constellation = 5},
	{ .freq = 55752700, .constellation = 5},
	{ .freq = 61753000, .constellation = 5},
	{ .freq = 67753300, .constellation = 5},
	{ .freq = 77753900, .constellation = 5},
	{ .freq = 83754200, .constellation = 5},
	{ .freq = 175758700, .constellation = 5},
	{ .freq = 181759000, .constellation = 5},
	{ .freq = 187759300, .constellation = 5},
	{ .freq = 193759600, .constellation = 5},
	{ .freq = 199759900, .constellation = 5},
	{ .freq = 205760200, .constellation = 5},
	{ .freq = 211760500, .constellation = 5},
	{ .freq = 121756000, .constellation = 5},
	{ .freq = 127756300, .constellation = 5},
	{ .freq = 133756600, .constellation = 5},
	{ .freq = 139756900, .constellation = 5},
	{ .freq = 145757200, .constellation = 5},
	{ .freq = 151757500, .constellation = 5},
	{ .freq = 157757800, .constellation = 5},
	{ .freq = 163758100, .constellation = 5},
	{ .freq = 169758400, .constellation = 5},
	{ .freq = 217760800, .constellation = 5},
	{ .freq = 223761100, .constellation = 5},
	{ .freq = 229761400, .constellation = 5},
	{ .freq = 235761700, .constellation = 5},
	{ .freq = 241762000, .constellation = 5},
	{ .freq = 247762300, .constellation = 5},
	{ .freq = 253762600, .constellation = 5},
	{ .freq = 259762900, .constellation = 5},
	{ .freq = 265763200, .constellation = 5},
	{ .freq = 271763500, .constellation = 5},
	{ .freq = 277763800, .constellation = 5},
	{ .freq = 283764100, .constellation = 5},
	{ .freq = 289764400, .constellation = 5},
	{ .freq = 295764700, .constellation = 5},
	{ .freq = 301765000, .constellation = 5},
	{ .freq = 307765300, .constellation = 5},
	{ .freq = 313765600, .constellation = 5},
	{ .freq = 319765900, .constellation = 5},
	{ .freq = 325766200, .constellation = 5},
	{ .freq = 331766500, .constellation = 5},
	{ .freq = 337766800, .constellation = 5},
	{ .freq = 343767100, .constellation = 5},
	{ .freq = 349767400, .constellation = 5},
	{ .freq = 355767700, .constellation = 5},
	{ .freq = 361768000, .constellation = 5},
	{ .freq = 367768300, .constellation = 5},
	{ .freq = 373768600, .constellation = 5},
	{ .freq = 379768900, .constellation = 5},
	{ .freq = 385769200, .constellation = 5},
	{ .freq = 391769500, .constellation = 5},
	{ .freq = 397769800, .constellation = 5},
	{ .freq = 403770100, .constellation = 5},
	{ .freq = 409770400, .constellation = 5},
	{ .freq = 415770700, .constellation = 5},
	{ .freq = 421771000, .constellation = 5},
	{ .freq = 427771300, .constellation = 5},
	{ .freq = 433771600, .constellation = 5},
	{ .freq = 439771900, .constellation = 5},
	{ .freq = 445772200, .constellation = 5},
	{ .freq = 451772500, .constellation = 5},
	{ .freq = 457772800, .constellation = 5},
	{ .freq = 463773100, .constellation = 5},
	{ .freq = 469773400, .constellation = 5},
	{ .freq = 475773700, .constellation = 5},
	{ .freq = 481774000, .constellation = 5},
	{ .freq = 487774300, .constellation = 5},
	{ .freq = 493774600, .constellation = 5},
	{ .freq = 499774900, .constellation = 5},
	{ .freq = 505775200, .constellation = 5},
	{ .freq = 511775500, .constellation = 5},
	{ .freq = 517775800, .constellation = 5},
	{ .freq = 523776100, .constellation = 5},
	{ .freq = 529776400, .constellation = 5},
	{ .freq = 535776700, .constellation = 5},
	{ .freq = 541777000, .constellation = 5},
	{ .freq = 547777300, .constellation = 5},
	{ .freq = 553777600, .constellation = 5},
	{ .freq = 559777900, .constellation = 5},
	{ .freq = 565778200, .constellation = 5},
	{ .freq = 571778500, .constellation = 5},
	{ .freq = 577778800, .constellation = 5},
	{ .freq = 583779100, .constellation = 5},
	{ .freq = 589779400, .constellation = 5},
	{ .freq = 595779700, .constellation = 5},
	{ .freq = 601780000, .constellation = 5},
	{ .freq = 607780300, .constellation = 5},
	{ .freq = 613780600, .constellation = 5},
	{ .freq = 619780900, .constellation = 5},
	{ .freq = 625781200, .constellation = 5},
	{ .freq = 631781500, .constellation = 5},
	{ .freq = 637781800, .constellation = 5},
	{ .freq = 643782100, .constellation = 5},
	{ .freq = 91754500, .constellation = 5},
	{ .freq = 97754800, .constellation = 5},
	{ .freq = 103755100, .constellation = 5},
	{ .freq = 109775000, .constellation = 5},
	{ .freq = 115775000, .constellation = 5},
	{ .freq = 649782400, .constellation = 5},
	{ .freq = 655782700, .constellation = 5},
	{ .freq = 661783000, .constellation = 5},
	{ .freq = 667783300, .constellation = 5},
	{ .freq = 673783600, .constellation = 5},
	{ .freq = 679783900, .constellation = 5},
	{ .freq = 685784200, .constellation = 5},
	{ .freq = 691784500, .constellation = 5},
	{ .freq = 697784800, .constellation = 5},
	{ .freq = 703785100, .constellation = 5},
	{ .freq = 709785400, .constellation = 5},
	{ .freq = 715785700, .constellation = 5},
	{ .freq = 721786000, .constellation = 5},
	{ .freq = 727786300, .constellation = 5},
	{ .freq = 733786600, .constellation = 5},
	{ .freq = 739786900, .constellation = 5},
	{ .freq = 745787200, .constellation = 5},
	{ .freq = 751787500, .constellation = 5},
	{ .freq = 757787800, .constellation = 5},
	{ .freq = 763788100, .constellation = 5},
	{ .freq = 769788400, .constellation = 5},
	{ .freq = 775788700, .constellation = 5},
	{ .freq = 781789000, .constellation = 5},
	{ .freq = 787789300, .constellation = 5},
	{ .freq = 793789600, .constellation = 5},
	{ .freq = 799789900, .constellation = 5},
};

static const struct mux muxes_ATSC_us_Cable_EIA_542_IRC_center_frequencies_QAM256[] = {
	{ .freq = 75012500, .constellation = 5},
	{ .freq = 57025000, .constellation = 5},
	{ .freq = 63012500, .constellation = 5},
	{ .freq = 69012500, .constellation = 5},
	{ .freq = 79012500, .constellation = 5},
	{ .freq = 85012500, .constellation = 5},
	{ .freq = 177012500, .constellation = 5},
	{ .freq = 183012500, .constellation = 5},
	{ .freq = 189012500, .constellation = 5},
	{ .freq = 195012500, .constellation = 5},
	{ .freq = 201012500, .constellation = 5},
	{ .freq = 207012500, .constellation = 5},
	{ .freq = 213012500, .constellation = 5},
	{ .freq = 123012500, .constellation = 5},
	{ .freq = 129012500, .constellation = 5},
	{ .freq = 135012500, .constellation = 5},
	{ .freq = 141012500, .constellation = 5},
	{ .freq = 147012500, .constellation = 5},
	{ .freq = 153012500, .constellation = 5},
	{ .freq = 159012500, .constellation = 5},
	{ .freq = 165012500, .constellation = 5},
	{ .freq = 171012500, .constellation = 5},
	{ .freq = 219012500, .constellation = 5},
	{ .freq = 225012500, .constellation = 5},
	{ .freq = 231012500, .constellation = 5},
	{ .freq = 237012500, .constellation = 5},
	{ .freq = 243012500, .constellation = 5},
	{ .freq = 249012500, .constellation = 5},
	{ .freq = 255012500, .constellation = 5},
	{ .freq = 261012500, .constellation = 5},
	{ .freq = 267012500, .constellation = 5},
	{ .freq = 273012500, .constellation = 5},
	{ .freq = 279012500, .constellation = 5},
	{ .freq = 285012500, .constellation = 5},
	{ .freq = 291012500, .constellation = 5},
	{ .freq = 297012500, .constellation = 5},
	{ .freq = 303012500, .constellation = 5},
	{ .freq = 309012500, .constellation = 5},
	{ .freq = 315012500, .constellation = 5},
	{ .freq = 321012500, .constellation = 5},
	{ .freq = 327012500, .constellation = 5},
	{ .freq = 333025000, .constellation = 5},
	{ .freq = 339012500, .constellation = 5},
	{ .freq = 345012500, .constellation = 5},
	{ .freq = 351012500, .constellation = 5},
	{ .freq = 357012500, .constellation = 5},
	{ .freq = 363012500, .constellation = 5},
	{ .freq = 369012500, .constellation = 5},
	{ .freq = 375012500, .constellation = 5},
	{ .freq = 381012500, .constellation = 5},
	{ .freq = 387012500, .constellation = 5},
	{ .freq = 393012500, .constellation = 5},
	{ .freq = 399012500, .constellation = 5},
	{ .freq = 405012500, .constellation = 5},
	{ .freq = 411012500, .constellation = 5},
	{ .freq = 417012500, .constellation = 5},
	{ .freq = 423012500, .constellation = 5},
	{ .freq = 429012500, .constellation = 5},
	{ .freq = 435012500, .constellation = 5},
	{ .freq = 441012500, .constellation = 5},
	{ .freq = 447012500, .constellation = 5},
	{ .freq = 453012500, .constellation = 5},
	{ .freq = 459012500, .constellation = 5},
	{ .freq = 465012500, .constellation = 5},
	{ .freq = 471012500, .constellation = 5},
	{ .freq = 477012500, .constellation = 5},
	{ .freq = 483012500, .constellation = 5},
	{ .freq = 489012500, .constellation = 5},
	{ .freq = 495012500, .constellation = 5},
	{ .freq = 501012500, .constellation = 5},
	{ .freq = 507012500, .constellation = 5},
	{ .freq = 513012500, .constellation = 5},
	{ .freq = 519012500, .constellation = 5},
	{ .freq = 525012500, .constellation = 5},
	{ .freq = 531012500, .constellation = 5},
	{ .freq = 537012500, .constellation = 5},
	{ .freq = 543012500, .constellation = 5},
	{ .freq = 549012500, .constellation = 5},
	{ .freq = 555012500, .constellation = 5},
	{ .freq = 561012500, .constellation = 5},
	{ .freq = 567012500, .constellation = 5},
	{ .freq = 573012500, .constellation = 5},
	{ .freq = 579012500, .constellation = 5},
	{ .freq = 585012500, .constellation = 5},
	{ .freq = 591012500, .constellation = 5},
	{ .freq = 597012500, .constellation = 5},
	{ .freq = 603012500, .constellation = 5},
	{ .freq = 609012500, .constellation = 5},
	{ .freq = 615012500, .constellation = 5},
	{ .freq = 621012500, .constellation = 5},
	{ .freq = 627012500, .constellation = 5},
	{ .freq = 633012500, .constellation = 5},
	{ .freq = 639012500, .constellation = 5},
	{ .freq = 645012500, .constellation = 5},
	{ .freq = 93012500, .constellation = 5},
	{ .freq = 99012500, .constellation = 5},
	{ .freq = 105012500, .constellation = 5},
	{ .freq = 111012500, .constellation = 5},
	{ .freq = 117012500, .constellation = 5},
	{ .freq = 651012500, .constellation = 5},
	{ .freq = 657012500, .constellation = 5},
	{ .freq = 663012500, .constellation = 5},
	{ .freq = 669012500, .constellation = 5},
	{ .freq = 675012500, .constellation = 5},
	{ .freq = 681012500, .constellation = 5},
	{ .freq = 687012500, .constellation = 5},
	{ .freq = 693012500, .constellation = 5},
	{ .freq = 699012500, .constellation = 5},
	{ .freq = 705012500, .constellation = 5},
	{ .freq = 711012500, .constellation = 5},
	{ .freq = 717012500, .constellation = 5},
	{ .freq = 723012500, .constellation = 5},
	{ .freq = 729012500, .constellation = 5},
	{ .freq = 735012500, .constellation = 5},
	{ .freq = 741012500, .constellation = 5},
	{ .freq = 747012500, .constellation = 5},
	{ .freq = 753012500, .constellation = 5},
	{ .freq = 759012500, .constellation = 5},
	{ .freq = 765012500, .constellation = 5},
	{ .freq = 771012500, .constellation = 5},
	{ .freq = 777012500, .constellation = 5},
	{ .freq = 783012500, .constellation = 5},
	{ .freq = 789012500, .constellation = 5},
	{ .freq = 795012500, .constellation = 5},
	{ .freq = 801012500, .constellation = 5},
};

static const struct mux muxes_ATSC_us_Cable_HRC_center_frequencies_QAM256[] = {
	{ .freq = 73753600, .constellation = 5},
	{ .freq = 55752700, .constellation = 5},
	{ .freq = 61753000, .constellation = 5},
	{ .freq = 67753300, .constellation = 5},
	{ .freq = 77753900, .constellation = 5},
	{ .freq = 83754200, .constellation = 5},
	{ .freq = 175758700, .constellation = 5},
	{ .freq = 181759000, .constellation = 5},
	{ .freq = 187759300, .constellation = 5},
	{ .freq = 193759600, .constellation = 5},
	{ .freq = 199759900, .constellation = 5},
	{ .freq = 205760200, .constellation = 5},
	{ .freq = 211760500, .constellation = 5},
	{ .freq = 121756000, .constellation = 5},
	{ .freq = 127756300, .constellation = 5},
	{ .freq = 133756600, .constellation = 5},
	{ .freq = 139756900, .constellation = 5},
	{ .freq = 145757200, .constellation = 5},
	{ .freq = 151757500, .constellation = 5},
	{ .freq = 157757800, .constellation = 5},
	{ .freq = 163758100, .constellation = 5},
	{ .freq = 169758400, .constellation = 5},
	{ .freq = 217760800, .constellation = 5},
	{ .freq = 223761100, .constellation = 5},
	{ .freq = 229761400, .constellation = 5},
	{ .freq = 235761700, .constellation = 5},
	{ .freq = 241762000, .constellation = 5},
	{ .freq = 247762300, .constellation = 5},
	{ .freq = 253762600, .constellation = 5},
	{ .freq = 259762900, .constellation = 5},
	{ .freq = 265763200, .constellation = 5},
	{ .freq = 271763500, .constellation = 5},
	{ .freq = 277763800, .constellation = 5},
	{ .freq = 283764100, .constellation = 5},
	{ .freq = 289764400, .constellation = 5},
	{ .freq = 295764700, .constellation = 5},
	{ .freq = 301765000, .constellation = 5},
	{ .freq = 307765300, .constellation = 5},
	{ .freq = 313765600, .constellation = 5},
	{ .freq = 319765900, .constellation = 5},
	{ .freq = 325766200, .constellation = 5},
	{ .freq = 331766500, .constellation = 5},
	{ .freq = 337766800, .constellation = 5},
	{ .freq = 343767100, .constellation = 5},
	{ .freq = 349767400, .constellation = 5},
	{ .freq = 355767700, .constellation = 5},
	{ .freq = 361768000, .constellation = 5},
	{ .freq = 367768300, .constellation = 5},
	{ .freq = 373768600, .constellation = 5},
	{ .freq = 379768900, .constellation = 5},
	{ .freq = 385769200, .constellation = 5},
	{ .freq = 391769500, .constellation = 5},
	{ .freq = 397769800, .constellation = 5},
	{ .freq = 403770100, .constellation = 5},
	{ .freq = 409770400, .constellation = 5},
	{ .freq = 415770700, .constellation = 5},
	{ .freq = 421771000, .constellation = 5},
	{ .freq = 427771300, .constellation = 5},
	{ .freq = 433771600, .constellation = 5},
	{ .freq = 439771900, .constellation = 5},
	{ .freq = 445772200, .constellation = 5},
	{ .freq = 451772500, .constellation = 5},
	{ .freq = 457772800, .constellation = 5},
	{ .freq = 463773100, .constellation = 5},
	{ .freq = 469773400, .constellation = 5},
	{ .freq = 475773700, .constellation = 5},
	{ .freq = 481774000, .constellation = 5},
	{ .freq = 487774300, .constellation = 5},
	{ .freq = 493774600, .constellation = 5},
	{ .freq = 499774900, .constellation = 5},
	{ .freq = 505775200, .constellation = 5},
	{ .freq = 511775500, .constellation = 5},
	{ .freq = 517775800, .constellation = 5},
	{ .freq = 523776100, .constellation = 5},
	{ .freq = 529776400, .constellation = 5},
	{ .freq = 535776700, .constellation = 5},
	{ .freq = 541777000, .constellation = 5},
	{ .freq = 547777300, .constellation = 5},
	{ .freq = 553777600, .constellation = 5},
	{ .freq = 559777900, .constellation = 5},
	{ .freq = 565778200, .constellation = 5},
	{ .freq = 571778500, .constellation = 5},
	{ .freq = 577778800, .constellation = 5},
	{ .freq = 583779100, .constellation = 5},
	{ .freq = 589779400, .constellation = 5},
	{ .freq = 595779700, .constellation = 5},
	{ .freq = 601780000, .constellation = 5},
	{ .freq = 607780300, .constellation = 5},
	{ .freq = 613780600, .constellation = 5},
	{ .freq = 619780900, .constellation = 5},
	{ .freq = 625781200, .constellation = 5},
	{ .freq = 631781500, .constellation = 5},
	{ .freq = 637781800, .constellation = 5},
	{ .freq = 643782100, .constellation = 5},
	{ .freq = 91754500, .constellation = 5},
	{ .freq = 97754800, .constellation = 5},
	{ .freq = 103755100, .constellation = 5},
	{ .freq = 109755400, .constellation = 5},
	{ .freq = 115755700, .constellation = 5},
	{ .freq = 649782400, .constellation = 5},
	{ .freq = 655782700, .constellation = 5},
	{ .freq = 661783000, .constellation = 5},
	{ .freq = 667783300, .constellation = 5},
	{ .freq = 673783600, .constellation = 5},
	{ .freq = 679783900, .constellation = 5},
	{ .freq = 685784200, .constellation = 5},
	{ .freq = 691784500, .constellation = 5},
	{ .freq = 697784800, .constellation = 5},
	{ .freq = 703785100, .constellation = 5},
	{ .freq = 709785400, .constellation = 5},
	{ .freq = 715785700, .constellation = 5},
	{ .freq = 721786000, .constellation = 5},
	{ .freq = 727786300, .constellation = 5},
	{ .freq = 733786600, .constellation = 5},
	{ .freq = 739786900, .constellation = 5},
	{ .freq = 745787200, .constellation = 5},
	{ .freq = 751787500, .constellation = 5},
	{ .freq = 757787800, .constellation = 5},
	{ .freq = 763788100, .constellation = 5},
	{ .freq = 769788400, .constellation = 5},
	{ .freq = 775788700, .constellation = 5},
	{ .freq = 781789000, .constellation = 5},
	{ .freq = 787789300, .constellation = 5},
	{ .freq = 793789600, .constellation = 5},
	{ .freq = 799789900, .constellation = 5},
};

static const struct mux muxes_ATSC_us_Cable_IRC_center_frequencies_QAM256[] = {
	{ .freq = 75000000, .constellation = 5},
	{ .freq = 57000000, .constellation = 5},
	{ .freq = 63000000, .constellation = 5},
	{ .freq = 69000000, .constellation = 5},
	{ .freq = 79000000, .constellation = 5},
	{ .freq = 85000000, .constellation = 5},
	{ .freq = 177000000, .constellation = 5},
	{ .freq = 183000000, .constellation = 5},
	{ .freq = 189000000, .constellation = 5},
	{ .freq = 195000000, .constellation = 5},
	{ .freq = 201000000, .constellation = 5},
	{ .freq = 207000000, .constellation = 5},
	{ .freq = 213000000, .constellation = 5},
	{ .freq = 123000000, .constellation = 5},
	{ .freq = 129000000, .constellation = 5},
	{ .freq = 135000000, .constellation = 5},
	{ .freq = 141000000, .constellation = 5},
	{ .freq = 147000000, .constellation = 5},
	{ .freq = 153000000, .constellation = 5},
	{ .freq = 159000000, .constellation = 5},
	{ .freq = 165000000, .constellation = 5},
	{ .freq = 171000000, .constellation = 5},
	{ .freq = 219000000, .constellation = 5},
	{ .freq = 225000000, .constellation = 5},
	{ .freq = 231000000, .constellation = 5},
	{ .freq = 237000000, .constellation = 5},
	{ .freq = 243000000, .constellation = 5},
	{ .freq = 249000000, .constellation = 5},
	{ .freq = 255000000, .constellation = 5},
	{ .freq = 261000000, .constellation = 5},
	{ .freq = 267000000, .constellation = 5},
	{ .freq = 273000000, .constellation = 5},
	{ .freq = 279000000, .constellation = 5},
	{ .freq = 285000000, .constellation = 5},
	{ .freq = 291000000, .constellation = 5},
	{ .freq = 297000000, .constellation = 5},
	{ .freq = 303000000, .constellation = 5},
	{ .freq = 309000000, .constellation = 5},
	{ .freq = 315000000, .constellation = 5},
	{ .freq = 321000000, .constellation = 5},
	{ .freq = 327000000, .constellation = 5},
	{ .freq = 333000000, .constellation = 5},
	{ .freq = 339000000, .constellation = 5},
	{ .freq = 345000000, .constellation = 5},
	{ .freq = 351000000, .constellation = 5},
	{ .freq = 357000000, .constellation = 5},
	{ .freq = 363000000, .constellation = 5},
	{ .freq = 369000000, .constellation = 5},
	{ .freq = 375000000, .constellation = 5},
	{ .freq = 381000000, .constellation = 5},
	{ .freq = 387000000, .constellation = 5},
	{ .freq = 393000000, .constellation = 5},
	{ .freq = 399000000, .constellation = 5},
	{ .freq = 405000000, .constellation = 5},
	{ .freq = 411000000, .constellation = 5},
	{ .freq = 417000000, .constellation = 5},
	{ .freq = 423000000, .constellation = 5},
	{ .freq = 429000000, .constellation = 5},
	{ .freq = 435000000, .constellation = 5},
	{ .freq = 441000000, .constellation = 5},
	{ .freq = 447000000, .constellation = 5},
	{ .freq = 453000000, .constellation = 5},
	{ .freq = 459000000, .constellation = 5},
	{ .freq = 465000000, .constellation = 5},
	{ .freq = 471000000, .constellation = 5},
	{ .freq = 477000000, .constellation = 5},
	{ .freq = 483000000, .constellation = 5},
	{ .freq = 489000000, .constellation = 5},
	{ .freq = 495000000, .constellation = 5},
	{ .freq = 501000000, .constellation = 5},
	{ .freq = 507000000, .constellation = 5},
	{ .freq = 513000000, .constellation = 5},
	{ .freq = 519000000, .constellation = 5},
	{ .freq = 525000000, .constellation = 5},
	{ .freq = 531000000, .constellation = 5},
	{ .freq = 537000000, .constellation = 5},
	{ .freq = 543000000, .constellation = 5},
	{ .freq = 549000000, .constellation = 5},
	{ .freq = 555000000, .constellation = 5},
	{ .freq = 561000000, .constellation = 5},
	{ .freq = 567000000, .constellation = 5},
	{ .freq = 573000000, .constellation = 5},
	{ .freq = 579000000, .constellation = 5},
	{ .freq = 585000000, .constellation = 5},
	{ .freq = 591000000, .constellation = 5},
	{ .freq = 597000000, .constellation = 5},
	{ .freq = 603000000, .constellation = 5},
	{ .freq = 609000000, .constellation = 5},
	{ .freq = 615000000, .constellation = 5},
	{ .freq = 621000000, .constellation = 5},
	{ .freq = 627000000, .constellation = 5},
	{ .freq = 633000000, .constellation = 5},
	{ .freq = 639000000, .constellation = 5},
	{ .freq = 645000000, .constellation = 5},
	{ .freq = 93000000, .constellation = 5},
	{ .freq = 99000000, .constellation = 5},
	{ .freq = 105000000, .constellation = 5},
	{ .freq = 111000000, .constellation = 5},
	{ .freq = 117000000, .constellation = 5},
	{ .freq = 651000000, .constellation = 5},
	{ .freq = 657000000, .constellation = 5},
	{ .freq = 663000000, .constellation = 5},
	{ .freq = 669000000, .constellation = 5},
	{ .freq = 675000000, .constellation = 5},
	{ .freq = 681000000, .constellation = 5},
	{ .freq = 687000000, .constellation = 5},
	{ .freq = 693000000, .constellation = 5},
	{ .freq = 699000000, .constellation = 5},
	{ .freq = 705000000, .constellation = 5},
	{ .freq = 711000000, .constellation = 5},
	{ .freq = 717000000, .constellation = 5},
	{ .freq = 723000000, .constellation = 5},
	{ .freq = 729000000, .constellation = 5},
	{ .freq = 735000000, .constellation = 5},
	{ .freq = 741000000, .constellation = 5},
	{ .freq = 747000000, .constellation = 5},
	{ .freq = 753000000, .constellation = 5},
	{ .freq = 759000000, .constellation = 5},
	{ .freq = 765000000, .constellation = 5},
	{ .freq = 771000000, .constellation = 5},
	{ .freq = 777000000, .constellation = 5},
	{ .freq = 783000000, .constellation = 5},
	{ .freq = 789000000, .constellation = 5},
	{ .freq = 795000000, .constellation = 5},
	{ .freq = 801000000, .constellation = 5},
};

static const struct mux muxes_ATSC_us_Cable_Standard_center_frequencies_QAM256[] = {
	{ .freq = 57000000, .constellation = 5},
	{ .freq = 63000000, .constellation = 5},
	{ .freq = 69000000, .constellation = 5},
	{ .freq = 79000000, .constellation = 5},
	{ .freq = 85000000, .constellation = 5},
	{ .freq = 177000000, .constellation = 5},
	{ .freq = 183000000, .constellation = 5},
	{ .freq = 189000000, .constellation = 5},
	{ .freq = 195000000, .constellation = 5},
	{ .freq = 201000000, .constellation = 5},
	{ .freq = 207000000, .constellation = 5},
	{ .freq = 213000000, .constellation = 5},
	{ .freq = 123012500, .constellation = 5},
	{ .freq = 129012500, .constellation = 5},
	{ .freq = 135012500, .constellation = 5},
	{ .freq = 141000000, .constellation = 5},
	{ .freq = 147000000, .constellation = 5},
	{ .freq = 153000000, .constellation = 5},
	{ .freq = 159000000, .constellation = 5},
	{ .freq = 165000000, .constellation = 5},
	{ .freq = 171000000, .constellation = 5},
	{ .freq = 219000000, .constellation = 5},
	{ .freq = 225000000, .constellation = 5},
	{ .freq = 231012500, .constellation = 5},
	{ .freq = 237012500, .constellation = 5},
	{ .freq = 243012500, .constellation = 5},
	{ .freq = 249012500, .constellation = 5},
	{ .freq = 255012500, .constellation = 5},
	{ .freq = 261012500, .constellation = 5},
	{ .freq = 267012500, .constellation = 5},
	{ .freq = 273012500, .constellation = 5},
	{ .freq = 279012500, .constellation = 5},
	{ .freq = 285012500, .constellation = 5},
	{ .freq = 291012500, .constellation = 5},
	{ .freq = 297012500, .constellation = 5},
	{ .freq = 303012500, .constellation = 5},
	{ .freq = 309012500, .constellation = 5},
	{ .freq = 315012500, .constellation = 5},
	{ .freq = 321012500, .constellation = 5},
	{ .freq = 327012500, .constellation = 5},
	{ .freq = 333025000, .constellation = 5},
	{ .freq = 339012500, .constellation = 5},
	{ .freq = 345012500, .constellation = 5},
	{ .freq = 351012500, .constellation = 5},
	{ .freq = 357012500, .constellation = 5},
	{ .freq = 363012500, .constellation = 5},
	{ .freq = 369012500, .constellation = 5},
	{ .freq = 375012500, .constellation = 5},
	{ .freq = 381012500, .constellation = 5},
	{ .freq = 387012500, .constellation = 5},
	{ .freq = 393012500, .constellation = 5},
	{ .freq = 399012500, .constellation = 5},
	{ .freq = 405000000, .constellation = 5},
	{ .freq = 411000000, .constellation = 5},
	{ .freq = 417000000, .constellation = 5},
	{ .freq = 423000000, .constellation = 5},
	{ .freq = 429000000, .constellation = 5},
	{ .freq = 435000000, .constellation = 5},
	{ .freq = 441000000, .constellation = 5},
	{ .freq = 447000000, .constellation = 5},
	{ .freq = 453000000, .constellation = 5},
	{ .freq = 459000000, .constellation = 5},
	{ .freq = 465000000, .constellation = 5},
	{ .freq = 471000000, .constellation = 5},
	{ .freq = 477000000, .constellation = 5},
	{ .freq = 483000000, .constellation = 5},
	{ .freq = 489000000, .constellation = 5},
	{ .freq = 495000000, .constellation = 5},
	{ .freq = 501000000, .constellation = 5},
	{ .freq = 507000000, .constellation = 5},
	{ .freq = 513000000, .constellation = 5},
	{ .freq = 519000000, .constellation = 5},
	{ .freq = 525000000, .constellation = 5},
	{ .freq = 531000000, .constellation = 5},
	{ .freq = 537000000, .constellation = 5},
	{ .freq = 543000000, .constellation = 5},
	{ .freq = 549000000, .constellation = 5},
	{ .freq = 555000000, .constellation = 5},
	{ .freq = 561000000, .constellation = 5},
	{ .freq = 567000000, .constellation = 5},
	{ .freq = 573000000, .constellation = 5},
	{ .freq = 579000000, .constellation = 5},
	{ .freq = 585000000, .constellation = 5},
	{ .freq = 591000000, .constellation = 5},
	{ .freq = 597000000, .constellation = 5},
	{ .freq = 603000000, .constellation = 5},
	{ .freq = 609000000, .constellation = 5},
	{ .freq = 615000000, .constellation = 5},
	{ .freq = 621000000, .constellation = 5},
	{ .freq = 627000000, .constellation = 5},
	{ .freq = 633000000, .constellation = 5},
	{ .freq = 639000000, .constellation = 5},
	{ .freq = 645000000, .constellation = 5},
	{ .freq = 93000000, .constellation = 5},
	{ .freq = 99000000, .constellation = 5},
	{ .freq = 105000000, .constellation = 5},
	{ .freq = 111025000, .constellation = 5},
	{ .freq = 117025000, .constellation = 5},
	{ .freq = 651000000, .constellation = 5},
	{ .freq = 657000000, .constellation = 5},
	{ .freq = 663000000, .constellation = 5},
	{ .freq = 669000000, .constellation = 5},
	{ .freq = 675000000, .constellation = 5},
	{ .freq = 681000000, .constellation = 5},
	{ .freq = 687000000, .constellation = 5},
	{ .freq = 693000000, .constellation = 5},
	{ .freq = 699000000, .constellation = 5},
	{ .freq = 705000000, .constellation = 5},
	{ .freq = 711000000, .constellation = 5},
	{ .freq = 717000000, .constellation = 5},
	{ .freq = 723000000, .constellation = 5},
	{ .freq = 729000000, .constellation = 5},
	{ .freq = 735000000, .constellation = 5},
	{ .freq = 741000000, .constellation = 5},
	{ .freq = 747000000, .constellation = 5},
	{ .freq = 753000000, .constellation = 5},
	{ .freq = 759000000, .constellation = 5},
	{ .freq = 765000000, .constellation = 5},
	{ .freq = 771000000, .constellation = 5},
	{ .freq = 777000000, .constellation = 5},
	{ .freq = 783000000, .constellation = 5},
	{ .freq = 789000000, .constellation = 5},
	{ .freq = 795000000, .constellation = 5},
	{ .freq = 801000000, .constellation = 5},
};

static const struct mux muxes_ATSC_us_CA_SF_Bay_Area[] = {
	{ .freq = 503000000, .constellation = 7},
	{ .freq = 551000000, .constellation = 7},
	{ .freq = 563000000, .constellation = 7},
	{ .freq = 593000000, .constellation = 7},
	{ .freq = 623000000, .constellation = 7},
	{ .freq = 635000000, .constellation = 7},
	{ .freq = 647000000, .constellation = 7},
	{ .freq = 659000000, .constellation = 7},
	{ .freq = 683000000, .constellation = 7},
	{ .freq = 689000000, .constellation = 7},
	{ .freq = 701000000, .constellation = 7},
	{ .freq = 725000000, .constellation = 7},
	{ .freq = 731000000, .constellation = 7},
};

static const struct mux muxes_ATSC_us_ID_Boise[] = {
	{ .freq = 195000000, .constellation = 7},
	{ .freq = 213000000, .constellation = 7},
	{ .freq = 515000000, .constellation = 7},
	{ .freq = 533000000, .constellation = 7},
	{ .freq = 545000000, .constellation = 7},
	{ .freq = 557000000, .constellation = 7},
};

static const struct mux muxes_ATSC_us_MA_Boston[] = {
	{ .freq = 503000000, .constellation = 7},
	{ .freq = 509000000, .constellation = 7},
	{ .freq = 527000000, .constellation = 7},
	{ .freq = 563000000, .constellation = 7},
	{ .freq = 569000000, .constellation = 7},
	{ .freq = 575000000, .constellation = 7},
	{ .freq = 581000000, .constellation = 7},
	{ .freq = 623000000, .constellation = 7},
	{ .freq = 635000000, .constellation = 7},
	{ .freq = 641000000, .constellation = 7},
	{ .freq = 647000000, .constellation = 7},
};

static const struct mux muxes_ATSC_us_MI_Lansing[] = {
	{ .freq = 617000000, .constellation = 7},
	{ .freq = 695000000, .constellation = 7},
	{ .freq = 719000000, .constellation = 7},
	{ .freq = 731000000, .constellation = 7},
	{ .freq = 743000000, .constellation = 7},
};

static const struct mux muxes_ATSC_us_NTSC_center_frequencies_8VSB[] = {
	{ .freq = 57000000, .constellation = 7},
	{ .freq = 63000000, .constellation = 7},
	{ .freq = 69000000, .constellation = 7},
	{ .freq = 79000000, .constellation = 7},
	{ .freq = 85000000, .constellation = 7},
	{ .freq = 177000000, .constellation = 7},
	{ .freq = 183000000, .constellation = 7},
	{ .freq = 189000000, .constellation = 7},
	{ .freq = 195000000, .constellation = 7},
	{ .freq = 201000000, .constellation = 7},
	{ .freq = 207000000, .constellation = 7},
	{ .freq = 213000000, .constellation = 7},
	{ .freq = 473000000, .constellation = 7},
	{ .freq = 479000000, .constellation = 7},
	{ .freq = 485000000, .constellation = 7},
	{ .freq = 491000000, .constellation = 7},
	{ .freq = 497000000, .constellation = 7},
	{ .freq = 503000000, .constellation = 7},
	{ .freq = 509000000, .constellation = 7},
	{ .freq = 515000000, .constellation = 7},
	{ .freq = 521000000, .constellation = 7},
	{ .freq = 527000000, .constellation = 7},
	{ .freq = 533000000, .constellation = 7},
	{ .freq = 539000000, .constellation = 7},
	{ .freq = 545000000, .constellation = 7},
	{ .freq = 551000000, .constellation = 7},
	{ .freq = 557000000, .constellation = 7},
	{ .freq = 563000000, .constellation = 7},
	{ .freq = 569000000, .constellation = 7},
	{ .freq = 575000000, .constellation = 7},
	{ .freq = 581000000, .constellation = 7},
	{ .freq = 587000000, .constellation = 7},
	{ .freq = 593000000, .constellation = 7},
	{ .freq = 599000000, .constellation = 7},
	{ .freq = 605000000, .constellation = 7},
	{ .freq = 611000000, .constellation = 7},
	{ .freq = 617000000, .constellation = 7},
	{ .freq = 623000000, .constellation = 7},
	{ .freq = 629000000, .constellation = 7},
	{ .freq = 635000000, .constellation = 7},
	{ .freq = 641000000, .constellation = 7},
	{ .freq = 647000000, .constellation = 7},
	{ .freq = 653000000, .constellation = 7},
	{ .freq = 659000000, .constellation = 7},
	{ .freq = 665000000, .constellation = 7},
	{ .freq = 671000000, .constellation = 7},
	{ .freq = 677000000, .constellation = 7},
	{ .freq = 683000000, .constellation = 7},
	{ .freq = 689000000, .constellation = 7},
	{ .freq = 695000000, .constellation = 7},
	{ .freq = 701000000, .constellation = 7},
	{ .freq = 707000000, .constellation = 7},
	{ .freq = 713000000, .constellation = 7},
	{ .freq = 719000000, .constellation = 7},
	{ .freq = 725000000, .constellation = 7},
	{ .freq = 731000000, .constellation = 7},
	{ .freq = 737000000, .constellation = 7},
	{ .freq = 743000000, .constellation = 7},
	{ .freq = 749000000, .constellation = 7},
	{ .freq = 755000000, .constellation = 7},
	{ .freq = 761000000, .constellation = 7},
	{ .freq = 767000000, .constellation = 7},
	{ .freq = 773000000, .constellation = 7},
	{ .freq = 779000000, .constellation = 7},
	{ .freq = 785000000, .constellation = 7},
	{ .freq = 791000000, .constellation = 7},
	{ .freq = 797000000, .constellation = 7},
	{ .freq = 803000000, .constellation = 7},
};

static const struct mux muxes_ATSC_us_PA_Philadelphia[] = {
	{ .freq = 575000000, .constellation = 7},
	{ .freq = 581000000, .constellation = 7},
	{ .freq = 593000000, .constellation = 7},
	{ .freq = 647000000, .constellation = 7},
	{ .freq = 713000000, .constellation = 7},
	{ .freq = 773000000, .constellation = 7},
	{ .freq = 785000000, .constellation = 7},
	{ .freq = 791000000, .constellation = 7},
};

static const struct network networks_ATSC_ca[] = {
	{
		.name = "AB-Calgary",
		.muxes = muxes_ATSC_ca_AB_Calgary,
		.nmuxes = sizeof(muxes_ATSC_ca_AB_Calgary) / sizeof(struct mux),
	},
};

static const struct network networks_ATSC_us[] = {
	{
		.name = "ATSC-center-frequencies-8VSB",
		.muxes = muxes_ATSC_us_ATSC_center_frequencies_8VSB,
		.nmuxes = sizeof(muxes_ATSC_us_ATSC_center_frequencies_8VSB) / sizeof(struct mux),
	},
	{
		.name = "CA-SF-Bay-Area",
		.muxes = muxes_ATSC_us_CA_SF_Bay_Area,
		.nmuxes = sizeof(muxes_ATSC_us_CA_SF_Bay_Area) / sizeof(struct mux),
	},
	{
		.name = "Cable-EIA-542-HRC-center-frequencies-QAM256",
		.muxes = muxes_ATSC_us_Cable_EIA_542_HRC_center_frequencies_QAM256,
		.nmuxes = sizeof(muxes_ATSC_us_Cable_EIA_542_HRC_center_frequencies_QAM256) / sizeof(struct mux),
	},
	{
		.name = "Cable-EIA-542-IRC-center frequencies-QAM256",
		.muxes = muxes_ATSC_us_Cable_EIA_542_IRC_center_frequencies_QAM256,
		.nmuxes = sizeof(muxes_ATSC_us_Cable_EIA_542_IRC_center_frequencies_QAM256) / sizeof(struct mux),
	},
	{
		.name = "Cable-HRC-center-frequencies-QAM256",
		.muxes = muxes_ATSC_us_Cable_HRC_center_frequencies_QAM256,
		.nmuxes = sizeof(muxes_ATSC_us_Cable_HRC_center_frequencies_QAM256) / sizeof(struct mux),
	},
	{
		.name = "Cable-IRC-center-frequencies-QAM256",
		.muxes = muxes_ATSC_us_Cable_IRC_center_frequencies_QAM256,
		.nmuxes = sizeof(muxes_ATSC_us_Cable_IRC_center_frequencies_QAM256) / sizeof(struct mux),
	},
	{
		.name = "Cable-Standard-center-frequencies-QAM256",
		.muxes = muxes_ATSC_us_Cable_Standard_center_frequencies_QAM256,
		.nmuxes = sizeof(muxes_ATSC_us_Cable_Standard_center_frequencies_QAM256) / sizeof(struct mux),
	},
	{
		.name = "ID-Boise",
		.muxes = muxes_ATSC_us_ID_Boise,
		.nmuxes = sizeof(muxes_ATSC_us_ID_Boise) / sizeof(struct mux),
	},
	{
		.name = "MA-Boston",
		.muxes = muxes_ATSC_us_MA_Boston,
		.nmuxes = sizeof(muxes_ATSC_us_MA_Boston) / sizeof(struct mux),
	},
	{
		.name = "MI-Lansing",
		.muxes = muxes_ATSC_us_MI_Lansing,
		.nmuxes = sizeof(muxes_ATSC_us_MI_Lansing) / sizeof(struct mux),
	},
	{
		.name = "NTSC-center-frequencies-8VSB",
		.muxes = muxes_ATSC_us_NTSC_center_frequencies_8VSB,
		.nmuxes = sizeof(muxes_ATSC_us_NTSC_center_frequencies_8VSB) / sizeof(struct mux),
	},
	{
		.name = "PA-Philadelphia",
		.muxes = muxes_ATSC_us_PA_Philadelphia,
		.nmuxes = sizeof(muxes_ATSC_us_PA_Philadelphia) / sizeof(struct mux),
	},
};

static const struct region regions_ATSC[] = {
	{
		.name = "Canada",
		.networks = networks_ATSC_ca,
		.nnetworks = sizeof(networks_ATSC_ca) / sizeof(struct network),
	},
	{
		.name = "United States",
		.networks = networks_ATSC_us,
		.nnetworks = sizeof(networks_ATSC_us) / sizeof(struct network),
	},
};

