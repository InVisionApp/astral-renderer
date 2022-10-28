Astral
======

Astral is an experimental project work-in progress to build a renderer
that allows for a large number of items on the screen and in addition
custom shaders for dramatic effects.


Building requirements
=====================
 - GNU Make
 - g++ (clang should be fine too)
 - perl
 - pkg-config
 - freetype2 version 2.10 or higher
 - SDL 2.0 and SDL Image 2.0 (for demos)
 - doxygen (for documentation)
 - graphviz (for documentation)


Building General
================

Do `make targets` to see what demos are available to build, each
demo foo has a foo-release and foo-debug variant. Build all
release demos with `make demos-release` and all debug variants
with `make demos-debug`. Build all release and debug demos with
`make demos`.

To check for dependencies type `make check`.


OpenGL and OpenGL ES Support
============================

Both the library and demos support (with the same build)
both OpenGL and OpenGL ES. For the demos, to attempt to use
OpenGL ES, add the command line argument `use_gles true`
to attempt to use OpenGL ES. For Linux, the open source
Mesa drivers support OpenGL ES. For MacOS, one needs to
have built ANGLE and have its libEGL and libGLESv2 libraries
in one's library path. Support for OpenGL ES on MS-Windows
is unsupported at this time.


Recipe on MacOS to get pre-requisites
=====================================

(all assuming that brew is installed)

1. Install X-Code and agree to the liscense
2. brew install pkg-config
3. brew install sdl2
4. brew install sdl2_image
5. brew install freetype
6. brew install doxygen (for documentation)
7. brew install graphviz (for documentation)


Mac M1 Compatibility
====================

The M1's GL implementation does not support glCopyBufferSubData (atleast
there is a warning printed to console when it is invoked). To avoid this
issue, add the following to the command line when running the demos:
`static_data_layout texture_2d_array vertex_buffer_layout texture_2d_array`
and when using Astral in an application set `m_vertex_buffer_layout` and
`m_static_data_layout` of the `RenderEngineGL3::Config` to the value of
`RenderEngineGL3::texture_2d_array`. This silences the warning.


IntelGPU for MacOS
==================

Tests have shown Astral runs horribly slow with Apple's GL drivers
for Intel integrated GPU's; this does NOT affect M1's or Mac's with
discrete GPU's. In the case where the only GPU available is an
Intel integrated GPU, one might recover performance by using OpenGL ES
using ANGLE. The IntelGPU works fine under Linux, thus the issue is NOT
the GPU or Astral; it is the GL drivers from Apple for these GPU's that
is the issue.

Building for Linux
==================

At the time of this writing, most Linux distros use an older version
of Freetype. Astral will still build and run with an older version,
but color scalable glyphs will not be rendered with color then.
Below are instructions to use and build Astral with Freetype 2.10 which
include scalable color glyph support.

1. Build a newer version of Freetype
 - wget https://download.savannah.gnu.org/releases/freetype/freetype-2.10.0.tar.bz2
 - tar jxf freetype-2.10.0.tar.bz2
 - cd freetype-2.10.0
 - ./configure --prefix=/SOME/PATH
 - make
 - make install

2. Before building Astral, tell pkg-config to use the Freetype built in 1:
   `export PKG_CONFIG_PATH=/SOME/PATH/lib/pkgconfig`
3. Build Astral (via `make demos`)
4. When running the demos make sure that the Freetype libs
   built in 1 are used: `export LD_LIRBARY_PATH=/SOME/PATH/lib:.`


Building for MS-Windows
=======================

