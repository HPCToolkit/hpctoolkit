// SPDX-FileCopyrightText: 2002-2024 Rice University
// SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

#ifndef files_h
#define files_h


//*****************************************************************************
// forward declarations
//*****************************************************************************

void hpcrun_files_set_directory();

void hpcrun_files_set_executable(const char *execname);

const char *hpcrun_files_executable_pathname();
const char *hpcrun_files_executable_name();
const char *hpcrun_files_output_directory();

int hpcrun_open_log_file(void);
int hpcrun_open_trace_file(int thread);
int hpcrun_open_profile_file(int rank, int thread);
int hpcrun_rename_log_file(int rank);
int hpcrun_rename_trace_file(int rank, int thread);

// storing the hash of the vdso for the current process
extern char vdso_hash_str[];
void hpcrun_save_vdso();

//*****************************************************************************

#endif // files_h
