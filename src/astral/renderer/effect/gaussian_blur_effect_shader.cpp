/*!
 * \file gaussian_blur_effect_shader.cpp
 * \brief file gaussian_blur_effect_shader.cpp
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

#include <astral/util/ostream_utility.hpp>
#include <astral/renderer/effect/gaussian_blur_effect_shader.hpp>
#include <astral/renderer/renderer.hpp>

namespace
{
  inline
  float
  compute_radius(float sigma)
  {
    /* Typical image processing takes radius = 3 * sigma, see for
     * example, https://en.wikipedia.org/wiki/Gaussian_blur
     */
    return 3.0f * sigma;
  }

  inline
  float
  compute_sigma(float pixel_radius)
  {
    return pixel_radius / 3.0f;
  }

  class InitedItemDataValueMapping
  {
  public:
    InitedItemDataValueMapping(void)
    {
      m_values.add(astral::ItemDataValueMapping::render_value_image,
                   astral::ItemDataValueMapping::z_channel,
                   0);
    }

    astral::ItemDataValueMapping m_values;
  };


  /*!
   * Class that specifies the parameters to the data passed to
   * Effect::render_effect() and Effect::material_effect() to
   * the astral::Effect created with GaussianBlurEffectShader::create_effect()
   */
  class ProcessedGaussianBlurParameters
  {
  public:
    /*!
     * Enumeration to hold the offset values
     */
    enum offset_t:uint32_t
      {
        /*!
         * Offset at which to store (as a float) the blur
         * radius used in the Gaussian blur shaders. This
         * radius is essentially the product of the rendering
         * scale factor with the actual blur radius.
         */
        blur_radius_offset,

        /*!
         * Offset at which to store (as a uint) if there is a
         * scaling factor applied between the blurred pixels
         * and its presentation.
         */
        scale_applied_offset,

        /*!
         * Offset at which to store (as a uint) if the halo from
         * blurring should be drawn.
         */
        include_halo_offset,

        /*!
         * Offset at which to store what mipmap level to use
         */
        mipmap_level_offset,

        /*!
         * Offset at which to store the red channel value for
         * the color modulation
         */
        color_modulation_red_offset,

        /*!
         * Offset at which to store the green channel value for
         * the color modulation
         */
        color_modulation_green_offset,

        /*!
         * Offset at which to store the blue channel value for
         * the color modulation
         */
        color_modulation_blue_offset,

        /*!
         * Offset at which to store the alpha channel value for
         * the color modulation
         */
        color_modulation_alpha_offset,

        /*!
         * Offset at which to store the post-sampling mode
         */
        post_sampling_mode_offset,

        processed_param_size
      };
  };

  class GaussianBlurEffect:public astral::Effect
  {
  public:
    GaussianBlurEffect(const astral::MaterialShader &horiz_shader,
                       const astral::MaterialShader &vert_shader);

    virtual
    void
    compute_overridable_buffer_properties(const astral::Effect::BufferParameters &effect_parameters,
                                          astral::Effect::OverridableBufferProperties *out_properties) const override;

    virtual
    void
    compute_buffer_properties(const OverridableBufferProperties &overridable_properties,
                              const astral::Effect::BufferParameters &effect_parameters,
                              std::vector<astral::generic_data> *out_processed_params,
                              astral::Effect::BufferProperties *out_buffer_properties) const override;

    virtual
    void
    render_effect(astral::RenderEncoderBase dst,
                  astral::c_array<const astral::generic_data> processed_parameters,
                  astral::EffectWorkRoom &workroom,
                  const astral::SubImage &content,
                  const astral::Effect::BlitParameters &blit_params,
                  enum astral::blend_mode_t blend_mode,
                  const astral::ItemMask &clip) const override;
    virtual
    void
    material_effect(astral::Renderer &renderer,
                    astral::c_array<const astral::generic_data> processed_parameters,
                    astral::EffectWorkRoom &workroom,
                    const astral::SubImage &content,
                    const astral::Effect::BlitParameters &blit_params,
                    astral::EffectMaterial *out_material) const override;

  private:
    enum blur_coordinate_t:uint32_t
      {
        blur_coordinate_x = 0,
        blur_coordinate_y = 1
      };

    void
    compute_offsets_weights(float sigma,
                            astral::c_array<astral::generic_data> raw_weights,
                            astral::c_array<astral::generic_data> texel_weights,
                            astral::c_array<astral::generic_data> texel_offsets) const;

    astral::ItemData
    create_item_data(uint32_t bits,
                     astral::c_array<astral::generic_data> texel_weights,
                     astral::c_array<astral::generic_data> texel_offsets,
                     const astral::SubImage &image,
                     astral::EffectWorkRoom &workroom,
                     astral::Renderer &renderer) const;

    /* if the render blur radius is less than one, returns an ItemData
     * for which ItemData::valid() is false and sets out_pass1 to a null-image.
     * Otherwise renders the first pass and returns an ImageData which is to be
     * used to render the 2nd pass.
     */
    astral::ItemData
    render_pass1(astral::Renderer &renderer,
                 astral::c_array<const astral::generic_data> processed_parameters,
                 astral::EffectWorkRoom &workroom,
                 const astral::SubImage &content,
                 astral::reference_counted_ptr<const astral::Image> *out_pass1) const;

    static
    astral::Transformation
    compute_pass_transformation_logical(const astral::Image &pass,
                                        astral::c_array<const astral::generic_data> processed_parameters,
                                        const astral::Effect::BlitParameters &blit_params,
                                        const astral::SubImage &content);

    /* Draw the contents of an astral::Image with either m_horizontal_shader
     * or m_vertical_shader;
     * \param blur_coordinate in which coordinate is blurred applied
     * \param dst the RenderEncoderBase to which to render content
     * \param image SubImage from which material is derived, provides
     *              the exact size of the rect to draw in logical coordinates
     * \param material the ItemMaterial prepared to blit the content blurred
     * \param tile_influence_radius the radius, in coordinates of the
     *                              ImageMipElement mip, of how far a non-empty
     *                              pixel influences other pixels
     */
    void
    draw_blurred_image(astral::RenderEncoderBase dst,
                       enum blur_coordinate_t blur_coordinate,
                       const astral::SubImage &image,
                       const astral::ItemMaterial &material,
                       int tile_influence_radius) const;

    astral::reference_counted_ptr<const astral::MaterialShader> m_horizontal_shader, m_vertical_shader;
    mutable std::vector<bool> m_tiles_hit_by_blur;
  };
}

