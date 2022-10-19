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
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/text/freetype_face.hpp>

#include "sdl_demo.hpp"
#include "render_engine_gl3_demo.hpp"
#include "PanZoomTracker.hpp"
#include "simple_time.hpp"
#include "UniformScaleTranslate.hpp"
#include "cycle_value.hpp"
#include "command_line_list.hpp"
#include "print_bytes.hpp"
#include "generic_hierarchy.hpp"

typedef astral::reference_counted<GenericHierarchy>::NonConcurrent Hierarchy;

class GlyphTest:public render_engine_gl3_demo
{
public:
  GlyphTest(void);

  ~GlyphTest();

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
  enum mode_t
    {
      text_from_command_line,
      text_from_file,
      glyph_list_from_file,
      draw_glyph_set,
    };

  enum hud_mode_t
    {
      hud_none,
      hud_show_fps,
      hud_show_glyph_info,

      hud_mode_number
    };

  float
  update_smooth_values(void);

  void
  reset_zoom_transformation(void);

  void
  add_text(std::istream &str, astral::TextItem &text_item);

  void
  add_text(astral::vec2 &pen, const std::string &text, astral::TextItem &text_item);

  void
  add_text(std::istream &stream,
           std::vector<astral::reference_counted_ptr<astral::TextItem>> &text_item);

  static
  std::string
  substitute_tabs(const std::string &v)
  {
    std::string return_value;

    for (char ch : v)
      {
        if (ch != '\t')
          {
            return_value.push_back(ch);
          }
        else
          {
            return_value.push_back(' ');
            return_value.push_back(' ');
            return_value.push_back(' ');
            return_value.push_back(' ');
          }
      }
    return return_value;
  }

  template<typename T>
  void
  add_text(const std::string &str, T &text_item)
  {
    std::istringstream i(str);
    add_text(i, text_item);
  }

  void
  add_glyph_set(float width);

  void
  add_glyphs(astral::vec2 &pen, unsigned int v1, unsigned int v2, float width);

  void
  add_end_of_line_text(float height, astral::vec2 &pen, int start, int end);

  void
  add_glyph_list(float width, std::istream &stream);

  static
  void
  compute_skewed_rect(astral::SkewParameters skew, astral::Rect R,
                      astral::vec2 post_translate,
                      astral::vecN<astral::vec2, 4> *out_value);

  void
  create_hierarchy(void);

  Hierarchy*
  glyph_hierarchy(void)
  {
    if (!m_glyph_hierarchy)
      {
        create_hierarchy();
      }
    return m_glyph_hierarchy.get();
  }

  const astral::Font&
  user_font(void)
  {
    ASTRALassert(!m_static_text_item.empty());
    return m_static_text_item.front()->font();
  }

  astral::reference_counted_ptr<astral::TextItem>
  create_text_item(const astral::Font &font);

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_font_file;
  command_line_argument_value<int> m_face_index;
  command_line_argument_value<std::string> m_source;
  enumerated_command_line_argument_value<enum mode_t> m_mode;
  command_line_argument_value<float> m_pixel_size;
  command_line_argument_value<float> m_scale_x;
  command_line_argument_value<float> m_skew_x;
  command_line_argument_value<std::string> m_image_file;
  command_line_argument_value<float> m_glyph_red, m_glyph_green, m_glyph_blue, m_glyph_alpha;

  command_line_argument_value<float> m_max_pixel_size;
  command_line_argument_value<bool> m_dynamic_glyph_bitmap;
  command_line_argument_value<bool> m_draw_as_paths;
  command_line_argument_value<bool> m_color_glyphs_observe_material;
  command_line_argument_value<astral::vec2> m_scale_pre_rotate, m_scale_post_rotate;
  command_line_argument_value<float> m_rotate_angle;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;

  astral::reference_counted_ptr<astral::Image> m_image;
  astral::reference_counted_ptr<astral::TextItem> m_dynamic_text_item;

  /* one astral::TextItem per line of text, first element is always made. */
  std::vector<astral::reference_counted_ptr<astral::TextItem>> m_static_text_item;
  astral::BoundingBox<float> m_static_text_item_bb;

  astral::reference_counted_ptr<astral::TextItem> m_static_text_indices;

