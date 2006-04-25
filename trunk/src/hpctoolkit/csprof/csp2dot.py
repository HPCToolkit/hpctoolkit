#!/usr/bin/env python

import struct
import sys
import string
import os
import operator
import bisect
import profile
import copy
import getopt
import gc

MAGIC_STR_LEN = 10
VERSION_STR_LEN = 5

# this variable is somewhat misnamed; what it does is control the
# combining of nodes during dot graph creation.  if it is false,
# nodes are combined only on the basis of their program counters
# (so some context information is lost).  if it is true, nodes are
# distinguished both by stack pointer and instruction pointer (thus
# turning the resulting graph into more of a tree structure).
distinguish_ips = 0

# cleaning up hideously awful C++ function names
nuke_template_args = 0

# where to find load modules of various kinds
binary_path = []

# whether metrics contain descriptive flags or not
metrics_have_flags = 0

nm_program = "/usr/bin/nm"

# a table which supplies the needed options for 'nm', keyed by values
# of sys.platform.  If you want to add a new platform, you'll need to
# figure out at least three things:
#
# * how to print symbol addresses in hexadecimal;
# * how to print symbol information in BSD-format (space-separated
#   address/section/name).  Any symbols in the '.text' section should
#   have 't' or 'T' as their 'section'.
# * how to sort symbols by address.
#
# Support for the BSD format is mainly because this script was
# developed on a Tru64 system, which can print it in such a fashion.
# Platforms with the GNU toolchain have an 'nm' which also understands
# printing symbol info in the BSD format.
nm_options_table = {}
# Linux needs the added '-D' option to access minisymbols
# '-C' demangles C++ symbol names
nm_options_table['linux2'] = "-B -v -t x -C"
nm_options_table['linux2-stripped'] = "-B -v -t x -D -C"
nm_options_table['osf1V5'] = "-B -v -t x"
nm_options_table['osf1V5-stripped'] = "-B -v -t x"

# ugh, FIXME:
address_twiddlage = {}
address_twiddlage['linux2'] = 1
address_twiddlage['osf1V5'] = 0

def get_nm_options(is_program):
    platform = sys.platform

    if(not is_program):
        platform = platform + "-stripped"

    options = nm_options_table.get(platform, "")

    if options == "":
        print "Error: do not know about nm options for %s" % platform
        sys.exit(1)
    else:
        return options
        
print_useful_graph = 0
print_edge_counts = 0

# ratio of samples taken in a particular function to total samples for
# said function to be considered "useful"
sample_threshold = 0.10

# the font name to be used in node labels
node_font = "Times"

# namespaces to be stripped from C++ functions and arguments
stripped_namespaces = []

# the name of the node to be considered as the "root" for printing call chains
root_node_name = None

def find_file(filename):
    "Search for filename in the global variable 'binary_path'."
    if os.path.isabs(filename):
        # no sense in looking for it
        return filename
    else:
        for prefix in binary_path:
            true_file = os.path.normpath(os.path.join(prefix, filename))

            if os.path.exists(true_file):
                return true_file

        if os.path.exists(filename):
            # last ditch effort
            return filename
        else:
            raise NameError, "Could not find %s in binary_path" % filename

# load modules (the executable binary and/or shared libraries)

# working with nm and load modules of various kinds
#
# you'd think we'd do caching in here, rather than in Epoch, since
# LoadModules are shared across Epochs.  experiments show this is
# a horrible idea, so caching of address lookup is only in Epoch.
# adding it to LoadModule *and* Epoch does very little for a speed
# increase.
class LoadModule:
    def __init__(self, filename, vaddr, mapaddr):
        print "load module %s @ %x" % (filename, mapaddr)
        self.filename = find_file(filename)
        self.text_section_start = None
        self.vaddr = vaddr
        self.mapaddr = mapaddr
        self.addresses = []
        self.function_names = []
        self.is_binary = self.is_fully_linked(self.filename)
        self.parse_nm_output(self.filename)

    def __str__(self):
        return self.filename

    def __cmp__(self, other):
        return cmp(self.addresses[0] + self.mapaddr,
                   other.addresses[1] + other.mapaddr)
    
    # expects `line' to be in BSD nm style
    def parse_nm_line(self, line):
        # C++ function names might have spaces in them due to
        # carrying along argument type information, so we limit
        # the number of splits
        fields = string.split(line, " ", 2)

        if len(fields) < 3:
            return
    
        hexaddr = fields[0]
        section = fields[1]
        func_name = fields[2]

        # check to see if we're in the text section
        if section == 'T' or section == 't':
            func_name = string.strip(func_name)
            hexaddr = long(string.strip(hexaddr), 16)

            if self.text_section_start is None:
                self.text_section_start = hexaddr
            
            offset = hexaddr - self.vaddr

            assert offset >= 0, \
                   "Invalid virtual address %x specified for module '%s'" % \
                   (hexaddr, self.filename)

            self.addresses.append(offset)

            # do common stripping in one place.  this means that all nodes
            # don't have to do this and therefore the nodes can just have
            # references into the LoadModule's table of functions.
            #
            # this change kinda-sorta makes the csp file from "parser"
            # on the SPEC benchmark suite work

            if nuke_template_args:
                func_name = remove_template_args(func_name)

            for namespace in stripped_namespaces:
                func_name = string.replace(func_name, namespace, "")
        
            self.function_names.append(func_name)

    def parse_nm_output(self, program_filename):
        nm_invocation = "%s %s %s" % (nm_program, get_nm_options(self.is_binary), program_filename)
        nm_stream = os.popen(nm_invocation, 'r')

        for line in nm_stream.readlines():
            self.parse_nm_line(line)

        nm_stream.close()

        if len(self.addresses) == 0:
            # assume this means that nm wasn't able to find the module
            print "Unable to locate '%s' from the current directory" % program_filename
            sys.exit(1)

    def is_fully_linked(self, filename):
        """Determines whether 'filename' is fully linked (and thus a program)
        or is not fully linked (possibly stripped) and therefore a library."""
        # assume that libraries on the target platform are stripped
        is_stripped = filename.find("/lib/")

        if(is_stripped != -1):
            return 0
        else:
            is_stripped = filename.find("/lib64/")

            if(is_stripped != -1):
                return 0

        return 1
        
    def highest_address(self):
        if self.is_binary and address_twiddlage[sys.platform]:
            highest_address = self.addresses[-1]
        else:
            highest_address = self.addresses[-1] + self.mapaddr

        return highest_address
    
    def find_function_name_for_address(self, address):
        if self.is_binary and address_twiddlage[sys.platform]:
            func_offset = address
        else:
            func_offset = address - self.mapaddr

        index = bisect.bisect(self.addresses, func_offset)

        if index == len(self.addresses):
            return None
        else:
            return self.function_names[index-1]

