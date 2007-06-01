import struct
import sys
import getopt
import StringIO

object_file_header = "HHiqiHH"

ALPHA_MAGIC = 0603
ALPHA_MAGIC_Z = 0610
ALPHA_U_MAGIC = 0617

optional_header_format = "4h3q4QIIq"

OMAGIC = 0407
NMAGIC = 0410
ZMAGIC = 0413

optional_header_magic_numbers = [OMAGIC, NMAGIC, ZMAGIC]

section_header_format = "8sQQqQQQHHi"

section_TEXT = ".text"
section_INIT = ".init"
section_FINI = ".fini"
section_RCONST = ".rconst"
section_RDATA = ".rdata"
section_DATA = ".data"
section_LITA = ".lita"
section_LIT8 = ".lit8"
section_LIT4 = ".lit4"
section_SDATA = ".sdata"
section_BSS = ".bss"
section_SBSS = ".sbss"
section_UCODE = ".ucode"
section_GOT = ".got"
section_DYNAMIC = ".dynamic"
section_DYNSYM = ".dynsym"
section_REL_DYN = ".rel.dyn"
section_DYNSTR = ".dynstr"
section_HASH = ".hash"
section_MSYM = ".msym"
section_LIBLIST = ".liblist"
section_CONFLICT = ".conflic"
section_REGINFO = ".reginfo"
section_XDATA = ".xdata"
section_PDATA = ".pdata"
section_TLS_DATA = ".tlsdata"
section_TLS_BSS = ".tlsbss"
section_TLS_INIT = ".tlsinit"
section_COMMENT = ".comment"

section_names = [section_TEXT, section_INIT, section_FINI, section_RCONST,
                 section_RDATA, section_DATA, section_LITA, section_LIT8,
                 section_LIT4, section_SDATA, section_BSS, section_SBSS, section_UCODE,
                 section_GOT, section_DYNAMIC, section_DYNSYM,
                 section_REL_DYN, section_DYNSTR, section_HASH, section_MSYM,
                 section_LIBLIST, section_CONFLICT, section_REGINFO, section_XDATA,
                 section_PDATA, section_TLS_DATA, section_TLS_BSS,
                 section_TLS_INIT, section_COMMENT]

relocation_table_entry_format = "qiI"

symbol_table_header_format = "HH11iq11Q"

procedure_descriptor_format = "QqiiIiiIiiiiIHH"

local_symbol_table_entry_format = "qiI"

file_descriptor_entry_format = "Q3q14iIHI"

library_dependency_format = "IIIII"

comment_header_format = "IIQ"
comment_linker_data = "II"


# various exceptions which can be raised when things go wrong
class COFFError(Exception):
    pass


# various entities which can be contained in a COFF file

# duplication of work here from csp2dot.py, FIXME
class COFFBlob:
    def __init__(self, format_string, stream):
        self.format_string = format_string
        len = struct.calcsize(self.format_string)
        self.data = stream.read(len)

    def unpack(self):
        return struct.unpack(self.format_string, self.data)

class LibraryDependency(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, library_dependency_format, stream)
        (self.name, self.timestamp, self.checksum, self.version, self.flags) = self.unpack()

class FileHeader(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, object_file_header, stream)
        (self.magic, self.n_sections, self.timestamp, self.symptr, \
         self.n_syms, self.opthdr, self.flags) = self.unpack()

        if self.magic == ALPHA_U_MAGIC:
            raise COFFError, "Cannot disassemble a UMAGIC file"

        if self.magic != ALPHA_MAGIC:
            raise COFFError, "File not a COFF binary"
        
    def has_optional_header(self):
        return self.opthdr != 0

class OptionalHeader(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, optional_header_format, stream)
        (self.magic, self.vstamp, self.build_revision, padcell, self.text_size, \
         self.data_size, self.bss_size, self.entry, self.text_start, \
         self.data_start, self.bss_start, self.gprmask, self.fprmask, \
         self.gp_value) = self.unpack()

        if self.magic not in optional_header_magic_numbers:
            raise COFFError, "Optional header magic number invalid"

