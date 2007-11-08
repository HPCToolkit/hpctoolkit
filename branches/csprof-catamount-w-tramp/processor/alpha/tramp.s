#include <regdef.h>
#include <asm.h>
 # Copyright ((c)) 2002, Rice University 
 # All rights reserved.
 # 
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions are
 # met:
 # 
 # * Redistributions of source code must retain the above copyright
 #   notice, this list of conditions and the following disclaimer.
 # 
 # * Redistributions in binary form must reproduce the above copyright
 #   notice, this list of conditions and the following disclaimer in the
 #   documentation and/or other materials provided with the distribution.
 # 
 # * Neither the name of Rice University (RICE) nor the names of its
 #   contributors may be used to endorse or promote products derived from
 #   this software without specific prior written permission.
 # 
 # This software is provided by RICE and contributors "as is" and any
 # express or implied warranties, including, but not limited to, the
 # implied warranties of merchantability and fitness for a particular
 # purpose are disclaimed. In no event shall RICE or contributors be
 # liable for any direct, indirect, incidental, special, exemplary, or
 # consequential damages (including, but not limited to, procurement of
 # substitute goods or services; loss of use, data, or profits; or
 # business interruption) however caused and on any theory of liability,
 # whether in contract, strict liability, or tort (including negligence
 # or otherwise) arising in any way out of the use of this software, even
 # if advised of the possibility of such damage.

#include "tramp.h"

        .text
	.arch generic
	.align 4			# don't think this much matters

	.set noat			# avoid warnings about using $28
	.set noreorder			# I'm smarter than as(1)
 # things defined outside this file that we need to access

	.extern csprof_trampoline2

 # `csprof_trampoline' is "called" upon return from a function previously
 # instrumented.  "called" is used loosely above, because `csprof_trampoline'
 # is invoked instead of the procedure that was supposed to be returned to.
 # `csprof_trampoline''s job is to figure out where we're supposed to be
 # returning to, fix things to make it look like csprof_trampoline was
 # called from that point, and call `csprof_trampoline2' to do some trickier
 # work in C that would be a pain to accomplish in assembler.  upon
 # completion of that work, we return to the target function as though
 # nothing had happened.  part of the fun comes from making it *actually*
 # look like nothing is wrong with what we've just described above.

	.globl csprof_trampoline
	.ent csprof_trampoline
	# the frame needs to be octaword (16 bytes) aligned
	#
	# Compaq's documentation says that bits 31, 30, 29, and the
	# return register's bit should not be set in this mask.  The
	# observant will note that bit 29 is set in this mask.  Why?
	# I think it's possible that the procedure that we're returning
	# *to* may not reset its $gp register--since the procedure that
	# it called may have used the same $gp.  and so, since we change
	# it out from underneath of it, we should do the nice thing
	# and save it.
	#
	# one thing to investigate might be placing $29 in a temporary
	# stack slot (*not* in the register save area) and restoring
	# it before returning).  That way we don't have to violate
	# Compaq's standards and the function to which we return shouldn't
	# see anything different.
	.mask 0x3bffffff, -TRAMP_STACK_SPACE
	.frame sp, TRAMP_STACK_SPACE, ra
#if TRAMPOLINE_SAVE_SAVED_FP_REGISTERS || TRAMPOLINE_SAVE_ALL_FP_REGISTERS
	.fmask TRAMP_FPMASK, -TIRSS
#endif

csprof_trampoline:
	lda sp, -TRAMP_STACK_SPACE(sp)

	# we save the return address here for completeness' sake; we
	# think the exception handler unwinder is getting confused
	# by what we were doing before (attempting to store a proper
	# return address first), so we just punt and hope things will
	# work out properly
	stq $26, 0(sp)			# save the return address

#if TRAMPOLINE_SAVE_ALL_INT_REGISTERS
	stq $0, 8(sp)			# begin saving calling context
	stq $1, 16(sp)			# save other integer registers
	stq $2, 24(sp)
	stq $3, 32(sp)
	stq $4, 40(sp)
	stq $5, 48(sp)
	stq $6, 56(sp)
	stq $7, 64(sp)
	stq $8, 72(sp)
	stq $9, 80(sp)
	stq $10, 88(sp)
	stq $11, 96(sp)
	stq $12, 104(sp)
	stq $13, 112(sp)
	stq $14, 120(sp)
	stq $15, 128(sp)
	stq $16, 136(sp)
	stq $17, 144(sp)
	stq $18, 152(sp)
	stq $19, 160(sp)
	stq $20, 168(sp)
	stq $21, 176(sp)
	stq $22, 184(sp)
	stq $23, 192(sp)
	stq $24, 200(sp)
	stq $25, 208(sp)
	stq $27, 216(sp)		# $26 (ra) was saved earlier
	stq $28, 224(sp)		# might not have to save this (?)
	stq $29, 232(sp)
#else
	stq $0, 8(sp)			# begin saving calling context
	stq $1, 16(sp)			# save other integer registers
	stq $2, 24(sp)
	stq $3, 32(sp)
	stq $4, 40(sp)
	stq $5, 48(sp)
	stq $6, 56(sp)
	stq $7, 64(sp)
	stq $8, 72(sp)
	stq $15, 80(sp)			# save frame pointer
	stq $22, 88(sp)
	stq $23, 96(sp)
	stq $24, 104(sp)
	stq $25, 112(sp)
#endif
	# we think storing the stack pointer is screwing with things

	# $31 is hardwired to zero, so we don't need to store it

