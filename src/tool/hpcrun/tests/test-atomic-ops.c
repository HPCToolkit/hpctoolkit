// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// -----------------------------------
// Part of HPCToolkit (hpctoolkit.org)
// -----------------------------------
// 
// Copyright ((c)) 2002-2009, Rice University 
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// 
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// 
// * Neither the name of Rice University (RICE) nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
// 
// This software is provided by RICE and contributors "as is" and any
// express or implied warranties, including, but not limited to, the
// implied warranties of merchantability and fitness for a particular
// purpose are disclaimed. In no event shall RICE or contributors be
// liable for any direct, indirect, incidental, special, exemplary, or
// consequential damages (including, but not limited to, procurement of
// substitute goods or services; loss of use, data, or profits; or
// business interruption) however caused and on any theory of liability,
// whether in contract, strict liability, or tort (including negligence
// or otherwise) arising in any way out of the use of this software, even
// if advised of the possibility of such damage. 
// 
// ******************************************************* EndRiceCopyright *

#include <stdio.h>
#include <assert.h>

#include <lib/prof-lean/atomic-op.h>

#define L_INIT 17
#define M_INIT 41

long l = L_INIT;
long m = M_INIT;

long *p = &l;

void 
print_long_status()
{
	printf("value *(%p) = %d, *(%p) = %d\n", &l, l, &m, m);
}

int 
main(int argc, char **argv)
{
	printf("initial long state: ");
	print_long_status();
	assert(p == &l && *p == L_INIT);

	printf("swapping *(%p) = %p with %p, ", &p, p, &m); 
	long *oldptr = fetch_and_store_ptr(&p, &m);
 	printf("old value = %p, new value = %p\n", oldptr, p); 
	assert(p == &m && *p == M_INIT);
	print_long_status();

	printf("swapping *(%p) = %d with 2\n", &l, l); 
	long old = fetch_and_store(&l, 2);
	assert(old == L_INIT && l == 2);
	print_long_status();

	printf("swapping *(%p) = %d with 3\n", &l, l); 
	old = fetch_and_store(&l, 3);
	assert(old == 2 && l == 3);
	print_long_status();

	printf("fetch-and-add(&l, 4),  old value = %d\n", old = fetch_and_add(&l, 4));
	assert(old == 3 && l == 7);
	print_long_status();

#if    defined(LL_BODY) && defined (SC_BODY)
	long _val = 20;
        printf("load_linked(%p) = %d, store_conditional(%p,20) = %d\n", &l, old = load_linked(&l), &l, store_conditional(&l,_val));
	assert(old == 7 && l == 20); /* prior sc should succeed */
	print_long_status();
	_val = 21;
        printf("store_conditional(%p,%d) = %d\n", &l, _val, old = store_conditional(&l,_val));
	assert(old == 0 && l == 20); /* prior sc should fail */
	print_long_status();
#endif
}

