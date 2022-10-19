#########################################
## Flags for demos
DEMO_COMMON_LIBS := \
	$(shell pkg-config freetype2 --libs) \
	$(shell pkg-config SDL2_image --libs) \
	$(shell pkg-config libpng --libs)

DEMO_DEBUG_LIBS = $(DEMO_COMMON_LIBS) $(BASE_LINKER_DEBUG_FLAGS) -L. -lAstral_debug
DEMO_RELEASE_LIBS = $(DEMO_COMMON_LIBS) $(BASE_LINKER_RELEASE_FLAGS) -L. -lAstral_release

DEMO_COMMON_FLAGS := \
	-Wall -Wextra -Wdouble-promotion -Wunused \
	-Iinc -Idemos/common -Idemos/tutorial/common \
	$(shell pkg-config freetype2 --cflags) \
	$(shell pkg-config SDL2_image --cflags) \
	$(shell pkg-config libpng --cflags)

DEMO_DEBUG_FLAGS = $(DEMO_COMMON_FLAGS) $(BASE_COMPILE_DEBUG_FLAGS) -g
DEMO_RELEASE_FLAGS = $(DEMO_COMMON_FLAGS) $(BASE_COMPILE_RELEASE_FLAGS) -O3

##########################################################
## Debug Demo recipes
$(BUILD)/debug/demo/%.cpp.o: %.cpp $(BUILD)/debug/demo/%.cpp.d $(ASTRAL_GL_HPP) $(BUILD)/check_demo_dependencies
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(DEMO_DEBUG_FLAGS) -MT $@ -MMD -MP -MF $(BUILD)/debug/demo/$*.cpp.d -c $< -o $@
$(BUILD)/debug/demo/%.cpp.d: ;
.PRECIOUS: $(BUILD)/debug/demo/%.cpp.d

$(BUILD)/demo/debug/%.resource_string.cpp.o: $(BUILD)/string_resources_cpp/%.resource_string.cpp
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(DEMO_DEBUG_FLAGS) -c $< -o $@

##################################################
## Release demo recipes
$(BUILD)/release/demo/%.cpp.o: %.cpp $(BUILD)/release/demo/%.cpp.d $(ASTRAL_GL_HPP) $(BUILD)/check_demo_dependencies
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(DEMO_RELEASE_FLAGS) -MT $@ -MMD -MP -MF $(BUILD)/release/demo/$*.cpp.d -c $< -o $@
$(BUILD)/release/demo/%.cpp.d: ;
.PRECIOUS: $(BUILD)/release/demo/%.cpp.d

$(BUILD)/demo/release/%.resource_string.cpp.o: $(BUILD)/string_resources_cpp/%.resource_string.cpp
	@mkdir -p $(dir $@)
	$(TARGET_CXX) $(DEMO_RELEASE_FLAGS) -c $< -o $@

######################
## Rule to make demos
DEMO_COMMON_LIBS+=$(ASTRAL_DEP_LIBS)
COMMON_DEMO_OBJS = $(patsubst %, %.o, $(COMMON_DEMO_SOURCES))

COMMON_DEMO_SHADER_DEBUG_OBJS = $(patsubst %.resource_string, $(BUILD)/demo/debug/%.resource_string.cpp.o, $(COMMON_DEMO_SHADERS))
COMMON_DEMO_DEBUG_OBJS = $(addprefix $(BUILD)/debug/demo/, $(COMMON_DEMO_OBJS))
COMMON_DEMO_DEBUG_DEPS = $(patsubst %.o, %.d, $(COMMON_DEMO_DEBUG_OBJS))
-include $(COMMON_DEMO_DEBUG_DEPS)

COMMON_DEMO_SHADER_RELEASE_OBJS = $(patsubst %.resource_string, $(BUILD)/demo/release/%.resource_string.cpp.o, $(COMMON_DEMO_SHADERS))
COMMON_DEMO_RELEASE_OBJS = $(addprefix $(BUILD)/release/demo/, $(COMMON_DEMO_OBJS))
COMMON_DEMO_RELEASE_DEPS = $(patsubst %.o, %.d, $(COMMON_DEMO_RELEASE_OBJS))
-include $(COMMON_DEMO_RELEASE_DEPS)

define demorule
$(eval THISDEMO_$(1)_SHADER_DEBUG_OBJS:=$$(patsubst %.resource_string, $(BUILD)/demo/debug/%.resource_string.cpp.o, $$($(1)_SHADERS))
THISDEMO_$(1)_DEBUG_OBJS:=$$(addprefix $(BUILD)/debug/demo/, $$(patsubst %, %.o, $$($(1)_SOURCES)))
THISDEMO_$(1)_DEBUG_DEPS:= $$(patsubst %.o, %.d, $$(THISDEMO_$(1)_DEBUG_OBJS))
-include $$(THISDEMO_$(1)_DEBUG_DEPS)
$(1)-debug: $(libAstral_debug_so) $$(COMMON_DEMO_DEBUG_OBJS) $$(COMMON_DEMO_SHADER_DEBUG_OBJS) $$(THISDEMO_$(1)_DEBUG_OBJS) $$(THISDEMO_$(1)_SHADER_DEBUG_OBJS)
	$(TARGET_CXX) $(DEMO_DEBUG_FLAGS) $$(THISDEMO_$(1)_DEBUG_OBJS) $$(COMMON_DEMO_DEBUG_OBJS) $(COMMON_DEMO_SHADER_DEBUG_OBJS) $$(THISDEMO_$(1)_SHADER_DEBUG_OBJS) -o $$@ $(DEMO_DEBUG_LIBS)

THISDEMO_$(1)_SHADER_RELEASE_OBJS:=$$(patsubst %.resource_string, $(BUILD)/demo/release/%.resource_string.cpp.o, $$($(1)_SHADERS))
THISDEMO_$(1)_RELEASE_OBJS:=$$(addprefix $(BUILD)/release/demo/, $$(patsubst %, %.o, $$($(1)_SOURCES)))
THISDEMO_$(1)_RELEASE_DEPS:= $$(patsubst %.o, %.d, $$(THISDEMO_$(1)_RELEASE_OBJS))
-include $$(THISDEMO_$(1)_RELEASE_DEPS)
$(1)-release: $(libAstral_release_so) $$(COMMON_DEMO_RELEASE_OBJS) $$(COMMON_DEMO_SHADER_RELEASE_OBJS) $$(THISDEMO_$(1)_RELEASE_OBJS) $$(THISDEMO_$(1)_SHADER_RELEASE_OBJS)
	$(TARGET_CXX) $(DEMO_RELEASE_FLAGS)  $$(THISDEMO_$(1)_RELEASE_OBJS) $$(COMMON_DEMO_RELEASE_OBJS) $$(COMMON_DEMO_SHADER_RELEASE_OBJS) $$(THISDEMO_$(1)_SHADER_RELEASE_OBJS) -o $$@ $(DEMO_RELEASE_LIBS)

$(1): $(1)-debug $(1)-release
demos-debug: $(1)-debug
demos-release: $(1)-release
demos: $(1)
.PHONY: $(1) demos demos-debug demos-release
TARGETLIST += $(1) $(1)-debug $(1)-release
)
endef

TARGETLIST += demos demos-debug demos-release
$(foreach demoname,$(ASTRAL_DEMOS),$(call demorule,$(demoname)))
