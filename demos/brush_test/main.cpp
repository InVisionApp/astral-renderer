/*!
 * \file main.cpp
 * \brief main.cpp
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
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <stdlib.h>

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>

#include "command_line_list.hpp"
#include "sdl_demo.hpp"
#include "render_engine_gl3_demo.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "read_colorstops.hpp"
#include "cycle_value.hpp"
#include "text_helper.hpp"

class BrushTest:public render_engine_gl3_demo
{
public:
  BrushTest(void);

protected:
  virtual
  void
  init_gl(int, int) override;

  virtual
  void
  draw_frame(void) override;

  virtual
  void
  handle_event(const SDL_Event &ev) override;

private:
  enum mode_t
    {
      direct_to_surface,
      blur_mode,
      blur_mode_alt,
      layer_mode,

      number_modes
    };

  static
  astral::c_string
  label(enum mode_t mode)
  {
    static const astral::c_string v[number_modes]=
      {
        [direct_to_surface] = "direct_to_surface",
        [blur_mode] = "blur_mode",
        [blur_mode_alt] = "blur_mode_alt",
        [layer_mode] = "layer_mode",
      };

    return v[mode];
  }

  class PerImage
  {
  public:
    PerImage(const std::string &filename,
             astral::ivec2 sz):
      m_filename(filename),
      m_image_transformation_active(false),
      m_gradient_transformation_active(false),
      m_image_transformation_mapping_active(false),
      m_gradient_transformation_mapping_active(false)
    {
      m_rect
        .min_point(0.0f, 0.0f)
        .max_point(sz.x(), sz.y());

      m_big_rect
        .min_point(-3 * sz.x(), -3 * sz.y())
        .max_point(+3 * sz.x(), +3 * sz.y());

      m_image_transformation
        .x_tile(astral::TileRange()
                .begin(0.0f)
                .end(sz.x())
                .mode(astral::tile_mode_repeat))
        .y_tile(astral::TileRange()
                .begin(0.0f)
                .end(sz.y())
                .mode(astral::tile_mode_repeat));

      m_gradient_transformation = m_image_transformation;
    }

    PerImage(const std::string &filename,
             astral::reference_counted_ptr<astral::Image> image):
      PerImage(filename, astral::ivec2(image->size()))
    {
      m_image = image;
    }

    const astral::GradientTransformation&
    gradient_transformation(const astral::Transformation &tr)
    {
      if (m_gradient_transformation_mapping_active)
        {
          m_gradient_transformation.m_transformation = tr;
        }
      else
        {
          m_gradient_transformation.m_transformation = astral::Transformation();
        }
      return m_gradient_transformation;
    }

    std::string m_filename;
    astral::Rect m_rect, m_big_rect;
    astral::reference_counted_ptr<astral::Image> m_image;
    astral::GradientTransformation m_image_transformation, m_gradient_transformation;
    bool m_image_transformation_active, m_gradient_transformation_active;
    bool m_image_transformation_mapping_active, m_gradient_transformation_mapping_active;
  };

  class PerColorStop
  {
  public:
    PerColorStop(BrushTest &p,
                 const std::string &filename,
                 astral::c_array<const astral::ColorStop<astral::FixedPointColor_sRGB>> colorstops):
      m_filename(filename)
    {
      m_sequence = p.engine().colorstop_sequence_atlas().create(colorstops);
      std::cout << "Made colorstop sequence " << m_filename << " from "
                << colorstops << ", location = " << m_sequence->location()
                << ", layer = " << m_sequence->layer()
                << "\n";
    }

    std::string m_filename;
    astral::reference_counted_ptr<astral::ColorStopSequence> m_sequence;
  };

  void
  create_images(int w, int h);

  astral::reference_counted_ptr<astral::Image>
  create_miptest_pattern(unsigned int log2_size);

  void
  create_colorstop_sequences(void);

  void
  create_ui_rects(void);

  float
  update_smooth_values(void);

  astral::Gradient
  generate_gradient(void);

  void
  draw_ui_rect(astral::RenderEncoderSurface render_encoder,
               astral::RenderValue<astral::Brush> outer,
               astral::RenderValue<astral::Brush> inner,
               astral::vec2 p);

  void
  draw_hud(astral::RenderEncoderSurface render_encoder,
           float frame_ms, const astral::Transformation &tr);

  command_separator m_demo_options;
  command_line_list_images m_loaded_images;
  command_line_list_colorstops<astral::colorspace_srgb> m_loaded_colorstop_sequences;
  enumerated_command_line_argument_value<enum mode_t> m_mode;
  command_line_argument_value<float> m_blur_radius;
  command_line_argument_value<int> m_max_sample_radius;

  command_line_argument_value<bool> m_draw_big_rect, m_with_aa;
  command_line_argument_value<unsigned int> m_current_image;
  command_line_argument_value<unsigned int> m_current_colorstop;
  enumerated_command_line_argument_value<enum astral::tile_mode_t> m_gradient_tile_mode;
  enumerated_command_line_argument_value<enum astral::Gradient::type_t> m_gradient_type;
  command_line_argument_value<astral::vec2> m_gradient_p0, m_gradient_p1;
  command_line_argument_value<float> m_gradient_r0, m_gradient_r1;
  command_line_argument_value<float> m_gradient_sweep_factor;
  enumerated_command_line_argument_value<enum astral::filter_t> m_filter_mode;
  command_line_argument_value<unsigned int> m_mipmap_mode;
  command_line_argument_value<bool> m_include_halo;
  command_line_argument_value<float> m_blur_min_scale_factor;
  command_line_argument_value<float> m_scale_factor;
  command_line_argument_value<astral::vec2> m_scale_pre_rotate, m_scale_post_rotate;
  command_line_argument_value<float> m_rotate_angle;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;

  command_line_argument_value<bool> m_image_transformation_active, m_gradient_transformation_active;
  command_line_argument_value<bool> m_image_transformation_mapping_active, m_gradient_transformation_mapping_active;
  enumerated_command_line_argument_value<enum astral::tile_mode_t> m_image_tile_x, m_image_tile_y;
  enumerated_command_line_argument_value<enum astral::tile_mode_t> m_gradient_tile_x, m_gradient_tile_y;

  std::vector<PerImage> m_images;
  std::vector<PerColorStop> m_colorstop_sequences;
  astral::Rect m_ui_inner_rect, m_ui_outer_rect;

  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  std::vector<unsigned int> m_prev_stats;

  PanZoomTrackerSDLEvent m_zoom;
  simple_time m_draw_timer;
  bool m_draw_detailed_hud;
};

//////////////////////////////////
// BrushTest methods
BrushTest::
BrushTest(void):
  m_demo_options("Demo Options", *this),
  m_loaded_images(&std::cout, "add_image", "Add an image to view", *this),
  m_loaded_colorstop_sequences("add_colorstop", "Add a colorstop to use", *this),
  m_mode(direct_to_surface,
         enumerated_string_type<enum mode_t>()
         .add_entry(label(direct_to_surface), direct_to_surface, "render rect directly to window")
         .add_entry(label(blur_mode), blur_mode, "render rect with gaussian blur applied")
         .add_entry(label(blur_mode_alt), blur_mode_alt, "render rect with gaussian blur applied using effect collection")
         .add_entry(label(layer_mode), layer_mode, "render rect to a layer first"),
         "initial_render_mode",
         "Specified initial rendering mode",
         *this),
  m_blur_radius(4.0f, "initial_blur_radius", "Initial blur radius", *this),
  m_max_sample_radius(16, "initial_max_blur_sample_radius", "", *this),
  m_draw_big_rect(false, "draw_big_rect",
                  "if true draw a large rect that 6 times the size of the original image "
                  "where the inflation is all size of the rect", *this),
  m_with_aa(true, "with_aa", "apply anti-aliasing when drawing the rect", *this),
  m_current_image(0u, "current_image", "A value of 0 indicates to not apply an image, "
                  "a value i for 1 <= i <= N where N is the number of images loaded "
                  "indicates to use the i'th image loaded. A value of N + 1 indicates "
                  "to use the last image loaded which has upto 16 mipmap levels deep "
                  "and was generated on GPU. A value of N + 2 indicates to use the last "
                  "imae loaded with only one mipmap level generated on GPU. A value of "
                  "N + 3 indicates to use an image which has 9 mipmap levels where each "
                  "mipmap level is a checkerboard, but the colors alternate on mipmap "
                  "levels. If no image is loaded, the demo will use a checkerboard image "
                  "rendered by GPU as the 'last' image", *this),
  m_current_colorstop(0u, "current_colorstop", "a value of i for 0 <= i < N where N is "
                      "the number of loaded colorstop sequences to use the (i+1)'th loaded "
                      "colorstop. If no colorstop sequences was loaded, the demo will create "
                      "a default colorstop sequence.", *this),
  m_gradient_tile_mode(astral::tile_mode_repeat,
                       enumerated_string_type<enum astral::tile_mode_t>(&astral::label, astral::tile_mode_number_modes),
                       "gradient_tile_mode",
                       "tile mode to apply to gradient pattern for interpolate outside of [0, 1]",
                       *this),
  m_gradient_type(astral::Gradient::number_types,
                  enumerated_string_type<enum astral::Gradient::type_t>(&astral::label, astral::Gradient::number_types)
                  .add_entry("no_gradient", astral::Gradient::number_types, ""),
                  "gradient_type",
                  "specify the whhat kind of gradient (if any) to apply",
                  *this),
  m_gradient_p0(astral::vec2(0.0f, 0.0f), "gradient_p0", "position for start point of gradient (linear and radial) or position of gradent center (for sweep gradients) ", *this),
  m_gradient_p1(astral::vec2(0.0f, 0.0f), "gradient_p1",
                "if set position for end point of gradient (linear and radial) or position of point to determine start axis "
                "(for sweep gradients), if not set value will be the dimensions of the window", *this),
  m_gradient_r0(0.0f, "gradient_r0", "if set, start radius for radial gradient, if not set value is maximum of the width and height of the window", *this),
  m_gradient_r1(0.0f, "gradient_r1", "if set, end radius for radial gradient, if not set value is maximum of the width and height of the window", *this),
  m_gradient_sweep_factor(1.0f, "gradient_sweep_factor", "gradient sweep factor for sweep gradient (i.e. how many times it repeats)", *this),
  m_filter_mode(astral::filter_linear,
                enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                "filter_mode",
                "what filter to apply to the image",
                *this),
  m_mipmap_mode(astral::mipmap_ceiling, "mipmap_mode",
                "Mipmap mode to apply to image with additional values to control to use a specific mipmap:\n"
                "\t0 <---> mipmap_none\n"
                "\t1 <---> mipmap_ceiling\n"
                "\t2 <---> mipmap_floor\n"
                "\tN + 2 <--> use mipmap level N\n",
                *this),
  m_include_halo(true, "include_halo", "when draing blurred, include the blur halo around the rectangle", *this),
  m_blur_min_scale_factor(0.0f, "blur_min_scale_factor",
                          "sets the minimum rendering scale when drawing blurred",
                          *this),
  m_scale_factor(1.0f, "layer_scale_factor", "if mode is layer_mode, draw the rect with this scale factor", *this),
  m_scale_pre_rotate(astral::vec2(1.0f, 1.0f), "scale_pre_rotate", "scaling transformation to apply to rectangle before rotation, formatted as ScaleX:SaleY", *this),
  m_scale_post_rotate(astral::vec2(1.0f, 1.0f), "scale_post_rotate", "scaling transformation to apply to rectangle after rotation, formatted as ScaleX:SaleY", *this),
  m_rotate_angle(0.0f, "rotate_angle", "rotation of path in degrees to apply to path", *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera", "Initial position of camera", *this),
  m_image_transformation_active(false, "image_transformation_active", "if set, initialize all image choices to that the image transformation is applied", *this),
  m_gradient_transformation_active(false, "gradient_transformation_active", "if set, initialize all image choices to that the gradient transformation is applied", *this),
  m_image_transformation_mapping_active(false, "image_transformation_mapping_active", "if set, initialize all image choices to that the image transformation has the mapping is applied", *this),
  m_gradient_transformation_mapping_active(false, "gradient_transformation_mapping_active", "if set, initialize all image choices to that the gradient transformation has the mapping is applied", *this),
  m_image_tile_x(astral::tile_mode_repeat,
                 enumerated_string_type<enum astral::tile_mode_t>(&astral::label, astral::tile_mode_number_modes),
                 "image_tile_x",
                 "If set, initialize all image choices' tile mode applied to the image in the x-coordinate",
                 *this),
  m_image_tile_y(astral::tile_mode_repeat,
                 enumerated_string_type<enum astral::tile_mode_t>(&astral::label, astral::tile_mode_number_modes),
                 "image_tile_y",
                 "If set, initialize all image choices' tile mode applied to the image in the y-coordinate",
                 *this),
  m_gradient_tile_x(astral::tile_mode_repeat,
                    enumerated_string_type<enum astral::tile_mode_t>(&astral::label, astral::tile_mode_number_modes),
                    "gradient_tile_x",
                    "If set, initialize all image choices' tile mode applied to the gradient in the x-coordinate",
                    *this),
  m_gradient_tile_y(astral::tile_mode_repeat,
                    enumerated_string_type<enum astral::tile_mode_t>(&astral::label, astral::tile_mode_number_modes),
                    "gradient_tile_y",
                    "If set, initialize all image choices' tile mode applied to the gradient in the y-coordinate",
                    *this),
  m_draw_detailed_hud(false)
{
  std::cout << "Controls:"
            << "\n\tspace: toggle hud"
            << "\n\tp: print current (most of) state as command line arguments"
            << "\n\ta: toggle rect anti-alias"
            << "\n\ti: change image"
            << "\n\tf: change image filter"
            << "\n\tm: change image mipmap mode"
            << "\n\tb: toggle blur"
            << "\n\tn: toggle using mipmaps for blur"
            << "\n\tup/down arror: increase/decrease blur radius"
            << "\n\tright/left arror: increase/decrease maximum pixel blur radius"
            << "\n\tc: change color-stop"
            << "\n\th: change tile mode applied to gradient interpolate"
            << "\n\tg: change gradient type"
            << "\n\t1: increase start radius for radial gradient"
            << "\n\t2: decrease start radius for radial gradient"
            << "\n\t3: increase end radius for radial gradient"
            << "\n\t4: decrease end radius for radial gradient"
            << "\n\t1: decrease sweep factor for sweep gradient"
            << "\n\t2: increase sweep factor for sweep gradient"
            << "\n\tr: toggle drawing big or small rect"
            << "\n\ts: toggle apply image_transformation to image"
            << "\n\tx: change x-tile mode on image if image image_transformation is on"
            << "\n\ty: change y-tile mode on image if image image_transformation is on"
            << "\n\tz: toggle image-transformation on image if image image_transformation is on"
            << "\n\tctrl-s: toggle apply image_transformation to gradient"
            << "\n\tctrl-x: change x-tile mode on gradient if gradient image_transformation is on"
            << "\n\tctrl-y: change y-tile mode on gradient if gradient image_transformation is on"
            << "\n\tctrl-z: toggle gradient-transformation on gradient if gradient image_transformation is on"
            << "\n\tq: reset transformation applied to rect"
            << "\n\t6: increase horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t9/0 increase/decrease angle of rotation"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse button, then drag up/down: zoom out/in"
            << "\n\tDrag Middle Mouse button: move start gradient point\n"
            << "\n\tDraw Right Mouse button: move start gradient point\n"
            << "\n";
}

static
void
create_checker_board(unsigned int log2_size,
                     astral::u8vec4 c0, astral::u8vec4 c1,
                     std::vector<astral::u8vec4> *dst_pixels)
{
  std::vector<astral::u8vec4> &pixels(*dst_pixels);
  unsigned int sz(1u << log2_size);

  pixels.resize(sz * sz);
  for (unsigned int y = 0; y < sz; ++y)
    {
      for (unsigned int x = 0; x < sz; ++x)
        {
          unsigned int v;

          v = (x + y) & 1u;
          pixels[x + y * sz] = (v == 0) ? c0 : c1;
        }
    }
}

astral::reference_counted_ptr<astral::Image>
BrushTest::
create_miptest_pattern(unsigned int log2_size)
{
  /* Generate a special image to test mipmaps;
   * On even LOD's it is white and red squares
   * and on odd LOD's it is green and blue squares.
   */
  unsigned int size(1u << log2_size);
  astral::reference_counted_ptr<astral::Image> image;

  image = engine().image_atlas().create_image(astral::uvec2(size));
  for (unsigned int LOD = 0; LOD <= log2_size; ++LOD)
    {
      unsigned int L(log2_size - LOD), sz(1u << L);
      std::vector<astral::u8vec4> pixels;
      astral::u8vec4 c0, c1;

      if ((LOD & 1u) == 0u)
        {
          c0 = astral::u8vec4(255, 255, 255, 255);
          c1 = astral::u8vec4(255, 0, 0, 255);
        }
      else
        {
          c0 = astral::u8vec4(0, 255, 0, 255);
          c1 = astral::u8vec4(0, 0, 255, 255);
        }

      create_checker_board(L, c0, c1, &pixels);
      image->set_pixels(LOD, astral::ivec2(0, 0), astral::ivec2(sz, sz), sz,
                        astral::make_c_array(pixels));
    }
  image->override_to_opaque();
  image->colorspace(astral::colorspace_srgb);

  return image;
}

