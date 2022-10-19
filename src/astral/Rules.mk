# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

dir := $(d)/util
include $(dir)/Rules.mk

dir := $(d)/text
include $(dir)/Rules.mk

dir := $(d)/renderer
include $(dir)/Rules.mk

ASTRAL_SOURCES += $(call filelist, animated_path.cpp path.cpp \
	contour.cpp contour_approximator.cpp contour_curve.cpp \
	contour_curve_util.cpp animated_contour_util.cpp \
	animated_contour.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