#if TRAMPOLINE_SAVE_SAVED_FP_REGISTERS || TRAMPOLINE_SAVE_ALL_FP_REGISTERS
	# it might not be totally necessary to save the floating-point
	# registers, since the C code doesn't use any floating-point values.
	# however, you never know...if careful examination of the code
	# determines that floating-point could will *never* *ever* be
	# generated for `csprof_trampoline2' and its callees, then all
	# this can probably be eliminated (and save a significant amount
	# of time in polluting the cache and so forth).

        stt $f0, TIRSS+0(sp)
        stt $f1, TIRSS+8(sp)
        stt $f10, TIRSS+16(sp)
        stt $f11, TIRSS+24(sp)
        stt $f12, TIRSS+32(sp)
        stt $f13, TIRSS+40(sp)
        stt $f14, TIRSS+48(sp)
        stt $f15, TIRSS+56(sp)
        stt $f16, TIRSS+64(sp)
        stt $f17, TIRSS+72(sp)
        stt $f18, TIRSS+80(sp)
        stt $f19, TIRSS+88(sp)
        stt $f20, TIRSS+96(sp)
        stt $f21, TIRSS+104(sp)
        stt $f22, TIRSS+112(sp)
        stt $f23, TIRSS+120(sp)
        stt $f24, TIRSS+128(sp)
        stt $f25, TIRSS+136(sp)
        stt $f26, TIRSS+144(sp)
        stt $f27, TIRSS+152(sp)
        stt $f28, TIRSS+160(sp)
        stt $f29, TIRSS+168(sp)
        stt $f30, TIRSS+176(sp)
#endif

	# $f31 is hardwired to floating-point zero, so don't store it

	.prologue 0			# dunno

	# find out how to access global data.  one would think that
	# we could do this at the beginning of the routine, since
	# $26 contains the address that we need to load gp.  however,
	# we have to preserve machine state *exactly* as it was,
	# since we're really not supposed to be here.  hence the
	# small amount of extra work we do here.
	br gp, TRAMP_LABEL
TRAMP_LABEL:
	ldgp gp, 0(gp)

	lda pv, csprof_trampoline2	# indicate to csprof_trampoline2
					# where it's located, so it can
					# properly access global data
	mov sp, a0			# our C trampoline wants to know
					# about the stack so it can grovel
					# around with the registers

	jsr (pv)			# go do the real bookkeeping

	# calling standard says that we have to do this, since the above is
	# "a standard call".  it's pretty pointless, but what harm can two
	# integer instructions do on a processor like the Alpha?
	ldgp gp, 0(ra)

	# the trampoline returns us our proper return address in v0
	mov v0, ra

	# reload everything we saved in preparation for the return

#if TRAMPOLINE_SAVE_ALL_INT_REGISTERS
        ldq $0, 8(sp)
        ldq $1, 16(sp)
        ldq $2, 24(sp)
        ldq $3, 32(sp)
        ldq $4, 40(sp)
        ldq $5, 48(sp)
        ldq $6, 56(sp)
        ldq $7, 64(sp)
        ldq $8, 72(sp)
        ldq $9, 80(sp)
        ldq $10, 88(sp)
        ldq $11, 96(sp)
        ldq $12, 104(sp)
        ldq $13, 112(sp)
        ldq $14, 120(sp)
        ldq $15, 128(sp)
        ldq $16, 136(sp)
        ldq $17, 144(sp)
        ldq $18, 152(sp)
        ldq $19, 160(sp)
        ldq $20, 168(sp)
        ldq $21, 176(sp)
        ldq $22, 184(sp)
        ldq $23, 192(sp)
        ldq $24, 200(sp)
        ldq $25, 208(sp)
	# $26 has already been reloaded, above
        ldq $27, 216(sp)
	ldq $28, 224(sp)
        ldq $29, 232(sp)
#else
	ldq $0, 8(sp)
	ldq $1, 16(sp)
	ldq $2, 24(sp)
	ldq $3, 32(sp)
	ldq $4, 40(sp)
	ldq $5, 48(sp)
	ldq $6, 56(sp)
	ldq $7, 64(sp)
	ldq $8, 72(sp)
	ldq $15, 80(sp)
	ldq $22, 88(sp)
	ldq $23, 96(sp)
	ldq $24, 104(sp)
	ldq $25, 112(sp)
#endif

#if TRAMPOLINE_SAVE_SAVED_FP_REGISTERS || TRAMPOLINE_SAVE_ALL_FP_REGISTERS
	# load the floating-point registers

        ldt $f0, TIRSS+0(sp)
        ldt $f1, TIRSS+8(sp)
        ldt $f10, TIRSS+16(sp)
        ldt $f11, TIRSS+24(sp)
        ldt $f12, TIRSS+32(sp)
        ldt $f13, TIRSS+40(sp)
        ldt $f14, TIRSS+48(sp)
        ldt $f15, TIRSS+56(sp)
        ldt $f16, TIRSS+64(sp)
        ldt $f17, TIRSS+72(sp)
        ldt $f18, TIRSS+80(sp)
        ldt $f19, TIRSS+88(sp)
        ldt $f20, TIRSS+96(sp)
        ldt $f21, TIRSS+104(sp)
        ldt $f22, TIRSS+112(sp)
        ldt $f23, TIRSS+120(sp)
        ldt $f24, TIRSS+128(sp)
        ldt $f25, TIRSS+136(sp)
        ldt $f26, TIRSS+144(sp)
        ldt $f27, TIRSS+152(sp)
        ldt $f28, TIRSS+160(sp)
        ldt $f29, TIRSS+168(sp)
        ldt $f30, TIRSS+176(sp)
#endif

	lda sp, TRAMP_STACK_SPACE(sp)
	ret (ra)			# back to Kansas with you!

	.end csprof_trampoline
	.globl csprof_trampoline_end
	.ent csprof_trampoline_end
csprof_trampoline_end:
	.end csprof_trampoline_end

