# -*- mode:python;indent-tabs-mode:nil;show-trailing-whitespace:t; -*-
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington UW TechTransfer, email: license@u.washington.edu.

"""Available command-line options.

You should rarely need to edit 'defaults', usually only when
a new platform or compiler becomes supported.

Wildcards ("*") ask the build system to autodetect the value.

Note that none of the select_* functions will be called with
values not allowed by defaults.  Any failures will be caused
by the build system not actually supporting a platform which
we claim to support.

compilers - Compilers and their supported version numbers.
operating_systems - OSes and their supported version numbers/names.
  XXX: Do we need to support OS bit-widths?
architectures - Processors and their supported bit-widths.
"""

import sys, os
if sys.version_info >= (2, 3):
    import platform
    from platform import uname as _uname
else:
    from os import uname as _uname
from os import popen as _popen

def select_compiler(supported, requested):
    """Figure out the actual C++ compiler name, based on the compiler type.
    Default is the requested name.

    requested - The requested compiler name.
    """
    actual = {
#        "gcc" : "g++",
#        "icc" : "icpc",
        # XXX: others?
    }.get(requested, requested)

    return actual


def select_compiler_version(supported, compiler, requested):
    """Figure out the actual compiler version.
    How to do this is compiler-dependent.  A compiler that can't
    provide its version number is currently assumed to be unsupported
    but what this really means is that this function should be edited
    to account for that case.

    requested - The requested compiler version number, as a string.
      If the requested number doesn't match the actual number, a
      KeyError is raised.  A wildcard ("*") will match any compiler
      version.
    expected - The value as provided by the compiler itself.
    compiler - The name of the actual compiler.  If the requested
      version is not one of the legal values for the compiler a
      ValueError is raised.  As the system is currently implemented
      this will never happen: the SCons Options class will catch this
      condition in the SConstruct.
"""

    if requested == "*":
        actual, actual_full = _get_compiler_version(compiler)
    else:
        actual, actual_full = requested, requested

    if not actual:
        raise RuntimeError, \
            "Could not determine version for compiler '%s'.  Check whether compiler by that name is installed and on your path." % \
                (compiler)

    if compiler in supported.cxx:
        versions = supported.cxx[compiler]
        if actual not in versions:
            raise KeyError, \
                "Unknown version number %s for compiler '%s'" % \
                (actual, compiler)
    else:
        raise KeyError, \
            "Do not know how to verify version number for unknown compiler '%s'" % \
            (compiler)

    # If the actual version doesn't match the requested version, fail.
    # By match we mean 'substring rooted at the start': it's legitimate
    # to get any 3.4.x compiler with a request for 3.4 for example.
    # A wildcard request will match any actual version.
    if requested != "*":
        if actual_full.find(requested) != 0:
            raise ValueError, \
                "Actual compiler version '%s' does not match requested version '%s'" % \
                (actual, requested)

    # If the request was for "*" and we haven't hit any errors, then we
    # want to keep actual as "*" to properly select the unadorned compiler.
    if requested == "*":
        actual = requested

    return actual


def select_os(supported, requested, expected = None):
    """Figure out which OS is being run.
    requested - The name of the OS selected by the options
    expected - The actual name of the OS being run on as specified
      in platform.uname().
"""

    if not expected:
        expected = _get_os()

    # Map names we've seen from _get_os() to their build system names.
    # All unspecified cases default to 'expected'.
    actual = {
        "darwin": "macos",
        "cygwin_nt-5.1": "cygwin",
    }.get(expected, expected)

    # Look up translated name in the default os names.
    if actual not in supported.os:
        raise KeyError, "Operating system '%s' is unsupported." % (actual)

    if requested != "*" and requested != actual:
        raise ValueError, "Actual operating system '%s' does not match requested version '%s'" % (actual, requested)

    return actual


def select_os_version(supported, os, requested):
    """
"""
    actual, actual_full = _get_os_version()

    if os == "macos":
        if actual.startswith("8."):
            actual = "10.4"
        elif actual.startswith("9."):
            actual = "10.5"
        elif actual.startswith("10."):
            actual = "10.6"
        elif actual.startswith("11."):
            actual = "10.7"
    if requested != "*" and requested != actual:
        raise ValueError, "Actual operating system version '%s' does not match requested version '%s'" % (actual, requested)


    return actual


