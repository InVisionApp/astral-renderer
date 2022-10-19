# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/common
include $(dir)/Rules.mk

dir := $(d)/example_path
include $(dir)/Rules.mk

dir := $(d)/example_brush
include $(dir)/Rules.mk

dir := $(d)/example_custom_brush
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
