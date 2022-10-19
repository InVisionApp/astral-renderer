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

#include <random>
#include <cctype>
#include <string>
#include <fstream>
#include <sstream>

#include <astral/util/ostream_utility.hpp>
#include <astral/path.hpp>
#include <astral/animated_path.hpp>
#include <astral/renderer/gl3/render_target_gl3.hpp>
#include <astral/renderer/renderer.hpp>
#include <astral/text/freetype_face.hpp>
#include <astral/text/freetype_lib.hpp>
#include <astral/text/glyph_metrics.hpp>

#include "sdl_demo.hpp"
#include "render_engine_gl3_demo.hpp"
#include "PanZoomTracker.hpp"
#include "simple_time.hpp"
#include "UniformScaleTranslate.hpp"
#include "cycle_value.hpp"
#include "text_helper.hpp"
#include "demo_macros.hpp"

class Glyph:public astral::reference_counted<Glyph>::non_concurrent
{
public:
  Glyph(astral::Typeface &typeface, uint32_t glyph_code)
  {
    astral::GlyphIndex G(glyph_code);
    m_glyph = typeface.fetch_glyph(G);
  }

  const astral::Path&
  path(void) const
  {
    enum astral::fill_rule_t ignored;
    return *m_glyph.path(0, &ignored);
  }

  const astral::GlyphMetrics&
  metrics(void) const
  {
    return m_glyph.scalable_metrics();
  }

private:
  astral::Glyph m_glyph;
};

class Font:public astral::reference_counted<Font>::non_concurrent
{
public:
  Font(astral::reference_counted_ptr<astral::FreetypeFace::GeneratorFile> font_generator,
       astral::reference_counted_ptr<astral::FreetypeLib> lib)
  {
    m_typeface = astral::Typeface::create(font_generator->create_glyph_generator(8, lib));
    m_glyphs.resize(m_typeface->number_glyphs(), nullptr);
  }

  ~Font()
  {
    /* This is ... stupid. We need to clear m_glyphs first
     * before m_typeface because the glyph data references
     * into data of the typeface.
     */
    m_glyphs.clear();
  }

  astral::reference_counted_ptr<Glyph>
  fetch_glyph(uint32_t glyph_code)
  {
    ASTRALassert(glyph_code < m_glyphs.size());
    if (!m_glyphs[glyph_code])
      {
        m_glyphs[glyph_code] = ASTRALnew Glyph(*m_typeface, glyph_code);
      }
    return m_glyphs[glyph_code];
  }

  uint32_t
  glyph_code(uint32_t character_code)
  {
    return m_typeface->glyph_index(character_code).m_value;
  }

  astral::Typeface&
  typeface(void)
  {
    return *m_typeface;
  }

  const astral::TypefaceMetricsScalable&
  metrics(void) const
  {
    return m_typeface->scalable_metrics();
  }

  uint32_t
  number_glyphs(void) const
  {
    return m_glyphs.size();
  }

private:
  std::vector<astral::reference_counted_ptr<Glyph>> m_glyphs;
  astral::reference_counted_ptr<astral::Typeface> m_typeface;
};

class AnimatedGlyph:public astral::reference_counted<AnimatedGlyph>::non_concurrent
{
public:
  AnimatedGlyph(const Glyph& g0, const Glyph& g1):
    m_metrics0(g0.metrics()),
    m_metrics1(g1.metrics())
  {
    m_path.set(g0.path(), g1.path(), astral::AnimatedPath::LengthContourSorter());
  }

  const astral::GlyphMetrics&
  metrics0(void) const
  {
    return m_metrics0;
  }

  const astral::GlyphMetrics&
  metrics1(void) const
  {
    return m_metrics1;
  }

  const astral::AnimatedPath&
  path(void) const
  {
    return m_path;
  }

private:
  astral::GlyphMetrics m_metrics0, m_metrics1;
  astral::AnimatedPath m_path;
};

class AnimatedFont:public astral::reference_counted<AnimatedFont>::non_concurrent
{
public:
  class GlyphCodePair
  {
  public:
    astral::uvec2 m_v;

    bool
    operator<(const GlyphCodePair &rhs) const
    {
      return m_v < rhs.m_v;
    }
  };

  class CharacterCodePair
  {
  public:
    astral::uvec2 m_v;
  };

  AnimatedFont(astral::reference_counted_ptr<Font> font0,
               astral::reference_counted_ptr<Font> font1):
    m_font0(font0),
    m_font1(font1)
  {}

  astral::reference_counted_ptr<const AnimatedGlyph>
  glyph_from_character_code(uint32_t character_code,
                            astral::reference_counted_ptr<Glyph> *pg0,
                            astral::reference_counted_ptr<Glyph> *pg1)
  {
    CharacterCodePair K;

    K.m_v[0] = character_code;
    if (m_font0 == m_font1)
      {
        if (std::isupper(character_code))
          {
            K.m_v[1] = std::tolower(character_code);
          }
        else if (std::islower(character_code))
          {
            K.m_v[1] = std::toupper(character_code);
          }
        else
          {
            K.m_v[1] = character_code;
          }
      }
    else
      {
        K.m_v[1] = character_code;
      }

    return animated_glyph(K, pg0, pg1);
  }

  astral::reference_counted_ptr<const AnimatedGlyph>
  animated_glyph(CharacterCodePair K,
                 astral::reference_counted_ptr<Glyph> *pg0,
                 astral::reference_counted_ptr<Glyph> *pg1)
  {
    GlyphCodePair G;

    G.m_v[0] = m_font0->glyph_code(K.m_v[0]);
    G.m_v[1] = m_font1->glyph_code(K.m_v[1]);
    return animated_glyph(G, pg0, pg1);
  }

  astral::reference_counted_ptr<const AnimatedGlyph>
  animated_glyph(GlyphCodePair G,
                 astral::reference_counted_ptr<Glyph> *pg0,
                 astral::reference_counted_ptr<Glyph> *pg1)
  {
    std::map<GlyphCodePair, astral::reference_counted_ptr<AnimatedGlyph>>::iterator iter;

    *pg0 = m_font0->fetch_glyph(G.m_v[0]);
    *pg1 = m_font1->fetch_glyph(G.m_v[1]);

    iter = m_glyphs.find(G);
    if (iter == m_glyphs.end())
      {
        astral::reference_counted_ptr<AnimatedGlyph> A;
        astral::reference_counted_ptr<Glyph> &g0(*pg0);
        astral::reference_counted_ptr<Glyph> &g1(*pg1);

        A = ASTRALnew AnimatedGlyph(*g0, *g1);
        m_glyphs[G] = A;
        return A;
      }

    return iter->second;
  }

  const astral::TypefaceMetricsScalable&
  metrics0(void)
  {
    return m_font0->metrics();
  }

  const astral::TypefaceMetricsScalable&
  metrics1(void)
  {
    return m_font1->metrics();
  }

  Font&
  font0(void) const
  {
    return *m_font0;
  }

