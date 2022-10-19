################
## Detect OS
UNAME := $(shell uname -s)
UNAMER := $(shell uname -r)

DEFAULT_CXX ?= $(CXX)
DEFAULT_CC ?= $(CC)
DEFAULT_HOST_CXX ?= $(CXX)
DEFAULT_HOST_CC = $(CC)

BUILD ?= build

GLSL_DOCS ?= 1
ENVIRONMENTALDESCRIPTIONS += "GLSL_DOCS: if 1, then when making docoumentation via doxygen, include GLSL shader docs (default 1)"

# must be set before for make to gets its rules correct
ASTRAL_GL_HPP=inc/astral/util/gl/astral_gl.hpp
ASTRAL_GL_CPP=src/astral/util/gl/astral_gl.cpp

MINGW_BUILD = 0
ifeq ($(findstring MINGW,$(UNAME)),MINGW)
  MINGW_BUILD = 1
  ifeq ($(findstring 1.0, $(UNAMER)), 1.0)
	MINGW_MODE = MINGW
  else ifeq ($(findstring MINGW64,$(UNAME)),MINGW64)
	MINGW_MODE = MINGW64
  else ifeq ($(findstring MINGW32,$(UNAME)),MINGW32)
	MINGW_MODE = MINGW32
  endif
endif

DARWIN_BUILD = 0
ifeq ($(UNAME),Darwin)
  DARWIN_BUILD = 1
endif

TARGET_CXX ?= $(DEFAULT_CXX)
ENVIRONMENTALDESCRIPTIONS += "TARGET_CXX: Target C++ compiler for demos and library (default $(DEFAULT_CXX))"

TARGET_CC ?= $(DEFAULT_CC)
ENVIRONMENTALDESCRIPTIONS += "TARGET_CC: Target C compiler for demos and library (default $(DEFAULT_CC))"

HOST_CXX ?= $(DEFAULT_HOST_CXX)
ENVIRONMENTALDESCRIPTIONS += "HOST_CXX: Host C++ compiler for building host executables (default $(DEFAULT_HOST_CXX))"

HOST_CC ?= $(DEFAULT_HOST_CC)
ENVIRONMENTALDESCRIPTIONS += "HOST_CC: Host C compiler for building host executables (default $(DEFAULT_HOST_CC))"

#install location
INSTALL_LOCATION ?= /usr/local
ENVIRONMENTALDESCRIPTIONS += "INSTALL_LOCATION: provides install location for make install (default /usr/local)"

#####################################
## use Clang's address sanitizer
##
## Set the default to 1 if we detect clang++ for the C++ compiler
CLANG_GREP_RESULT := ${shell $(CXX) --version | grep 'clang'}
ifeq ($(CLANG_GREP_RESULT),)
    ASTRAL_DEFAULT_USE_SANTIZER_IN_DEBUG = 0
else
    ASTRAL_DEFAULT_USE_SANTIZER_IN_DEBUG = 1
endif

ASTRAL_USE_SANTIZER_IN_DEBUG ?= $(ASTRAL_DEFAULT_USE_SANTIZER_IN_DEBUG)
ENVIRONMENTALDESCRIPTIONS += "ASTRAL_USE_SANTIZER_IN_DEBUG: if 1, then use address santizer in debug builds, requires clang as compiler (default $(ASTRAL_USE_SANTIZER_IN_DEBUG))"

ifeq ($(MINGW_BUILD),1)
  fPIC =
else
  fPIC = -fPIC
endif

ifeq ($(DARWIN_BUILD),0)
  SONAME = -soname
  SED_INPLACE_REPLACE=sed -i
else
  SONAME = -install_name
  SED_INPLACE_REPLACE=sed -i ''
endif