class Epoch:
    def __init__(self, load_modules):
        self.load_modules = load_modules
        # make sure things appear in sorted order
        self.load_modules.sort()
        self.cached_addresses = {}

    def find_function_name_for_address(self, address):
        if self.cached_addresses.has_key(address):
            return self.cached_addresses[address]

        for module in self.load_modules:
            # the address cannot possibly be in this load module
            if address > module.highest_address():
                continue

            name = module.find_function_name_for_address(address)

            if name is None:
                # didn't find it :(
                continue
            else:
                ret_tuple = (name, module)
                
                self.cached_addresses[address] = ret_tuple

                return ret_tuple

        # bracket it with dot comments
        print "/* Unknown address: %x */" % address
        return ("Unknown_library_function", None)


# reading hpcfile files
class FileObject:
    def __init__(self, format_string, stream, format):
        self.format_string = format + format_string
        len = struct.calcsize(self.format_string)
        self.data = stream.read(len)

    def unpack(self):
        print self.format_string, [ i for i in self.data ]
        return struct.unpack(self.format_string, self.data)

class FileString(FileObject):
    def __init__(self, stream, format):
        FileObject.__init__(self, "II", stream, format)
        (self.tag, self.length) = self.unpack()
        self.string = stream.read(self.length)

    def __str__(self):
        return self.string

class FileNumber(FileObject):
    def __init__(self, stream, format):
        FileObject.__init__(self, "IQ", stream, format)
        (self.tag, self.num) = self.unpack()

    def __str__(self):
        return "%d" % self.num

class FileHeader(FileObject):
    def __init__(self, stream):
        global metrics_have_flags
        # UGH FIXME
        FileObject.__init__(self, "10s5scQ", stream, '<')
        (self.magic, self.version, self.endian, self.num_chunks) \
                     = self.unpack()
        if self.version == "01.01":
            metrics_have_flags = 1

class TreeHeader(FileObject):
    def __init__(self, stream, format):
        FileObject.__init__(self, "10s5scIIQI", stream, format)

        (self.magic, self.version, self.endian, self.vma_size, \
         self.uint_size, self.num_nodes, self.epoch) = self.unpack()

    def vma_format(self):
        if self.vma_size == 4:
            return "I"
        else:
            return "Q"

    def uint_format(self):
        if self.uint_size == 4:
            return "I"
        else:
            return "Q"

def read_value(format_string, stream):
    return struct.unpack(format_string, stream.read(struct.calcsize(format_string)))[0]

class ProfilingMetric(FileObject):
    def __init__(self, stream, format):
        self.name = FileString(stream, format)
        print "grabbing metric flags", metrics_have_flags
        if metrics_have_flags:
            number = FileNumber(stream, format)
            self.flags = number.num
        number = FileNumber(stream, format)
        self.period = number.num

    def __str__(self):
        return "<%s, period %d>" % (self.name, self.period)

class ProfilingData(FileObject):
    def __init__(self, stream, format):
        self.target = FileString(stream, format)
        num_metrics = read_value(format + "I", stream)
        self.metrics = [ 0 ] * num_metrics
        for i in range(num_metrics):
            self.metrics[i] = ProfilingMetric(stream, format)
    def __str__(self):
        return "<%s, %d metrics>" % (self.target, len(self.metrics))

