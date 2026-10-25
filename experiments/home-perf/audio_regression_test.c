/* SPDX-License-Identifier: GPL-3.0-only */
/* Compare stdout between feature-off, V4 and corrected builds. Real keyboard/
 * MIDI paths; host timing does not measure the FM-1 deadline. */
#define FELUCCA_CPU2 0
#include "../../web/emu/felucca_web.c"
#include <math.h>
static unsigned checks;
#define REQUIRE(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL line %u: %s\n",__LINE__,#x); exit(1); } } while (0)
static uint64_t digest=1469598103934665603ull;
static void advance(unsigned ms,int capture)
{
 while(ms--) {
  web_render(44);
  if(capture)for(unsigned i=0;i<44;i++) {
   REQUIRE(isfinite(out_l[i])&&isfinite(out_r[i]));
   uint32_t bits[2];memcpy(&bits[0],&out_l[i],4);memcpy(&bits[1],&out_r[i],4);
   digest=(digest^bits[0])*1099511628211ull;digest=(digest^bits[1])*1099511628211ull;
  }
 }
}
int main(int argc,char **argv)
{
 int macros=argc>1;unsigned cases=0;
 web_nor_erase();web_master(700);web_boot();advance(1400,0);
 song.g[G_ROUTE]=ROUTE_TRACK;mch_set(0,0);mch_set(1,1);
 for(unsigned mix=0;mix<2;mix++)for(unsigned mode=0;mode<4;mode++)
 for(unsigned preset=0;preset<ENG_DRONE.npresets;preset++) {
  web_keys(0);web_midi(0xB1,123,0);advance(350,0);
  for(unsigned t=0;t<NPART;t++){trk_all_off(&trk[t]);memset(trk[t].v,0,sizeof trk[t].v);}
  memset(eng_mem_owner,0,sizeof eng_mem_owner);
  host_preset(&trk[0],ENGI_DRONE,preset);host_preset(&trk[1],ENGI_HADES,preset%ENG_HADES.npresets);
  trk[0].p[P_VOICE]=mode;track_select(0);
#if FELUCCA_HOME_PERF
  REQUIRE(!ep.on);
  if(macros){
   REQUIRE(web_home_macro_route(0,0,0,P_E7,19));
   REQUIRE(web_home_macro_route(0,1,0,P_E7,-11));
   REQUIRE(web_home_macro_route(0,2,0,P_FILT,30));
   web_home_macro_set(0,0,61);web_home_macro_set(0,1,97);web_home_macro_set(0,2,33);
  }
#else
  REQUIRE(!macros);
#endif
  advance(25,0);digest=1469598103934665603ull;
  if(mix) {
   web_midi(0x91,36+preset%12,100);advance(4,0);
   unsigned actual=0;for(unsigned v=0;v<NVOICE;v++) actual+=trk[1].v[v].active&&trk[1].v[v].gate&&trk[1].v[v].note==36+preset%12;
   REQUIRE(actual>0);
  }
  web_keys(1u);advance(180,1);
  REQUIRE(drone_state(&trk[0])->held[53/32]&(1u<<(53%32)));
  web_keys(1u|(1u<<12));advance(180,1);
  REQUIRE(drone_state(&trk[0])->held[65/32]&(1u<<(65%32)));
  web_keys(1u<<12);advance(100,1);
  REQUIRE(!(drone_state(&trk[0])->held[53/32]&(1u<<(53%32))));
  web_keys(0);advance(100,1);
  REQUIRE(!(drone_state(&trk[0])->held[65/32]&(1u<<(65%32))));
  if(mix)web_midi(0x81,36+preset%12,0);
  web_keys(1u<<4);advance(160,1);web_keys(0);advance(160,1);
  printf("mix=%u mode=%u drone=%u hash=%016llx\n",mix,mode,preset,(unsigned long long)digest);cases++;
 }
 fprintf(stderr,"audio regression: %u cases, %u checks; macros=%d\n",cases,checks,macros);return 0;
}
