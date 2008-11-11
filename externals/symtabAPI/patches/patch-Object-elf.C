--- src/core/symtabAPI/src/Object-elf.C.orig	2008-04-11 15:46:40.000000000 -0500
+++ src/core/symtabAPI/src/Object-elf.C	2008-11-11 10:30:08.000000000 -0600
@@ -2928,7 +2928,8 @@
          pd_dwarf_handler(err, NULL);
          return false;
       }
-      lsda = ((char *) fde_bytes) + sizeof(int) * 4;
+      
+      lsda = ((char *) fde_bytes) + sizeof(int)*2 + sizeof(long)*2;
       if (lsda == outinstrs) {
          continue;
       }
@@ -3073,9 +3074,6 @@
       //likely to happen if we're not using gcc
       return true;
    }
-#if defined(arch_x86_64)
-   return true;
-#endif
 
    eh_frame_base = eh_frame->sh_addr();
    except_base = except_scn->sh_addr();
@@ -3084,10 +3082,6 @@
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