class TreeNode:
    def __init__(self, data, node_format, epoch):
        # node_format being mutable means that we can't let Python do
        # the unpacking for us; we have to do it all manually
        x = struct.unpack(node_format, data)
        self.id = int(x[0])
        self.parent = int(x[1])
        self.ip = x[2]
        self.sp = x[3]
        self.weight = x[4]
        if len(x) > 5:
            self.calls = x[5]
        if len(x) > 6:
            self.extra_metrics = list(x)[6:]

        self.children = []
        self.locked = 0

        #print "node ip %x | sp %x | weight %d | calls %d" % (self.ip, self.sp, self.weight, self.calls)
        # these are used all over the place
        (self.func_name, self.load_module) = \
                         epoch.find_function_name_for_address(self.ip)

        self.dot_name = dot_node_name(self)

    def graph_name(self):
        if distinguish_ips != 0:
            return self.dot_name
        else:
            return self.func_name

    def is_grouping(self):
        return 0
    
    def __eq__(self, other):
        return (self.load_module is other.load_module) \
               and (self.ip == other.ip)
    
    def __str__(self):
        return self.func_name

class GroupingNode(TreeNode):
    def __init__(self, group_name, load_module):
        # somehow these should really be filled in
        self.calls = 0
        self.weight = 0
        self.sp = 0
        self.ip = 0

        self.locked = 0
        self.func_name = group_name
        self.load_module = load_module
        self.dot_name = dot_node_name(self)
        pass

    def is_grouping(self):
        return 1

class Parser:
    def __init__(self, filename, builder):
        self.stream = open(filename, 'r')
        self.builder = builder
        self.struct_format = '<'
        self.cached_epoch_modules = {}
        
    def parse(self):
        header = FileHeader(self.stream)
        if header.endian == 'l':
            self.struct_format = '<'
        else:
            self.struct_format = '>'

        self.profiling_data = ProfilingData(self.stream, self.struct_format)

        self.read_epochs(self.stream)
        self.read_trees(self.stream)

        return self.builder.get_result()

    def read_profiling_data(self, stream):
        profiling_data = ProfilingData(self.stream, self.struct_format)

        self.builder.profiling_data(profiling_data)

    def read_epochs(self, stream):
        n_epochs = read_value(self.struct_format + "I", stream);

        self.builder.begin_epochs(n_epochs)

        for i in range(n_epochs):
            e = self.read_epoch(stream)
            
            self.builder.add_epoch(e)

    def read_epoch(self, stream):
        (n_modules,) = struct.unpack("I", stream.read(struct.calcsize("I")))

        modules = [ 0 ] * n_modules

        for i in range(n_modules):
            modules[i] = self.read_epoch_module(stream)

        return Epoch(modules)

    def read_epoch_module(self, stream):
        (namelen,) = struct.unpack("I", stream.read(struct.calcsize("I")))

        # FIXME: these 'Q's should be conditional on the vma_size
        # of the data file, but we don't have that information when
        # we start parsing epochs.
        module_format = self.struct_format + "%dsQQ" % namelen;
        module_len = struct.calcsize(module_format)
    
        (name,vaddr,mapaddr) = struct.unpack(module_format, stream.read(module_len))

        tuple_key = (name, vaddr  , mapaddr)

        #if name.find("lib") == -1:
        #    print '$$$', os.path.basename(name)
            
        if self.cached_epoch_modules.has_key(tuple_key):
            return self.cached_epoch_modules[tuple_key]
        else:
            try:
                module = LoadModule(name, vaddr, mapaddr)
            except NameError:
                print sys.exc_value
                sys.exit(1)

        self.cached_epoch_modules[tuple_key] = module

        return module

    def read_trees(self, stream):
        (n_trees,unsafe) = struct.unpack("<IQ", stream.read(struct.calcsize("<IQ")))

        self.builder.unsafe_samples(unsafe)
        
        for i in range(n_trees):
            tree_header = TreeHeader(stream, self.struct_format)

            self.builder.begin_tree(tree_header)

            # FIXME: this would be nice to have
            #assert tree_header.epoch < len(epochs), \
            #       "Invalid epoch number '%d' in tree header" % tree_header.epoch

            tree_epoch = self.builder.get_epoch(tree_header.epoch)

            if not(tree_header.version == "01.00" or tree_header.version == "01.0T"):
                print "Error in csp file: tree header '%s' invalid" % tree_header.version
                sys.exit(1)

            # how many extra values hang off a node?
            n_values = 4 + len(self.profiling_data.metrics)

            node_format = self.struct_format + tree_header.vma_format() * n_values

            self.read_tree_nodes(stream, tree_header, tree_epoch, node_format)

            self.builder.end_tree()

    def read_tree_nodes(self, stream, tree_header, tree_epoch, node_format):
        # slurp all the data in at once to avoid lots of system call
        # overhead for large trees
        nodelen = struct.calcsize(node_format)
        treenode_data = stream.read(nodelen*tree_header.num_nodes)

        for j in xrange(tree_header.num_nodes):
            # ugh. the combination of immutable strings and struct.unpack
            # not taking an index into the string means that we have to
            # perform this pointless allocation over and over and over
            data = treenode_data[j*nodelen:(j+1)*nodelen]
            node = TreeNode(data, node_format, tree_epoch)

            if node.parent >= (tree_header.num_nodes):
                print "Invalid parent value: %d" % node.parent
                sys.exit(1)

            self.builder.add_node(node)

