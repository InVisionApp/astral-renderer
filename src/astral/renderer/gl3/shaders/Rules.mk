# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/blending
include $(dir)/Rules.mk

dir := $(d)/brush
include $(dir)/Rules.mk

dir := $(d)/material
include $(dir)/Rules.mk

dir := $(d)/glyph
include $(dir)/Rules.mk

dir := $(d)/item-shaders
include $(dir)/Rules.mk

dir := $(d)/lighting
include $(dir)/Rules.mk

dir := $(d)/renderer-support
include $(dir)/Rules.mk

dir := $(d)/stc-filling
include $(dir)/Rules.mk

dir := $(d)/stroking
include $(dir)/Rules.mk

dir := $(d)/util
include $(dir)/Rules.mk

ASTRAL_SHADERS += $(call filelist, \
	astral_gles_precisions.glsl.resource_string \
	astral_types_bo.glsl.resource_string \
	astral_uniforms_ubo_typeless.glsl.resource_string \
	astral_uniforms_common.glsl.resource_string \
	astral_compute_shadow_map_depth.glsl.resource_string \
	astral_main_clip_window.vert.glsl.resource_string \
	astral_main_packing_bo.glsl.resource_string \
	astral_main_bo.vert.glsl.resource_string \
	astral_main_bo.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