////////////////////////////////////
// GaussianBlurEffect methods
GaussianBlurEffect::
GaussianBlurEffect(const astral::MaterialShader &horiz_shader,
                   const astral::MaterialShader &vert_shader):
  m_horizontal_shader(&horiz_shader),
  m_vertical_shader(&vert_shader)
{
}

void
GaussianBlurEffect::
compute_overridable_buffer_properties(const astral::Effect::BufferParameters &effect_parameters,
                                      astral::Effect::OverridableBufferProperties *out_properties) const
{
  using namespace astral;

  c_array<const generic_data> effect_params(effect_parameters.m_custom_data);
  float raw_radius, pixel_radius, max_sample_radius, scale_factor, effective_parent_render_scale;

  ASTRALassert(effect_params.size() == GaussianBlurParameters::effect_param_size);

  raw_radius = effect_params[GaussianBlurParameters::radius_offset].f;
  max_sample_radius = effect_params[GaussianBlurParameters::max_sample_radius_offset].f;

  effective_parent_render_scale = t_min(1.0f, t_sqrt(effect_parameters.m_render_scale_factor.x() * effect_parameters.m_render_scale_factor.y()));

  /* Step 1: convert the logical radius value to pixel radius;
   *         in truth we are lieing some by taking the sqrt of
   *         the product of the singular values to make it
   *         isotropic. One idea we should consider is that
   *         Effect is also passed the vectors associated
   *         to the singular values to derive a direction for the
   *         gaussian blur axis and from there the two passes would
   *         be those perpindicular directions.
   */
  if (effect_params[GaussianBlurParameters::blur_radius_in_local_coordinates_offset].u == 1u)
    {
      float sc;

      sc = t_sqrt(effect_parameters.m_singular_values[0] * effect_parameters.m_singular_values[1]);
      pixel_radius = raw_radius * sc;
    }
  else
    {
      pixel_radius = raw_radius;
    }

  /* Step 2: if the radius is too large make the scale factor smaller
   *         to keep the radius small enough; we also make sure that
   *         the requested scale at which to render is not more than
   *         effective_parent_render_scale
   */
  if (pixel_radius > max_sample_radius)
    {
      float min_scale_factor;

      min_scale_factor = effect_params[GaussianBlurParameters::min_render_scale_offset].f;
      min_scale_factor *= effective_parent_render_scale;
      min_scale_factor = astral::t_min(min_scale_factor, 1.0f);

      if (effect_params[GaussianBlurParameters::force_pow2_render_scale_offset].u == 1u)
        {
          /* we need to force the scale_factor to be a power of 0.5. We do this
           * by computing its reciprocal rounded down and then taking log2 of
           * it.
           */
          uint32_t recip_rounded_down, log2_recip_rounded_down;

          recip_rounded_down = static_cast<unsigned int>(pixel_radius / max_sample_radius);
          log2_recip_rounded_down = uint32_log2_floor(recip_rounded_down);

          scale_factor = 1.0f / static_cast<float>(1u << log2_recip_rounded_down);
        }
      else
        {
          scale_factor = max_sample_radius / pixel_radius;
        }

      while (scale_factor < min_scale_factor)
        {
          scale_factor *= 2.0f;
        }

      /* we cannot have scale_factor exceed the rendering scale factor
       * of the parent either
       */
      if (scale_factor > effective_parent_render_scale)
        {
          scale_factor = effective_parent_render_scale;
        }
    }
  else
    {
      scale_factor = effective_parent_render_scale;
    }

  out_properties->m_render_scale_factor = scale_factor;

  /* this slack (note that it is in logical coordinates) is to make
   * sure that the image provided to render_effect() has the pixels
   * that affect the blur. It is also exactly the region that
   * corresponds the the blur-halo.
   */
  if (effect_params[GaussianBlurParameters::blur_radius_in_local_coordinates_offset].u == 1u)
    {
      out_properties->m_logical_slack = 2.0f * raw_radius;
    }
  else
    {
      /* get the logical slack from pixel coordinate to logical coordinates. Note
       * that we take the min of the singular values instead of their geoemtric
       * mean; the reason is that we need get the pixels.
       *
       * ISSUE: if one of the values is close to zero, we have a serious issue.
       *        Perhaps, in such situations we should view that no blur is applied?
       *
       * HACK: for now we just make a hard minimum at 1e-6.
       */
      float sc;

      sc = t_min(effect_parameters.m_singular_values[0], effect_parameters.m_singular_values[1]);
      sc = 1.0f / t_max(sc, float(1e-6));
      out_properties->m_logical_slack = 2.0f * sc * raw_radius;
    }
}

