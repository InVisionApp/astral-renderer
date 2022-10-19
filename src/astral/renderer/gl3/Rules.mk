# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/shaders
include $(dir)/Rules.mk

ASTRAL_SOURCES += $(call filelist, render_target_gl3.cpp \
	render_engine_gl_util.cpp \
	item_shader_gl3.cpp \
	material_shader_gl3.cpp \
	stroke_shader_gl3.cpp \
	shader_implement_gl3.cpp \
	render_engine_gl3.cpp \
	render_engine_gl3_packing.cpp \
	render_engine_gl3_shader_builder.cpp \
	render_engine_gl3_blend_builder.cpp \
	render_engine_gl3_backend.cpp \
	render_engine_gl3_static_data.cpp \
	render_engine_gl3_colorstop.cpp \
	render_engine_gl3_image.cpp \
	render_engine_gl3_shadow_map.cpp \
	render_engine_gl3_atlas_blitter.cpp \
	render_engine_gl3_fbo_blitter.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
