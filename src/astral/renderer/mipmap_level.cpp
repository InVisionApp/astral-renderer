/*!
 * \file mipmap_level.cpp
 * \brief file mipmap_level.cpp
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

#include <math.h>
#include <astral/renderer/mipmap_level.hpp>

astral::MipmapLevel::
MipmapLevel(enum mipmap_t mip, const float2x2 &matrix)
{
  switch (mip)
    {
    default:
      ASTRALfailure("bad mipmap_t value passed to draw_image");

    case mipmap_none:
    case mipmap_chosen:
      m_value = 0u;
      break;

    case mipmap_floor:
    case mipmap_ceiling:
      {
        vec2 dx, dy;
        float rho, log2rho;

        dx = vec2(matrix.row_col(0, 0), matrix.row_col(1, 0));
        dy = vec2(matrix.row_col(0, 1), matrix.row_col(1, 1));

        rho = t_max(dot(dx, dx), dot(dy, dy));
        log2rho = t_max(0.0f, 0.5f * ::log2f(rho));

        m_value = static_cast<uint32_t>(log2rho);
        if (mip == mipmap_ceiling && log2rho > static_cast<float>(m_value))
          {
            ++m_value;
          }
      }
      break;
    }
}
