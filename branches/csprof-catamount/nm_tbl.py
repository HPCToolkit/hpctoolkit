#!/usr/bin/env python
'''
Use nm to extract the symbol table info for start/end of functions.
Put the data into a C array for later compilation.
Start / end data ultimately used by unwinder
'''
import os
import sys
import re

t_re_s = r'^ (?:0*)([1-9a-f][0-9a-f]*) \s+ (.) \s+ (\S+)$'
t_re   = re.compile(t_re_s,re.X)

table = '''
unsigned long csprof_nm_addrs[] = {
%s
};
int csprof_nm_addrs_len  = sizeof(csprof_nm_addrs) / sizeof(csprof_nm_addrs[0]);
'''
ftype = '''
unsigned int csprof_relocatable = %s;
'''
stype = '''
unsigned int csprof_stripped = %s;
'''
def process(prog, mypath = ''):
    (pathn,base) = os.path.split(prog)
    c_prog_name = os.path.join(mypath, '%s.nm.c' % base)
    so_name     = os.path.join(mypath, '%s.nm.so' % base)

#------------------------------
#    debugging output
#------------------------------
#    print "prog=", prog
#    print "mypath=", mypath

    c_prog_f = file(c_prog_name,'w')
    
    relocatable = 0
    stripped = 0
    executable = 0
    for l in  os.popen('file %s' % prog):
        s = l.rsplit()
        slen = len(s)
        if slen > 2 and s[slen-1] == 'stripped' and s[slen-2] != 'not':
            stripped = 1
        if 'shared' in s and 'object,' in s:
            relocatable = 1
        if 'executable,' in s:
            executable = 1
        break

    c_prog_f.write(stype % stripped)
    c_prog_f.write(ftype % relocatable)

    if stripped == 0 and (executable or relocatable):
        seg_addrs = []
        for l in os.popen('nm -v %s' % prog):
            m = t_re.match(l)
            if m:
                if m.group(2) in 'TtW':
                    if seg_addrs and seg_addrs[-1][0] == m.group(1):
                        seg_addrs[-1] = (seg_addrs[-1][0],seg_addrs[-1][1] + ',' + m.group(3))
                    else:
                        seg_addrs.append((m.group(1),m.group(3)))

#---------------------------------------------------------
# don't look for any more symbols after _f$ini. we won't
# get any samples in _fini because we turn off sampling 
# by then. looking for more symbols is dangerous because 
# a weak symbol data_start causes us to treat addresses
# BETWEEN the text and data segment as valid.
#---------------------------------------------------------
                    if m.group(3) == "_fini": break

    
        addr_s   = '  ' + ',\n  '.join([ '0x%s /* %s */' % v for v in seg_addrs ])
        c_prog_f.write(table % addr_s)

    c_prog_f.close()
    os.system('gcc %s -fPIC -shared -nostartfiles -o %s' %(c_prog_name,so_name))

if __name__ == '__main__':
    process(sys.argv[1], sys.argv[2])
