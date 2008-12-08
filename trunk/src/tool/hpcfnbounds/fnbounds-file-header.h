//
// $Id$
//

#ifndef _FNBOUNDS_FILE_HEADER_
#define _FNBOUNDS_FILE_HEADER_

//
// Printf format strings for fnbounds file names and extensions.
// The %s conversion args are directory and basename.
//
#define FNBOUNDS_BINARY_FORMAT  "%s/%s.fnbounds.bin"
#define FNBOUNDS_C_FORMAT       "%s/%s.fnbounds.c"
#define FNBOUNDS_TEXT_FORMAT    "%s/%s.fnbounds.txt"

//
// The extra info in the binary file of function addresses, written by
// hpcfnbounds-bin and read in the main process.  We call it "header",
// even though it's actually at the end of the file.
//
#define FNBOUNDS_MAGIC  0xf9f9f9f9

struct fnbounds_file_header {
    long zero_pad;
    long magic;
    long num_entries;
    int  relocatable;
};

#endif