  simple_time m_draw_timer;
  PanZoomTrackerSDLEvent m_zoom;
  bool m_print_stats;
  enum hud_mode_t m_hud_mode;
  astral::GlyphShader::SyntheticData m_synth;

  astral::reference_counted_ptr<Hierarchy> m_glyph_hierarchy;
  std::vector<int> m_strike_index_used;

  class PerGlyph
  {
  public:
    unsigned int m_text_item;
    unsigned int m_glyph;
  };
  std::vector<PerGlyph> m_glyph_list;
};

//////////////////////////////////////////
// GlyphTest methods
GlyphTest::
GlyphTest(void):
  m_demo_options("Demo Options", *this),
  m_font_file(DEFAULT_FONT, "font_file", "TTF File from which to extract glyph(s)", *this),
  m_face_index(0, "face_index", "Which face of the file to load", *this),
  m_source("Hello World", "source", "Source specifying what to draw", *this),
  m_mode(text_from_command_line,
         enumerated_string_type<enum mode_t>()
         .add_entry("text_from_command_line", text_from_command_line, "source is the text to draw")
         .add_entry("text_from_file", text_from_file, "source is a txt file to draw")
         .add_entry("draw_glyph_set", draw_glyph_set, "ignore source and display all glyphs of the file")
         .add_entry("glyph_list_from_file", glyph_list_from_file, "source is a file containing a list of glyphs to draw"),
         "mode",
         "Specifies the interpretation of source argument",
         *this),
  m_pixel_size(32.0f, "pixel_size", "Pixel size at which to render the glyphs", *this),
  m_scale_x(1.0f, "scale_x", "Scale factor to apply to x-coordinate for skewing the text", *this),
  m_skew_x(0.0f, "skew_x", "Amount of skew to apply to the text", *this),
  m_image_file("", "image", "name of file for image background", *this),
  m_glyph_red(1.0f, "glyph_red", "red channel value for drawing glyphs", *this),
  m_glyph_green(1.0f, "glyph_green", "green channel value for drawing glyphs", *this),
  m_glyph_blue(1.0f, "glyph_blue", "blue channel value for drawing glyphs", *this),
  m_glyph_alpha(1.0f, "glyph_alpha", "alpha channel value for drawing glyphs", *this),
  m_max_pixel_size(-1.0f, "max_pixel_size",
                   "only has effect if dynamic_glyph_bitmap is true; "
                   "if set and non-negative gives the maximum size "
                   "allowed for bitmap glyphs",
                   *this),
  m_dynamic_glyph_bitmap(true, "dynamic_glyph_bitmap",
                         "If true, Astral will use choose strikes "
                         "from bitmap glyphs that closer match the "
                         "presentation of the glyphs. If false, the "
                         "strike used is entirely based off of "
                         "pixel_size", *this),
  m_draw_as_paths(false, "draw_as_paths", "If true, draw the text as paths", *this),
  m_color_glyphs_observe_material(false, "color_glyphs_observe_material", "If true, color glyphs will observe the material passed to draw-text", *this),
  m_scale_pre_rotate(astral::vec2(1.0f, 1.0f), "scale_pre_rotate", "scaling transformation to apply to glyphs before rotation, formatted as ScaleX:SaleY", *this),
  m_scale_post_rotate(astral::vec2(1.0f, 1.0f), "scale_post_rotate", "scaling transformation to apply to glyphs after rotation, formatted as ScaleX:SaleY", *this),
  m_rotate_angle(0.0f, "rotate", "amount by which to rotate glyphs in degrees", *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera", "Initial position of camera", *this),
  m_print_stats(false),
  m_hud_mode(hud_show_glyph_info)
{
  std::cout << "Controls:"
            << "\n\tspace: toggle showing frame rate to console"
            << "\n\tq: reset transformation applied to the text"
            << "\n\tp: cycle through different HUD modes"
            << "\n\tw: reset synthetic font properties to default (0)"
            << "\n\te: toggle drawing text as paths"
            << "\n\tf: toggle color glyphs observing material fully"
            << "\n\tup/down arrow: increase/decrease synthetic font property skew"
            << "\n\treturn-up/down: increase/decrease synthetic font property boldness"
            << "\n\tright/left: increase/decrease synthetic font property scale-x"
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
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n";
}

GlyphTest::
~GlyphTest()
{
}

void
GlyphTest::
reset_zoom_transformation(void)
{
  /* Initialize zoom location to be identity */
  m_zoom.transformation(UniformScaleTranslate<float>());
}

void
GlyphTest::
add_text(std::istream &stream, astral::TextItem &text_item)
{
  const astral::Font &font(text_item.font());
  float height(font.base_metrics().m_height);
  astral::vec2 pen(0.0f, 0.0f);
  std::string line;

  while (std::getline(stream, line))
    {
      pen.x() = 0.0f;
      pen.y() += height;
      add_text(pen, line, text_item);
    }
}

void
GlyphTest::
add_text(std::istream &stream, std::vector<astral::reference_counted_ptr<astral::TextItem>> &text_items)
{
  float height(user_font().base_metrics().m_height);
  astral::vec2 pen(0.0f, 0.0f);
  std::string line;

  while (std::getline(stream, line))
    {
      pen.x() = 0.0f;
      pen.y() += height;
      text_items.push_back(create_text_item(user_font()));
      add_text(pen, line, *text_items.back());
    }
}

void
GlyphTest::
add_glyphs(astral::vec2 &pen, unsigned int v1, unsigned int v2, float width)
{
  const astral::Font &font(user_font());
  astral::Typeface &typeface(font.typeface());
  unsigned int number_glyphs(typeface.number_glyphs());
  float height(font.base_metrics().m_height);
  float advance_scaling_factor(font.scaling_factor());
  float stop_width(width);
  unsigned int start(v1);
  astral::BoundingBox<float> static_text_bb;

  pen.x() = 0.0f;
  pen.y() += height;
  for (; v1 <= v2 && v1 < number_glyphs; ++v1)
    {
      astral::GlyphIndex glyph_index(v1);
      astral::Glyph G(typeface.fetch_glyph(glyph_index));
      const astral::GlyphMetrics &metrics(font.glyph_metrics(G));
      const astral::GlyphColors *colors(G.colors());
      float dx, lh;

      lh = metrics.m_horizontal_layout_offset.x();
      dx = astral::t_max(metrics.m_size.x() + astral::t_max(0.0f, lh),
                         metrics.m_advance.x());
      dx += astral::t_max(0.0f, -lh);
      dx *= advance_scaling_factor;

      if (lh < 0.0f)
        {
          pen.x() -= lh * advance_scaling_factor;
        }

      if (colors)
        {
          for (unsigned int P = 0; P < colors->number_palettes(); ++P)
            {
              if (pen.x() + dx >= stop_width)
                {
                  pen.x() = width;
                  add_end_of_line_text(height, pen, start, v1);
                  start = v1;

                  m_static_text_item.push_back(create_text_item(font));
                }

              m_static_text_item.back()->add_glyph(G.glyph_index(), pen, astral::GlyphPaletteID(P));
              pen.x() += dx;
            }
        }
      else
        {
          if (pen.x() + dx >= stop_width)
            {
              pen.x() = width;
              add_end_of_line_text(height, pen, start, v1);
              start = v1;

              m_static_text_item.push_back(create_text_item(font));
            }

          m_static_text_item.back()->add_glyph(G.glyph_index(), pen);
          pen.x() += dx;
        }
    }

  if (start < v1 && v1 > 0u)
    {
      pen.x() = width;
      add_end_of_line_text(height, pen, start, v1 - 1u);
    }
}

void
GlyphTest::
add_text(astral::vec2 &pen, const std::string &in_line, astral::TextItem &text_item)
{
  float scaling_factor(text_item.font().scaling_factor());
  astral::Typeface &typeface(text_item.font().typeface());
  std::string line(substitute_tabs(in_line));

  for (char ch : line)
    {
      astral::GlyphIndex glyph_index;
      astral::Glyph glyph;

      glyph_index = typeface.glyph_index(ch);
      glyph = typeface.fetch_glyph(glyph_index);
      ASTRALassert(glyph.valid());

      text_item.add_glyph(glyph_index, pen);

      pen.x() += scaling_factor * text_item.font().glyph_metrics(glyph).m_advance.x();
    }
}

void
GlyphTest::
add_end_of_line_text(float height, astral::vec2 &pen, int start, int end)
{
  std::ostringstream str;
  astral::vec2 p(0.0f, pen.y());

  str << " [" << start << "-" << end << "]";
  add_text(p, str.str(), *m_static_text_indices);
  pen.x() = 0.0f;
  pen.y() += height;
}

void
GlyphTest::
add_glyph_list(float width, std::istream &stream)
{
  /* Each line in the file corresponds to one line displayed.
   * A can have up to three integer: first glyph, last glyph and glyphs-per-line
   */
  astral::vec2 pen(0.0f, 0.0f);
  std::string line;

  while (std::getline(stream, line))
    {
      std::istringstream line_stream(line);
      unsigned int v1, v2;

      line_stream >> v1;
      if (!line_stream)
        {
          break;
        }

      line_stream >> v2;
      if (!line_stream)
        {
          v2 = v1;
        }

      add_glyphs(pen, v1, v2, width);
    }
}

void
GlyphTest::
add_glyph_set(float width)
{
  const astral::Font &font(user_font());
  astral::Typeface &typeface(font.typeface());
  unsigned int number_glyphs(typeface.number_glyphs());
  simple_time glyph_realize_timer;
  std::vector<astral::GlyphIndex> glyph_indices(number_glyphs);
  std::vector<astral::Glyph> glyphs(number_glyphs);

  for (unsigned int i = 0; i < number_glyphs; ++i)
    {
      glyph_indices[i] = astral::GlyphIndex(i);
    }

  glyph_realize_timer.restart();
  typeface.fetch_glyphs_parallel(typeface_threads(),
                                 astral::make_c_array(glyph_indices),
                                 astral::make_c_array(glyphs));
  std::cout << "Took " << glyph_realize_timer.restart()
            << " ms to realize the glyph data\n";

  if (number_glyphs > 0u)
    {
      astral::vec2 pen(0.0f, 0.0f);
      add_glyphs(pen, 0, number_glyphs - 1u, width);
    }

  for (const auto &text_item : m_static_text_item)
    {
      text_item->render_data(1.0f, engine());
    }

  std::cout << "Took " << glyph_realize_timer.restart()
            << " ms to build the TextItem\n";
}

void
GlyphTest::
compute_skewed_rect(astral::SkewParameters skew, astral::Rect R,
                    astral::vec2 post_translate,
                    astral::vecN<astral::vec2, 4> *out_values_ptr)
{
  astral::vecN<astral::vec2, 4> &out_values(*out_values_ptr);
  for (unsigned int c = 0; c < 4; ++c)
    {
      astral::vec2 p;

      p = R.point(static_cast<enum astral::Rect::corner_t>(c));
      out_values[c].x() = p.x() * skew.m_scale_x - p.y() * skew.m_skew_x;
      out_values[c].y() = p.y();

      out_values[c].x() += post_translate.x() * skew.m_scale_x;
      out_values[c].y() += post_translate.y();
    }
}

void
GlyphTest::
create_hierarchy(void)
{
  using namespace astral;

  m_glyph_hierarchy = Hierarchy::create(m_synth.bounding_box(m_static_text_item_bb, user_font().base_metrics()));
  for (unsigned int text_item = 0; text_item < m_static_text_item.size(); ++text_item)
    {
      const astral::TextItem &txt(*m_static_text_item[text_item]);
      const Font &font(txt.font());

      for (unsigned int g = 0, endg = txt.number_glyphs(); g < endg; ++g)
        {
          Glyph glyph;
          vec2 position;
          BoundingBox<float> bb, skew_bb;
          GlyphMetrics metrics;
          vecN<vec2, 4> pts;

          txt.glyph(g, &glyph, &position);
          font.glyph_metrics(glyph, &metrics);

          bb.union_point(metrics.m_horizontal_layout_offset);
          bb.union_point(metrics.m_horizontal_layout_offset + vec2(metrics.m_size.x(), -metrics.m_size.y()));

          compute_skewed_rect(m_synth.m_skew, bb.as_rect(), position, &pts);
          skew_bb.union_points(pts.begin(), pts.end());

          m_glyph_hierarchy->add(skew_bb, m_glyph_list.size());

          PerGlyph PG;
          PG.m_text_item = text_item;
          PG.m_glyph = g;
          m_glyph_list.push_back(PG);
        }
    }
}

astral::reference_counted_ptr<astral::TextItem>
GlyphTest::
create_text_item(const astral::Font &font)
{
  if (m_dynamic_glyph_bitmap.value())
    {
      if (m_max_pixel_size.value() > 0.0f)
        {
          return astral::TextItem::create(font, m_max_pixel_size.value());
        }
      else
        {
          return astral::TextItem::create(font, astral::TextItem::use_nearest_strike);
        }
    }
  else
    {
      return astral::TextItem::create(font, astral::TextItem::use_strike_as_indicated_by_font);
    }
}

void
GlyphTest::
init_gl(int w, int h)
{
  ASTRALunused(h);
  m_zoom.transformation(m_initial_camera.value());

  if (!m_image_file.value().empty())
    {
      astral::reference_counted_ptr<ImageLoader> pixels = ImageLoader::create(m_image_file.value());
      astral::uvec2 image_dims;

      image_dims = pixels->dimensions();

      if (pixels->non_empty())
        {
          std::cout << "Loaded image from file \""
                    << m_image_file.value() << "\"\n";
          m_image = engine().image_atlas().create_image(image_dims);
          for (unsigned int mip = 0, w = image_dims.x(), h = image_dims.y();
               w > 0u && h > 0u && mip < m_image->number_mipmap_levels();
               w >>= 1u, h >>= 1u, ++mip)
            {
              m_image->set_pixels(mip,
                                  astral::ivec2(0, 0),
                                  astral::ivec2(w, h),
                                  w,
                                  pixels->mipmap_pixels(mip));
            }
        }
    }

  /* make the Typeface */
  astral::reference_counted_ptr<astral::Typeface> typeface;

  if (m_font_file.value() != std::string(DEFAULT_FONT) || m_face_index.value() != 0)
    {
      typeface = create_typeface_from_file(m_face_index.value(), m_font_file.value().c_str());
    }
  else
    {
      typeface = &default_typeface();
    }

  m_synth.m_skew.m_scale_x = m_scale_x.value();
  m_synth.m_skew.m_skew_x = m_skew_x.value();

  astral::Font default_font(default_typeface(), m_pixel_size.value());
  astral::Font font(*typeface, m_pixel_size.value());

  m_dynamic_text_item = create_text_item(default_font);
  m_static_text_indices = create_text_item(default_font);
  m_static_text_item.push_back(create_text_item(font));

  int pre_fp16_allocated(engine().static_data_allocator16().amount_allocated());
  int pre_gvec4_allocated(engine().static_data_allocator32().amount_allocated());

  switch (m_mode.value())
    {
    case draw_glyph_set:
      add_glyph_set(w);
      break;

    case text_from_file:
      {
        std::ifstream file(m_source.value().c_str());
        add_text(file, m_static_text_item);
      }
      break;

    case text_from_command_line:
      {
        std::istringstream stream(m_source.value());
        add_text(stream, m_static_text_item);
      }
      break;

    case glyph_list_from_file:
      {
        std::ifstream file(m_source.value().c_str());
        add_glyph_list(w, file);
      }
      break;
    }

  int text_static_data_size(0u), num_glyphs(0);
  const astral::StaticData *text_static_data;

  /* force the glyph data to be realized into GPU memory */
  for (const auto &p : m_static_text_item)
    {
      m_static_text_item_bb.union_box(p->bounding_box());
      text_static_data = p->render_data(1.0f, engine()).m_static_data.get();
      num_glyphs += p->number_glyphs();

      if (text_static_data)
        {
          text_static_data_size += text_static_data->size();
        }
    }

  m_strike_index_used.resize(m_static_text_item.size(), -1);

  int fp16_allocated(engine().static_data_allocator16().amount_allocated() - pre_fp16_allocated);
  int gvec4_allocated(engine().static_data_allocator32().amount_allocated() - pre_gvec4_allocated);

  gvec4_allocated -= text_static_data_size;
  std::cout << "Allocated " << fp16_allocated << " fp16-vec4 values ("
            << PrintBytes(fp16_allocated * sizeof(astral::u16vec4), PrintBytes::do_not_round) << ")\n"
            << "Allocated " << gvec4_allocated << " gvec4 values for glyphs ("
            << PrintBytes(gvec4_allocated * sizeof(astral::gvec4), PrintBytes::do_not_round) << ")\n"
            << "TextItem used " << text_static_data_size << " gvec4 values for "
            << num_glyphs << " glyphs ("
            << PrintBytes(text_static_data_size * sizeof(astral::gvec4), PrintBytes::do_not_round) << ")\n";
}

void
GlyphTest::
draw_frame(void)
{
  float frame_ms;
  astral::ivec2 dims(dimensions());
  astral::RenderEncoderSurface render_encoder;
  astral::c_array<const unsigned int> stats;
  astral::c_array<const astral::c_string> stats_labels(renderer().stats_labels());

  frame_ms = update_smooth_values();

  render_encoder = renderer().begin( render_target());
  if (m_image)
    {
      astral::Brush brush;
      astral::RenderValue<astral::Brush> br;
      astral::vec2 target_sz(dims);
      astral::vec2 src_sz(m_image->size());
      astral::ImageSampler image(*m_image, astral::filter_cubic, astral::mipmap_ceiling);

      brush.image(render_encoder.create_value(image));
      br = render_encoder.create_value(brush);

      render_encoder.scale(target_sz / src_sz);
      render_encoder.draw_rect(astral::Rect().size(src_sz), br);
    }

  render_encoder.transformation(m_zoom.transformation().astral_transformation());
  render_encoder.scale(m_scale_pre_rotate.value());
  render_encoder.rotate(m_rotate_angle.value() * (ASTRAL_PI / 180.0f));
  render_encoder.scale(m_scale_post_rotate.value());

  astral::RenderValue<astral::Brush> glyph_brush;
  astral::vec4 glyph_color(m_glyph_red.value(), m_glyph_green.value(), m_glyph_blue.value(), m_glyph_alpha.value());

  glyph_brush = render_encoder.create_value(astral::Brush().base_color(glyph_color));
  for (unsigned int i = 0, endi = m_static_text_item.size(); i < endi; ++i)
    {
      if (m_draw_as_paths.value() && m_static_text_item[i]->font().typeface().is_scalable())
        {
          m_strike_index_used[i] = 0;
          render_encoder.draw_text_as_path(*m_static_text_item[i], glyph_brush);
        }
      else
        {
          if (m_color_glyphs_observe_material.value())
            {
              astral::GlyphShader shader = render_encoder.default_shaders().m_glyph_shader_observe_material_always;
              m_strike_index_used[i] = render_encoder.draw_text(shader, *m_static_text_item[i], m_synth, glyph_brush);
            }
          else
            {
              m_strike_index_used[i] = render_encoder.draw_text(*m_static_text_item[i], m_synth, glyph_brush);
            }
        }
    }

  render_encoder.save_transformation();
  render_encoder.translate(m_synth.bounding_box(m_static_text_item_bb, user_font().base_metrics()).max_point().x(), 0.0f);
  render_encoder.draw_text(*m_static_text_indices);
  render_encoder.restore_transformation();

  enum hud_mode_t hud_mode(m_hud_mode);
  if (pixel_testing())
    {
      hud_mode = hud_none;
    }

  switch (hud_mode)
    {
    case hud_show_glyph_info:
      {
        astral::ivec2 mp;
        astral::vec2 p;
        std::vector<unsigned int> hits;
        std::ostringstream str;
        astral::Glyph glyph;
        astral::vec2 pen_position;
        astral::GlyphPaletteID palette;
        astral::GlyphMetrics metrics;
        astral::BoundingBox<float> bb;
        astral::Transformation inverse_skew(m_synth.m_skew.as_transformation().inverse());
        PerGlyph per_glyph;

        GetMouseState(&mp.x(), &mp.y());
        p = render_encoder.transformation().inverse().apply_to_point(astral::vec2(mp.x(), mp.y()));
        glyph_hierarchy()->query(p, &hits);

        for (unsigned int pg : hits)
          {
            astral::vec2 q, rel_p;

            per_glyph = m_glyph_list[pg];

            /* see if the point p is in the glyph rect */
            m_static_text_item[per_glyph.m_text_item]->glyph(per_glyph.m_glyph, &glyph, &pen_position, &palette);
            ASTRALassert(glyph.valid());

            /* the text in m_static_text is BEFORE the skewing is applied.
             * Thus apply m_synth.m_skew.m_scale_x to the pen position's x-coordinate
             */
            pen_position.x() *= m_synth.m_skew.m_scale_x;

            /* get the point relative to the pen-position */
            rel_p = p - pen_position;

            /* map rel_p to before the skew is applied */
            q = inverse_skew.apply_to_point(rel_p);

            /* get the box of the glyph before skew is applied */
            bb.clear();
            user_font().glyph_metrics(glyph, &metrics);
            bb.union_point(metrics.m_horizontal_layout_offset);
            bb.union_point(metrics.m_horizontal_layout_offset + astral::vec2(metrics.m_size.x(), -metrics.m_size.y()));

            if (bb.contains(q))
              {
                break;
              }

            /* reset glyph to null */
            glyph = astral::Glyph();
          }

        if (glyph.valid())
          {
            render_encoder.save_transformation();
            render_encoder.translate(pen_position.x(), pen_position.y());
            render_encoder.concat(m_synth.m_skew.as_transformation());
            render_encoder.draw_rect(bb.as_rect(),
                                     render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 0.3f))));
            render_encoder.restore_transformation();


            str << "Glyph #" << std::dec << glyph.glyph_index().m_value
                << " at " << p << "\n\tpen = " << pen_position
                << "\n\tlayout_offset = " << metrics.m_horizontal_layout_offset
                << "\n";

            if (glyph.is_scalable())
              {
                unsigned int number_layers;
                std::string prefix;

                if (glyph.is_colored())
                  {
                    number_layers = glyph.colors()->number_layers();
                    prefix = "\t\t";
                    str << "\tUsing palette " << palette.m_value << "\n";
                  }
                else
                  {
                    number_layers = 1;
                    prefix = "\t";
                  }

                for (unsigned int layer = 0; layer < number_layers; ++layer)
                  {
                    const astral::Path *path;
                    enum astral::fill_rule_t fill_rule;
                    astral::reference_counted_ptr<const astral::ItemPath> item_path;

                    path = glyph.path(layer, &fill_rule, &item_path);
                    if (glyph.is_colored())
                      {
                        str << "\tLayer #" << layer << "\n"
                            << "\t\tcolor = " << glyph.colors()->color(palette, layer)
                            << "\n";
                      }
                    str << prefix << path->number_contours() << " contours\n"
                        << prefix << item_path->properties().m_number_bands.x() << " horizontal bands\n"
                        << prefix << item_path->properties().m_number_bands.y() << " vertical bands\n"
                        << prefix << item_path->properties().m_fp16_data_size << " fp16-vec4's\n"
                        << prefix << item_path->properties().m_generic_data_size << " gvec4's\n";
                  }
              }
            else
              {
                astral::reference_counted_ptr<const astral::Image> image;

                if (m_strike_index_used[per_glyph.m_text_item] >= 0)
                  {
                    glyph.image_render_data(engine(), m_strike_index_used[per_glyph.m_text_item], &image);
                    if (image)
                      {
                        str << "\tImage of size " << image->size() << "\n";
                      }
                  }
              }
          }
        else
          {
            str << "No Glyph at " << p;
          }
        m_dynamic_text_item->clear();
        add_text(str.str(), *m_dynamic_text_item);

        render_encoder.transformation(astral::Transformation());

        render_encoder.draw_rect(m_dynamic_text_item->bounding_box().as_rect(),
                                 render_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 0.50f))));

        render_encoder.draw_text(*m_dynamic_text_item,
                                 render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.85f))));
      }
      break;

    case hud_show_fps:
      {
        std::ostringstream str;

        str << "FPS = " << 1000.0f / frame_ms
            << "\n(" << frame_ms << " ms)"
            << "\nZoom = " << m_zoom.transformation().m_scale;
        m_dynamic_text_item->clear();
        add_text(str.str(), *m_dynamic_text_item);

        render_encoder.transformation(astral::Transformation());
        render_encoder.draw_rect(m_dynamic_text_item->bounding_box().as_rect(),
                                 render_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 0.0f, 0.50f))));
        render_encoder.draw_text(*m_dynamic_text_item,
                                 render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 0.85f))));
      }
      break;

    default:
      break;
    }

  stats = renderer().end();
  if (m_print_stats)
    {
      m_print_stats = false;
      std::cout << "frame ms = " << frame_ms << "\n";
      for (unsigned int i = 0; i < stats.size(); ++i)
        {
          std::cout << "\t" << stats_labels[i] << " = " << stats[i] << "\n";
        }
    }
}

