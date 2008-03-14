#! /usr/bin/env python
import os
for i in xrange(0,5):
    print "working on iter: %d" % (i+1)
    os.system('./tp0 2>&1 | tail -500 > tp.log.%d' % (i+1))