void
BrushTest::
create_images(int w, int h)
{
  m_images.push_back(PerImage("noimage", astral::ivec2(w, h)));
  for (const auto &e : m_loaded_images.elements())
    {
      m_images.push_back(PerImage(e.m_filename,
                                  e.m_loaded_value->create_image(engine().image_atlas())));
    }
  m_loaded_images.clear();
}

void
BrushTest::
create_colorstop_sequences(void)
{
  for (const auto &e : m_loaded_colorstop_sequences.elements())
    {
      m_colorstop_sequences.push_back(PerColorStop(*this, e.m_filename,
                                                   astral::make_c_array(e.m_loaded_value)));
    }

  if (m_colorstop_sequences.empty())
    {
      std::vector<astral::ColorStop<astral::FixedPointColor_sRGB>> colorstops;

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(255u, 255u, 255u, 255u))
                           .t(0.0f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(0u, 255u, 0u, 255u))
                           .t(0.25f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(0u, 0u, 255u, 255u))
                           .t(0.5f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(255u, 0u, 0u, 255u))
                           .t(0.5f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(0u, 255u, 0u, 255u))
                           .t(0.75f));

      colorstops.push_back(astral::ColorStop<astral::FixedPointColor_sRGB>()
                           .color(astral::FixedPointColor_sRGB(255u, 255u, 0u, 255u))
                           .t(1.0f));

      m_colorstop_sequences.push_back(PerColorStop(*this, "default-colorstop-sequence",
                                                   astral::make_c_array(colorstops)));
    }
}

