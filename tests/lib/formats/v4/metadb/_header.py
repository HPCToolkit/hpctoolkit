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

from ._context_tree import ContextTreeSection
from ._functions import FunctionsSection
from ._general_properties import GeneralPropertiesSection
from ._id_names import IdentifierNamesSection
from ._load_modules import LoadModulesSection
from ._performance_metrics import PerformanceMetricsSection
from ._source_files import SourceFilesSection

from ..._base import VersionedFormat, FileHeader
from ..._util import cached_property
import textwrap

@VersionedFormat.minimize
class MetaDB(FileHeader,
             format=b'meta', footer=b'_meta.db', majorVersion=4, minorVersion=0,
    # Added in v4.0
    General = (0,),
    IdNames = (0,),
    Metrics = (0,),
    Context = (0,),
    Strings = (0,),
    Modules = (0,),
    Files = (0,),
    Functions = (0,),
  ):
  """The meta.db file format."""
  __slots__ = ['_general', '_idNames', '_metrics', '_context',
               '_modules', '_files', '_functions']

  def _init_(self, *, general, idNames, metrics, context, modules, files,
             functions):
    super()._init_()
    self.general = (general if isinstance(general, GeneralPropertiesSection)
                    else GeneralPropertiesSection(**general))
    self.idNames = (idNames if isinstance(idNames, IdentifierNamesSection)
                    else IdentifierNamesSection(**idNames))
    self.metrics = (metrics if isinstance(metrics, PerformanceMetricsSection)
                    else PerformanceMetricsSection(**metrics))
    self.context = (context if isinstance(context, ContextTreeSection)
                    else ContextTreeSection(**context))
    self.modules = (modules if isinstance(modules, LoadModulesSection)
                    else LoadModulesSection(**modules))
    self.files = (files if isinstance(files, SourceFilesSection)
                  else SourceFilesSection(**files))
    self.functions = (functions if isinstance(functions, FunctionsSection)
                      else FunctionsSection(**functions))

  @cached_property('_general')
  @VersionedFormat.subunpack(GeneralPropertiesSection)
  def general(self, *args):
    return GeneralPropertiesSection(*args, offset=self._pGeneral)

  @cached_property('_idNames')
  @VersionedFormat.subunpack(IdentifierNamesSection)
  def idNames(self, *args):
    return IdentifierNamesSection(*args, offset=self._pIdNames)

  @cached_property('_metrics')
  @VersionedFormat.subunpack(PerformanceMetricsSection)
  def metrics(self, *args):
    return PerformanceMetricsSection(*args, offset=self._pMetrics)

  @cached_property('_context')
  @VersionedFormat.subunpack(ContextTreeSection)
  def context(self, *args):
    return ContextTreeSection(*args, offset=self._pContext,
                                     lmSec=self.modules, sfSec=self.files,
                                     fSec=self.functions)

  @cached_property('_modules')
  @VersionedFormat.subunpack(LoadModulesSection)
  def modules(self, *args):
    return LoadModulesSection(*args, offset=self._pModules)

  @cached_property('_files')
  @VersionedFormat.subunpack(SourceFilesSection)
  def files(self, *args):
    return SourceFilesSection(*args, offset=self._pFiles)

  @cached_property('_functions')
  @VersionedFormat.subunpack(FunctionsSection)
  def functions(self, *args):
    return FunctionsSection(*args, offset=self._pFunctions,
                                   lmSec=self.modules, sfSec=self.files)

  def __eq__(self, other):
    if not isinstance(other, MetaDB): return NotImplemented
    return (self.general == other.general
            and self.idNames == other.idNames
            and self.metrics == other.metrics
            and self.context == other.context
            and self.modules == other.modules
            and self.files == other.files
            and self.functions == other.functions)

  def __repr__(self):
    return (f"{self.__class__.__name__}("
            f"general={self.general!r}, "
            f"idNames={self.idNames!r}, "
            f"metrics={self.metrics!r}, "
            f"context={self.context!r}, "
            f"modules={self.modules!r}, "
            f"files={self.files!r}, "
            f"functions={self.functions!r})")

  def __str__(self):
    return (f"MetaDB v4.{self.minorVersion}:\n"
            " General Properties:\n"
            + textwrap.indent(str(self.general), '  ') + "\n"
            " Hierarchical Identifier Names:\n"
            + textwrap.indent(str(self.idNames), '  ') + "\n"
            " Performance Metrics:\n"
            + textwrap.indent(str(self.metrics), '  ') + "\n"
            " Load Modules:\n"
            + textwrap.indent(str(self.modules), '  ') + "\n"
            " Source Files:\n"
            + textwrap.indent(str(self.files), '  ') + "\n"
            " Functions:\n"
            + textwrap.indent(str(self.functions), '  ') + "\n"
            " Context Tree:\n"
            + textwrap.indent(str(self.context), '  ')
           )

  pack, pack_into = None, None
