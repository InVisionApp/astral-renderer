# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_gaussian_blur.vert.glsl.resource_string \
	astral_gaussian_blur.frag.glsl.resource_string \
	astral_blit_mask_tile.vert.glsl.resource_string \
	astral_blit_mask_tile.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
