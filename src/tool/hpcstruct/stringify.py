#!/usr/bin/env python

import sys
import re

end = re.compile('\n')
with open(sys.argv[1], 'r') as infile:
    with open(sys.argv[2], 'w') as outfile:
        for line in infile:
            print('"' + end.sub(r'\\n"', line), file=outfile)
