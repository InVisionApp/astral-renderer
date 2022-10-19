# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_DEMOS+=path_test
path_test_SOURCES:=$(call filelist, main.cpp graph_stroke.cpp wavy_stroke.cpp custom_stroke_shader_generator.cpp)
path_test_SHADERS:=$(call filelist, spacing.glsl.resource_string)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