  Font&
  font1(void) const
  {
    return *m_font1;
  }

private:
  astral::reference_counted_ptr<Font> m_font0, m_font1;
  std::map<GlyphCodePair, astral::reference_counted_ptr<AnimatedGlyph>> m_glyphs;
};

class PerGlyph
{
public:
  void
  shift_down(unsigned int num_lines, float path0_height, float path1_height)
  {
    float num_linesf;

    num_linesf = static_cast<float>(num_lines);
    m_logical_transformation_glyph0.m_translation.y() += path0_height * num_linesf;
    m_logical_transformation_glyph1.m_translation.y() += path1_height * num_linesf;
  }

  UniformScaleTranslate<float> m_logical_transformation_glyph0;
  UniformScaleTranslate<float> m_logical_transformation_glyph1;
  astral::reference_counted_ptr<const AnimatedGlyph> m_glyph;
  astral::reference_counted_ptr<Glyph> m_glyph0, m_glyph1;
};

class PerLine
{
public:
  void
  reserve_space(unsigned int len)
  {
    m_path0_translates.reserve(len);
    m_path0_matrices.reserve(len);
    m_path1_translates.reserve(len);
    m_path1_matrices.reserve(len);
    m_t0_paths.reserve(len);
    m_t1_paths.reserve(len);
    m_animated_paths.reserve(len);
  }

  void
  add_glyph(const PerGlyph &G)
  {
    m_t0_paths.push_back(&G.m_glyph0->path());
    m_t1_paths.push_back(&G.m_glyph1->path());
    m_animated_paths.push_back(&G.m_glyph->path());

    m_path0_translates.push_back(G.m_logical_transformation_glyph0.m_translation);
    m_path1_translates.push_back(G.m_logical_transformation_glyph1.m_translation);

    m_path0_matrices.push_back(astral::scale_matrix(G.m_logical_transformation_glyph0.m_scale));
    m_path1_matrices.push_back(astral::scale_matrix(G.m_logical_transformation_glyph1.m_scale));
  }

  astral::CombinedPath
  path0(void) const
  {
    return astral::CombinedPath(astral::make_c_array(m_t0_paths),
                                astral::make_c_array(m_path0_translates),
                                astral::make_c_array(m_path0_matrices));
  }

  astral::CombinedPath
  path1(void) const
  {
    return astral::CombinedPath(astral::make_c_array(m_t1_paths),
                                astral::make_c_array(m_path1_translates),
                                astral::make_c_array(m_path1_matrices));
  }

  astral::CombinedPath
  path(float t) const
  {
    float s(1.0f - t);
    unsigned int N(m_path0_translates.size());

    m_path_t_translates.resize(N);
    m_path_t_matrices.resize(N);
    for (unsigned int i = 0; i < N; ++i)
      {
        m_path_t_translates[i] = s * m_path0_translates[i] + t * m_path1_translates[i];
        m_path_t_matrices[i] = s * m_path0_matrices[i] + t * m_path1_matrices[i];
      }

    return astral::CombinedPath(t, astral::make_c_array(m_animated_paths),
                                astral::make_c_array(m_path_t_translates),
                                astral::make_c_array(m_path_t_matrices));
  }

  void
  shift_down(int num_lines, float path0_height, float path1_height)
  {
    float num_linesf;

    num_linesf = static_cast<float>(num_lines);
    for (astral::vec2 &p : m_path0_translates)
      {
        p.y() += num_linesf * path0_height;
      }

    for (astral::vec2 &p : m_path1_translates)
      {
        p.y() += num_linesf * path1_height;
      }
  }

  unsigned int
  size(void) const
  {
    return m_animated_paths.size();
  }

private:
  std::vector<const astral::Path*> m_t0_paths, m_t1_paths;
  std::vector<const astral::AnimatedPath*> m_animated_paths;
  std::vector<astral::vec2> m_path0_translates, m_path1_translates;
  std::vector<astral::float2x2> m_path0_matrices, m_path1_matrices;
  mutable std::vector<astral::vec2> m_path_t_translates;
  mutable std::vector<astral::float2x2> m_path_t_matrices;
};

class AnimatedText:public render_engine_gl3_demo
{
public:
  AnimatedText(void);

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
  enum draw_mode_t
    {
     draw_animated_glyphs,
     draw_animated_glyphs_at_0,
     draw_t0_glyphs,
     draw_animated_glyphs_at_1,
     draw_t1_glyphs,

     number_draw_mods,
    };

  enum
    {
      basic_hud,
      basic_hud_with_commands,
      detail_level1_hud,
      detail_level2_hud,
      detail_level3_hud,

      number_hud_modes,
    };

  static
  astral::c_string
  label(enum draw_mode_t D)
  {
    static const astral::c_string labels[number_draw_mods] =
      {
        [draw_animated_glyphs] = "draw_animated_glyphs",
        [draw_animated_glyphs_at_0] = "draw_animated_glyphs_at_0",
        [draw_t0_glyphs] = "draw_t0_glyphs",
        [draw_animated_glyphs_at_1] = "draw_animated_glyphs_at_1",
        [draw_t1_glyphs] = "draw_t1_glyphs",
      };

    return labels[D];
  }

  float
  update_smooth_values(void);

  void
  reset_zoom_transformation(void);

  void
  create_font(void);

  void
  create_glyphs(void);

  void
  create_glyphs(AnimatedFont &font, std::istream &stream0, std::istream &stream1);

  bool
  grab_lines(std::istream &stream0, std::istream &stream1,
             std::string &line0, std::string &line1);

  void
  draw_glyph(astral::RenderEncoderBase encoder,
             astral::RenderValue<astral::Brush> fill_brush,
             astral::RenderValue<astral::Brush> stroke_brush,
             float t, const PerGlyph &G);

  void
  draw_line(astral::RenderEncoderBase encoder,
            astral::RenderValue<astral::Brush> fill_brush,
            astral::RenderValue<astral::Brush> stroke_brush,
            float t, const PerLine &L);

  void
  draw_combined_path(astral::RenderEncoderBase encoder,
                     const astral::CombinedPath &P,
                     astral::RenderValue<astral::Brush> fill_brush,
                     astral::RenderValue<astral::Brush> stroke_brush);

  float
  compute_animation_interpolate(void);

  void
  draw_hud(astral::RenderEncoderSurface encoder, float frame_ms);

  astral::FillParameters m_fill_params;
  astral::FillMaskProperties m_mask_fill_params;
  astral::MaskUsage m_mask_fill_usage_params;

  astral::StrokeParameters m_stroke_params;
  astral::StrokeMaskProperties m_mask_stroke_params;
  astral::MaskUsage m_mask_stroke_usage_params;