void
BrushTest::
create_ui_rects(void)
{
  float inner_size(15.0f);
  float outer_size(30.0f);

  m_ui_inner_rect
    .min_point(-0.5f * inner_size, -0.5f * inner_size)
    .max_point(+0.5f * inner_size, +0.5f * inner_size);

  m_ui_outer_rect
    .min_point(-0.5f * outer_size, -0.5f * outer_size)
    .max_point(+0.5f * outer_size, +0.5f * outer_size);
}

void
BrushTest::
init_gl(int w, int h)
{
  create_images(w, h);
  create_colorstop_sequences();
  create_ui_rects();

  if (!m_gradient_p1.set_by_command_line())
    {
      m_gradient_p1.value() = astral::vec2(w, h);
    }

  if (!m_gradient_r0.set_by_command_line())
    {
      m_gradient_r0.value() = astral::t_max(w, h);
    }

  if (!m_gradient_r1.set_by_command_line())
    {
      m_gradient_r1.value() = astral::t_max(w, h);
    }

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  m_prev_stats.resize(renderer().stats_labels().size(), 0);

  /* generate a mip image */
  astral::RenderEncoderSurface render_encoder;
  astral::RenderEncoderImage image_encoder;
  std::string prefix_name(" mips generated by Renderer: "), image_name;
  std::string pre_prefix_name1("1"), pre_prefix_name16("16");

  render_encoder = renderer().begin(render_target());

  if (m_images.size() <= 1u)
    {
      astral::vecN<astral::RenderValue<astral::Brush>, 2> b;
      const unsigned int square_size(32), num_squares(32), sz(square_size * num_squares);

      image_encoder = render_encoder.encoder_image(astral::ivec2(sz, sz));
      b[0] = image_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
      b[1] = image_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 1.0f, 0.0f, 1.0f)));

      for (unsigned int y = 0; y < num_squares; ++y)
        {
          for (unsigned int x = 0; x < num_squares; ++x)
            {
              astral::Rect R;
              unsigned int idx;

              R.m_min_point = astral::vec2(x * square_size, y * square_size);
              R.m_max_point = R.m_min_point + astral::vec2(square_size, square_size);
              idx = (x + y) & 1u;

              image_encoder.draw_rect(R, false, b[idx]);
            }
        }
      image_name = "checkerboard";
    }
  else
    {
      const astral::Image &image(*m_images.back().m_image);
      astral::Brush brush;
      astral::Rect R;

      image_encoder = render_encoder.encoder_image(astral::ivec2(image.size()));

      R.m_min_point = astral::vec2(0.0f, 0.0f);
      R.m_max_point = astral::vec2(image.size());
      brush.image(image_encoder.create_value(astral::ImageSampler(image, astral::MipmapLevel(0))));
      image_encoder.draw_rect(R, false, image_encoder.create_value(brush));

      image_name = m_images.back().m_filename;
    }

  image_encoder.finish();
  m_images.push_back(PerImage(pre_prefix_name16 + prefix_name + image_name, image_encoder.image_with_mips(16)));
  m_images.push_back(PerImage(pre_prefix_name1 + prefix_name + image_name, image_encoder.image_with_mips(1)));
  m_images.push_back(PerImage("MipTest", create_miptest_pattern(9u)));

  for (PerImage &im: m_images)
    {
      if (m_image_transformation_active.set_by_command_line())
        {
          im.m_image_transformation_active = m_image_transformation_active.value();
        }

      if (m_gradient_transformation_active.set_by_command_line())
        {
          im.m_gradient_transformation_active = m_gradient_transformation_active.value();
        }

      if (m_image_transformation_mapping_active.set_by_command_line())
        {
          im.m_image_transformation_mapping_active = m_image_transformation_mapping_active.value();
        }

      if (m_gradient_transformation_mapping_active.set_by_command_line())
        {
          im.m_gradient_transformation_mapping_active = m_gradient_transformation_mapping_active.value();
        }

      if (m_image_tile_x.set_by_command_line())
        {
          im.m_image_transformation.m_x_tile.m_mode = m_image_tile_x.value();
        }

      if (m_image_tile_y.set_by_command_line())
        {
          im.m_image_transformation.m_y_tile.m_mode = m_image_tile_y.value();
        }

      if (m_gradient_tile_x.set_by_command_line())
        {
          im.m_gradient_transformation.m_x_tile.m_mode = m_gradient_tile_x.value();
        }

      if (m_gradient_tile_y.set_by_command_line())
        {
          im.m_gradient_transformation.m_y_tile.m_mode = m_gradient_tile_y.value();
        }
    }

  m_zoom.transformation(m_initial_camera.value());

  renderer().end();
}

