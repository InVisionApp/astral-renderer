# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SHADERS += $(call filelist, \
	astral_gpu_vertex_streaming_blitter.vert.glsl.resource_string \
	astral_gpu_vertex_streaming_blitter.frag.glsl.resource_string \
	astral_image_atlas_blitter.vert.glsl.resource_string \
	astral_image_atlas_blitter.frag.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
