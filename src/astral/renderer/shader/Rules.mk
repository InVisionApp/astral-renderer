# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SOURCES += $(call filelist, \
	fill_stc_shader.cpp \
	stroke_shader.cpp \
	stroke_shader_item_data_packer.cpp \
	stroke_query.cpp \
	stroke_data_hierarchy.cpp \
	item_shader.cpp material_shader.cpp \
	blit_mask_tile_shader.cpp \
	dynamic_rect_shader.cpp \
	glyph_shader.cpp \
	masked_rect_shader.cpp \
	light_material_shader.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
