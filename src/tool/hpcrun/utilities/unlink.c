// -*-Mode: C++;-*- // technically C99

// * BeginRiceCopyright *****************************************************
//
// $HeadURL$
// $Id$
//
// --------------------------------------------------------------------------
// Part of HPCToolkit (hpctoolkit.org)
//
// Information about sources of support for research and development of
// HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
// --------------------------------------------------------------------------
//
// Copyright ((c)) 2002-2016, Rice University
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

/*
 *  Function to recursively unlink a directory hierarchy.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>

#include "unlink.h"


/*
 *  Unlink the tree hierarchy rooted at "path" (helper function).
 *  path_len = strlen(path),
 *  max_len = length of buffer for path.
 *
 *  Stevens describes this technique of using a single buffer for the
 *  path names in "Advanced Programming in the Unix Environment",
 *  Section 4.21 (Reading Directories).
 *
 *  Returns: 0 on success, or else the number of unlink() or rmdir()
 *  calls that failed.
 */
static int
do_unlink_tree(char *path, int path_len, int max_len)
{
    DIR *dirp;
    struct dirent *dent;
    struct stat sb;
    int len, num_failures;

    /*
     * Use unlink(2) for anything that's not a directory.
     */
    if (lstat(path, &sb) != 0)
	return (1);
    if (! S_ISDIR(sb.st_mode))
	return (unlink(path) ? 1 : 0);

    /*
     * Recursively unlink directory entries, except "." and "..".
     */
    dirp = opendir(path);
    if (dirp == NULL)
	return (1);
    num_failures = 0;
    path[path_len] = '/';
    while ((dent = readdir(dirp)) != NULL) {
	if (strncmp(dent->d_name, ".", 2) == 0 ||
	    strncmp(dent->d_name, "..", 3) == 0)
	    continue;
	len = strlen(dent->d_name);
	if (path_len + len + 2 >= max_len) {
	    num_failures++;
	} else {
	    strcpy(&path[path_len + 1], dent->d_name);
	    num_failures += do_unlink_tree(path, path_len + len + 1, max_len);
	}
    }
    path[path_len] = 0;
    closedir(dirp);

    /*
     * Finally, rmdir(2) the directory.
     */
    if (rmdir(path) != 0)
	num_failures++;

    return (num_failures);
}

/*
 *  Unlink the file hierarchy rooted at "path".
 *
 *  Returns: 0 on success, or else the number of unlink() or rmdir()
 *  calls that failed.
 */
#define BUF_LEN  4096
int
unlink_tree(char *path)
{
    char buf[BUF_LEN];
    int len;

    len = strlen(path);
    if (len >= BUF_LEN)
	return (1);
    strcpy(buf, path);
    return do_unlink_tree(buf, len, BUF_LEN);
}
