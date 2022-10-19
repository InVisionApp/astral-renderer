# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_banded_rays_common.glsl.resource_string \
	astral_banded_rays_neighbor_pixel.glsl.resource_string \
	astral_banded_rays_nearest_curve.glsl.resource_string \
	astral_utils.glsl.resource_string \
	astral_unpackHalf2x16.glsl.resource_string \
	astral_unpack.glsl.resource_string \
	astral_image_util.glsl.resource_string \
	astral_image.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
