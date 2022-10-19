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
#include <dirent.h>
#include <stdlib.h>

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>

#include "render_engine_gl3_demo.hpp"
#include "sdl_demo.hpp"
#include "read_path.hpp"
#include "command_line_list.hpp"
#include "ImageLoader.hpp"
#include "PanZoomTracker.hpp"
#include "cycle_value.hpp"

class ClipBlitTest:public render_engine_gl3_demo
{
public:
  ClipBlitTest(void);

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
  class PerImage
  {
  public:
    PerImage(const std::string &filename,
             const astral::reference_counted_ptr<astral::Image> &image):
      m_filename(filename),
      m_image(image),
      m_image_sampler(*m_image)
    {
    }

    std::string m_filename;
    astral::reference_counted_ptr<astral::Image> m_image;
    astral::ImageSampler m_image_sampler;
  };

  void
  draw_ui_rect(astral::RenderEncoderSurface dst,
               astral::vec2 position,
               astral::vec4 color);

  static
  bool
  load_path(const std::string &filename, astral::Path *dst)
  {
    std::ifstream path_file(filename.c_str());
    if (path_file)
      {
        read_path(dst, path_file);
        return true;
      }

    return false;
  }

  command_separator m_demo_options;
  command_line_list_images m_loaded_images;
  command_line_argument_value<std::string> m_path_file;
  enumerated_command_line_argument_value<enum astral::mask_channel_t> m_clip_by_image;
  enumerated_command_line_argument_value<enum astral::filter_t> m_mask_filter_mode;
  enumerated_command_line_argument_value<enum astral::mipmap_t> m_mask_mipmap_mode;
  enumerated_command_line_argument_value<enum astral::mask_type_t> m_mask_type;
  command_line_argument_value<unsigned int> m_clip_in_choice, m_clip_out_choice, m_mask_choice;
  command_line_argument_value<astral::vec2> m_clip_in_min_corner, m_clip_out_min_corner;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_mask_transformation;

  std::vector<PerImage> m_images;
  astral::Path m_path;

  PanZoomTrackerSDLEvent m_mask_zoom;
};

//////////////////////////////////
// ClipBlitTest methods
ClipBlitTest::
ClipBlitTest(void):
  m_demo_options("Demo Options", *this),
  m_loaded_images("add_image", "Add an image to the image pool", *this),
  m_path_file("", "path", "File from which to read the path", *this),
  m_clip_by_image(astral::number_mask_channel,
                  enumerated_string_type<enum astral::mask_channel_t>(&astral::label, astral::number_mask_channel)
                  .add_entry("path", astral::number_mask_channel, ""),
                  "clip_by", "specifies if to clip against path of image and what image channel",
                  *this),
  m_mask_filter_mode(astral::filter_linear,
                     enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                     "mask_filter_mode",
                     "filter to apply to mask used for clipping",
                     *this),
  m_mask_mipmap_mode(astral::mipmap_ceiling,
                     enumerated_string_type<enum astral::mipmap_t>(&astral::label, astral::number_mipmap_modes),
                     "mask_mipmap_mode",
                     "mipmap mode to apply to mask used for clipping",
                     *this),
  m_mask_type(astral::mask_type_distance_field,
              enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type),
              "mask_type",
              "specifies the interpretation of pixels of the mask",
              *this),
  m_clip_in_choice(0, "clip_in_choice", "a value of i for 0 <= i < N where N is "
                   "the number of loaded images to use the (i+1)'th image loaded "
                   "for the content to be clipped-in by the mask",
                   *this),
  m_clip_out_choice(0, "clip_out_choice", "a value of i for 0 <= i < N where N is "
                   "the number of loaded images to use the (i+1)'th image loaded "
                   "for the content to be clipped-out by the mask",
                   *this),
  m_mask_choice(0, "mask_choice", "a value of i for 0 <= i < N where N is "
                "the number of loaded images to use the (i+1)'th image loaded "
                "to speciy the image to be used as a mask", *this),
  m_clip_in_min_corner(astral::vec2(0.0f, 0.0f), "clip_in_min_corner", "location of upper-left corner of clipped-in content", *this),
  m_clip_out_min_corner(astral::vec2(0.0f, 0.0f), "clip_out_min_corner", "location of upper-left corner of clipped-out content", *this),
  m_initial_mask_transformation(UniformScaleTranslate<float>(), "initial_mask_transformation", "Initial transformation to apply to mask", *this)
{
  std::cout << "Controls:\n"
            << "\ti: cycle through clip-in image\n"
            << "\to: cycle through clip-out image\n"
            << "\tp: cycle through mask image\n"
            << "\tc: cycle both clip-in and clip-out image\n"
            << "\tf: cycle through filter applied to mask\n"
            << "\tm: cycle through mipmap mode applied to mask\n"
            << "\tx: cycle through mask type\n"
            << "\ty: cycle through clipping by image channel\n"
            << "\tLeft button draw: move mask image on screen\n"
            << "\tMiddle button draw: move clip-in image on screen\n"
            << "\tRight button draw: move clip-out image on screen\n"
            << "\tHold Left Mouse button then drag: zoom in/out mask image\n";
}

