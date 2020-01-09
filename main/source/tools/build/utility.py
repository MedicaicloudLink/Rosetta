# -*- mode:python;indent-tabs-mode:nil;show-trailing-whitespace:t; -*-
# (c) Copyright Rosetta Commons Member Institutions.
# (c) This file is part of the Rosetta software suite and is made available under license.
# (c) The Rosetta software is developed by the contributing members of the Rosetta Commons.
# (c) For more information, see http://www.rosettacommons.org. Questions about this can be
# (c) addressed to University of Washington CoMotion, email: license@uw.edu.

"""Utility functions used by the Rosetta build system.  None of these are
critical: they are mostly useful shortcuts.
"""
from __future__ import print_function
import fnmatch, os

# Paths

def make_platform_path(build_type):
    """Generate a build path from the platform build type.
These are of the form:
    <target>/build/<os>/<arch>/<compiler>[/<compiler_version>]
e.g.:
    rosetta/build/linux32/x86/gcc/3.3.3
r"""
    path = [ build_type.mode, build_type.os, build_type.arch, \
             build_type.cxx, build_type.cxx_ver ]
    path = "/".join(path)
    return path


def relative_path(from_, to_):
    """Calculate a relative path from one file to another.
    """

    def common_prefix(from_, to_):
        """A version of os.path.commonprefix that works with path -elements-
        not just characters.
        Only works on two paths.
        """
        length = min(len(from_), len(to_))
        result = []
        for ii in range(0, length):
            if from_[ii] == to_[ii]:
                result += [from_[ii]]
        return result

    import os.path
    from_ = from_.split('/')
    to_ = to_.split('/')
    prefix = common_prefix(from_, to_)
    # XXX: Assume that 'to' is a file not a directory and subtract off last
    # XXX: component in its path.
    relative = [os.path.pardir] * (len(prefix) + (len(to_) - 1))
    if not relative: relative = "."
    relative = "/".join(relative)
    return relative


# Finding files

def find_cc_sources(directory):
    return find_files(directory, "*.cc") # [".cc", ".cxx", ".cpp", ".C"])


def find_hh_sources(directory):
    return find_files(directory, "*.hh")


def find_dox_sources(directory):
    return find_files(directory, "*.dox")


def find_files(directory, pattern):
    """Find all files in or below 'directory', which match the Unix
file glob 'pattern'."""
    results = []
    for dir, subdirs, files in os.walk(directory):
        # Don't traverse Subversion directories
        if subdirs and subdirs[0] == ".svn":
            del subdirs[0]
        results += [ "%s/%s" % (dir, file) for file in fnmatch.filter(files, pattern) ]
    return results


# Map

def map_subset(map, keys):
    """Extract a subset of key/value pairs from a dictionary.
(Why this isn't part of dict's interface is beyond me.)
"""
    result = {}
    for key in keys:
        result[key] = map[key]
    return result


# Printing

def print_settings(settings, indent = 0):
    if isinstance(settings, dict):
        keys = list(settings.keys()); keys.sort()
        if len(settings) == 0:
            print("{}", end=' ')
        for key in keys:
            value = settings[key]
#             # Don't print empty items
#             if not (\
#                 type(value) is type(None) or \
#                 type(value) is str and len(value) == 0
#             ):
            if True:
                print("\n" + (indent * 2) * " " + key + ":", end=' ')
                print_settings(value, indent + 1)
    elif type(settings) in (list, tuple):
        if len(settings) == 0:
            print("[]", end=' ')
        # count = 1
        for item in settings:
            print("\n" + (indent * 2) * " " + "-", end=' ') # str(count) + ":",
            print_settings(item, indent + 1)
            # count += 1
    elif type(settings) is type(None):
        print("<NONE>", end=' ')
    elif type(settings) is str and len(settings) == 0:
        print("\"\"", end=' ')
    else:
        print(str(settings), end=' ')


def print_map(map, keys = None):
    if len(map) > 0:
        if keys is None:
            keys = list(map.keys())
            keys.sort()
        for key in keys:
            value = map[key]
            print(key, "=", value)
        print("")


def print_environment(environment, *filter):
    symbols = environment.Dictionary()
    if filter and type(filter[0]) is type(print_environment):
        keys = list(symbols.keys())
        filter = filter[0]
    else:
        keys = filter
        filter = None
    keep = {}
    for key in keys:
        key = str(key)
        value = str(symbols[key])
        if filter:
            if list(filter(key)) or list(filter(value)):
                keep[key] = value
        else:
            keep[key] = value
    sorted_keys = list(keep.keys())
    sorted_keys.sort()
    for key in sorted_keys:
        print(key, '=', symbols[key])

# Install rules

def install_links(target, source, env):
    """Install via symlink if possible, otherwise via copy.
    """
    import os
    source = "%s/%s" % (relative_path(source, target), source)
    if "symlink" in os.__dict__:
        os.symlink(source, target)
    else:
        import shutil
        shutil.copy2(source, target)
# XXX: Not sure if this works yet.
#    import stat
#    status = os.stat(source)
#    os.chmod(target, stat.S_IMODE(status[stat.ST_MODE]) | stat.S_IWRITE)
    return 0

def install_links_with_stripped_target(target, source, env):
    """Install via symlink if possible, otherwise via copy.  The
       stripped target removes the second to last part of the
       extensions, if it's 'default'

       For Example:
           target: app_name.default.linuxgccrelease
           stripped_target: app_name.linuxgccrelease
    """
    import os
    source = "%s/%s" % (relative_path(source, target), source)

    split_target_name = str(target).split(".")

    if len(split_target_name) >= 3 and split_target_name[-2] == "default":
        stripped_target = ".".join(split_target_name[:-2] + split_target_name[-1:])
    else:
        stripped_target = None

    if "symlink" in os.__dict__:
        try: os.unlink(target)
        except: pass
        os.symlink(source, target)

        if stripped_target is not None:
            try: os.unlink(stripped_target)
            except: pass
            os.symlink(source, stripped_target)
    else:
        import shutil
        shutil.copy2(source, target)
        if stripped_target is not None:
            shutil.copy2(source, stripped_target)
# XXX: Not sure if this works yet.
#    import stat
#    status = os.stat(source)
#    os.chmod(target, stat.S_IMODE(status[stat.ST_MODE]) | stat.S_IWRITE)
    return 0


def salt(build_options, separator = "_"):
    """Generate string to disambiguate binary names for different build variants.
    Currently only 'mode' is used.
    """
    return separator + build_options.mode
