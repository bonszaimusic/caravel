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

/* v2 keeps the native project at byte 32 and compresses the unchanged HOME
 * codec plus all four parameter snapshots. v1/10,000-byte inputs stay readable.
 * No snapshot data is appended to the native flash project or audio pool. */
#define HP_SNAPSHOT_STORE (CARAVEL_ACTION_BANKS && FELUCCA_RANDOM_MORPH)
#define HP_V2_META 3328u
#define HP_V2_SIZE (32u+sizeof(project_store_t)+HP_V2_META)
#if HP_SNAPSHOT_STORE
#define HP_CAPTURE_BYTES HP_V2_SIZE
#else
#define HP_CAPTURE_BYTES sizeof(hp_store_t)
#endif