  command_separator m_demo_options;
  command_line_argument_value<std::string> m_font_file0;
  command_line_argument_value<std::string> m_font_file1;
  command_line_argument_value<std::string> m_text0, m_text1;
  command_line_argument_value<int> m_repeat_text_on_each_line;
  command_line_argument_value<int> m_repeat_text;
  command_line_argument_value<bool> m_use_file0, m_use_file1;
  command_line_argument_value<unsigned int> m_animation_time;
  command_line_argument_value<float> m_render_size;
  command_line_argument_value<unsigned int> m_number_characters_per_random_line;
  command_line_argument_value<unsigned int> m_number_random_lines;
  command_line_argument_value<bool> m_identical_formatting;
  command_line_argument_value<UniformScaleTranslate<float>> m_initial_camera;
  command_line_argument_value<astral::vec2> m_scale_pre_rotate, m_scale_post_rotate;
  command_line_argument_value<float> m_rotate_angle;
  command_line_argument_value<bool> m_use_lines;
  enumerated_command_line_argument_value<enum draw_mode_t> m_mode;
  command_line_argument_value<bool> m_stroke_path;
  command_line_argument_value<float> m_scale_factor;
  enumerated_command_line_argument_value<enum astral::filter_t> m_mask_filter;
  command_line_argument_value<float> m_alpha;
  command_line_argument_value<simple_time> m_glyph_time;
  enumerated_command_line_argument_value<enum astral::fill_rule_t&> m_fill_params_fill_rule;
  enumerated_command_line_argument_value<enum astral::anti_alias_t&> m_fill_params_aa;
  enumerated_command_line_argument_value<enum astral::fill_method_t&> m_fill_params_fill_method;
  enumerated_command_line_argument_value<enum astral::mask_type_t&> m_fill_mask_type;
  command_line_argument_value<bool&> m_stroke_params_sparse_mask;
  enumerated_command_line_argument_value<enum astral::mask_type_t&> m_stroke_mask_type;
  command_line_argument_value<float&> m_stroke_params_width;
  enumerated_command_line_argument_value<enum astral::join_t&> m_stroke_params_join;
  enumerated_command_line_argument_value<enum astral::cap_t&> m_stroke_params_cap;
  enumerated_command_line_argument_value<enum astral::join_t&> m_stroke_params_glue_join;
  enumerated_command_line_argument_value<enum astral::join_t&> m_stroke_params_glue_cusp_join;
  command_line_argument_value<float&> m_stroke_params_miter_limit;
  command_line_argument_value<bool&> m_stroke_params_draw_edges;

  simple_time m_draw_timer;
  average_timer m_frame_time_average;

  astral::reference_counted_ptr<astral::TextItem> m_text_item;
  astral::reference_counted_ptr<Font> m_font0, m_font1;
  astral::reference_counted_ptr<AnimatedFont> m_animated_font;
  std::vector<PerGlyph> m_glyphs;
  std::vector<PerLine> m_lines;

  PanZoomTrackerSDLEvent m_zoom;

  unsigned int m_hud_mode;
  std::vector<unsigned int> m_prev_stats;
  bool m_show_offscreen_alloc_info;
  astral::Renderer::OffscreenBufferAllocInfo m_offscreen_alloc_info;
};

