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
#include <stdlib.h>

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>

#include "command_line_list.hpp"
#include "render_engine_gl3_demo.hpp"
#include "sdl_demo.hpp"
#include "PanZoomTracker.hpp"
#include "simple_time.hpp"
#include "ImageLoader.hpp"
#include "cycle_value.hpp"

class TestImageAtlas:public render_engine_gl3_demo
{
public:
  TestImageAtlas(void);

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
  enum sort_t
    {
      unsorted,
      sort_by_area,
      sort_by_width,
      sort_by_height,
      sort_by_perimiter,
      sort_random
    };

  class PerImage
  {
  public:
    explicit
    PerImage(const astral::reference_counted_ptr<ImageLoader> &image_data,
             const std::string &filename);

    explicit
    PerImage(astral::uvec2 sz);

    void
    create_image(TestImageAtlas *p);

    void
    load_padding(int LOD, astral::ivec2 wh, astral::c_array<const astral::u8vec4> pixels);

    std::string m_filename;
    astral::reference_counted_ptr<ImageLoader> m_image_data;
    astral::ivec2 m_dims;
    astral::Rect m_rect;
    int m_area;
    astral::reference_counted_ptr<astral::Image> m_image;
    unsigned int m_num_mips;
  };

  class SortByArea
  {
  public:
    explicit
    SortByArea(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerImage &lhs, const PerImage &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_area < rhs.m_area):
        (lhs.m_area > rhs.m_area);
    }

  private:
    bool m_bad_ordering;
  };

  class SortByWidth
  {
  public:
    explicit
    SortByWidth(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerImage &lhs, const PerImage &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_dims.x() > rhs.m_dims.x()):
        (lhs.m_dims.x() > rhs.m_dims.x());
    }

  private:
    bool m_bad_ordering;
  };

  class SortByHeight
  {
  public:
    explicit
    SortByHeight(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerImage &lhs, const PerImage &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_dims.y() < rhs.m_dims.y()):
        (lhs.m_dims.y() > rhs.m_dims.y());
    }

  private:
    bool m_bad_ordering;
  };

  class SortByPerimiter
  {
  public:
    explicit
    SortByPerimiter(bool bad_ordering):
      m_bad_ordering(bad_ordering)
    {}

    bool
    operator()(const PerImage &lhs, const PerImage &rhs) const
    {
      return m_bad_ordering ?
        (lhs.m_dims.x() + lhs.m_dims.y() < rhs.m_dims.x() + rhs.m_dims.y()):
        (lhs.m_dims.x() + lhs.m_dims.y() > rhs.m_dims.x() + rhs.m_dims.y());
    }

  private:
    bool m_bad_ordering;
  };

  void
  create_images(void);

  command_separator m_demo_options;
  command_line_list_images m_image_list;
  command_line_argument_value<unsigned int> m_recreate_images;
  command_line_argument_value<unsigned int> m_recreate_images_stride;
  command_line_argument_value<unsigned int> m_duplicate_images;
  enumerated_command_line_argument_value<enum sort_t> m_sort_images;
  command_line_argument_value<bool> m_bad_sort_order;
  command_line_argument_value<bool> m_time_image_creation_only;

  std::vector<PerImage> m_images;
  unsigned int m_current_image;
  astral::reference_counted_ptr<const astral::gl::MaterialShaderGL3> m_atlas_brush;
  astral::Rect m_atlas_rect;

  enum astral::filter_t m_filter_mode;
  enum astral::mipmap_t m_mipmap_mode;
  unsigned int m_atlas_mipmap;
  bool m_show_atlas_pixel_boundary;

  PanZoomTrackerSDLEvent m_zoom;
};

//////////////////////////////////////
// TestImageAtlas::PerImage methods
TestImageAtlas::PerImage::
PerImage(const astral::reference_counted_ptr<ImageLoader> &pimage_data,
         const std::string &filename):
  m_filename(filename),
  m_image_data(pimage_data),
  m_dims(pimage_data->dimensions()),
  m_area(m_dims.x() * m_dims.y())
{
  m_rect.m_min_point = astral::vec2(0.0f, 0.0f);
  m_rect.m_max_point = astral::vec2(m_dims.x(), m_dims.y());
}

