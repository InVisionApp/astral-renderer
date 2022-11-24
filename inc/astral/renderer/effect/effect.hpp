/*!
 * \file effect.hpp
 * \brief file effect.hpp
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

#ifndef ASTRAL_EFFECT_HPP
#define ASTRAL_EFFECT_HPP

#include <vector>
#include <astral/util/util.hpp>
#include <astral/util/vecN.hpp>
#include <astral/util/rect.hpp>
#include <astral/util/reference_counted.hpp>
#include <astral/util/transformation.hpp>
#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/material.hpp>
#include <astral/renderer/render_scale_factor.hpp>
#include <astral/renderer/item_material.hpp>
#include <astral/renderer/image.hpp>
#include <astral/renderer/relative_bounding_box.hpp>

namespace astral
{
/*!\addtogroup Renderer
 * @{
 */

  class RenderEncoderBase;
  class Renderer;
  class Material;

  /*!
   * \brief
   * A type for astral::Effect to use for work room
   */
  class EffectWorkRoom
  {
  public:
    /*!
     * Clears both \ref m_scalars and \ref m_vec4s
     */
    void
    clear(void)
    {
      m_scalars.clear();
      m_vec4s.clear();
    }

    /*!
     * A dynamically size array for work-room for scalar values
     */
    std::vector<generic_data> m_scalars;

    /*!
     * A dynamically size array for work-room for gvec4 values
     */
    std::vector<gvec4> m_vec4s;
  };

  /*!
   * Represents a material that can be re-used (cheeply)
   * that represents an astral::Effect applied to the
   * contents of an astral::Image
   */
  class EffectMaterial
  {
  public:
    /*!
     * The material to apply to the rect to draw
     */
    Material m_material;

    /*!
     * The transformation from the coordinates of
     * the \ref m_rect to the the coordinates of
     * the material
     */
    Transformation m_material_transformation_rect;

    /*!
     * The rect to draw; the coordinate system of the
     * rect is the same as the logical coordinate system
     * of the RenderEncoderBase object at the time when
     * RenderEncoderBase::snapshot_effect() is called.
     *
     * A caller can assume that \ref m_rect always
     * contains the astral::BoundingBox<float> passed
     * to RenderEncoderBase::snapshot_effect().
     */
    Rect m_rect;
  };

  /*!
   * An astral::EffectParameters are the parameters to an astral::Effect
   */
  class EffectParameters
  {
  public:
    /*!
     * Ctor that makes effect coordinates and logical
     * coordinates the same.
     * \param d initial value for \ref m_data
     */
    EffectParameters(c_array<const generic_data> d = c_array<const generic_data>()):
      m_data(d),
      m_effect_transformation_logical(0.0f, 0.0f)
    {}

    /*!
     * Ctor.
     * \param d initial value for \ref m_data
     * \param tr initial value for \ref m_effect_transformation_logical
     */
    EffectParameters(c_array<const generic_data> d, const vec2 &tr):
      m_data(d),
      m_effect_transformation_logical(tr)
    {}

    /*!
     * Set the value of \ref m_data
     */
    EffectParameters&
    data(c_array<const generic_data> d)
    {
      m_data = d;
      return *this;
    }

    /*!
     * Set the value of \ref m_effect_transformation_logical
     */
    EffectParameters&
    effect_transformation_logical(const vec2 &tr)
    {
      m_effect_transformation_logical = tr;
      return *this;
    }

    /*!
     * Raw data to feed an astral::Effect. The backing of this
     * data only needs to be valid during the call to
     * RenderEncoderBase::begin_layer().
     */
    c_array<const generic_data> m_data;

    /*!
     * Transformation from logical to effect coordinates; note that
     * the transformation can only be a translation.
     */
    vec2 m_effect_transformation_logical;
  };

  /*!
   * \brief
   * A Effect embodies apply per-pixel operations to
   * rendered image data. Examples include shadow blur,
   * distortions and so on.
   */
  class Effect:public reference_counted<Effect>::non_concurrent
  {
  public:
    virtual
    ~Effect()
    {}

    /*!
     * Class to specify the parameters to pass to
     * Effect::compute_buffer_properties() and
     * Effect::compute_overridable_buffer_properties().
     */
    class BufferParameters
    {
    public:
      /*!
       * Data customized for the astral::Effect (for example
       * the blur radius for gaussian blur would be stored
       * in this array). The backing of this data does not
       * stay alive past the call to compute_buffer_properties().
       */
      c_array<const generic_data> m_custom_data;

      /*!
       * The transformation from logical to pixel coordinates
       * of the calling astral::RenderEncoderBase
       */
      Transformation m_pixel_transformation_logical;

      /*!
       * The singular values of Transformation::m_matrix
       * of \ref m_pixel_transformation_logical
       */
      vecN<float, 2> m_singular_values;

      /*!
       * The value of RenderEncoderBase::render_scale_factor()
       * of the encoder used to draw the effect.
       */
      float m_render_scale_factor;

      /*!
       * The rectangle of the area to which the effect will apply
       */
      Rect m_logical_rect;
    };

    /*!
     * Class for an astral::Effect to to specify values that
     * the caller can override (to greater values) for how
     * to render content to an astral::Image which holds the
     * rendering to which the effect is applied
     */
    class OverridableBufferProperties
    {
    public:
      OverridableBufferProperties(void):
        m_logical_slack(0u),
        m_render_scale_factor(1.0f)
      {}

      /*!
       * The amount in logical coordinates to add to the bounding
       * box for the area to be rendered. This slack is added after
       * the clipping.
       */
      float m_logical_slack;

      /*!
       * The non-relative rendering scale factor at which to render the
       * content an effect is to apply to. It is an error for this
       * value to be less than or equal to zero.
       */
      float m_render_scale_factor;
    };

    /*!
     * Class for an astral::Effect to specify how to render
     * content to an astral::Image which holds the rendering
     * to which the effect is applied
     */
    class BufferProperties
    {
    public:
      BufferProperties(void):
        m_pixel_slack(0u),
        m_required_lod(0u)
      {}

      /*!
       * The number of pixels around the generated image of slack
       * to add to the bounding box for the area to be rendered.
       * This slack is added after the clipping.
       */
      unsigned int m_pixel_slack;

      /*!
       * Required LOD level to be present in the astral::Image
       * to be passed to render_effect().
       */
      unsigned int m_required_lod;
    };

    /*!
     * Class to specify to where to blit the effect applied
     * to the astral::Image
     */
    class BlitParameters
    {
    public:
      /*!
       * The transformation from destination logical
       * coordinates to the astral::Image holding
       * the content to which the effect applies.
       */
      Transformation m_content_transformation_logical;

      /*!
       * The location of the rect in logical coordinates
       * to blit the astral::Effect. The value of the
       * field RelativeBoundingBox::m_padding is the
       * same as BufferProperties::m_logical_slack.
       */
      RelativeBoundingBox m_logical_rect;
    };

    /*!
     * To be implemented by a derived class to compute the
     * Effect::OverridableBufferProperties that the Effect
     * prefers.
     * \param effect_parameters parameters for effect
     * \param out_properties location to which to write values
     */
    virtual
    void
    compute_overridable_buffer_properties(const BufferParameters &effect_parameters,
                                          OverridableBufferProperties *out_properties) const = 0;

    /*!
     * To be implemented by a derived class the nature of the offscreen
     * buffer to which the effect is applied
     * \param overridable_properties overridable parameters on how to render
     *                               the content to which the effect is applied
     * \param effect_parameters parameters for effect
     * \param out_processed_params location to which to write the values
     *                             that render_effect() will consume
     * \param out_buffer_properties location to which to write how the
     *                              offscreen content is to be rendered
     */
    virtual
    void
    compute_buffer_properties(const OverridableBufferProperties &overridable_properties,
                              const BufferParameters &effect_parameters,
                              std::vector<generic_data> *out_processed_params,
                              BufferProperties *out_buffer_properties) const = 0;

    /*!
     * Render the effect
     * \param dst the RenderEncoderBase to which to render the content with
     *            the effect applied
     * \param processed_parameters parameter values made by compute_buffer_properties()
     * \param workroom place for temporary storage
     * \param content astral::Image holding the pixel to which the efect applies.
     * \param blit_params provides to where to blit the pixels.
     * \param blend_mode the blend mode to apply
     * \param clip the clipping to apply to the effect
     */
    virtual
    void
    render_effect(RenderEncoderBase dst,
                  c_array<const generic_data> processed_parameters,
                  EffectWorkRoom &workroom,
                  const SubImage &content,
                  const BlitParameters &blit_params,
                  enum blend_mode_t blend_mode,
                  const ItemMask &clip) const = 0;

    /*!
     * To be implemented by a derived class to generate an astral::Material
     * that can be reused. Typically, the returned astral::Material would
     * correspond to an astral::Brush with just an image attached.
     * \param renderer astral::Renderer to use to generate encoder with which to render
     * \param processed_parameters parameter values made by compute_buffer_properties()
     * \param workroom place for temporary storage
     * \param content astral::Image holding the pixel to which the efect applies.
     * \param blit_params provides to where to blit the pixels.
     * \param out_material location to which to write the output EffectMaterial
     */
    virtual
    void
    material_effect(Renderer &renderer,
                    c_array<const generic_data> processed_parameters,
                    EffectWorkRoom &workroom,
                    const SubImage &content,
                    const BlitParameters &blit_params,
                    EffectMaterial *out_material) const = 0;
  };

  /*!
   * An EffectCollectionBase is essentially an sequence of effects to apply.
   * It provides an interface (via virtual functions) to specify
   * what astral::Effect object with what astral::EffectParameters
   * to apply to each astral::Effect object.
   */
  class EffectCollectionBase
  {
  public:
    virtual
    ~EffectCollectionBase()
    {}

    /*!
     * To be implemented by a derived class to specify the
     * number of effects to apply.
     */
    virtual
    unsigned int
    number_effects(void) const = 0;

    /*!
     * To be implemented by a derived class to specify the
     * astral::Effect to apply
     * \param e which astral::Effect with 0 <= e < number_effects()
     */
    virtual
    const Effect&
    effect(unsigned int e) const = 0;

    /*!
     * To be implemented by a derived class to specify the
     * astral::EffectParameters used by the named astral::Effect
     * \param e which astral::Effect with 0 <= e < number_effects()
     */
    virtual
    const EffectParameters&
    effect_parameters(unsigned int e) const = 0;

    /*!
     * To be implemented by a derived class to specify the
     * blend mode to apply for the named effect
     * \param e which astral::Effect with 0 <= e < number_effects()
     */
    virtual
    enum blend_mode_t
    blend_mode(unsigned int e) const = 0;

    /*!
     * Tobe implemented by a derived class to specify the
     * translation to apply to the bounding box in layer
     */
    virtual
    vec2
    translate_capture_bb(unsigned int e) const = 0;
  };

  /*!
   * An astral::EffectCollection is a simple implementation
   * of astral::EffectCollectionBase where a single effect
   * is applied with different astral::EffectParameters
   * values.
   */
  class EffectCollection:public EffectCollectionBase
  {
  public:
    /*!
     * Ctor.
     * \param peffect what Effect to apply
     * \param params an array of astral::EffectParameters, the values
     *               are NOT copied.
     * \param b blend mode to apply
     * \param translate_bb an array of translate values that translate
     *                     the capture logical bounding box; an empty
     *                     array indicates to NOT translate otherwise
     *                     must be the same size as params
     */
    EffectCollection(const Effect &peffect,
                     c_array<const EffectParameters> params,
                     enum blend_mode_t b = blend_porter_duff_src_over,
                     c_array<const vec2> translate_bb = c_array<const vec2>()):
      m_effect(&peffect),
      m_effect_parameters(params),
      m_blend_mode(b),
      m_translate_bb(translate_bb)
    {
      ASTRALassert(m_translate_bb.empty() || m_translate_bb.size() == m_effect_parameters.size());
    }

    /*!
     * Ctor.
     * \param peffect what Effect to apply
     * \param params an array of astral::EffectParameters, the values
     *               are NOT copied.
     * \param b blend mode to apply
     * \param translate_bb an array of translate values that translate
     *                     the capture logical bounding box; an empty
     *                     array indicates to NOT translate otherwise
     *                     must be the same size as params
     */
    EffectCollection(const Effect &peffect,
                     c_array<const EffectParameters> params,
                     c_array<const vec2> translate_bb,
                     enum blend_mode_t b = blend_porter_duff_src_over):
      m_effect(&peffect),
      m_effect_parameters(params),
      m_blend_mode(b),
      m_translate_bb(translate_bb)
    {
      ASTRALassert(m_translate_bb.empty() || m_translate_bb.size() == m_effect_parameters.size());
    }

    unsigned int
    number_effects(void) const override
    {
      return m_effect_parameters.size();
    }

    const Effect&
    effect(unsigned int e) const override
    {
      ASTRALunused(e);
      ASTRALassert(e < m_effect_parameters.size());

      return *m_effect;
    }

    const EffectParameters&
    effect_parameters(unsigned int e) const override
    {
      return m_effect_parameters[e];
    }

    enum blend_mode_t
    blend_mode(unsigned int e) const override
    {
      ASTRALunused(e);
      ASTRALassert(e < m_effect_parameters.size());

      return m_blend_mode;
    }

    virtual
    vec2
    translate_capture_bb(unsigned int e) const override
    {
      ASTRALassert(e < m_effect_parameters.size());
      return m_translate_bb.empty() ? vec2(0.0f, 0.0f) : m_translate_bb[e];
    }

  private:
    reference_counted_ptr<const Effect> m_effect;
    c_array<const EffectParameters> m_effect_parameters;
    enum blend_mode_t m_blend_mode;
    c_array<const vec2> m_translate_bb;
  };

/*! @} */
}

#endif