void
GaussianBlurEffect::
compute_buffer_properties(const OverridableBufferProperties &overridable_properties,
                          const astral::Effect::BufferParameters &effect_parameters,
                          std::vector<astral::generic_data> *out_processed_params,
                          astral::Effect::BufferProperties *out_buffer_properties) const
{
  using namespace astral;

  c_array<const generic_data> effect_params(effect_parameters.m_custom_data);
  float render_radius, raw_radius, pixel_radius, max_sample_radius;
  uint32_t mipmap_lod(0u);

  ASTRALassert(effect_params.size() == GaussianBlurParameters::effect_param_size);

  raw_radius = effect_params[GaussianBlurParameters::radius_offset].f;
  max_sample_radius = effect_params[GaussianBlurParameters::max_sample_radius_offset].f;

  if (effect_params[GaussianBlurParameters::blur_radius_in_local_coordinates_offset].u == 1u)
    {
      float sc;

      sc = t_sqrt(effect_parameters.m_singular_values[0] * effect_parameters.m_singular_values[1]);
      pixel_radius = raw_radius * sc;
    }
  else
    {
      pixel_radius = raw_radius;
    }

  /* we are given the scale_factor at which the content will be rendered,
   * so the render_radius is just the product of the pixel_radius and
   * the scale_factor.
   */
  render_radius = overridable_properties.m_render_scale_factor * pixel_radius;

  /* if the render_radius is too large, we then will use a mipmap to
   * get even lower resolution content; each level of mipmap reduces
   * the render_radius of the effect by a factor of one half.
   */
  while (render_radius > max_sample_radius)
    {
      render_radius *= 0.5f;
      mipmap_lod += 1u;
    }

  /* the pixel slack to make sure that sampling on the content edge
   * with linear filtering is simple in the shader
   */
  out_buffer_properties->m_pixel_slack = (2u << mipmap_lod);

  /* We need LOD mipmap_lod */
  out_buffer_properties->m_required_lod = mipmap_lod;

  out_processed_params->resize(ProcessedGaussianBlurParameters::processed_param_size);
  out_processed_params->operator[](ProcessedGaussianBlurParameters::blur_radius_offset).f = render_radius;
  out_processed_params->operator[](ProcessedGaussianBlurParameters::scale_applied_offset).u = (overridable_properties.m_render_scale_factor != 1.0f);
  out_processed_params->operator[](ProcessedGaussianBlurParameters::include_halo_offset) = effect_params[GaussianBlurParameters::include_halo_offset];
  out_processed_params->operator[](ProcessedGaussianBlurParameters::mipmap_level_offset).u = mipmap_lod;
  out_processed_params->operator[](ProcessedGaussianBlurParameters::color_modulation_red_offset) = effect_params[GaussianBlurParameters::color_modulation_red_offset];
  out_processed_params->operator[](ProcessedGaussianBlurParameters::color_modulation_green_offset) = effect_params[GaussianBlurParameters::color_modulation_green_offset];
  out_processed_params->operator[](ProcessedGaussianBlurParameters::color_modulation_blue_offset) = effect_params[GaussianBlurParameters::color_modulation_blue_offset];
  out_processed_params->operator[](ProcessedGaussianBlurParameters::color_modulation_alpha_offset) = effect_params[GaussianBlurParameters::color_modulation_alpha_offset];
  out_processed_params->operator[](ProcessedGaussianBlurParameters::post_sampling_mode_offset) = effect_params[GaussianBlurParameters::post_sampling_mode_offset];
}

