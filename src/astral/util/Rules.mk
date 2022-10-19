# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/astral_gl_generator
include $(dir)/Rules.mk

dir := $(d)/gl
include $(dir)/Rules.mk

ASTRAL_SOURCES += $(call filelist, static_resource.cpp \
	astral_memory.cpp util.cpp memory_pool.cpp \
        data_buffer.cpp api_callback.cpp matrix.cpp \
	clip_util.cpp interval_allocator.cpp \
	layered_rect_atlas.cpp tile_allocator.cpp \
	transformed_bounding_box.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
