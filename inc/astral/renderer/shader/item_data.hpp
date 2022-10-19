/*!
 * \file item_data.hpp
 * \brief file item_data.hpp
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

#ifndef ASTRAL_ITEM_DATA_HPP
#define ASTRAL_ITEM_DATA_HPP

#include <vector>
#include <astral/util/c_array.hpp>
#include <astral/util/util.hpp>
#include <astral/renderer/render_value.hpp>
#include <astral/renderer/image_id.hpp>
#include <astral/renderer/shadow_map_id.hpp>

namespace astral
{
  ///@cond
  class Renderer;
  class RenderBackend;
  class RenderEncoderBase;
  class RenderEncoderMask;
  ///@endcond

/*!\addtogroup Renderer
 * @{
 */

  /*!
   * \brief
   * Provides the information how a value of an
   * astral::ItemData entry is to be interpreted
   * exactly when the entry is an astral::RenderValue.
   *
   * It is necessary for the astral::RenderBackend to know
   * the astral::RenderValue type of the cookie, because the
   * value used by the Renderer, is different from the value
   * the shader system reads. Therefore, the value for must
   * be converted based on the RenderValue type (the purpose
   * which the value will be used in the shader for).
   */
  class ItemDataValueMapping
  {
  public:
    /*!
     * \brief
     * Enumeration specifying how an element of
     * a ItemData is to be intepreted
     */
    enum type_t:uint32_t
      {
        /*!
         * The value in interpreted as a \ref RenderValue<\ref Transformation>
         */
        render_value_transformation,

        /*!
         * The value in interpreted as a \ref RenderValue<\ref ScaleTranslate>
         */
        render_value_scale_translate,

        /*!
         * The value in interpreted as a \ref RenderValue<\ref Brush>
         */
        render_value_brush,

        /*!
         * The value in interpreted as a \ref RenderValue<\ref ImageSampler>
         */
        render_value_image,

        /*!
         * The value in interpreted as a \ref RenderValue<\ref Gradient>
         */
        render_value_gradient,

        /*!
         * The value in interpreted as a \ref RenderValue<\ref GradientTransformation>
         */
        render_value_image_transformation,

        /*!
         * The value in interpreted as a \ref RenderValue<\ref ClipWindow>
         */
        render_value_clip,

        /*!
         * The value in interpreted as a \ref ItemData
         */
        render_value_item_data,

        /*!
         * The value in interpreted as a \ref RenderValue<\ref ShadowMap &>
         */
        render_value_shadow_map,

        render_value_type_count
      };

    /*!
     * \brief
     * Enumeration to specify a channel of gvec4
     */
    enum channel_t:uint32_t
      {
        /*!
         * Represents the x-channel, i.e. vecN::x().
         */
        x_channel = 0,

        /*!
         * Represents the y-channel, i.e. vecN::y().
         */
        y_channel = 1,

        /*!
         * Represents the z-channel, i.e. vecN::z().
         */
        z_channel = 2,

        /*!
         * Represents the w-channel, i.e. vecN::w().
         */
        w_channel = 3,
      };

    /*!
     * \brief
     * An entry of a \ref ItemDataValueMapping
     */
    class entry
    {
    public:
      /*!
       * Ctor
       * \param tp value to which to initialize \ref m_type
       * \param ch value to which to initialize \ref m_channel
       * \param idx value to which to initialize \ref m_component
       */
      entry(enum type_t tp, enum channel_t ch, unsigned int idx):
        m_type(tp),
        m_channel(ch),
        m_component(idx)
      {}

      /*!
       * How the value is interpreted
       */
      enum type_t m_type;

      /*!
       * which channel
       */
      enum channel_t m_channel;

      /*!
       * Which element of the array of gvec4's
       */
      unsigned int m_component;
    };

    /*!
     * Add an interpreted value to the \ref ItemDataValueMapping
     */
    ItemDataValueMapping&
    add(enum type_t tp, enum channel_t ch, unsigned idx)
    {
      m_data.push_back(entry(tp, ch, idx));
      return *this;
    }

    /*!
     * Clears this \ref ItemDataValueMapping
     */
    ItemDataValueMapping&
    clear(void)
    {
      m_data.clear();
      return *this;
    }

    /*!
     * Return the entries of this \ref ItemDataValueMapping
     */
    c_array<const entry>
    data(void) const
    {
      return make_c_array(m_data);
    }

  private:
    std::vector<entry> m_data;
  };

  /*!
   * Enumeration type to indicates when creating
   * astral::ItemData that there are no entries
   * to interpret, i.e. there is no interpretation
   * of values via an astral::ItemDataValueMapping
   */
  enum no_item_data_value_mapping_t
    {
      /*!
       * Enumeration tag to indicate when creating
       * astral::ItemData that there are no entries
       * to interpret, i.e. there is no interpretation
       * of values via an astral::ItemDataValueMapping
       */
      no_item_data_value_mapping,
    };

  /*!
   * If an astral::ItemData uses data from any astral::ImageID
   * or any astral::ShadowMapID directly (i.e. not by packing a
   * field named in an astral::ItemDataValueMapping, then rendering
   * needs to know of those dependencies.
   */
  class ItemDataDependencies
  {
  public:
    /*!
     * Ctor
     * \param im value with which to initialized \ref m_images
     * \param sm value with which to initialized \ref m_shadow_maps
     */
    ItemDataDependencies(c_array<const ImageID> im = c_array<const ImageID>(),
                         c_array<const ShadowMapID> sm = c_array<const ShadowMapID>()):
      m_images(im),
      m_shadow_maps(sm)
    {}

    /*!
     * List of image dependencies. Note: depending on any element of
     * astral::Image::mip_chain() means depending on the astral::Image().
     */
    c_array<const ImageID> m_images;

    /*!
     * List of shadow map dependencies
     */
    c_array<const ShadowMapID> m_shadow_maps;
  };

  /*!
   * \brief
   * ItemData represents the data used by an astral::ItemShader
   * that is common to all vertices and fragments. It is essentially the
   * abstraction of uniforms. An astral::ItemData can be created
   * by any of the following:
   * - astral::Renderer::create_item_data(c_array<const gvec4>, const ItemDataValueMapping&, const ItemDataDependencies&)
   * - astral::Renderer::create_item_data(c_array<const gvec4>, c_array<const ItemDataValueMapping::entry>, const ItemDataDependencies&)
   * - astral::Renderer::create_item_data(c_array<const gvec4>, enum no_item_data_value_mapping_t, const ItemDataDependencies&)
   * - astral::RenderEncoderBase::create_item_data(c_array<const gvec4>, const ItemDataValueMapping&, const ItemDataDependencies&) const
   * - astral::RenderEncoderBase::create_item_data(c_array<const gvec4>, c_array<const ItemDataValueMapping::entry>, const ItemDataDependencies&) const
   * - astral::RenderEncoderBase::create_item_data(c_array<const gvec4>, enum no_item_data_value_mapping_t, const ItemDataDependencies&) const
   */
  class ItemData
  {
  public:
    /*!
     * Ctor, initializes the handle as a nul-value.
     */
    ItemData(void):
      m_cookie(InvalidRenderValue),
      m_begin_cnt(0),
      m_backend(nullptr)
    {}

    /*!
     * Returns true if this \ref ItemData
     * is valid for use
     */
    bool
    valid(void) const;

    /*!
     * Returns true if this \ref RenderValue
     * is valid for the specified RenderEncoderMask
     */
    bool
    valid_for(const RenderEncoderMask &p) const;

    /*!
     * Returns true if this \ref RenderValue
     * is valid for the specified RenderEncoderBase
     */
    bool
    valid_for(const RenderEncoderBase &p) const;

    /*!
     * Comparison operator
     * \param rhs value against which to compare
     */
    bool
    operator==(const ItemData &rhs) const
    {
      return (valid() == rhs.valid()
              && m_cookie == rhs.m_cookie);
    }

    /*!
     * Comparison operator
     * \param rhs value against which to compare
     */
    bool
    operator!=(const ItemData &rhs) const
    {
      return !operator==(rhs);
    }

    /*!
     * Returns the cookie value used by an implementation
     * of \ref RenderBackend
     */
    uint32_t
    cookie(void) const
    {
      return m_cookie;
    }

  private:
    friend class RenderBackend;
    friend class RenderEncoderBase;

    void
    init(uint32_t v, RenderBackend &r);

    uint32_t m_cookie;
    uint32_t m_begin_cnt;
    RenderBackend *m_backend;
  };

/*! @} */
}

#endif