void
GaussianBlurEffect::
compute_offsets_weights(float sigma,
                        astral::c_array<astral::generic_data> raw_weights,
                        astral::c_array<astral::generic_data> texel_weights,
                        astral::c_array<astral::generic_data> texel_offsets) const
{
  /*
   * When doing a 2-pass GaussianBlur, typically one does
   * the following in the fragment shader:
   *
   *   out_color = w0 * fetch(0) + w1 * fetch(1) + w1 * fetch(-1) + ... wM * fetch(M) + wM * fetch(-M)
   *
   * However, we can collapse fetch-pairs into a single fetch using the linear filtering
   * of the sampler as follows. Consider
   *
   *  V = sum_{1 <= k <= 2n} w[k] * fetch(k)
   *
   * Let b[k] = w[2 * k - 1] + w[2 * k]
   *     t[k] = w[2 * k] / b[k]
   *
   * under the assumption that each w[k] > 0, we get
   *
   *  V = sum_{1 <= k <= n} b[k] * mix(fetch(2 * k - 1), fetch(2 * k), t[k])
   *    = sum_{1 <= k <= n} b[k] * fetch(2 * k - 1 + t[k])
   *
   * Thus if we say M = 2 * N, then
   *
   *  out_color = w0 * fetch(0) + sum_{1 <= k <= N} b[k] * (fetch(-s[k]) + fetch(s[k]))
   *
   * where
   *
   *  s[k] = 2 * k - 1 + t[k], 1 <= k <= N
   *
   * the caller will pad texel_weights[] and texel_offsets[] infront
   * so that we don't fight off-by-one indexing pain.
   */
  using namespace astral;

  int N, pixel_radius;
  float recip_sum, sum, denom;

  ASTRALassert(texel_weights.size() > 1);
  N = texel_weights.size() - 1;
  pixel_radius = 2 * N;

  ASTRALassert(static_cast<unsigned int>(pixel_radius + 1) == raw_weights.size());

  /* We normalize by the sum of the terms. Doing so
   * makes sure that the overal brightness of the image
   * does not decrease.
   */
  denom = 1.0f / (2.0f * sigma * sigma);
  sum = 1.0f;
  texel_weights[0].f = 1.0f;

  for (int k = 1; k <= pixel_radius; ++k)
    {
      float fk(k);
      raw_weights[k].f = t_exp(-fk * fk * denom);
      sum += 2.0f * raw_weights[k].f;
    }

  recip_sum = 1.0f / sum;
  texel_weights[0].f *= recip_sum;
  for (int k = 1; k <= pixel_radius; ++k)
    {
      raw_weights[k].f *= recip_sum;
    }

  for (int k = 1; k <= N; ++k)
    {
      texel_weights[k].f = raw_weights[2 * k - 1].f + raw_weights[2 * k].f;
      texel_offsets[k].f = float(2 * k - 1) + raw_weights[2 * k].f / texel_weights[k].f;
    }
}

astral::ItemData
GaussianBlurEffect::
create_item_data(uint32_t bits,
                 astral::c_array<astral::generic_data> texel_weights,
                 astral::c_array<astral::generic_data> texel_offsets,
                 const astral::SubImage &image,
                 astral::EffectWorkRoom &workroom,
                 astral::Renderer &renderer) const
{
  using namespace astral;

  int N, item_data_size;
  int mip((bits & GaussianBlurEffectShader::sample_from_lod1_mask) ? 1u : 0u);
  ImageSampler image_sampler(image, MipmapLevel(mip), filter_linear);
  RenderValue<ImageSampler> im;

  im = renderer.create_value(image_sampler);

  ASTRALassert(texel_weights.size() > 1);
  N = texel_weights.size() - 1;

  item_data_size = 1 + N / 2;
  if (N & 1)
    {
      ++item_data_size;
    }
  workroom.m_vec4s.resize(item_data_size);

  /* Data packing format:
   *  - [0].x.u = N
   *  - [0].y.f = w0
   *  - [0].z.u = Image
   *  - [0].w.u = bits from GaussianBlurEffectShader::bits_t
   *  - [I].x = texel_weights[2 * I - 1]
   *  - [I].y = texel_offsets[2 * I - 1]
   *  - [I].z = texel_weights[2 * I]
   *  - [I].w = texel_offsets[2 * I]
   */
  workroom.m_vec4s[0].x().u = N;
  workroom.m_vec4s[0].y().f = texel_weights[0].f;
  workroom.m_vec4s[0].z().u = im.cookie();
  workroom.m_vec4s[0].w().u = bits;

  for (int I = 1; I < item_data_size; ++I)
    {
      workroom.m_vec4s[I].x() = texel_weights[2 * I - 1];
      workroom.m_vec4s[I].y() = texel_offsets[2 * I - 1];
      if (2 * I <= N)
        {
          workroom.m_vec4s[I].z() = texel_weights[2 * I];
          workroom.m_vec4s[I].w() = texel_offsets[2 * I];
        }
      else
        {
          workroom.m_vec4s[I].z().f = 0.0f;
          workroom.m_vec4s[I].w().f = 0.0f;
        }
    }

  return renderer.create_item_data(make_c_array(workroom.m_vec4s),
                                   astral::GaussianBlurEffectShader::item_data_value_map());
}