def select_arch(supported, os, requested, expected = None):
    """Figure out which processor is being run on.

    Caveats:
    - There is currently no way to distinguish Intel x86 from AMD x86.
    - It's not clear if uname() can tell if a platform is 64-bit.
"""

    if not expected:
        expected = _get_arch()

    # Map names we've seen from _get_arch() to their build system names.
    # All unspecified cases default to 'expected'.
    actual = {
        # Results from platform.processor()
        "i386": "x86",
        "i486": "x86",
        "i586": "x86",
        "i686" : "x86",
        "x86_64" : "x86",
        "ppc64" : "ppc64",
        "powerpc" : "ppc",
        # Results from os.uname()['machine']
        # This isn't strictly true.  But we are not currently distinguishing
        # between AMD and Intel processors.
        "athlon" : "x86",
        "Power Macintosh" : "ppc",

	# Some architectures for Gentoo Linux:
	"Intel(R) Core(TM)2 CPU T7400 @ 2.16GHz" : "x86",
	'Intel(R) Xeon(TM) CPU 3.00GHz' : "x86",
	'Intel(R) Core(TM) i7 CPU Q 720 @ 1.60GHz' : "x86",
    }.get(expected, expected)

    # Windows returns a blank string.  Assume it's running on x86.
    if actual == "":
        if os == "windows":
            actual = "x86"

    # Look up translated name in the default os names.
    if actual not in supported.arch:
        raise KeyError, "Processor architecture '%s' is unsupported." % (actual)

    if requested != "*" and requested != actual:
        raise ValueError, "Actual processor architecture '%s' does not match requested version '%s'" % (actual, requested)

    return actual


def select_arch_size(supported, os, arch, requested):
    """Figure out the bit-width of the current processor architecture.
There's no obvious portable way to do this.  By default
just assume the requested size is correct.
"""
    actual = _get_arch_size()
    actual = {
        "32bit" : "32",
        "64bit" : "64",
        # XXX: We are guessing here.  This may prove incorrect
        "i386" : "32",
        "i486" : "32",
        "i586" : "32",
        "i686" : "32",
        # XXX: What do 64 bit Macs show?
        "Power Macintosh" : "32",
    }.get(actual, "<unknown>")

    # Windows returns a blank string.  Assume it's running on x86.
    if actual == "":
        if os == "windows":
            actual = "32"

    if os == "macos":
        if requested != "*":
            actual = requested
        else:
            sysctlhwlines = _popen("/usr/sbin/sysctl hw").readlines()
            if "hw.optional.x86_64: 1\n" in sysctlhwlines:
                # only x86_64 Macs default to building 64-bit binaries
                actual = "64"
            else:
                actual = "32"
            # 64-bit builds segfault on Mac OS X 10.4 (Darwin 8.x)
            if _get_os_version()[0].startswith("8."):
                actual = "32"

    return actual


def select_cat(supported, requested):
    """
    """
    if str(requested) == "all":
        actual = supported.cat
    else:
        actual = list(requested)
    return actual


def get_os_pathsep():
    if sys.version_info >= (2, 3):
        pathsep = os.path.pathsep
    else:
        pathsep = os.pathsep


def _get_compiler_version(compiler):
    """Ask the compiler for it's version number.

Most compilers can tell you their version from the command line
but the command line probably differs from compiler to compiler.
Neither GCC nor Intel C++ provide this number isolated: it needs
to be parsed out.
"""
    # We don't handle MSVC yet.  Don't know how to ask it for it's
    # version, or what the output would look like.
   # assert compiler == "gcc" or compiler == "icc", \
   #        "Compiler '%s' needs explicit handling in %s._get_compiler_version" % \
   #        (compiler, __name__)
    if compiler == 'gcc':
        compiler_output = os.popen("%s -dumpversion" % compiler).read()
        if compiler_output:
            full_version = compiler_output.strip()
            version = ".".join(full_version.split(".")[0:2])
        else:
            full_version = "8.0"#None
            version = "8.0"#None
    else:
        compiler_output = os.popen("%s --version" % compiler).read()
        # New versions of Apple provided clang return: "Apple clang version 2.0 (tags/Apple/clang-137) (based on LLVM 2.9svn)..."
        if compiler_output:
            full_version = compiler_output.split()[2]
            if full_version == 'version' and compiler == 'clang': 
                full_version = compiler_output.split()[3]
            version = ".".join(full_version.split(".")[0:2])
            
        else:
            full_version = "8.0"#None
            version = "8.0"#None
    return version, full_version


def _get_os():
    """Ask Python what the operating system name is.
    """
    return _uname()[0].lower()


def _get_os_version():
    """Ask Python what the operating system version is.
    """
    full_version = ".".join(_uname()[2].split(".")[0:3])
    version = ".".join(full_version.split(".")[0:2])
    return version, full_version


def _get_arch():
    """Ask Python what the architecture is.

    On Python versions without 'platform' this gets tricky.
    """
    # Note that these are very different values, and need processing
    if globals().has_key("platform"):
        return platform.processor() or _uname()[4]
    else:
        return _uname()[4]

def _get_arch_size():
    """Ask Python what the architecture size is.

    On Python versions without 'platform' this gets tricky.
    """
    if globals().has_key("platform"):
        return platform.architecture()[0]
    else:
        return _uname()[4]
