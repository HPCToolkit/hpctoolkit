//
// $Id$
//

#ifndef _FNBOUNDS_FILE_HEADER_
#define _FNBOUNDS_FILE_HEADER_

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
    int  stripped;
};

#endif
