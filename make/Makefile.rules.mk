##############################################
## Give the rule to convert shaders into .cpp
$(BUILD)/string_resources_cpp/%.resource_string.cpp: %.resource_string $(STRING_RESOURCE_CC)
	@mkdir -p $(dir $@)
	$(STRING_RESOURCE_CC) $< $(notdir $<) $(dir $@)


##########################################
# Compile flags
#  - BASE_COMPILE_DEBUG/RELEASE_FLAGS flags required for using the library
#  - BUILD_COMPILE_DEBUG/RELEASE_FLAGS flags required for building the library
BASE_COMPILE_FLAGS = -std=c++11

BASE_COMPILE_DEBUG_FLAGS = $(BASE_COMPILE_FLAGS) -DASTRAL_DEBUG -DASTRAL_GL_DEBUG -D_GLIBCXX_ASSERTIONS
BASE_COMPILE_RELEASE_FLAGS = $(BASE_COMPILE_FLAGS)

ifeq ($(ASTRAL_USE_SANTIZER_IN_DEBUG), 1)
 BASE_COMPILE_DEBUG_FLAGS += -fsanitize=address
endif

BUILD_COMPILE_COMMON_FLAGS := -Iinc $(shell pkg-config freetype2 --cflags) -Wall -Wextra -Wdouble-promotion -Wunused $(fPIC)
BUILD_COMPILE_DEBUG_FLAGS = $(BUILD_COMPILE_COMMON_FLAGS) $(BASE_COMPILE_DEBUG_FLAGS) -g
BUILD_COMPILE_RELEASE_FLAGS = $(BUILD_COMPILE_COMMON_FLAGS) $(BASE_COMPILE_RELEASE_FLAGS) -O3

#############################################
# Linker flags for libAstral
BASE_LINKER_FLAGS =
BASE_LINKER_DEBUG_FLAGS = $(BASE_LINKER_FLAGS)
BASE_LINKER_RELEASE_FLAGS = $(BASE_LINKER_FLAGS)

ifeq ($(ASTRAL_USE_SANTIZER_IN_DEBUG), 1)
 BASE_LINKER_DEBUG_FLAGS += -fsanitize=address
endif

BUILD_LINKER_COMMON_FLAGS := $(shell pkg-config freetype2 --libs)
BUILD_LINKER_DEBUG_FLAGS = $(BUILD_LINKER_COMMON_FLAGS)
BUILD_LINKER_RELEASE_FLAGS = $(BUILD_LINKER_COMMON_FLAGS)

################################################
## Target to print the compiler flags
PrintFlags:
	@$(ECHO) ""
	@$(ECHO) "------- Compiler/Linker flags not including dependencies or extra warnings ------"
	@$(ECHO) "BASE_COMPILE_DEBUG_FLAGS: $(BASE_COMPILE_DEBUG_FLAGS)"
	@$(ECHO) "BASE_COMPILE_RELEASE_FLAGS: $(BASE_COMPILE_RELEASE_FLAGS)"
	@$(ECHO) "BASE_LINKER_DEBUG_FLAGS: $(BASE_LINKER_DEBUG_FLAGS)"
	@$(ECHO) "BASE_LINKER_RELEASE_FLAGS: $(BASE_LINKER_RELEASE_FLAGS)"
	@$(ECHO) ""
	@$(ECHO) "------- Compiler/Linker flags including dependencies and extra warnings ------"
	@$(ECHO) "BUILD_COMPILE_RELEASE_FLAGS: $(BUILD_COMPILE_RELEASE_FLAGS)"
	@$(ECHO) "BUILD_COMPILE_DEBUG_FLAGS: $(BUILD_COMPILE_DEBUG_FLAGS)"
	@$(ECHO) "BUILD_LINKER_DEBUG_FLAGS: $(BUILD_LINKER_DEBUG_FLAGS)"
	@$(ECHO) "BUILD_LINKER_RELEASE_FLAGS: $(BUILD_LINKER_RELEASE_FLAGS)"
.PHONY: PrintFlags


#######################################################
## Debug library recipes
$(BUILD)/debug/%.resource_string.cpp.o: $(BUILD)/string_resources_cpp/%.resource_string.cpp
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(BUILD_COMPILE_DEBUG_FLAGS) -c $< -o $@

$(BUILD)/debug/%.cpp.o: %.cpp $(BUILD)/debug/%.cpp.d  $(ASTRAL_GL_HPP) $(BUILD)/check_astral_dependencies
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(BUILD_COMPILE_DEBUG_FLAGS) -MT $@ -MMD -MP -MF $(BUILD)/debug/$*.cpp.d -c $< -o $@
$(BUILD)/debug/%.cpp.d: ;
.PRECIOUS: $(BUILD)/debug/%.cpp.d

