/* SPDX-License-Identifier: GPL-3.0-only
 * STASIS 2.0: grains scatter across the REGION window with MOTN (real host DSP, frozen and live).
 * Build with REVERB_MINIMAL, STASIS_EXPAND, STASIS2. */
#if !CARAVEL_STASIS2 || !CARAVEL_STASIS_EXPAND || !CARAVEL_REVERB_MINIMAL
#error "needs STASIS2, expanded capture and minimal reverb"
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>
#include <math.h>
typedef struct { double lo[2], hi[2]; uint32_t starts; stasis_region_t region; } res_t;
static res_t run(int motn,int frozen,int len,int pos)
{
    host_tracks_init();
    memset(&fx,0,sizeof fx);memset(&hl,0,sizeof hl);memset(&st,0,sizeof st);memset(dly_buf,0,sizeof dly_buf);rev_clear();
    st_expand.grain=2;st_expand.quality=0;st_expand.scan=pos;st_expand.reverse=0;st_expand.length=len;
    song.g[G_RTYPE]=3;song.g[G_DRLVL]=0;song.g[G_RSIZE]=100;song.g[G_DRREV]=motn;
    uint32_t p=0,n=ST_RING_LEN*8/CTL,frz=ST_RING_LEN*3/CTL,last[2]={~0u,~0u};
    res_t r={{1e9,1e9},{0,0},0,{0}};
    for(uint32_t b=0;b<n;b++){
        int32_t zero[CTL]={0},bus[CTL],wet[CTL];
        if(frozen&&b==frz)song.g[G_DRLVL]=1;
        for(uint32_t j=0;j<CTL;j++){bus[j]=(int32_t)(12000.0*sin(2*3.14159265*(110.0+30.0*sin(p/20000.0))*p/44100.0));p++;wet[j]=0;}
        fx_buses(zero,zero,bus,wet,CTL);
        if(b>frz+ST_RING_LEN/CTL){   /* motion smoothing has settled */
            for(int h=0;h<2;h++) if(st.gr_l[h]!=last[h]){
                last[h]=st.gr_l[h];r.starts++;
                double age=(double)((st.w-st.gr_l[h])&ST_RING_MASK);
                if(age<r.lo[h])r.lo[h]=age;if(age>r.hi[h])r.hi[h]=age;
            }
        }
    }
    r.region.young=st.region_young;r.region.effective_length=st.region_length;
    return r;
}
int main(void)
{
    res_t a=run(0,1,64,0), b=run(45,1,64,0), c=run(127,1,64,0), d=run(127,1,127,0), e=run(127,1,16,0), f=run(127,0,64,0);
    double w0=a.hi[0]-a.lo[0], w45=b.hi[0]-b.lo[0], w127=c.hi[0]-c.lo[0], wlong=d.hi[0]-d.lo[0], wshort=e.hi[0]-e.lo[0], wlive=f.hi[0]-f.lo[0];
    double win=(double)c.region.effective_length;
    fprintf(stderr,"spread (frames) head0: MOTN0 %.0f | 45 %.0f | 127 %.0f | 127 LENGTH127 %.0f | LENGTH16 %.0f | live 127 %.0f ; window %.0f ; heads differ: %.0f/%.0f\\n",
            w0,w45,w127,wlong,wshort,wlive,win,c.lo[0],c.lo[1]);
    assert(a.starts>0 && w0<64.0);                       /* MOTN 0: frozen, the same spot (a rounding frame at most) */
    assert(w45>w0 && w127>w45);                          /* spread grows with MOTN */
    assert(w127>0.5*win);                                /* MOTN 127 covers more than half of the window */
    assert(wlong>w127 && wshort<w127);                   /* LENGTH is now the width of the scatter */
    assert(wlive>0.3*win);                               /* live too */
    assert(c.starts>=20 && c.hi[1]-c.lo[1]>0.4*win);    /* head 1 scatters on its own */
    res_t g=run(127,1,64,0);
    assert(g.lo[0]==c.lo[0] && g.hi[0]==c.hi[0] && g.starts==c.starts);   /* deterministic */
    puts("stasis grain wander: MOTN 0 fixed, 45 small, 127 over most of the window; LENGTH is the scatter width, live and frozen");
    return 0;
}
