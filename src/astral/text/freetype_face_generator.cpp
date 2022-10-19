/*!
 * \file freetype_face_generator.cpp
 * \brief file freetype_face_generator.cpp
 *
 * Copyright 2019 by InvisionApp.
 *
 * Contact: kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_TRUETYPE_TABLES_H
#include FT_OUTLINE_H
#include FT_TYPE1_TABLES_H


#if FREEYPE_MARJOR > 3 || (FREETYPE_MAJOR >= 2 && FREETYPE_MINOR >= 10)
#include FT_COLOR_H
#define COLOR_GLYPH_LAYER_SUPPORTED
#else
#warning !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#warning !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#warning Version of Freetype is pre 2.10, scalable color glyphs not available
#warning !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#warning !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
#endif

#include <astral/path.hpp>
#include <astral/text/freetype_face.hpp>

namespace
{
  class OutlineDecompser
  {
  public:
    static
    void
    extract_path(astral::Path *dst, FT_Outline &outline, bool negate_y)
    {
      FT_Outline_Funcs funcs;
      OutlineDecompser decomposer(dst, negate_y);

      funcs.move_to = &OutlineDecompser::path_move_to;
      funcs.line_to = &OutlineDecompser::path_line_to;
      funcs.conic_to = &OutlineDecompser::path_conic_to;
      funcs.cubic_to = &OutlineDecompser::path_cubic_to;
      funcs.shift = 0;
      funcs.delta = 0;

      FT_Outline_Decompose(&outline, &funcs, &decomposer);
      if (decomposer.m_has_curves)
        {
          dst->close();
        }
    }

  private:
    OutlineDecompser(astral::Path *p, bool negate_y):
      m_path(p),
      m_has_curves(false),
      m_has_move(false),
      m_y_mult(negate_y ? -1 : 1)
    {}

    static
    int
    path_move_to(const FT_Vector *pt, void *user)
    {
      OutlineDecompser *p;
      p = static_cast<OutlineDecompser*>(user);

      if (p->m_has_move)
        {
          p->m_path->close();
        }
      p->m_has_move = true;
      p->m_path->move(astral::vec2(pt->x, p->m_y_mult * pt->y));
      return 0;
    }

    static
    int
    path_line_to(const FT_Vector *pt, void *user)
    {
      OutlineDecompser *p;
      p = static_cast<OutlineDecompser*>(user);
      p->m_has_curves = true;
      p->m_path->line_to(astral::vec2(pt->x, p->m_y_mult * pt->y));
      return 0;
    }

    static
    int
    path_conic_to(const FT_Vector *control_pt,
                  const FT_Vector *pt, void *user)
    {
      OutlineDecompser *p;
      p = static_cast<OutlineDecompser*>(user);
      p->m_has_curves = true;
      p->m_path->quadratic_to(astral::vec2(control_pt->x, p->m_y_mult * control_pt->y),
                              astral::vec2(pt->x, p->m_y_mult * pt->y));
      return 0;
    }

    static
    int
    path_cubic_to(const FT_Vector *control_pt0,
                  const FT_Vector *control_pt1,
                  const FT_Vector *pt, void *user)
    {
      OutlineDecompser *p;
      p = static_cast<OutlineDecompser*>(user);
      p->m_has_curves = true;
      p->m_path->cubic_to(astral::vec2(control_pt0->x, p->m_y_mult * control_pt0->y),
                          astral::vec2(control_pt1->x, p->m_y_mult * control_pt1->y),
                          astral::vec2(pt->x, p->m_y_mult * pt->y));
      return 0;
    }

    astral::Path *m_path;
    bool m_has_curves, m_has_move;
    int m_y_mult;
  };

  class GlyphGeneratorFreetype:public astral::GlyphGenerator
  {
  public:
    GlyphGeneratorFreetype(astral::reference_counted_ptr<astral::FreetypeLib> lib,
                           unsigned int number_threads,
                           astral::reference_counted_ptr<const astral::FreetypeFace::GeneratorBase> src);

    virtual
    uint32_t
    number_glyphs(void) const override
    {
      return m_number_glyphs;
    }

    virtual
    void
    fill_character_mapping(unsigned int thread_slot,
                           astral::CharacterMapping &mapping) const override;

    virtual
    unsigned int
    number_threads(void) const override
    {
      return m_faces.size();
    }

    virtual
    const astral::TypefaceMetricsScalable*
    scalable_metrics(void) const override
    {
      return m_scalable_metrics;
    }

    virtual
    astral::c_array<const astral::TypefaceMetricsFixedSize>
    fixed_metrics(void) const override
    {
      return astral::make_c_array(m_fixed_metrics);
    }

    virtual
    void
    scalable_glyph_info(unsigned int thread_slot,
                        astral::GlyphIndex glyph_index,
                        astral::GlyphMetrics *out_metrics,
                        astral::GlyphColors *out_layer_colors,
                        std::vector<astral::Path> *out_paths,
                        std::vector<enum astral::fill_rule_t> *out_fill_rules) const override;

    virtual
    bool
    fixed_glyph_info(unsigned int thread_slot,
                     astral::GlyphIndex glyph_index,
                     unsigned int size_idx,
                     astral::GlyphMetrics *out_metrics,
                     astral::ivec2 *out_image_size,
                     std::vector<astral::FixedPointColor_sRGB> *out_pixels) const override;

  private:
    static
    void
    add_path(FT_Outline &outline,
             std::vector<astral::Path> *out_paths,
             std::vector<enum astral::fill_rule_t> *out_fill_rules);

    static
    void
    grab_metrics(FT_GlyphSlot glyph,
                 astral::GlyphMetrics *out_metrics,
                 bool scale_by_64);

    static
    bool //returns if the glyph has color, i.e. a pixel with (R, G, B) not all the same
    convert_bgra_pixels(astral::ivec2 sz, const FT_Bitmap *bitmap,
                        astral::c_array<astral::FixedPointColor_sRGB> out_pixels);

    static
    void
    convert_grey_pixels(astral::ivec2 sz, const FT_Bitmap *bitmap,
                        astral::c_array<astral::FixedPointColor_sRGB> out_pixels);

    astral::TypefaceMetricsScalable m_scalable_metrics_backing;
    const astral::TypefaceMetricsScalable *m_scalable_metrics;
    std::vector<astral::TypefaceMetricsFixedSize> m_fixed_metrics;
    unsigned int m_number_glyphs;
    FT_Int m_load_flags;
    std::vector<astral::reference_counted_ptr<astral::FreetypeFace>> m_faces;
    astral::GlyphColors m_palettes;
  };
}

/////////////////////////////////////////////
// GlyphGeneratorFreetype methods
GlyphGeneratorFreetype::
GlyphGeneratorFreetype(astral::reference_counted_ptr<astral::FreetypeLib> lib,
                       unsigned int number_threads,
                       astral::reference_counted_ptr<const astral::FreetypeFace::GeneratorBase> src)
{
  using namespace astral;

  ASTRALassert(src);
  ASTRALassert(lib);

  m_faces.push_back(src->create_face(lib));

  ASTRALassert(m_faces.back());

  FT_Face face(m_faces.back()->face());
  m_number_glyphs = face->num_glyphs;

  if ((face->face_flags & FT_FACE_FLAG_SCALABLE) == 0)
    {
      m_scalable_metrics = nullptr;
      m_load_flags = FT_LOAD_RENDER | FT_LOAD_COLOR;

      m_fixed_metrics.resize(face->num_fixed_sizes);
      for (int i = 0; i < face->num_fixed_sizes; ++i)
        {
          m_fixed_metrics[i].m_pixel_size = static_cast<float>(face->available_sizes[i].size) / 64.0f;
          m_fixed_metrics[i].m_height = static_cast<float>(face->available_sizes[i].height);
        }
    }
  else
    {
      m_scalable_metrics = &m_scalable_metrics_backing;
      m_load_flags = FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING
        | FT_LOAD_NO_BITMAP | FT_LOAD_IGNORE_TRANSFORM
        | FT_LOAD_LINEAR_DESIGN;

      m_scalable_metrics_backing.m_height = face->height;
      m_scalable_metrics_backing.m_ascender = face->ascender;
      m_scalable_metrics_backing.m_descender = face->descender;
      m_scalable_metrics_backing.m_units_per_EM = face->units_per_EM;
      m_scalable_metrics_backing.m_underline_position = face->underline_position;
      m_scalable_metrics_backing.m_underline_thickness = face->underline_thickness;

      TT_OS2* os2;

      os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
      if (!os2)
        {
          m_scalable_metrics_backing.m_strikeout_position = m_scalable_metrics_backing.m_height * 0.5f;
          m_scalable_metrics_backing.m_strikeout_thickness = m_scalable_metrics_backing.m_underline_thickness;
        }
      else
        {
          m_scalable_metrics_backing.m_strikeout_position = os2->yStrikeoutPosition;
          m_scalable_metrics_backing.m_strikeout_thickness = os2->yStrikeoutSize;
        }

      /* Load the color palettes of the font */
      #ifdef COLOR_GLYPH_LAYER_SUPPORTED
        {
          int error_code;
          FT_Palette_Data palette_data;

          error_code = FT_Palette_Data_Get(face, &palette_data);
          if (error_code == 0)
            {
              m_palettes.resize(palette_data.num_palette_entries, palette_data.num_palettes);
              for (unsigned int P = 0; P < palette_data.num_palettes; ++P)
                {
                  c_array<vec4> dst_colors(m_palettes.layer_colors(astral::GlyphPaletteID(P)));
                  FT_Color *palette(nullptr);

                  error_code = FT_Palette_Select(face, P, &palette);
                  if (error_code == 0 && palette != nullptr)
                    {
                      for (unsigned int i = 0; i < dst_colors.size(); ++i)
                        {
                          dst_colors[i] = vec4(palette[i].red, palette[i].green,
                                               palette[i].blue, palette[i].alpha);
                          dst_colors[i] /= 255.0f;
                        }
                    }
                  else
                    {
                      std::fill(dst_colors.begin(), dst_colors.end(),
                                vec4(1.0f, 1.0f, 1.0f, 1.0f));
                    }
                }
            }
        }
      #endif
    }

  #if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
    {
      while (m_faces.size() < number_threads)
        {
          m_faces.push_back(src->create_face(lib));
        }
    }
  #endif
}

