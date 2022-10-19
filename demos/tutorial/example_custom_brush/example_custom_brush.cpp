/*!
 * \file example_custom_brush.cpp
 * \brief example_custom_brush.cpp
 *
 * Copyright 2020 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */


//! [ExampleCustomBrush]

#include <astral/renderer/gl3/material_shader_gl3.hpp>

#include "initialization.hpp"
#include "ImageLoader.hpp"

class ExampleCustomBrush:public Initialization
{
public:
  ExampleCustomBrush(DemoRunner *runner, int argc, char **argv);

  virtual
  void
  draw_frame(void) override final;

private:
  void
  create_custom_brush(void);

  astral::reference_counted_ptr<astral::Image> m_image;
  astral::reference_counted_ptr<const astral::ColorStopSequence> m_colorstop_sequence;
  astral::reference_counted_ptr<const astral::gl::MaterialShaderGL3> m_custom_brush;
};

//////////////////////////////////////////
// ExampleCustomBrush methods
ExampleCustomBrush::
ExampleCustomBrush(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv)
{
  if (argc > 1)
    {
      /* We are using a utility that is part of demos/common which loads
       * a image data via SDL_image2 from a file and computes (via a
       * simple box-filter) the mipmap pyramid. The class name is
       * ImageLoader and is not an important point of the demo, it just
       * is a way to load an image.
       */
      astral::c_string image_filename(argv[1]);
      astral::reference_counted_ptr<ImageLoader> image_data = ImageLoader::create(image_filename);
      int w(image_data->width());
      int h(image_data->height());

      if (w > 0 && h > 0)
        {
          /* Create the astral::Image by calling ImageAtlas::create_image()
           * of the ImageAtlas of the RenderEngine initialized by our base
           * class Initialization.
           */
          m_image = engine().image_atlas().create_image(astral::uvec2(w, h));

          /* Set the pixel data of the astral::Image. Astral supports mipmapping
           * with the caveat that the max LOD levels is min(log2(width), log2(height))
           * instead max(log2(width), log2(height)) which is found in 3D API's.
           */
          for (unsigned int lod = 0; w > 0 && h > 0; h >>= 1, w >>= 1, ++lod)
            {
              astral::c_array<const astral::u8vec4> pixels;

              pixels = image_data->mipmap_pixels(lod);
              m_image->set_pixels(lod,
                                  astral::ivec2(0, 0),
                                  astral::ivec2(w, h),
                                  w,
                                  pixels);
            }
        }
    }

  create_custom_brush();

  /* An astral::ColorStopSequence is a resource that is meant to
   * be resued across frames. Internally, Astral compiles the
   * color stops that define an astral::ColorStopSequence into
   * something that is used by the 3D API to perform color-stop
   * linear filtering quickly.
   */
  std::vector<astral::ColorStop<astral::FixedPointColor_sRGB>> colorstops;

  colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>(0.0f, astral::FixedPointColor_sRGB(255, 0, 0, 255)));
  colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>(0.3f, astral::FixedPointColor_sRGB(0, 255, 0, 255)));
  colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>(0.5f, astral::FixedPointColor_sRGB(0, 0, 255, 255)));
  colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>(0.8f, astral::FixedPointColor_sRGB(0, 255, 255, 255)));
  colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>(1.0f, astral::FixedPointColor_sRGB(255, 0, 255, 255)));

  m_colorstop_sequence = engine().colorstop_sequence_atlas().create(astral::make_c_array(colorstops));
}

