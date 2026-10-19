/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#define EDITOR_TEST_NO_MAIN 1
#include "editor_test.c"
int main(void)
{
    reset();
    uint8_t a[5] = {2,400u&127u,400u>>7,0,65}; /* RATE 128 clamps to 127 */
    uint32_t n = request(ED_L2_SET,a,5);
    assert(n>6 && host_wire[5]==0 && lfo2_get(2,400)==127 && lfo2_get(0,400)==0);
    n=request(ED_L2_DESC,a+1,2); assert(n>6 && host_wire[5]==0);
    n=request(ED_L2_GET,a,1); assert(n==22 && host_wire[5]==2 && host_wire[6]==1);
    a[1]=L2_FIRST&127; a[2]=L2_FIRST>>7;
    assert(request(ED_L2_SET,a,4) && lfo2_get(2,400)==127);
    a[0]=4; request(ED_L2_SET,a,5); assert(lfo2_get(2,400)==127);
    a[0]=2; a[1]=99; a[2]=5; request(ED_L2_SET,a,5); assert(lfo2_get(2,400)==127);
    a[0]=0; a[1]=20; a[2]=1; a[3]=L2_FIRST&127; a[4]=L2_FIRST>>7;
    n=request(ED_LEARN2_SET,a,5); assert(n>6 && (host_wire[5]==0 || host_wire[5]==3) && ml_cc_of(1,L2_FIRST)==21);
    puts("LFO2 editor protocol tests passed"); return 0;
}
