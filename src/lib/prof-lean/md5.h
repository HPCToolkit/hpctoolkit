/******************************

This license only applies to the two files: md5.c and md5.h.
These files are from git clone https://github.com/galenguyer/md5

-------------------------------

This is free and unencumbered software released into the public domain.

Anyone is free to copy, modify, publish, use, compile, sell, or
distribute this software, either in source code form or as a compiled
binary, for any purpose, commercial or non-commercial, and by any
means.

In jurisdictions that recognize copyright laws, the author or authors
of this software dedicate any and all copyright interest in the
software to the public domain. We make this dedication for the benefit
of the public at large and to the detriment of our heirs and
successors. We intend this dedication to be an overt act of
relinquishment in perpetuity of all present and future rights to this
software under copyright law.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

For more information, please refer to <http://unlicense.org/>

******************************/

#ifndef PROF_LEAN_MD5_H
#define PROF_LEAN_MD5_H

#define MD5_HASH_SIZE 16

struct md5_context {
    // state
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;
    // number of bits, modulo 2^64 (lsb first)
    unsigned int count[2];
    // input buffer
    unsigned char input[64];
    // current block
    unsigned int block[16];
};

struct md5_digest {
    unsigned char bytes[MD5_HASH_SIZE];
};

void md5_init(struct md5_context *ctx);
void md5_update(struct md5_context* ctx, const void* buffer, unsigned int buffer_size);
void md5_finalize(struct md5_context* ctx, struct md5_digest* digest);
// char* md5(const char* input);

#endif  // PROF_LEAN_MD5_H
