/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS 2.0 level: a steady REV send through the plain HALL and through STASIS (live and frozen),
 * checking the make-up, the HALL curve and that HALL 0 is still usable. Host DSP, no device or listening claim.
 * Build with REVERB_MINIMAL, STASIS_EXPAND, STASIS2. */
#if !CARAVEL_STASIS2 || !CARAVEL_STASIS_EXPAND || !CARAVEL_REVERB_MINIMAL
#error "needs STASIS2, expanded capture and minimal reverb"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
/* Same REV send (sine 220 Hz, RMS ~8485) through each reverb type; compare output level to send level. */
static double run(int rtype,int str,int hall,int frozen){
    host_tracks_init();
    memset(&fx,0,sizeof fx);memset(&hl,0,sizeof hl);memset(&st,0,sizeof st);memset(dly_buf,0,sizeof dly_buf);rev_clear();
    st_expand.grain=2;st_expand.quality=0;st_expand.scan=0;st_expand.reverse=0;st_expand.length=64;
    song.g[G_RTYPE]=rtype;song.g[G_DRLVL]=0;song.g[G_RSIZE]=str;song.g[G_DRREV]=75;
    st_hall_ret=hall;st_hall_feed=64;
    double e=0;uint64_t n=0;uint32_t p=0;
    uint32_t total=(ST_RING_LEN*4)/CTL, frz=(ST_RING_LEN*2)/CTL;
    for(uint32_t b=0;b<total;b++){
        int32_t zero[CTL]={0},bus[CTL],wet[CTL];
        if(frozen&&b==frz)song.g[G_DRLVL]=1;
        for(uint32_t j=0;j<CTL;j++){bus[j]=(frozen&&b>=frz)?0:(int32_t)(12000.0*sin(2*3.14159265*220.0*(p++)/44100.0));wet[j]=0;}
        fx_buses(zero,zero,bus,wet,CTL);
        if(b>frz+ST_RING_LEN/CTL/2)for(uint32_t j=0;j<CTL;j++){double l=wet[j]+rev_side[j],r=wet[j]-rev_side[j];e+=l*l+r*r;n+=2;}
    }
    return 10*log10(e/n+1e-9)-10*log10(12000.0*12000.0/2);
}
#include <assert.h>
int main(void){
    double plain=run(2,100,64,0);
    double lv=run(3,100,64,0), fz=run(3,100,64,1);
    double lv0=run(3,100,0,0), fz0=run(3,100,0,1);
    double fz32=run(3,100,32,1), fz127=run(3,100,127,1);
    fprintf(stderr,"plain HALL %+.1f | live: default %+.1f, HALL 0 %+.1f | frozen: default %+.1f, HALL 0 %+.1f, 32 %+.1f, 127 %+.1f dB\n",plain,lv,lv0,fz,fz0,fz32,fz127);
    assert(lv-plain>1.0 && lv-plain<6.0);               /* STASIS stays a little above the plain HALL by default */
    assert(fz127-fz>7.5 && fz127-fz<10.5);              /* HALL 127 = x3 = +9.5 dB */
    assert(fz-fz32>3.0 && fz-fz32<8.0);                 /* HALL 32 = half the hall share */
    assert(fz-fz0<11.0 && lv-lv0<16.0);                /* HALL 0: the direct grains alone are no longer ~20 dB down */
    assert(fz0>plain-10.0);                             /* ... and within a few dB of a plain HALL */
    assert(run(3,100,64,1)==fz);                        /* deterministic */
    puts("stasis level: default a little over the plain HALL, HALL 127 +9.5 dB, HALL 0 (direct grains) within 11 dB when frozen");
    return 0;
}
