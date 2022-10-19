/*!
 * \file ImageSaver.hpp
 * \brief ImageSaver.hpp
 *
 * Copyright 2022 by InvisionApp.
 *
 * Contact itaidanan@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */

#ifndef ASTRAL_DEMO_IMAGE_SAVER_HPP
#define ASTRAL_DEMO_IMAGE_SAVER_HPP

#include <string>
#include <ostream>

#include <astral/util/c_array.hpp>
#include <astral/util/vecN.hpp>
#include <astral/renderer/render_target.hpp>

// Save the content an image of the given size using pixels are an array of all pixels consecutively
// from the top-left of the image to the lower-right.
enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, astral::ivec2 size, astral::c_array<astral::u8vec4> pixels, std::ostream &os, bool flip_y = false);

// Save the contents of the RenderTarget to the specified file.
enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, astral::ivec2 size, astral::c_array<astral::u8vec4> pixels, const std::string &filepath, bool flip_y = false);

// Save the contents of the RenderTarget to the specified stream.
enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, const astral::RenderTarget &target, std::ostream &os);

// Save the contents of the RenderTarget to the specified file.
enum astral::return_code
save_png(bool input_is_with_alpha_premultiplied, const astral::RenderTarget &target, const std::string &filepath);

#endif
