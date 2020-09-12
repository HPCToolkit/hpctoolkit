# HPCToolkit

HPCToolkit is an integrated suite of tools for measurement and
analysis of program performance on computers ranging from multicore
desktop systems to the nation's largest supercomputers. HPCToolkit
provides accurate measurements of a program's work, resource
consumption, and inefficiency, correlates these metrics with the
program's source code, works with multilingual, fully optimized
binaries, has very low measurement overhead, and scales to large
parallel systems. HPCToolkit's measurements provide support for
analyzing a program execution cost, inefficiency, and scaling
characteristics both within and across nodes of a parallel system.

HPCToolkit is released under the 3-clause BSD license.
See the file README.License for details.


## Build Instructions

Build instructions are available at:

http://hpctoolkit.org/software-instructions.html

HPCToolkit can be installed either by the one-button `spack install
hpctoolkit` method, or by building the prerequisites with spack and
then using the classic autotools `configure && make && make install`
method.


## Documentation

Documentation is available at the HPCToolkit home page:

http://hpctoolkit.org/index.html

In particular, there is a PDF User's Manual at:

http://hpctoolkit.org/manual/HPCToolkit-users-manual.pdf


## Bug Reports and Questions

The best way to contact us for bug reports or questions is via the
mailing list hpctoolkit-forum =at= rice.edu.  The mailman list is
technically members-only, but you don't really need to be a member.
Just send mail to this address and then we can white-list you.

If reporting a problem, be sure to include sufficient information
about your system, what you tried, what went wrong, etc, to allow us
follow your steps.

* type of system, architecture, linux distribution and version,
compiler version, glibc version.

* hpctoolkit version.

* if a build failure, then the `config.log` file from the build
directory and the output from `make`.

* if a run failure, then what command you ran and how it failed.