class SectionHeader(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, section_header_format, stream)
        (self.name, self.paddr, self.vaddr, self.size, self.scnptr, \
         self.relptr, self.lnnoptr, self.n_relocs, self.n_lnno, \
         self.flags) = self.unpack()

        null_index = self.name.find('\0')

        if null_index != -1:
            self.name = self.name[:null_index]

        if self.name not in section_names:
            raise COFFError, "Section name %s invalid" % self.name

    def __str__(self):
        return "Section %s @ %x" % (self.name, self.paddr)

class DenseNumberRecord(COFFBlob):
    def __init__(self, stream):
        # FIXME
        COFFBlob.__init__(self, "II", stream)
        (self.fd_index, self.sym_index) = self.unpack()

class ProcedureDescriptor(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, procedure_descriptor_format, stream)
        (self.addr, self.line_offset, self.sym_index, self.lines, \
         self.regmask, self.regoffset, self.opt_index, \
         self.fregmask, self.regoffset, self.frame_offset, \
         self.line_low, self.line_high, bitfield, \
         self.framereg, self.pcreg) = self.unpack()

        self.gp_prologue = (bitfield) & 0xff
        self.gp_used = (bitfield >> 8) & 0x1
        self.reg_frame = (bitfield >> 9) & 0x1
        self.prof = (bitfield >> 10) & 0x1
        self.tail_call = (bitfield >> 11) & 0x1
        reserved = (bitfield >> 12) & 0xf
        self.no_stack_data = (bitfield >> 16) & 0x1
        reserved = (bitfield >> 17) & 0x3f
        self.localoff = (bitfield >> 24) & 0xff

    def __str__(self):
        return "ProcedureDescriptor for %x" % self.addr
    
    def is_stack_procedure(self):
        return not self.reg_frame

class SymbolTableHeader(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, symbol_table_header_format, stream)

        self.header_pos = stream.tell()
        
        (self.magic, self.version_stamp, self.n_line_entries, \
         self.dense_max, self.n_procedures, self.n_local_symbols, \
         self.opt_max, self.n_auxiliary, self.local_string_max, \
         self.ext_string_max, self.n_fds, self.n_rfds, self.ext_sym_max, \
         self.n_line_bytes, self.line_index, self.dense_index, \
         self.pd_index, self.local_sym_index, self.opt_index, \
         self.aux_sym_index, self.local_str_index, self.ext_str_index, \
         self.fd_index, self.rfd_index, self.ext_sym_index) = self.unpack()

        self.read_strings(stream)
        self.read_local_symbols(stream)
        self.read_external_symbols(stream)
        self.read_auxiliary_symbols(stream)
        self.read_file_descriptor_table(stream)
        self.read_procedure_descriptors(stream)

        #self.print_table_info("lines", self.n_line_entries, self.line_index)
        #self.print_table_info("dense", self.dense_max, self.dense_index)
        #self.print_table_info("procds", self.n_procedures, self.pd_index)
        #self.print_table_info("local syms", self.n_local_symbols, self.local_sym_index)
        #self.print_table_info("opt syms", self.opt_max, self.opt_index)
        #self.print_table_info("aux syms", self.n_auxiliary, self.aux_sym_index)
        #self.print_table_info("local strs", self.local_string_max, self.local_str_index)
        #self.print_table_info("ext strs", self.ext_string_max, self.ext_str_index)
        #self.print_table_info("fds", self.n_fds, self.fd_index)
        #self.print_table_info("rfds", self.n_rfds, self.rfd_index)
        #self.print_table_info("ext syms", self.ext_sym_max, self.ext_sym_index)

#         for s in self.external_symbols:
#             if s.type == 6 and s.storage_class == 1:
#                 print "proc addr: %x" % s.value, self.external_symbol_name(s)