float
BrushTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float delta, scale_delta, angle_delta, frame_ms;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);

  ASTRALassert(keyboard_state != nullptr);
  frame_ms = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
    }

  scale_delta = 0.01f * delta;
  angle_delta = 0.0025f * delta * 180.0f / ASTRAL_PI;

  if (m_mode.value() == layer_mode || m_mode.value() == blur_mode || m_mode.value() == blur_mode_alt)
    {
      bool changed(false);
      astral::c_string txt;
      float *dst;
      float factor;

      dst = (m_mode.value() == layer_mode) ? &m_scale_factor.value() : &m_blur_radius.value();
      txt = (m_mode.value() == layer_mode) ? "scale factor" : "blur_radius";
      factor = (m_mode.value() == layer_mode) ? 0.0001f: 0.01f;

      if (keyboard_state[SDL_SCANCODE_UP])
        {
          changed = true;
          *dst += factor * delta;
        }

      if (keyboard_state[SDL_SCANCODE_DOWN])
        {
          changed = true;
          *dst -= factor * delta;
        }

      if (changed)
        {
          *dst = astral::t_max(*dst, 0.0f);
          if (m_mode.value() == layer_mode)
            {
              *dst = astral::t_min(*dst, 1.0f);
            }

          std::cout << txt << " set to " << *dst << "\n";
        }
    }

  if (keyboard_state[SDL_SCANCODE_LCTRL] || keyboard_state[SDL_SCANCODE_RCTRL])
    {
      scale_delta = -scale_delta;
    }

  if (keyboard_state[SDL_SCANCODE_RETURN])
    {
      scale_ptr = &m_scale_post_rotate.value();
      scale_txt = "post-rotate-scale";
    }
  else
    {
      scale_ptr = &m_scale_pre_rotate.value();
      scale_txt = "pre-rotate-scale";
    }

  if (keyboard_state[SDL_SCANCODE_V])
    {
      m_blur_min_scale_factor.value() += scale_delta * 0.1f;
      m_blur_min_scale_factor.value() = astral::t_clamp(m_blur_min_scale_factor.value(), 0.0f, 1.0f);
      std::cout << "Blur min-scale factor set to: " << m_blur_min_scale_factor.value() << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_6])
    {
      scale_ptr->x() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7])
    {
      scale_ptr->y() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9])
    {
      m_rotate_angle.value() += angle_delta;
      if (angle_delta > 360.0f)
        {
          m_rotate_angle.value() -= 360.0f;
        }
      std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
    }
  if (keyboard_state[SDL_SCANCODE_0])
    {
      m_rotate_angle.value() -= angle_delta;
      if (angle_delta < 0.0f)
        {
          m_rotate_angle.value() += 360.0f;
        }
      std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
    }

  if (astral::Gradient::is_radial_gradient(m_gradient_type.value()))
    {
      float rd(delta * 0.1f);
      bool p(false);

      if (keyboard_state[SDL_SCANCODE_1])
        {
          m_gradient_r0.value() -= rd;
          m_gradient_r0.value() = astral::t_max(0.0f, m_gradient_r0.value());
          p = true;
        }
      if (keyboard_state[SDL_SCANCODE_2])
        {
          m_gradient_r0.value() += rd;
          p = true;
        }

      if (keyboard_state[SDL_SCANCODE_3])
        {
          m_gradient_r1.value() -= rd;
          m_gradient_r1.value() = astral::t_max(0.0f, m_gradient_r1.value());
          p = true;
        }
      if (keyboard_state[SDL_SCANCODE_4])
        {
          m_gradient_r1.value() += rd;
          p = true;
        }

      if (p)
        {
          std::cout << "Gradient r0 = " << m_gradient_r0.value()
                    << ", r1 = " << m_gradient_r1.value()
                    << "\n";
        }
    }

  if (m_gradient_type.value() == astral::Gradient::sweep)
    {
      float rd(delta * 0.01f);
      bool p(false);

      if (keyboard_state[SDL_SCANCODE_1])
        {
          m_gradient_sweep_factor.value() -= rd;
          p = true;
        }
      if (keyboard_state[SDL_SCANCODE_2])
        {
          m_gradient_sweep_factor.value() += rd;
          p = true;
        }

      if (p)
        {
          std::cout << "Gradient sweep-factor = "
                    << m_gradient_sweep_factor.value()
                    << "\n";
        }
    }

  return frame_ms;
}

