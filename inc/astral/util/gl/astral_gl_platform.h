/*!
 * \file astral_gl_platform.h
 * \brief file astral_gl_platform.h
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

#ifndef ASTRAL_GL_PLATFORM_H
#define ASTRAL_GL_PLATFORM_H

#if defined(_WIN32) && !defined(APIENTRY) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#endif

/*!\def ASTRAL_GL_APIENTRY
 * Takes the role of APIENTRY in native GL and
 * GL_APIENTRY in native GLES; when porting Astral
 * to other platforms, one may need to adjust the
 * definition of ASTRAL_GL_APIENTRY; the default is
 * to map to APIENTRY if it is defined and nothing
 * otherwise. This is suitable for native on Linux
 * desktop (for GL and GLES), MacOS and MS-Windows.
 */

/*!\def ASTRAL_GL_APIENTRYP
 * Takes the role of APIENTRYP in native GL and
 * GL_APIENTRYP in native GLES; when porting Astral
 * to other platforms, one may need to adjust the
 * definition of ASTRAL_GL_APIENTRYP; the default is
 * to map to APIENTRYP if it is defined and ASTRAL_GL_APIENTRY*
 * otherwise. This is suitable for native on Linux
 * desktop (for GL and GLES), MacOS and MS-Windows.
 */

/*!\def ASTRAL_GLAPI
 * Takes the role of GLAPI in native GL and GL_APICALL
 * in native GLES; when porting Astral to other platforms,
 * one may need to adjust the definition of ASTRAL_GL_API
 * the default is to map to GLAPI if it is defined and extern
 * otherwise. This is suitable for native on Linux desktop
 * (for GL and GLES), MacOS and MS-Windows.
 */

#ifndef APIENTRY
#define ASTRAL_GL_APIENTRY
#else
#define ASTRAL_GL_APIENTRY APIENTRY
#endif

#ifndef APIENTRYP
  #ifdef APIENTRY
     #define ASTRAL_GL_APIENTRYP APIENTRY*
  #else
     #define ASTRAL_GL_APIENTRYP ASTRAL_GL_APIENTRY*
  #endif
#else
#define ASTRAL_GL_APIENTRYP APIENTRYP
#endif

#ifndef GLAPI
#define ASTRAL_GLAPI extern
#else
#define ASTRAL_GLAPI GLAPI
#endif

#endif