//////////////////////////////////////////
//  methods
AnimatedText::
AnimatedText(void):
  m_mask_fill_usage_params(astral::mask_type_distance_field),
  m_mask_stroke_usage_params(astral::mask_type_distance_field),
  m_demo_options("Demo Options", *this),
  m_font_file0(DEFAULT_FONT, "font_file0", "TTF File from which to extract glyph(s)", *this),
  m_font_file1(DEFAULT_FONT, "font_file1", "TTF File from which to extract glyph(s)", *this),
  m_text0("Hello World", "text0", "Text (or file) to draw", *this),
  m_text1("World Hello", "text1", "Text (or file) to draw", *this),
  m_repeat_text_on_each_line(0, "repeat_text_on_each_line",
                             "For each line of text, repeat it this number of times on the same line",
                             *this),
  m_repeat_text(0, "repeat_text", "Repeat the entire text this number of times", *this),
  m_use_file0(false, "use_file0", "If true text represents a file to load", *this),
  m_use_file1(false, "use_file1", "If true text represents a file to load", *this),
  m_animation_time(3000, "animation_time", "Animation time between glyphs in ms", *this),
  m_render_size(48.0f, "render_size", "Render size for animated text", *this),
  m_number_characters_per_random_line(0, "number_characters_per_random_line",
                                      "Number of characters per added random line",
                                      *this),
  m_number_random_lines(0, "number_random_lines",
                        "number of lines of random characters to add to the text",
                        *this),
  m_identical_formatting(false, "identical_formatting",
                         "if true, make the formatting of the to and from identical",
                         *this),
  m_initial_camera(UniformScaleTranslate<float>(), "initial_camera", "Initial position of camera", *this),
  m_scale_pre_rotate(astral::vec2(1.0f, 1.0f), "scale_pre_rotate", "scaling transformation to apply to path before rotation, formatted as ScaleX:SaleY", *this),
  m_scale_post_rotate(astral::vec2(1.0f, 1.0f), "scale_post_rotate", "scaling transformation to apply to path after rotation, formatted as ScaleX:SaleY", *this),
  m_rotate_angle(0.0f, "rotate_angle", "rotation of path in degrees to apply to path", *this),
  m_use_lines(true, "use_lines", "If true, draw glyphs on a line together, if false draw each glyph seperately", *this),
  m_mode(draw_animated_glyphs,
         enumerated_string_type<enum draw_mode_t>(&label, number_draw_mods),
         "draw_mode", "mode to specify how to draw the glyphs", *this),
  m_stroke_path(false, "stroke_path", "If true, add a stroke to each glyph", *this),
  m_scale_factor(1.0f, "scale_factor",
                 "Scale factor at which to generate stroke and fill masks "
                 "a value of less than 1.0 indicates that the mask is at a lower resolution "
                 "than its display", *this),
  m_mask_filter(astral::filter_linear,
                enumerated_string_type<enum astral::filter_t>(&astral::label, astral::number_filter_modes),
                "mask_filter",
                "filter to apply when sampling from masks generated for stroking and filling",
                *this),
  m_alpha(1.0f, "alpha", "alpha value to apply to glyph drawing", *this),
  m_glyph_time(simple_time(),
               "glyph_time",
               "If set, pauses the timer for glyph aimation and specifies the intial time value in ms",
               *this),
  m_fill_params_fill_rule(m_fill_params.m_fill_rule,
                          enumerated_string_type<enum astral::fill_rule_t>(&astral::label, astral::number_fill_rule)
                          .add_entry("no_fill", astral::number_fill_rule, ""),
                          "fill_rule",
                          "initial fill rule to apply to path",
                          *this),
  m_fill_params_aa(m_fill_params.m_aa_mode,
                   enumerated_string_type<enum astral::anti_alias_t>(&astral::label, astral::number_anti_alias_modes),
                   "fill_aa",
                   "anti-aliasing mode to apply to path fill",
                   *this),
  m_fill_params_fill_method(m_mask_fill_params.m_sparse_mask,
                            enumerated_string_type<enum astral::fill_method_t>(&astral::label, astral::number_fill_method_t),
                            "fill_method",
                            "method for generating fill mask",
                            *this),
  m_fill_mask_type(m_mask_fill_usage_params.m_mask_type,
                   enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type),
                   "fill_mask_type",
                   "specifies the kind of mask to use when filling the path",
                   *this),
  m_stroke_params_sparse_mask(m_mask_stroke_params.m_sparse_mask, "sparse_stroke",
                              "if true, generate a sparse mask for stroking",
                              *this),
  m_stroke_mask_type(m_mask_stroke_usage_params.m_mask_type,
                     enumerated_string_type<enum astral::mask_type_t>(&astral::label, astral::number_mask_type),
                     "stroke_mask_type",
                     "specifies the kind of mask to use when stroking the path",
                     *this),
  m_stroke_params_width(m_stroke_params.m_width, "stroke_width", "stroking width", *this),
  m_stroke_params_join(m_stroke_params.m_join,
                       enumerated_string_type<enum astral::join_t>(&astral::label, astral::number_join_t),
                       "stroke_join",
                       "stroking join style",
                       *this),
  m_stroke_params_cap(m_stroke_params.m_cap,
                      enumerated_string_type<enum astral::cap_t>(&astral::label, astral::number_cap_t),
                      "stroke_cap",
                      "stroking cap style",
                      *this),
  m_stroke_params_glue_join(m_stroke_params.m_glue_join,
                            enumerated_string_type<enum astral::join_t>(&astral::label, astral::number_join_t),
                            "stroke_glue_join",
                            "how to draw glue joins when stroking",
                            *this),
  m_stroke_params_glue_cusp_join(m_stroke_params.m_glue_cusp_join,
                                 enumerated_string_type<enum astral::join_t>(&astral::label, astral::number_join_t),
                                 "stroke_glue_cusp_join",
                                 "how to draw glue joins at cusps when stroking",
                                 *this),
  m_stroke_params_miter_limit(m_stroke_params.m_miter_limit, "stroke_miter_limit", "Miter limit when stroking", *this),
  m_stroke_params_draw_edges(m_stroke_params.m_draw_edges, "stroke_draw_edges", "specifies if to draw edges when stroking", *this),
  m_frame_time_average(1000u),
  m_hud_mode(basic_hud),
  m_show_offscreen_alloc_info(false)
{
  std::cout << "Controls:"
            << "\n\tp: pause animation"
            << "\n\tshift-p: restart animation"
            << "\n\talt-space: toggle showing offscreen buffer allocation"
            << "\n\tspace: cycle through hud modes"
            << "\n\tq: reset transformation applied to rect"
            << "\n\tl: toggle drawing text one line or one character at a time"
            << "\n\td: cylce through drawing animated path, path at time 0 and path at time 1"
            << "\n\te: toggle stroking edges when stroking"
            << "\n\tr: cycle through different fill rules"
            << "\n\tshift-r: toggle sparse filling on/off"
            << "\n\ta: toggle filling with our without anti-aliasing"
            << "\n\tf: cycle through filling the path directly, drawing as clip-in only, or drawing both clip-in and clip-out"
            << "\n\tb: cycle through the blend modes"
            << "\n\tl: toggle rendering to layer"
            << "\n\tk: cycle through filter mode when sampling from the mask or layer "
            << "\n\tj: cycle through different join styles"
            << "\n\tc: cycle through different cap styles"
            << "\n\ts: toggle stroking on and off"
            << "\n\to: toggle anti-aliasing for stroking"
            << "\n\tshift-o: toggle sparse stroking on/off"
            << "\n\tv: toggle between stroking via arcs or quadratic curves"
            << "\n\tb/n: decrease/increate miter limit"
            << "\n\tg: cycle through different ways to use the offscreen mask for filling"
            << "\n\tt: cycle through different ways to use the offscreen mask for stroking"
            << "\n\tz/shift-z: increase/decrease rendering accuracy"
            << "\n\treturn + up/down: increase/decrease render fill scale factor"
            << "\n\talt + 1,2, ... 9: set render fill scale factor to 10%, 20%, ..., 90% repsectively"
            << "\n\talt + 0: set render fill scale factor to 100%"
            << "\n\tup/down arrow: increase/decrease opacity"
            << "\n\t6: increase horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-6: decrease horizontal pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t7: increase vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\tctrl-7: decrease vertical pre-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 6: increase horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-6: decrease horizontal post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + 7: increase vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\treturn + ctrl-7: decrease vertical post-rotate scale (hold left-shit for slow change, hold right-shift for faster change)"
            << "\n\t9/0 increase/decrease angle of rotation"
            << "\n\t[/] : decrease/incrase strokign width"
            << "\n\tLeft Mouse Drag: pan"
            << "\n\tHold Left Mouse, then drag up/down: zoom out/in"
            << "\n";
}

void
AnimatedText::
reset_zoom_transformation(void)
{
  /* Initialize zoom location to be identity */
  m_zoom.transformation(UniformScaleTranslate<float>());
}

void
AnimatedText::
create_font(void)
{
  astral::reference_counted_ptr<astral::FreetypeFace::GeneratorFile> font_generator;

  font_generator = astral::FreetypeFace::GeneratorFile::create(m_font_file0.value().c_str(), 0);
  m_font0 = ASTRALnew Font(font_generator, freetype_lib());

  if (m_font_file0.value() != m_font_file1.value())
    {
      font_generator = astral::FreetypeFace::GeneratorFile::create(m_font_file1.value().c_str(), 0);
      m_font1 = ASTRALnew Font(font_generator, freetype_lib());
    }
  else
    {
      m_font1 = m_font0;
    }

  m_animated_font = ASTRALnew AnimatedFont(m_font0, m_font1);
}

void
AnimatedText::
create_glyphs(void)
{
  std::istream *s0, *s1;

  create_font();
  if (m_use_file0.value())
    {
      s0 = ASTRALnew std::ifstream(m_text0.value().c_str());
    }
  else
    {
      s0 = ASTRALnew std::istringstream(m_text0.value());
    }

  if (m_use_file1.value())
    {
      s1 = ASTRALnew std::ifstream(m_text1.value().c_str());
    }
  else
    {
      s1 = ASTRALnew std::istringstream(m_text1.value());
    }

  create_glyphs(*m_animated_font, *s0, *s1);

  ASTRALdelete(s0);
  ASTRALdelete(s1);
}

bool
AnimatedText::
grab_lines(std::istream &stream0, std::istream &stream1,
           std::string &line0, std::string &line1)
{
  bool r0, r1;

  line0.clear();
  line1.clear();

  r0 = bool(std::getline(stream0, line0));
  r1 = bool(std::getline(stream1, line1));

  return r0 || r1;
}

