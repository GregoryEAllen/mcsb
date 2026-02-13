# MCSB, the Multi-Client Shared Buffer #

## Introduction ##

The Multi-Client Shared Buffer, or MCSB, is a C++ middleware library designed
to assist in implementing high-throughput, soft real-time DSP systems on POSIX
systems. MCSB uses shared memory with a zero-copy interface to provide a
scalable many-to-many message-based middleware, while providing formal
guarantees about liveness and memory bounds. Message throughput on a shared
memory machine is limited only by the memory bandwidth.

MCSB has been in continuous use since 2009. MCSB version 2 is a significant
architectural redesign, addressing limitations and lessons learned in the
design of version 1.

The MCSBv2 source code has been publicly released at
http://bitbucket.org/gallen/mcsb/.

## Features ##

Features of MCSB (since v1):

 - excels at delivering high-throughput messages on shared memory systems
 - allows zero-copy sending and receiving of messages
 - every message begins on a page-aligned boundary (for SIMD performance)
 - slow consumers will not adversely affect faster producers
    - consumers that are far behind may intentionally have messages dropped
    - such consumers are reliably notified if messages were dropped
 - includes first-class Python bindings with [SWIG](http://www.swig.org/)
 - built and installed with the widely-used tool [CMake](http://cmake.org)
 - runs nightly builds and tests on several platforms with CTest/CDash
 - has advanced command-line argument parsing with the help of
    [libvariant](http://gallen.bitbucket.org/libvariant-docs/) ArgParse
 - has an `examples` directory providing basic examples to get started
 - provides access to the underlying file descriptor for easy integration
     with external event loops (e.g. [libev](http://libev.schmorp.de/))
 - MCSB recording and playback tools available
 - MCSB-Bridge available, transparently exchanges messages on a network

Features new to v2:

 - variable message sizes with guaranteed contiguous messages
 - allocated memory can (optionally) grow to meet demand
 - a well-documented public API (with doxygen)
     served at http://gallen.bitbucket.org/mcsb-docs/
 - less sensitive to runtime queue-size parameters, including
     better utilization of free memory to avoid dropped messages
 - a new message descriptor API that can provide:
    - multiple outstanding zero-copy send buffers with out-of-order send
    - out-of-order retirement of zero-copy receive messages
    - iovec interfaces for send/recv of very large (multi-segment) messages
 - a new optional procedural API layer (BaseClient) to read incoming
     messages on demand (vs reactively, with callbacks)
 - includes a non-realtime/playback mode for as-fast-as-possible
     reprocessing/playback of recorded data
 - includes several built-in testing and performance evaluation tools

## Installation ##

MCSB is built with [CMake](http://www.cmake.org/), the cross-platform,
open-source build system. MCSB has been built and tested on several flavors of
Linux (RHEL5, RHEL6, Fedora, ARCH, Ubuntu), MacOS X (10.8-10.9), and FreeBSD.
It would probably work on most POSIX platforms with minimal effort. There are
currently no plans for Windows support.

By default, CMake uses make. Building and installing is typically:

    cd /path/to/build_dir
    cmake /path/to/source_dir
    make
    make install

Add `-DCMAKE_INSTALL_PREFIX:PATH=/path/to/install_prefix` to the `cmake` line
to install somewhere besides the default of `/usr/local`. See the CMake
documentation for more details and other CMake options.

### Dependencies ###

MCSB requires [libev](http://software.schmorp.de/pkg/libev.html), a
high-performance event loop library.

MCSB can optionally use [libvariant](http://bitbucket.org/gallen/libvariant/),
a library that provides advanced command-line argument parsing (in addition to
many other things). Without libvariant, several of the tools for testing MCSB
will not get compiled.

### Testing ###

MCSB comes with a number of automated self tests using CTest, a component of
CMake. Once MCSB configured and built, these tests can be run with:

    make test

The directory `apps/tools` also contains several tools that can be used for
manual testing of MCSB messaging.

## Examples ##

The `examples` directory contains some example programs to provide an
introduction to using MCSB. These examples are meant to be read, compiled, and
experimented with. The examples begin with the simplest `Hello, world` program
and move on to more complicated examples.

Once MCSB is configured and built, these examples can be built with:

    make examples

## Documentation ##

The public API for MCSB is in `include/MCSB`. Documentation can be generated
for MCSB by using the open source tool,
[Doxygen](http://www.stack.nl/~dimitri/doxygen/).

Once MCSB is configured, the Doxygen documentation can be built with:

    make doc

Of course, this requires Doxygen to be installed on your system.

The current MCSB Doxygen documentation is also served on the web at
[gallen.bitbucket.org/mcsb-docs/](http://gallen.bitbucket.org/mcsb-docs/).

## Mailing List ##

There is a Google mailing list for public discussion of MCSB:

	mcsb-middleware AT googlegroups DOT com

or

	http://groups.google.com/d/forum/mcsb-middleware
