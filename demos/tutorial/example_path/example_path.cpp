/*!
 * \file example_path.cpp
 * \brief example_path.cpp
 *
 * Copyright 2020 by InvisionApp.
 *
 * Contact kevinrogovin@invisionapp.com
 *
 * This Source Code Form is subject to the
 * terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with
 * this file, You can obtain one at
 * http://mozilla.org/MPL/2.0/.
 *
 */


//! [ExamplePath]

#include "initialization.hpp"

class ExamplePath:public Initialization
{
public:
  ExamplePath(DemoRunner *runner, int argc, char **argv);

  virtual
  void
  draw_frame(void) override final;

private:
  astral::Path m_path;
};

//////////////////////////////////////////
// ExamplePath methods
ExamplePath::
ExamplePath(DemoRunner *runner, int argc, char **argv):
  Initialization(runner, argc, argv)
{
  m_path
    // start a new contour
    .move(astral::vec2(50.0f, 35.0f))
    .quadratic_to(astral::vec2(60.0f, 50.0f), astral::vec2(70.0f, 35.0f))
    .arc_to(ASTRAL_PI, astral::vec2(70.0f, -100.0f))
    .cubic_to(astral::vec2(60.0, -150.0), astral::vec2(30.0, -50.0), astral::vec2(0.0, -100.0))
    .arc_close(ASTRAL_PI * 0.5f) //this closes the contour with arc

    // start another contour
    .move(astral::vec2(200.0f, 200.0f))
    .line_to(astral::vec2(400.0f, 200.0f))
    .line_to(astral::vec2(400.0f, 400.0f))
    .line_to(astral::vec2(200.0f, 400.0f))
    .close() //this closes the contour with a line segment

    // start a new contour
    .move(astral::vec2(-50.0f, 100.0f))
    .line_to(astral::vec2(0.0f, 200.0f))
    .line_to(astral::vec2(100.0f, 300.0f))
    .line_to(astral::vec2(150.0f, 325.0f))
    .line_to(astral::vec2(150.0f, 100.0f))
    .close() //this closes the contour with a line segment

    .move(astral::vec2(300.0f, 300.0f));
}

void
ExamplePath::
draw_frame(void)
{
  /* Rendering for a frame begins with Renderer::begin() which
   * returns a astral::RenderEncoderSurface which is essentially
   * just a handle. No commands are sent to the GPU for rendering
   * until Renderer::end() is called. The transformation is initialized
   * as the identity with the convention that the upper-left corner
   * of the RenderTarget is (0, 0).
   */
  astral::RenderEncoderSurface render_encoder;
  render_encoder = renderer().begin(render_target());

  /* apply a translation */
  render_encoder.translate(100.0f, 200.0f);

  /* fill the path with the color white with anti-aliasing
   * applying the odd-even fill rule.
   *
   * Advanced: The class CombinedPath allows one to have the
   * fill of multiple paths -combined- into a single virtual
   * path. Doing so allows one to allow paths to drag across
   * each other dramatically affecting what is drawn without
   * creating new path objects each frame.
   *
   * Advanced 2: when performing a path fill with anti-aliasing,
   * a mask buffer is generated (rendered by the GPU). The
   * method fill_paths() provides, through additional optional
   * arguments, the ability to have that mask buffer as an
   * Image to be resued later along with the ability to specify
   * additional properties on how the mask is generated and used.
   */
  render_encoder.fill_paths(m_path,
                            astral::FillParameters()
                            .fill_rule(astral::odd_even_fill_rule)
                            .aa_mode(astral::with_anti_aliasing),
                            render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 1.0f, 1.0f, 1.0f))));

  /* Clipping in Astral is different and more flexible than found
   * in other vector graphic renderers. Instead of the current
   * clipping being mutable state of a RenderEncoderBase object,
   * clipping is immutable for a RenderEncoder. Instead clipped
   * content is given their own unique RenderEncoder to which to
   * render. The RenderEncoder objects have their own independent
   * state stack as well, i.e. changing their transformation has
   * no impact on the RenderEncoderBase that generated them.
   *
   * We will clip-in and clip-out simutaneously against out filled
   * path. The default bounding box used when clipping by a path
   * is the bounding box of the path. However we will be stroking
   * so we want to inflate the bounding box of clip to capture all
   * of the stroking for the clip-out. One important feature of
   * Astral's clipping model is that there is no crack between
   * clip-in and clip-out content with shader based anti-aliasing.
   *
   * Advanced: just like fill_paths(), this generates an offscreen
   * mask that can be reused. In addition, there is a overload method
   * for clipping to feed it an offscreen mask. As an exercise, reuse
   * the mask buffer from filling the path to do the clipping against
   * the path.
   */
  astral::RenderClipNode clip_encoders;
  astral::BoundingBox<float> bb;

  bb = m_path.bounding_box();
  bb.enlarge(astral::vec2(66.0f, 66.0f));
  clip_encoders = render_encoder.begin_clip_node_logical(astral::RenderEncoderBase::clip_node_both,
                                                         m_path,
                                                         astral::FillParameters()
                                                         .fill_rule(astral::odd_even_fill_rule)
                                                         .aa_mode(astral::with_anti_aliasing),
                                                         astral::FillMaskProperties()
                                                         .complement_bbox(&bb));

  /* we use clip_in to render clipped-in content, lets stroke in red */
  clip_encoders.clip_in().stroke_paths(m_path,
                                       astral::StrokeParameters()
                                       .join(astral::join_rounded)
                                       .cap(astral::cap_rounded)
                                       .width(25.0f),
                                       render_encoder.create_value(astral::Brush().base_color(astral::vec4(1.0f, 0.0f, 0.0f, 0.5f))));

  /* we use clip_in to render clipped-in content, lets stroke in blue with a different width */
  clip_encoders.clip_out().stroke_paths(m_path,
                                        astral::StrokeParameters()
                                        .join(astral::join_rounded)
                                        .cap(astral::cap_rounded)
                                        .width(66.0f),
                                        render_encoder.create_value(astral::Brush().base_color(astral::vec4(0.0f, 0.0f, 1.0f, 0.5f))));

  /* when we are done specifying what draws are clipped-in,
   * we must issue the matching end_clip_node() call to the
   * spawning encoder; it is at end_clip_node() when the
   * clipped content is drawn.
   */
  render_encoder.end_clip_node(clip_encoders);

  /* At Renderer::end() is when the commands to render all the content
   * are sent to the GPU.
   *
   * Advanced: the Image objects returned by fill_paths(), stroke_paths(),
   * clip_node_logical() and RenderEncoderImage::end() are useable
   * within the begin()/end() pair that generated them. However, they
   * are not modifiable though Image::set_pixels() until AFTER Renderer::end()
   * AND such images do not have mipmapping either. Internally, their pixels
   * are not generated until Renderer::end() and Renderer has bookkeeping
   * to make sure that it before it sends a command to use an image for
   * rendering that its pixels are generated. In addition, multiple Image
   * renders are often generated on the same render target, so an applicaiton
   * should NEVER attempt to do its own atlasing to reduce render target
   * changes and instead just let Astral handle the batching.
   */
  renderer().end();
}

int
main(int argc, char **argv)
{
  DemoRunner demo_runner;
  return demo_runner.main<ExamplePath>(argc, argv);
}

//! [ExamplePath]
