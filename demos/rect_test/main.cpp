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
#include <cctype>
#include <string>
#include <fstream>
#include <sstream>

#include <astral/util/ostream_utility.hpp>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/renderer/item_path.hpp>

#include "sdl_demo.hpp"
#include "read_path.hpp"
#include "render_engine_gl3_demo.hpp"
#include "PanZoomTracker.hpp"
#include "simple_time.hpp"
#include "UniformScaleTranslate.hpp"
#include "cycle_value.hpp"
#include "ImageLoader.hpp"
#include "command_line_list.hpp"
#include "text_helper.hpp"
#include "demo_macros.hpp"

class RectTest:public render_engine_gl3_demo
{
public:
  RectTest(void);

protected:
  virtual
  void
  init_gl(int, int) override final;

  virtual
  void
  draw_frame(void) override final;

  virtual
  void
  handle_event(const SDL_Event &ev) override final;

private:
  enum
    {
      basic_hud,
      detail_level1_hud,
      detail_level2_hud,
      detail_level3_hud,

      number_hud_modes,
    };

  enum show_mode_t
    {
      show_rect_white,
      show_rect_image,
      show_rect_image_sparse,
      show_draw_image,
      show_draw_image_sparse,

      number_show_modes
    };

  astral::reference_counted_ptr<const astral::Image>
  image(enum show_mode_t v)
  {
    switch (v)
      {
      case show_rect_image:
      case show_draw_image:
        return m_full_image;

      case show_rect_image_sparse:
      case show_draw_image_sparse:
        return m_sparse_image;

      default:
        return astral::reference_counted_ptr<const astral::Image>();
      }
  }

  static
  bool
  use_draw_image(enum show_mode_t v)
  {
    return (v == show_draw_image || v == show_draw_image_sparse);
  }

  astral::vec4
  base_color(void)
  {
    return m_transparent.value() ?
      astral::vec4(1.0f, 1.0f, 1.0f, 0.5f) :
      astral::vec4(1.0f, 1.0f, 1.0f, 1.0f);
  }

  astral::SubImage
  sub_image(const astral::Image &image, bool use_sub)
  {
    astral::SubImage v(image);

    if (use_sub)
      {
        astral::uvec2 sz;

        sz = v.m_size;
        v.m_min_corner += sz / 8;
        v.m_size -= sz / 4;
      }

    return v;
  }

  static
  astral::c_string
  label(enum show_mode_t v)
  {
    const static astral::c_string labels[number_show_modes] =
      {
        [show_rect_white] = "show_rect_white",
        [show_rect_image] = "show_rect_image",
        [show_rect_image_sparse] = "show_rect_image_sparse",
        [show_draw_image] = "show_draw_image",
        [show_draw_image_sparse] = "show_draw_image_sparse",
      };

    ASTRALassert(v < number_show_modes);
    return labels[v];
  }

  static
  astral::c_string
  return_not_on_false(bool b)
  {
    return (b) ? "" : "not ";
  }

  float
  update_smooth_values(void);

  void
  reset_zoom_transformation(PanZoomTrackerSDLEvent *p);

  void
  draw_hud(astral::RenderEncoderSurface encoder, float frame_ms);

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_image_file;

  enumerated_command_line_argument_value<enum show_mode_t> m_show_mode;
  command_line_argument_value<bool> m_with_aa, m_subimage;
  command_line_argument_value<bool> m_transparent;
  command_line_argument_value<unsigned int> m_mipmap_level;
  enumerated_command_line_argument_value<enum astral::filter_t> m_filter;
  command_line_argument_value<float> m_rotate_angle;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;

  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  astral::reference_counted_ptr<astral::Image> m_full_image;
  astral::reference_counted_ptr<astral::Image> m_sparse_image;

  astral::ShaderSet::RectSideAAList m_aa_list;
  unsigned int m_num_miplevels;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;
  PanZoomTrackerSDLEvent m_zoom;

  astral::Rect m_rect;

  unsigned int m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
};