void
GaussianBlurEffect::
draw_blurred_image(astral::RenderEncoderBase dst,
                   enum blur_coordinate_t blur_coordinate,
                   const astral::SubImage &image,
                   const astral::ItemMaterial &material,
                   int tile_influence_radius) const
{
  using namespace astral;

  const ImageMipElement &mip(*image.mip_chain().front());
  if (mip.number_elements(ImageMipElement::empty_element) > 0)
    {
      const unsigned int other_coordinate = 1 - blur_coordinate;
      uvec2 tile_count;
      int tile_radius;
      Rect image_rect;

      tile_count = mip.tile_count();
      tile_radius = tile_influence_radius / ImageAtlas::tile_size_without_padding;
      if (tile_radius * ImageAtlas::tile_size_without_padding < tile_influence_radius)
        {
          ++tile_radius;
        }

      /* So this is a little nightmare of coordinate transformations.
       * We want to work in the coordinates of the ImageMipElement mip.
       * The material coordinates are the same as logical coordinate
       * which is coordinates same as the coordinates of the argument
       * image. The releation between logical (L) coordinates and mip
       * coordinates (M) is
       *
       *   L = M - image.m_min_corner
       *
       * We want to work in (M) coordinates which means all input
       * values need to be subtracted by image.m_min_corner
       */
      dst.translate(-vec2(image.m_min_corner));

      /* We then need to insert a material transformation
       * that also does the above.
       */
      ItemMaterial mapped_material(material);

      ASTRALassert(!mapped_material.m_material_transformation_logical.valid());
      mapped_material.m_material_transformation_logical = dst.create_value(Transformation(-vec2(image.m_min_corner)));

      /* Now figure out what portion of the image.m_image is used */
      image_rect.m_min_point = vec2(image.m_min_corner);
      image_rect.m_max_point = vec2(image.m_min_corner + image.m_size);

      /* TODO: instead of walking the entire range or tiles, restrict the range
       *       of tiles that intersect image_rect
       */
      for (unsigned int strip = 0; strip < tile_count[other_coordinate]; ++strip)
        {
          m_tiles_hit_by_blur.clear();
          m_tiles_hit_by_blur.resize(tile_count[blur_coordinate], false);

          /* The variables are all int's; by doing so we can safely handle
           * negative values.
           */
          for (int t = 0, last_unmarked = 0, endt = tile_count[blur_coordinate]; t < endt && last_unmarked < endt; ++t)
            {
              enum ImageMipElement::element_type_t tp;
              uvec2 tile_xy;

              tile_xy[blur_coordinate] = t;
              tile_xy[other_coordinate] = strip;

              tp = mip.tile_type(tile_xy);
              if (tp != ImageMipElement::empty_element)
                {
                  int s;

                  /* Only mark the tiles tha have not been marked since the
                   * last unmarked tile and update the last unarked tile
                   * This trick makes allows us to aovid marking a fixed tile
                   * more than once as covered.
                   */
                  s = t_max(last_unmarked, t - tile_radius);
                  last_unmarked = t_min(t + tile_radius + 1, endt);
                  for (int m = s; m < last_unmarked; ++m)
                    {
                      m_tiles_hit_by_blur[m] = true;
                    }
                }
            }

          /* we now know which tiles are affected by the blur; now we need to just draw
           * those tiles intersected agains image_rect
           *
           * TODO: instead of starting at 0 and ending at tile_count[blur_coordinate],
           *       track the first and last tile hit and just go in that range.
           */
          for (int t = 0, endt = tile_count[blur_coordinate]; t < endt; ++t)
            {
              if (m_tiles_hit_by_blur[t])
                {
                  Rect tile_rect, intersect_rect;
                  uvec2 tile_xy;

                  tile_xy[blur_coordinate] = t;
                  tile_xy[other_coordinate] = strip;

                  tile_rect.m_min_point = vec2(mip.tile_location(tile_xy));
                  tile_rect.m_max_point = tile_rect.m_min_point + vec2(mip.tile_size(tile_xy));

                  if (Rect::compute_intersection(tile_rect, image_rect, &intersect_rect))
                    {
                      dst.draw_rect(intersect_rect, false, mapped_material, astral::blend_porter_duff_src);
                    }
                }
            }
        }
    }
  else
    {
      dst.draw_rect(Rect()
                    .min_point(0.0f, 0.0f)
                    .size(vec2(image.m_size)),
                    false, material, astral::blend_porter_duff_src);
    }
}

