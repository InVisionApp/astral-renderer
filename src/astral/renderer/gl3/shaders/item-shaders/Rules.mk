# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_rect_shader.vert.glsl.resource_string \
	astral_rect_shader.frag.glsl.resource_string \
	astral_item_path_common.vert.glsl.resource_string \
	astral_color_item_path.vert.glsl.resource_string \
	astral_color_item_path.frag.glsl.resource_string \
	astral_mask_item_path.vert.glsl.resource_string \
	astral_mask_item_path.frag.glsl.resource_string \
	astral_cover_rect.vert.glsl.resource_string \
	astral_cover_rect.frag.glsl.resource_string \
	astral_combine_clip.vert.glsl.resource_string \
	astral_combine_clip.frag.glsl.resource_string \
	astral_color_item_shader_from_mask_shader.vert.glsl.resource_string \
	astral_color_item_shader_from_mask_shader.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