//////////////////////////////////////////
//  methods
RectTest::
RectTest(void):
  m_demo_options("Demo Options", *this),
  m_image_file("", "image", "if non-empty use the named file as an image source, otherwise use a checkerboard pattern", *this),
  m_show_mode(show_rect_white,
              enumerated_string_type<enum show_mode_t>(&label, number_show_modes),
              "show_mode",
              "Mode specifying what and how to draw",
              *this),
  m_with_aa(false, "with_aa", "When in drawing mode that uses RenderEncoderBase::draw_image(), draw with anti-aliasing", *this),
  m_subimage(false, "subimage", "if true use a sub-image of the image", *this),
  m_transparent(false, "transparent", "if true, draw the rect transparenty", *this),
  m_mipmap_level(0, "mipmap_level", "what mipmap level to use from the image", *this),
  m_filter(astral::filter_nearest,
           enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
           "filter",
           "filter to apply to the image when drawn",
           *this),
  m_rotate_angle(25.0f * ASTRAL_PI / 180.0f, "angle", "Angle by which to rotate the rect", *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera", "initial position of camera", *this),
  m_frame_time_average(1000u),
  m_hud_mode(basic_hud)
{
  std::cout << "Controls:"
            << "\n\t1: toggle x-min side anti-aliased"
            << "\n\t2: toggle y-min side anti-aliased"
            << "\n\t3: toggle x-max side anti-aliased"
            << "\n\t4: toggle y-max side anti-aliased"
            << "\n\t5: toggle anti-aliased image-draw"
            << "\n\t9/0 increase/decrease angle of rect"
            << "\n\ti: cycle through showing image"
            << "\n\tl: cycle through different mipmap levels"
            << "\n\ts: toggle using a sub-image"
            << "\n\tf: cycle through different filters"
            << "\n\tt: toggle transparency"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n\tRight Mouse: move path"
            << "\n\tMiddle Mouse: move clip-path"
            << "\n";
}

void
RectTest::
reset_zoom_transformation(PanZoomTrackerSDLEvent *p)
{
  /* Initialize zoom location to be identity */
  p->transformation(UniformScaleTranslate<float>());
}

void
RectTest::
init_gl(int w, int h)
{
  astral::vec2 wh(w, h);

  m_prev_stats.resize(renderer().stats_labels().size(), 0);
  m_rect.m_min_point = astral::vec2(0.0f, 0.0f);
  m_rect.m_max_point = wh;

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  /* create the full image */
  astral::RenderEncoderImage image_encoder;
  astral::reference_counted_ptr<ImageLoader> pixels;
  astral::ivec2 image_size(w, h);

  renderer().begin(astral::colorspace_srgb);
  image_encoder = renderer().encoder_image(image_size);

  if (!m_image_file.value().empty())
    {
      pixels = ImageLoader::create(m_image_file.value());
    }

  if (pixels && pixels->non_empty())
    {
      astral::uvec2 image_dims;
      astral::reference_counted_ptr<astral::Image> im;

      image_dims = pixels->dimensions();
      im = engine().image_atlas().create_image(image_dims);
      im->set_pixels(0,
                     astral::ivec2(0, 0),
                     astral::ivec2(image_dims),
                     image_dims.x(),
                     pixels->mipmap_pixels(0));

      astral::ImageSampler sampler(*im, astral::MipmapLevel(0), astral::filter_nearest,
                                   astral::color_post_sampling_mode_direct,
                                   astral::tile_mode_repeat,
                                   astral::tile_mode_repeat);

      image_encoder.draw_rect(astral::Rect().min_point(0, 0).max_point(w, h), false,
                              image_encoder.create_value(astral::Brush().image(image_encoder.create_value(sampler))));
    }
  else
    {
      astral::ivec2 square_count(8, 8);
      astral::vec2 square_size;
      square_size = astral::vec2(image_size) / astral::vec2(square_count);

      astral::vecN<astral::RenderValue<astral::Brush>, 2> b;
      b[0] = image_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 1.0f)));
      b[1] = image_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 1.0f, 1.0f, 1.0f)));

      for (int y = 0; y < square_count.y(); ++y)
        {
          for (int x = 0; x < square_count.x(); ++x)
            {
              astral::Rect R;
              unsigned int idx;

              R.m_min_point = astral::vec2(x, y) * square_size;
              R.m_max_point = R.m_min_point + square_size;
              idx = (x + y) & 1u;

              image_encoder.draw_rect(R, false, b[idx]);
            }
        }
    }

  image_encoder.finish();

  m_num_miplevels = astral::t_min(astral::uint32_log2_floor(w), astral::uint32_log2_floor(h));
  m_full_image = image_encoder.image_with_mips(m_num_miplevels);

  renderer().end();

  std::vector<astral::reference_counted_ptr<const astral::ImageMipElement>> sparse_mip_chain;
  for (const astral::reference_counted_ptr<const astral::ImageMipElement> &mip_ptr : m_full_image->mip_chain())
    {
      /* create the sparse image as having every other mip-element in the root */
      const astral::ImageMipElement &mip(*mip_ptr);
      astral::uvec2 cnt(mip.tile_count());
      std::vector<astral::uvec2> empty_tiles, white_tiles;
      std::vector<std::pair<astral::uvec2, astral::ImageAtlas::TileElement>> shared_tiles;

      for (unsigned int y = 0, idx = 0; y < cnt.y(); ++y)
        {
          for (unsigned int x = 0; x < cnt.x(); ++x, ++idx)
            {
              if (idx == 3u)
                {
                  idx = 0u;
                }

              switch (idx)
                {
                case 0:
                  empty_tiles.push_back(astral::uvec2(x, y));
                  break;

                case 1:
                  white_tiles.push_back(astral::uvec2(x, y));
                  break;

                case 2:
                  {
                    std::pair<astral::uvec2, astral::ImageAtlas::TileElement> E;

                    E.second.m_src = &mip;
                    E.first = E.second.m_tile = astral::uvec2(x, y);
                    shared_tiles.push_back(E);
                  }
                  break;
                }
            }
        }

      astral::reference_counted_ptr<const astral::ImageMipElement> mm;
      mm = engine().image_atlas().create_mip_element(astral::uvec2(mip.size()), 2,
                                                     astral::make_c_array(empty_tiles),
                                                     astral::make_c_array(white_tiles),
                                                     astral::make_c_array(shared_tiles));

      sparse_mip_chain.push_back(mm);
    }
  m_sparse_image = engine().image_atlas().create_image(astral::make_c_array(sparse_mip_chain));

  m_zoom.transformation(m_initial_camera.value());
}