Currently, building via MinGW with the environment provided by
Msys2 (https://www.msys2.org/) is supported, but GLES is NOT
supported on MS-Windows.


Documentation
=============

Run `make docs` and open your browser to `docs/doxy/html/index.html`.
Effort is being made to document how to use the Astral and an
internal goal for Astral is that each public class, function,
macro, enum and namespace has a doxytag. The documentation also
includes example code. When getting started with Astral, make the
documentation and read the examples can be quite helpful.

To build the documentation requires doxygen which in turn
requires, graphviz and potentially mathjax. The latter,
depenending one one's system, may or may not need to be
explicitely installed.

One can also run `make code-docs` and open your browser to
`docs/doxy/code_docs/html/index.html` to make and view the
documentation of Astral together with a code browers. The
purpose of the code browser is to open the door for others to
start to work on Astral.


Running Demos
=============

The demos (naturally) link against the Astral libraries, thus they
need to be in the library path. For Linux, this is done by appending
the path where the libraries are located to LD_LIBRARY_PATH. A simple
quick hack is to do `export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH`. For
MacOS, the current working directory is already part of the library
search path which can be added to by `DYLD_LIBRARY_PATH`.
  
All demos have options which can be see by passing `--help` as the one
and only command line option to the demo.


Install and Including in Other Projects
=======================================

Make can be used to install Astral via `make INSTALL_LOCATION=/some/path install`
where the headers will be copied to `/some/path/include/` (under the subdirectory
`astral`), the library objects to `/some/path/lib/` and the pkg-config files to
`/some/path/lib/pkgconfig`. If no value for `INSTALL_LOCATION` is given, then it
defaults to `/usr/local/`. To use Astral via `pkg-config`, after installing
Astral, make sure that `/some/path/lib/pkgconfig` is in the search path of
`pkg-config`; this can done by appending `/some/path/lib/pkgconfig` to the
environmental variable `PKG_CONFIG_PATH`. From there, one uses `pkg-config` to
add the compile and link flags to one's projects:
- for release, `pkg-config astral-release --cflags` and `pkg-config astral-release --libs`
  provide the compiler flags and library flags respectively.
- for debug, `pkg-config astral-debug --cflags` and `pkg-config astral-debug --libs`
  provide the compiler flags and library flags respectively.

If one does not wish to use `pkg-config`, then one needs to add the following compile
flags:
- for release, compile flags to add are `-I$(INC) -std=c++11 $(shell pkg-config freetype2 --cflags)`
  and library flags to add are `-L$(LIB) -lAstral_release $(shell pkg-config freetype2 --libs)`
- for debug, compile flags to add are `-I$(INC) -std=c++11 $(shell pkg-config freetype2 --cflags) -DASTRAL_DEBUG -DASTRAL_GL_DEBUG`
  and library flags to add are `-L$(LIB) -lAstral_debug $(shell pkg-config freetype2 --libs)`

where `INC` is the location of the Astral headers, `LIB` is the directory of the
Astral libraries and `$(shell blah-blah-blah)` is what is printed at the console
running the command `blah-blah-blah`. If `make install` was executed, then
`LIB` is `/usr/local/lib` and `INC` is `/usr/local/include`. Note that
`-std=c++11` is not necessary if one already has flags present that give C++11
or greater. If using and IDE, configure the IDE to add the above flags to the
application build and to also add the correct Astral library; to use the debug
build of Astral, add `libAstral_debug.so`; to use the release build of Astral,
add `libAstral_release.so`. In addition, one needs to add the freetype library
as well; the shell command `$(shell pkg-config freetype2 --libs)` will give
the location of the freetype library.

NOTE: the release and debug builds of Astral are NOT binary compatible and
that binary incompatibility is embodied by the debug flags `-DASTRAL_DEBUG`
and `-DASTRAL_GL_DEBUG`.

Alternatively, one can make Astral built into one's project. The makefile has
the targets `PrintFlags`, `PrintCppSources`, `PrintGLSLSources`, and
`PrintNGLInfo` to assist importing Astral into another project. The file
list outputted by `PrintGLSLSources` is a list of GLSL shader source
files that need to be processed by `shell-scripts/create-resource-cpp-file.sh`
to create .cpp files from them. A file `some/path/foo.glsl.resource_string`
should be processed with `shell-scripts/create-resource-cpp-file.sh some/path/foo.glsl.resource_string foo.glsl.resource_string OUTPUTPATH`
where `OUTPUTPATH` is where the file `foo.glsl.resource_string.cpp` that
contains the file to add to the build. One will need to intelligently adjust
the compile and linker flags outputted from `PrintFlags` for ones targets.
I strongly suggest to use the current build system to generate the `astral_gl` files
and just add them to one's project, these are created by either of these commands:
- Native makefile: `make inc/astral/util/gl/astral_gl.hpp src/astral/util/gl/astral_gl.cpp`
- EMSDK makfile: `make -f Makefile.emscripten inc/astral/util/gl/astral_gl.hpp src/astral/util/gl/astral_gl.cpp`

Building for Emscripten
=======================

Astral can be built with the Emscripten tool chain using `Makefile.emscripten`,
i.e. `make -f Makefile.emscripten demos`. Tested to build and run against
1.38.34 and 2.0.1.

Notes:
 - The emsdk utility emrun on emsdk version 1.38.34 can be used to specify
   command line options for the demos
 - After 1.38.34, running the demos with emrun makes many demos stuck on
   "Downloading data .."; this is most likely an emscripten bug. Sighs.
 - Threading support sort-of-works in 2.0.1 but with caveats. Specifically,
   loading glyphs across multiple threads is much slower than loading from
   one thread. The likely cause is that threads in Emscripten are not
   real pre-emptive threads that get their own CPU core.
 - Loading a font completely in memory via FreetypeFace::GeneratorMemory
   using a filename although works without issue on native, fails creatively
   in WASM. It does report the correct number of glyphs, but glyph loading
   is not reliable, threaded or not. However, if a font file is realized
   as a static resource (using `shell_scripts/create-resource-cpp-file.sh`)
   and FreetypeFace::GeneratorMemory is used, then font loading works fine.
   The likely cause is an Emscripten bug where one attempts to dump an
   entire file packed via Emscripten into memory at once.
 - Safari has preliminary support for WebGL2 (the tech preview as of December
   2020 has it enabled by default). However, Safari's WebGL2 has different bugs
   depending on the version. At the time of this writing (Octobor 2022), the
   following work arounds are needed.
   - Safari has serious bugs with respect to dynamically indexing into a UBO.
     Adding "use_texture_for_uniform_buffer true" to the
     command line when running via emrun will make Astral work on Safari. If running
     Astral in a wider web-application set `RenderEngineGL3::Config::m_use_texture_for_uniform_buffer`
     to `true` when creating a `RenderEngineGL3` if one detects running on Safari.
     In addition, the setting `WebGL via Metal` found in `Experimental Features` of
     Safari's `Develop` menu must also be **_enabled_**; the translation from WebGL2 to
     OpenGL has bugs that prevent Astral from working.
   - Safari has a serious performance issues with indexless rendering; to avoid that
     issue set `RenderEngineGL3::Config::m_use_indices` to `true`.
 - Some versions of Chrome and Edge have had (or do have issues) with ubder-shading,
   to avoid these issues set `RenderEngine::OverridableProperties::m_uber_shader_method`
   to `astral::uber_shader_none` and call `Renderer::overridable_properties()` appropriately

For inclusion in a project with emscripten, look to the
  file `Makefile.emscripten`. The reader's digest version is
  1. Build `libAstral_debug.bc` and `libAstral_release.bc` with Makefile.emscripten
  2. When using Astral in DEBUG use the flags: `-s USE_WEBGL2=1 -s USE_FREETYPE=1 -s ALLOW_MEMORY_GROWTH=1 -std=c++11 -I$(INC) -DASTRAL_DEBUG -DASTRAL_GL_DEBUG`
  3. When using Astral in RELEASE use the flags: `-s USE_WEBGL2=1 -s USE_FREETYPE=1 -s ALLOW_MEMORY_GROWTH=1 -std=c++11 -I$(INC)`