astral::ItemData
GaussianBlurEffect::
render_pass1(astral::Renderer &renderer,
             astral::c_array<const astral::generic_data> processed_parameters,
             astral::EffectWorkRoom &workroom,
             const astral::SubImage &content,
             astral::reference_counted_ptr<const astral::Image> *out_pass1) const
{
  using namespace astral;

  int pixel_radius, padding, mip;
  float sigma, radius;
  ivec2 unpadded_size, src_size, target_size;
  ItemData item_data;
  RenderValue<Transformation> image_transformation;
  RenderValue<ImageSampler> image;
  RenderValue<Brush> brush;
  uint32_t item_data_bits, lod;

  /* It would be ideal if we could take advantage of the padding
   * at the start of a tiled image. However, that will dramatically
   * complicate the logic.
   */

  ASTRALassert(processed_parameters.size() == ProcessedGaussianBlurParameters::processed_param_size);

  radius = processed_parameters[ProcessedGaussianBlurParameters::blur_radius_offset].f;
  if (radius < 1.0f)
    {
      return item_data;
    }

  sigma = compute_sigma(radius);
  lod = processed_parameters[ProcessedGaussianBlurParameters::mipmap_level_offset].u;

  /* Clamp the LOD to the last LOD available */
  lod = t_min(lod, content.m_image.number_mipmap_levels() - 1u);

  /* Step 1: compute the filter values values */
  pixel_radius = int(radius);
  if (pixel_radius <= 0 || (pixel_radius & 1) != 0)
    {
      ++pixel_radius;
    }

  /* by taking advantage of linear filtering, we can half the number of texel fetches */
  int N(pixel_radius >> 1);

  /* We never sample just the center */
  if (N == 0)
    {
      N = 1;
    }

  workroom.m_scalars.resize(4 * N + 3);
  c_array<generic_data> pworkroom, raw_weights, texel_weights, texel_offsets;

  pworkroom = make_c_array(workroom.m_scalars);

  /* we make the valid range [1, 2N] and [1, N]
   * so that the code is more readable between the
   * math and the C code.
   */
  raw_weights = pworkroom.sub_array(0, 2 * N + 1);
  texel_weights = pworkroom.sub_array(2 * N + 1, N + 1);
  texel_offsets = pworkroom.sub_array(3 * N + 2, N + 1);
  compute_offsets_weights(sigma, raw_weights, texel_weights, texel_offsets);

  /* code below strongly assumes that each ImageMipElement holds
   * two mipmap levels
   */
  ASTRALstatic_assert(ImageMipElement::maximum_number_of_mipmaps == 2u);

  mip = lod >> 1u;

  SubImage content_mip_tail(content.mip_tail(mip));

  padding = 2u;
  target_size = src_size = ivec2(content_mip_tail.m_size);
  unpadded_size = src_size - 2 * ivec2(padding);

  item_data_bits = 0u;
  if (lod & 1u)
    {
      target_size.x() >>= 1;
      target_size.y() >>= 1;
      item_data_bits |= GaussianBlurEffectShader::sample_from_lod1_mask;
    }

  ASTRALassert(target_size.x() > 0);
  ASTRALassert(target_size.y() > 0);

  /* Step 2: render to an offscreen buffer using m_horizontal_shader
   *
   * Note that we make the SubImage as from the named ImageMipElement
   * restricted to not include the two pixel padding around it; This
   * is -different- than taking the entire image and drawing a rect
   * which is the the rect of the image with the padding stripped
   * because the shader implementation when passed the SubImage when
   * it tries to sample the padding will get zero, where as taking
   * the entire ImageMipElement and drawing a sub-rect would allow
   * the shader to sample the padding.
   */
  RenderEncoderImage pass1;
  unsigned int tile_influence_radius(pixel_radius);
  SubImage sub_image(content_mip_tail.sub_image(uvec2(padding), uvec2(unpadded_size)));

  pass1 = renderer.encoder_image(target_size);
  item_data = create_item_data(item_data_bits, texel_weights, texel_offsets,
                               sub_image, workroom, renderer);
  if (lod & 1u)
    {
      tile_influence_radius *= 2u;
      pass1.scale(0.5f);
    }
  pass1.translate(padding, padding);
  draw_blurred_image(pass1, blur_coordinate_x,
                     sub_image,
                     Material(*m_horizontal_shader, item_data),
                     tile_influence_radius);
  pass1.finish();
  *out_pass1 = pass1.image();

  item_data_bits = 0u;

  padding = 1u;
  unpadded_size = ivec2(pass1.image()->size()) - 2 * ivec2(padding);
  return create_item_data(item_data_bits, texel_weights, texel_offsets,
                          SubImage(*pass1.image(),
                                   uvec2(padding),
                                   uvec2(unpadded_size)),
                          workroom, renderer);
}

