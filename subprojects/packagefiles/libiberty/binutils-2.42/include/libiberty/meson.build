# SPDX-FileCopyrightText: 2024 Contributors to the HPCToolkit Project
#
# SPDX-License-Identifier: MIT

fs = import('fs')

iface_inc = include_directories('..')
iface_sources = []
foreach filename : files(
  '../ansidecl.h',
  '../demangle.h',
  '../dyn-string.h',
  '../fibheap.h',
  '../floatformat.h',
  '../hashtab.h',
  '../libiberty.h',
  '../objalloc.h',
  '../partition.h',
  '../safe-ctype.h',
  '../sort.h',
  '../splay-tree.h',
  '../timeval-utils.h',
)
  iface_sources += [fs.copyfile(filename)]
endforeach
