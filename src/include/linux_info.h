#ifndef linux_info_h

#define LINUX_KERNEL_NAME               "<vmlinux>"
#define LINUX_KERNEL_SYMBOL_FILE_SHORT  "kallsyms"
#define LINUX_KERNEL_SYMBOL_FILE        "/proc/" LINUX_KERNEL_SYMBOL_FILE_SHORT
#define LINUX_PERF_EVENTS_FILE          "/proc/sys/kernel/perf_event_paranoid"
#define LINUX_PERF_EVENTS_MAX_RATE      "/proc/sys/kernel/perf_event_max_sample_rate"

#define PATH_KERNEL_KPTR_RESTICT        "/proc/sys/kernel/kptr_restrict"

#endif /* linux_info_h */
