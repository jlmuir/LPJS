# SPJS

## Description

SPJS (Simple, Portable Job Scheduler) is a batch system, i.e. a job scheduler
and resource manager for HPC (High Performance Computing) clusters.

Unlike other batch systems, SPJS is designed to be easy to deploy and manage,
and portable to any
POSIX platform.  Other existing batch systems are extremely complex (including
our long-time favorite SLURM, which originally stood for "Simple Linux Utility
for Resource Management", but is no longer simple by any stretch of the
imagination).  For example, recent versions of SLURM require complex database
setup to enable job accounting.

As such, other batch systems present a major barrier to learning and
implementing small-scale HPC, which is crucial for research groups that have
no ready access to centralized HPC resources.

In some venues, centralizing HPC into one massive cluster improves utilization
of resources and reduces cost.  In many other venues, maintaining a large
cluster is simply not feasible.

Large HPC clusters are dominated by Redhat Enterprise Linux and its derivatives
for good reasons, such as commercial support for the operating system and
support for commercial science and engineering applications such as
ANSYS, Fluent, Abacus, etc.  The need to support a wide range of commercial
and open source software on very large-scale clusters has led to the
prohibitive complexity of most existing batch systems.

In contrast, the SPJS project is committed to the following design principals:

- KISS (Keep it simple, stupid).  We will not allow SPJS to fall victim to
creeping feature syndrome.

- Complete portability.  Our primary intention is to foster research and
development of HPC clusters using any POSIX operating system on any hardware.

- Minimal configuration.  HPC sysadmins should only be required to provide
information that is not easily attained automatically.  E.g. compute node
hardware specifications shall be automatically detected on all platforms.
Most configuration parameters will simply be overrides of reasonable defaults.

As such, SPJS makes it easy to build and maintain small HPC clusters for
independent research groups using open source software, or experiment
with alternative operating systems or hardware platforms.

SPJS will require only functionality that can be implemented with reasonable
effort on all POSIX platforms, including but not limited to:

- Queuing of batch jobs

- Management of available cores and memory

- Enforcement of resource limits stated by job descriptions

- Job monitoring

- Job accounting (maintaining records of completed jobs)

This does not preclude support for operating system specific features such
as cgroups, but all such features will be optional, implemented and
installed separately as plugins.

## Design and Implementation

The code is organized following basic object-oriented design principals, but
implemented in C to minimize overhead and keep the source code accessible to
scientists who don't have time to master the complexities of C++.

Structures are treated as classes, with accessor and mutator functions
(or macros) provided, so dependent applications and libraries need not access
structure members directly.  Since the C language cannot enforce this, it's
up to application programmers to exercise self-discipline.

For detailed coding standards, see
https://github.com/outpaddling/Coding-Standards/.

## Building and installing

SPJS is intended to build cleanly in any POSIX environment on any CPU
architecture.  Please don't hesitate to open an issue if you encounter
problems on any Unix-like system.

Primary development is done on FreeBSD with clang, but the code is frequently
tested on CentOS, MacOS, and NetBSD as well.  MS Windows is not supported,
unless using a POSIX environment such as Cygwin or Windows Subsystem for Linux.

The Makefile is designed to be friendly to package managers, such as
[Debian packages](https://www.debian.org/distrib/packages),
[FreeBSD ports](https://www.freebsd.org/ports/),
[MacPorts](https://www.macports.org/), [pkgsrc](https://pkgsrc.org/), etc.
End users should install via one of these if at all possible.

I maintain a FreeBSD port and a pkgsrc package, which is sufficient to install
cleanly on virtually any POSIX platform.  If you would like to see a
package in another package manager, please consider creating a package
yourself.  This will be one of the easiest packages in the collection and
hence a good vehicle to learn how to create packages.

### Installing SPJS on FreeBSD:

FreeBSD is a highly underrated platform for scientific computing, with over
1,900 scientific libraries and applications in the FreeBSD ports collection
(of more than 30,000 total), modern clang compiler, fully-integrated ZFS
filesystem, and renowned security, performance, and reliability.
FreeBSD has a somewhat well-earned reputation for being difficult to set up
and manage compared to user-friendly systems like [Ubuntu](https://ubuntu.com/).
However, if you're a little bit Unix-savvy, you can very quickly set up a
workstation, laptop, or VM using
[desktop-installer](http://www.acadix.biz/desktop-installer.php).  If
you're new to Unix, you can also reap the benefits of FreeBSD by running
[GhostBSD](https://ghostbsd.org/), a FreeBSD distribution augmented with a
graphical installer and management tools.  GhostBSD does not offer as many
options as desktop-installer, but it may be more comfortable for Unix novices.

```
pkg install SPJS
```

### Installing via pkgsrc

pkgsrc is a cross-platform package manager that works on any Unix-like
platform. It is native to [NetBSD](https://www.netbsd.org/) and well-supported
on [Illumos](https://illumos.org/), [MacOS](https://www.apple.com/macos/),
[RHEL](https://www.redhat.com)/[CentOS](https://www.centos.org/), and
many other Linux distributions.
Using pkgsrc does not require admin privileges.  You can install a pkgsrc
tree in any directory to which you have write access and easily install any
of the nearly 20,000 packages in the collection.  The
[auto-pkgsrc-setup](http://netbsd.org/~bacon/) script can assist you with
basic setup.

First bootstrap pkgsrc using auto-pkgsrc-setup or any
other method.  Then run the following commands:

```
cd pkgsrc-dir/sysutils/SPJS
bmake install clean
```

There may also be binary packages available for your platform.  If this is
the case, you can install by running:

```
pkgin install SPJS
```

See the [Joyent Cloud Services Site](https://pkgsrc.joyent.com/) for
available package sets.

### Building SPJS locally

Below are cave man install instructions for development purposes, not
recommended for regular use.

1. Clone the repository
2. Run "make depend" to update Makefile.depend
3. Run "make install"

The default install prefix is ../local.  Clone SPJS,  and dependent
apps into sibling directories so that ../local represents a common path to all
of them.

To facilitate incorporation into package managers, the Makefile respects
standard make/environment variables such as CC, CFLAGS, PREFIX, etc.  

Add-on libraries required for the build, such as , should be found
under ${LOCALBASE}, which defaults to ../local.
The library, headers, and man pages are installed under
${DESTDIR}${PREFIX}.  DESTDIR is empty by default and is primarily used by
package managers to stage installations.  PREFIX defaults to ${LOCALBASE}.

To install directly to /myprefix, assuming  is installed there as well,
using a make variable:

```
make LOCALBASE=/myprefix clean depend install
```

Using an environment variable:

```
# C-shell and derivatives
setenv LOCALBASE /myprefix
make clean depend install

# Bourne shell and derivatives
LOCALBASE=/myprefix
export LOCALBASE
make clean depend install
```

View the Makefile for full details.
