#New job distributor README

Author: Steven Lewis

last updated 8/31/10

-   author (of the readme): Steven Lewis (smlewi@gmail.com)
-   author (of the code): Steven Lewis, Andrew Leaver-Fay
-   author for SilentFile IO: James Thompson, Oliver Lange
-   author for Parser concrete classes: Sarel Fleishman
-   author for MPI job distributors: Doug Renfrew, Oliver Lange

Am I using jd2?
===============

Open up the executable's code and search for the letters "jd2", if they're there, you're probably using jd2 and this documentation applies. If you find protocols::jobdist instead, this documentation probably does not apply.

What work should be done? Singleton manager and polymorphic JobDistributor
==========================================================================

A) This Job Distributor (JD) is intended to automatically determine from compile- and run-time flags what sort of work needs to be done, and instantiate a Job Distributor capable of doing that. The JD is a singleton object, meaning that (on single processor runs) there will only be, at most, one JD object created. Multiple processors will still use a singleton pattern with various thread safety caveats. MPI puts one JD on each processor, with communication between them.

A JobDistributorFactory class is responsible for determining what JD should exist, and creating that one, upon the first request (via the singleton interface) for the JD. JobDistributorFactory.cc therefore contains the logic which determines what JobDistributor class actually gets instantiated in any given invocation of an executeable. This is easy code to read, and by definition it is up to date, whereas this document might not be (so look at it if these instructions fail!)

One concrete class is FileSystemJobDistributor, which distributes jobs to a single processor. It can coordinate independent executeables by marking in-progress jobs in the filesystem. This distributor is compatible with running separate jobs in separate directories, or many instances of one command line (large nstruct) in the same directory, with the option -run::multiple\_processes\_writing\_to\_one\_directory. This is the vanilla, default distributor.

ShuffleJobDistributor exists; this author is not sure of its purpose. I think it's for randomizing job run order; this is of value for its child BOINCJobDistributor. BOINC is used for the Rosetta project, setting up BOINC to use this distributor is beyond the scope of this documentation.

MPI Job Distributors
====================

1) This one is MPI in name only. Activate it with jd2::mpi\_work\_partition\_job\_distributor. Here, each processor looks at the job list and determines which jobs are its own from how many other processors there are (in other words, divide number of jobs by number of processors; each processor does that many). Its files contain an example. This KD has the best potential efficiency with MPI but isn't useful for big stuff, because you have to load-balance yourself by setting it up so that (njobs % nprocs) is either zero or close to nprocs. It is recommended only for small jobs, or jobs with tightly controlled runtimes. I use it for generating a small number of trajectories (100 jobs on 100 processors) to determine where to set my filters, then run large jobs under the next jd.

2) MPIWorkPoolJobDistributor: this is the default in MPI mode. Here, one node does no Rosetta work and instead controls all the other nodes. All other nodes request jobs from the head node, do them, report, and then get a new one until all the jobs are done. If you only have one processor, then you get only a head node, and no work is done: make sure you're using MPI properly! This one has "blocking" output - only one job can write to disk at a time, to prevent silent files from being corrupted. It's a good choice if the runtime of each individual job is relatively large, so that output happens sparsely (because then the blocking is a non-issue). If your jobs are very short and write to disk often, or you have a huge number of processors and they write often just because there's a ton of them, this job distributor will be inefficient. As of this writing, this is the job distributor of choice when writing PDB files.

3) MPIFileBufJobDistributor: this is the other default for MPI. It allocates two nodes to non-rosetta jobs. One head node as before and one node dedicated to managing output. It is nonblocking, so it is the best choice if job completion occurs often relative to how long filesystem writes take (short job times or huge numbers of processors or both). At the moment it works only with silent file output. It is the best choice if you have short job times or large (many thousands) of processors. (Think abinitio)

4) There's another one called MPIArchiveJobDistributor, maybe Oliver will add documentation for it.

Batching - It exists! I don't know how to use it! Maybe Oliver will add documentation for it.

What sort of I/O? How is it separated from the JobDistributor?
==============================================================

B) We want many types of I/O in Rosetta. To prevent a combinatorial problem between JobDistributors and I/O, the I/O is handled in two classes: JobInputter and JobOutputter. JobDistributor keeps OPs to one each of these classes, and all JobInputter/Outputter classes should (ideally) be compatible with any JobDistributor.

JobInputter is responsible for determining what jobs can exist, and for creating poses from Job objects. The simplest class is PDBJobInputter, which handles PDBs or gzipped PDBs from disk. It determines what jobs ought to exist from -s/-l and -nstruct. Other JobInputters exist, one based on silent files and one which does not have starting structures for abinitio. (Threading also exists, I don't know what it does?)

JobOutputter is responsible for outputting jobs' results. This class interfaces with both the JobDistributor and actual protocols. Its three responsibilities are A) determining what a job's name is and/or where its results are stored, B) if the job has completed data (left over from a previous run, or from another concurrent execution) already stored and thus should not be reattempted, and C) handling the storage of completed protocols' results (poses and other data).