class Builder:
    def __init__(self):
        self.epochs = []
        self.epoch_index = None
        self.trees = []
        self.unsafe = 0

    def unsafe_samples(self, x):
        self.unsafe = x

    def profiling_data(self, x):
        self.profiling_data = x
        
    def add_node(self, node):
        pass

    def begin_tree(self, header):
        pass

    def end_tree(self):
        pass

    def begin_epochs(self, n_epochs):
        self.epochs = [ 0 ] * n_epochs
        self.epoch_index = 0
    
    def add_epoch(self, epoch):
        self.epochs[self.epoch_index] = epoch
        self.epoch_index = self.epoch_index + 1

    def get_epoch(self, index):
        if index > len(self.epochs):
            raise IndexError, "Epoch %d is not valid" % index
        return self.epochs[index]

    def end_epochs(self):
        pass
    
    def get_result(self):
        pass

class TreeBuilder(Builder):
    def __init__(self, collapse_common_children=0):
        Builder.__init__(self)
        self.nodes = []
        self.node_index = 0
        # if 'p' has children 'n1' and 'n2' such that
        # n1.func_name == n2.func_name, collapse those nodes
        self.collapse_common_children = collapse_common_children

    def begin_tree(self, header):
        self.nodes = [0] * header.num_nodes
        self.node_index = 0

    def end_tree(self):
        self.trees.append(self.nodes)
        
    def add_node(self, node):
        parent = self.nodes[node.parent]
        self.nodes[self.node_index] = node

        if parent is not 0:
            # build the tree explicitly
            node.parent = parent
            parent.children.append(node)
            node.dot_name = dot_node_name(node, parent)
        else:
            # uniquely identify the root node
            node.parent = None

        self.node_index = self.node_index + 1

    def get_result(self):
        return (self.epochs, self.trees)

class StatisticsBuilder(Builder):
    def __init__(self):
        Builder.__init__(self)
        self.total_samples = 0
        self.total_returns = 0
        self.aggregate_samples = {}
        self.trees = []

    def begin_tree(self, header):
        self.child_counts = [0] * header.num_nodes
        
    def add_node(self, node):
        self.total_samples = self.total_samples + node.weight
        self.total_returns = self.total_returns + node.calls

        self.child_counts[node.parent] = self.child_counts[node.parent] + 1
        
        # get a flat profile, too
        func = node.func_name
        
        self.aggregate_samples[func] = self.aggregate_samples.get(func, 0) + node.weight

    def end_tree(self):
        self.trees.append(self.child_counts)
        
    def get_result(self):
        return (self.unsafe, self.total_samples, self.total_returns, self.aggregate_samples, self.trees)


def remove_caret_info(s):
    p = s.find("^")

    if p == -1:
        return s
    else:
        return s[:p]

class DotGraphContext:
    def __init__(self, tree_nodes):
        self.edge_counts = {}
        self.info = {}
        self.max_samples = 0
        self.nodes = tree_nodes

        self.compute_common_information()
        self.compute_maximum_sample()

        self.compute_edge_counts()

    def compute_common_information(self):
        default = ([], 0)
        
        for node in self.nodes:
            func_name = node.graph_name()

            (nodes,weight) = self.info.get(func_name, default)

            if len(nodes) == 0:
                # nothing's been added yet
                self.info[func_name] = ([node], node.weight)
            else:
                nodes.append(node)
                self.info[func_name] = (nodes, weight+node.weight)
    
    # abbreviation for 'aggregate_weight'
    def aggregate_weight(self, name):
        (nodes,weight) = self.info.get(name, ([], 0))

        return weight

    def get_common_nodes(self, name):
        (nodes,weight) = self.info.get(name, ([], 0))

        return nodes
    
    def compute_maximum_sample(self):
        max_sample = -1

        for (nodes,weight) in self.info.itervalues():
            max_sample = max(max_sample, weight)

        self.max_samples = max_sample

    def compute_edge_counts(self):
        for node in self.nodes:
            if node.parent is not None:
                edge = self.construct_edge(node, node.parent)

                self.edge_counts[edge] = self.edge_counts.get(edge, 0) + node.calls

    def node_weight(self, node):
        return self.aggregate_weight(node.graph_name())

    def print_dot_node(self, stream, node):
        func_name = node.func_name
        node_name = node.graph_name()

        if(not self.nodes_printed.has_key(node_name)):
            weight = self.node_weight(node)

            if weight != 0:
                label = func_name + " @ " + str(weight)
            else:
                label = func_name

            importance = float(weight)/float(self.max_samples)

            self.print_dot_node_star(stream, node_name, label, importance)
            self.nodes_printed[node_name] = 1

    def construct_edge(self, node, parent):
        node_name = node.graph_name()
        parent_node_name = parent.graph_name()

        return (parent_node_name, node_name)

    def print_dot_node_star(self, stream, name, label, importance):
        print >> stream, '"%s" [ label="%s", style=filled, fillcolor="0.0,%f,1.0" ]' \
              % (name, label, importance)

    def total_calls_beneath(self, node):
        n_calls = 0

        for callee in node.children:
            edge = self.construct_edge(callee, node)
            n_calls = n_calls + self.edge_counts.get(edge)

        return n_calls

    def print_dot_edge(self, stream, node):
        parent_node = node.parent

        # node is the root, don't bother with it
        if parent_node is not None:
            edge_tuple = self.construct_edge(node, parent_node)

            if(not self.edges_printed.has_key(edge_tuple)):
                calls = self.edge_counts[edge_tuple]
                parent_n_calls = self.total_calls_beneath(parent_node)

                if parent_n_calls == 0:
                    brightness = float(0)              # black
                else:
                    brightness = 1.0 - float(calls)/float(parent_n_calls)

                if print_edge_counts:
                    # draw a box representing call counts
                    call_count_node_name = "%s-%s" % edge_tuple
                
                    print >> stream, '"%s" [ shape=box, label="%d calls" ]' % (call_count_node_name, calls)

                    # draw the edges to the call count box
                    print >> stream, '"%s" -> "%s" []' % (edge_tuple[0], call_count_node_name)
                    print >> stream, '"%s" -> "%s" []' % (call_count_node_name, edge_tuple[1])
                else:
                    # draw the actual edge
                    print >> stream, '"%s" -> "%s" []' % edge_tuple
                    #print >> stream, '[ color="0.0,0.0,%f" ]' % brightness
                
                self.edges_printed[edge_tuple] = 1

    def count_function_occurrences(self, stream):
        # print the information to the dot file
        for (name, nodes) in self.common_nodes.items():
            print >> stream, "/* %s: %d nodes, %d samples */" % (name, len(nodes), aggregate_weight(name))

    def count_function_calls(self, stream):
        for (edge, calls) in self.edge_counts.items():
            print >> stream, "/* %s -> %s:" % edge, "%d calls */" % calls
        
    def print_call_chain(self, stream, node):
        self.print_dot_node(stream, node)

        while not is_root_node(node):
            self.print_dot_edge(stream, node)

            node = node.parent

            self.print_dot_node(stream, node)

    def print_graph_info(self, stream):
        print >> stream, "/* number of nodes: %s */" % str(len(self.nodes))

        # this is kind of wasteful
        total_samples = reduce(lambda x, y: x + y.weight, self.nodes, 0)
        print >> stream, "/* number of samples: %d */" % total_samples

        total_returns = reduce(lambda x, y: x + y.calls, self.nodes, 0)
        print >> stream, "/* number of returns: %d */" % total_returns
        
    def print_useful_call_chains(self, stream):
        for (func_name, (nodes, weight)) in self.info.items():
            if float(weight)/self.max_samples > sample_threshold:
                nodes.sort(nodecmp)
                n_nodes = len(nodes)

                for i in xrange(max(1, n_nodes)):
                    node = find_grouped_node(nodes[i])

                    self.print_call_chain(stream, node)