void
ClipBlitTest::
init_gl(int, int)
{
  for (const auto &e : m_loaded_images.elements())
    {
      m_images.push_back(PerImage(e.m_filename,
                                  e.m_loaded_value->create_image(engine().image_atlas())));

      std::cout << "Loaded image " << m_images.back().m_filename << " of size "
                << m_images.back().m_image->size().x() << "x"
                << m_images.back().m_image->size().y() << "\n";
    }
  m_loaded_images.clear();

  if (m_images.empty())
    {
      std::cout << "Need Image to run the demo!\n";
      end_demo(0);
      return;
    }

  if (!load_path(m_path_file.value(), &m_path))
    {
      const char *default_path =
        "[ (50.0, 35.0) [[(60.0, 50.0) ]] (70.0, 35.0)\n"
        "arc 180 (70.0, -100.0)\n"
        "[[ (60.0, -150.0) (30.0, -50.0) ]]\n"
        "(0.0, -100.0) arc 90 ]\n"
        "{ (200, 200) (400, 200) (400, 400) (200, 400) }\n"
        "[ (-50, 100) (0, 200) (100, 300) (150, 325) (150, 100) ]\n"
        "{ (300 300) }\n";

      read_path(&m_path, default_path);
    }

  m_mask_zoom.transformation(m_initial_mask_transformation.value());
}

void
ClipBlitTest::
handle_event(const SDL_Event &ev)
{
  m_mask_zoom.handle_event(ev);
  switch (ev.type)
    {
    case SDL_MOUSEMOTION:
      {
        astral::vec2 p(ev.motion.x + ev.motion.xrel,
                       ev.motion.y + ev.motion.yrel);

        if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_MIDDLE))
          {
            m_clip_in_min_corner.value() = p;
            //std::cout << "Set clip-in corner to " << p << "\n";
          }

        if (ev.motion.state & SDL_BUTTON(SDL_BUTTON_RIGHT))
          {
            m_clip_out_min_corner.value() = p;
            //std::cout << "Set clip-out corner to " << p << "\n";
          }
      }
      break;

    case SDL_KEYDOWN:
      {
        switch (ev.key.keysym.sym)
          {
          case SDLK_i:
            cycle_value(m_clip_in_choice.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_images.size());
            std::cout << "ClipIn image set to " << m_images[m_clip_in_choice.value()].m_filename << "\n";
            break;

          case SDLK_o:
            cycle_value(m_clip_out_choice.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_images.size());
            std::cout << "ClipOut image set to " << m_images[m_clip_out_choice.value()].m_filename << "\n";
            break;

          case SDLK_p:
            cycle_value(m_mask_choice.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_images.size());
            std::cout << "Mask image set to " << m_images[m_mask_choice.value()].m_filename << "\n";
            break;

          case SDLK_c:
            cycle_value(m_clip_in_choice.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_images.size());
            cycle_value(m_clip_out_choice.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_images.size());
            std::cout << "ClipOut image set to " << m_images[m_clip_out_choice.value()].m_filename << "\n"
                      << "ClipIn image set to " << m_images[m_clip_in_choice.value()].m_filename << "\n";
            break;

          case SDLK_f:
            cycle_value(m_mask_filter_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        astral::number_filter_modes);
            std::cout << "Filter mode set to " << astral::label(m_mask_filter_mode.value()) << "\n";
            break;

          case SDLK_m:
            cycle_value(m_mask_mipmap_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        astral::number_mipmap_modes);
            std::cout << "Mipmap mode set to " << astral::label(m_mask_mipmap_mode.value()) << "\n";
            break;

          case SDLK_x:
            cycle_value(m_mask_type.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        astral::number_mask_type);
            std::cout << "Mask mode set to: " << astral::label(m_mask_type.value()) << "\n";
            break;

          case SDLK_y:
            cycle_value(m_clip_by_image.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        astral::number_mask_channel + 1);
            if (m_clip_by_image.value() != astral::number_mask_channel)
              {
                std::cout << "Clip to image channel:" << astral::label(m_clip_by_image.value()) << "\n";
              }
            else
              {
                std::cout << "Clip to clip-path\n";
              }
            break;
          }
      }
    }
  render_engine_gl3_demo::handle_event(ev);
}

void
ClipBlitTest::
draw_ui_rect(astral::RenderEncoderSurface dst,
             astral::vec2 position,
             astral::vec4 color)
{
  dst.draw_rect(astral::Rect()
                .min_point(position.x() - 15.0f, position.y() - 15.0f)
                .max_point(position.x() + 15.0f, position.y() + 15.0f),
                false,
                dst.create_value(astral::Brush().base_color(color)));
}

