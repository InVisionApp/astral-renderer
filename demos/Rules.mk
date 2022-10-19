# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/common
include $(dir)/Rules.mk

dir := $(d)/tests
include $(dir)/Rules.mk

dir := $(d)/atlas-tests
include $(dir)/Rules.mk

dir := $(d)/backend-tests
include $(dir)/Rules.mk

dir := $(d)/test_animated_path
include $(dir)/Rules.mk

dir := $(d)/animated_text
include $(dir)/Rules.mk

dir := $(d)/brush_test
include $(dir)/Rules.mk

dir := $(d)/path_test
include $(dir)/Rules.mk

dir := $(d)/glyph_test
include $(dir)/Rules.mk

dir := $(d)/light_test
include $(dir)/Rules.mk

dir := $(d)/clip_combine_test
include $(dir)/Rules.mk

dir := $(d)/clip_blit_test
include $(dir)/Rules.mk

dir := $(d)/rect_test
include $(dir)/Rules.mk

dir := $(d)/item_path_test
include $(dir)/Rules.mk

dir := $(d)/svg
include $(dir)/Rules.mk

dir := $(d)/rect_intersection_test
include $(dir)/Rules.mk

dir := $(d)/snapshot_test
include $(dir)/Rules.mk

dir := $(d)/intersection_test
include $(dir)/Rules.mk

dir := $(d)/create_animated_path
include $(dir)/Rules.mk

dir := $(d)/effect_collection_test
include $(dir)/Rules.mk

dir := $(d)/tutorial
include $(dir)/Rules.mk

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
