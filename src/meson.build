# Generate the hpctoolkit-config.h header
config_h = vcs_tag(output: 'hpctoolkit-config.h', input: configure_file(output: 'hpctoolkit-config.h.in', configuration: cfg))

subdir('lib')
subdir('tool')
