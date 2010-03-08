#! /usr/bin/env python

# $Id$

#import FindBin
#sys.path.insert(0, FindBin.Bin)

import sys, traceback
import getopt
import string, re
#True = 1; False = 0

DBG = False

#############################################################################

the_program = sys.argv[0];
the_usage = (
"""Usage:
  %s [options] < <cilk2c-file>
  %s [options] <cilk2c-file>

Given a cilk2c file, generate line directives to partition Cilk
overhead by overhead type.

By necessity (given the use of line directives), assumes that function
calls are not nested in other statements.

""") % (the_program, the_program)

the_shortOpts = "o:vhd"
the_longOpts  = ("output-file=",
                 "verbose", "help", "debug")

#############################################################################

class UsageExcept(Exception):
    def __init__(self, msg):
        self.msg = msg
        
    def __str__(self):
        if (self.msg):
            msg = ("%s: %s\n") % (the_program, self.msg)
            msg = msg + ("Try `%s --help' for more information.\n"
                         % the_program)
        else:
            msg = the_usage
        return msg

class FatalError(Exception):
    def __init__(self, msg):
        self.msg = msg

    def __str__(self):
        msg = ("%s: %s\n") % (the_program, self.msg)
        return msg

#############################################################################

def main(argv=None):
    if (argv is None):
        argv = sys.argv
        
    the_globals = {
        'inputFileNm' : None,
        'verbose'     : None,
        'debug'       : None
    }

    try:
        (opts, args) = parseCmdLine(argv, the_shortOpts, the_longOpts)
        processCmdLine(the_globals, opts, args)
        processFile(the_globals['inputFileNm']);
    except (UsageExcept, FatalError), exc:
        sys.stderr.write("%s" % exc)
        return 1
    except (SystemExit), exc:
        return 0;
    except:
        sys.stderr.write("%s: Uncaught exception:\n" % the_program)
        sys.stderr.write('-'*60+"\n")
        traceback.print_exc(file=sys.stderr)
        return 2
    else:
        return 0


def processCmdLine(ovars, opts, args):
    """Given the results of parseCmdLine(), populate the 'ovars' dict
    for this main routine."""
    # Get optional arguments: verbose, help
    if (opts.has_key('verbose') or opts.has_key('v')):
        ovars['verbose'] = True
    if (opts.has_key('debug') or opts.has_key('d')):
        ovars['debug'] = True
    if (opts.has_key('help') or opts.has_key('h')):
        raise UsageExcept(None)

    if (len(args) == 0):
        ovars['inputFileNm'] = None
    elif (len(args) == 1):
        ovars['inputFileNm'] = args[0]
    else:
        raise UsageExcept("Invalid number of arguments!")


#############################################################################

def parseCmdLine(argv, shortOpts, longOpts):
    """Given the inputs for getopt, return the parsed argv line as a
    (dictionary-of-options, rest-of-arguments) tuple.  Note that argv
    should contain the full command line"""
    try:
        (opts, args) = getopt.getopt(argv[1:], shortOpts, longOpts)
    except (getopt.error), msg:
        raise UsageExcept(msg)
    else:
        dashRE = re.compile(r"^-{1,2}") # find leading dashes
        dict = { }
        for (o, a) in opts:
            o = dashRE.sub('', o) # strip off leading dashes
            dict[o] = a
        return (dict, args)


def openInFile(filenm):
    """Given a filename 'filenm', return an input stream.  If 'filenm'
    is NULL, return stdin; otherwise open the file."""
    if (filenm):
        try:
            stream = open(filenm, "r")
        except (IOError), msg:
            raise FatalError(msg)
    else:
        stream = sys.stdin
    return stream


def closeFile(filenm, stream):
    if (filenm):
        stream.close()

#############################################################################

CilkOverhead = {
  'parallel' : [
    'CILK2C_INIT_FRAME',
    'CILK2C_PUSH_FRAME',

    'CILK2C_XPOP_FRAME_RESULT',
    'CILK2C_XPOP_FRAME_NORESULT',

    'CILK2C_SET_NORESULT',
    'CILK2C_BEFORE_RETURN_FAST',

    'CILK2C_START_THREAD_FAST',
    'CILK2C_START_THREAD_SLOW', 

    'CILK2C_BEFORE_RETURN_FAST',      # part cp 
    'CILK2C_BEFORE_RETURN_SLOW',      # part cp 
    'CILK2C_BEFORE_RETURN_INLET',

    'CILK2C_AT_SYNC_FAST',            # part cp
    'CILK2C_AT_THREAD_BOUNDARY_SLOW', # part cp

    # sync ---------------------
    #'CILK2C_SYNC',                   # <-- FIXME should be a function

    'CILK2C_ABORT_SLOW',
    'CILK2C_ABORT_STANDALONE',

    # cp -----------------------
    'CILK2C_BEFORE_SPAWN_FAST',
    'CILK2C_BEFORE_SPAWN_SLOW',
    'CILK2C_AFTER_SPAWN_FAST',
    'CILK2C_AFTER_SPAWN_SLOW',
    'CILK2C_BEFORE_SYNC_SLOW',
    'CILK2C_AFTER_SYNC_SLOW',
    ],
  
  'sync' : [
    ],

  'cp' : [
    ],

  }

