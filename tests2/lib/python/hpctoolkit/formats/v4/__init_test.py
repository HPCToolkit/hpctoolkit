## * BeginRiceCopyright *****************************************************
##
## $HeadURL$
## $Id$
##
## --------------------------------------------------------------------------
## Part of HPCToolkit (hpctoolkit.org)
##
## Information about sources of support for research and development of
## HPCToolkit is at 'hpctoolkit.org' and in 'README.Acknowledgments'.
## --------------------------------------------------------------------------
##
## Copyright ((c)) 2022-2022, Rice University
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted provided that the following conditions are
## met:
##
## * Redistributions of source code must retain the above copyright
##   notice, this list of conditions and the following disclaimer.
##
## * Redistributions in binary form must reproduce the above copyright
##   notice, this list of conditions and the following disclaimer in the
##   documentation and/or other materials provided with the distribution.
##
## * Neither the name of Rice University (RICE) nor the names of its
##   contributors may be used to endorse or promote products derived from
##   this software without specific prior written permission.
##
## This software is provided by RICE and contributors "as is" and any
## express or implied warranties, including, but not limited to, the
## implied warranties of merchantability and fitness for a particular
## purpose are disclaimed. In no event shall RICE or contributors be
## liable for any direct, indirect, incidental, special, exemplary, or
## consequential damages (including, but not limited to, procurement of
## substitute goods or services; loss of use, data, or profits; or
## business interruption) however caused and on any theory of liability,
## whether in contract, strict liability, or tort (including negligence
## or otherwise) arising in any way out of the use of this software, even
## if advised of the possibility of such damage.
##
## ******************************************************* EndRiceCopyright *

import dataclasses
import io
import os
import sys
from pathlib import Path

import pytest
import ruamel.yaml
from hpctoolkit.test.tarball import extracted

from .._test_util import assert_good_traversal, dump_to_string, testdatadir, yaml
from . import *


def test_small_v4_0(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml") as f:
        expected = yaml.load(f)

    with extracted(testdatadir / "dbase" / "v4.0" / "small.tar.xz") as db:
        got = Database.from_dir(db)

    assert dataclasses.asdict(got) == dataclasses.asdict(expected)


def test_traversal(yaml):
    with open(testdatadir / "dbase" / "v4.0" / "small.yaml") as f:
        db = yaml.load(f)
    assert_good_traversal(db)