#         for s in self.local_symbols:
#             print s

#         for fd in self.file_descriptors:
#             start = fd.local_sym_base

#             if(fd.rss != -1):
#                 ending_null = self.local_string_table.find(chr(0), fd.rss)
                
#                 print "processing file %d:" % fd.rss, self.local_string_table[fd.rss:ending_null+20]
                
#             for s in self.local_symbols[start:start+fd.n_local_syms]:
#                 if s.type == 14 and s.storage_class == 1 and s.iss != -1:
#                     print "static proc addr: %x" % s.value, self.local_symbol_name(s, fd.local_str_base)

            
    def print_table_info(self, string, count, offset):
        print "%s: %ld entries, %ld offset" % (string, count, offset)

    def read_strings(self, stream):
        stream.seek(self.local_str_index, 0)

        self.local_string_table = stream.read(self.local_string_max)

        stream.seek(self.ext_str_index, 0)

        self.external_string_table = stream.read(self.ext_string_max)
        
    # functions to divide up the functionality of reading everything in
    # this gigantic structure.  most of them don't process their information
    # (yet), but just slurp the requisite number of bytes to ensure the
    # next part starts at the right place
    def read_line_numbers(self, stream):
        pass
    
    def read_dense_numbers(self, stream):
        stream.seek(self.header_pos + self.dense_index, 0)
        
        for i in range(self.dense_max):
            dn_record = DenseNumberRecord(stream)

    def read_procedure_descriptors(self, stream):
        stream.seek(self.pd_index, 0)

        procedure_descriptors = []
        
        for i in range(self.n_procedures):
            pd_entry = ProcedureDescriptor(stream)

            procedure_descriptors.append(pd_entry)

        self.procedure_descriptors = procedure_descriptors

    def read_local_symbols(self, stream):
        stream.seek(self.local_sym_index, 0)

        local_symbols = []
        
        for i in range(self.n_local_symbols):
            local_sym = LocalSymbol(stream)

            local_symbols.append(local_sym)

        self.local_symbols = local_symbols

    def read_external_symbols(self, stream):
        stream.seek(self.ext_sym_index, 0)

        external_symbols = []

        for i in range(self.ext_sym_max):
            ext_sym = ExternalSymbol(stream)

            external_symbols.append(ext_sym)

        self.external_symbols = external_symbols

    def read_auxiliary_symbols(self, stream):
        stream.seek(self.aux_sym_index, 0)

        auxiliary_symbols = []

        for i in range(self.n_auxiliary):
            aux_sym = AuxiliarySymbol(stream)

            auxiliary_symbols.append(aux_sym)

        self.auxiliary_symbols = auxiliary_symbols
        
    def get_string(self, symbol, table, base=0):
        if symbol.iss == -1:
            return "no name"
        
        ending_null = table.find(chr(0), base+symbol.iss)
        name = table[base+symbol.iss:ending_null]

        return "start: %d, end: %d, str: %s" % (symbol.iss, ending_null, name)
    
    def external_symbol_name(self, external_sym):
        return self.get_string(external_sym, self.external_string_table)
    
    def local_symbol_name(self, local_sym, base):
        return self.get_string(local_sym, self.local_string_table, base)

    def find_descriptor_for_addr(self, func_address):
        for pd in self.procedure_descriptors:
            if pd.addr == func_address:
                return pd

        raise KeyError
    
    def read_optimization_symbols(self, stream):
        stream.seek(self.header_pos + self.opt_index, 0)
        
        for i in range(self.opt_max):
            opt_sym = OptimizationSymbol(stream)

    def read_file_descriptor_table(self, stream):
        stream.seek(self.fd_index, 0)

        file_descriptors = []
        
        # is this fd + rfd?  or just fd?
        for i in range(self.n_fds):
            fd = FileDescriptor(stream)

            file_descriptors.append(fd)

        self.file_descriptors = file_descriptors

