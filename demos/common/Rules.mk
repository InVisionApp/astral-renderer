# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

COMMON_DEMO_SOURCES += $(call filelist, generic_command_line.cpp \
	sdl_demo.cpp PanZoomTracker.cpp ImageLoader.cpp ImageSaver.cpp \
	ImageCompare.cpp cycle_value.cpp stream_holder.cpp text_helper.cpp \
	render_engine_gl3_options.cpp read_colorstops.cpp \
	read_path.cpp read_dash_pattern.cpp generic_hierarchy.cpp \
	load_svg.cpp nanosvg.cpp animated_path_reflect.cpp render_engine_gl3_demo.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
