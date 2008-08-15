--- src/core/symtabAPI/src/emitElf-64.C.orig	2008-02-20 16:52:15.000000000 -0600
+++ src/core/symtabAPI/src/emitElf-64.C	2008-07-15 15:09:32.000000000 -0500
@@ -1110,7 +1110,11 @@
 #elif defined(arch_x86_64)
             rels[i].r_info = ELF64_R_INFO(dynSymNameMapping[newRels[i].name()], R_X86_64_64);
 #elif defined(arch_power)
+#ifdef R_PPC_ADDR64
             rels[i].r_info = ELF64_R_INFO(dynSymNameMapping[newRels[i].name()], R_PPC_ADDR64);
+#else
+            rels[i].r_info = ELF64_R_INFO(dynSymNameMapping[newRels[i].name()], R_PPC64_ADDR64);
+#endif
 #endif
         }
     }
