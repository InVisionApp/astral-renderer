# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

HGEN_TEMPLATE_DIRECTORY:=$(d)/templates
HGEN_GL_XML:=$(d)/gl.xml

HGEN_TEMPLATE_FILES=$(call filelist, templates/head.cpp templates/post.cpp templates/post.hpp templates/pre.cpp templates/pre.hpp)
HGEN_SRC:=$(call filelist, main.cpp)
HGEN_DEPENDENCIES:=$(call filelist, pugiconfig.hpp pugixml.cpp pugixml.hpp)

$(BUILD)/hgen: $(HGEN_SRC) $(HGEN_DEPENDENCIES)
	@mkdir -p $(dir $@)
	$(HOST_CXX) --std=c++11 -Wall $(HGEN_SRC) -o $@

$(ASTRAL_GL_CPP) $(ASTRAL_GL_HPP) &: $(BUILD)/hgen inc/astral/util/gl/astral_gl_platform.h $(HGEN_TEMPLATE_FILES)
	@mkdir -p $(dir $@)
	$(BUILD)/hgen $(HGEN_GL_XML) $(ASTRAL_GL_HPP) $(ASTRAL_GL_CPP) -templates $(HGEN_TEMPLATE_DIRECTORY)

ASTRAL_SOURCES += $(ASTRAL_GL_CPP)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
