/*!
 * \file item_shader.hpp
 * \brief file item_shader.hpp
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

#ifndef ASTRAL_ITEM_SHADER_HPP
#define ASTRAL_ITEM_SHADER_HPP

#include <astral/util/reference_counted.hpp>

namespace astral
{
  class RenderEngine;
  class ItemShaderBackend;
  class ItemShader;
  class ColorItemShader;
  class MaskItemShader;
  class ShadowMapItemShader;

  /*!
   * \brief
   * Abstractly embodies how to process vertices and compute
   * color values.
   */
  class ItemShader:public reference_counted<ItemShader>::non_concurrent
  {
  public:
    /*!
     * Enumeration to describe different item shader types
     */
    enum type_t
      {
        /*!
         * Shader is for generating masks; such shaders are
         * not limited to drawing rectangles, but do not have
         * mask or material.
         */
        mask_item_shader = 0u,

        /*!
         * Shader is for generating shadow maps; such shaders
         * are not limited to drawing rectangles, but do not
         * have mask or material.
         */
        shadow_map_item_shader = 1u,

        /*!
         * Shader is for drawing rectangles with that can
         * be masked, have custom materials and fragment
         * shading.
         */
        color_item_shader = 2u,

        /* NOTE: color_item_shader must come last in value for the
         *       enumeration because BackendBlendMode value packing
         *       assumes this
         */

        number_item_shader_types
      };

    virtual
    ~ItemShader()
    {}

    /*!
     * Returns the shaderID. If two astral::ItemShader
     * objects have the same shaderID() value, then they
     * shade exactly the same way. In addition, the value
     * of shaderID() for a sub-shader is just the sum of
     * the shaderID() value of the parent plus the value
     * of which sub-shader.
     */
    unsigned int
    shaderID(void) const
    {
      return m_shaderID;
    }

    /*!
     * Returns the number of sub-shaders
     */
    unsigned int
    num_sub_shaders(void) const
    {
      return m_num_sub_shaders;
    }

    /*!
     * Returns the sub-shader of the parent shader.
     */
    unsigned int
    subshader(void) const
    {
      return m_sub_shader;
    }

    /*!
     * Returns the backend opaque data for the shader.
     */
    const ItemShaderBackend&
    backend(void) const
    {
      return *m_backend;
    }

    /*!
     * Returns the shader type; use this to avoid
     * dynamic_cast.
     */
    enum type_t
    type(void) const
    {
      return m_type;
    }

  private:
    friend class ColorItemShader;
    friend class MaskItemShader;
    friend class ShadowMapItemShader;

    /*!
     * Ctor.
     * \param tp the shader type
     * \param backend opaque data from backend
     * \param sub_shader_id which sub-shader of backend
     */
    ItemShader(enum type_t tp, const ItemShaderBackend &backend,
               unsigned int sub_shader_id = 0);

    /*!
     * Ctor that comes from a sub-shader of a parent ItemShader
     * \param parent shader
     * \param sub_shader_id which sub-shader of the parent
     */
    ItemShader(const ItemShader &parent, unsigned int sub_shader_id);

    reference_counted_ptr<const ItemShaderBackend> m_backend;
    unsigned int m_num_sub_shaders, m_shaderID, m_sub_shader;
    enum type_t m_type;
  };

  /*!
   * \brief
   * An astral::MaskItemShader represents a shader for drawing to a mask.
   */
  class MaskItemShader final:public ItemShader
  {
  public:
    /*!
     * Ctor.
     * \param backend opaque data from backend
     * \param sub_shader_id which sub-shader
     */
    static
    reference_counted_ptr<const MaskItemShader>
    create(const ItemShaderBackend &backend,
           unsigned int sub_shader_id = 0)
    {
      return ASTRALnew MaskItemShader(backend, sub_shader_id);
    }

    /*!
     * Ctor that comes from a sub-shader of a parent
     * astral::ColorItemShader
     * \param parent shader
     * \param sub_shader_id which sub-shader of the parent
     */
    static
    reference_counted_ptr<const MaskItemShader>
    create(const MaskItemShader &parent, unsigned int sub_shader_id)
    {
      return ASTRALnew MaskItemShader(parent, sub_shader_id);
    }

  private:
    explicit
    MaskItemShader(const ItemShaderBackend &backend,
                   unsigned int sub_shader_id):
      ItemShader(mask_item_shader, backend, sub_shader_id)
    {}

    MaskItemShader(const MaskItemShader &parent, unsigned int sub_shader_id):
      ItemShader(parent, sub_shader_id)
    {}
  };

  /*!
   * \brief
   * An astral::ColorItemShader represents a shader for drawing to a
   * color buffer.
   */
  class ColorItemShader final:public ItemShader
  {
  public:
    /*!
     * \brief
     * Provides basic properties describing a \ref ItemShader
     */
    class Properties
    {
    public:
      Properties(void):
        m_emits_partially_covered_fragments(false),
        m_emits_transparent_fragments(false)
      {}

      /*!
       * Sets \ref m_emits_partially_covered_fragments
       */
      Properties&
      emits_partially_covered_fragments(bool v)
      {
        m_emits_partially_covered_fragments = v;
        return *this;
      }

      /*!
       * Sets \ref m_emits_transparent_fragments
       */
      Properties&
      emits_transparent_fragments(bool v)
      {
        m_emits_transparent_fragments = v;
        return *this;
      }

      /*!
       * If true, indicates that the shader may emit fragments
       * that are only partially covered.
       */
      bool m_emits_partially_covered_fragments;

      /*!
       * If true, indicates that the shader may emit fragments
       * with alpha strictly less than one.
       */
      bool m_emits_transparent_fragments;
    };

    /*!
     * Ctor.
     * \param backend opaque data from backend
     * \param sub_shader_id which sub-shader
     * \param props properties of the shader
     */
    static
    reference_counted_ptr<const ColorItemShader>
    create(const ItemShaderBackend &backend, const Properties &props,
           unsigned int sub_shader_id = 0)
    {
      return ASTRALnew ColorItemShader(backend, props, sub_shader_id);
    }

    /*!
     * Ctor.
     * \param backend opaque data from backend
     * \param sub_shader_id which sub-shader
     * \param props properties of the shader
     */
    static
    reference_counted_ptr<const ColorItemShader>
    create(const ItemShaderBackend &backend,
           unsigned int sub_shader_id,
           const Properties &props)
    {
      return ASTRALnew ColorItemShader(backend, sub_shader_id, props);
    }

    /*!
     * Ctor that comes from a sub-shader of a parent ColorItemShader
     * \param parent shader
     * \param sub_shader_id which sub-shader of the parent
     * \param props properties of the shader
     */
    static
    reference_counted_ptr<const ColorItemShader>
    create(const ColorItemShader &parent, unsigned int sub_shader_id,
           const Properties &props)
    {
      return ASTRALnew ColorItemShader(parent, sub_shader_id, props);
    }

    /*!
     * Ctor that comes from a sub-shader of a parent ColorItemShader
     * \param parent shader
     * \param sub_shader_id which sub-shader of the parent
     * \param props properties of the shader
     */
    static
    reference_counted_ptr<const ColorItemShader>
    create(const ColorItemShader &parent, const Properties &props,
           unsigned int sub_shader_id)
    {
      return ASTRALnew ColorItemShader(parent, props, sub_shader_id);
    }

    /*!
     * Returns the properties of this shader
     */
    const Properties&
    properties(void) const
    {
      return m_properties;
    }

  private:
    ColorItemShader(const ItemShaderBackend &backend, const Properties &props,
                   unsigned int sub_shader_id):
      ItemShader(color_item_shader, backend, sub_shader_id),
      m_properties(props)
    {}

    ColorItemShader(const ItemShaderBackend &backend,
                    unsigned int sub_shader_id,
                    const Properties &props):
      ItemShader(color_item_shader, backend, sub_shader_id),
      m_properties(props)
    {}

    ColorItemShader(const ColorItemShader &parent, unsigned int sub_shader_id,
                    const Properties &props):
      ItemShader(parent, sub_shader_id),
      m_properties(props)
    {}

    ColorItemShader(const ColorItemShader &parent, const Properties &props,
                   unsigned int sub_shader_id):
      ItemShader(parent, sub_shader_id),
      m_properties(props)
    {}

    Properties m_properties;
  };

  /*!
   * \brief
   * An astral::ShadowMapItemShader represents a shader for drawing to a shadow map.
   */
  class ShadowMapItemShader final:public ItemShader
  {
  public:
    /*!
     * Ctor.
     * \param backend opaque data from backend
     * \param sub_shader_id which sub-shader
     */
    static
    reference_counted_ptr<ShadowMapItemShader>
    create(const ItemShaderBackend &backend,
           unsigned int sub_shader_id = 0)
    {
      return ASTRALnew ShadowMapItemShader(backend, sub_shader_id);
    }

    /*!
     * Ctor that comes from a sub-shader of a parent
     * astral::ShadowMapItemShader
     * \param parent shader
     * \param sub_shader_id which sub-shader of the parent
     */
    static
    reference_counted_ptr<ShadowMapItemShader>
    create(const ShadowMapItemShader &parent,
           unsigned int sub_shader_id)
    {
      return ASTRALnew ShadowMapItemShader(parent, sub_shader_id);
    }

  private:
    ShadowMapItemShader(const ItemShaderBackend &backend,
                        unsigned int sub_shader_id):
      ItemShader(shadow_map_item_shader, backend, sub_shader_id)
    {}

    ShadowMapItemShader(const ShadowMapItemShader &parent,
                        unsigned int sub_shader_id):
      ItemShader(parent, sub_shader_id)
    {}
  };

  /*!
   * \brief
   * An astral::ItemShaderBackend represents opaque data
   * for a rendering backend for each non-child shader.
   */
  class ItemShaderBackend:public reference_counted<ItemShaderBackend>::non_concurrent
  {
  public:
    virtual
    ~ItemShaderBackend()
    {}

    /*!
     * An astral::ItemShaderBackend represents N different shaders where
     * N = num_sub_shaders(). Those shaders have Item::shaderID() as
     * begin_shaderID() + I where 0 <= I < N; i.e., begin_shaderID()
     * represents the first shader in the block of shaders of an
     * astral::ItemShaderBackend(). It is guaranteed that no two
     * distinct astral::ItemShaderBackend() will have these shader
     * ID ranges intersect.
     */
    unsigned int
    begin_shaderID(void) const
    {
      return m_begin_shaderID;
    }

    /*!
     * Returns the number of sub-shader
     */
    unsigned int
    num_sub_shaders(void) const
    {
      return m_num_sub_shaders;
    }

    /*!
     * Each astral::ItemShaderBackend is give a unique
     * uniqueID() value. The implementation of astral::Renderer
     * will reorder draws by uniqueID() when drawing order
     * does not impact output in order to reduce GPU state
     * thrashing.
     */
    unsigned int
    uniqueID(void) const
    {
      return m_uniqueID;
    }

    /*!
     * Create an astral::ColorItemShader using the shader code
     * of this astral::ItemShaderBackend
     * \param properties properties of the returned astral::ColorItemShader
     * \param sub_shader_id what sub-shader, value must be strcitly less
     *                      than num_sub_shaders()
     */
    reference_counted_ptr<const ColorItemShader>
    create_color_item_shader(const ColorItemShader::Properties &properties,
                             unsigned int sub_shader_id = 0u) const
    {
      return ColorItemShader::create(*this, properties, sub_shader_id);
    }

    /*!
     * Create an astral::MaskItemShader using the shader code
     * of this astral::ItemShaderBackend
     * \param sub_shader_id what sub-shader, value must be strcitly less
     *                      than num_sub_shaders()
     */
    reference_counted_ptr<const MaskItemShader>
    create_mask_shader(unsigned int sub_shader_id = 0u) const
    {
      return MaskItemShader::create(*this, sub_shader_id);
    }

    /*!
     * Create an astral::ShadowMapItemShader using the shader code
     * of this astral::ItemShaderBackend
     * \param sub_shader_id what sub-shader, value must be strcitly less
     *                      than num_sub_shaders()
     */
    reference_counted_ptr<const ShadowMapItemShader>
    create_shadow_map_shader(unsigned int sub_shader_id = 0u) const
    {
      return ShadowMapItemShader::create(*this, sub_shader_id);
    }

  protected:
    /*!
     * Ctor
     * \param engine astral::RenderEngine to which shader will belong
     * \param num_sub_shaders number of sub-shader of the shader
     */
    ItemShaderBackend(RenderEngine &engine, unsigned int num_sub_shaders);

  private:
    unsigned int m_begin_shaderID, m_num_sub_shaders;
    unsigned int m_uniqueID;
  };

  inline
  ItemShader::
  ItemShader(enum type_t tp, const ItemShaderBackend &backend,
             unsigned int sub_shader_id):
    m_backend(&backend),
    m_num_sub_shaders(m_backend->num_sub_shaders() - sub_shader_id),
    m_shaderID(m_backend->begin_shaderID() + sub_shader_id),
    m_sub_shader(sub_shader_id),
    m_type(tp)
  {
    ASTRALassert(sub_shader_id < m_backend->num_sub_shaders());
  }

  inline
  ItemShader::
  ItemShader(const ItemShader &parent, unsigned int sub_shader_id):
    m_backend(parent.m_backend),
    m_num_sub_shaders(parent.num_sub_shaders() - sub_shader_id),
    m_shaderID(parent.shaderID() + sub_shader_id),
    m_sub_shader(sub_shader_id),
    m_type(parent.m_type)
  {
    ASTRALassert(sub_shader_id < parent.num_sub_shaders());
  }
}

#endif
