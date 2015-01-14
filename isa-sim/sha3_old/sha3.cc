#include "rocc.h"
#include "mmu.h"
#include "extension.h"
#include "sha3.h"

constexpr uint64_t sha3_t::keccakf_rndc[24];
constexpr int sha3_t::keccakf_rotc[24];
constexpr int sha3_t::keccakf_piln[24];

void sha3_t::printState(uint64_t st[25])
{
  int i,j;
  for(i = 0; i<5; i++){
     for(j = 0; j<5; j++){
       printf("%016llx ", st[i+j*5]);
     }
     printf("\n");
  }
  printf("\n");
}

// update the state with given number of rounds
void sha3_t::keccakf(uint64_t st[25], int rounds)
{
  int i, j, round_num;
  uint64_t t, bc[5];

  printf("Starting\n");
  //printState(st);
  for (round_num = 0; round_num < rounds; round_num++) 
  {
    // Theta
    for (i = 0; i < 5; i++) 
      bc[i] = st[i] ^ st[i + 5] ^ st[i + 10] ^ st[i + 15] ^ st[i + 20];
    
    for (i = 0; i < 5; i++) 
    {
      t = bc[(i + 4) % 5] ^ ROTL64(bc[(i + 1) % 5], 1);
      for (j = 0; j < 25; j += 5)
  st[j + i] ^= t;
      }

    //printf("After Theta:\n");
    //printState(st);
    // Rho Pi
    t = st[1];
    for (i = 0; i < 24; i++) 
    {
      j = keccakf_piln[i];
      bc[0] = st[j];
      st[j] = ROTL64(t, keccakf_rotc[i]);
      t = bc[0];
    }
    //printf("After RhoPi:\n");
    //printState(st);

    //  Chi
    for (j = 0; j < 25; j += 5) 
    {
      for (i = 0; i < 5; i++)
  bc[i] = st[j + i];
      for (i = 0; i < 5; i++)
  st[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
    }

    //printf("After Chi:\n");
    //printState(st);
    //  Iota
    st[0] ^= keccakf_rndc[round_num];
    //printf("After Round %d:\n",round_num);
    //printState(st);
  }
}

int sha3_t::sha3ONE(reg_t message, unsigned int len, reg_t digest)
{
  sha3_state sctx;
  sha3_init(&sctx);
  sha3_update(&sctx, message, len);
  sha3_final(&sctx, digest);
  return 0;
}

void sha3_t::sha3_init(sha3_state *sctx)
{
  memset(sctx, 0, sizeof(*sctx));
  sctx->md_len = SHA3_DEFAULT_DIGEST_SIZE;
  sctx->rsiz = 200 - 2 * SHA3_DEFAULT_DIGEST_SIZE;
  sctx->rsizw = sctx->rsiz / 8;
}

void sha3_t::sha3_update(sha3_state *sctx, reg_t data, unsigned int len)
{
  unsigned int done;
  reg_t src;

  done = 0;
  src = data;

  if ((sctx->partial + len) > (sctx->rsiz - 1)) 
  {
    if (sctx->partial) 
    {
      done = -sctx->partial;
      //memcpy(sctx->buf + sctx->partial, data,
       //done + sctx->rsiz);
      for(uint32_t i=0; i < done + sctx->rsiz; i++)
        sctx->buf[sctx->partial + i] = p->get_mmu()->load_uint8(data + i);
      src = (reg_t )sctx->buf;
    }

    do {
      unsigned int i;

      for (i = 0; i < sctx->rsizw; i++)
  state[i] ^= ((uint64_t *) src)[i];
      keccakf(state, KECCAK_ROUNDS);

      done += sctx->rsiz;
      src = data + done;
    } while (done + (sctx->rsiz - 1) < len);

    sctx->partial = 0;
  }
  //memcpy(sctx->buf + sctx->partial, src, len - done);
  for(uint32_t i = 0; i < len - done; i++)
    sctx->buf[sctx->partial + i] = p->get_mmu()->load_uint8(src + i);
  sctx->partial += (len - done);
}

void sha3_t::sha3_final(sha3_state *sctx, reg_t out)
{
  unsigned int i, inlen = sctx->partial;

  sctx->buf[inlen++] = 1;
  memset(sctx->buf + inlen, 0, sctx->rsiz - inlen);
  sctx->buf[sctx->rsiz - 1] |= 0x80;

  for (i = 0; i < sctx->rsizw; i++)
  state[i] ^= ((uint64_t *) sctx->buf)[i];

  keccakf(state, KECCAK_ROUNDS);

  // RBF - On big endian systems, we may need to reverse the bit order here
  // RBF - CONVERT FROM CPU TO LE64
  /*
  for (i = 0; i < sctx->rsizw; i++)
    sctx->st[i] = cpu_to_le64(sctx->st[i]);
  */
  //memcpy(out, state, sctx->md_len);
  for(uint32_t i=0; i < sctx->md_len; i++)
    p->get_mmu()->store_uint8(out + i, state[i]);

  memset(sctx, 0, sizeof(*sctx));
}