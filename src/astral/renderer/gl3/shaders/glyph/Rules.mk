# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_glyph.vert.glsl.resource_string \
	astral_glyph.frag.glsl.resource_string \
	astral_image_glyph.vert.glsl.resource_string \
	astral_image_glyph.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
