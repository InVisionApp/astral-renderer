# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/solver
include $(dir)/Rules.mk

dir := $(d)/contour_tessellation
include $(dir)/Rules.mk

dir := $(d)/region_allocators
include $(dir)/Rules.mk

dir := $(d)/interval_allocator
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
