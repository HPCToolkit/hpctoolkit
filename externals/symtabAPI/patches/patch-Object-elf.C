--- src/core/symtabAPI/src/Object-elf.C.orig	2008-04-11 15:46:40.000000000 -0500
+++ src/core/symtabAPI/src/Object-elf.C	2008-12-11 16:07:43.000000000 -0600
@@ -75,6 +75,7 @@
 bool Object::truncateLineFilenames = true;
 #if defined(os_linux) && (defined(arch_x86) || defined(arch_x86_64))
 static bool find_catch_blocks(Elf_X &elf, Elf_X_Shdr *eh_frame, Elf_X_Shdr *except_scn,
+                              Address txtaddr, Address dataaddr,
                               std::vector<ExceptionBlock> &catch_addrs);
 #endif
     
@@ -181,6 +182,8 @@
 const char* DYNAMIC_NAME     = ".dynamic";
 const char* EH_FRAME_NAME    = ".eh_frame";
 const char* EXCEPT_NAME      = ".gcc_except_table";
+const char* EXCEPT_NAME_ALT      = ".except_table";
+
 
 // loaded_elf(): populate elf section pointers
 // for EEL rewritten code, also populate "code_*_" members
@@ -410,7 +413,8 @@
 	else if (strcmp(name, EH_FRAME_NAME) == 0) {
 	    eh_frame = scnp;
 	}
