/* SPDX-License-Identifier: GPL-3.0-only */
/* ED 91: request op, nonce [,seconds]. Reply op, rc, nonce, word-count, LE 7-bit u32 words.
 * op 0 snapshot, 1 start (1..60 seconds), 2 stop. Audio-boundary acknowledgement.
 * rc 0 accepted, 1 malformed, 2 already running. No flash access or musical mutation.
 */
static uint32_t pd_wire[PD_WORDS] __attribute__((section(".pool")));
static uint32_t pd_bytes(int32_t a,int32_t b,int32_t c,int32_t d)
{return ((uint32_t)a&255u)|(((uint32_t)b&255u)<<8)|(((uint32_t)c&255u)<<16)|(((uint32_t)d&255u)<<24);}
static void pd_snapshot(void)
{
    uint32_t irq=perf_irq_save(),i=16;
    pd_wire[0]=PD_MAGIC;pd_wire[1]=1;pd_wire[2]=PD_REAL_CLOCK;
    pd_wire[3]=pd.state|(pd_request?0x100u:0);pd_wire[4]=pd.session;pd_wire[5]=pd.seconds;
    pd_wire[6]=pd.last_ms-pd.start_ms;pd_wire[7]=pd.halves;pd_wire[8]=pd.total;pd_wire[9]=pd.maximum;
    pd_wire[10]=pd.nested;pd_wire[11]=pd.nested_max;pd_wire[12]=pd.late;pd_wire[13]=pd.shed;pd_wire[14]=pd.over85;pd_wire[15]=PD_PERIOD;
    for(uint32_t e=0;e<3;e++){pd_wire[i++]=pd.engine[e].sum;pd_wire[i++]=pd.engine[e].max;pd_wire[i++]=pd.engine[e].calls;}
    for(uint32_t t=0;t<NTRK;t++){pd_wire[i++]=pd.track[t].sum;pd_wire[i++]=pd.track[t].max;}
    pd_wire[33]=song.cpu_q8;pd_wire[34]=felucca_dbg.late;pd_wire[35]=shed_count;pd_wire[36]=usb.resets;
    pd_wire[37]=song.sel;pd_wire[38]=ep.on?((1u<<ep.a)|(1u<<ep.b)):0;
    pd_wire[39]=ep.base;pd_wire[40]=ep.effective;pd_wire[41]=ep.mode;pd_wire[42]=ep.on?ep.cells:0;pd_wire[43]=pd.deadline;
    for(uint32_t t=0;t<NTRK;t++){
        const track_t *tr=&trk[t];const int16_t *p=tr->p;uint32_t a=0,g=0,f=0,k=44+t*18;
        for(uint32_t v=0;v<NVOICE;v++){a+=!!tr->v[v].active;g+=tr->v[v].active&&tr->v[v].gate;f+=tr->v[v].active&&tr->v[v].stage==4;}
        pd_wire[k++]=pd_bytes(tr->engine,tr->eng_req,tr->preset,p[P_VOICE]);pd_wire[k++]=pd_bytes(a,g,f,tr->nheld);
        pd_wire[k++]=pd_bytes(p[P_DIST],p[P_CHOR],p[P_DLY],p[P_REV]);
        pd_wire[k++]=pd_bytes(p[P_ITYPE],p[P_IMIX],0,0)|((uint32_t)(uint16_t)p[P_FILT]<<16);
        pd_wire[k++]=pd_bytes(p[P_LRATE],p[P_LWAVE],p[P_LFADE],p[P_LPOL]);
        const uint8_t *l=lfo2_config[t].p;pd_wire[k++]=pd_bytes(l[L2_RATE],l[L2_WAVE],l[L2_FADE],l[L2_POL]);
        pd_wire[k++]=pd_bytes(p[P_LD_PIT],p[P_LD_FLT],p[P_LD_SHP],p[P_LD_AMP]);
        for(uint32_t j=0;j<8;j+=2)pd_wire[k++]=(uint16_t)p[P_E0+j]|((uint32_t)(uint16_t)p[P_E0+j+1]<<16);
        pd_wire[k++]=pd_bytes(p[P_ATK],p[P_DEC],p[P_SUS],p[P_REL]);
        pd_wire[k++]=pd_bytes(song.playing?hp_macro_live[t][0]:hp.macro[t][0].base,song.playing?hp_macro_live[t][1]:hp.macro[t][1].base,song.playing?hp_macro_live[t][2]:hp.macro[t][2].base,song.playing?hp_macro_live[t][3]:hp.macro[t][3].base);
        for(uint32_t j=0;j<4;j++)pd_wire[k++]=pd_bytes(p[P_M1SRC+j*3],p[P_M1DST+j*3],p[P_M1AMT+j*3],0);
        pd_wire[k]=pd_bytes(p[P_AMODE],p[P_AHOLD],p[P_CHRD],p[P_VOIC]);
    }
    pd_wire[116]=pd.gap_max;pd_wire[117]=pd.gap_long;pd_wire[118]=24;pd_wire[119]=0;
#if FELUCCA_CPU2
    /* Reserved V1 word: active bit0, fault bits1..2, dispatched blocks bits3..31. */
    pd_wire[119]=(c1_blocks<<3)|((uint32_t)fm1_c1_gave_up<<1)|!!fm1_c1_on;
#endif
    perf_irq_restore(irq);
}
static int ed_diag_handle(uint32_t cmd,const uint8_t *a,uint32_t n)
{
    if(cmd!=91)return 0;
    uint32_t op=n?a[0]:127,token=n>1?a[1]:0,rc=1;
    if((op==0||op==2)&&n==2)rc=0;
    if(op==1&&n==3&&a[2]>=1&&a[2]<=60)rc=0;
    if(!rc){
        uint32_t irq=perf_irq_save();
        if(op==1){if(pd_active||pd_request)rc=2;else{pd_seconds=a[2];pd_request=1;}}
        if(op==2)pd_request=2;
        perf_irq_restore(irq);
    }
    ed_b(op);ed_b(rc);ed_b(token);ed_b(rc?0:PD_WORDS);
    if(!rc){pd_snapshot();for(uint32_t i=0;i<PD_WORDS;i++)ed_bk_u32(pd_wire[i]);}
    return 1;
}