float
GlyphTest::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, scale_delta, angle_delta;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);
  float thicken_rate(0.001f);
  bool thicken_changed(false);

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

  if (m_hud_mode != hud_show_glyph_info)
    {
      bool skew_changed(false);
      const float skew_rate(0.002f);

      if (keyboard_state[SDL_SCANCODE_UP] && !keyboard_state[SDL_SCANCODE_RETURN])
        {
          m_synth.m_skew.m_skew_x += delta * skew_rate;
          skew_changed = true;
        }

      if (keyboard_state[SDL_SCANCODE_DOWN] && !keyboard_state[SDL_SCANCODE_RETURN])
        {
          m_synth.m_skew.m_skew_x -= delta * skew_rate;
          skew_changed = true;
        }

      if (keyboard_state[SDL_SCANCODE_LEFT])
        {
          m_synth.m_skew.m_scale_x -= delta * skew_rate;
          skew_changed = true;
        }

      if (keyboard_state[SDL_SCANCODE_RIGHT])
        {
          m_synth.m_skew.m_scale_x += delta * skew_rate;
          skew_changed = true;
        }

      if (skew_changed)
        {
          m_glyph_hierarchy = nullptr;
          m_glyph_list.clear();
          std::cout << "Skew changed to skew_x = " << m_synth.m_skew.m_skew_x
                    << ", scale_x = " << m_synth.m_skew.m_scale_x << "\n"
                    << std::flush;
        }
    }

  if (keyboard_state[SDL_SCANCODE_UP] && keyboard_state[SDL_SCANCODE_RETURN])
    {
      thicken_changed = true;
      m_synth.m_thicken += delta * thicken_rate;
    }

  if (keyboard_state[SDL_SCANCODE_DOWN] && keyboard_state[SDL_SCANCODE_RETURN])
    {
      thicken_changed = true;
      m_synth.m_thicken -= delta * thicken_rate;
    }

  if (thicken_changed)
    {
      m_synth.m_thicken = astral::t_min(1.0f, astral::t_max(0.0f, m_synth.m_thicken));
      std::cout << "Glyph thickent set to " << m_synth.m_thicken << "\n";
    }

  scale_delta = 0.01f * delta;
  angle_delta = 0.0025f * delta * 180.0f / ASTRAL_PI;
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

  return return_value;
}

void
GlyphTest::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_SPACE:
          m_print_stats = true;
          break;

        case SDLK_w:
          m_synth.m_skew.m_skew_x = 0.0f;
          m_synth.m_skew.m_scale_x = 1.0f;
          m_synth.m_thicken = 0.0f;
          m_glyph_hierarchy.clear();
          break;

        case SDLK_q:
          reset_zoom_transformation();
          m_scale_pre_rotate.value() = m_scale_post_rotate.value() = astral::vec2(1.0f, 1.0f);
          m_rotate_angle.value() = 0.0f;
          break;

        case SDLK_p:
          cycle_value(m_hud_mode,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      hud_mode_number);
          break;

        case SDLK_e:
          m_draw_as_paths.value() = !m_draw_as_paths.value();
          std::cout << "Draw text as paths set to " << std::boolalpha
                    << m_draw_as_paths.value() << "\n";
          break;

        case SDLK_f:
          m_color_glyphs_observe_material.value() = !m_color_glyphs_observe_material.value();
          std::cout << "Color glyphs observe material set to " << std::boolalpha
                    << m_color_glyphs_observe_material.value() << "\n";
          break;
        }
    }
  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  GlyphTest M;
  return M.main(argc, argv);
}