void
ClipBlitTest::
draw_frame(void)
{
  astral::Transformation tr;
  astral::RenderEncoderSurface render_encoder;
  astral::RenderClipNode clip_encoders;
  astral::Rect clip_in_rect, clip_out_rect;
  astral::vec4 mask_color(0.0f, 0.0f, 1.0f, 0.5f);
  astral::vec2 mask_min_pt, mask_max_pt;

  render_encoder = renderer().begin(render_target());

  tr = m_mask_zoom.transformation().astral_transformation();

  clip_in_rect
    .min_point(m_clip_in_min_corner.value())
    .size(astral::vec2(m_images[m_clip_in_choice.value()].m_image->size()));

  clip_out_rect
    .min_point(m_clip_out_min_corner.value())
    .size(astral::vec2(m_images[m_clip_out_choice.value()].m_image->size()));

  if (m_clip_by_image.value() != astral::number_mask_channel)
    {
      astral::ScaleTranslate pixel_tr_mask;
      astral::ScaleTranslate mask_tr_pixel;
      astral::MaskDetails mask_details;

      pixel_tr_mask.m_translate = m_mask_zoom.transformation().m_translation;
      pixel_tr_mask.m_scale = astral::vec2(m_mask_zoom.transformation().m_scale);

      mask_details.m_mask = m_images[m_mask_choice.value()].m_image;
      mask_details.m_min_corner = astral::vec2(0.0f, 0.0f);
      mask_details.m_size = astral::vec2(m_images[m_mask_choice.value()].m_image->size());
      mask_details.m_mask_channel = m_clip_by_image.value();
      mask_details.m_mask_type = m_mask_type.value();
      mask_details.m_mask_transformation_pixel = pixel_tr_mask.inverse();

      clip_encoders = render_encoder.begin_clip_node_pixel(astral::RenderEncoderBase::clip_node_both,
                                                           mask_details,
                                                           astral::BoundingBox<float>(clip_in_rect),
                                                           astral::BoundingBox<float>(clip_out_rect),
                                                           m_mask_filter_mode.value());
      mask_min_pt = astral::vec2(0.0f, 0.0f);
      mask_max_pt = astral::vec2(m_images[m_mask_choice.value()].m_image->size());
    }
  else
    {
      astral::FillParameters fill_params;
      astral::FillMaskProperties mask_params;
      astral::MaskDetails mask_details;
      astral::CombinedPath P(m_path, tr.m_translate, tr.m_matrix);

      render_encoder.generate_mask(P, fill_params, mask_params, astral::mask_type_distance_field, &mask_details);

      clip_encoders = render_encoder.begin_clip_node_pixel(astral::RenderEncoderBase::clip_node_both,
                                                           mask_details,
                                                           astral::BoundingBox<float>(clip_in_rect),
                                                           astral::BoundingBox<float>(clip_out_rect),
                                                           m_mask_filter_mode.value());

      mask_min_pt = m_path.bounding_box().min_point();
      mask_max_pt = m_path.bounding_box().max_point();
    }

  clip_encoders.clip_in().translate(m_clip_in_min_corner.value().x(), m_clip_in_min_corner.value().y());
  clip_encoders.clip_in().draw_rect(astral::Rect()
                                    .min_point(0.0f, 0.0f)
                                    .size(clip_in_rect.size()),
                                    false,
                                    render_encoder.create_value(astral::Brush().image(render_encoder.create_value(astral::ImageSampler(*m_images[m_clip_in_choice.value()].m_image)))));

  clip_encoders.clip_out().translate(m_clip_out_min_corner.value().x(), m_clip_out_min_corner.value().y());
  clip_encoders.clip_out().draw_rect(astral::Rect()
                                     .min_point(0.0f, 0.0f)
                                     .size(clip_out_rect.size()),
                                     false,
                                     render_encoder.create_value(astral::Brush().image(render_encoder.create_value(astral::ImageSampler(*m_images[m_clip_out_choice.value()].m_image)))));

  render_encoder.end_clip_node(clip_encoders);

  astral::vec4 clip_in_color(1.0f, 0.0f, 0.0f, 0.5f);
  astral::vec4 clip_out_color(0.0f, 1.0f, 0.0f, 0.5f);

  draw_ui_rect(render_encoder, tr.apply_to_point(mask_min_pt), mask_color);
  draw_ui_rect(render_encoder, tr.apply_to_point(mask_max_pt), mask_color);

  draw_ui_rect(render_encoder, clip_in_rect.m_min_point, clip_in_color);
  draw_ui_rect(render_encoder, clip_in_rect.m_max_point, clip_in_color);

  draw_ui_rect(render_encoder, clip_out_rect.m_min_point, clip_out_color);
  draw_ui_rect(render_encoder, clip_out_rect.m_max_point, clip_out_color);

  renderer().end();
}

int
main(int argc, char **argv)
{
  ClipBlitTest M;
  return M.main(argc, argv);
}
