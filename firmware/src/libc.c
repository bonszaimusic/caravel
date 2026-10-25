/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The few libc routines the compiler may call implicitly (struct copies,
 * zeroing). Freestanding: no other libc. */
void *memset(void *d, int c, unsigned n)
{
    unsigned char *p = d;
    while (n--)
        *p++ = (unsigned char)c;
    return d;
}

void *memcpy(void *d, const void *s, unsigned n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}

int memcmp(const void *a, const void *b, unsigned n)
{
    const unsigned char *p = a, *q = b;
    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}

static int str_eq(const char *a, const char *b)
{
    while (*a && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static unsigned str_len(const char *s)
{
    unsigned n = 0;
    while (s[n])
        n++;
    return n;
}

static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) {
        d[i] = s[i];
        i++;
    }
    d[i] = 0;
}

/* decimal with an optional fixed point: fmt_fix(buf, -63, 1) -> "-6.3" */
static void fmt_fix(char *b, int32_t v, int decimals)
{
    char t[12];
    int n = 0, neg = v < 0, i;
    uint32_t u = (uint32_t)(neg ? -v : v);
    do {
        t[n++] = (char)('0' + u % 10u);
        u /= 10u;
        if (n == decimals)
            t[n++] = '.';
    } while (u || n <= decimals);
    if (neg)
        t[n++] = '-';
    for (i = 0; i < n; i++)
        b[i] = t[n - 1 - i];
    b[n] = 0;
}

static void fmt_int(char *b, int32_t v) { fmt_fix(b, v, 0); }

static uint32_t rng_state = 0x1234567u;
static uint32_t rng(void)
{
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

#if FELUCCA_PAIR_MORPH || FELUCCA_HOME_PERF
#include <stdarg.h>
/* The performance UI needs only strings and decimal integers, no hosted libc. */
static int perf_format(char *out,unsigned cap,const char *fmt,...) {
 va_list ap;va_start(ap,fmt);unsigned n=0;
 while(*fmt){
  const char *word;char num[12];
  if(*fmt!='%'){if(n+1<cap)out[n]=*fmt;n++;fmt++;continue;}
  fmt++;int plus=*fmt=='+';if(plus)fmt++;
  if(*fmt=='s'){word=va_arg(ap,const char*);fmt++;}
  else if(*fmt=='u'||*fmt=='d'){
   int sign=*fmt=='d';uint32_t v=sign?(uint32_t)va_arg(ap,int):va_arg(ap,unsigned);fmt++;
   unsigned len=0,start=0;int neg=sign&&(int32_t)v<0;if(neg)v=0u-v;
   if(neg||plus)num[start++]=neg?'-':'+';
   do{num[start+len++]=(char)('0'+v%10u);v/=10u;}while(v);
   for(unsigned i=0;i<len/2;i++){char c=num[start+i];num[start+i]=num[start+len-1-i];num[start+len-1-i]=c;}
   num[start+len]=0;word=num;
  }else{num[0]=*fmt?*fmt++:'%';num[1]=0;word=num;}
  while(*word){if(n+1<cap)out[n]=*word;n++;word++;}
 }
 va_end(ap);if(cap)out[n<cap?n:cap-1]=0;return (int)n;
}
#endif