#         for (func, func_samples) in self.aggregate_weight.items():
#             if float(func_samples)/self.max_samples > sample_threshold:
#                 func_nodes = get_common_nodes(func)
#                 # FIXME: this should be moved someplace else to avoid
#                 # spurious sorting
#                 func_nodes.sort(nodecmp)
#                 n_nodes = len(func_nodes)

#                 # FIXME: `n_nodes' used to be `n_nodes/2'; for large
#                 # programs like R, this makes a difference in the graph
#                 # viewed.  there should be some way of tweaking this,
#                 # preferably from the command-line.
#                 for i in range(max(1, n_nodes)):
#                     node = find_grouped_node(func_nodes[i])

#                     self.print_call_chain(stream, node)

    def print_dot_file(self, stream):
        self.nodes_printed = {}
        self.edges_printed = {}

        self.print_graph_info(stream)
        
        print >> stream, "digraph callgraph {"

	print >> stream, 'node [ fontname="%s", color="0.0,0.0,0.0" ];' % node_font

        if print_useful_graph:
            self.print_useful_call_chains(stream)
        else:
            # print out the nodes first
            for node in self.nodes:
                self.print_dot_node(stream, node)

            # print out node edges, too
            for node in self.nodes:
                self.print_dot_edge(stream, node)

        print >> stream, "}"

def is_root_node(node):
    if root_node_name is None:
        return node.parent is None
    else:
        return node.func_name == root_node_name

def nodecmp(n1, n2):
    return n1.weight > n2.weight

def discover_leaf_nodes(tree_nodes):
    leaf_nodes = []

    for node in tree_nodes:
        if len(node.children) == 0:
            leaf_nodes.append(node)

    return leaf_nodes


# grouping nodes for call path refinement and prettier dot files

def find_grouped_node(node):
    """Find a grouped ancestor node starting from NODE.  This function finds the
    grouped node highest in the ancestor chain, which may or may not be
    exactly what you want.  If there are no grouped nodes in the call chain,
    return the node which was passed in."""
    candidate = node

    while 1:
        # top of the tree
        if node.parent is None:
            return candidate
        # record a grouping node
        if node.is_grouping():
            candidate = node

        # continue up the tree
        node = node.parent

def add_grouping_nodes(root, predicate, group_node_name, added_nodes):
    matches = []
    non_matches = []

    if len(root.children) == 0:
        # nothing to see here
        return
    else:
        for node in root.children:
            if predicate(node):
                matches.append(node)
            else:
                non_matches.append(node)

        # we have figured out which nodes should be grouped and which not.
        # now construct the grouping node, add matches as its children,
        # and swap out root's children as appropriate
        if len(matches) != 0:
            groupie = GroupingNode(group_node_name, matches[0].load_module)
            groupie.calls = reduce(lambda x, y: x + y.calls, matches, 0)
            groupie.weight = reduce(lambda x, y: x + y.weight, matches, 0)
            groupie.children = matches
            groupie.parent = root

            for node in matches:
                node.parent = groupie
            
            added_nodes.append(groupie)

            new_children = non_matches[:]
            new_children.append(groupie)
            root.children = new_children

        # recur on everything which didn't match
        for node in non_matches:
            add_grouping_nodes(node, predicate, group_node_name, added_nodes)


