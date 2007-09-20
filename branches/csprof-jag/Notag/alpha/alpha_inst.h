/* alpha_inst.h -- various modular tests for alpha machine instructions */

#ifndef CSPROF_ALPHA_INST_H
#define CSPROF_ALPHA_INST_H

/* lots of functions to inspect machine instructions */
static inline int
csprof_inst_is_return(unsigned int inst)
{
    /* ret zero, ra, 0001 */
    /* probably a poor assumption that we always return through ra */
    return ((inst & 0xfffa0001) == 0x6bfa0001);
}

static inline int
csprof_inst_is_ldq(unsigned int inst)
{
    /* ldq ra, ... */
    unsigned int upper = (inst >> 20) & 0xffe;
    return upper == 0xa74;
}

static inline int
csprof_inst_is_addq(unsigned int inst)
{
    /* addq sp, ..., sp */
    return ((inst & 0xffe0041f) == 0x43c0041e);
}

static inline int
csprof_inst_is_lda(unsigned int inst, unsigned int frame_size)
{
    /* lda sp, ... */
    unsigned int upper = inst >> 16;
    return upper == 0x23de && ((inst & 0xffff) == frame_size);
}

static inline int
csprof_inst_is_subq(unsigned int inst)
{
    /* subq sp, ..., sp */
    return ((inst & 0xffe0053f) == 0x43c0053e);
}

static inline int
csprof_inst_is_stqra(unsigned int inst)
{
    /* stq ra, ... */
    unsigned int upper = (inst >> 20) & 0xffe;
    return upper == 0xb74;
}

static inline int
csprof_inst_is_bis(unsigned int inst, unsigned int from, unsigned int to)
{
    /* we can actually get the bit pattern exact in this case...
       or at least a lot closer than most of the others */
    unsigned int bitpattern =
        (0x11 << 26) | (0x1f << 21) | (from << 16) | to;

    return ((inst & 0xffff001f) == bitpattern);
}

static inline int
csprof_inst_is_backwards_branch(unsigned int inst)
{
    unsigned int opcode = (inst >> 26) & 0x3f;
    unsigned int backwards = (inst >> 20) & 0x1;

    /* FIXME: may want to make all branches, just because */
    return (opcode == 0x30) || (opcode == 0x34)
        || ((0x38 <= opcode) && (opcode <= 0x3f) && backwards);
}

#endif
