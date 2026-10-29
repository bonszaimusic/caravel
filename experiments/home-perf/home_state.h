/* SPDX-License-Identifier: GPL-3.0-only */
/* Experimental HOME model. Never consumes a native P_* or motion ID. */
#if !defined(FELUCCA_WEB) && !defined(PAIR_TEST) && !FELUCCA_PERF_HW
#error HOME performance prototype is emulator only
#endif
#define HP_ROWS 32u
#define HP_EVENTS 128u
#define HP_VFIRST 2048u
enum {HP_NONE, HP_TRACK, HP_GLOBAL, HP_LFO2, HP_DX, HP_PAIR, HP_MACRO, HP_FM6};
typedef struct { uint16_t id, page; uint8_t kind, track, engine, aux; } hp_ref_t;
typedef struct { hp_ref_t ref; int16_t amount; uint16_t pad; } hp_route_t;
typedef struct { int16_t base; uint16_t pad; hp_route_t route[4]; } hp_macro_t;
typedef struct { uint8_t count, active, pad[2]; /* pad[0]: optional user-preset origin, 0 unknown; codec high bits. */ hp_ref_t cell[HP_ROWS][4]; } hp_track_t;
typedef struct { uint8_t track, step, control, lock; int16_t value; uint16_t pad; } hp_event_t;
typedef struct { hp_track_t track[NTRK]; hp_macro_t macro[NTRK][4]; uint16_t events, pad; hp_event_t event[HP_EVENTS]; int16_t pair_base; uint8_t pair_motion,pair_valid; } hp_model_t;
static hp_model_t hp;
static hp_ref_t hp_last;
static uint8_t hp_last_valid, hp_last_track, hp_default=1, hp_macro_sel, hp_route_sel;
static uint8_t hp_edit_row, hp_edit_col, hp_edit_action, hp_apply_armed;
static uint8_t hp_edit_track=255, hp_backup_track=255;
static hp_track_t hp_edit_backup;
/* Four macros with four routes each touch at most sixteen parameters. */
#define HP_MACRO_TARGETS 16u
_Static_assert(P_COUNT <= 256, "Macro IDs require more than one byte");
static int16_t hp_macro_live[NTRK][4], hp_macro_keep[NTRK][HP_MACRO_TARGETS];
static uint8_t hp_macro_ids[NTRK][HP_MACRO_TARGETS], hp_macro_count[NTRK];
static uint8_t hp_dirty,hp_combo_pending;
static int hp_virtual(uint32_t id) { return id >= HP_VFIRST && id < HP_VFIRST+8u; }
static void hp_begin(track_t *t);
static void hp_end(track_t *t);
static void hp_motion_step(track_t *t, uint32_t step);
static void hp_motion_stop(void);
static void hp_midi(track_t *t, uint32_t id, uint32_t value);
static const param_desc_t *hp_vdesc(uint32_t id);
static int hp_project_save(uint32_t slot, const char *name);
static int hp_project_load(uint32_t slot);
#if FELUCCA_PERF_HW
static int hp_project_rename(uint32_t slot,const char*name);
#endif
static void hp_autosave_poll(void);
static int hp_autosave_boot(int allowed);
static void hp_init(void);
static void hp_apply(uint32_t track, uint32_t set);
static void hp_apply_template(uint32_t track, uint32_t set);
static void hp_open_editor(void);
static void hp_add_last(void);
static void hp_touch(uint32_t slot);
static void hp_draw_home(void);
static void hp_page_cards(void);
static void hp_page_knob(uint32_t slot, int32_t steps);
static void hp_turn(uint32_t slot, int32_t steps, int momentary);
static void hp_row_turn(int32_t steps);
static uint32_t hp_lock_id(uint32_t slot);
static void hp_lock_turn(uint32_t slot, int32_t steps, uint64_t held);
static void hp_editor_apply(void);
static void hp_source(void);
static void hp_page_graph(void);
static void hp_editor_sync(void);
static int hp_oct_button(uint32_t button);

static int hp_forget(uint32_t slot);

static void hp_motion_clear(track_t*t);

static int hp_sound_changed(const track_t *t);

#if FELUCCA_RANDOM_MORPH
/* Volatile HOME Random Morph state shared by the menu and performance UI. */
#define HP_RM_MAX 128u
#define HP_RM_IDLE 0u
#define HP_RM_RANDOM 1u
#define HP_RM_FROZEN 2u
#define HP_RM_RETURN 3u
#define HP_RM_DONE 4u
#define HP_RM_RECALL 5u
#define HP_RM_ROW 0u
#define HP_RM_TRACK 1u
#define HP_RM_ALL 2u
#define HP_RM_LINEAR 0u
#define HP_RM_SMOOTH 1u
#define HP_RM_DRIFT 2u

typedef struct {
 hp_ref_t ref;
 int16_t original,from,to,last;
 uint8_t released; /* manual edit takes permanent priority for this morph */
} hp_rm_cell_t;
typedef struct {
 hp_rm_cell_t cell[HP_RM_MAX];
 uint32_t count,phase,stamp,seed;
 uint8_t state,bars,depth,scope,shape,origin_track,origin_row,frozen_from;
} hp_rm_state_t;
static hp_rm_state_t hp_rm = {.bars=8,.depth=35,.scope=HP_RM_TRACK,.shape=HP_RM_SMOOTH,.seed=0x8C89D39Bu};
static const uint8_t hp_rm_times[]={1,2,4,8,16,32,64};
static const uint8_t hp_rm_depths[]={10,25,35,50,75,100};
#if CARAVEL_ACTION_BANKS
/* Volatile four-slot snapshot memory. Store refs/values only: no sound,
 * project, FX audio or duplicate synth parameter stores. No heap use. */
#define HP_RM_SNAP_SLOTS 4u
typedef struct {
 uint16_t count;
 uint8_t scope,origin_track,origin_row,pad;
 /* Snapshot recall never uses source-page metadata. Three bytes losslessly
  * encode every supported pinned target; separate values avoid struct padding.
  * Keep all four slots and all 128 targets while retaining target RAM reserves. */
 uint8_t refs[HP_RM_MAX][3];
 int16_t values[HP_RM_MAX];
} hp_rm_snap_t;
static hp_rm_snap_t hp_rm_snap[HP_RM_SNAP_SLOTS];
#endif


static void hp_rm_reset(void);
static void hp_rm_freeze(void);

#endif /* FELUCCA_RANDOM_MORPH */
