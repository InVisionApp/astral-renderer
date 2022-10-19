/*!
 * \file painter.cpp
 * \brief file painter.cpp
 *
 * Copyright 2022 by InvisionApp.
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

#include <astral/renderer/painter.hpp>

astral::Painter::
Painter(void)
{
}

astral::Painter::
Painter(RenderEncoderBase encoder)
{
  begin(encoder);
}

astral::Painter::
~Painter()
{
  if (active())
    {
      end();
    }

  ASTRALassert(!m_encoder.valid());
  ASTRALassert(!m_start_encoder.valid());
  ASTRALassert(m_layers.empty());
}

void
astral::Painter::
begin(RenderEncoderBase encoder)
{
  ASTRALassert(!m_encoder.valid());
  ASTRALassert(!m_start_encoder.valid());
  ASTRALassert(m_layers.empty());

  ASTRALassert(encoder.valid());
  m_start_encoder = m_encoder = encoder;
}

astral::RenderEncoderBase
astral::Painter::
end(void)
{
  RenderEncoderBase return_value;

  end_layer(LayerIndex(0));
  ASTRALassert(m_start_encoder == m_encoder);

  return_value = m_encoder;
  m_start_encoder = m_encoder = RenderEncoderBase();

  return return_value;
}

void
astral::Painter::
end_layer(reference_counted_ptr<Image> *out_image,
              ScaleTranslate *out_image_transformation_pixel)
{
  ASTRALassert(!m_layers.empty());
  ASTRALassert(m_layers.back().encoder() == m_encoder);
  ASTRALassert(m_layers.back().parent_encoder().valid());

  m_layers.back().parent_encoder().end_layer(m_layers.back());
  m_encoder = m_layers.back().parent_encoder();

  if (out_image)
    {
      *out_image = m_layers.back().encoder().image();
    }

  if (out_image_transformation_pixel)
    {
      *out_image_transformation_pixel = m_layers.back().encoder().image_transformation_pixel();
    }

  m_layers.pop_back();
}

void
astral::Painter::
end_layer(LayerIndex v)
{
  while (current_layer().m_value > v.m_value)
    {
      end_layer();
    }
}

void
astral::Painter::
begin_layer_implement(RenderEncoderLayer layer)
{
  ASTRALassert(layer.parent_encoder() == m_encoder);
  m_layers.push_back(layer);
  m_encoder = layer.encoder();
}
