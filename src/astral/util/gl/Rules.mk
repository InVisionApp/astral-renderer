# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SOURCES += $(call filelist, gl_shader_source.cpp \
	gl_uniform_implement.cpp gl_context_properties.cpp \
	gl_binding.cpp gl_get.cpp gl_program.cpp \
	unpack_source_generator.cpp \
	gl_shader_symbol_list.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
