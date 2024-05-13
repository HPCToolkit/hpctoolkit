/*
 * SPDX-FileCopyrightText: 2012-2024 Rice University
 * SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

        .file   "ia64-tramp.s"
        .pred.safe_across_calls p1-p5,p16-p63
        .text
        .align 16
        .global hpcrun_trampoline#
        .global hpcrun_trampoline_end#
        .proc hpcrun_trampoline#
hpcrun_trampoline:
        .prologue 2, 2
        .vframe r2
hpcrun_trampoline_end:
        mov r2 = r12
        .body
        ;;
        .restore sp
        mov r12 = r2
        br.ret.sptk.many b0
        ;;
        .endp hpcrun_trampoline#
        .ident  "GCC: (GNU) 4.1.2 20080704 (Red Hat 4.1.2-46)"
        .section        .note.GNU-stack,"",@progbits
