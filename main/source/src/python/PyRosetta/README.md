PyRosetta 4
===========

Building with external Rosetta libraries
----------------------------------------

PyRosetta may be linked against externally compiled Rosetta shared libraries.
This may be used to suppport "extras" builds or debug builds of the shared
libraries with release-mode bindings. To support external links, configure &
build via cmake and clang, then specify `--external-link` to `build.py`.

For example:
````
export CC=`which clang`
export CXX=`which clang++`

cd `git rev-parse --show-toplevel`

pushd source/cmake
./make_project.py all

pushd build_debug
cmake -G ninja && ninja

popd
popd

pushd source/src/python/PyRosetta
python build.py --external-link debug
````

Building PyRosetta under Anaconda python
----------------------------------------
If you use Anaconda python (https://www.continuum.io) and would like to use
it to build PyRosetta, you will need to make the build system aware of the
location of the appropriate C headers and library. These values are specified
using the `python-include-dir` and `python-lib` command line flags when calling
`build.py`. For example, if Anaconda 3 is installed in the default location,
the command to run `build.py` would be:

`python build.py -jX --python-include-dir=$HOME/anaconda3/include/python3.5m --python-lib=$HOME/anaconda3/lib/libpython3.5m.dylib`

where X is the number of jobs to parallelize the build over.

Bootstrap PyRosetta Development Environment
-------------------------------------------

An existing PyRosetta build can be used to bootstrap a development
environment for python-level development. The bootstrap process will (a)
reset the work tree to match the source binary version and (b) copy the
source binary compiled module and database into the PyRosetta `src`
directory.

To bootstrap into a conda-based development environment:

```
# Create and activate a development environment.
conda create -n pyrosetta-dev python=3.6
conda activate pyrosetta-dev

# Install pyrosetta, optionally specifying a target development version.
conda install -c rosettacommons pyrosetta

# Copy binary and reset tree to the prebuilt version.
./bootstrap_dev_env

# Replace conda package with development-mode install of working tree.
conda remove pyrosetta && pushd src && python setup.py develop
```

The bootstrap process resets the git working tree to the prebuilt version
in a detached HEAD state. To begin development create a feature branch
`git checkout -b user/py_feature`. If you have already created commits on
a existing branch your git HEAD will no longer point to this branch.

You can recover this initial work post-bootstrap by checkout,
cherry-pick, or rebase. Assuming you were on an branch pre-bootstrap:

* Create new branch `git checkout -b user/py_new`, checkout the state of
  the old branch, `git checkout HEAD@{1} -- src` and work and
  recommit `git commit`.

* Create new branch `git checkout -b user/py_new`, cherry-pick all the
  work in the old branch `git cherry-pick origin/master..HEAD@{1}`.

* Rebase the old branch (here `user/py_feature`) onto the current HEAD
  `git rebase -i --onto HEAD origin/master user/py_feature`.