void
GlyphGeneratorFreetype::
fill_character_mapping(unsigned int thread_slot,
                       astral::CharacterMapping &mapping) const
{
  FT_Face face;
  FT_ULong character_code;
  FT_UInt glyph_index;

  ASTRALassert(thread_slot < m_faces.size());
  face = m_faces[thread_slot]->face();

  character_code = FT_Get_First_Char(face, &glyph_index);
  while (glyph_index != 0)
    {
      mapping.m_values[character_code] = astral::GlyphIndex(glyph_index);
      character_code = FT_Get_Next_Char(face, character_code, &glyph_index);
    }
}

void
GlyphGeneratorFreetype::
add_path(FT_Outline &outline,
         std::vector<astral::Path> *out_paths,
         std::vector<enum astral::fill_rule_t> *out_fill_rules)
{
  using namespace astral;

  enum fill_rule_t fill_rule;

  fill_rule = (outline.flags & FT_OUTLINE_EVEN_ODD_FILL) ?
    odd_even_fill_rule:
    nonzero_fill_rule;

  /* Is using the winding number the correct way to fill
   * always? Font's also have a notion of orientation and
   * Freetype also allows one to query those values; the
   * the bit FT_OUTLINE_REVERSE_FILL talks about outlines
   * of certain orientations must be filled and the other
   * orientation not filled; this data also appears to
   * be available via FT_Outline_Get_Orientation(). The
   * documentation states that the orientation is "computed",
   * so perhaps this is just a hint to allow for simpler
   * rasterizers?
   */

  out_fill_rules->push_back(fill_rule);
  out_paths->push_back(Path());
  OutlineDecompser::extract_path(&out_paths->back(), outline, true);
}