TestImageAtlas::PerImage::
PerImage(astral::uvec2 sz):
  m_dims(sz),
  m_area(m_dims.x() * m_dims.y())
{
  std::ostringstream str;

  str << m_dims;

  m_filename = str.str();
  m_rect.m_min_point = astral::vec2(0.0f, 0.0f);
  m_rect.m_max_point = astral::vec2(m_dims.x(), m_dims.y());
}

void
TestImageAtlas::PerImage::
load_padding(int LOD, astral::ivec2 wh, astral::c_array<const astral::u8vec4> pixels)
{
  for (int P = 1, endP = m_image->tile_padding(LOD); P <= endP; ++P)
    {
      // pre-padding column
      m_image->set_pixels(LOD,
                          astral::ivec2(-P, 0),
                          astral::ivec2(1, wh.y()),
                          wh.x(), pixels);

      // pre-paddng row
      m_image->set_pixels(LOD,
                          astral::ivec2(0, -P),
                          astral::ivec2(wh.x(), 1),
                          wh.x(), pixels);
    }

  // pre-padding corner
  for (int Px = 1, endP = m_image->tile_padding(LOD); Px <= endP; ++Px)
    {
      for (int Py = 1; Py <= endP; ++Py)
        {
          m_image->set_pixels(LOD,
                              astral::ivec2(-Px, -Py),
                              astral::ivec2(1, 1),
                              wh.x(), pixels);
        }
    }
}

void
TestImageAtlas::PerImage::
create_image(TestImageAtlas *p)
{
  std::cout << "Processing \"" << m_filename << "\" of size " << m_dims << "\n";

  ASTRALassert(!m_image);
  m_image = p->engine().image_atlas().create_image(astral::uvec2(m_dims));

  if (m_image_data)
    {
      const ImageLoaderData &image_data(*m_image_data);
      unsigned int w(m_dims.x());
      unsigned int h(m_dims.y());

      for (m_num_mips = 0; w > 0u && h > 0u && m_num_mips < m_image->number_mipmap_levels(); w >>= 1u, h >>= 1u, ++m_num_mips)
        {
          m_image->set_pixels(m_num_mips,
                              astral::ivec2(0, 0),
                              astral::ivec2(w, h),
                              w,
                              image_data.mipmap_pixels(m_num_mips));
          load_padding(m_num_mips, astral::ivec2(w, h), image_data.mipmap_pixels(m_num_mips));
        }
    }
  else
    {
      unsigned int w(m_dims.x());
      unsigned int h(m_dims.y());
      std::vector<astral::u8vec4> pixels;
      std::vector<astral::u8vec4> mip_pixels;

      pixels.resize(m_dims.x() * m_dims.y());
      for (unsigned int y = 0, idx = 0; y < h; ++y)
        {
          for (unsigned int x = 0; x < w; ++x, ++idx)
            {
              uint32_t v;

              v = (x + y) >> 5u;
              v = (v & 1u) * 128u + (v & 2u) * 32u;
              pixels[idx] = astral::u8vec4(v, 255 - v, 0, 255u);
            }
        }

      for (m_num_mips = 0; w > 0u && h > 0u && m_num_mips < m_image->number_mipmap_levels(); ++m_num_mips)
        {
          m_image->set_pixels(m_num_mips,
                              astral::ivec2(0, 0),
                              astral::ivec2(w, h),
                              w,
                              astral::make_c_array(pixels));

          load_padding(m_num_mips, astral::ivec2(w, h), astral::make_c_array(pixels));

          create_mipmap_level(astral::ivec2(w, h),
                              astral::make_c_array(pixels),
                              mip_pixels);
          w >>= 1u;
          h >>= 1u;
          std::swap(mip_pixels, pixels);
        }
    }
  m_image->default_use_prepadding(true);

  ASTRALassert(static_cast<int>(m_image->size().x()) == m_dims.x());
  ASTRALassert(static_cast<int>(m_image->size().y()) == m_dims.y());
}