void
AnimatedText::
create_glyphs(AnimatedFont &font, std::istream &stream0, std::istream &stream1)
{
  astral::vec2 pen0(0.0f), pen1(0.0f);
  float ratio0, ratio1;
  float pen_y_advance0, pen_y_advance1;
  std::string line0, line1;

  ratio0 = m_render_size.value() / font.metrics0().m_units_per_EM;
  ratio1 = m_render_size.value() / font.metrics1().m_units_per_EM;

  m_stroke_params.m_width = 2.0f;

  pen_y_advance0 = ratio0 * font.metrics0().m_height;
  pen_y_advance1 = ratio1 * font.metrics1().m_height;

  if (m_identical_formatting.value())
    {
      pen_y_advance0 = pen_y_advance1 = astral::t_max(pen_y_advance0, pen_y_advance1);
    }

  pen0.y() = pen_y_advance0;
  pen1.y() = pen_y_advance1;

  while (grab_lines(stream0, stream1, line0, line1))
    {
      m_lines.push_back(PerLine());
      m_lines.back().reserve_space(astral::t_max(line0.length(), line1.length()));
      for (int i = 0; i < m_repeat_text_on_each_line.value() + 1; ++i)
        {
          unsigned int count;
          std::string *L;

          count = astral::t_max(line0.length(), line1.length());
          L = (line0.length() < count) ? &line0 : &line1;
          while (L->length() < count)
            {
              L->push_back(' ');
            }

          for (unsigned int c = 0; c < count; ++c)
            {
              PerGlyph G;
              AnimatedFont::CharacterCodePair C;
              float x_advance0, x_advance1;

              C.m_v[0] = line0[c];
              C.m_v[1] = line1[c];
              G.m_glyph = font.animated_glyph(C, &G.m_glyph0, &G.m_glyph1);

              G.m_logical_transformation_glyph0.m_scale = ratio0;
              G.m_logical_transformation_glyph0.m_translation = pen0;

              G.m_logical_transformation_glyph1.m_scale = ratio1;
              G.m_logical_transformation_glyph1.m_translation = pen1;

              m_glyphs.push_back(G);
              m_lines.back().add_glyph(G);

              x_advance0 = ratio0 * G.m_glyph->metrics0().m_advance.x();
              x_advance1 = ratio1 * G.m_glyph->metrics1().m_advance.x();

              if (m_identical_formatting.value())
                {
                  x_advance0 = x_advance1 = astral::t_max(x_advance0, x_advance1);
                }

              pen0.x() += x_advance0;
              pen1.x() += x_advance1;
            }
        }

      pen0.y() += pen_y_advance0;
      pen1.y() += pen_y_advance1;

      pen0.x() = 0.0f;
      pen1.x() = 0.0f;
    }

  std::linear_congruential_engine<std::uint_fast32_t, 48271, 0, 2147483647> rd;
  std::mt19937 gen0(rd()), gen1(rd());
  std::uniform_int_distribution<> distrib0(33, 126);
  std::uniform_int_distribution<> distrib1(33, 126);

  for (unsigned int i = 0; i < m_number_random_lines.value(); ++i)
    {
      m_lines.push_back(PerLine());
      m_lines.back().reserve_space(m_number_characters_per_random_line.value());
      for (unsigned int j = 0; j < m_number_characters_per_random_line.value(); ++j)
        {
          AnimatedFont::CharacterCodePair code;
          PerGlyph G;
          float x_advance0, x_advance1;

          code.m_v[0] = distrib0(gen0);
          code.m_v[1] = distrib1(gen1);
          G.m_glyph = font.animated_glyph(code, &G.m_glyph0, &G.m_glyph1);

          G.m_logical_transformation_glyph0.m_scale = ratio0;
          G.m_logical_transformation_glyph0.m_translation = pen0;

          G.m_logical_transformation_glyph1.m_scale = ratio1;
          G.m_logical_transformation_glyph1.m_translation = pen1;

          m_glyphs.push_back(G);
          m_lines.back().add_glyph(G);

          x_advance0 = ratio0 * G.m_glyph->metrics0().m_advance.x();
          x_advance1 = ratio1 * G.m_glyph->metrics1().m_advance.x();

          if (m_identical_formatting.value())
            {
              x_advance0 = x_advance1 = astral::t_max(x_advance0, x_advance1);
            }

          pen0.x() += x_advance0;
          pen1.x() += x_advance1;
        }

      pen0.y() += pen_y_advance0;
      pen1.y() += pen_y_advance1;

      pen0.x() = 0.0f;
      pen1.x() = 0.0f;
    }

  unsigned int num_glyphs(m_glyphs.size());
  unsigned int num_lines(m_lines.size());

  m_glyphs.reserve(num_glyphs * (m_repeat_text.value() + 1));
  m_lines.reserve(num_lines * (m_repeat_text.value() + 1));
  for (int i = 0, num_lines = m_lines.size(); i < m_repeat_text.value(); ++i)
    {
      unsigned int gsz(m_glyphs.size()), lsz(m_lines.size());
      unsigned int shift_down((i + 1) * num_lines);

      m_glyphs.resize(m_glyphs.size() + num_glyphs);
      std::copy(m_glyphs.begin(), m_glyphs.begin() + num_glyphs, m_glyphs.begin() + gsz);
      for (auto iter = m_glyphs.begin() + gsz, end = iter + num_glyphs; iter != end; ++iter)
        {
          iter->shift_down(shift_down, pen_y_advance0, pen_y_advance1);
        }

      m_lines.resize(m_lines.size() + num_lines);
      std::copy(m_lines.begin(), m_lines.begin() + num_lines, m_lines.begin() + lsz);
      for (auto iter = m_lines.begin() + lsz, end = iter + num_lines; iter != end; ++iter)
        {
          iter->shift_down(shift_down, pen_y_advance0, pen_y_advance1);
        }
    }
}

void
AnimatedText::
init_gl(int, int)
{
  create_glyphs();
  m_zoom.transformation(m_initial_camera.value());
  m_prev_stats.resize(renderer().stats_labels().size(), 0);

  float pixel_size(32.0f);
  astral::Font font(default_typeface(), pixel_size);
  m_text_item = astral::TextItem::create(font);

  if (!m_glyph_time.set_by_command_line())
    {
      m_glyph_time.value().restart();
    }
}

void
AnimatedText::
draw_combined_path(astral::RenderEncoderBase encoder,
                   const astral::CombinedPath &P,
                   astral::RenderValue<astral::Brush> fill_brush,
                   astral::RenderValue<astral::Brush> stroke_brush)
{
  if (m_fill_params.m_fill_rule != astral::number_fill_rule)
    {
      m_mask_fill_usage_params.m_filter = m_mask_filter.value();
      encoder.fill_paths(P,
                         m_fill_params,
                         fill_brush,
                         astral::blend_porter_duff_src_over,
                         m_mask_fill_usage_params,
                         m_mask_fill_params);
    }

  if (m_stroke_path.value())
    {
      m_mask_stroke_usage_params.m_filter = m_mask_filter.value();
      encoder.stroke_paths(P, m_stroke_params,
                           stroke_brush,
                           astral::blend_porter_duff_src_over,
                           m_mask_stroke_usage_params,
                           m_mask_stroke_params);
    }
}

