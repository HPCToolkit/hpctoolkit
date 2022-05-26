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

  execution = PropagationScope(name='execution', type='execution')
  function = PropagationScope(name='function', type='transitive', propagationIndex=0)
  point = PropagationScope(name='point', type='point')

  foo_c = { 'path': 'src/tmp/foo.c', 'flags': ['copied'], }
  foo_exe = { 'path': '/tmp/foo', }

  main = { 'name': 'main',
           'module': foo_exe, 'offset': 0x1183,
           'file': foo_c, 'line': 12,
         }
  foo  = { 'name': 'foo',
           'module': foo_exe, 'offset': 0x1129,
           'file': foo_c, 'line': 3,
         }
  bar  = { 'name': 'bar',
           'module': foo_exe, 'offset': 0x1172,
           'file': foo_c, 'line': 8,
         }

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
      'scopes': [execution, function, point],
      'metrics': [
        { 'name': 'cycles',
          'scopeInsts': [
            { 'propMetricId': 17, 'scope': execution, },
            { 'propMetricId': 16, 'scope': function, },
            { 'propMetricId': 15, 'scope': point, },
          ],
          'summaries': [
            { 'statMetricId': 29,
              'scope': execution, 'formula': '$$', 'combine': 'max',
            },
            { 'statMetricId': 26,
              'scope': execution, 'formula': '$$', 'combine': 'min',
            },
            { 'statMetricId': 20,
              'scope': execution, 'formula': '$$', 'combine': 'sum',
            },
            { 'statMetricId': 23,
              'scope': execution, 'formula': '($$^2)', 'combine': 'sum',
            },
            { 'statMetricId': 17,
              'scope': execution, 'formula': '1', 'combine': 'sum',
            },

            { 'statMetricId': 28,
              'scope': function, 'formula': '$$', 'combine': 'max',
            },
            { 'statMetricId': 25,
              'scope': function, 'formula': '$$', 'combine': 'min',
            },
            { 'statMetricId': 19,
              'scope': function, 'formula': '$$', 'combine': 'sum',
            },
            { 'statMetricId': 22,
              'scope': function, 'formula': '($$^2)', 'combine': 'sum',
            },
            { 'statMetricId': 16,
              'scope': function, 'formula': '1', 'combine': 'sum',
            },

            { 'statMetricId': 27,
              'scope': point, 'formula': '$$', 'combine': 'max',
            },
            { 'statMetricId': 24,
              'scope': point, 'formula': '$$', 'combine': 'min',
            },
            { 'statMetricId': 18,
              'scope': point, 'formula': '$$', 'combine': 'sum',
            },
            { 'statMetricId': 21,
              'scope': point, 'formula': '($$^2)', 'combine': 'sum',
            },
            { 'statMetricId': 15,
              'scope': point, 'formula': '1', 'combine': 'sum',
            },
          ],
        },
        { 'name': 'instructions',
          'scopeInsts': [
            { 'propMetricId': 2, 'scope': execution, },
            { 'propMetricId': 1, 'scope': function, },
            { 'propMetricId': 0, 'scope': point, },
          ],
          'summaries': [
            { 'statMetricId': 14,
              'scope': execution, 'formula': '$$', 'combine': 'max',
            },
            { 'statMetricId': 11,
              'scope': execution, 'formula': '$$', 'combine': 'min',
            },
            { 'statMetricId': 5,
              'scope': execution, 'formula': '$$', 'combine': 'sum',
            },
            { 'statMetricId': 8,
              'scope': execution, 'formula': '($$^2)', 'combine': 'sum',
            },
            { 'statMetricId': 2,
              'scope': execution, 'formula': '1', 'combine': 'sum',
            },

            { 'statMetricId': 13,
              'scope': function, 'formula': '$$', 'combine': 'max',
            },
            { 'statMetricId': 10,
              'scope': function, 'formula': '$$', 'combine': 'min',
            },
            { 'statMetricId': 4,
              'scope': function, 'formula': '$$', 'combine': 'sum',
            },
            { 'statMetricId': 7,
              'scope': function, 'formula': '($$^2)', 'combine': 'sum',
            },
            { 'statMetricId': 1,
              'scope': function, 'formula': '1', 'combine': 'sum',
            },

            { 'statMetricId': 12,
              'scope': point, 'formula': '$$', 'combine': 'max',
            },
            { 'statMetricId': 9,
              'scope': point, 'formula': '$$', 'combine': 'min',
            },
            { 'statMetricId': 3,
              'scope': point, 'formula': '$$', 'combine': 'sum',
            },
            { 'statMetricId': 6,
              'scope': point, 'formula': '($$^2)', 'combine': 'sum',
            },
            { 'statMetricId': 0,
              'scope': point, 'formula': '1', 'combine': 'sum',
            },
          ],
        },
      ],
    },
    modules = {
      'modules': [foo_exe],
    },
    files = {
      'files': [foo_c],
    },
    functions = {
      'functions': [main, foo, bar],
    },
    context = {
      'entryPoints': [
        { 'entryPoint': 'unknown_entry', 'prettyName': 'unknown entry', 'ctxId': 7 },
        { 'entryPoint': 'main_thread', 'prettyName': 'main thread', 'ctxId': 1,
          'children': [
            { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 2,
              'function': main,
              'children': [
                { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'line', 'ctxId': 3,
                  'file': foo_c, 'line': 13,
                  'children': [
                    { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 5,
                      'function': foo,
                      'children': [
                        { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'loop', 'ctxId': 6,
                          'file': foo_c, 'line': 5,
                          'children': [
                            { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'line', 'ctxId': 7,
                              'file': foo_c, 'line': 5,
                            },
                          ],
                        },
                        { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'line', 'ctxId': 9,
                          'file': foo_c, 'line': 3,
                        },
                      ],
                    },
                    { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 12,
                      'function': bar,
                      'children': [
                        { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'line', 'ctxId': 13,
                          'file': foo_c, 'line': 9,
                          'children': [
                            { 'relation': 'call', 'lexicalType': 'function', 'ctxId': 15,
                              'function': foo,
                              'children': [
                                { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'loop', 'ctxId': 16,
                                  'file': foo_c, 'line': 5,
                                  'children': [
                                    { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'line', 'ctxId': 17,
                                      'file': foo_c, 'line': 5,
                                    },
                                  ],
                                },
                                { 'relation': 'lexical', 'propagation': 1, 'lexicalType': 'line', 'ctxId': 20,
                                  'file': foo_c, 'line': 3,
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
