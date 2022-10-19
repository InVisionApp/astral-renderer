Unified GL Header Generator
===========================

The Unfied GL Header Generator produces a header and implementation file for select types, enums and functions
from GL and GLES which are prefixed global and defines or placed in the astral::gl_bingind namespace.

Usage
=====

```
Usage: hgen <SOURCE.XML> <TARGET.H> [<TARGET.CPP>] [-templates <TEMPLATE_DIR>] [-list-native <FILE1.TXT>] [-list-wasm <FILE2.TXT>]
       <SOURCE.XML>      Specifies the XML describing all GL and GLES API.
       <TARGET.H>        Specifies header to write.
       <TARGET.CPP>      Specifies implementation to write.
       <TEMPLATE_DIR>    Specifies the location of template files which are used as the start and end of each generated file.
       -list-native      Outputs selected Native entities to <FILE1.TXT>.
       -list-wasm        Outputs selected WASM entities to <FILE2.TXT>.
```

The first input is always the XML file from Khronos group saved locally as `<SOURCE.XML>`.

The second input is always required and is the optionally qualifed path and file name of the header to generate.
Absolute and relative paths are supported.

The third argument os the optionally qualifed path and file name of the implementation file to produce. When it
is not specified, the `<TARGET.H>` path is used with the extension changed from `.h` or `.hpp` to `.cpp`.

Additional options are supported:
- The `-template` option specifies the directory from which templates to generate output files are used. This is
  by default `ngl_generator/templates` relative to the execution path. Setting the options makes it possible to
  run the generator from any directory. Generated files start with the contents of `pre.h` (for the header) or
  `pre.cpp` (for the implementation), the end with `post.h` and `post.cpp`, respectively.
- The `-list-native` and `-list-wasm` options specify text files that list the converted entities, independently
  for native and wasm support.

More complex configuration can be changed in the `main.cpp` file near the top. Each option is documented via
comments. Those controls the exact generated output naming, matching and formatting.

Structure
=========

The `main()` and entire logic for the generator is in `main.cpp`. It complies with C++11 and uses standard C++
headers plus the header-only variant of pugixml, included in the same directory. Only `main.cpp` needs to be
compiled and `pugixml.cpp` is unused but it part of the `pugixml` package.

There is a subfolder `templates` that contains files used as input to the header generator. Those are not
complete C++ files and must not be compiled. They are injected as part of the ouput header and implementation
files.

The documentation and license for pugixml is includes as `PUGIXML_README.md` and `PUGIXML_LICENSE.md`.

Building
========

Building the header generator tool:

`make build/hgen`

To generate the header and implementation files, use any of:

`make inc/astral/util/gl/astral_gl.hpp`
`make src/astral/util/gl/astral_gl.cpp`
`make -f Makefile.emscripten inc/astral/util/gl/astral_gl.hpp`
`make -f Makefile.emscripten src/astral/util/gl/astral_gl.cpp`

Testing
=======

To validate the generated Native implementation, build release or debug with:

`make build/release/src/astral/util/gl/astral_gl.cpp.o`
`make build/debug/src/astral/util/gl/astral_gl.cpp.o`

To validate the generated Native header, use any target that includes the header:

`make build/release/src/astral/renderer/gl3/render_target_gl3.cpp.o`
`make build/debug/src/astral/renderer/gl3/render_target_gl3.cpp.o`

To validate generated WASM implementation, build for release or debug with:

`make -f Makefile.emscripten wasm/build/release/src/astral/util/gl/astral_gl.bc`
`make -f Makefile.emscripten wasm/build/debug/src/astral/util/gl/astral_gl.bc`

To validate the generated WASM header, use any target that includes the header:

`make -f Makefile.emscripten wasm/build/release/src/astral/renderer/gl3/render_target_gl3.bc`
`make -f Makefile.emscripten wasm/build/debug/src/astral/renderer/gl3/render_target_gl3.bc`

Test debug functions using any Astral demos target build in debug mode. Such target ebds with `-debug`.
Run the selected debug demo with the `log_gl stdout` option passed as command line argument.