astral::Transformation
GaussianBlurEffect::
compute_pass_transformation_logical(const astral::Image &pass,
                                    astral::c_array<const astral::generic_data> processed_parameters,
                                    const astral::Effect::BlitParameters &blit_params,
                                    const astral::SubImage &content)
{
  using namespace astral;

  /* TODO: the transformation from content to pass is likely a little off
   *       when any of the dimensions of content.image() is not a multiple
   *       of 2^LOD.
   */
  unsigned int lod(processed_parameters[ProcessedGaussianBlurParameters::mipmap_level_offset].u);
  float sc(1u << lod);
  ScaleTranslate pass_transformation_content(vec2(0.0f, 0.0f), vec2(1.0f / sc));

  ASTRALunused(pass);
  ASTRALunused(content);

  return Transformation(pass_transformation_content) * blit_params.m_content_transformation_logical;
}

void
GaussianBlurEffect::
render_effect(astral::RenderEncoderBase dst,
              astral::c_array<const astral::generic_data> processed_parameters,
              astral::EffectWorkRoom &workroom,
              const astral::SubImage &content,
              const astral::Effect::BlitParameters &blit_params,
              enum astral::blend_mode_t blend_mode,
              const astral::ItemMask &clip) const
{
  using namespace astral;

  ItemData item_data;
  RenderValue<Transformation> image_transformation;
  RenderValue<ImageSampler> image;
  RenderValue<Brush> brush;
  int padding;
  unsigned int lod;
  ivec2 unpadded_size;
  Rect R;
  reference_counted_ptr<const Image> pass1;
  bool scale_applied;
  enum color_post_sampling_mode_t post_sampling_mode;
  vec4 color(processed_parameters[ProcessedGaussianBlurParameters::color_modulation_red_offset].f,
             processed_parameters[ProcessedGaussianBlurParameters::color_modulation_green_offset].f,
             processed_parameters[ProcessedGaussianBlurParameters::color_modulation_blue_offset].f,
             processed_parameters[ProcessedGaussianBlurParameters::color_modulation_alpha_offset].f);

  post_sampling_mode =
    static_cast<enum color_post_sampling_mode_t>(processed_parameters[ProcessedGaussianBlurParameters::post_sampling_mode_offset].u);

  lod = processed_parameters[ProcessedGaussianBlurParameters::mipmap_level_offset].u;

  item_data = render_pass1(dst.renderer(), processed_parameters, workroom, content, &pass1);
  if (!item_data.valid())
    {
      ImageSampler image_sampler(content, MipmapLevel(lod), filter_linear, post_sampling_mode);

      /* blur radius of less than a pixel means no blur and just blit the
       * required lod of the image.
       */
      image = dst.create_value(image_sampler);
      image_transformation = dst.create_value(blit_params.m_content_transformation_logical);
      brush = dst.create_value(Brush()
                               .base_color(color)
                               .image(image)
                               .image_transformation(image_transformation));
      dst.draw_rect(blit_params.m_logical_rect.m_bb.as_rect(), false,
                    ItemMaterial(brush, clip),
                    blend_mode);

      return;
    }

  scale_applied = processed_parameters[ProcessedGaussianBlurParameters::scale_applied_offset].u;

  padding = 1u;
  unpadded_size = ivec2(pass1->size()) - 2 * ivec2(padding);

  /* The halo is region coverered by the padding around the logical rect */
  R = blit_params.m_logical_rect.m_bb.as_rect();
  if (processed_parameters[ProcessedGaussianBlurParameters::include_halo_offset].u)
    {
      float h;

      /* The value for the padding is set as twice the radius, but
       * the halo is just to the rendering radius.
       */
      h = 0.5f * blit_params.m_logical_rect.m_padding;
      R.outset(h, h);
    }

  if (lod != 0u || scale_applied
      || post_sampling_mode != color_post_sampling_mode_direct
      || color != vec4(1.0f, 1.0f, 1.0f, 1.0f))
    {
      /* render pass2 to another offscreen buffer and the blit the result upscaled */
      RenderEncoderImage pass2;

      pass2 = dst.encoder_image(ivec2(pass1->size()));

      pass2.translate(padding, padding);
      draw_blurred_image(pass2, blur_coordinate_y,
                         SubImage(*pass1, uvec2(padding), uvec2(unpadded_size)),
                         Material(*m_vertical_shader, item_data),
                         processed_parameters[ProcessedGaussianBlurParameters::blur_radius_offset].f);
      pass2.finish();

      /* Step 4: blit the results of Step 3 to dst.
       *
       * TODO: make the draw sparse, i.e. only the rect of the tiles
       *       that are actually backed.
       */
      ImageSampler image_sampler(*pass2.image(), filter_linear, mipmap_none, post_sampling_mode);
      image_transformation = dst.create_value(compute_pass_transformation_logical(*pass2.image(), processed_parameters, blit_params, content));
      image = dst.create_value(image_sampler);
      brush = dst.create_value(Brush()
                               .base_color(color)
                               .image(image)
                               .image_transformation(image_transformation));
      dst.draw_rect(R, false, ItemMaterial(brush, clip), blend_mode);
    }
  else
    {
      /* render the 2nd pass directly to dst */

      ItemMaterial material;
      Transformation material_transformation_logical;

      material_transformation_logical = blit_params.m_content_transformation_logical
        * Transformation(-vec2(padding, padding));

      material.m_material = Material(*m_vertical_shader, item_data);
      material.m_material_transformation_logical = dst.create_value(material_transformation_logical);
      material.m_clip = clip;

      dst.draw_rect(R, false, material, blend_mode);
    }
}

