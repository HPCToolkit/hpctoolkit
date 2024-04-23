#ifndef HPCTOOLKI_VERSION_HPP
#define HPCTOOLKI_VERSION_HPP

#ifdef __cplusplus
extern "C" {
#endif

/// Print the HPCToolkit version information to stdout
void hpctoolkit_print_version(const char* prog_name);

/// Also print information on enabled features
void hpctoolkit_print_version_and_features(const char* prog_name);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // HPCTOOLKI_VERSION_HPP
