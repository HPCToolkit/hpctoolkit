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

from . import *

from pathlib import Path

testdatadir = Path(__file__).parent.parent / 'testdata'

def test_small_v4_0():
  a = MetaDB(open(testdatadir/'small_v4.0'/'meta.db'))

  main = { 'name': 'main',
           'module': { 'path': '/tmp/foo', }, 'offset': 0x1187,
           'file': { 'path': '/tmp/foo.c', }, 'line': 15,
         }
  foo  = { 'name': 'foo',
           'module': { 'path': '/tmp/foo', }, 'offset': 0x1129,
           'file': { 'path': '/tmp/foo.c', }, 'line': 6,
         }
  bar  = { 'name': 'bar',
           'module': { 'path': '/tmp/foo', }, 'offset': 0x1176,
           'file': { 'path': '/tmp/foo.c', }, 'line': 11,
         }
  _stext = { 'name': '_stext [vmlinux]',
             'module': { 'path': '<vmlinux.007f0101>', }, 'offset': 0xffffffffa7800000,
           }

  x_children = []
  for id in reversed(range(12, 39, 2)):
    x_children = [{
      'relation': 'call', 'lexicalType': 'function', 'ctxId': id,
      'function': _stext,
      'children': x_children,
    }]
  y_children = []
  for id in reversed(range(49, 70, 2)):
    y_children = [{
      'relation': 'call', 'lexicalType': 'function', 'ctxId': id,
      'function': _stext,
      'children': y_children,
    }]


  b = MetaDB(
    general = {
      'title': 'foo',
      'description': 'TODO database description',
    },
    idNames = {
      'names': ['SUMMARY', 'NODE', 'RANK', 'THREAD', 'GPUDEVICE', 'GPUCONTEXT',
                'GPUSTREAM', 'CORE'],
    },
    metrics = {
      'metrics': [
        { 'name': 'cycles',
          'scopes': [
            { 'scope': 'point', 'propMetricId': 15,
              'summaries': [
                 {'formula': '$$',     'combine': 'sum', 'statMetricId': 18},
                 {'formula': '($$^2)', 'combine': 'sum', 'statMetricId': 21},
                 {'formula': '1',      'combine': 'sum', 'statMetricId': 15},
                 {'formula': '$$',     'combine': 'min', 'statMetricId': 24},
                 {'formula': '$$',     'combine': 'max', 'statMetricId': 27},
              ],
            },
            { 'scope': 'function', 'propMetricId': 16,
              'summaries': [
                 {'formula': '$$',     'combine': 'sum', 'statMetricId': 19},
                 {'formula': '($$^2)', 'combine': 'sum', 'statMetricId': 22},
                 {'formula': '1',      'combine': 'sum', 'statMetricId': 16},
                 {'formula': '$$',     'combine': 'min', 'statMetricId': 25},
                 {'formula': '$$',     'combine': 'max', 'statMetricId': 28},
              ],
            },
            { 'scope': 'execution', 'propMetricId': 17,
              'summaries': [
                 {'formula': '$$',     'combine': 'sum', 'statMetricId': 20},
                 {'formula': '($$^2)', 'combine': 'sum', 'statMetricId': 23},
                 {'formula': '1',      'combine': 'sum', 'statMetricId': 17},
                 {'formula': '$$',     'combine': 'min', 'statMetricId': 26},
                 {'formula': '$$',     'combine': 'max', 'statMetricId': 29},
              ],
            },
          ],
        },
        { 'name': 'instructions',
          'scopes': [
            { 'scope': 'point', 'propMetricId': 0,
              'summaries': [
                 {'formula': '$$',     'combine': 'sum', 'statMetricId': 3},
                 {'formula': '($$^2)', 'combine': 'sum', 'statMetricId': 6},
                 {'formula': '1',      'combine': 'sum', 'statMetricId': 0},
                 {'formula': '$$',     'combine': 'min', 'statMetricId': 9},
                 {'formula': '$$',     'combine': 'max', 'statMetricId': 12},
              ],
            },
            { 'scope': 'function', 'propMetricId': 1,
              'summaries': [
                 {'formula': '$$',     'combine': 'sum', 'statMetricId': 4},
                 {'formula': '($$^2)', 'combine': 'sum', 'statMetricId': 7},
                 {'formula': '1',      'combine': 'sum', 'statMetricId': 1},
                 {'formula': '$$',     'combine': 'min', 'statMetricId': 10},
                 {'formula': '$$',     'combine': 'max', 'statMetricId': 13},
              ],
            },
            { 'scope': 'execution', 'propMetricId': 2,
              'summaries': [
                 {'formula': '$$',     'combine': 'sum', 'statMetricId': 5},
                 {'formula': '($$^2)', 'combine': 'sum', 'statMetricId': 8},
                 {'formula': '1',      'combine': 'sum', 'statMetricId': 2},
                 {'formula': '$$',     'combine': 'min', 'statMetricId': 11},
                 {'formula': '$$',     'combine': 'max', 'statMetricId': 14},
              ],
            },
          ],
        },
      ],
    },
    modules = {
      'modules': [
        { 'path': '/tmp/foo', },
        { 'path': '<vmlinux.007f0101>', },
      ],
    },
    files = {
      'files': [
        { 'path': '/tmp/foo.c', },
      ],
    },
    functions = {
      'functions': [
        { 'name': '<program root>', },
        { 'name': '<no activity>', },
        main, foo, bar, _stext,
      ],
    },
    context = {
      'roots': [
        { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 73, },
        { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 1,
          'function': { 'name': '<no activity>', },
        },
        { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 2,
          'function': { 'name': '<program root>', },
          'children': [
            { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 3,
              'function': main,
              'children': [
                { 'relation': 'lexical', 'lexicalType': 'line', 'ctxId': 4,
                  'file': { 'path': '/tmp/foo.c', }, 'line': 17,
                  'children': [
                    { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 6,
                      'function': foo,
                      'children': [
                        { 'relation': 'lexical', 'lexicalType': 'line', 'ctxId': 7,
                          'file': { 'path': '/tmp/foo.c', }, 'line': 8,
                          'children': x_children,
                        },
                      ],
                    },
                  ],
                },
                { 'relation': 'lexical', 'lexicalType': 'line', 'ctxId': 40,
                  'file': { 'path': '/tmp/foo.c', }, 'line': 18,
                  'children': [
                    { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 42,
                      'function': bar,
                      'children': [
                        { 'relation': 'lexical', 'lexicalType': 'line', 'ctxId': 43,
                          'file': { 'path': '/tmp/foo.c', }, 'line': 13,
                          'children': [
                            { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 45,
                              'function': foo,
                              'children': [
                                { 'relation': 'lexical', 'lexicalType': 'line', 'ctxId': 46,
                                  'file': { 'path': '/tmp/foo.c', }, 'line': 8,
                                  'children': y_children,
                                },
                              ],
                            },
                          ],
                        },
                      ],
                    },
                  ],
                },
              ],
            },
          ],
        },
      ],
    },
  )

  assert str(a) == str(b)
  assert a.context.identical(b.context)
  assert a.identical(b)

  assert 'object at 0x' not in repr(a)
  c = eval(repr(a))
  assert str(a) == str(c)
  assert a.identical(c)
