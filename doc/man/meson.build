if not rst2man.found()
  subdir_done()
endif

_cdata = configuration_data()
_cdata.set('PROJECT_VERSION', meson.project_version())

# Datestamp is the time of configuration, but only the month and year.
_date = run_command(python, '-c', '''
import datetime
months = ["ERROR", "January", "February", "March", "April", "May", "June",
          "July", "August", "September", "October", "November", "December"]
today = datetime.datetime.today()
print(f"{months[today.month]} {today.year:d}")
''', check: true)
_cdata.set('DATE', _date.stdout().strip())

assert(meson.project_license() == ['BSD-3-Clause'])
_cdata.set('LICENSE_RST', 'the BSD 3-Clause License <`<https://opensource.org/license/bsd-3-clause/>`_>')

_srcs = files(
  'hpcprof.rst',
  'hpcproftt.rst',
  'hpcrun.rst',
  'hpcstruct.rst',
  'hpctoolkit.rst',
  'hpcviewer.rst',
)

_manpages = []
foreach page : _srcs
  # Extract the manual section. Default to the non-section 0 on error.
  _section = '0'
  foreach line : fs.read(page).split('\n')
    if line.contains(':manual_section:')
      _section = line.replace(':manual_section:', '').strip()
      break
    endif
  endforeach
  assert(_section != '0')

  _manpages += configure_file(
    output: '@BASENAME@.'+_section, command: [rst2man, '@INPUT@', '@OUTPUT@'],
    input: configure_file(output: '@PLAINNAME@', input: page,
                          configuration: _cdata),
  )
endforeach

install_man(_manpages)
