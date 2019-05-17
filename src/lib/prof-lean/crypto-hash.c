//*****************************************************************************
// system includes
//*****************************************************************************

#include <assert.h>
#include <string.h>

#include <mbedtls/md5.h>  // MD5



//*****************************************************************************
// macros
//*****************************************************************************

#define MD5_HASH_NBYTES 16

#define HASH_LENGTH MD5_HASH_NBYTES 

#define LOWER_NIBBLE_MASK 	(0x0f)
#define UPPER_NIBBLE(c) 	((c >> 4) & LOWER_NIBBLE_MASK)
#define LOWER_NIBBLE(c) 	(c  & LOWER_NIBBLE_MASK)

#define HEX_TO_ASCII(c) ((c > 9) ?  'a' + (c - 10) : ('0' + c))



//*****************************************************************************
// interface operations
//*****************************************************************************


int
crypto_hash_compute
(
  const unsigned char *input, 
  size_t input_length,
  unsigned char *hash,
  unsigned int hash_length
)
{
  if (hash_length > HASH_LENGTH) {
    // failure: caller not prepared to accept a hash of at least the length 
    // that we will provide
    return -1;
  }

  // zero result
  memset(hash, 0, hash_length); 

  mbedtls_md5(input, input_length, hash);
  return 0;
}


int
crypto_hash_to_hexstring
(
  unsigned char *hash,
  char *hash_string,
  unsigned int hash_string_length
)
{
  int hex_digits = HASH_LENGTH << 1;
  if (hash_string_length < hex_digits + 1) {
    return -1;
  }

  int chars = HASH_LENGTH;
  while (chars-- > 0) {
    unsigned char val_u = UPPER_NIBBLE(*hash); 
    *hash_string++ = HEX_TO_ASCII(val_u);

    unsigned char val_l = LOWER_NIBBLE(*hash);
    *hash_string++ = HEX_TO_ASCII(val_l);
    hash++;
  }
  *hash_string++ = 0;

  return 0;
}


unsigned int
crypto_hash_length
(
  void
)
{
  return HASH_LENGTH;
}

#if UNIT_TEST

int
main(int argc, char **argv)
{
  int verbose = 1;
  int status = mbedtls_md5_self_test(verbose);
  char *status_string = NULL;

  if (status == 0) {
    status_string = "succeeded";
  } else {
    status_string = "failed";
  }

  printf("mbedtls_md5_self_test %s\n", status_string);

  printf(
}


#endif
