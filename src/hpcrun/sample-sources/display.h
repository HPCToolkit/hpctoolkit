// SPDX-FileCopyrightText: 2002-2024 Rice University
//
// SPDX-License-Identifier: BSD-3-Clause

// -*-Mode: C++;-*- // technically C99

/*
 * display.h
 *
 *  Created on: Jul 28, 2017
 *      Author: la5
 */

#ifndef _SAMPLE_SOURCES_DISPLAY_H_
#define _SAMPLE_SOURCES_DISPLAY_H_

/******************************************************************************
 * system includes
 *****************************************************************************/

#include <stdio.h>

void display_line_single(FILE *output);

void display_line_double(FILE *output);

void display_header(FILE *output, const char *title);

void display_header_event(FILE *output);

void display_event_info(FILE *output, const char *event, const char *desc);

#endif /* _SAMPLE_SOURCES_DISPLAY_H_ */
