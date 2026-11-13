/* SPDX-License-Identifier: GPL-3.0-only */
/* Source-level standalone graphics geometry checks (not a firmware LCD test). */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../experiments/home-perf/home_elastic_visual.h"

static void test_rest_and_direction(void)
{
    assert(hp_evis_shift(0,18*256)==0);
    assert(hp_evis_shift(1,18*256)==1);
    assert(hp_evis_shift(-1,18*256)==-1);
    assert(hp_evis_shift(9*256,18*256)==2);
    assert(hp_evis_shift(-18*256,18*256)==-3);
    assert(hp_evis_shift(100000,18*256)==3);
    assert(hp_evis_shift(-100000,18*256)==-3);
    assert(hp_evis_shift(10,0)==0);
    /* Unsigned Q8 magnitude must handle the signed minimum precisely. */
    assert(hp_evis_shift(INT32_MIN,18*256)==-3);
    assert(hp_evis_shift(INT32_MAX,18*256)==3);
}
static void test_no_card_overflow(void)
{
    /* Mini-coil occupies x+7..x+27, y+39..y+47 in ACTION cards,
       y+59..y+67 in the tall HOME cards; never crosses the gauge/border. */
    for(int32_t shift=-3;shift<=3;shift++){
        int32_t span=hp_evis_span(shift);
        assert(span>=14 && span<=20);
        for(uint32_t k=0;k<=6;k++){
            int32_t x=7+(span*(int32_t)k)/6;
            int32_t compact=43+shift+hp_evis_wave_y(0,0,k);
            int32_t tall=63+shift+hp_evis_wave_y(0,0,k);
            assert(x>=7 && x<=27);
            assert(compact>=39 && compact<=47);
            assert(tall>=59 && tall<=67);
        }
    }
}
static void test_bounded_arbitrary_motion(void)
{
    uint32_t seed=0x45A89A21u;
    for(uint32_t i=0;i<100000;i++){
        seed=seed*1664525u+1013904223u;
        int32_t q8=(int32_t)(seed%65537u)-32768;
        int32_t span=(int32_t)(seed/1024u%32768u)+1;
        int32_t shift=hp_evis_shift(q8,span);
        assert(shift>=-3 && shift<=3);
        assert((shift>0)==(q8>0) || !shift);
        assert(hp_evis_span(shift)>=14 && hp_evis_span(shift)<=20);
    }
}
int main(void)
{
    test_rest_and_direction();
    test_no_card_overflow();
    test_bounded_arbitrary_motion();
    puts("home_elastic_visual_test: PASS (3 groups, 100k motion samples)");
    return 0;
}