# printing dot files from csp files
def dot_node_name(node, parent=None):
    func_name = node.func_name

    if distinguish_ips != 0:
        if parent is None:
            return "%s-%lx" % (func_name, node.sp)
        else:
            return "%s-%lx^%s" % (func_name, node.sp, parent.dot_name)
    else:
        return func_name

# doesn't remove everything, but nuking all the templates in function
# arguments tends to cut the names down to something readable...it can
# also introduce spurious connections in the graph, but so far that's
# been something we can put up with
def remove_template_funargs(func_name):
    # locate start of function parameters
    fargs_begin = func_name.find("(")

    if fargs_begin == -1:
        # no function parameters?  OK, kinda weird, but we'll buy it
        return (func_name,"")
    else:
        # locate the end of the function parameters
        fargs_end = func_name.find(")", fargs_begin+1)

        if fargs_end == -1:
            # um, even weirder...
            return (func_name,"")
        else:
            args = func_name[fargs_begin:fargs_end+1]
            args = remove_template_funarg(args)

            return (func_name[:fargs_begin],args)

def find_matching_bracket(func_args, start):
    index = start
    count = 0

    while 1:
        if func_args[index] == "<":
            # "push" this template onto the stack
            count = count + 1
        elif func_args[index] == ">":
            if count == 0:
                # the matching ">" to the first "<" we found
                return index
            else:
                # "pop" this template from the stack
                count = count - 1

        index = index + 1
def remove_template_funarg(func_args, start=0):
    index = start
    length = len(func_args)
    
    while 1:
        if index >= length:
            # end of the road
            return func_args
        else:
            if func_args[index] == "<":
                # found the start of a template.  find the matching ">"
                targs_begin = index
                end = find_matching_bracket(func_args, index+1)

                # convenience variable
                new_func_args = func_args[:targs_begin+1] + func_args[end:]
                #print "new:", new_func_args

                # recursively remove args, nuking as we go
                return remove_template_funarg(new_func_args, targs_begin+1)
            else:
                pass

        index = index + 1

def remove_template_args(func_name):
    (func_name,args) = remove_template_funargs(func_name)
                                        
    targs_begin = func_name.find("<")

    if targs_begin == -1:
        # no template arguments
        return func_name + args
    else:
        targs_end = func_name.rfind(">")

        if targs_end == -1:
            # found the start but not the end?  hmmm...punt
            return func_name + args
        else:
            # nuke everything between the brackets
            return func_name[:targs_begin+1] + func_name[targs_end:] + args


# totally random implementation of Hall's MAP-CALL-PATH algorithm
extended_refinement = "..."

def map_call_tree(tree, path, fn):
    """Apply FN to each maximal node of stack tree TREE in the
    denotation of call path PATH."""

    # nothing left on the path, time to call the function
    if len(path) == 0:
        fn(tree)
    else:
        if tree.func_name != path[0]:
            # no match, recur on subtrees
            for child in tree.children:
                map_call_tree(child, path, fn)
        else:
            # we matched, see how far it can be extended
            if len(path) == 1:
                # can't be extended, so this is the end
                fn(tree)
            else:
                callee = path[1]

                if callee == extended_refinement:
                    # indiscriminately map over the children
                    path_remains = path[2:]
                    
                    for child in tree.children:
                        map_call_tree(child, path_remains, fn)
                else:
                    # attempt to ensure matches with the rest of
                    # the current call path
                    for child in tree.children:
                        if child.func_name == callee:
                            # a match, use it
                            path_remains = path[1:]
                            
                            map_call_tree(child, path_remains, fn)
                        else:
                            # no match, try to start everything over
                            map_call_tree(child, path, fn)

# Hall's downward refinement computations

# these globals should be done away with
required_parent_label = None
function_records = {}
call_records = {}

def compute_refinement(tree, path, is_extended):
    global function_records, required_parent_label

    required_parent_label = None
    function_records.clear()
    call_records.clear()

    if is_extended or len(path) == 0:
        map_call_tree(tree, path, extended_refinement)
    else:
        map_call_tree(tree, path, immediate_refinement)

    build_profile(function_records, call_records, is_extended)

def build_profile(sample_records, call_records, is_extended):
    for (func_name, count) in sample_records.items():
        if count != 0:
            # surely we can do something more sophisticated than this
            print "%-30s -> %4d samples, %3d calls" % (func_name, count, call_records[func_name])

def compute_function_costs(node, parent):
    global function_records, required_parent_label
    
    if parent is None or parent.func_name == required_parent_label:
        label = node.func_name
        is_locked = node.locked

        if not is_locked:
            function_records[label] = function_records.get(label, 0) + node.weight
            call_records[label] = call_records.get(label, 0) + node.calls

        # make sure this node doesn't get counted twice
        #
        # is this really necessary in our scheme, since child nodes never
        # point to their parents?  does this make a difference?
        node.locked = 1

        for child in node.children:
            if parent is not None:
                compute_function_costs(child, node)
            else:
                compute_function_costs(child, None)

        node.locked = is_locked
    else:
        for child in node.children:
            compute_function_costs(child, node)

