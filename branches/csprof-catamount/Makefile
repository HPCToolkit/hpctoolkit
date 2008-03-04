module_dirs := utilities                \
               memory                   \
               processor/x86-64         \
               unwind-analysis/common \
               unwind-analysis/x86-family \
               sampling/types/callstack \
               Pmsg                     \
               hpcfile                  \
               os/catamount             \
               os/linux                 \
               sampling/sources/itimer  \
               sampling/sources/papi    \
               lush                     \
               lush-agents

.PHONY: TAGS
TAGS:
	etags *.[csh] hpcfile/*.[csh] $(addsuffix /*.[csh],$(module_dirs))