void
BrushTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  switch (ev.type)
    {
      case SDL_MOUSEMOTION:
      {
        astral::vec2 p, c(ev.motion.x + ev.motion.xrel,
                          ev.motion.y + ev.motion.yrel);

        /* map from window coordinates to item coordinates */
        if (m_images[m_current_image.value()].m_gradient_transformation_active
            && m_images[m_current_image.value()].m_gradient_transformation_mapping_active)
          {
            p = c;
          }
        else
          {
            astral::Transformation tr;

            tr = m_zoom.transformation().astral_transformation();
            tr.scale(m_scale_pre_rotate.value());
            tr.rotate(m_rotate_angle.value() * ASTRAL_PI / 180.0f);
            tr.scale(m_scale_post_rotate.value());
            tr = tr.inverse();
            p = tr.apply_to_point(c);
          }

        if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
          {
            m_gradient_p0.value() = p;
          }

        if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
          {
            m_gradient_p1.value() = p;
          }
      }
      break;

    case SDL_KEYDOWN:
      {
        switch (ev.key.keysym.sym)
          {
          case SDLK_p:
            {
              const UniformScaleTranslate<float> &tr(m_zoom.transformation());
              astral::c_string gradient_type;

              if (m_gradient_type.value() == astral::Gradient::number_types)
                {
                  gradient_type = "no_gradient";
                }
              else
                {
                  gradient_type = astral::label(m_gradient_type.value());
                }

              std::cout << "initial_camera " << tr.m_translation.x()
                        << ":" << tr.m_translation.y() << ":" << tr.m_scale
                        << " blur_min_scale_factor " << m_blur_min_scale_factor.value()
                        << " initial_max_blur_sample_radius " << m_max_sample_radius.value()
                        << " initial_blur_radius " << m_blur_radius.value()
                        << " initial_render_mode " << label(m_mode.value())
                        << " draw_big_rect " << std::boolalpha << m_draw_big_rect.value()
                        << " with_aa " << std::boolalpha << m_with_aa.value()
                        << " current_image " << m_current_image.value()
                        << " current_colorstop " << m_current_colorstop.value()
                        << " gradient_tile_mode " << astral::label(m_gradient_tile_mode.value())
                        << " gradient_type " << gradient_type
                        << " gradient_p0 " << m_gradient_p0.value().x() << ":" << m_gradient_p0.value().y()
                        << " gradient_p1 " << m_gradient_p1.value().x() << ":" << m_gradient_p1.value().y()
                        << " gradient_r0 " << m_gradient_r0.value()
                        << " gradient_r1 " << m_gradient_r1.value()
                        << " gradient_sweep_factor " << m_gradient_sweep_factor.value()
                        << " filter_mode " << astral::label(m_filter_mode.value())
                        << " mipmap_mode " << m_mipmap_mode.value()
                        << " include_halo " << std::boolalpha << m_include_halo.value()
                        << " layer_scale_factor " << m_scale_factor.value()
                        << " scale_pre_rotate " << m_scale_pre_rotate.value().x() << ":" << m_scale_pre_rotate.value().y()
                        << " scale_post_rotate " << m_scale_post_rotate.value().x() << ":" << m_scale_post_rotate.value().y()
                        << " rotate_angle " << m_rotate_angle.value()
                        << "\n";
            }
            break;

          case SDLK_a:
            m_with_aa.value() = !m_with_aa.value();
            if (!m_with_aa.value())
              {
                std::cout << "NOT applying ";
              }
            else
              {
                std::cout << "Applying ";
              }
            std::cout << "anti-alias\n";
            break;

          case SDLK_i:
            cycle_value(m_current_image.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_images.size());
            std::cout << "Using image " << m_images[m_current_image.value()].m_filename << "\n";
            break;

          case SDLK_b:
            {
              cycle_value(m_mode.value(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          number_modes);
              std::cout << "Mode set to " << label(m_mode.value()) << "\n";
            }
            break;

          case SDLK_n:
            m_include_halo.value() = !m_include_halo.value();
            std::cout << "Draw halo set to: " << m_include_halo.value() << "\n";
            break;

          case SDLK_c:
            cycle_value(m_current_colorstop.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_colorstop_sequences.size());
            std::cout << "Using ColorStopSequence "
                      << m_colorstop_sequences[m_current_colorstop.value()].m_filename << "\n";
            break;

          case SDLK_h:
            {
              cycle_value(m_gradient_tile_mode.value(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::tile_mode_number_modes);

              std::cout << "Gradient tile mode set to "
                        << astral::label(m_gradient_tile_mode.value())
                        << "\n";
            }
            break;

          case SDLK_g:
            {
              astral::c_string label;

              cycle_value(m_gradient_type.value(),
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::Gradient::number_types + 1);
              label = (m_gradient_type.value() == astral::Gradient::number_types) ?
                "no-gradient" :
                astral::label(static_cast<astral::Gradient::type_t>(m_gradient_type.value()));

              std::cout << "Gradient type set to " << label << "\n";
            }
            break;

          case SDLK_r:
            {
              astral::c_string label;

              m_draw_big_rect.value() = !m_draw_big_rect.value();
              label = (m_draw_big_rect.value()) ? "big" : "small";
              std::cout << "Draw " << label << " rect\n";
            }
            break;

          case SDLK_x:
            if (ev.key.keysym.mod & KMOD_CTRL)
              {
                if (m_images[m_current_image.value()].m_gradient_transformation_active)
                  {
                    cycle_value(m_images[m_current_image.value()].m_gradient_transformation.m_x_tile.m_mode,
                                KMOD_SHIFT | KMOD_ALT,
                                astral::tile_mode_number_modes);
                    std::cout << "Gradient x-tile mode for " << m_images[m_current_image.value()].m_filename << " set to "
                              << astral::label(m_images[m_current_image.value()].m_gradient_transformation.m_x_tile.m_mode)
                              << "\n";
                  }
              }
            else
              {
                if (m_images[m_current_image.value()].m_image_transformation_active)
                  {
                    cycle_value(m_images[m_current_image.value()].m_image_transformation.m_x_tile.m_mode,
                                KMOD_SHIFT | KMOD_ALT,
                                astral::tile_mode_number_modes);
                    std::cout << "Image x-tile mode for " << m_images[m_current_image.value()].m_filename << " set to "
                              << astral::label(m_images[m_current_image.value()].m_image_transformation.m_x_tile.m_mode)
                              << "\n";
                  }
              }
            break;

          case SDLK_y:
            if (ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT))
              {
                if (m_images[m_current_image.value()].m_gradient_transformation_active)
                  {
                    cycle_value(m_images[m_current_image.value()].m_gradient_transformation.m_y_tile.m_mode,
                                false,
                                astral::tile_mode_number_modes);
                    std::cout << "Gradient y-tile mode for " << m_images[m_current_image.value()].m_filename << " set to "
                              << astral::label(m_images[m_current_image.value()].m_gradient_transformation.m_y_tile.m_mode)
                              << "\n";
                  }
              }
            else
              {
                if (m_images[m_current_image.value()].m_image_transformation_active)
                  {
                    cycle_value(m_images[m_current_image.value()].m_image_transformation.m_y_tile.m_mode,
                                false,
                                astral::tile_mode_number_modes);
                    std::cout << "Image y-tile mode for " << m_images[m_current_image.value()].m_filename << " set to "
                              << astral::label(m_images[m_current_image.value()].m_image_transformation.m_y_tile.m_mode)
                              << "\n";
                  }
              }
            break;

          case SDLK_SPACE:
            m_draw_detailed_hud = !m_draw_detailed_hud;
            break;

          case SDLK_s:
            if (ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT))
              {
                m_images[m_current_image.value()].m_gradient_transformation_active = !m_images[m_current_image.value()].m_gradient_transformation_active;
                std::cout << "Gradient image_transformation for " << m_images[m_current_image.value()].m_filename
                          << " active = " << m_images[m_current_image.value()].m_gradient_transformation_active
                          << "\n";
              }
            else
              {
                m_images[m_current_image.value()].m_image_transformation_active = !m_images[m_current_image.value()].m_image_transformation_active;
                std::cout << "Image image_transformation for " << m_images[m_current_image.value()].m_filename
                          << " active = " << m_images[m_current_image.value()].m_image_transformation_active
                          << "\n";
              }
            break;

          case SDLK_z:
            if (ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT))
              {
                if (m_images[m_current_image.value()].m_gradient_transformation_active)
                  {
                    m_images[m_current_image.value()].m_gradient_transformation_mapping_active
                      = !m_images[m_current_image.value()].m_gradient_transformation_mapping_active;
                    std::cout << "Gradient image_transformation for " << m_images[m_current_image.value()].m_filename
                              << " transformation active = "
                              << m_images[m_current_image.value()].m_gradient_transformation_mapping_active
                              << "\n";
                  }
              }
            else
              {
                if (m_images[m_current_image.value()].m_image_transformation_active)
                  {
                    m_images[m_current_image.value()].m_image_transformation_mapping_active
                      = !m_images[m_current_image.value()].m_image_transformation_mapping_active;
                    std::cout << "Image image_transformation for " << m_images[m_current_image.value()].m_filename
                              << " transformation active = "
                              << m_images[m_current_image.value()].m_image_transformation_mapping_active
                              << "\n";
                  }
              }
            break;

          case SDLK_f:
            cycle_value(m_filter_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        astral::number_filter_modes);
            std::cout << "Filter mode set to " << astral::label(m_filter_mode.value()) << "\n";
            break;

          case SDLK_m:
            cycle_value(m_mipmap_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        astral::mipmap_chosen + 16);
            std::cout << "Mipmap mode set to ";
            if (m_mipmap_mode.value() < astral::mipmap_chosen)
              {
                enum astral::mipmap_t mip;

                mip = static_cast<enum astral::mipmap_t>(m_mipmap_mode.value());
                std::cout << astral::label(mip);
              }
            else
              {
                std::cout << "FixedLevel " << m_mipmap_mode.value() - astral::mipmap_chosen;
              }
            std::cout << "\n";
            break;

          case SDLK_q:
            m_scale_pre_rotate.value() = m_scale_post_rotate.value() = astral::vec2(1.0f, 1.0f);
            m_rotate_angle.value() = 0.0f;
            break;

          case SDLK_RIGHT:
            ++m_max_sample_radius.value();
            std::cout << "Max blur pixel radius set to: " << m_max_sample_radius.value() << "\n";
            break;

          case SDLK_LEFT:
            m_max_sample_radius.value() = astral::t_max(m_max_sample_radius.value() - 1, 1);
            std::cout << "Max blur pixel radius set to: " << m_max_sample_radius.value() << "\n";
            break;
          }
      }
      break;
    }
  render_engine_gl3_demo::handle_event(ev);
}

