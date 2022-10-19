# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_blending_fixed_funciton.glsl.resource_string\
	astral_blending_clear.glsl.resource_string \
	astral_blending_src.glsl.resource_string \
	astral_blending_src_in.glsl.resource_string \
	astral_blending_src_out.glsl.resource_string \
	astral_blending_dst_in.glsl.resource_string \
	astral_blending_dst_atop.glsl.resource_string \
	astral_blending_modulate.glsl.resource_string \
	astral_blending_max.glsl.resource_string \
	astral_blending_min.glsl.resource_string \
	astral_blending_difference.glsl.resource_string \
	astral_blending_multiply.glsl.resource_string \
	astral_blending_overlay.glsl.resource_string \
	astral_blending_darken.glsl.resource_string \
	astral_blending_lighten.glsl.resource_string \
	astral_blending_color_dodge.glsl.resource_string \
	astral_blending_color_burn.glsl.resource_string \
	astral_blending_hardlight.glsl.resource_string \
	astral_blending_softlight.glsl.resource_string \
	astral_blending_exclusion.glsl.resource_string \
	astral_blending_hue.glsl.resource_string \
	astral_blending_saturation.glsl.resource_string \
	astral_blending_color.glsl.resource_string \
	astral_blending_luminosity.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
