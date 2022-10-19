# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/backend
include $(dir)/Rules.mk

dir := $(d)/effect
include $(dir)/Rules.mk

dir := $(d)/shader
include $(dir)/Rules.mk

ASTRAL_SOURCES += $(call filelist, \
	render_engine.cpp \
	render_enums.cpp \
	render_clip_node.cpp \
	render_encoder.cpp \
	render_encoder_layer.cpp \
	renderer.cpp \
	renderer_workroom.cpp \
	renderer_mask_drawer.cpp \
	renderer_draw_command.cpp \
	renderer_clip_element.cpp \
	renderer_clip_geometry.cpp \
	renderer_cached_transformation.cpp \
	renderer_virtual_buffer.cpp \
	renderer_filler.cpp \
	renderer_filler_common_clipper.cpp \
	renderer_filler_line_clipping.cpp \
	renderer_filler_curve_clipping.cpp \
	renderer_stc_data.cpp \
	renderer_uber_shading_key_collection.cpp \
	renderer_stroke_builder.cpp \
	renderer_tile_hit_detection.cpp \
	combined_path.cpp \
	mask_details.cpp \
	colorstop_sequence.cpp \
	vertex_data.cpp \
	static_data.cpp \
	item_path.cpp \
	image.cpp \
	shadow_map.cpp \
	stroke_parameters.cpp \
	mipmap_level.cpp \
	painter.cpp)

dir := $(d)/gl3
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
