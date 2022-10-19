# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_stroke_common.glsl.resource_string \
	astral_stroke_common_join.glsl.resource_string \
	astral_stroke_common_cap.glsl.resource_string \
	astral_stroke_capper_util.glsl.resource_string \
	astral_stroke_biarc_util.glsl.resource_string \
	astral_chain_stroke_line.vert.glsl.resource_string \
	astral_chain_stroke_line.frag.glsl.resource_string \
	astral_chain_stroke_biarc.vert.glsl.resource_string \
	astral_chain_stroke_biarc.frag.glsl.resource_string \
	astral_chain_stroke_line_biarc_common.frag.glsl.resource_string \
	astral_chain_stroke_rounded_join.vert.glsl.resource_string \
	astral_chain_stroke_rounded.frag.glsl.resource_string \
	astral_chain_stroke_join.vert.glsl.resource_string \
	astral_chain_stroke_cap.vert.glsl.resource_string \
	astral_chain_stroke_bevel_join.vert.glsl.resource_string \
	astral_chain_stroke_miter_join.vert.glsl.resource_string \
	astral_chain_stroke_bevel_miter_join.frag.glsl.resource_string \
	astral_chain_stroke_rounded_cap.vert.glsl.resource_string \
	astral_chain_stroke_square_cap.vert.glsl.resource_string \
	astral_chain_stroke_square_cap.frag.glsl.resource_string \
	astral_chain_stroke_capper.vert.glsl.resource_string \
	astral_chain_stroke_capper.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
