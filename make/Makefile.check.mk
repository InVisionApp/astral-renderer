ASTRAL_CANNOT_BUILD := NO
DEMOS_CANNOT_BUILD := NO

pkg_config_path := $(shell which pkg-config)
ifeq (, $(pkg_config_path))
CHECK_PKG_CONFIG := "Cannot find pkg-config tool in path"
ASTRAL_CANNOT_BUILD := YES
else
CHECK_PKG_CONFIG := "Found pkg-config tool in path at $(pkg_config_path)"
endif

CHECK_FREETYPE_CFLAGS_CODE := $(shell pkg-config freetype2 --cflags 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_FREETYPE_CFLAGS_CODE), 0)
CHECK_FREETYPE_CFLAGS := "Found cflags for freetype2: $(shell pkg-config freetype2 --cflags)"
else
CHECK_FREETYPE_CFLAGS := "Dependency missing: freetype2 headers"
ASTRAL_CANNOT_BUILD := YES
endif

CHECK_FREETYPE_LIBS_CODE := $(shell pkg-config freetype2 --libs 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_FREETYPE_LIBS_CODE), 0)
CHECK_FREETYPE_LIBS := "Found libs for freetype2: $(shell pkg-config freetype2 --libs)"
else
CHECK_FREETYPE_LIBS := "Dependency missing: freetype2 libs"
ASTRAL_CANNOT_BUILD := YES
endif

CHECK_SDL_CFLAGS_CODE := $(shell pkg-config SDL2_image --cflags 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_SDL_CFLAGS_CODE), 0)
CHECK_SDL_CFLAGS := "Found cflags for SDL2_image: $(shell pkg-config SDL2_image --cflags)"
else
CHECK_SDL_CFLAGS := "Cannot build demos: Unable to find SDL2/SDL_2_image headers"
DEMOS_CANNOT_BUILD := YES
endif

CHECK_SDL_LIBS_CODE := $(shell pkg-config SDL2_image --libs 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_SDL_LIBS_CODE), 0)
CHECK_SDL_LIBS := "Found libs for SDL2_image: $(shell pkg-config SDL2_image --libs)"
else
CHECK_SDL_LIBS := "Cannot build demos: Unable to find SDL2/SDL2_image libs"
DEMOS_CANNOT_BUILD := YES
endif

CHECK_PNG_CFLAGS_CODE := $(shell pkg-config libpng --cflags 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_PNG_CFLAGS_CODE), 0)
CHECK_PNG_CFLAGS := "Found cflags for libpng: $(shell pkg-config libpng --cflags)"
else
CHECK_PNG_CFLAGS := "Cannot build demos: Unable to find libpng headers"
DEMOS_CANNOT_BUILD := YES
endif

CHECK_PNG_LIBS_CODE := $(shell pkg-config libpng --libs 1> /dev/null 2> /dev/null; $(ECHO) $$?)
ifeq ($(CHECK_PNG_LIBS_CODE), 0)
CHECK_PNG_LIBS := "Found libs for libpng: $(shell pkg-config libpng --libs)"
else
CHECK_PNG_LIBS := "Cannot build demos: Unable to find libpng libs"
DEMOS_CANNOT_BUILD := YES
endif

ifneq ($(ASTRAL_CANNOT_BUILD),YES)
ASTRAL_CAN_BUILD:=YES
endif

ifneq ($(DEMOS_CANNOT_BUILD),YES)
DEMOS_CAN_BUILD:=YES
endif

$(BUILD)/check_astral_dependencies:
	@[ "${ASTRAL_CAN_BUILD}" ] || ( echo ">> Astral library build dependencies not met, issue make check for details"; exit 1 )
	@$(ECHO) "$(ASTRAL_CAN_BUILD)" > $@

$(BUILD)/check_demo_dependencies: $(BUILD)/check_astral_dependencies
	@[ "${DEMOS_CAN_BUILD}" ] || ( echo ">> Demos build dependencies not met, issue make check for details"; exit 1 )
	@$(ECHO) "$(DEMOS_CAN_BUILD)" > $@

check:
	@$(ECHO) "$(CHECK_PKG_CONFIG)"
	@$(ECHO) "$(CHECK_LEX)"
	@$(ECHO) "$(CHECK_FREETYPE_CFLAGS)"
	@$(ECHO) "$(CHECK_FREETYPE_LIBS)"
	@$(ECHO) "$(CHECK_SDL_CFLAGS)"
	@$(ECHO) "$(CHECK_SDL_LIBS)"
	@$(ECHO) "$(CHECK_PNG_CFLAGS)"
	@$(ECHO) "$(CHECK_PNG_LIBS)"

.PHONY: check check_astral_dependencies check_demo_dependencies