void
GlyphGeneratorFreetype::
grab_metrics(FT_GlyphSlot glyph,
             astral::GlyphMetrics *out_metrics,
             bool scale_by_64)
{
  using namespace astral;
  const FT_Glyph_Metrics &metrics(glyph->metrics);
  FT_Glyph ggg;
  FT_BBox bb;

  /* copy the metrics of the glyph, note that the offsets
   * are negated. This is because add_path() inverts the
   * glyph's y for us.
   */
  out_metrics->m_size = vec2(metrics.width, metrics.height);
  out_metrics->m_horizontal_layout_offset = vec2(metrics.horiBearingX,
                                                 metrics.height - metrics.horiBearingY);
  out_metrics->m_vertical_layout_offset = vec2(metrics.vertBearingX,
                                               metrics.height - metrics.vertBearingY);
  out_metrics->m_advance = vec2(metrics.horiAdvance, metrics.vertAdvance);

  out_metrics->m_bb.clear();
  FT_Get_Glyph(glyph, &ggg);

  if (scale_by_64)
    {
      /* When loading bitmap data, the metrics are in 26.6 units, so
       * we need to divide by 64.0f
       */
      out_metrics->m_size /= 64.0f;
      out_metrics->m_horizontal_layout_offset /= 64.0f;
      out_metrics->m_vertical_layout_offset /= 64.0f;
      out_metrics->m_advance /= 64.0f;

      FT_Glyph_Get_CBox(ggg, FT_GLYPH_BBOX_PIXELS, &bb);
      out_metrics->m_bb.union_point(vec2(bb.xMin, bb.yMin));
      out_metrics->m_bb.union_point(vec2(bb.xMax, bb.yMax));
    }
  else
    {
      FT_Glyph_Get_CBox(ggg, FT_GLYPH_BBOX_UNSCALED, &bb);
      out_metrics->m_bb.union_point(vec2(bb.xMin, -bb.yMax));
      out_metrics->m_bb.union_point(vec2(bb.xMax, -bb.yMin));
    }

}

