/* SPDX-License-Identifier: GPL-3.0-only
 * Shared note/grid helpers retained for the sequencer UI; no drum engine. */
enum { DV_KICK,DV_SNARE,DV_CLAP,DV_HATC,DV_HATO,DV_TOM,DV_RIM,DV_BELL };
static uint8_t drum_mute[NPART];
static const uint8_t DRUM_LANE_NOTE[NLANE] = {36, 38, 39, 42, 46, 45, 37, 56};

/* the lane a note strikes (KIT-proof: a kit plays its own piece inside the lane) */
static uint32_t drum_lane(uint32_t note)
{
    uint32_t n = note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u;
    static const uint8_t lane[47]={0,0,6,1,2,1,5,3,5,3,5,4,5,5,4,5,4,4,7,3,4,7,4,6,4,5,5,5,5,5,5,5,7,7,3,3,6,6,3,3,6,6,6,5,5,7,7};
    return lane[n-35u];
}

/* KIT's swaps of lanes 6..8 (1 TOM -> CONGA, 2 RIM -> CLAVE, 4 BELL -> CYM) */
static uint32_t drum_swaps(const track_t*t){(void)t;return 0;}
/* the lane's name as the track's KIT plays it (5 characters at most) */
static const char *drum_lane_name(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
    uint32_t kit = drum_swaps(t);
    l &= NLANE - 1u;
    if (l == DV_TOM && (kit & 1u))
        return "CONGA";
    if (l == DV_RIM && (kit & 2u))
        return "CLAVE";
    return (kit & 4u) && l == DV_BELL ? "CYM" : N[l];
}
/* .. in two letters, the drum machine way (the grid's lane column) */
static const char *drum_lane_abbr(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    uint32_t kit = drum_swaps(t);
    l &= NLANE - 1u;
    if (l == DV_TOM && (kit & 1u))
        return "CG";
    if (l == DV_RIM && (kit & 2u))
        return "CL";
    return (kit & 4u) && l == DV_BELL ? "CY" : N[l];
}

/* the lanes step s strikes: its hits, and its notes on their lanes (a NOTE step only) */
static uint32_t step_lanes(const step_t *s)
{
    uint32_t m = 0, i;
    if (s->time != ST_NOTE)
        return 0;
    for (i = 0; i < s->n && i < 4u; i++)
        m |= 1u << drum_lane(s->note[i]);
    return m | s->hit;
}

/* .. and which of them are accented */
static uint32_t step_accents(const step_t *s) { return s->flags & SF_ACCENT ? step_lanes(s) : s->acc & step_lanes(s); }

/* a step's notes that are a lane's note become that lane's hits (the same note, the same velocity: nothing
 * sounds different). Other notes (a low tom 41, a crash 49) stay notes, shown on their lane. A step accent
 * of a step left with hits only becomes the hits' accents. Pattern loads into a DRUM track, projects of
 * before the grid, the grid's edits */
static void step_to_grid(step_t *s)
{
    uint32_t i, k = 0;
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++) {
        uint32_t l = drum_lane(s->note[i]);
        if (s->note[i] == DRUM_LANE_NOTE[l])
            s->hit |= (uint8_t)(1u << l);
        else
            s->note[k++] = s->note[i];
    }
    for (i = k; i < 4u; i++)
        s->note[i] = 0;
    s->n = (uint8_t)k;
    if (!k && (s->flags & SF_ACCENT)) {
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
}