JobOutputter classes will output scored poses and scorefiles as you might want. However, these classes do NOT know or care about scorefunctions, they print energies based only on things stored in the Pose (thus, the energies object). YOUR MOVER is responsible for preparing these score repositiories. The scores stored in the Energies object (whatever was put there last time the pose was scored) can be printed. Additional scores & data can be printed via the Job object (below).

What's in a Job?
================

C) Jobs are three-layered. One layer is the starting pose, the next is nstruct, and the last is output data relevant to the job. The first layers contain two pieces of information: all the data needed to create a pose and start a protocol is one; which index into nstruct the job is is the other. To prevent massive duplication of the first (expensive) data, there are two layers. Job objects contain an nstruct index and a pointer to an InnerJob; the InnerJob contains the heavier information. InnerJob has a PoseCOP, and input string naming where to look for a PDB, silent file, etc, and the maximum nstruct for that InnerJob. This may have been changed somewhat; in some cases the JobInputter retains the PoseCOP instead of InnerJob.

The third layer is output data. This “accessory data” is extra stuff you want dumped at the end of the PDB like in Rosetta++. Anything you load into the Job will get attached to the final pose in some fashion (in the silent file, PDB, scorefile, etc). If you are printing intermediate poses, they will also have this data attached; you can change that by making a temporary Job object for the intermediate pose (the JobDistributor will ignore it, this is a local object), filling it, and handing it out to the JobOutputter (see example in jd2test.cc). The functions for adding extra data are Job-\>add\_string(), add\_string\_string\_pair, and add\_string\_real\_pair()

What can I distribute?
======================

D) You can work with the regular Mover class's children. The two significant changes to Mover are A) a special copy-like operation called fresh\_instance() which allows the JD to recreate the mover new for each job if desired, along with some bool-returning functions stating if this is desireable, and B) a status function which returns an enumerated type indicating success/failure of the last apply() (MoverStatus). Any Mover will work without modification, but you'll have to add code to take advantage of all the features.

What's new?
===========

E) The new framework allows two major new things, plus more that's not documented here, or only sort-of-new: a) protocols can output poses and information tagged with the Job's identifier mid-protocol. This means you can get myjob\_0001.pdb at the end, plus centroid\_myjob\_0001.pdb as the end of the centroid phase of your protocol. Activate these features via JobOutputter-\>other\_pose().

b) Movers can automatically filter their results, and tell the job distributor “well, this run was a waste, don't increment the job count and try again”. Activate filtering through Mover class's set\_last\_move\_status function; this must be set in the outermost Mover that the job distributor is running directly.

Use
===

F) Use apps/pilot/JD2/jd2test.cc is an app that uses the new JD; it's basically just a hack using SidechainMover and PackRotamersMover which demonstrates a few of the new functions. It also serves as a integration test ensuring proper functionality. apps/public/scenarios/FloppyTail.cc was built from the ground up with the new job distributor in mind and uses a few of its advanced functions.

Parser
======

G) An interface for a Parser class has been included. The Parser will take XML files and create your mover at runtime from the instructions in that file. Please see the [[RosettaScripts Developer Guide]] for more information.

EMPTY\_JOB\_use\_jd2 output
====================

H)  What will happen if a mover attempts to query the job distributor, but it is not actually running within the job distributor's `go` function? This might happen if you are debugging code, or perhaps if you've borrowed someone else's mover without examining it closely. The JobDistributor's initial state contains a faux Job object; the JobDistributor will behave as if all jobs are that object. If you get output tagged EMPTY\_JOB\_use\_jd2, this is where it came from.

I want extra input per trajectory (two poses per nstruct, etc)
==============================================================

I) Many people have asked about passing arbitrary extra input data per trajectory - a new resfile with each input pose, or different fragments, or two poses as input, or whatever.

The Mover interface allows for one and only one input per trajectory: a starting Pose. This is a limitation of the Mover interface. Altering other data per-trajectory requires using non-base-class Mover functions (and is thus something the JD2 can't do.)

The workaround is to subclass the JobInputter. One of the major purposes for its OO-modularity is to make it easy to subclass. Subclass JobInputter to my\_special\_protocol\_JobInputter, and have it organize your extra data.

Where does the extra data go? One place would be the Pose itself, in the DataCache. Another choice would be to also subclass the Job object and put it there. Either way you're pointer casting to extract the data.

Alternatively, the [[ResourceManager]] works with JD2 to facilitate accessing extra data resources from within a protocol. In RosettaScripts, see documentation for the [[MultiplePoseMover]].

##See Also

* [[Development tutorials home page|devel-tutorials]]
* [[Development Documentation]]: The development documentation home page
* [[Rosetta tests]]: Links to pages on running and writing tests in Rosetta
* [[Rosetta overview]]: Overview of major concepts in Rosetta
* [[RosettaEncyclopedia]]: Detailed descriptions of additional concepts in Rosetta.
* [[Glossary]]: Defines key Rosetta terms