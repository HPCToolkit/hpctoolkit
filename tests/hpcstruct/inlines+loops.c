// SPDX-FileCopyrightText: 2023-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

#include <stdio.h>
#include <stdlib.h>

volatile int unconstant_i = 2;
volatile int unconstant_j = 2;
volatile int unconstant_k = 2;
volatile int x = 17;

// DECLARE: !!S^ <S/* i=* l=inparbounds v=* <S/* i=* l="0" v=*
// DECLARE: !!S0^ <S/* i=* l="0" v=*
// DECLARE: !!A^ dbg:<A* i=* f=* l=* n="" v="{}" <*>* </A

// FIXME: See https://gitlab.com/hpctoolkit/hpctoolkit/-/issues/730
// DECLARE: !!PS^ dbg:<A? i=* f=file[nodbg:*] l=line v="{}" n="<inline>" <S/? i=* l=line v=* </A <S/* i=* l=inparbounds v=*

// DEFINE: !f_inlined
static inline __attribute__((always_inline))
void f_inlined() {  // CHECK: dbg:<A i=* f=file l=nextline n="f_inlined" v="{}" !!S^
  x += unconstant_k;  // CHECK: dbg:<S/ i=* l=line v=*
}  // CHECK: </A
// ENDDEFINE: !f_inlined

// DEFINE: !f_inlined_loop
static inline __attribute__((always_inline))
void f_inlined_loop() {  // CHECK: dbg:<A i=* f=file l=nextline n="f_inlined_loop" v="{}" !!S^
  for(volatile int k = 0; k < unconstant_k; k++) {  // CHECK: <L i=* f=file[nodbg:*] l=line v=* !!S^
    x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  }  // CHECK: </L
}  // CHECK: </A
// ENDDEFINE: !f_inlined_loop

// CHECK: <LM i=* n=binary has-calls="0" v="{}"
// CHECK: dbg:<F i=* n=""[*] <P* i=* n=* l=* ln=* v=* <*>* </P </F
// CHECK: <F? i=* n=**"/crti.S" <*>* </F
// CHECK: <F i=* n=file[nodbg:*] nodbg:<P* i=* n=* l=* ln=* v=* <*>* </P

// 1. Standard function
void f1() {  // CHECK: <P i=* l=inbounds n="f1"[*] ln="f1" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
}  // CHECK: </P

// 2. Inlined function call
void f2() {  // CHECK: <P i=* l=inbounds n="f2"[*] ln="f2" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  f_inlined();  // CHECK: dbg:<A i=* f=* l=* v="{}" n="" !f_inlined </A
}  // CHECK: </P

// 3. Loops
void f3_1() {  // CHECK: <P i=* l=inbounds n="f3_1"[*] ln="f3_1" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  for(volatile int i = 0; i < unconstant_i; i++) {  // CHECK: <L i=* f=file[nodbg:*] l=line v=* !!S^
    x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  }  // CHECK: </L
}  // CHECK: </P
void f3_2() {  // CHECK: <P i=* l=inbounds n="f3_2"[*] ln="f3_2" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  for(volatile int i = 0; i < unconstant_i; i++) {  // CHECK: <L i=* f=file[nodbg:*] l=line v=* !!S^
    for(volatile int j = 0; j < unconstant_j; j++) {  // CHECK: <L i=* f=file[nodbg:*] l=line v=* !!S^
      x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
    }  // CHECK: </L
  }  // CHECK: </L
}  // CHECK: </P
void f3_3() {  // CHECK: <P i=* l=inbounds n="f3_3"[*] ln="f3_3" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  for(volatile int i = 0; i < unconstant_i; i++) {  // CHECK: <L i=* f=file[nodbg:*] l=line v=* !!S^
    for(volatile int j = 0; j < unconstant_j; j++) {  // CHECK: <L i=* f=file[nodbg:*] l=line v=* !!S^
      for(volatile int k = 0; k < unconstant_k; k++) {  // CHECK: <L i=* f=file[nodbg:*] l=line v=* !!S^
        x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
      }  // CHECK: </L
    }  // CHECK: </L
  }  // CHECK: </L
}  // CHECK: </P

// 4. Interleaved loops + inlined calls
void f4_1() {  // CHECK: <P i=* l=inbounds n="f4_1"[*] ln="f4_1" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  for(volatile int i = 0; i < unconstant_i; i++) {  // CHECK: !!A^ <L i=* f=file[nodbg:*] l=line v=* !!S^
    f_inlined();  // CHECK: dbg:<A i=* f=* l=* n="" v="{}" !f_inlined </A
  }  // CHECK: </L
}  // CHECK: </P
void f4_2() {  // CHECK: <P i=* l=inbounds n="f4_2"[*] ln="f4_2" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  for(volatile int i = 0; i < unconstant_i; i++) {  // CHECK: !!A^ <L i=* f=file[nodbg:*] l=line v=* !!S^
    f_inlined_loop();  // CHECK: dbg:<A i=* f=* l=* n="" v="{}" !f_inlined_loop </A
  }  // CHECK: </L
}  // CHECK: </P
void f4_3() {  // CHECK: <P i=* l=inbounds n="f4_3"[*] ln="f4_3" v=* !!PS^ !!S0^
  x += unconstant_k;  // CHECK: <S/ i=* l=line v=*
  for(volatile int i = 0; i < unconstant_i; i++) {  // CHECK: !!A^ <L i=* f=file[nodbg:*] l=line v=* !!S^
    for(volatile int j = 0; j < unconstant_j; j++) {  // CHECK: !!A^ <L i=* f=file[nodbg:*] l=line v=* !!S^
      f_inlined_loop();  // CHECK: dbg:<A i=* f=* l=* n="" v="{}" !f_inlined_loop </A
    }  // CHECK: </L
  }  // CHECK: </L
}  // CHECK: </P

// CHECK: </F
// CHECK: </LM