##############################################################
## Release library recips
$(BUILD)/release/%.cpp.o: %.cpp $(BUILD)/release/%.cpp.d $(ASTRAL_GL_HPP) $(BUILD)/check_astral_dependencies
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(BUILD_COMPILE_RELEASE_FLAGS) -MT $@ -MMD -MP -MF $(BUILD)/release/$*.cpp.d -c $< -o $@
$(BUILD)/release/%.cpp.d: ;
.PRECIOUS: $(BUILD)/release/%.cpp.d

$(BUILD)/release/%.resource_string.cpp.o: $(BUILD)/string_resources_cpp/%.resource_string.cpp
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(BUILD_COMPILE_RELEASE_FLAGS) -c $< -o $@

##########################################
## Collection
ASTRAL_OBJS = $(patsubst %, %.o, $(ASTRAL_SOURCES))

ASTRAL_DEBUG_SHADER_OBJS = $(patsubst %.resource_string, $(BUILD)/debug/%.resource_string.cpp.o, $(ASTRAL_SHADERS))
ASTRAL_DEBUG_OBJS = $(addprefix $(BUILD)/debug/, $(ASTRAL_OBJS))
ASTRAL_DEBUG_DEPS = $(patsubst %.o, %.d, $(ASTRAL_DEBUG_OBJS))
-include $(ASTRAL_DEBUG_DEPS)

ASTRAL_RELEASE_SHADER_OBJS = $(patsubst %.resource_string, $(BUILD)/release/%.resource_string.cpp.o, $(ASTRAL_SHADERS))
ASTRAL_RELEASE_OBJS = $(addprefix $(BUILD)/release/, $(ASTRAL_OBJS))
ASTRAL_RELEASE_DEPS = $(patsubst %.o, %.d, $(ASTRAL_RELEASE_OBJS))
-include $(ASTRAL_RELEASE_DEPS)

###########################
## make the Astral library
ifeq ($(MINGW_BUILD),1)
libAstral_debug: libAstral_debug.dll
libAstral_debug.dll.a: libAstral_debug.dll
libAstral_debug.dll: $(ASTRAL_DEBUG_OBJS) $(ASTRAL_DEBUG_SHADER_OBJS)
	$(TARGET_CXX) $(BUILD_COMPILE_DEBUG_FLAGS) --shared -Wl,--out-implib,libAstral_debug.dll.a -o $@ $^ $(BUILD_LINKER_DEBUG_FLAGS)

libAstral_release: libAstral_release.dll
libAstral_release.dll.a: libAstral_release.dll
libAstral_release.dll: $(ASTRAL_RELEASE_OBJS) $(ASTRAL_RELEASE_SHADER_OBJS)
	$(TARGET_CXX) $(BUILD_COMPILE_RELEASE_FLAGS) --shared -Wl,--out-implib,libAstral_release.dll.a -o $@ $^ $(BUILD_LINKER_RELEASE_FLAGS)

libAstral_debug_so = libAstral_debug.dll
libAstral_release_so = libAstral_release.dll

INSTALL_LIBS = libAstral_debug.dll.a libAstral_release.dll.a
INSTALL_EXES = libAstral_debug.dll libAstral_release.dll

else

libAstral_debug: libAstral_debug.so
libAstral_debug.so:  $(ASTRAL_DEBUG_OBJS) $(ASTRAL_DEBUG_SHADER_OBJS)
	$(TARGET_CXX) $(BUILD_COMPILE_DEBUG_FLAGS) -shared -Wl,$(SONAME),$@ -o $@ $^ $(BUILD_LINKER_DEBUG_FLAGS)

libAstral_release: libAstral_release.so
libAstral_release.so:  $(ASTRAL_RELEASE_OBJS) $(ASTRAL_RELEASE_SHADER_OBJS)
	$(TARGET_CXX) $(BUILD_COMPILE_RELEASE_FLAGS) -shared -Wl,$(SONAME),$@ -o $@ $^ $(BUILD_LINKER_RELEASE_FLAGS)

libAstral_debug_so = libAstral_debug.so
libAstral_release_so = libAstral_release.so

INSTALL_EXES =
INSTALL_LIBS = libAstral_debug.so libAstral_release.so

endif

libAstral: libAstral_debug libAstral_release
.PHONY: libAstral libAstral_debug libAstral_release
TARGETLIST += libAstral_debug libAstral_release libAstral

clean-build:
	-rm -r build $(ASTRAL_GL_HPP) $(ASTRAL_GL_CPP) *-release *-debug *.so *.dll *.dll.a $(CLEAN_FILES)

clean: clean-build
