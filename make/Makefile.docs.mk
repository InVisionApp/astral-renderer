# Explicitely include those GLSL sources that have doxytags#
glsl_sources = \
	src/astral/renderer/gl3/shaders/astral_compute_shadow_map_depth.glsl.resource_string \
	src/astral/renderer/gl3/shaders/astral_types_bo.glsl.resource_string \
	src/astral/renderer/gl3/shaders/astral_uniforms_common.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_utils.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_banded_rays_common.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_banded_rays_neighbor_pixel.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_banded_rays_nearest_curve.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_unpackHalf2x16.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_unpack.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_image.glsl.resource_string \
	src/astral/renderer/gl3/shaders/util/astral_image_util.glsl.resource_string \
	src/astral/renderer/gl3/shaders/brush/astral_gradient_bo.glsl.resource_string \
	src/astral/renderer/gl3/shaders/stroking/astral_stroke_common.glsl.resource_string \
	src/astral/renderer/gl3/shaders/stroking/astral_stroke_common_join.glsl.resource_string \
	src/astral/renderer/gl3/shaders/stroking/astral_stroke_common_cap.glsl.resource_string \
	src/astral/renderer/gl3/shaders/stroking/astral_stroke_capper_util.glsl.resource_string \
	src/astral/renderer/gl3/shaders/stroking/astral_stroke_biarc_util.glsl.resource_string


glsl_processed_sources = $(patsubst %.glsl.resource_string, docs/doxy/glsl/%.glsl.hpp, $(glsl_sources))
all_glsl_processed_sources = $(patsubst %.glsl.resource_string, docs/doxy/code_docs/glsl/%.glsl.hpp, $(ASTRAL_SHADERS))

ifeq ($(GLSL_DOCS), 1)
include_glsl_processed_sources = $(glsl_processed_sources)
endif

docs/doxy/glsl/%.glsl.hpp: %.glsl.resource_string
	@mkdir -p $(dir $@)
	@sed 's/$(notdir $<)/$(notdir $@)/g' $< > $@
	@$(ECHO) "Creating $@ from $<"

docs/doxy/code_docs/glsl/%.glsl.hpp: %.glsl.resource_string
	@mkdir -p $(dir $@)
	@sed 's/$(notdir $<)/$(notdir $@)/g' $< > $@
	@$(ECHO) "Creating $@ from $<"

docs: docs/doxy/html/index.html
.PHONY: docs
docs/doxy/html/index.html: $(include_glsl_processed_sources)
	@mkdir -p $(dir $@)
	-cd docs && doxygen Doxyfile

code-docs: docs/doxy/code_docs/html/index.html
.PHONY: docs
docs/doxy/code_docs/html/index.html: $(all_glsl_processed_sources)
	@mkdir -p $(dir $@)
	-cd docs && doxygen Doxyfile.code

TARGETLIST += docs code-docs
clean-docs:
	-rm -r docs/doxy/html docs/doxy/code_docs/html $(glsl_processed_sources) $(all_glsl_processed_sources)

clean: clean-docs
