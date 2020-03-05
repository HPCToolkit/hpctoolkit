#!/usr/bin/env python

import sys
import re

end = re.compile('\n')
for line in sys.stdin:
    print('"' + end.sub(r'\\n"', line))
