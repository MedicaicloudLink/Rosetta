#A Guide to the Structure of the Rosetta Library

The `rosetta/main/source` directory contains eight top level directories of which every developer should be aware.

-   `src/` The src directory contains all the source files for the rosetta library. See [[An overview page for the src directory of the Rosetta|src-index-page]] for more information.
-   `test/` The test directory contains source files for rosetta's unit tests. The integration, and scientific tests now live in rosetta/main/tests. Together, these two sets of tests are used to verify that the rosetta library is functioning correctly. See [[A Guide to Running and Writing Tests for Rosetta|rosetta-tests]] for more information on Rosetta Tests.
-   `bin/` The bin directory contains soft links to all the rosetta applications. Currently separate softlinks are stored for each combination of operating system (linux, macos), compiler(gcc, icc), and build mode (debug, release, release_debug). See the [[build documentation]] page for further information of the build system.
-   `build/` The build directory is where all the object files and shared libraries for build configuration are stored. The directory structure is automatically created by the scons build system. See the [[build documentation]] page for further information of the build system.
-   `scripts/` The scripts directory is the recommended location for all RosettaScripts XML files (in the rosetta_scripts subdirectory) and Python scripts for scientific purposes (in the python subdirectory). Private PyRosetta applications are also placed in this directory.
-   `doc/` The doc directory [[once contained|History-of-Rosetta-documentation]] all the stand alone doxygen pages. Those pages are now this wiki (the documentation repository). It still holds an autogenerated page of option system information.
-   `tools/` The tools directory contains the files for the custom SCons Builder for the rosetta library. See the [[build documentation]] page for further information of the build system.
-   `demo/` The demo directory once contained demos (now the demos repository).
##See Also

* [[Glossary]]: Brief definitions of Rosetta terms
* [[RosettaEncyclopedia]]: Detailed descriptions of additional concepts in Rosetta.
* [[Rosetta overview]]: Overview of major concepts in Rosetta
* [[Development Documentation]]: The main development documentation page

<!--- SEO
code structure
code structure
code structure
code structure
library structure
library structure
library structure
library structure
--->