def extended_refinement(node):
    for child in node.children:
        compute_function_costs(child, None)

def immediate_refinement(node):
    global required_parent_label

    required_parent_label = node.func_name
    
    for child in node.children:
        compute_function_costs(child, node)

def invert_tree(tree_nodes):
    """Returns a list of nodes such that each node has its parent-child
    relationships reversed with respect to its status in 'tree_nodes'."""

    # copy everything 
    inverted_nodes = [ 0 ] * len(tree_nodes)

    i = 0

    for node in tree_nodes:
        inverted_nodes[i] = copy.copy(node)
        # null out children to avoid problems later
        inverted_nodes[i].children = []
        i = i + 1

    # set parent-child relationships correctly
    for invert in inverted_nodes:
        # the original relationship in the tree
        flip = tree_nodes[invert.id]
        flip_parent = flip.parent

        # now invert the relationships
        if flip_parent is not None:
            invert_child = inverted_nodes[flip_parent.id]

            invert.children.append(invert_child)
            invert_child.parent = invert

    # create a new root node
    invert_root = copy.copy(tree_nodes[0])
    invert_root.children = []
    invert_root.func_name = "{function body}"
    invert_root.dot_name = invert_root.func_name

    # find the leaf nodes in the original tree
    leaf_nodes = discover_leaf_nodes(tree_nodes)

    # the nodes corresponding to these leaf nodes in the inverted tree
    # do not have any parents.  the new root node created earlier is
    # going to be the root node of the inverted tree, with these former
    # leaf nodes (inverted) as its children.  create that child list
    root_children = []

    for node in leaf_nodes:
        root_children.append(inverted_nodes[node.id])

    invert_root.children = root_children

    # this royally screws things up, but a lot of code depends on the
    # root being at the beginning rather than the end
    inverted_nodes.insert(0, invert_root)

    i = 1

    # go through and fix all the id's of the inverted nodes
    while i != len(inverted_nodes):
        node = inverted_nodes[i]
        node.id = i
        i = i +1

    return inverted_nodes
        

# merging epoch-ly different trees so that we can print out a single
# call graph for the entire run of the program

def merge_trees(root1, root2):
    for child in root2.children:
        try:
            match = root1.children.index(child)
            match = root1.children[match]
            
            # there's a match, recursively merge trees

            # copy values between these two identical nodes first
            match.calls = match.calls + child.calls
            match.weight = match.weight + child.weight
            
            merge_trees(match, child)
        except ValueError:
            # there's no match, so append because this is a new call path
            root1.children.append(child)
            child.parent = root1

def flatten_tree(tree):
    # stupid namespace rules in Python
    def flatten_tree_internal(flat_func, tree, node_list):
        node_list.append(tree)

        for child in tree.children:
            flat_func(flat_func, child, node_list)

    list = []

    flatten_tree_internal(flatten_tree_internal, tree[0], list)
    
    return list

def bigger_tree(tree1, tree2):
    if len(tree1) > len(tree2):
        return tree1
    else:
        return tree2

def merge_all_trees(tree_array):
    if len(tree_array) == 1:
        return tree_array[0]

    biggest_tree = reduce(bigger_tree, tree_array[1:], tree_array[0])

    tree_array.remove(biggest_tree)
    
    for tree in tree_array:
        merge_trees(biggest_tree[0], tree[0])

    nodes = flatten_tree(biggest_tree)
    
    return nodes


# generating Hall-style profiles from the command line

def call_path_from_string(str):
    # I love dynamic languages
    return eval(str)

def dump_tree(root):
    def dump_tree_internal(dumper, root, depth):
        print "%s%s ($ip %x $sp %x): %d samples, %d calls" % \
              (" " * depth, root.func_name, root.ip, root.sp, root.weight, root.calls)

        for child in root.children:
            dumper(dumper, child, depth +1)

    dump_tree_internal(dump_tree_internal, root, 0)


# the guts of everything

def usage():
    print '''csp2dot.py: a script for post-processing csprof data files

Usage: python ./csp2dot.py [options] <csprof-data-file>

Options:
    -a		Remove template arguments from C++ function names
    -b [path]	Add [path] to the list of places to search for binaries
    -c [func]	Print the callers of [func] and their call counts
    -d		Print a dot-formatted graph to stdout (the default)
    -e		Print edge counts as added nodes along edges
    -f [font]	Specify the [font] to be used in dot output
    -h		Print this help message and exit
    -l [lib]	Provide all functions in [lib] with a common parent
    -n [name]	Treat the node [name] as the root of the tree
    -p [name]	Strip the namespace [name] from C++ function names
    		(may be given multiple times)
    -s		Distinguish different call sites by stack pointers.
		This has the effect of making the graph more tree-like,
		rather than a (potentially) tangled mass of edges.
    -r [func]	Print the callees of [func] and their call counts
    -t		Print the collected call tree
    -u [pct]	Print only "useful" functions called from "main"
    -x		Print statistics'''