void
AnimatedText::
draw_glyph(astral::RenderEncoderBase encoder,
           astral::RenderValue<astral::Brush> fill_brush,
           astral::RenderValue<astral::Brush> stroke_brush,
           float t, const PerGlyph &G)
{
  astral::CombinedPath P;

  switch (m_mode.value())
    {

    case number_draw_mods:
      ASTRALfailure("Illegal draw mode");
      //fall through

    case draw_animated_glyphs:
    case draw_animated_glyphs_at_0:
    case draw_animated_glyphs_at_1:
      {
        UniformScaleTranslate<float> sc;

        sc = UniformScaleTranslate<float>::interpolate(G.m_logical_transformation_glyph0,
                                                G.m_logical_transformation_glyph1,
                                                t);
        P = astral::CombinedPath(t, G.m_glyph->path(),
                                 sc.m_translation,
                                 astral::vec2(sc.m_scale, sc.m_scale));
      }
      break;

    case draw_t0_glyphs:
    case draw_t1_glyphs:
      {
        const UniformScaleTranslate<float> *sc;
        const astral::Path *path;

        sc = (m_mode.value() == draw_t0_glyphs) ?
          &G.m_logical_transformation_glyph0:
          &G.m_logical_transformation_glyph1;

        path = (m_mode.value() == draw_t0_glyphs) ?
          &G.m_glyph0->path():
          &G.m_glyph1->path();

        P = astral::CombinedPath(*path,
                                 sc->m_translation,
                                 astral::vec2(sc->m_scale, sc->m_scale));
      }
      break;
    }

  draw_combined_path(encoder, P, fill_brush, stroke_brush);
}

void
AnimatedText::
draw_line(astral::RenderEncoderBase encoder,
          astral::RenderValue<astral::Brush> fill_brush,
          astral::RenderValue<astral::Brush> stroke_brush,
          float t, const PerLine &L)
{
  astral::CombinedPath P;

  switch (m_mode.value())
    {
    case draw_animated_glyphs_at_0:
    case draw_animated_glyphs_at_1:
    case draw_animated_glyphs:
      P = L.path(t);
      break;

    case draw_t0_glyphs:
      P = L.path0();
      break;

    case draw_t1_glyphs:
      P = L.path1();
      break;

    case number_draw_mods:
      ASTRALfailure("Illegal draw mode");
      P = L.path(t);
      break;
    }

  draw_combined_path(encoder, P, fill_brush, stroke_brush);
}

float
AnimatedText::
compute_animation_interpolate(void)
{
  int32_t ms;
  float t;

  ms = m_glyph_time.value().elapsed() % (2 * m_animation_time.value());
  t = static_cast<float>(ms) / m_animation_time.value();
  t = astral::t_min(2.0f, astral::t_max(0.0f, t));
  t = (t > 1.0f) ? 2.0f - t : t;
  t = astral::t_min(1.0f, astral::t_max(0.0f, t));

  return t;
}

void
AnimatedText::
draw_hud(astral::RenderEncoderSurface encoder, float frame_ms)
{
  static const enum astral::Renderer::renderer_stats_t vs[] =
    {
      astral::Renderer::number_sparse_fill_subrects_clipping,
      astral::Renderer::number_sparse_fill_subrect_skip_clipping,
      astral::Renderer::number_sparse_fill_curves_clipped,
      astral::Renderer::number_sparse_fill_curves_mapped,
      astral::Renderer::number_sparse_fill_contours_clipped,
      astral::Renderer::number_sparse_fill_late_culled_contours,
      astral::Renderer::number_virtual_buffer_pixels,
      astral::Renderer::number_non_degenerate_virtual_buffers,
      astral::Renderer::number_offscreen_render_targets,
      astral::Renderer::number_vertices_streamed,
      astral::Renderer::number_static_u32vec4_streamed,
      astral::Renderer::number_virtual_buffers,
    };

  static const enum astral::RenderBackend::render_backend_stats_t bvs[] =
    {
      astral::RenderBackend::stats_number_draws,
      astral::RenderBackend::stats_vertices,
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
  std::ostringstream hud_text;

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

  /* draw the hud in a fixed locaiton */
  encoder.transformation(astral::Transformation());
  hud_text << "Resolution = " << dimensions() << "\n"
           << "average over " << m_frame_time_average.interval_ms() << " ms: "
           << m_frame_time_average.average_elapsed_ms()
           << m_frame_time_average.parity_string()
           << "\n"
           << "Zoom = " << m_zoom.transformation().m_scale << "\n"
           << "Hud Level: " << m_hud_mode << " [space]\n";

  if (m_hud_mode >= basic_hud_with_commands)
    {
      hud_text << "Render Accuracy: " << renderer().default_render_accuracy() << " [z]\n"
               << "Draw mode:" << label(m_mode.value()) << " [d]\n"
               << "ShowOffscreenAllocation: " << std::boolalpha
               << m_show_offscreen_alloc_info << " [alt-space]\n"
               << "Animation paused: " << m_glyph_time.value().paused() << " [p]\n";

      if (m_fill_params.m_fill_rule != astral::number_fill_rule
          || m_stroke_path.value())
        {
          hud_text << "MaskScaleFactor:" << m_scale_factor.value() << " [alt-0, alt-1, ..., alt-9]\n"
                   << "MaskFilter: " << astral::label(m_mask_filter.value()) << " [k]\n";
        }

      hud_text << "Stroking: " << std::boolalpha << m_stroke_path.value() << " [s]\n";
      if (m_stroke_path.value())
        {
          hud_text << "\tSparse: " << std::boolalpha
                   << m_mask_stroke_params.m_sparse_mask << " [o]\n"
                   << "\tJoin Style: " << astral::label(m_stroke_params.m_join)
                   << " [j]\n"
                   << "\tCap Style: " << astral::label(m_stroke_params.m_cap)
                   << " [c]\n"
                   << "\tDraw Edges: " << std::boolalpha
                   << m_stroke_params.m_draw_edges << " [e]\n"
                   << "\tMaskType: " << astral::label(m_mask_stroke_usage_params.m_mask_type)
                   << " [t]\n";
        }

      if (m_fill_params.m_fill_rule == astral::number_fill_rule)
        {
          hud_text << "Filling off [r]\n";
        }
      else
        {
          hud_text << "Filling: " << astral::label(m_fill_params.m_fill_rule) << " [r]\n"
                   << "\tSparse: " << astral::label(m_mask_fill_params.m_sparse_mask) << " [shift-r]\n"
                   << "\tAnti-alias: " << astral::label(m_fill_params.m_aa_mode) << " [a]\n"
                   << "\tMaskType: " << astral::label(m_mask_fill_usage_params.m_mask_type) << " [g]\n";
        }
    }

  set_and_draw_hud(encoder, frame_ms,
                   astral::make_c_array(m_prev_stats),
                   *m_text_item, hud_text.str(),
                   vs_p, bvs_p, gvs_p);
}

void
AnimatedText::
draw_frame(void)
{
  float t, frame_ms;
  astral::Transformation base_tr;
  astral::c_array<const unsigned int> stats;
  astral::RenderEncoderSurface encoder;

  m_frame_time_average.increment_counter();
  frame_ms = update_smooth_values();
  base_tr = m_zoom.transformation().astral_transformation();

  base_tr.scale(m_scale_pre_rotate.value());
  base_tr.rotate(m_rotate_angle.value() * (ASTRAL_PI / 180.0f));
  base_tr.scale(m_scale_post_rotate.value());

  t = compute_animation_interpolate();

  // std::cout << std::setprecision(9)
  //        << "Z = " << m_zoom.transformation().m_scale
  //        << ", TR = " << m_zoom.transformation().m_translation
  //        << ", t = " << t << "\n";

  if (m_mode.value() == draw_animated_glyphs_at_0)
    {
      t = 0.0f;
    }
  else if (m_mode.value() == draw_animated_glyphs_at_1)
    {
      // various test values on letter R for cracks
      t = 1.0f; //0.982667; //0.999444f; //0.996556f;
    }

  encoder = renderer().begin(render_target());
  encoder.transformation(base_tr);

  m_mask_fill_params.render_scale_factor(m_scale_factor.value());
  m_mask_stroke_params.render_scale_factor(m_scale_factor.value());

  astral::RenderValue<astral::Brush> fill_brush, stroke_brush;

  fill_brush = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, m_alpha.value())));
  stroke_brush = encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, m_alpha.value())));

  if (m_use_lines.value())
    {
      for (const PerLine &L : m_lines)
        {
          draw_line(encoder, fill_brush, stroke_brush, t, L);
        }
    }
  else
    {
      for (const PerGlyph &G : m_glyphs)
        {
          draw_glyph(encoder, fill_brush, stroke_brush, t, G);
        }
    }

  if (!pixel_testing())
    {
      draw_hud(encoder, frame_ms);
    }

  /* draw the previous frames offscreen alloc info at the top */
  if (m_show_offscreen_alloc_info)
    {
      draw_offscreen_alloc_hud(astral::vec2(dimensions()),
                               encoder,
                               m_offscreen_alloc_info);
    }

  stats = renderer().end(&m_offscreen_alloc_info);
  ASTRALassert(m_prev_stats.size() == stats.size());
  std::copy(stats.begin(), stats.end(), m_prev_stats.begin());

  if (stats[renderer().stat_index(astral::Renderer::number_sparse_fill_clipping_errors)] != 0)
    {
      astral::generic_data Z, trX, trY;

      Z.f = m_zoom.transformation().m_scale;
      trX.f = m_zoom.transformation().m_translation.x();
      trY.f = m_zoom.transformation().m_translation.y();

      std::cout << "Clipping error encountered at:\n"
                << "\tZ = " << print_float_and_bits(m_zoom.transformation().m_scale) << "\n"
                << "\tTR = " << print_float_and_bits(m_zoom.transformation().m_translation) << "\n"
                << "\tt = " << print_float_and_bits(t) << "\n"
                << "\tpre-rotate = " << print_float_and_bits(m_scale_pre_rotate.value()) << "\n"
                << "\trotate = " << print_float_and_bits(m_rotate_angle.value()) << "\n"
                << "\tpost-rotate = " << print_float_and_bits(m_scale_post_rotate.value()) << "\n";
    }
}