void
RectTest::
draw_hud(astral::RenderEncoderSurface encoder, float frame_ms)
{
  static const enum astral::Renderer::renderer_stats_t vs[] =
    {
      astral::Renderer::number_offscreen_render_targets,
      astral::Renderer::number_non_degenerate_virtual_buffers,
      astral::Renderer::number_virtual_buffers,
    };

  static const enum astral::RenderBackend::render_backend_stats_t bvs[] =
    {
      astral::RenderBackend::stats_number_draws,
      astral::RenderBackend::stats_vertices,
      astral::RenderBackend::stats_render_targets,
    };

  static const unsigned int gvs[] =
    {
      astral::gl::RenderEngineGL3::number_draws,
      astral::gl::RenderEngineGL3::number_program_binds,
      astral::gl::RenderEngineGL3::number_staging_buffers,
    };

  astral::c_array<const enum astral::Renderer::renderer_stats_t> vs_p;
  astral::c_array<const enum astral::RenderBackend::render_backend_stats_t> bvs_p;
  astral::c_array<const unsigned int> gvs_p;
  std::ostringstream ostr;

  if (m_hud_mode >= detail_level1_hud)
    {
      bvs_p = MAKE_C_ARRAY(bvs, const astral::RenderBackend::render_backend_stats_t);
    }

  if (m_hud_mode >= detail_level2_hud)
    {
      gvs_p = MAKE_C_ARRAY(gvs, const unsigned int);
    }

  if (m_hud_mode >= detail_level3_hud)
    {
      vs_p = MAKE_C_ARRAY(vs, const astral::Renderer::renderer_stats_t);
    }

  astral::ivec2 mouse_pos;
  GetMouseState(&mouse_pos.x(), &mouse_pos.y());

  ostr << "Resolution = " << dimensions() << "\n"
       << "Zoom = " << m_zoom.transformation().m_scale
       << ", Translation = " << m_zoom.transformation().m_translation << "\n"
       << "Mouse at " << mouse_pos << "\n"
       << "\n"
       << "[t] transparent = " << std::boolalpha << m_transparent.value() << "\n"
       << "[s] subimage = " << std::boolalpha << m_subimage.value() << "\n"
       << "[f] filter = " << astral::label(m_filter.value()) << "\n"
       << "[9/0] angle = " << m_rotate_angle.value()
       << "[i] Mode = " << label(m_show_mode.value()) << "\n";

  if (!use_draw_image(m_show_mode.value()))
    {
      ostr << "\t[1] minx-side is " << return_not_on_false(m_aa_list.value(astral::RectEnums::minx_side)) << "anti-aliased\n"
           << "\t[2] miny-side is " << return_not_on_false(m_aa_list.value(astral::RectEnums::miny_side)) << "anti-aliased\n"
           << "\t[3] maxx-side is " << return_not_on_false(m_aa_list.value(astral::RectEnums::maxx_side)) << "anti-aliased\n"
           << "\t[4] maxy-side is " << return_not_on_false(m_aa_list.value(astral::RectEnums::maxy_side)) << "anti-aliased\n";
    }
  else
    {
      ostr << "\t[5] with_aa = " << std::boolalpha << m_with_aa.value() << "\n";
    }

  ostr << "\nAverage over " << m_frame_time_average.interval_ms() << " ms: "
       << m_frame_time_average.average_elapsed_ms()
       << m_frame_time_average.parity_string() << "\n";

  /* draw the HUD in fixed location */
  encoder.transformation(astral::Transformation());
  set_and_draw_hud(encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, ostr.str(),
                   vs_p, bvs_p, gvs_p);
}

