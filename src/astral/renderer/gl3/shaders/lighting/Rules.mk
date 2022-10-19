# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_light_util.glsl.resource_string \
	astral_shadow_util.glsl.resource_string \
	astral_light_material.frag.glsl.resource_string \
	astral_light_material.vert.glsl.resource_string \
	astral_edge_shadow.frag.glsl.resource_string \
	astral_edge_shadow.vert.glsl.resource_string \
	astral_conic_shadow.frag.glsl.resource_string \
	astral_conic_shadow.vert.glsl.resource_string \
	astral_clear_shadow.frag.glsl.resource_string \
	astral_clear_shadow.vert.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