float
AnimatedText::
update_smooth_values(void)
{
  const Uint8 *keyboard_state = SDL_GetKeyboardState(nullptr);
  float return_value, delta, scale_delta, angle_delta;
  astral::vec2 *scale_ptr(nullptr);
  astral::c_string scale_txt(nullptr);
  bool alpha_changed(false), scale_factor_changed(false);
  const float alpha_rate(0.001f);
  const float scale_rate(0.0001f);
  const float stroke_rate(0.1f / m_zoom.transformation().m_scale);
  const float miter_rate(0.02f);
  float scale_factor_delta(0.0f);
  bool alt_held;

  ASTRALassert(keyboard_state != nullptr);
  return_value = delta = static_cast<float>(m_draw_timer.restart_us()) * 0.001f;

  alt_held = keyboard_state[SDL_SCANCODE_LALT] || keyboard_state[SDL_SCANCODE_RALT];

  if (keyboard_state[SDL_SCANCODE_LSHIFT])
    {
      delta *= 0.1f;
    }
  if (keyboard_state[SDL_SCANCODE_RSHIFT])
    {
      delta *= 10.0f;
    }

  if (keyboard_state[SDL_SCANCODE_N])
    {
      m_stroke_params.m_miter_limit
        = astral::t_max(0.0f, m_stroke_params.m_miter_limit - delta * miter_rate);
      std::cout << "Miter limit set to: " << m_stroke_params.m_miter_limit << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_M])
    {
      m_stroke_params.m_miter_limit += delta * miter_rate;
      std::cout << "Miter limit set to: " << m_stroke_params.m_miter_limit << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET])
    {
      m_stroke_params.m_width += stroke_rate * delta;
    }

  if (keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      m_stroke_params.m_width -= stroke_rate * delta;
      m_stroke_params.m_width = astral::t_max(m_stroke_params.m_width, 0.0f);
    }

  if (keyboard_state[SDL_SCANCODE_RIGHTBRACKET] || keyboard_state[SDL_SCANCODE_LEFTBRACKET])
    {
      std::cout << "Stroke width set to: "
                << m_stroke_params.m_width
                << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_UP])
    {
      if (keyboard_state[SDL_SCANCODE_RETURN])
        {
          scale_factor_delta += delta * scale_rate;
          scale_factor_changed = true;
        }
      else
        {
          m_alpha.value() += delta * alpha_rate;
          alpha_changed = true;
        }
    }

  if (keyboard_state[SDL_SCANCODE_DOWN])
    {
      if (keyboard_state[SDL_SCANCODE_RETURN])
        {
          scale_factor_delta -= delta * scale_rate;
          scale_factor_changed = true;
        }
      else
        {
          m_alpha.value() -= delta * alpha_rate;
          alpha_changed = true;
        }
    }

  if (alpha_changed)
    {
      m_alpha.value() = astral::t_max(0.0f, astral::t_min(1.0f, m_alpha.value()));
      std::cout << "alpha set to " << m_alpha.value()
                << "(" << int(m_alpha.value() * 255.0f) << ")\n"
                << std::flush;
    }

  if (scale_factor_changed)
    {
      m_scale_factor.value() += scale_factor_delta;
      m_scale_factor.value() = astral::t_max(0.0f, m_scale_factor.value());
      std::cout << "Fill path scale factor set to " << m_scale_factor.value()
                << "\n" << std::flush;
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

  if (keyboard_state[SDL_SCANCODE_6] && !alt_held)
    {
      scale_ptr->x() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }
  if (keyboard_state[SDL_SCANCODE_7] && !alt_held)
    {
      scale_ptr->y() += scale_delta;
      std::cout << scale_txt << " set to: " << *scale_ptr << "\n";
    }

  if (keyboard_state[SDL_SCANCODE_9] && !alt_held)
    {
      m_rotate_angle.value() += angle_delta;
      if (angle_delta > 360.0f)
        {
          m_rotate_angle.value() -= 360.0f;
        }
      std::cout << "Angle set to: " << m_rotate_angle.value() << " degrees\n";
    }
  if (keyboard_state[SDL_SCANCODE_0] && !alt_held)
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
AnimatedText::
handle_event(const SDL_Event &ev)
{
  m_zoom.handle_event(ev);
  if (ev.type == SDL_KEYDOWN)
    {
      switch(ev.key.keysym.sym)
        {
        case SDLK_p:
          if (ev.key.keysym.mod & KMOD_SHIFT)
            {
              m_glyph_time.value().restart();
            }
          else
            {
              m_glyph_time.value().pause(!m_glyph_time.value().paused());
              if (m_glyph_time.value().paused())
                {
                  std::cout << "Animation paused at " << compute_animation_interpolate() << "\n";
                }
            }
          break;

        case SDLK_SPACE:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              m_show_offscreen_alloc_info = !m_show_offscreen_alloc_info;
              std::cout << "Show offscreen buffer allocation set to " << std::boolalpha
                        << m_show_offscreen_alloc_info << "\n";
            }
          else
            {
              cycle_value(m_hud_mode, ev.key.keysym.mod & KMOD_SHIFT, number_hud_modes);
            }
          break;

        case SDLK_l:
          m_use_lines.value() = !m_use_lines.value();
          if (m_use_lines.value())
            {
              std::cout << "Draw text one line at a time\n";
            }
          else
            {
              std::cout << "Draw text one character at a time\n";
            }
          break;

        case SDLK_d:
          {
            cycle_value(m_mode.value(),
                        ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                        number_draw_mods);
            std::cout << "Draw mode set to " << label(m_mode.value()) << "\n";
          }
          break;

        case SDLK_q:
          reset_zoom_transformation();
          m_scale_pre_rotate.value() = m_scale_post_rotate.value() = astral::vec2(1.0f, 1.0f);
          m_rotate_angle.value() = 0.0f;
          break;

        case SDLK_e:
          m_stroke_params.m_draw_edges = !m_stroke_params.m_draw_edges;
          if (m_stroke_params.m_draw_edges)
            {
              std::cout << "Stroke with edges\n";
            }
          else
            {
              std::cout << "Stroke without edges\n";
            }
          break;

        case SDLK_s:
          m_stroke_path.value() = !m_stroke_path.value();
          if (m_stroke_path.value())
            {
              std::cout << "Stroking path: ON\n";
            }
          else
            {
              std::cout << "Stroking path: OFF\n";
            }
          break;

        case SDLK_a:
          cycle_value(m_fill_params.m_aa_mode,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_anti_alias_modes);
          std::cout << "Fill anti-aliasing set to "
                    << astral::label(m_fill_params.m_aa_mode) << "\n";
          break;

        case SDLK_o:
          if (m_stroke_path.value())
            {
              m_mask_stroke_params.m_sparse_mask = !m_mask_stroke_params.m_sparse_mask;
              std::cout << "SparseStroking set to " << std::boolalpha
                        << m_mask_stroke_params.m_sparse_mask << "\n";
            }
          break;

        case SDLK_r:
          if (ev.key.keysym.mod & KMOD_SHIFT)
            {
              cycle_value(m_mask_fill_params.m_sparse_mask, false, astral::number_fill_method_t);
              std::cout << "Filling with sparse mask set to: "
                        << astral::label(m_mask_fill_params.m_sparse_mask)
                        << "\n";
            }
          else
            {
              cycle_value(m_fill_params.m_fill_rule,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::number_fill_rule + 1);
              if (m_fill_params.m_fill_rule != astral::number_fill_rule)
                {
                  std::cout << "Fill rule set to "
                            << astral::label(m_fill_params.m_fill_rule) << "\n";
                }
              else
                {
                  std::cout << "Filling off\n";
                }
            }
          break;

        case SDLK_j:
          if (m_stroke_path.value())
            {
              cycle_value(m_stroke_params.m_join,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::number_join_t + 1);
              std::cout << "Join style set to "
                        << astral::label(m_stroke_params.m_join) << "\n";
            }
          break;

        case SDLK_c:
          if (m_stroke_path.value())
            {
              cycle_value(m_stroke_params.m_cap,
                          ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                          astral::number_cap_t);
              std::cout << "Cap style set to "
                        << astral::label(m_stroke_params.m_cap) << "\n";
            }
          break;

        case SDLK_g:
          cycle_value(m_mask_fill_usage_params.m_mask_type,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_mask_type);
          std::cout << "FillMask mode set to: " << astral::label(m_mask_fill_usage_params.m_mask_type) << "\n";
          break;


        case SDLK_t:
          cycle_value(m_mask_stroke_usage_params.m_mask_type,
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_mask_type);
          std::cout << "StrokeMask mode set to: " << astral::label(m_mask_stroke_usage_params.m_mask_type) << "\n";
          break;

        case SDLK_k:
          cycle_value(m_mask_filter.value(),
                      ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT),
                      astral::number_filter_modes);
          std::cout << "Mask filter mode mode set to " << astral::label(m_mask_filter.value()) << "\n";
          break;

        case SDLK_z:
          if (ev.key.keysym.mod & (KMOD_SHIFT|KMOD_CTRL|KMOD_ALT))
            {
              renderer().default_render_accuracy(2.0f * renderer().default_render_accuracy());
            }
          else
            {
              renderer().default_render_accuracy(0.5f * renderer().default_render_accuracy());
            }
          std::cout << "Render accuracy set to "
                    << renderer().default_render_accuracy() << "\n";
          break;

        case SDLK_0:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              m_scale_factor.value() = 1.0f;
              std::cout << "Mask render scale factor set to " << m_scale_factor.value()
                        << "\n" << std::flush;
            }
          break;

        case SDLK_1:
        case SDLK_2:
        case SDLK_3:
        case SDLK_4:
        case SDLK_5:
        case SDLK_6:
        case SDLK_7:
        case SDLK_8:
        case SDLK_9:
          if (ev.key.keysym.mod & KMOD_ALT)
            {
              int v;
              float fv;

              v = (ev.key.keysym.sym - SDLK_1) + 1;
              fv = static_cast<float>(v) * 0.1f;
              m_scale_factor.value() = fv;
              std::cout << "Mask render scale factor set to " << m_scale_factor.value()
                        << "\n" << std::flush;
            }
          break;

        } // of switch()
    }
  render_engine_gl3_demo::handle_event(ev);
}

int
main(int argc, char **argv)
{
  AnimatedText M;
  return M.main(argc, argv);
}
