#!/usr/bin/env python3

import sys
import xml.sax

from hpctoolkit.test.execution import hpcstruct

with hpcstruct(sys.argv[1]) as structfile:
    xml.sax.parse(structfile, xml.sax.handler.ContentHandler())
