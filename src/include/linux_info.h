#ifndef linux_info_h

#define LINUX_KERNEL_NAME_REAL          "vmlinux"
#define LINUX_KERNEL_NAME_REAL_CHARS    7
#define LINUX_KERNEL_NAME               "<"  LINUX_KERNEL_NAME_REAL  ">"
#define LINUX_KERNEL_SYMBOL_FILE_SHORT  "kallsyms"
#define LINUX_KERNEL_SYMBOL_FILE        "/proc/" LINUX_KERNEL_SYMBOL_FILE_SHORT
#define LINUX_PERF_EVENTS_FILE          "/proc/sys/kernel/perf_event_paranoid"
#define LINUX_PERF_EVENTS_MAX_RATE      "/proc/sys/kernel/perf_event_max_sample_rate"

#define LINUX_KERNEL_KPTR_RESTICT        "/proc/sys/kernel/kptr_restrict"

#define LINUX_KERNEL_SUFFIX_CHARS       8

// directory where to collect the copied kallsyms files
#define DIRECTORY_FILE_COLLECTION   "collect"

#endif /* linux_info_h */