//////////////////////////////////
// TestImageAtlas methods
TestImageAtlas::
TestImageAtlas(void):
  m_demo_options("Demo Options", *this),
  m_image_list(&std::cout, "add_image", "Add an image to the atlas", *this),
  m_recreate_images(0, "recreate_images",
                    "If non-zero, recreate the images the number of times to stress-test ImageAtlas",
                    *this),
  m_recreate_images_stride(1u, "recreate_images_stride",
                           "if recreate_images > 0, gives the number of images that skip "
                           "recreation; use this to test atlas behavior during partial clear",
                           *this),
  m_duplicate_images(0, "duplicate_images",
                     "If non-zero, duplicate each image loaged the named number of times",
                     *this),
  m_sort_images(unsorted,
                enumerated_string_type<enum sort_t>()
                .add_entry("unsorted", unsorted, "do not sort the images")
                .add_entry("area", sort_by_area, "sort images in decreasing order of area")
                .add_entry("width", sort_by_width, "sort images in decreasing order of width")
                .add_entry("height", sort_by_height, "sort images in decreasing order of height")
                .add_entry("perimiter", sort_by_perimiter, "sort images in decreasing order of perimiter")
                .add_entry("random", sort_random, "place images into random order"),
                "sort_images",
                "Specifies if and how images are sorted before being added to the image atlas",
                *this),
  m_bad_sort_order(false, "bad_sort_order",
                   "If true sort as according to sort_images "
                   "but place in increasing order, gives worse "
                   "packing into atlas", *this),
  m_time_image_creation_only(false, "time_image_creation_only",
                             "If true, immediately after image creation, exit",
                             *this),
  m_current_image(0),
  m_filter_mode(astral::filter_linear),
  m_mipmap_mode(astral::mipmap_ceiling),
  m_atlas_mipmap(0),
  m_show_atlas_pixel_boundary(false)
{
  std::cout << "Controls:"
            << "\n\ti: increment (hold ctrl to decrement) to next image, once get to last image then increment through atlas layers"
            << "\n\tf: change image filter"
            << "\n\tm: change mipmap mode"
            << "\n\tp: toggle display pixel boundary in atlas"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n";
}

void
TestImageAtlas::
create_images(void)
{
  if (m_image_list.elements().empty() || true)
    {
      // m_images.push_back(PerImage(astral::uvec2(512, 512)));
      //m_images.push_back(PerImage(astral::uvec2(588, 588)));
      m_images.push_back(PerImage(astral::uvec2(35512, 124)));
    }

  for (const auto &e : m_image_list.elements())
    {
      m_images.push_back(PerImage(e.m_loaded_value, e.m_filename));
    }
  m_image_list.clear();

  unsigned int num(m_images.size());
  for (unsigned int dup = 0; dup < m_duplicate_images.value(); ++dup)
    {
      m_images.push_back(PerImage(astral::uvec2(35512, 124)));
      for (unsigned int image = 1; image < num; ++image)
        {
          std::ostringstream str;

          str << m_images[image].m_filename << "->Duplicate #" << dup + 1u;
          m_images.push_back(PerImage(m_images[image].m_image_data, str.str()));
        }
    }

  switch (m_sort_images.value())
    {
    case unsorted:
      break;

    case sort_by_area:
      std::sort(m_images.begin(), m_images.end(), SortByArea(m_bad_sort_order.value()));
      break;

    case sort_by_width:
      std::sort(m_images.begin(), m_images.end(), SortByWidth(m_bad_sort_order.value()));
      break;

    case sort_by_height:
      std::sort(m_images.begin(), m_images.end(), SortByHeight(m_bad_sort_order.value()));
      break;

    case sort_by_perimiter:
      std::sort(m_images.begin(), m_images.end(), SortByPerimiter(m_bad_sort_order.value()));
      break;

    case sort_random:
      {
        std::vector<PerImage> tmp;

        srand(0);
        tmp.swap(m_images);
        m_images.reserve(tmp.size());
        while (!tmp.empty())
          {
            int r;

            r = rand() % tmp.size();
            std::swap(tmp[r], tmp.back());
            m_images.push_back(tmp.back());
            tmp.pop_back();
          }
      }
      break;
    }

  simple_time creation_time;
  for (auto &P : m_images)
    {
      P.create_image(this);
    }

  unsigned int cr(0);
  for (unsigned int r = 0; r < m_recreate_images.value(); ++r)
    {
      unsigned nxt(m_recreate_images_stride.value() + 1);

      for (unsigned int image = 0, end_image = m_images.size(); image < end_image; image += nxt, ++cr)
        {
          m_images[image].m_image = nullptr;
        }

      for (unsigned int image = 0, end_image = m_images.size(); image < end_image; image += nxt)
        {
          m_images[image].create_image(this);
        }
    }

  for (auto &P : m_images)
    {
      P.m_image_data = nullptr;
    }

  std::cout << "Took " << creation_time.elapsed()
            << " ms to test the atlas with "
            << m_images.size() << " images with recreate of "
            << cr << " images (counting repeated recreation)\n";
  if (m_time_image_creation_only.value())
    {
      end_demo(0);
    }
}

