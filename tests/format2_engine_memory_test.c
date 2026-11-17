/* SPDX-License-Identifier: GPL-3.0-only */
/* Switching from every other retained engine must erase arbitrary predecessor
 * memory before the destination sounds, without touching another track. */
#include <assert.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
static uint64_t render(uint32_t from,uint32_t to,uint32_t preset,uint32_t poison) {
 int fd[2];assert(!pipe(fd));pid_t child=fork();assert(child>=0);
 if(!child) {
  close(fd[0]);host_tracks_init();track_t *t=&trk[0];
  host_preset(t,from,0);memset(ENG_MEM(0),poison,ENG_MEM_PART);memset(ENG_MEM(1),0xC7,ENG_MEM_PART);
  host_preset_req(t,to,preset);engine_block(t);assert(t->engine==to);
  t->p[P_VOICE]=V_POLY;t->p[P_AMODE]=0;
  for(uint32_t n=0;n<3;n++)trk_note_on(t,48+n*7,100);
  uint64_t hash=0xCBF29CE484222325ull;int32_t out[CTL];
  for(uint32_t b=0;b<128;b++) {
   engine_block(t);track_render(t,out,CTL);
   for(uint32_t i=0;i<CTL;i++){hash^=(uint32_t)out[i];hash*=0x100000001B3ull;}
  }
  for(uint32_t i=0;i<ENG_MEM_PART;i++)assert(((uint8_t*)ENG_MEM(1))[i]==0xC7);
  assert(write(fd[1],&hash,sizeof hash)==sizeof hash);_exit(0);
 }
 close(fd[1]);uint64_t hash=0;ssize_t n=read(fd[0],&hash,sizeof hash);close(fd[0]);int status=0;assert(waitpid(child,&status,0)==child);
 assert(n==sizeof hash&&WIFEXITED(status)&&!WEXITSTATUS(status));return hash;
}
int main(void) {
 uint32_t count=0;
 for(uint32_t a=0;a<6;a++)for(uint32_t b=0;b<6;b++)if(a!=b) {
  uint32_t from=eng_unstore(a),to=eng_unstore(b);
  for(uint32_t p=0;p<ENGINES[to]->npresets;p++) {
   assert(render(from,to,p,0)==render(from,to,p,0xA5));count++;
  }
 }
 printf("FORMAT 2 engine memory: %u predecessor/preset transitions, arbitrary previous memory cannot change sound or adjacent track storage\n",count);
}