void
RectTest::
draw_frame(void)
{
  float frame_ms;
  astral::RenderEncoderSurface render_encoder;
  astral::c_array<const unsigned int> stats;

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();

  render_encoder = renderer().begin(render_target(), astral::colorspace_srgb, astral::u8vec4(125, 100, 127, 255));
    {
      render_encoder.transformation(m_zoom.transformation().astral_transformation());
      render_encoder.translate(m_rect.center_point());
      render_encoder.rotate(m_rotate_angle.value());
      render_encoder.translate(-m_rect.center_point());

      if (!use_draw_image(m_show_mode.value()))
        {
          astral::RenderValue<astral::Brush> brush;
          astral::reference_counted_ptr<const astral::Image> im;
          astral::RenderValue<astral::ImageSampler> sampler;

          im = image(m_show_mode.value());
          if (im)
            {
              sampler = render_encoder.create_value(astral::ImageSampler(sub_image(*im, m_subimage.value()),
                                                                         astral::MipmapLevel(m_mipmap_level.value()),
                                                                         m_filter.value()));
            }
          brush = render_encoder.create_value(astral::Brush().base_color(base_color()).image(sampler));

          render_encoder.draw_rect(*render_encoder.default_shaders().dynamic_rect_shader(m_aa_list), m_rect, brush);
        }
      else
        {
          astral::reference_counted_ptr<const astral::Image> im;

          im = image(m_show_mode.value());
          ASTRALassert(im);
          render_encoder.draw_image(sub_image(*im, m_subimage.value()),
                                    astral::MipmapLevel(m_mipmap_level.value()),
                                    astral::RenderEncoderBase::ImageDraw()
                                    .filter(m_filter.value())
                                    .with_aa(m_with_aa.value())
                                    .base_color(base_color()));
        }

      if (!pixel_testing())
        {
          draw_hud(render_encoder, frame_ms);
        }
    }
  stats = renderer().end();

  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());
}

float
RectTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, angle_delta;

  ASTRALassert(keyboard_state != nullptr);
  return_value = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }

  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
    }

  angle_delta = 0.0025f * delta;

  if ((keyboard_state[SDL_SCANCODE_0] || keyboard_state[SDL_SCANCODE_9]))
    {
      if (keyboard_state[SDL_SCANCODE_0])
        {
          m_rotate_angle.value() -= angle_delta;
          if (m_rotate_angle.value() < 0.0f)
            {
              m_rotate_angle.value() += 2.0f * ASTRAL_PI;
            }
        }
      else
        {
          m_rotate_angle.value() += angle_delta;
          if (m_rotate_angle.value() > 2.0f * ASTRAL_PI)
            {
              m_rotate_angle.value() -= 2.0f * ASTRAL_PI;
            }
        }
      std::cout << "Angle set to: " << m_rotate_angle.value() * (180.0f / ASTRAL_PI) << " degrees\n";
    }

  return return_value;
}

void
RectTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev, SDL_BUTTON_LEFT);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_q:
          m_rotate_angle.value() = 45.0f * ASTRAL_PI / 180.0f;
          break;

        case SDLK_SPACE:
          cycle_value(m_hud_mode, false, number_hud_modes);
          break;

        case SDLK_1:
          m_aa_list.value(astral::RectEnums::minx_side, !m_aa_list.value(astral::RectEnums::minx_side));
          break;

        case SDLK_2:
          m_aa_list.value(astral::RectEnums::miny_side, !m_aa_list.value(astral::RectEnums::miny_side));
          break;

        case SDLK_3:
          m_aa_list.value(astral::RectEnums::maxx_side, !m_aa_list.value(astral::RectEnums::maxx_side));
          break;

        case SDLK_4:
          m_aa_list.value(astral::RectEnums::maxy_side, !m_aa_list.value(astral::RectEnums::maxy_side));
          break;

        case SDLK_5:
          m_with_aa.value() = !m_with_aa.value();
          break;

        case SDLK_t:
          m_transparent.value() = !m_transparent.value();
          break;

        case SDLK_l:
          cycle_value(m_mipmap_level.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      m_num_miplevels);
          std::cout << "Mipmap level set to " << m_mipmap_level.value() << "\n";
          break;

        case SDLK_f:
          cycle_value(m_filter.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_filter_modes);
          std::cout << "Filter set to " << astral::label(m_filter.value()) << "\n";
          break;

        case SDLK_s:
          m_subimage.value() = !m_subimage.value();
          std::cout << "SubImage set to " << std::boolalpha << m_subimage.value() << "\n";
          break;

        case SDLK_i:
          cycle_value(m_show_mode.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      number_show_modes);
          std::cout << "Show mode set to " << label(m_show_mode.value()) << "\n";
          break;

        }
    }
  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  RectTest M;
  return M.main(argc, argv);
}