void
ExampleCustomBrush::
create_custom_brush(void)
{
  /* We now create a custom material shader that builds off of the
   * default material shader that provides a common brush. This
   * material will modify the gradient and item sampling point
   * per pixel to give a wavy effect
   */

  /* First get the MaterialShaderGL3 that is used for brush shading */
  const astral::gl::MaterialShaderGL3 *brush_base;
  brush_base = engine().gl3_shaders().m_brush_shader.get();
  ASTRALassert(brush_base);

  /* now specify the vertex and fragment shaders of our custom brush
   *
   * In our toy example, the gradient positions are normalized
   * to [0, 1]x[0, 1] and the image is normalized to the dimensions
   * of the image. We will give a wobbly effect with a dynamic
   * amplitude. The parameters of the wobbly: phase, omega and
   * amplitute are for the unit square which makes it suitable for
   * the gradient. For the image side, we need to scale by the size
   * of the image which is represented by image_scale
   */
  astral::c_string vertex_shader =
    "void astral_material_pre_vert_shader(in uint sub_shader, in uint shader_data,\n"
    "                                     in uint brush_idx, in vec2 item_p, in AstralTransformation tr)\n"
    "{\n"
    // call the base brush's per-shader body; note the +1 because the base brush sahder data starts there
    "     brush_base::astral_material_pre_vert_shader(sub_shader, shader_data + 1u, brush_idx, item_p, tr);\n"
    "}\n"
    "void astral_material_vert_shader(in uint sub_shader, in uint shader_data,\n"
    "                                 in uint brush_idx, in vec2 item_p, in AstralTransformation  tr)\n"
    "{\n"
    "    const float PI = 3.14159265358979323846;\n"
    "    vec4 values;\n"
    // extract the shader data from at shader_data
    "    values = astral_read_item_dataf(shader_data).xyzw;\n"
    // send the shader data values as varyings
    "    custom_brush_phase = values.x;\n"
    "    custom_brush_omega = values.y;\n"
    "    custom_brush_amplitude = values.z;\n"
    "    custom_brush_image_scale = values.w;\n"
    // call the base brush; note the +1 because the base brush sahder data starts there
    "    brush_base::astral_material_vert_shader(sub_shader, shader_data + 1u, brush_idx, item_p, tr);\n"
    "}\n";

  astral::c_string fragment_shader =
    "void astral_material_pre_frag_shader(in uint sub_shader, in uint color_space) {}\n"
    "void astral_material_frag_shader(in uint sub_shader, in uint color_space, inout vec4 color, inout float coverage)\n"
    "{\n"
    "   const float PI = 3.14159265358979323846;\n"
    // call the pre-shader for the base material shader to
    // have its symbols ready for us to modify
    "   brush_base::astral_material_pre_frag_shader(sub_shader, color_space);\n"
    // modify the position of the image sampling point of the base_brush
    "   brush_base::astral_brush_image_p_x += custom_brush_image_scale * custom_brush_amplitude * cos(custom_brush_omega * (custom_brush_phase + brush_base::astral_brush_image_p_y / custom_brush_image_scale));\n"
    // modify the position of the gradient sampling point of the base_brush
    "   brush_base::astral_brush_gradient_p_x += custom_brush_amplitude * cos(custom_brush_omega * (custom_brush_phase + brush_base::astral_brush_gradient_p_y));\n"
    "   brush_base::astral_material_frag_shader(sub_shader, color_space, color, coverage);\n"
    "}\n";

  m_custom_brush
    = astral::gl::MaterialShaderGL3::create(engine(),
                                            astral::gl::ShaderSource().add_source(vertex_shader, astral::gl::ShaderSource::from_string),
                                            astral::gl::ShaderSource().add_source(fragment_shader, astral::gl::ShaderSource::from_string),
                                            astral::gl::ShaderSymbolList() //add the varyings we need
                                            .add_varying("custom_brush_phase", astral::gl::ShaderVaryings::interpolator_flat)
                                            .add_varying("custom_brush_omega", astral::gl::ShaderVaryings::interpolator_flat)
                                            .add_varying("custom_brush_amplitude", astral::gl::ShaderVaryings::interpolator_flat)
                                            .add_varying("custom_brush_image_scale", astral::gl::ShaderVaryings::interpolator_flat),
                                            brush_base->properties(),
                                            astral::gl::MaterialShaderGL3::DependencyList() //depend on the base brush and give it name base_brush
                                            .add("brush_base", *brush_base),
                                            brush_base->num_sub_shaders());
}

