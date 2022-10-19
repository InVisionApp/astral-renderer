/*!
 * \file example_brush.cpp
 * \brief example_brush.cpp
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


//! [ExampleBrush]

#include "initialization.hpp"
#include "ImageLoader.hpp"

class ExampleBrush:public Initialization
{
public:
  ExampleBrush(DemoRunner *runner, int argc, char **argv);

  virtual
  void
  draw_frame(void) override final;

private:
  astral::reference_counted_ptr<astral::Image> m_image;
  astral::reference_counted_ptr<const astral::ColorStopSequence> m_colorstop_sequence;
};

//////////////////////////////////////////
// ExampleBrush methods
ExampleBrush::
ExampleBrush(DemoRunner *runner, int argc, char **argv):
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

          /* Set the pixel data of the astral::Image. Mipmapping
           * is supported for image data as well in Astral.
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
ExampleBrush::
draw_frame(void)
{
  astral::ivec2 dims(window_dimensions());
  astral::vec2 half_dims(0.5f * astral::vec2(dims));

  astral::RenderEncoderSurface render_encoder;
  render_encoder = renderer().begin(render_target());

  /* an astral::Brush represents the how pixels are colored;
   * a brush has a base color (default value is white opaque),
   * an optional image to apply and an optional gradient to apply.
   * In addition both the image and gradient can have repeat patterns
   * and a transformation applied to them for effects.
   */
  astral::Brush render_brush;

  /* save the current transformation */
  render_encoder.save_transformation();

  /* Draw a rect with m_image sretched across the left-corner */
  astral::Rect rect;

  rect.m_min_point = astral::vec2(0.0f, 0.0f);
  if (m_image)
    {
      astral::vec2 src_size;

      src_size = astral::vec2(m_image->size());
      render_encoder.scale(half_dims / src_size);

      rect.m_max_point = src_size;

      /* an astral::ImageSampler specifies from what portions
       * (if any) of an astral::Image one is sampling from
       * along with how the image is filtered
       */
      astral::ImageSampler image_sampler;

      /* specify to source from the entire image with
       * mipmap and cubic filtering.
       */
      image_sampler = astral::ImageSampler(*m_image,
                                           astral::filter_cubic,
                                           astral::mipmap_ceiling);

      /* an astral::Brush does not take directly
       * an astral::ImageSampler value. Instead it takes
       * an astral::RenderValue<astral::ImageSampler> which
       * represents the values of an astral::ImageSampler
       * "compiled" for the underlying backend. One important
       * issue is that an astral::RenderValue<> can only
       * be made during a Renderer::begin() / Renderer::end()
       * pair and that value may only be used within the
       * pair where it was generated. It is highly encouraged
       * to reuse image_sampler_value -values- (the objects
       * themselves are just handles really) within a frame
       * so that a backend does not need to upload duplicate
       * values to the GPU.
       */
      astral::RenderValue<astral::ImageSampler> image_sampler_value;

      image_sampler_value = render_encoder.create_value(image_sampler);
      render_brush.image(image_sampler_value);
    }
  else
    {
      rect.m_max_point = half_dims;
    }

  /* just like astral::ImageSampler, we cannot use a
   * astral::Brush directly, but must have an
   * astral::RenderValue<astral::Brush>
   */
  astral::RenderValue<astral::Brush> render_brush_value;
  render_brush_value = render_encoder.create_value(render_brush);

  /* issue the call to draw the rect with out brush.
   * An astral::Brush's sampling of the image
   * takes place in -LOGICAL- coordinates, this is
   * why we draw a rect of the same size as the image
   * but have the active transformation of the RenderEncoder
   * stretch and translate the image to the top-left corner
   */
  render_encoder.draw_rect(rect, render_brush_value);

  /* restore the tranformation */
  render_encoder.restore_transformation();

  /* lets use m_colorstop_sequence to draw a linear gradient
   * in the top right corner
   */
  render_encoder.save_transformation();

  /* make the transformation so that the rect [0, 1]x[0, 1]
   * in logical coordinates is the entire top-right corner
   */
  render_encoder.translate(half_dims.x(), 0.0f);
  render_encoder.scale(half_dims);

  /* build the astral::Gradient to be a linear gradient
   * going from (0.25, 0.25) to (0.75, 0.75) with a mirror
   * repeat tile mode
   */
  astral::Gradient linear_gradient(m_colorstop_sequence,
                                         astral::vec2(0.25f, 0.25f),
                                         astral::vec2(0.75f, 0.75f),
                                         astral::tile_mode_mirror_repeat);

  /* line before, make an astral::RenderValue<astral::Gradient>
   * to be used by the brush
   */
  astral::RenderValue<astral::Gradient> gradient_render_value;

  gradient_render_value = render_encoder.create_value(linear_gradient);
  render_brush.gradient(gradient_render_value);

  // also clear the image sourcing, we jsut want the linear gradient.
  render_brush.image(astral::RenderValue<astral::ImageSampler>());

  // create a new RenderValue<Brush> for the new values
  render_brush_value = render_encoder.create_value(render_brush);

  // daraw the rect at logical [0, 1]x[0, 1]
  render_encoder.draw_rect(astral::Rect().min_point(0.0f, 0.0f).max_point(1.0f, 1.0f),
                           render_brush_value);

  // restore the transformation
  render_encoder.restore_transformation();

  /* lets repeat much of the above draing a radial gradient to the
   * bottom left hand corner.
   */
  render_encoder.save_transformation();

  /* make the transformation so that the rect [0, 1]x[0, 1]
   * in logical coordinates is the entire bottom-left corner
   */
  render_encoder.translate(0.0f, half_dims.y());
  render_encoder.scale(half_dims);

  /* A radial gradient has a start point, start radius,
   * end point and end radius. In this example we will
   * make the start and end point the same location with
   * the start radius as 0.
   */
  astral::Gradient radial_gradient(m_colorstop_sequence,
                                         astral::vec2(0.5f, 0.5f), 0.0f,
                                         astral::vec2(0.5f, 0.5f), 0.25f,
                                         astral::tile_mode_repeat);

  render_brush
    .image(astral::RenderValue<astral::ImageSampler>())
    .gradient(render_encoder.create_value(radial_gradient));
  render_encoder.draw_rect(astral::Rect().min_point(0.0f, 0.0f).max_point(1.0f, 1.0f),
                           render_encoder.create_value(render_brush));

  // restore the transformation
  render_encoder.restore_transformation();

  /* lets repeat one more time to draw a sweep gradient
   * in the bottom right corner.
   */
  render_encoder.save_transformation();
  render_encoder.translate(half_dims.x(), half_dims.y());
  render_encoder.scale(half_dims);

  /* A sweep gradient consists of a center point, a starting angle
   * where the gradient starts and a multiplier with a value
   * of N representing having the gradient interpolate go from
   * 0 at the start angle to N after going around the entire circle
   */
  astral::Gradient sweep_gradient(m_colorstop_sequence,
                                  astral::vec2(0.5f, 0.5f), //center point
                                  0.0f, //start angle in RADIANS
                                  1.5f, //multiplier
                                  astral::tile_mode_clamp);

  render_brush
    .image(astral::RenderValue<astral::ImageSampler>())
    .gradient(render_encoder.create_value(sweep_gradient));
  render_encoder.draw_rect(astral::Rect().min_point(0.0f, 0.0f).max_point(1.0f, 1.0f),
                           render_encoder.create_value(render_brush));

  // restore the transformation
  render_encoder.restore_transformation();

  /* Send the rendering commands to the GPU */
  renderer().end();
}

int
main(int argc, char **argv)
{
  DemoRunner demo_runner;
  return demo_runner.main<ExampleBrush>(argc, argv);
}

//! [ExampleBrush]