symbol_types = ["stNil", "stGlobal", "stStatic", "stParam", "stLocal",
                "stLabel", "stProc", "stBlock", "stEnd", "stMember",
                "stTypedef", "stFile", "NULL", "NULL" "stStaticProc", "stConstant",
                "NULL", "stBase", "stVirtBase", "stTag",
                "stInter", "NULL", "stModule", "stUsing", "stAlias",
                "NULL", "NULL", "NULL", "NULL", "NULL", "stExternal",
                "stUseModule", "stRename", "stInterface"]

storage_classes = ["scNil", "scText", "scData", "scBss", "scRegister",
                   "scAbs", "scUndefined", "scUnallocated", "NULL",
                   "scTlsUndefined", "NULL", "scInfo", "scSData",
                   "scSBss", "scRData", "scVar", "scCommon",
                   "scSCommon", "scVarRegister", "scFileDesc", "scSUndefined",
                   "scInit", "scReportDesc", "scXData", "scPData",
                   "scFini", "scRConst", "scTlsCommon", "scTlsData",
                   "scTlsBss", "scMax"]

class LocalSymbol(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, local_symbol_table_entry_format, stream)
        (self.value, self.iss, bitfield) = self.unpack()

        self.type = (bitfield) & 0x3f
        self.storage_class = (bitfield >> 6) & 0x1f
        reserved = (bitfield >> 11) & 0x1
        self.index = (bitfield >> 12) & 0xfffff

    def __str__(self):
        return "LocalSymbol %x (%s/%s/%d)" % (self.value, symbol_types[self.type], \
                                              storage_classes[self.storage_class], \
                                              self.index)

class ExternalSymbol(LocalSymbol):
    def __init__(self, stream):
        LocalSymbol.__init__(self, stream)
        COFFBlob.__init__(self, "Hi", stream)
        (bitfield, self.fd_index) = self.unpack()

        self.is_weak = (bitfield) & 0x1

# meant to be called on local/external symbols
def symbol_is_function(sym):
    return sym.storage_class == 1 and (sym.type == 6 or sym.type == 14)

class OptimizationSymbol(COFFBlob):
    def __init__(self, stream):
        # FIXME
        COFFBlob.__init__(self, "IIQ", stream)
        (self.tag, self.len, self.val) = self.unpack()

relocation_section_numbers = ["R_SN_NULL", "R_SN_TEXT", "R_SN_RDATA",
                              "R_SN_DATA", "R_SN_SDATA", "R_SN_SBSS",
                              "R_SN_BSS", "R_SN_INIT", "R_SN_LIT8",
                              "R_SN_LIT4", "R_SN_XDATA", "R_SN_PDATA"
                              "R_SN_FINI", "R_SN_LITA", "R_SN_ABS",
                              "R_SN_RCONST", "R_SN_TLSDATA", "R_SN_TLSBSS",
                              "R_SN_TLSINIT", "R_SN_RESTEXT", "R_SN_GOT"]

relocation_types = ["R_ABS", "R_REFLONG", "R_REFQUAD",
                    "R_GPREL32", "R_LITERAL", "R_LITUSE",
                    "R_GPDISP", "R_BRADDR", "R_HINT",
                    "R_SREL16", "R_SREL32", "R_SREL64",
                    "R_OP_PUSH", "R_OP_STORE", "R_OP_PSUB",
                    "R_OP_PRSHIFT", "R_GPVALUE", "R_GPRELHIGH",
                    "R_GPRELLOW"]

class RelocationTableEntry(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, relocation_table_entry_format, stream)
        (self.vaddr, self.symbol_index, bitfield) = self.unpack()
        self.type = (bitfield) & 0xff
        self.extern = (bitfield >> 8) & 0x1
        self.offset = (bitfield >> 9) & 0x3f
        self.reserved = (bitfield >> 15) & 0x7ff
        self.size = (bitfield >> 26) & 0x3f

    def __str__(self):
        return "%s - %d" % (relocation_types[self.type], self.size)