void
ExampleCustomBrush::
draw_frame(void)
{
  astral::ivec2 dims(window_dimensions());
  astral::vec2 half_dims(0.5f * astral::vec2(dims));
  astral::Rect rect;
  astral::RenderEncoderSurface render_encoder;
  astral::Brush render_brush;

  render_encoder = renderer().begin(render_target());
  rect.m_min_point = astral::vec2(0.0f, 0.0f);
  if (m_image)
    {
      astral::vec2 src_size;
      astral::ImageSampler image_sampler;
      astral::Transformation image_transformation;

      src_size = astral::vec2(m_image->size());

      rect.m_max_point = src_size;
      image_sampler = astral::ImageSampler(*m_image,
                                           astral::filter_cubic,
                                           astral::mipmap_ceiling);
      image_sampler
        .x_tile_mode(astral::tile_mode_mirror_repeat)
        .y_tile_mode(astral::tile_mode_mirror_repeat);

      render_brush
        .image(render_encoder.create_value(image_sampler))
        .image_transformation(render_encoder.create_value(image_transformation));
    }
  else
    {
      rect.m_max_point = half_dims;
    }

  astral::vecN<astral::gvec4, 1> custom_data;
  astral::ItemData custom_data_value;
  float phase, omega, amplitude, tf;
  uint32_t t, ticks;

  ticks = SDL_GetTicks();
  t = ticks % 4000u;
  tf = 2.0f * ASTRAL_PI * static_cast<float>(t) / 4000.0f ;
  phase = static_cast<float>(t) / 4000.0f;
  omega = 8.0f * ASTRAL_PI;
  amplitude = 0.1f * astral::t_cos(tf);

  /* pack the custom data and generate the ItemData
   * for our custom brush; the packing of data needs
   * to match perfectly the unpacking in the shader.
   */
  custom_data[0].x().f = phase;
  custom_data[0].y().f = omega;
  custom_data[0].z().f = amplitude;
  custom_data[0].w().f = astral::t_max(rect.m_max_point.x(), rect.m_max_point.y());
  custom_data_value = render_encoder.create_item_data(custom_data, astral::no_item_data_value_mapping);


  /* save the current transformation */
  render_encoder.save_transformation();
    {
      render_encoder.scale(half_dims / rect.m_max_point);
      render_encoder.draw_rect(rect,
                               astral::Material(*m_custom_brush,
                                                      render_encoder.create_value(render_brush),
                                                      custom_data_value));

    }
  render_encoder.restore_transformation();

  render_encoder.save_transformation();
    {
      render_encoder.translate(half_dims.x(), 0.0f);
      render_encoder.scale(half_dims);

      astral::Gradient linear_gradient(m_colorstop_sequence,
                                             astral::vec2(0.45f, 0.45f),
                                             astral::vec2(0.55f, 0.55f),
                                             astral::tile_mode_mirror_repeat);

      render_brush
        .gradient(render_encoder.create_value(linear_gradient))
        .image(astral::RenderValue<astral::ImageSampler>());
      render_encoder.draw_rect(astral::Rect().min_point(0.0f, 0.0f).max_point(1.0f, 1.0f),
                               astral::Material(*m_custom_brush,
                                                      render_encoder.create_value(render_brush),
                                                      custom_data_value));
    }
  render_encoder.restore_transformation();

  render_encoder.save_transformation();
    {
      render_encoder.translate(0.0f, half_dims.y());
      render_encoder.scale(half_dims);

      astral::Gradient radial_gradient(m_colorstop_sequence,
                                             astral::vec2(0.5f, 0.5f), 0.0f,
                                             astral::vec2(0.5f, 0.5f), 0.25f,
                                             astral::tile_mode_repeat);

      render_brush
        .image(astral::RenderValue<astral::ImageSampler>())
        .gradient(render_encoder.create_value(radial_gradient));
      render_encoder.draw_rect(astral::Rect().min_point(0.0f, 0.0f).max_point(1.0f, 1.0f),
                               astral::Material(*m_custom_brush,
                                                      render_encoder.create_value(render_brush),
                                                      custom_data_value));
    }
  render_encoder.restore_transformation();

  render_encoder.save_transformation();
    {
      unsigned int t;
      float s, c, tt;

      render_encoder.translate(half_dims.x(), half_dims.y());
      render_encoder.scale(half_dims);

      t = ticks % 32000u;
      tt = ASTRAL_PI * static_cast<float>(t) / 16000.0f;
      s = astral::t_sin(tt);
      c = astral::t_cos(tt);

      astral::Gradient sweep_gradient(m_colorstop_sequence,
                                            astral::vec2(0.5f + 0.25f * c, 0.5f + 0.25f * s), //center point
                                            tt, //start angle in RADIANS
                                            12.0f, //multiplier
                                            astral::tile_mode_mirror_repeat);

      render_brush
        .image(astral::RenderValue<astral::ImageSampler>())
        .gradient(render_encoder.create_value(sweep_gradient));
      render_encoder.draw_rect(astral::Rect().min_point(0.0f, 0.0f).max_point(1.0f, 1.0f),
                               astral::Material(*m_custom_brush,
                                                      render_encoder.create_value(render_brush),
                                                      custom_data_value));
    }
  render_encoder.restore_transformation();

  /* Send the rendering commands to the GPU */
  renderer().end();
}

int
main(int argc, char **argv)
{
  DemoRunner demo_runner;
  return demo_runner.main<ExampleCustomBrush>(argc, argv);
}

//! [ExampleCustomBrush]
