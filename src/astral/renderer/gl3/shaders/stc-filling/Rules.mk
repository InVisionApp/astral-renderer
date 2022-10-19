# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_stc_quad_tri_util.glsl.resource_string \
	astral_stc_quad_tri_fuzz.vert.glsl.resource_string \
	astral_stc_quad_tri_fuzz.frag.glsl.resource_string \
	astral_stc_line_fuzz.vert.glsl.resource_string \
	astral_stc_line_fuzz.frag.glsl.resource_string \
	astral_stc_line.vert.glsl.resource_string \
	astral_stc_line.frag.glsl.resource_string \
	astral_stc_quad_tri.vert.glsl.resource_string \
	astral_stc_quad_tri.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
