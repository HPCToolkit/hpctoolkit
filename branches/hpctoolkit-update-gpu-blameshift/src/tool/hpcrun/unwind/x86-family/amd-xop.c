#include "amd-xop.h"

typedef unsigned char uc;
typedef struct {
  uc vex  : 8;
  uc opcp : 5;
  uc B    : 1;
  uc X    : 1;
  uc R    : 1;
  uc pp   : 2;
  uc L    : 1;
  uc vv   : 4;
  uc W    : 1;
  uc opc  : 8;
  uc r_m  : 3;
  uc reg  : 3;
  uc mod  : 2;
} apm_t;

typedef struct {
  uc vex  : 8;
  uc pp   : 2;
  uc L    : 1;
  uc vv   : 4;
  uc R    : 1;
  uc opc  : 8;
  uc r_m  : 3;
  uc reg  : 3;
  uc mod  : 2;
} apm_c5_t;

static const uc vex_op4_tbl[] = {
  0x48,
  0x49,
  0x5c,
  0x5d,
  0x5e,
  0x5f,
  0x68,
  0x69,
  0x6a,
  0x6b,
  0x6c,
  0x6d,
  0x6e,
  0x6f,
  0x78,
  0x79,
  0x7a,
  0x7b,
  0x7c,
  0x7d,
  0x7e,
  0x7f,
};

static const uc xop_op4_tbl[] = {
  0x85,
  0x86,
  0x87,
  0x8e,
  0x8f,
  0x95,
  0x96,
  0x97,
  0x9f,
  0xa2,
  0xa3,
  0xa6,
  0xb6,
  0xcc,
  0xcd,
  0xce,
  0xcf,
  0xec,
  0xed,
  0xee,
  0xef,
};

#define Table_spec(t) t, sizeof(t)
#define Opc_in_tbl(t) opc_in_table(opc, Table_spec(t))

static inline bool
opc_in_table(uc opc, const uc opc_tbl[], const size_t len)
{
  for(size_t i=0; i < len; i++)
    if (opc == opc_tbl[i]) return true;
  return false;
}

static bool
xop_op4(void* seq)
{
  apm_t* in = seq;
  uc opc = in->opc;

  return Opc_in_tbl(xop_op4_tbl);
}

static bool
vex_op4(void* seq)
{
  apm_t* in = seq;
  uc opc = in->opc;

  return Opc_in_tbl(vex_op4_tbl);
}

static bool
vex5_op4(void* seq)
{
  apm_c5_t* in = seq;
  uc opc = in->opc;

  return Opc_in_tbl(vex_op4_tbl);
}

static size_t
op4_imm(void* seq)
{
  uc* in = seq;

  bool has4 = false;
  if (*in == 0xc4)
    has4 = vex_op4(seq);
  if (*in == 0xc5)
    has4 = vex5_op4(seq);
  if (*in == 0x8f)
    has4 = xop_op4(seq);

  return has4 ? 1 : 0;
}

static size_t
disp_size(void* seq)
{
  uc* escape = seq;
  size_t rv = 0;
  uc mod = 0xff;
  uc r_m = 0xff;

  if ((*escape == 0xc4) || (*escape == 0x8f)) {
    apm_t* in = seq;
    mod = in->mod;
    r_m = in->r_m;
  }
  if (*escape == 0xc5) {
    apm_c5_t* in = seq;
    mod = in->mod;
    r_m = in->r_m;
  }

  if ((mod == 0) && (r_m == 5)) rv = 4;
  else if (mod == 1) rv = 1;
  else if (mod == 2) rv = 4;

  return rv;
}

static size_t
sib_byte(void* seq)
{
  uc* escape = seq;
  uc r_m = 0xff;
  uc mod = 0xff;

  if ((*escape == 0xc4) || (*escape == 0x8f)) {
    apm_t* in = seq;
    mod = in->mod;
    r_m = in->r_m;
  }
  if (*escape == 0xc5) {
    apm_c5_t* in = seq;
    mod = in->mod;
    r_m = in->r_m;
  }

  return (mod != 3) && (r_m == 4) ? 1 : 0;
}

static size_t
escape_len(void* seq)
{
  uc* escape = seq;

  if ((*escape == 0xc4) || (*escape == 0x8f))
    return sizeof(apm_t);
  if (*escape == 0xc5)
    return sizeof(apm_c5_t);

  return 1;
}

static inline size_t
ins_len(void* seq)
{
  // length of instruction = size of escape seq + #immediate bytes + # disp bytes + # sib bytes
  return escape_len(seq) + op4_imm(seq) + disp_size(seq) + sib_byte(seq);
}

static const uc prefix_tbl[] = {
  0x66, // op size override
  0x67, // addr size override
  0x2e, 0x3e, 0x26, 0x64, 0x65, 0x36, // segment override
  0xf0, // lock
  0xf2, 0xf3, // repeat
};

static bool
is_prefix(void* ins)
{
  uc* in = ins;
  for (int i = 0; i < sizeof(prefix_tbl); i++) {
    if (*in ==prefix_tbl[i]) return true;
  }
  return false;
}

void
adv_amd_decode(amd_decode_t* stat, void* ins)
{
  uc* in = ins;
  stat->len = 0;
  // skip standard prefixes
  for (int i = 0; i < 4; i++) {
    if (is_prefix(in)) {
      in++;
      (stat->len)++;
    }
    else break;
  }
  stat->success = false;
  stat->weak    = false;

  if (*in == 0xc4) {
    stat->success = true;
  }
  if (*in == 0x8f) {
    stat->success = true;
    stat->weak = true;
  }

  stat->len += ins_len(in);
}