astral::Gradient
BrushTest::
generate_gradient(void)
{
  switch (m_gradient_type.value())
    {
    default:
      ASTRALassert(!"Bad gradient type enumeration");
      // fall through

    case astral::Gradient::linear:
      return astral::Gradient(m_colorstop_sequences[m_current_colorstop.value()].m_sequence,
                              m_gradient_p0.value(), m_gradient_p1.value(),
                              m_gradient_tile_mode.value());

    case astral::Gradient::radial_unextended_opaque:
    case astral::Gradient::radial_unextended_clear:
    case astral::Gradient::radial_extended:
      return astral::Gradient(m_colorstop_sequences[m_current_colorstop.value()].m_sequence,
                              m_gradient_p0.value(), m_gradient_r0.value(),
                              m_gradient_p1.value(), m_gradient_r1.value(),
                              m_gradient_tile_mode.value(),
                              astral::Gradient::gradient_extension_type(m_gradient_type.value()));
      break;

    case astral::Gradient::sweep:
      {
        float angle;
        astral::vec2 v;

        v = m_gradient_p1.value() - m_gradient_p0.value();
        angle = astral::t_atan2(v.y(), v.x());
        return astral::Gradient(m_colorstop_sequences[m_current_colorstop.value()].m_sequence,
                                m_gradient_p0.value(), angle,
                                m_gradient_sweep_factor.value(),
                                m_gradient_tile_mode.value());
      }
    }
}