void
GaussianBlurEffect::
material_effect(astral::Renderer &renderer,
                astral::c_array<const astral::generic_data> processed_parameters,
                astral::EffectWorkRoom &workroom,
                const astral::SubImage &content,
                const astral::Effect::BlitParameters &blit_params,
                astral::EffectMaterial *out_material) const
{
  using namespace astral;

  ItemData item_data;
  RenderValue<Transformation> image_transformation;
  RenderValue<ImageSampler> image;
  reference_counted_ptr<const Image> pass1;
  enum color_post_sampling_mode_t post_sampling_mode;
  vec4 color(processed_parameters[ProcessedGaussianBlurParameters::color_modulation_red_offset].f,
             processed_parameters[ProcessedGaussianBlurParameters::color_modulation_green_offset].f,
             processed_parameters[ProcessedGaussianBlurParameters::color_modulation_blue_offset].f,
             processed_parameters[ProcessedGaussianBlurParameters::color_modulation_alpha_offset].f);
  unsigned int lod;

  post_sampling_mode =
    static_cast<enum color_post_sampling_mode_t>(processed_parameters[ProcessedGaussianBlurParameters::post_sampling_mode_offset].u);

  lod = processed_parameters[ProcessedGaussianBlurParameters::mipmap_level_offset].u;

  item_data = render_pass1(renderer, processed_parameters, workroom, content, &pass1);
  if (!item_data.valid())
    {
      ImageSampler image_sampler(content, MipmapLevel(lod), filter_linear, post_sampling_mode);

      /* blur radius of less than a pixel means no blur and just blit the image. */
      image = renderer.create_value(image_sampler);
      image_transformation = renderer.create_value(blit_params.m_content_transformation_logical);

      out_material->m_rect = blit_params.m_logical_rect.m_bb.as_rect();
    }
  else
    {
      /* render pass2 */
      RenderEncoderImage pass2;
      int padding;
      ivec2 unpadded_size;
      float sc(1u << lod);
      ScaleTranslate pass_transformation_content(vec2(0.0f, 0.0f), vec2(1.0f / sc));

      padding = 1u;
      unpadded_size = ivec2(pass1->size()) - 2 * ivec2(padding);

      pass2 = renderer.encoder_image(ivec2(pass1->size()));

      pass2.translate(padding, padding);
      draw_blurred_image(pass2, blur_coordinate_y,
                         SubImage(*pass1, uvec2(padding), uvec2(unpadded_size)),
                         Material(*m_vertical_shader, item_data),
                         processed_parameters[ProcessedGaussianBlurParameters::blur_radius_offset].f);
      pass2.finish();

      /* the results of pass2 are the material */
      ImageSampler image_sampler(*pass2.image(), filter_linear, mipmap_none, post_sampling_mode);

      image = renderer.create_value(image_sampler);
      image_transformation = renderer.create_value(compute_pass_transformation_logical(*pass2.image(), processed_parameters, blit_params, content));

      out_material->m_rect = blit_params.m_logical_rect.m_bb.as_rect();
      if (processed_parameters[ProcessedGaussianBlurParameters::include_halo_offset].u)
        {
          float h;

          h = 0.5f * blit_params.m_logical_rect.m_padding;
          out_material->m_rect.outset(h, h);
        }
    }

  out_material->m_material_transformation_rect = Transformation();
  out_material->m_material = renderer.create_value(Brush()
                                                   .base_color(color)
                                                   .image(image)
                                                   .image_transformation(image_transformation));
}


////////////////////////////////////////////
// astral::GaussianBlurParameters methods
astral::GaussianBlurParameters&
astral::GaussianBlurParameters::
sigma(float v)
{
  return radius(compute_radius(v));
}

float
astral::GaussianBlurParameters::
sigma(void) const
{
  return compute_sigma(radius());
}

/////////////////////////////////////////////////
//astral::GaussianBlurEffectShader methods
astral::reference_counted_ptr<astral::Effect>
astral::GaussianBlurEffectShader::
create_effect(void) const
{
  return ASTRALnew GaussianBlurEffect(*m_horizontal_blur,
                                      *m_vertical_blur);
}

const astral::ItemDataValueMapping&
astral::GaussianBlurEffectShader::
item_data_value_map(void)
{
  static const InitedItemDataValueMapping v;
  return v.m_values;
}