class CilkMgr:
    def __init__(self):
        self.data = {
            're_lst' : [ ],
            }
        self.makeREs(CilkOverhead['parallel'], 'parallel');
        self.makeREs(CilkOverhead['sync'],     'sync');
        self.makeREs(CilkOverhead['cp'],       'cp');

        # special RE for handling cilk import routines
        redesc = REDesc()
        redesc.make_cilk_import()
        (self.data['re_lst']).append(redesc);
        
    def processLine(self, line, ppMgr):
        new_line = line;
        
        for redesc in (self.data['re_lst']):
            old_reObj = redesc.oldRE();
            new_re = redesc.make_newRE(ppMgr.fileNm(), ppMgr.lineNo());

            new_line = old_reObj.sub(new_re, new_line)

        if (DBG and new_line != line): print "DBG:", new_line;
        return new_line;

    # ------------------------------
    def makeREs(self, overheadFnLst, overhead_ty):
        for fn in overheadFnLst:
            redesc = REDesc()
            redesc.make_overhead(fn, overhead_ty)
            (self.data['re_lst']).append(redesc);


class REDesc:
    def __init__(self):
        self.data = {
            'old_reObj': None,
            'new_re'   : None,
            'ty_ohead' : True,
            }

    def oldRE(self):
        return self.data['old_reObj']
    def set_oldRE(self, x):
        self.data['old_reObj'] = x

    def make_newRE(self, filenm, lineno):
        if (self.isTyOverhead()):
            txt = self.newRE() % (lineno, lineno, filenm);
        else:
            txt = self.newRE() % (lineno, filenm);
        return txt;

    # ------------------------------

    def make_overhead(self, fn, overhead_ty):
        cilk2c_pre  = r'' #r';\s*'
        cilk2c_post = r'[^;]*\s*;'
        old_re = r"%s(%s%s)" % (cilk2c_pre, fn, cilk2c_post);
        self.set_oldRE(re.compile(old_re));

        lush_mark   = r'#line %s "lush:%s-overhead"' % ("%d", overhead_ty)
        lush_unmark = r'#line %d "%s"'; # line cilk-fnm
        self.set_newRE(r"\n%s\n  \1\n%s\n  " % (lush_mark, lush_unmark));

    def make_cilk_import(self):
        old_re = r"(^.*_cilk\w+_import\(.+$)";
        self.set_oldRE(re.compile(old_re));

        fix_mark = r'#line %d "%s"';
        self.set_newRE(r"\1\n%s" % (fix_mark));
        self.data['ty_ohead'] = False;

    # ------------------------------
    
    def newRE(self):
        return self.data['new_re']
    def set_newRE(self, x):
        self.data['new_re'] = x

    def isTyOverhead(self):
        return (self.data['ty_ohead']);

    def __str__(self):
        return ("old: %s\nnew: %s") % (self.oldRE().pattern, self.newRE())




class CppMgr:
    def __init__(self):
        self.data = {
            'dir_re'     : re.compile(r"^#"),
            'linedir_re' : re.compile(r'''^\#(?:line)?\s
                                             \s*(\d+)
                                             \s*(?:" ([^"]+) ")?''', re.X),
            'mr_filenm'  : None,
            'mr_lnno'    : 0, # as a number, not a string
            }

    def isDirective(self, line):
        return (self.data['dir_re']).match(line)

    def processDirLine(self, line):
        m = (self.data['linedir_re']).match(line)
        if (m):
            self.set_lineNo(long(m.group(1)))
            filenm = m.group(2)
            if (filenm):
                self.set_fileNm(filenm)
        if (DBG): print "DBG:", self;

    def processNonDirLine(self):
        if (self.data['mr_lnno']):
            self.data['mr_lnno'] += 1;

    def fileNm(self):
        return self.data['mr_filenm']
    def set_fileNm(self, x):
        self.data['mr_filenm'] = x
    
    def lineNo(self):
        return self.data['mr_lnno']
    def set_lineNo(self, x):
        self.data['mr_lnno'] = x

    def __str__(self):
        return ("%s:%s") % (self.fileNm(), self.lineNo())


def processFile(filenm):
    """..."""
    stream = openInFile(filenm)

    cilkMgr = CilkMgr()
    cppMgr = CppMgr()
    
    for line in stream:
        if (cppMgr.isDirective(line)):
            cppMgr.processDirLine(line)
        else:
            line = cilkMgr.processLine(line, cppMgr)
            cppMgr.processNonDirLine()
            
        print ("%s") % (line),
        
    closeFile(filenm, stream)


#############################################################################

if (__name__ == "__main__"):
    sys.exit(main())