void
TestImageAtlas::
init_gl(int, int)
{
  unsigned int wh;

  create_images();
  wh = engine().image_atlas().color_backing().width_height();
  m_atlas_rect.m_min_point = astral::vec2(0.0f, 0.0f);
  m_atlas_rect.m_max_point = astral::vec2(wh, wh);

  astral::c_string vertex_shader =
    "void astral_material_pre_vert_shader(in uint sub_shader, in uint shader_data,\n"
    "                                     in uint brush_idx, in vec2 item_p,\n"
    "                                     in AstralTransformation pixel_transformation_material)\n"
    "{}\n"
    "void astral_material_vert_shader(in uint sub_shader, in uint shader_data,\n"
    "                                 in uint brush_idx, in vec2 item_p,\n"
    "                                 in AstralTransformation pixel_transformation_material)\n"
    "{\n"
    "    vec3 static_data;\n"
    ""
    "    static_data = astral_read_item_dataf(shader_data).xyz;\n"
    "    material_brush_p_z = floatBitsToUint(static_data.x);\n"
    "    material_brush_lod = floatBitsToUint(static_data.y);\n"
    "    material_pixel_lines_wt = static_data.z;\n"
    "    material_brush_p_x = item_p.x;\n"
    "    material_brush_p_y = item_p.y;\n"
    "}\n";

  astral::c_string fragment_shader =
    "void astral_material_pre_frag_shader(in uint sub_shader, in uint color_space) {}\n"
    "void astral_material_frag_shader(in uint sub_shader, in uint color_space, inout vec4 color, inout float coverage)\n"
    "{\n"
    "    float vv, ss;\n"
    "    vec2 R, ww, ff;\n"
    "    uint pow2lod;\n"
    "    float fpow2lod;\n"
    "    ivec2 tp;\n"
    "\n"
    "    pow2lod = 1u << material_brush_lod;\n"
    "    fpow2lod = float(pow2lod);\n"
    "    ff = vec2(material_brush_p_x, material_brush_p_y);\n"
    "    ww = fwidth(vec2(material_brush_p_x, material_brush_p_y));\n"
    "\n"
    "    R = 64.0 * fract(ff / 64.0);\n"
    "    vv = step(R.x, 2.0 * ww.x) + step(R.y, 2.0 * ww.y);\n"
    "    vv = min(vv, 1.0);\n"
    ""
    "    R = fpow2lod * fract(ff / fpow2lod);\n"
    "    ss = step(R.x, 2.0 * ww.x) + step(R.y, 2.0 * ww.y);\n"
    "    ss = min(ss, 1.0) * material_pixel_lines_wt;\n"
    ""
    "    tp = ivec2(material_brush_p_x, material_brush_p_y) >> int(material_brush_lod);\n"
    "    color = texelFetch(astral_image_color_atlas, ivec3(tp, material_brush_p_z), int(material_brush_lod));\n"
    "    color = mix(color, vec4(0.0, 0.0, 0.0, 1.0), ss);\n"
    "    color = mix(color, vec4(1.0, 0.0, 0.0, 1.0), vv);\n"
    "}\n";

  m_atlas_brush
    = astral::gl::MaterialShaderGL3::create(engine(),
                                            astral::gl::ShaderSource().add_source(vertex_shader, astral::gl::ShaderSource::from_string),
                                            astral::gl::ShaderSource().add_source(fragment_shader, astral::gl::ShaderSource::from_string),
                                            astral::gl::ShaderSymbolList() //add the varyings we need
                                            .add_varying("material_brush_p_x", astral::gl::ShaderVaryings::interpolator_smooth)
                                            .add_varying("material_brush_p_y", astral::gl::ShaderVaryings::interpolator_smooth)
                                            .add_varying("material_pixel_lines_wt", astral::gl::ShaderVaryings::interpolator_flat)
                                            .add_varying("material_brush_p_z", astral::gl::ShaderVaryings::interpolator_uint)
                                            .add_varying("material_brush_lod", astral::gl::ShaderVaryings::interpolator_uint),
                                            astral::MaterialShader::Properties());
}

