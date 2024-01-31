# Generate the (two) generated headers
config_h = [
  configure_file(output: 'hpctoolkit-config.h', configuration: cfg),
  vcs_tag(output: 'hpctoolkit-version.h', input: files('hpctoolkit-version.h.in')),
]
