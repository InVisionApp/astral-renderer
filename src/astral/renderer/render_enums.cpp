/*!
 * \file render_enums.hpp
 * \brief file render_enums.hpp
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

#include <astral/renderer/render_enums.hpp>
#include <astral/renderer/gradient.hpp>

#define LABEL_MACRO(X) [X] = #X

astral::c_string
astral::
label(enum image_blit_processing_t tp)
{
  const static c_string values[image_processing_count] =
    {
      LABEL_MACRO(image_blit_stc_mask_processing),
      LABEL_MACRO(image_blit_direct_mask_processing),
      LABEL_MACRO(image_processing_none),
    };

  ASTRALassert(tp < image_processing_count);
  return values[tp];
}

astral::c_string
astral::
label(enum downsampling_processing_t tp)
{
  const static c_string values[downsampling_processing_count] =
    {
      LABEL_MACRO(downsampling_simple),
    };

  ASTRALassert(tp < downsampling_processing_count);
  return values[tp];
}

astral::c_string
astral::
label(enum tile_mode_t tp)
{
  const static c_string values[tile_mode_number_modes] =
    {
      LABEL_MACRO(tile_mode_decal),
      LABEL_MACRO(tile_mode_clamp),
      LABEL_MACRO(tile_mode_mirror),
      LABEL_MACRO(tile_mode_repeat),
      LABEL_MACRO(tile_mode_mirror_repeat),
    };

  ASTRALassert(tp < tile_mode_number_modes);
  return values[tp];
}

astral::c_string
astral::
label(enum Gradient::type_t tp)
{
  const static c_string values[Gradient::number_types] =
    {
      LABEL_MACRO(Gradient::linear),
      LABEL_MACRO(Gradient::sweep),
      LABEL_MACRO(Gradient::radial_extended),
      LABEL_MACRO(Gradient::radial_unextended_opaque),
      LABEL_MACRO(Gradient::radial_unextended_clear),
    };

  ASTRALassert(tp < Gradient::number_types);
  return values[tp];
}

astral::c_string
astral::
label(enum filter_t tp)
{
  const static c_string values[number_filter_modes] =
    {
      LABEL_MACRO(filter_nearest),
      LABEL_MACRO(filter_linear),
      LABEL_MACRO(filter_cubic),
    };

  ASTRALassert(tp < number_filter_modes);
  return values[tp];
}

astral::c_string
astral::
label(enum mipmap_t tp)
{
  const static c_string values[number_mipmap_modes] =
    {
      LABEL_MACRO(mipmap_none),
      LABEL_MACRO(mipmap_nearest),
      LABEL_MACRO(mipmap_ceiling),
      LABEL_MACRO(mipmap_floor),
      LABEL_MACRO(mipmap_chosen),
    };

  ASTRALassert(tp < number_mipmap_modes);
  return values[tp];
}

astral::c_string
astral::
label(enum fill_rule_t tp)
{
  const static c_string values[number_fill_rule] =
    {
      LABEL_MACRO(odd_even_fill_rule),
      LABEL_MACRO(nonzero_fill_rule),
      LABEL_MACRO(complement_odd_even_fill_rule),
      LABEL_MACRO(complement_nonzero_fill_rule),
    };

  ASTRALassert(tp < number_fill_rule);
  return values[tp];
}

astral::c_string
astral::
label(enum mask_channel_t tp)
{
  const static c_string values[number_mask_channel] =
    {
      [mask_channel_red] = "mask_channel_red",
      [mask_channel_green] = "mask_channel_green",
      [mask_channel_blue] = "mask_channel_blue",
      [mask_channel_alpha] = "mask_channel_alpha",
    };

  ASTRALassert(tp < number_mask_channel);
  return values[tp];
}

astral::c_string
astral::
label(enum mask_type_t tp)
{
  const static c_string values[number_mask_type] =
    {
      LABEL_MACRO(mask_type_coverage),
      LABEL_MACRO(mask_type_distance_field),
    };

  ASTRALassert(tp < number_mask_type);
  return values[tp];
}

astral::c_string
astral::
label(enum blend_mode_t tp)
{
  const static c_string values[number_blend_modes] =
    {
      LABEL_MACRO(blend_porter_duff_clear),
      LABEL_MACRO(blend_porter_duff_src),
      LABEL_MACRO(blend_porter_duff_dst),
      LABEL_MACRO(blend_porter_duff_src_over),
      LABEL_MACRO(blend_porter_duff_dst_over),
      LABEL_MACRO(blend_porter_duff_src_in),
      LABEL_MACRO(blend_porter_duff_dst_in),
      LABEL_MACRO(blend_porter_duff_src_out),
      LABEL_MACRO(blend_porter_duff_dst_out),
      LABEL_MACRO(blend_porter_duff_src_atop),
      LABEL_MACRO(blend_porter_duff_dst_atop),
      LABEL_MACRO(blend_porter_duff_xor),
      LABEL_MACRO(blend_porter_duff_plus),
      LABEL_MACRO(blend_porter_duff_modulate),
      LABEL_MACRO(blend_mode_max),
      LABEL_MACRO(blend_mode_min),
      LABEL_MACRO(blend_mode_difference),

      LABEL_MACRO(blend_mode_screen),
      LABEL_MACRO(blend_mode_multiply),
      LABEL_MACRO(blend_mode_overlay),
      LABEL_MACRO(blend_mode_darken),
      LABEL_MACRO(blend_mode_lighten),
      LABEL_MACRO(blend_mode_color_dodge),
      LABEL_MACRO(blend_mode_color_burn),
      LABEL_MACRO(blend_mode_hardlight),
      LABEL_MACRO(blend_mode_softlight),
      LABEL_MACRO(blend_mode_exclusion),
      LABEL_MACRO(blend_mode_hue),
      LABEL_MACRO(blend_mode_saturation),
      LABEL_MACRO(blend_mode_color),
      LABEL_MACRO(blend_mode_luminosity),
    };

  ASTRALassert(tp < number_blend_modes);
  return values[tp];
}

enum astral::blend_impact_t
astral::
blend_impact_with_clear_black(enum blend_mode_t tp)
{
  const static enum blend_impact_t values[number_blend_modes] =
    {
      [blend_porter_duff_clear] = blend_impact_clear_black,
      [blend_porter_duff_src] = blend_impact_clear_black,
      [blend_porter_duff_dst] = blend_impact_none,
      [blend_porter_duff_src_over] = blend_impact_none,
      [blend_porter_duff_dst_over] = blend_impact_none,
      [blend_porter_duff_src_in] = blend_impact_clear_black,
      [blend_porter_duff_dst_in] = blend_impact_clear_black,
      [blend_porter_duff_src_out] = blend_impact_clear_black,
      [blend_porter_duff_dst_out] = blend_impact_none,
      [blend_porter_duff_src_atop] = blend_impact_none,
      [blend_porter_duff_dst_atop] = blend_impact_clear_black,
      [blend_porter_duff_xor] = blend_impact_intertacts,
      [blend_porter_duff_plus] = blend_impact_none,
      [blend_porter_duff_modulate] = blend_impact_clear_black,
      [blend_mode_max] = blend_impact_none,
      [blend_mode_min] = blend_impact_clear_black,
      [blend_mode_difference] = blend_impact_none,

      [blend_mode_screen] = blend_impact_none,
      [blend_mode_multiply] = blend_impact_none,
      [blend_mode_overlay] = blend_impact_none,
      [blend_mode_darken] = blend_impact_none,
      [blend_mode_lighten] = blend_impact_none,
      [blend_mode_color_dodge] = blend_impact_none,
      [blend_mode_color_burn] = blend_impact_none,
      [blend_mode_hardlight] = blend_impact_none,
      [blend_mode_softlight] = blend_impact_none,
      [blend_mode_exclusion] = blend_impact_none,
      [blend_mode_hue] = blend_impact_none,
      [blend_mode_saturation] = blend_impact_none,
      [blend_mode_color] = blend_impact_none,
      [blend_mode_luminosity] = blend_impact_none,
    };

  ASTRALassert(tp < number_blend_modes);
  return values[tp];
}

astral::c_string
astral::
label(enum cap_t tp)
{
  const static c_string values[number_cap_t] =
    {
      LABEL_MACRO(cap_flat),
      LABEL_MACRO(cap_rounded),
      LABEL_MACRO(cap_square),
    };

  ASTRALassert(tp < number_cap_t);
  return values[tp];
}

astral::c_string
astral::
label(enum join_t tp)
{
  unsigned int v(tp);
  const static c_string values[number_join_t + 1] =
    {
      LABEL_MACRO(join_rounded),
      LABEL_MACRO(join_bevel),
      LABEL_MACRO(join_miter),
      LABEL_MACRO(join_none),
    };

  ASTRALassert(v < number_join_t + 1);
  return values[v];
}

astral::c_string
astral::
label(enum anti_alias_t tp)
{
  unsigned int v(tp);
  const static c_string values[number_anti_alias_modes] =
    {
      LABEL_MACRO(with_anti_aliasing),
      LABEL_MACRO(without_anti_aliasing),
    };

  ASTRALassert(v < number_anti_alias_modes);
  return values[v];
}

astral::c_string
astral::
label(enum fill_method_t tp)
{
  unsigned int v(tp);
  const static c_string values[number_fill_method_t] =
    {
      LABEL_MACRO(fill_method_no_sparse),
      LABEL_MACRO(fill_method_sparse_line_clipping),
      LABEL_MACRO(fill_method_sparse_curve_clipping),
    };

  ASTRALassert(v < number_fill_method_t);
  return values[v];
}

astral::c_string
astral::
label(enum clip_window_strategy_t v)
{
  unsigned int tp(v);

  const static c_string values[number_clip_window_strategy] =
    {
      LABEL_MACRO(clip_window_strategy_shader),
      LABEL_MACRO(clip_window_strategy_depth_occlude),
      LABEL_MACRO(clip_window_strategy_depth_occlude_hinted),
    };

  ASTRALassert(tp < number_clip_window_strategy);
  return values[tp];
}

astral::c_string
astral::
label(enum uber_shader_method_t v)
{
  unsigned int tp(v);

  const static c_string values[number_uber_shader_method] =
    {
      LABEL_MACRO(uber_shader_none),
      LABEL_MACRO(uber_shader_active),
      LABEL_MACRO(uber_shader_cumulative),
      LABEL_MACRO(uber_shader_active_blend_cumulative),
      LABEL_MACRO(uber_shader_all),
    };

  ASTRALassert(tp < number_uber_shader_method);
  return values[tp];
}