class FileDescriptor(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, file_descriptor_entry_format, stream)
        (self.addr, self.line_offset, self.line, self.local_str_bytes, \
         self.rss, self.local_str_base, self.local_sym_base, self.n_local_syms, \
         self.line_base, self.n_line_entries, self.opt_sym_base, \
         self.n_opt_syms, self.pdtbl_base, self.n_pds, self.aux_sym_base, \
         self.n_aux_syms, self.rfd_base, self.n_rfds, bitfield, \
         self.vstamp, reserved2) = self.unpack()

        self.language = (bitfield) & 0x1f
        self.merge = (bitfield >> 5) & 0x1
        self.readin = (bitfield >> 6) & 0x1
        self.endian = (bitfield >> 7) & 0x1
        self.glevel = (bitfield >> 8) & 0x3
        self.trim = (bitfield >> 10) & 0x1
        reserved = (bitfield >> 11) & 0xf
        self.full_externals = (bitfield >> 15) & 0x1

class AuxiliarySymbol(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, "I", stream)
        (self.isym,) = self.unpack()

class CommentHeader(COFFBlob):
    def __init__(self, stream):
        COFFBlob.__init__(self, comment_header_format, stream)
        (self.tag, self.len, self.val) = self.unpack()

def parse_comment_headers(stream):
    headers = []
    
    while 1:
        h = CommentHeader(stream)
        
        if h.tag == 0:
            break
        else:
            headers.append(h)

    return headers
            
class Section:
    def __init__(self, stream):
        self.header = SectionHeader(stream)
        self.stream = stream
        
        # save file position
        fpos = stream.tell()

        # read relocation entries
        stream.seek(self.header.relptr, 0)

        self.relocation_entries = []

        for i in range(self.header.n_relocs):
            entry = RelocationTableEntry(stream)

            self.relocation_entries.append(entry)

        # read section data
        stream.seek(self.header.scnptr, 0)

        self.data = stream.read(self.header.size)
            
        # restore file position
        stream.seek(fpos, 0)

    def modify_return_sequence(self):
        newdata = StringIO.StringIO()
        # last position written from
        lastpos = 0
        finding = 1
        extra = 0
        
        while finding:
            # find the beginning of the return instruction
            index = self.data.find('\x01\x80', lastpos)

            # writing data is really suboptimal, because of how Python
            # insists on having mutable strings and stupid interfaces to
            # manipulate them.
            
            if index == -1:
                # blow up or something
                newdata.write(self.data[lastpos:])
                finding = 0
            else:
                newdata.write(self.data[lastpos:index])
                if self.data[index+3] != '\x6b':
                    # not a *real* return instruction
                    newdata.write(self.data[index:index+4])
                    lastpos = index + 4
                    continue
                else:
                    rareg = ord(self.data[index+2]) & 0x1f

                    inst = construct_bic_instruction(rareg)
                    
                    # convert it to data
                    instdata = chr((inst >>  0) & 0xff)
                    instdata = instdata + chr((inst >>  8) & 0xff)
                    instdata = instdata + chr((inst >> 16) & 0xff)
                    instdata = instdata + chr((inst >> 24) & 0xff)

                    # dump it into the file
                    newdata.write(instdata)

                    # dump the old return instruction to the file
                    newdata.write(self.data[index:index+4])

                    # update the position (jump over the ret and the open slot)
                    lastpos = index + 8
                    extra = extra + 1

        newdata = newdata.getvalue()
        print 'comparing new data | %d' % (len(self.data) - lastpos)
        if(len(newdata) != len(self.data)):
            raise ValueError, "new %x | old %x | extra %d" % (len(newdata),len(self.data), extra)

        print 'writing new data'
        self.stream.seek(self.header.scnptr, 0)
        self.stream.write(newdata)
        self.stream.flush()
        print 'flushed write'

