#include "madx.h"

#include<stdint.h>

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795029
#endif

// -- MAD-NG RNG XorShift1024* ------------------------------------------------o

#define N 16

static struct {
  uint64_t s[N];
  int p, id;
} st_array[10], *st = st_array;

union numbit {
  uint64_t u;
  double   d;
};

static uint64_t mad_num_irand (void)
{
  int p = st->p;
  uint64_t *s = st->s;
  const uint64_t s0 = s[p];
  uint64_t s1 = s[p = (p + 1) & (N - 1)];
  s1 ^= s1 << 31; // A
  s[p] = s1 ^ s0 ^ (s1 >> 11) ^ (s0 >> 30); // B, C
  st->p = p;
  return s[p] * 1181783497276652981ULL; // number within [0,ULLONG_MAX]
}

static double mad_num_rand (void)
{
  uint64_t r = mad_num_irand();
  r = (r & 0x000fffffffffffffULL) | 0x3ff0000000000000ULL;
  const union numbit n = { .u = r }; // number within [1.,2.)
  return n.d - 1.0;                  // number within [0.,1.)
}

static void mad_num_randjump (void)
{
  static const uint64_t jump[N] = {
    0x84242f96eca9c41dULL, 0xa3c65b8776f96855ULL, 0x5b34a39f070b5837ULL,
    0x4489affce4f31a1eULL, 0x2ffeeb0a48316f40ULL, 0xdc2d9891fe68c022ULL,
    0x3659132bb12fea70ULL, 0xaac17d8efa43cab8ULL, 0xc4cb815590989b13ULL,
    0x5ee975283d71c93bULL, 0x691548c86c1bd540ULL, 0x7910c41d10a1e6a5ULL,
    0x0b5fc64563b3e2a8ULL, 0x047f7684e9fc949dULL, 0xb99181f2d8f685caULL,
    0x284600e3f30e38c3ULL
  };

  int p = st->p;
  uint64_t *s = st->s;
  uint64_t t[N] = { 0 };

  for(int i = 0; i < N; i++)
    for(int b = 0; b < 64; b++) {
      if (jump[i] & 1ULL << b)
        for(int j = 0; j < N; j++)
          t[j] ^= s[(j + p) & (N - 1)];
      mad_num_irand();
    }
  for(int j = 0; j < N; j++)
    s[(j + p) & (N - 1)] = t[j];
}

static void mad_num_randseed (int seed)
{
  uint64_t *s = st->s;
  const union numbit n = { .d = seed ? seed : M_PI };
  s[0] = n.u * 33;
  for (int i = 1; i < N; i++)
    s[i] = s[i-1] * 33;
  for (int i = 0; i < N; i++)
    mad_num_irand();

  mad_num_randjump();
}

#undef N

// -- MAD-X RNG ---------------------------------------------------------------o

// private functions

#define MAX_RAND 1000000000        /* for random generator */
#define NR_RAND 55                 /* for random generator */
#define NJ_RAND 24                 /* for random generator */
#define ND_RAND 21                 /* for random generator */

static int irn_rand[NR_RAND];      /* for random generator */

static void
madx_irngen(void)
  /* creates random number for frndm() */
{
  int i, j;
  for (i = 0; i < NJ_RAND; i++)
  {
    if ((j = irn_rand[i] - irn_rand[i+NR_RAND-NJ_RAND]) < 0) j += MAX_RAND;
    irn_rand[i] = j;
  }
  for (i = NJ_RAND; i < NR_RAND; i++)
  {
    if ((j = irn_rand[i] - irn_rand[i-NJ_RAND]) < 0) j += MAX_RAND;
    irn_rand[i] = j;
  }
  next_rand = 0;
}

// public interface

static void
madx_init55(int seed)
  /* initializes random number algorithm */
{
  int i, ii, k = 1, j = abs(seed)%MAX_RAND;
  irn_rand[NR_RAND-1] = j;
  for (i = 0; i < NR_RAND-1; i++) {
    ii = (ND_RAND*(i+1))%NR_RAND;
    irn_rand[ii-1] = k;
    if ((k = j - k) < 0) k += MAX_RAND;
    j = irn_rand[ii-1];
  }
  /* warm up */
  for (i = 0; i < 3; i++) madx_irngen();
}

static double
madx_frndm(void)
  /* returns random number r with 0 <= r < 1 from flat distribution */
{
  const double one = 1;
  double scale = one / MAX_RAND;
  if (next_rand == NR_RAND) madx_irngen();
  return scale*irn_rand[next_rand++];
}

// -- Public ------------------------------------------------------------------o

static void   (*init55_p) (int seed) = madx_init55;
static double (*frndm_p ) (void)     = madx_frndm;

void init55 (int seed)
{
  init55_p(seed);
}

double frndm (void)
{
  return frndm_p();
}

// -- Select RNG --------------------------------------------------------------o

void setrand (const char *kind, int rng_id)
{
  int info = get_option("info");

  if (!strcmp(kind,"best") || !strcmp(kind,"xorshift1024star")) {
    init55_p = mad_num_randseed;
    frndm_p  = mad_num_rand;

    if (rng_id < 0) rng_id = 0;
    if (rng_id > 0) { // stream id provided
      rng_id = (rng_id-1)%10;
      st = st_array + rng_id;
    }
    if (st->id == 0) { // stream not yet initialized
      st->id = rng_id+1;
      mad_num_randseed(0);
      for (int i = 0; i < st->id; i++)
        mad_num_randjump(); // ensure no overlap
    }
    if (info)
      fprintf(prt_file, "random number generator set to '%s[%d]'\n", kind, st->id);

    return;
  }

  if (!strcmp(kind,"default")) {
    init55_p = madx_init55;
    frndm_p  = madx_frndm;

    // default, already initialized
    if (info)
      fprintf(prt_file, "random number generator set to '%s'\n", kind);

    return;
  }

  warning("invalid kind of random generator (ignored): ", kind);
}

// -- Gaussian distribution ---------------------------------------------------o

double grndm(void)
  /* returns random number x from normal distribution */
{
  double xi1 = 2*frndm()-one,
         xi2 = 2*frndm()-one, zzr;
  while ((zzr = xi1*xi1+xi2*xi2) > one) {
    xi1 = 2*frndm()-one;
    xi2 = 2*frndm()-one;
  }
  zzr = sqrt(-2*log(zzr)/zzr);
  return xi1*zzr;
}

double tgrndm(double cut)
  /* returns random variable from normal distribution cat at 'cut' sigmas */
{
  double ret = zero;
  if (cut > zero) {
    ret = grndm();
    while (fabs(ret) > fabs(cut)) ret = grndm();
  }
  return ret;
}