void
BrushTest::
draw_ui_rect(astral::RenderEncoderSurface render_encoder,
             astral::RenderValue<astral::Brush> outer,
             astral::RenderValue<astral::Brush> inner,
             astral::vec2 p)
{
  render_encoder.save_transformation();
  render_encoder.transformation(astral::Transformation().translate(p));
  render_encoder.draw_rect(m_ui_outer_rect, false, outer);
  render_encoder.draw_rect(m_ui_inner_rect, false, inner);
  render_encoder.restore_transformation();
}

void
BrushTest::
draw_hud(astral::RenderEncoderSurface render_encoder,
         float frame_ms, const astral::Transformation &tr)
{
  /* draw the HUD in fixed location */
  std::ostringstream hud_text;

  if (m_draw_detailed_hud)
    {
      hud_text << "[r] Draw big rect:" << std::boolalpha << m_draw_big_rect.value() << "\n"
               << "[i] Current image: " << m_images[m_current_image.value()].m_filename << "\n"
               << "[s] Image Sampler: ";
      if (m_images[m_current_image.value()].m_image_transformation_active)
        {
          hud_text << "On\n"
                   << "\t[x] X-tile mode: "
                   << astral::label(m_images[m_current_image.value()].m_image_transformation.m_x_tile.m_mode)
                   << "\n"
                   << "\t[y] Y-tile mode: "
                   << astral::label(m_images[m_current_image.value()].m_image_transformation.m_y_tile.m_mode)
                   << "\n"
                   << "\t[z] Transformation active on image_transformation: " << std::boolalpha
                   << m_images[m_current_image.value()].m_image_transformation_mapping_active << "\n";
        }
      else
        {
          hud_text << "Off\n";
        }

      hud_text << "[g] Gradient mode: ";
      switch (m_gradient_type.value())
        {
        case astral::Gradient::number_types:
          {
            hud_text << "no-gradient\n";
          }
          break;

        default:
          {
            hud_text << astral::label(static_cast<astral::Gradient::type_t>(m_gradient_type.value())) << "\n"
                     << "\t[h] Gradient Tile Mode: " << astral::label(static_cast<astral::tile_mode_t>(m_gradient_tile_mode.value())) << "\n"
                     << "\t[c] Color Stop Sequence: " << m_colorstop_sequences[m_current_colorstop.value()].m_filename << "\n"
                     << "\t[Middle Mouse Drag] p0 : " << m_gradient_p0.value() << "\n"
                     << "\t[Right Mouse Drag] p1 : " << m_gradient_p1.value() << "\n";
            if (astral::Gradient::is_radial_gradient(m_gradient_type.value()))
              {
                hud_text << "\t[1/2]: r0: " << m_gradient_r0.value() << "\n"
                         << "\t[3/4]: r1: " << m_gradient_r1.value() << "\n";
              }
            hud_text << "\t[ctrl/shift/atl-s] Gradient Sampler: ";
            if (m_images[m_current_image.value()].m_gradient_transformation_active)
              {
                hud_text << "On\n"
                         << "\t\t[ctrl/shift/atl-x] X-tile mode: "
                         << astral::label(m_images[m_current_image.value()].m_gradient_transformation.m_x_tile.m_mode)
                         << "\n"
                         << "\t\t[ctrl/shift/atl-y] Y-tile mode: "
                         << astral::label(m_images[m_current_image.value()].m_gradient_transformation.m_y_tile.m_mode)
                         << "\n"
                         << "\t\t[ctrl/shift/atl-z] Transformation active on image_transformation: " << std::boolalpha
                         << m_images[m_current_image.value()].m_gradient_transformation_mapping_active << "\n";
              }
            else
              {
                hud_text << "Off\n";
              }
          }
          break;
        }

      hud_text << "[b] Mode: ";
      switch (m_mode.value())
        {
        default:
          {
            hud_text << "Direct Brush\n";
          }
          break;

        case blur_mode_alt:
        case blur_mode:
          {
            float R;

            R = astral::compute_singular_values(tr.m_matrix).x() * m_blur_radius.value();
            if (m_mode.value() == blur_mode_alt)
              {
                hud_text << "BlurAlt\n";
              }
            else
              {
                hud_text << "Blur\n";
              }

            hud_text << "\t[up/down arrow]Logical Blur radius: " << m_blur_radius.value() << "\n"
                     << "\tPixel Blur radius: " << R << "\n"
                     << "\t[v/ctrol-v] blur_min_scale_factor: " << m_blur_min_scale_factor.value() << "\n"
                     << "\t[n] include halo: " << std::boolalpha << m_include_halo.value() << "\n"
                     << "\t[left/right arrow]Max Sample Radius (before render at lower resolution): "
                     << m_max_sample_radius.value() << "\n";
          }
          break;

        case layer_mode:
          {
            hud_text << "Draw to Layer"
                     << "\t[up/down arrow]scale factor: " << m_scale_factor.value() << "\n";
          }
          break;
        }
    }
  hud_text << "[space] Detailed HUD:" << std::boolalpha << m_draw_detailed_hud << "\n"
           << "Zoom = " << m_zoom.transformation().m_scale << "\n"
           << "Current Image: " << m_images[m_current_image.value()].m_filename;

  if (m_images[m_current_image.value()].m_image)
    {
      hud_text << ", size = " << m_images[m_current_image.value()].m_image->size();
    }
  hud_text << "\n";

  render_encoder.transformation(astral::Transformation());
  set_and_draw_hud(render_encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, hud_text.str());
}

