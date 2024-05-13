/*
 * SPDX-FileCopyrightText: 2009-2024 Rice University
 * SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

        .file   "tramp.S"
        .section        ".text"
        .align 2
        .globl hpcrun_trampoline
        .type   hpcrun_trampoline, @function
        .globl hpcrun_trampoline_end
hpcrun_trampoline:
.L2:
hpcrun_trampoline_end:
        blr
        .size   hpcrun_trampoline,.-hpcrun_trampoline
        .ident  "GCC:    (GNU) 4.1.2 20070115 (SUSE Linux)"
        .section        .note.GNU-stack,"",@progbits