def root_node_of_tree(tree):
    return tree[0]

def interpret_options(options):
    global distinguish_ips, print_useful_graph, nuke_template_args, \
           print_edge_counts, node_font, sample_threshold, \
           stripped_namespaces, root_node_name, binary_path
    
    option_table = {}

    option_table["dotgraph"] = 1
    option_table["dumpstat"] = 0
    option_table["dumptree"] = 0
    
    for opt, arg in options:
        if opt == "-a":
            nuke_template_args = 1
        if opt == "-b":
            binary_path.append(arg)
        if opt == "-h":
            usage()
            sys.exit()
        if opt == "-d":
            option_table["dotgraph"] = 1
            option_table["dumpstat"] = 0
            option_table["dumptree"] = 0
        if opt == "-e":
            print_edge_counts = 1
        if opt == "-f":
            node_font = arg
        if opt == "-c":
            option_table["upfunc"] = arg
            option_table["dotgraph"] = 0
        # FIXME FIXME FIXME: there has to be a decent way to allow
        # multiple libraries and (even better) arbitrary grouping
        # conditions in here somewhere.  or perhaps that is best left
        # for the interactive interface?
        if opt == "-l":
            option_table["grouplib"] = arg
            option_table["nodepred"] = lambda node, lib=grouped_library: node.load_module.filename.find(lib) != -1
        if opt == "-n":
            root_node_name = arg
        if opt == "-p":
            namespace = arg
            stripped_namespaces.append(namespace)
        if opt == "-r":
            option_table["downfunc"] = arg
            option_table["dotgraph"] = 0
        if opt == "-s":
            distinguish_ips = 1
        if opt == "-t":
            option_table["dumptree"] = 1
            option_table["dotgraph"] = 0
            option_table["dumpstat"] = 0
        if opt == "-u":
            option_table["dumptree"] = 0
            print_useful_graph = 1
            if arg is None:
                sample_threshold = 0.05
            else:
                sample_threshold = string.atof(arg)
        if opt == "-x":
            option_table["dumpstat"] = 1
            option_table["dotgraph"] = 0
            option_table["dumptree"] = 0

    return option_table

def count(item, seq):
    c = 0

    for e in seq:
        if e == item:
            c = c + 1

    return c

def dump_statistics(cspfile):
    builder = StatisticsBuilder()

    parser = Parser(cspfile, builder)
    (unsafe, totsamp, totret, flatprof, child_countss) = parser.parse()

    size = os.stat(cspfile).st_size
    #print "***", cspfile
    print "unsafe %d | samples %d | returns %d | size %d" % (unsafe, totsamp, totret, size)
    #for counts in child_countss:
    #    interior_nodes = len(counts) - count(0, counts)
    #    avg_n_children = reduce(operator.add, counts, 0)/float(interior_nodes)
    #    print "avg fanout in epoch: %f" % avg_n_children
    items = flatprof.items()[:]
    items.sort(lambda x, y: -cmp(x[1], y[1]))
    for (func_name, samples) in items:
        if samples != 0:
            pct = float(samples)/float(totsamp)*100.0
            print "%5.2f %5d %s" % (pct, samples, func_name)
    #print "### end"

def dump_treeish_things(cspfile, option_table):
    builder = TreeBuilder()

    parser = Parser(cspfile, builder)

    # do our stuff
    (epochs, trees) = parser.parse()

    if len(trees) == 0:
        print "No trees found in", cspfile
        sys.exit(2)

    # one tree to rule them all...
    one_tree = merge_all_trees(trees)

    if option_table.get("grouplib", 0):
        added_nodes = []

        add_grouping_nodes(root_node_of_tree(one_tree),
                           option_table["nodepred"], \
                           option_table["grouplib"], added_nodes)

        print '/* added %d grouping nodes */' % len(added_nodes)

        # FIXME: need to add ids to all the added nodes at some point
        one_tree.extend(added_nodes)

    if option_table["dotgraph"]:
        context = DotGraphContext(one_tree)
        context.print_dot_file(sys.stdout)

    if not (option_table.get("downfunc", "") == ""):
        compute_refinement(root_node_of_tree(one_tree), [option_table["downfunc"]], 0)

    if not (option_table.get("upfunc", "") == ""):
        inverted_tree = invert_tree(one_tree)

        compute_refinement(root_node_of_tree(inverted_tree), \
                           [option_table["upfunc"]], 0)

    if option_table["dumptree"]:
        dump_tree(root_node_of_tree(one_tree))
        
def main():
    global distinguish_ips, print_useful_graph, nuke_template_args, \
           print_edge_counts, node_font, sample_threshold, \
           stripped_namespaces, root_node_name, binary_path, print_null
    
    try:
        opts, args = getopt.getopt(sys.argv[1:], "ab:c:def:hl:n:p:sr:tu:x")
    except getopt.GetoptError:
        usage()
        sys.exit(2)

    option_table = interpret_options(opts)

    if option_table["dumpstat"]:
        dump_statistics(args[0])
    else:
        dump_treeish_things(args[0], option_table)

if __name__ == '__main__':
    main()
    
    #build_profile(aggregate_weight, 0)
    
    #compute_callers_and_callees()
    #compute_refinement(nodes[0], ["Rf_evalList"], 0)