void
BrushTest::
draw_frame(void)
{
  astral::Brush brush;
  astral::RenderValue<astral::Brush> render_brush;
  astral::Transformation tr, itr;
  astral::RenderEncoderSurface render_encoder;
  float frame_ms;
  astral::c_array<const unsigned int> stats;

  frame_ms = update_smooth_values();
  render_encoder = renderer().begin(render_target(), astral::FixedPointColor_sRGB(0x7F, 0x77, 0x7F, 0xFF));

  /* draw our rect with the brush applied */
  tr = m_zoom.transformation().astral_transformation();
  render_encoder.transformation(tr);

  render_encoder.save_transformation();
  render_encoder.scale(m_scale_pre_rotate.value());
  render_encoder.rotate(m_rotate_angle.value() * ASTRAL_PI / 180.0f);
  render_encoder.scale(m_scale_post_rotate.value());
  itr = render_encoder.transformation();

  brush.m_base_color = astral::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  if (m_images[m_current_image.value()].m_image)
    {
      astral::ImageSampler im;

      if (m_mipmap_mode.value() >= astral::mipmap_chosen)
        {
          astral::MipmapLevel level(m_mipmap_mode.value() - astral::mipmap_chosen);

          im = astral::ImageSampler(*m_images[m_current_image.value()].m_image, level, m_filter_mode.value());
        }
      else
        {
          enum astral::mipmap_t mip;

          mip = static_cast<enum astral::mipmap_t>(m_mipmap_mode.value());
          im = astral::ImageSampler(*m_images[m_current_image.value()].m_image, m_filter_mode.value(), mip);
        }

      if (m_images[m_current_image.value()].m_image_transformation_active)
        {
          im
            .x_tile_mode(m_images[m_current_image.value()].m_image_transformation.m_x_tile.m_mode)
            .y_tile_mode(m_images[m_current_image.value()].m_image_transformation.m_y_tile.m_mode);

          if (m_images[m_current_image.value()].m_image_transformation_mapping_active)
            {
              brush.m_image_transformation = render_encoder.create_value(itr);
            }
        }
      brush.image(render_encoder.create_value(im));
    }

  if (m_gradient_type.value() != astral::Gradient::number_types)
    {
      brush.m_gradient = render_encoder.create_value(generate_gradient());
      if (m_images[m_current_image.value()].m_gradient_transformation_active)
        {
          brush.m_gradient_transformation = render_encoder.create_value(m_images[m_current_image.value()].gradient_transformation(itr));
        }
    }

  render_brush = render_encoder.create_value(brush);

  astral::RenderEncoderBase *encoder(&render_encoder);
  const astral::Rect *rect;
  astral::RenderEncoderImage image_encoder;
  astral::RenderEncoderLayer encoder_layer;

  if (m_draw_big_rect.value())
    {
      rect = &m_images[m_current_image.value()].m_big_rect;
    }
  else
    {
      rect = &m_images[m_current_image.value()].m_rect;
    }

  if (m_mode.value() != direct_to_surface)
    {
      astral::BoundingBox<float> bb(*rect);

      if (m_mode.value() == blur_mode || m_mode.value() == blur_mode_alt)
        {
          const astral::Effect *effect;
          astral::GaussianBlurParameters effect_params;

          effect_params.radius(m_blur_radius.value());
          effect_params.min_render_scale(m_blur_min_scale_factor.value());
          effect_params.max_sample_radius(m_max_sample_radius.value());
          effect_params.include_halo(m_include_halo.value());

          effect = render_encoder.default_effects().m_gaussian_blur.get();
          if (m_mode.value() == blur_mode_alt)
            {
              astral::EffectParameters effect_parameters(effect_params.effect_parameters());
              astral::c_array<const astral::EffectParameters> p(&effect_parameters, 1);
              astral::EffectCollection ef(*effect, p);
              encoder_layer = render_encoder.begin_layer(ef, bb);
            }
          else
            {
              encoder_layer = render_encoder.begin_layer(*effect, effect_params.effect_parameters(), bb);
            }
        }
      else
        {
          encoder_layer = render_encoder.begin_layer(bb, m_scale_factor.value(),
                                                     astral::vec4(1.0f, 1.0f, 1.0f, 0.8f));

        }
      image_encoder = encoder_layer.encoder();
      encoder = &image_encoder;
    }

  encoder->draw_rect(*rect, m_with_aa.value(), render_brush);
  if (m_mode.value() != direct_to_surface)
    {
      render_encoder.end_layer(encoder_layer);
    }

  render_encoder.restore_transformation();

  if (m_gradient_type.value() != astral::Gradient::number_types)
    {
      /* draw our litte UI helpers */
      astral::vec2 p;
      astral::RenderValue<astral::Brush> white, black;

      white = render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f)));
      black = render_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 1.0f)));

      if (m_images[m_current_image.value()].m_gradient_transformation_active
          && m_images[m_current_image.value()].m_gradient_transformation_mapping_active)
        {
          p = m_gradient_p0.value();
        }
      else
        {
          p = itr.apply_to_point(m_gradient_p0.value());
        }

      draw_ui_rect(render_encoder, white, black, p);

      if (m_images[m_current_image.value()].m_gradient_transformation_active
          && m_images[m_current_image.value()].m_gradient_transformation_mapping_active)
        {
          p = m_gradient_p1.value();
        }
      else
        {
          p = itr.apply_to_point(m_gradient_p1.value());
        }
      draw_ui_rect(render_encoder, black, white, p);
    }

  if (!pixel_testing())
    {
      draw_hud(render_encoder, frame_ms, itr);
    }

  stats = renderer().end();
  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());
}

int
main(int argc, char **argv)
{
  BrushTest M;
  return M.main(argc, argv);
}
