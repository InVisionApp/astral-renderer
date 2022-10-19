# Begin standard header
sp 		:= $(sp).x
dirstack_$(sp)	:= $(d)
d		:= $(dir)
# End standard header

ASTRAL_SOURCES += $(call filelist, freetype_lib.cpp \
	freetype_face.cpp font.cpp \
	typeface.cpp freetype_face_generator.cpp \
	text_item.cpp glyph_generator.cpp)

# Begin standard footer
d		:= $(dirstack_$(sp))
sp		:= $(basename $(sp))
# End standard footer