-	else if (strcmp(name, EXCEPT_NAME) == 0) {
+	else if ((strcmp(name, EXCEPT_NAME) == 0) || 
+            (strcmp(name, EXCEPT_NAME_ALT) == 0)) {
 	    gcc_except = scnp;
 	}
    else if (strcmp(name, INTERP_NAME) == 0) {
@@ -744,7 +748,7 @@
       //fprintf(stderr, "[%s:%u] - Exe Name\n", __FILE__, __LINE__);
 #if defined(os_linux) && (defined(arch_x86) || defined(arch_x86_64))
       if (eh_frame_scnp != 0 && gcc_except != 0) {
-         find_catch_blocks(elfHdr, eh_frame_scnp, gcc_except, catch_addrs_);
+         find_catch_blocks(elfHdr, eh_frame_scnp, gcc_except, txtaddr, dataddr, catch_addrs_);
       }
 #endif
       if(interp_scnp)
@@ -899,7 +903,7 @@
 #if defined(os_linux) && (defined(arch_x86) || defined(arch_x86_64))
       //fprintf(stderr, "[%s:%u] - Mod Name is %s\n", __FILE__, __LINE__, name.c_str());
       if (eh_frame_scnp != 0 && gcc_except != 0) {
-         find_catch_blocks(elfHdr, eh_frame_scnp, gcc_except, catch_addrs_);
+         find_catch_blocks(elfHdr, eh_frame_scnp, gcc_except, txtaddr, dataddr, catch_addrs_);
       }
 #endif
 #if defined(TIMED_PARSE)
@@ -2754,7 +2758,7 @@
 
 #if defined(os_linux) && (defined(arch_x86) || defined(arch_x86_64))
 
-static unsigned long read_uleb128(const char *data, unsigned *bytes_read)
+static unsigned long read_uleb128(const unsigned char *data, unsigned *bytes_read)
 {
    unsigned long result = 0;
    unsigned shift = 0;
@@ -2769,7 +2773,7 @@
    return result;
 }
 
-static signed long read_sleb128(const char *data, unsigned *bytes_read)
+static signed long read_sleb128(const unsigned char *data, unsigned *bytes_read)
 {
    unsigned long result = 0;
    unsigned shift = 0;
@@ -2788,6 +2792,7 @@
    return result;
 }
 
+#define DW_EH_PE_absptr  0x00
 #define DW_EH_PE_uleb128 0x01
 #define DW_EH_PE_udata2  0x02
 #define DW_EH_PE_udata4  0x03
@@ -2796,42 +2801,108 @@
 #define DW_EH_PE_sdata2  0x0A
 #define DW_EH_PE_sdata4  0x0B
 #define DW_EH_PE_sdata8  0x0C
-#define DW_EH_PE_absptr  0x00
 #define DW_EH_PE_pcrel   0x10
+#define DW_EH_PE_textrel 0x20
 #define DW_EH_PE_datarel 0x30
+#define DW_EH_PE_funcrel 0x40
+#define DW_EH_PE_aligned 0x50
 #define DW_EH_PE_omit    0xff
 
-static int get_ptr_of_type(int type, unsigned long *value, const char *addr)
+typedef struct {
+   int word_size;
+   unsigned long pc;
+   unsigned long text;
+   unsigned long data;
+   unsigned long func;
+} mach_relative_info;
+
+static int read_val_of_type(int type, unsigned long *value, const unsigned char *addr,
+                           const mach_relative_info &mi)
 {
    unsigned size;
    if (type == DW_EH_PE_omit)
       return 0;
 
-   switch (type & 0xf)
+   unsigned long base = 0x0;
+   /**
+    * LSB Standard says that the upper four bits (0xf0) encode the base.
+    * Except that none of these values should their upper bits set,
+    * and I'm finding the upper bit seems to sometimes contain garbage.
+    * gcc uses the 0x70 bits in its exception parsing, so that's what we'll do.
+    **/
+   switch (type & 0x70)
+   {
+      case DW_EH_PE_pcrel:
+         base = mi.pc;
+         break;
+      case DW_EH_PE_textrel:
+         base = mi.text;
+         break;
+      case DW_EH_PE_datarel:
+         base = mi.data;
+         break;
+      case DW_EH_PE_funcrel:
+         base = mi.func;
+         break;
+   }
+
+   switch (type & 0x0f)
    {
-      case DW_EH_PE_uleb128:
       case DW_EH_PE_absptr:
-         *value = read_uleb128(addr, &size);
-         return size;
+         if (mi.word_size == 4) {
+            *value = (unsigned long) *((const uint32_t *) addr);
+            size = 4;
+         }
+         else if (mi.word_size == 8) {
+            *value = (unsigned long) *((const uint64_t *) addr);
+            size = 8;
+         }
+         break;
+      case DW_EH_PE_uleb128:
+         *value = base + read_uleb128(addr, &size);
+         break;
       case DW_EH_PE_sleb128:
-         *value = read_sleb128(addr, &size);
-         return size;
+         *value = base + read_sleb128(addr, &size);
+         break;
       case DW_EH_PE_udata2:
+         *value = base + *((const uint16_t *) addr);
+         size = 2;
+         break;
       case DW_EH_PE_sdata2:         
-         *value = (unsigned long) *((const int16_t *) addr);
-         return 2;
+         *value = base + *((const int16_t *) addr);
+         size = 2;
+         break;
       case DW_EH_PE_udata4:
+         *value = base + *((const uint32_t *) addr);
+         size = 4;
+         break;
       case DW_EH_PE_sdata4:
-         *value = (unsigned long) *((const int32_t *) addr);
-         return 4;
+         *value = base + *((const int32_t *) addr);
+         size = 4;
+         break;
       case DW_EH_PE_udata8:
+         *value = base + *((const uint64_t *) addr);
+         size = 8;
+         break;
       case DW_EH_PE_sdata8:
-         *value = (unsigned long) *((const int64_t *) addr);
-         return 8;
+         *value = base + *((const int64_t *) addr);
+         size = 8;
+         break;
       default:
-         fprintf(stderr, "Error Unexpected type %x\n", (unsigned) type);
-         return 1;
+         return -1;
    }
+
+   if ((type & 0x70) == DW_EH_PE_aligned)
+   {
+      if (mi.word_size == 4) {
+         *value &= ~(0x3l);
+      }
+      else if (mi.word_size == 8) {
+         *value &= ~(0x7l);
+      }
+   }
+
+   return size;
 }
 
 /**
@@ -2842,41 +2913,48 @@
  *      tells us whether the FDE has any 'Augmentations', and
  *      the types of those augmentations.  The FDE also
  *      contains a pointer to a function.
- *   3. If the FDE has a 'Language Specific Data Area' 
- *      augmentation then we have a pointer to one or more
- *      entires in the gcc_except_table.
+ *   3. If one of the FDE augmentations is a 'Language Specific Data Area' 
+ *      then we have a pointer to one or more entires in the gcc_except_table.
  *   4. The gcc_except_table contains entries that point 
  *      to try and catch blocks, all encoded as offsets
  *      from the function start (it doesn't tell you which
  *      function, however). 
- *   5. We can add the function offsets from the except_table
+ *   5. We can add the function offsets from the gcc_except_table
  *      to the function pointer from the FDE to get all of
  *      the try/catch blocks.  
  **/ 
-
-static int read_except_table_gcc3(Elf_X_Shdr *except_table, 
-      Offset eh_frame_base, Offset except_base,
-      Dwarf_Fde *fde_data, Dwarf_Signed fde_count,
+#define SHORT_FDE_HLEN 4 
+#define LONG_FDE_HLEN 12
+static 
+int read_except_table_gcc3(Dwarf_Fde *fde_data, Dwarf_Signed fde_count,
+                           mach_relative_info &mi,
+                           Elf_X_Shdr *eh_frame, Elf_X_Shdr *except_scn,
       std::vector<ExceptionBlock> &addresses)
 {
    Dwarf_Error err = (Dwarf_Error) NULL;
-   Dwarf_Addr low_pc, except_ptr;
-   Dwarf_Unsigned fde_byte_length, bytes_in_cie, outlen;
-   Dwarf_Ptr fde_bytes, lsda, outinstrs;
-   Dwarf_Off fde_offset;
+   Dwarf_Addr low_pc;
+   Dwarf_Unsigned bytes_in_cie;
+   Dwarf_Off fde_offset, cie_offset;
    Dwarf_Fde fde;
    Dwarf_Cie cie;
-   int has_lsda = 0, is_pic = 0, has_augmentation_length = 0;
-   int augmentor_len, lsda_size, status;
-   unsigned bytes_read;
+   int status, result, ptr_size;
    char *augmentor;
    unsigned char lpstart_format, ttype_format, table_format;
-   unsigned long value, table_end, region_start, region_size, 
-                 catch_block, action;
-   int i, j;
+   unsigned long value, table_end, region_start, region_size;
+   unsigned long catch_block, action, augmentor_len;
+   Dwarf_Small *fde_augdata, *cie_augdata;
+   Dwarf_Unsigned fde_augdata_len, cie_augdata_len;
 
    //For each FDE
-   for (i = 0; i < fde_count; i++) {
+   for (int i = 0; i < fde_count; i++) {
+      unsigned int j;
+      unsigned char lsda_encoding = 0xff, personality_encoding = 0xff;
+      unsigned char *lsda_ptr = NULL, *personality_routine = NULL;
+      unsigned char *cur_augdata;
+      unsigned long except_off;
+      unsigned long fde_addr, cie_addr;
+      unsigned char *fde_bytes, *cie_bytes;
+
       //Get the FDE
       status = dwarf_get_fde_n(fde_data, (Dwarf_Unsigned) i, &fde, &err);
       if (status != DW_DLV_OK) {
@@ -2884,13 +2962,23 @@
          return false;
       }
 
-      //Get address of the function associated with this CIE
-      status = dwarf_get_fde_range(fde, &low_pc, NULL, &fde_bytes, 
-            &fde_byte_length, NULL, NULL, &fde_offset, &err);
+      //After this set of computations we should have:
+      // low_pc = mi.func = the address of the function that contains this FDE
+      // fde_bytes = the start of the FDE in our memory space
+      // cie_bytes = the start of the CIE in our memory space
+      status = dwarf_get_fde_range(fde, &low_pc, NULL, (void **) &fde_bytes,
+                                   NULL, &cie_offset, NULL, 
+                                   &fde_offset, &err);
       if (status != DW_DLV_OK) {
          pd_dwarf_handler(err, NULL);
          return false;
       }
+      //The LSB strays from the DWARF here, when parsing the except_eh section
+      // the cie_offset is relative to the FDE rather than the start of the
+      // except_eh section.
+      cie_offset = fde_offset - cie_offset +
+         (*(uint32_t*)fde_bytes == 0xffffffff ? LONG_FDE_HLEN : SHORT_FDE_HLEN);
+      cie_bytes = (unsigned char *)eh_frame->get_data().d_buf() + cie_offset;
 
       //Get the CIE for the FDE
       status = dwarf_get_cie_of_fde(fde, &cie, &err);
@@ -2906,93 +2994,174 @@
          pd_dwarf_handler(err, NULL);
          return false;
       }
-      //fprintf(stderr, "[%s:%u] - Augmentor string %s\n", __FILE__, __LINE__, augmentor);
+      
+      //Check that the string pointed to by augmentor has a 'L',
+      // meaning we have a LSDA
       augmentor_len = (augmentor == NULL) ? 0 : strlen(augmentor);
       for (j = 0; j < augmentor_len; j++) {
-         if (augmentor[j] == 'z')
-            has_augmentation_length = 1;
-         if (augmentor[j] == 'L')
-            has_lsda = 1;
-         if (augmentor[j] == 'R')
-            is_pic = 1;
+         if (augmentor[j] == 'L') {
+            break;
       }
-
+      }
+      if (j == augmentor_len)
       //If we don't have a language specific data area, then
       // we don't care about this FDE.
-      if (!has_lsda)
          continue;
 
-      //Get the Language Specific Data Area pointer
-      status = dwarf_get_fde_instr_bytes(fde, &outinstrs, &outlen, &err);
+      //Some ptr encodings may be of type DW_EH_PE_pcrel, which means
+      // that they're relative to their own location in the binary.  
+      // We'll figure out where the FDE and CIE original load addresses
+      // were and use those in pcrel computations.
+      fde_addr = eh_frame->sh_addr() + fde_offset;
+      cie_addr = eh_frame->sh_addr() + cie_offset;
+      
+      //Extract encoding information from the CIE.  
+      // The CIE may have augmentation data, specified in the 
+      // Linux Standard Base. The augmentation string tells us how
+      // which augmentation data is present.  We only care about one 
+      // field, a byte telling how the LSDA pointer is encoded.
+      status = dwarf_get_cie_augmentation_data(cie, 
+                                               &cie_augdata,
+                                               &cie_augdata_len,
+                                               &err);
       if (status != DW_DLV_OK) {
          pd_dwarf_handler(err, NULL);
          return false;
       }
-      lsda = ((char *) fde_bytes) + sizeof(int) * 4;
-      if (lsda == outinstrs) {
-         continue;
-      }
-      lsda_size = (unsigned) read_uleb128((char *) lsda, &bytes_read);
-      lsda = ((char *) lsda) + bytes_read;
 
-      //Read the exception table pointer from the LSDA, adjust for PIC
-      except_ptr = (Dwarf_Addr) *((long *) lsda);
-      if (!except_ptr) {
-         // Sometimes the FDE doesn't have an associated 
-         // exception table.
+      cur_augdata = (unsigned char *) cie_augdata;
+      lsda_encoding = DW_EH_PE_omit;
+      for (j=0; j<augmentor_len; j++)
+      {
+         if (augmentor[j] == 'L')
+         {
+            lsda_encoding = *cur_augdata;
+            cur_augdata++;
+         }
+         else if (augmentor[j] == 'P')
+         {
+            //We don't actually need the personality info, but we extract it 
+            // anyways to make sure we properly extract the LSDA.
+            personality_encoding = *cur_augdata;
+            cur_augdata++;
+            unsigned long personality_val;
+            mi.pc = cie_addr + (unsigned long) (cur_augdata - cie_bytes);
+            cur_augdata += read_val_of_type(personality_encoding, 
+                                            &personality_val, cur_augdata, mi);
+            personality_routine = (unsigned char *) personality_val;
+         }
+         else if (augmentor[j] == 'z' || augmentor[j] == 'R')
+         {
+            //Do nothing, these don't affect the CIE encoding.
+         }
+         else
+         {
+            //Fruit, Someone needs to check the Linux Standard Base, 
+            // section 11.6 (as of v3.1), to see what new encodings 
+            // exist and how we should decode them in the CIE.
+            break;
+         }
+      }
+      if (lsda_encoding == DW_EH_PE_omit)
          continue;
+
+
+      //Read the LSDA pointer out of the FDE.
+      // The FDE has an augmentation area, similar to the above one in the CIE.
+      // Where-as the CIE augmentation tends to contain things like bytes describing 
+      // pointer encodings, the FDE contains the actual pointers.
+      status = dwarf_get_fde_augmentation_data(fde, 
+                                               &fde_augdata,
+                                               &fde_augdata_len,
+                                               &err);
+      if (status != DW_DLV_OK) {
+         pd_dwarf_handler(err, NULL);
+         return false;
+      }
+      cur_augdata = (unsigned char *) fde_augdata;
+      for (j=0; j<augmentor_len; j++)
+      {
+         if (augmentor[j] == 'L')
+         {
+            unsigned long lsda_val;
+            mi.pc = fde_addr + (unsigned long) (cur_augdata - fde_bytes);          
+            ptr_size = read_val_of_type(lsda_encoding, &lsda_val, cur_augdata, mi);
+            if (ptr_size == -1)
+               break;
+            lsda_ptr = (unsigned char *) lsda_val;
+            cur_augdata += ptr_size;
+         }
+         else if (augmentor[j] == 'P' ||
+                  augmentor[j] == 'z' ||
+                  augmentor[j] == 'R')
+         {
+            //These don't affect the FDE augmentation data, do nothing
+         }
+         else
+         {
+            //See the comment for the 'else' case in the above CIE parsing
+            break;
       }
-      if (is_pic) {
-         low_pc = eh_frame_base + fde_offset + sizeof(int)*2 + (signed) low_pc;
-         except_ptr += eh_frame_base + fde_offset + 
-            sizeof(int)*4 + bytes_read;
       }
-      except_ptr -= except_base;
+      if (!lsda_ptr)
+         //Many FDE's have an LSDA area, but then have a NULL LSDA ptr.  
+         // Just means there's no exception info here.
+         continue;
 
       // Get the exception data from the section.
-      Elf_X_Data data = except_table->get_data();
+      Elf_X_Data data = except_scn->get_data();
       if (!data.isValid()) {
-         return true;
+         return false;
       }
-      const char *datap = data.get_string();
-      int except_size = data.d_size();
+      const unsigned char *datap = (unsigned char *) data.get_string();
+      unsigned long int except_size = data.d_size();
 
-      j = except_ptr;
-      if (j < 0 || j >= except_size) {
-         //fprintf(stderr, "[%s:%u] - Bad j %d.  except_size = %d\n", __FILE__, __LINE__, j, except_size);
+     except_off = (unsigned long) (lsda_ptr - except_scn->sh_addr());
+      if (except_off >= except_size) {
          continue;
       }
 
       // Read some variable length header info that we don't really
       // care about.
-      lpstart_format = datap[j++];
+      lpstart_format = datap[except_off++];
       if (lpstart_format != DW_EH_PE_omit)
-         j += get_ptr_of_type(DW_EH_PE_uleb128, &value, datap + j);
-      ttype_format = datap[j++];
+         except_off += read_val_of_type(DW_EH_PE_uleb128, &value, datap + except_off, mi);
+      ttype_format = datap[except_off++];
       if (ttype_format != DW_EH_PE_omit)
-         j += get_ptr_of_type(DW_EH_PE_uleb128, &value, datap + j);
+         except_off += read_val_of_type(DW_EH_PE_uleb128, &value, datap + except_off, mi);
 
       // This 'type' byte describes the data format of the entries in the 
       // table and the format of the table_size field.
-      table_format = datap[j++];
-      j += get_ptr_of_type(table_format, &table_end, datap + j);
-      table_end += j;
+      table_format = datap[except_off++];
+      mi.pc = except_scn->sh_addr() + except_off;
+      result = read_val_of_type(table_format, &table_end, datap + except_off, mi);
+      if (result == -1) {
+         continue;
+      }
+      except_off += result;
+      table_end += except_off;
 
-      while (j < (signed) table_end && j < (signed) except_size) {
+      while (except_off < table_end && except_off < except_size) {
          //The entries in the gcc_except_table are the following format:
          //   <type>   region start
          //   <type>   region length
          //   <type>   landing pad
          //  uleb128   action
          //The 'region' is the try block, the 'landing pad' is the catch.
-         j += get_ptr_of_type(table_format, &region_start, datap + j);
-         j += get_ptr_of_type(table_format, &region_size, datap + j);
-         j += get_ptr_of_type(table_format, &catch_block, datap + j);
-         j += get_ptr_of_type(DW_EH_PE_uleb128, &action, datap + j);
+         mi.pc = except_scn->sh_addr() + except_off;
+         except_off += read_val_of_type(table_format, &region_start, 
+                                        datap + except_off, mi);
+         mi.pc = except_scn->sh_addr() + except_off;
+         except_off += read_val_of_type(table_format, &region_size, 
+                                        datap + except_off, mi);
+         mi.pc = except_scn->sh_addr() + except_off;
+         except_off += read_val_of_type(table_format, &catch_block, 
+                                        datap + except_off, mi);
+         except_off += read_val_of_type(DW_EH_PE_uleb128, &action, datap + except_off, mi);
 
          if (catch_block == 0)
             continue;
-         ExceptionBlock eb(region_start + low_pc, region_size, 
+         ExceptionBlock eb(region_start + low_pc, (unsigned) region_size, 
                catch_block + low_pc);
          addresses.push_back(eb);
       }
@@ -3012,24 +3181,32 @@
  * out of it.
  **/
 static bool read_except_table_gcc2(Elf_X_Shdr *except_table, 
-      std::vector<ExceptionBlock> &addresses)
+                                   std::vector<ExceptionBlock> &addresses,
+                                   const mach_relative_info &mi)
 {
    Offset try_start;
    Offset try_end;
    Offset catch_start;
 
    Elf_X_Data data = except_table->get_data();
-   const char *datap = data.get_string();
-   unsigned except_size = data.d_size();
+   const unsigned char *datap = (const unsigned char *)data.get_string();
+   unsigned long except_size = data.d_size();
 
    unsigned i = 0;
    while (i < except_size) {
-      i += get_ptr_of_type(DW_EH_PE_udata4, &try_start, datap + i);
-      i += get_ptr_of_type(DW_EH_PE_udata4, &try_end, datap + i);
-      i += get_ptr_of_type(DW_EH_PE_udata4, &catch_start, datap + i);
+      if (mi.word_size == 4) {
+         i += read_val_of_type(DW_EH_PE_udata4, &try_start, datap + i, mi);
+         i += read_val_of_type(DW_EH_PE_udata4, &try_end, datap + i, mi);
+         i += read_val_of_type(DW_EH_PE_udata4, &catch_start, datap + i, mi);
+      }
+      else if (mi.word_size == 8) {
+         i += read_val_of_type(DW_EH_PE_udata8, &try_start, datap + i, mi);
+         i += read_val_of_type(DW_EH_PE_udata8, &try_end, datap + i, mi);
+         i += read_val_of_type(DW_EH_PE_udata8, &catch_start, datap + i, mi);
+      }
 
       if (try_start != (Offset) -1 && try_end != (Offset) -1) {
-         ExceptionBlock eb(try_start, try_end - try_start, catch_start);
+         ExceptionBlock eb(try_start, (unsigned) (try_end - try_start), catch_start);
          addresses.push_back(eb);
       }
    }
@@ -3041,10 +3218,7 @@
    bool operator()(const ExceptionBlock &e1, const ExceptionBlock &e2) {
       if (e1.tryStart() < e2.tryStart())
          return true;
-      else if (e1.tryStart() > e2.tryStart())
-         return false;
-      else
-         return true;
+      return false;
    }
 };
 
@@ -3054,7 +3228,9 @@
  *  'eh_frame' should point to the .eh_frame section
  *  the addresses will be pushed into 'addresses'
  **/
-static bool find_catch_blocks(Elf_X &elf, Elf_X_Shdr *eh_frame, Elf_X_Shdr *except_scn,
+static bool find_catch_blocks(Elf_X &elf, Elf_X_Shdr *eh_frame, 
+                              Elf_X_Shdr *except_scn, 
+                              Address txtaddr, Address dataaddr,
       std::vector<ExceptionBlock> &catch_addrs)
 {
    Dwarf_Cie *cie_data;
@@ -3073,9 +3249,6 @@
       //likely to happen if we're not using gcc
       return true;
    }
-#if defined(arch_x86_64)
-   return true;
-#endif
 
    eh_frame_base = eh_frame->sh_addr();
    except_base = except_scn->sh_addr();
@@ -3084,10 +3257,6 @@
    status = dwarf_elf_init(elf.e_elfp(), DW_DLC_READ, &pd_dwarf_handler, NULL,
          &dbg, &err);
    if ( status != DW_DLV_OK ) {
-      // GCC 3.3.3 seems to generate incorrect DWARF entries
-      // on the x86_64 platform right now.  Hopefully this will
-      // be fixed, and this #if macro can be removed.
-
       pd_dwarf_handler(err, NULL);
       goto err_noclose;
    }
@@ -3102,6 +3271,13 @@
    }
    //fprintf(stderr, "[%s:%u] - Found %d fdes\n", __FILE__, __LINE__, fde_count);
 
+   mach_relative_info mi;
+   mi.text = txtaddr;
+   mi.data = dataaddr;
+   mi.pc = 0x0;
+   mi.func = 0x0;
+   mi.word_size = elf.wordSize();
+
 
    //GCC 2.x has "eh" as its augmentor string in the CIEs
    for (i = 0; i < cie_count; i++) {
@@ -3118,11 +3294,12 @@
 
    //Parse the gcc_except_table
    if (gcc_ver == 2) {
-      result = read_except_table_gcc2(except_scn, catch_addrs);
+      result = read_except_table_gcc2(except_scn, catch_addrs, mi);
 
    } else if (gcc_ver == 3) {
-      result = read_except_table_gcc3(except_scn, eh_frame_base, except_base,
-            fde_data, fde_count, catch_addrs);
+      result = read_except_table_gcc3(fde_data, fde_count, mi,
+                                      eh_frame, except_scn, 
+                                      catch_addrs);
    }
    sort(catch_addrs.begin(),catch_addrs.end(),exception_compare());
    //VECTOR_SORT(catch_addrs, exception_compare);
