#!/bin/sh -e

# SPDX-FileCopyrightText: 2024 Rice University
#
# SPDX-License-Identifier: BSD-3-Clause

zypper install -y \
  tar
zypper clean all