void
GlyphGeneratorFreetype::
scalable_glyph_info(unsigned int thread_slot,
                    astral::GlyphIndex glyph_index,
                    astral::GlyphMetrics *out_metrics,
                    astral::GlyphColors *out_layer_colors,
                    std::vector<astral::Path> *out_paths,
                    std::vector<enum astral::fill_rule_t> *out_fill_rules) const
{
  using namespace astral;

  FT_Face face;
  unsigned int num_layers = 0;
  FT_UInt layer_glyph_index, layer_color_index;

  ASTRALassert(thread_slot < m_faces.size());
  face = m_faces[thread_slot]->face();

  FT_Load_Glyph(face, glyph_index.m_value, m_load_flags);
  grab_metrics(face->glyph, out_metrics, false);

  #ifdef COLOR_GLYPH_LAYER_SUPPORTED
    {
      FT_LayerIterator iter;
      /* compute the number of layers, if glyph is colored */
      iter.p = nullptr;
      while (FT_Get_Color_Glyph_Layer(face, glyph_index.m_value,
                                      &layer_glyph_index,
                                      &layer_color_index,
                                      &iter))
        {
          ++num_layers;
        }
    }
  #endif

  out_fill_rules->clear();
  out_paths->clear();

  /* realize the paths */
  if (num_layers == 0)
    {
      out_layer_colors->clear();
      add_path(face->glyph->outline, out_paths, out_fill_rules);
    }
  else
    {
      #ifdef COLOR_GLYPH_LAYER_SUPPORTED
        {
          FT_LayerIterator iter;

          out_layer_colors->resize(num_layers, m_palettes.number_palettes());

          /* reset the FT iterator before using it in the loop */
          iter.p = nullptr;
          for (unsigned int layer = 0; layer < num_layers; ++layer)
            {
              FT_Bool b;

              b = FT_Get_Color_Glyph_Layer(face, glyph_index.m_value,
                                           &layer_glyph_index,
                                           &layer_color_index,
                                           &iter);
              ASTRALassert(b);
              ASTRALunused(b);

              /* record the color for each palette */
              for (unsigned int P = 0; P < m_palettes.number_palettes(); ++P)
                {
                  GlyphPaletteID PID(P);
                  out_layer_colors->color(PID, layer) = m_palettes.color(PID, layer_color_index);
                }

              /* load the glyph specified by layer_glyph_index */
              FT_Load_Glyph(face, layer_glyph_index, m_load_flags);
              add_path(face->glyph->outline, out_paths, out_fill_rules);
            }
        }
      #endif
    }
}