void
TestImageAtlas::
draw_frame(void)
{
  astral::Transformation tr;
  astral::RenderEncoderSurface render_encoder;

  render_encoder = renderer().begin( render_target());

  tr = m_zoom.transformation().astral_transformation();
  render_encoder.transformation(tr);

  if (m_current_image < m_images.size())
    {
      astral::ImageSampler image(*m_images[m_current_image].m_image,
                                     m_filter_mode, m_mipmap_mode);
      astral::Brush br;

      br.image(renderer().create_value(image));
      render_encoder.draw_rect(m_images[m_current_image].m_rect, false, renderer().create_value(br));
    }
  else
    {
      astral::vecN<astral::gvec4, 1> shader_data;
      astral::ItemData shader_item_data;

      shader_data[0].x().u = static_cast<float>(m_current_image - m_images.size());
      shader_data[0].y().u = m_atlas_mipmap;
      shader_data[0].z().f = m_show_atlas_pixel_boundary ? 1.0f : 0.0f;
      shader_data[0].w().f = 0.0f;
      shader_item_data = render_encoder.create_item_data(shader_data, astral::no_item_data_value_mapping);

      astral::Material material(*m_atlas_brush, shader_item_data);
      render_encoder.draw_rect(m_atlas_rect, false, material);
    }

  renderer().end();
}

void
TestImageAtlas::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);

  if (ev.type == SDL_KEYDOWN)
    {
      switch (ev.key.keysym.sym)
        {
        case SDLK_i:
          {
            const astral::ImageAtlasColorBacking *backing;
            backing = &engine().image_atlas().color_backing();
            cycle_value(m_current_image,
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        m_images.size() + backing->number_layers());
            if (m_current_image < m_images.size())
              {
                std::cout << "Showing image " << m_images[m_current_image].m_filename << "\n";
              }
            else
              {
                std::cout << "Showing atlas layer #" << m_current_image - m_images.size() << "\n";
              }
          }
          break;

        case SDLK_f:
          cycle_value(m_filter_mode,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_filter_modes);
          std::cout << "Filter mode set to " << astral::label(m_filter_mode) << "\n";
          break;

        case SDLK_m:
          if (m_current_image < m_images.size())
            {
              cycle_value(m_mipmap_mode,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::number_mipmap_modes);
              std::cout << "Mipmap mode set to " << astral::label(m_mipmap_mode) << "\n";
            }
          else
            {
              cycle_value(m_atlas_mipmap,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::ImageMipElement::maximum_number_of_mipmaps);
              std::cout << "Show atlas mipmap level " << m_atlas_mipmap << "\n";
            }
          break;

        case SDLK_p:
          m_show_atlas_pixel_boundary = !m_show_atlas_pixel_boundary;
          std::cout << "Show atlas pixel boundary set to " << std::boolalpha
                    << m_show_atlas_pixel_boundary << "\n";
          break;
        }
    }

  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  TestImageAtlas M;
  return M.main(argc, argv);
}