def construct_bic_instruction(rareg):
    inst = 0x11 << 26
    inst = inst | (rareg << 21)
    inst = inst | (0x3 << 13)
    inst = inst | (0x1 << 12)
    inst = inst | (0x8 << 5)
    inst = inst | rareg

    return inst

examined_sections = [section_TEXT, section_INIT, section_FINI]

def sym_compare(s1, s2):
    return s1.value < s2.value

def process_text_section(header, symtbl):
    end_address = header.vaddr + header.size

    local_funcs = filter(symbol_is_function, symtbl.local_symbols)
    ext_funcs = filter(symbol_is_function, symtbl.external_symbols)

    funcs = local_funcs + ext_funcs

    funcs.sort(sym_compare)

    n_funcs = len(funcs)

    # we collect (start_addr, end_addr) tuples into this list
    invalid_pairs = []
    
    for i in range(n_funcs):
        func = funcs[i]

        func_start_addr = func.value
        func_start_index = func_start_addr - header.vaddr

        if i != n_funcs - 1:
            next_func = funcs[i+1]

            func_end_addr = next_func.value - 4
            func_end_index = next_func.value - header.vaddr
        else:
            func_end_addr = end_address - 4
            func_end_index = end_address - header.vaddr

        pd = symtbl_header.find_descriptor_for_addr(func_start_addr)

        if pd.is_stack_procedure():
            ri_index = find_return_address(section.data)

            if ri_index == -1:
                invalid_pairs.append((func_start_addr, func_end_addr))

    return invalid_pairs

def find_return_address(section_data, start, end):
    return_instruction = '\x01\x80\xfa\x6b'
    
    for i in range(start, end, 4):
        index = section_data.find(return_instruction, start, start+4)

        if index != -1:
            return index

    return -1
        
def process_address_pairs(address_pairs):
    start_addresses = map(lambda x: x[0], address_pairs)
    end_addresses = map(lambda x: x[1], address_pairs)

    n_funcs = len(start_addresses)

    struct_format_string = "Q%dQ%dQ" % (n_funcs, n_funcs)
    pack_arg_list = ([struct_format_string, n_funcs] + start_addresses) + end_addresses

    x = struct.pack(*pack_arg_list)

    return x

class LoadModule:
    def __init__(self, filename):
        fd = open(filename, 'r+b')
        self.file_header = FileHeader(fd)

        if self.file_header.has_optional_header():
            self.opt_header = OptionalHeader(fd)

        self.read_sections(fd)

        self.read_symbol_table_header(fd)

    def read_sections(self, fd):
        sections = []

        for i in range(self.file_header.n_sections):
            section = Section(fd)

            sections.append(section)

        self.sections = sections

    def read_symbol_table_header(self, fd):
        fd.seek(self.file_header.symptr, 0)

        self.symtbl_header = SymbolTableHeader(fd)

def grovel_bad_functions(binary):
    pairs = []

    for section in binary.sections:
        header = section.header

        if header.name == ".init" or header.name == ".fini":
            pairs.append((header.vaddr, header.vaddr + header.size - 4))

        if header.name == ".text":
            pairs.extend(process_text_section(header, symtbl_header))

    pairs.sort(lambda x, y: x[0] < y[0])
    
    x = process_address_pairs(pairs)

    sys.stdout.write(x)

def dump_section_contents(binary, name):
    for section in binary.sections:
        if section.header.name == name:
            for entry in section.relocation_entries:
                print entry

def modify_text_section_returns(binary):
    for section in binary.sections:
        if section.header.name == ".text":
            section.modify_return_sequence()
            return

def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'bd:x')
    except getopt.GetoptError:
        sys.exit(2)

    binary = LoadModule(args[0])

    for opt, arg in opts:
        if opt == "-b":
            grovel_bad_functions(binary)
            sys.exit(0)
        if opt == '-d':
            section_name = arg
            dump_section_contents(binary, arg)
        if opt == '-x':
            modify_text_section_returns(binary)

if __name__ == '__main__':
    main()


                
