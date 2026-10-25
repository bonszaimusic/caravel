/* SPDX-License-Identifier: GPL-3.0-only */
#define HP_MAGIC 0x314D5048u
typedef struct {
 uint8_t on,a,b,motion;int16_t base;uint16_t pad;
 int16_t lane[NSTEP];uint8_t mod[NTRK][NMSLOT];
} hp_pair_store_t;
typedef struct {
 uint32_t magic,version,size,seq,crc;uint32_t reserved[3];
 project_store_t native; hp_model_t model;hp_pair_store_t pair;
} hp_store_t;

_Static_assert(sizeof(hp_store_t)==10000u,"HPM1 interchange layout changed");
