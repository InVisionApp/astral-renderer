ECHO := $(shell which echo)
STRING_RESOURCE_CC = shell-scripts/create-resource-cpp-file.sh
filelist = $(foreach filename,$(1),$(d)/$(filename))

default: targets
all: demos

##########################################
# Collect the list of shaders and sources
TARGETLIST:= all
ENVIRONMENTALDESCRIPTIONS:=
ASTRAL_SOURCES:=
ASTRAL_SHADERS:=
ASTRAL_DEMOS:=
COMMON_DEMO_SOURCES:=
COMMON_DEMO_SHADERS:=

include make/Makefile.settings.mk

dir := src/astral
include $(dir)/Rules.mk

dir := demos
include $(dir)/Rules.mk

##################################################
# To make it easier to build Astral in other build
# systems that might be doing cross build, we do NOT
# include the Astral GL generated source file when listing
# the soruces in PrintCppSources.
ASTRAL_SOURCES_WITHOUT_ASTRAL_GL := $(ASTRAL_SOURCES)

targets:
	@echo "Info:"
	@echo "=============================="
	@echo "PrintCppSources: list C++ sources used to build Astral library"
	@echo "PrintGLSLSources: list GLSL sources used to build Astral library"
	@echo "PrintAstralGLInfo: list AstralGL headers and sources for different targets"
	@echo "PrintFlags: print comiler and liker flags to build Astral library"
	@echo
	@echo "Individual Demos available:"
	@echo "=============================="
	@printf "%s\n" $(ASTRAL_DEMOS)
	@printf "%s\n" $(DEMO_TARGETLIST)
	@echo
	@echo "Targets available:"
	@echo "=============================="
	@printf "%s\n" $(TARGETLIST)
	@echo
	@echo "Environmental variables:"
	@echo "=============================="
	@printf "%s\n" $(ENVIRONMENTALDESCRIPTIONS)

PrintCppSources:
	@echo "$(ASTRAL_SOURCES_WITHOUT_ASTRAL_GL)"

PrintGLSLSources:
	@echo "$(ASTRAL_SHADERS)"

PrintAstralGLInfo:
	@echo "Generated files for Astral GL:"
	@echo "\tSource: src/astral/util/gl/astral_gl.cpp"
	@echo "\tHeader: inc/astral/util/gl/astral_gl.hpp"

##################################
## Specifies how to build the files
## using only ASTRAL_SOURCES, ASTRAL_DEMOS
## and the magic for each demo.
include make/Makefile.rules.mk
include make/Makefile.demos.rules.mk
include make/Makefile.docs.mk
include make/Makefile.check.mk
include make/Makefile.install.mk
