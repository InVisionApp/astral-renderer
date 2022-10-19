/*!
 * \file wasm_missing_gl_enums.hpp
 * \brief file wasm_missing_gl_enums.hpp
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

#ifndef ASTRAL_WASM_MISSING_GL_ENUMS_HPP
#define ASTRAL_WASM_MISSING_GL_ENUMS_HPP

#include <astral/util/gl/astral_gl.hpp>

/* This silliness is to pick various enumerations that
 * GLES3/gl3.h does not have which Astral's GL backend
 * and utilities assume to exist; only needed for WASM
 * builds
 */

///@cond
#ifndef ASTRAL_GL_CLIP_DISTANCE0
#define ASTRAL_GL_CLIP_DISTANCE0 0x3000
#endif

#ifndef ASTRAL_GL_MAX_CLIP_DISTANCES
#define ASTRAL_GL_MAX_CLIP_DISTANCES 0x0D32
#endif

#ifndef ASTRAL_GL_COMPLETION_STATUS_KHR
#define ASTRAL_GL_COMPLETION_STATUS_KHR 0x91B1
#endif

#ifndef ASTRAL_GL_UNPACK_SWAP_BYTES
#define ASTRAL_GL_UNPACK_SWAP_BYTES 0x0CF0
#endif

#ifndef ASTRAL_GL_UNPACK_LSB_FIRST
#define ASTRAL_GL_UNPACK_LSB_FIRST 0x0CF1
#endif

#ifndef ASTRAL_GL_PACK_IMAGE_HEIGHT
#define ASTRAL_GL_PACK_IMAGE_HEIGHT 0x806C
#endif

#ifndef ASTRAL_GL_PACK_SKIP_IMAGES
#define ASTRAL_GL_PACK_SKIP_IMAGES 0x806B
#endif

#ifndef ASTRAL_GL_PACK_LSB_FIRST
#define ASTRAL_GL_PACK_LSB_FIRST 0x0D01
#endif

#ifndef ASTRAL_GL_PACK_SWAP_BYTES
#define ASTRAL_GL_PACK_SWAP_BYTES 0x0D00
#endif

#ifndef ASTRAL_GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE
#define ASTRAL_GL_TRANSLATED_SHADER_SOURCE_LENGTH_ANGLE 0x93A0
#endif
///@endcond



#endif