void
GlyphGeneratorFreetype::
convert_grey_pixels(astral::ivec2 sz, const FT_Bitmap *bitmap,
                    astral::c_array<astral::FixedPointColor_sRGB> out_pixels)
{
  int pitch;

  pitch = astral::t_abs(bitmap->pitch);
  for (int y = 0; y < sz.y(); ++y)
    {
      for (int x = 0; x < sz.x(); ++x)
        {
          int src, dst;
          uint8_t v;

          dst = x + y * sz.x();
          src = x + y * pitch;
          v = bitmap->buffer[src];

          /* interface requires giving texels are with alpha pre-multiplied */
          out_pixels[dst] = astral::FixedPointColor_sRGB(v, v, v, v);
        }
    }
}

bool
GlyphGeneratorFreetype::
convert_bgra_pixels(astral::ivec2 sz, const FT_Bitmap *bitmap,
                    astral::c_array<astral::FixedPointColor_sRGB> out_pixels)
{
  int pitch;
  bool return_value = false;

  pitch = astral::t_abs(bitmap->pitch);
  for (int y = 0; y < sz.y(); ++y)
    {
      for (int x = 0; x < sz.x(); ++x)
        {
          int src, dst;
          uint8_t b, g, r, a;

          /* interface requires giving texels are with alpha pre-multiplied,
           * but Freetype gives BGRA pixels with pre-multiplied by alpha
           */
          dst = x + y * sz.x();
          src = 4 * x + y * pitch;
          b = bitmap->buffer[src + 0];
          g = bitmap->buffer[src + 1];
          r = bitmap->buffer[src + 2];
          a = bitmap->buffer[src + 3];

          return_value = return_value || r != g || r != b || g != b;
          out_pixels[dst] = astral::FixedPointColor_sRGB(r, g, b, a);
        }
    }

  return return_value;
}

bool
GlyphGeneratorFreetype::
fixed_glyph_info(unsigned int thread_slot,
                 astral::GlyphIndex glyph_index,
                 unsigned int size_idx,
                 astral::GlyphMetrics *out_metrics,
                 astral::ivec2 *out_image_size,
                 std::vector<astral::FixedPointColor_sRGB> *out_pixels) const
{
  FT_Face face;
  bool return_value;

  ASTRALassert(thread_slot < m_faces.size());
  face = m_faces[thread_slot]->face();

  FT_Select_Size(face, size_idx);
  FT_Load_Glyph(face, glyph_index.m_value, m_load_flags);
  grab_metrics(face->glyph, out_metrics, true);

  out_image_size->x() = face->glyph->bitmap.width;
  out_image_size->y() = face->glyph->bitmap.rows;

  out_pixels->resize(out_image_size->x() * out_image_size->y());
  switch (face->glyph->bitmap.pixel_mode)
    {
    case FT_PIXEL_MODE_BGRA:
      return_value = convert_bgra_pixels(*out_image_size, &face->glyph->bitmap,
                                         make_c_array(*out_pixels));
      break;

    case FT_PIXEL_MODE_GRAY:
      return_value = false;
      convert_grey_pixels(*out_image_size, &face->glyph->bitmap,
                          make_c_array(*out_pixels));
      break;

    default:
      return_value = true;
      std::fill(out_pixels->begin(), out_pixels->end(), astral::FixedPointColor_sRGB(255u, 255u, 0u, 128u));
    }

  return return_value;
}

///////////////////////////////////////////
// astral::FreetypeFace methods
astral::reference_counted_ptr<const astral::GlyphGenerator>
astral::FreetypeFace::GeneratorBase::
create_glyph_generator(unsigned int number_threads,
                       reference_counted_ptr<FreetypeLib> lib) const
{
  return ASTRALnew GlyphGeneratorFreetype(lib, number_threads, this);
}
