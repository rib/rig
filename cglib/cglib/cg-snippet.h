/*
 * CGlib
 *
 * A Low-Level GPU Graphics and Utilities API
 *
 * Copyright (C) 2011, 2013 Intel Corporation.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Authors:
 *   Neil Roberts <neil@linux.intel.com>
 */

#if !defined(__CG_H_INSIDE__) && !defined(CG_COMPILATION)
#error "Only <cg/cg.h> can be included directly."
#endif

#ifndef __CG_SNIPPET_H__
#define __CG_SNIPPET_H__

CG_BEGIN_DECLS

/**
 * SECTION:cg-snippet
 * @short_description: Functions for creating and manipulating shader snippets
 *
 * #cg_snippet_t<!-- -->s are used to modify or replace parts of a
 * #cg_pipeline_t using GLSL. GLSL is a programming language supported
 * by OpenGL on programmable hardware to provide a more flexible
 * description of what should be rendered. A description of GLSL
 * itself is outside the scope of this documentation but any good
 * OpenGL book should help to describe it.
 *
 * Unlike in OpenGL, when using GLSL with CGlib it is possible to write
 * short snippets to replace small sections of the pipeline instead of
 * having to replace the whole of either the vertex or fragment
 * pipelines. Of course it is also possible to replace the whole of
 * the pipeline if needed.
 *
 * Each snippet is a standalone chunk of code which would attach to
 * the pipeline at a particular point. The code is split into four
 * separate strings (all of which are optional):
 *
 * <glosslist>
 *  <glossentry>
 *   <glossterm>declarations</glossterm>
 *   <glossdef><para>
 * The code in this string will be inserted outside of any function in
 * the global scope of the shader. This can be used to declare
 * uniforms, attributes, varyings and functions to be used by the
 * snippet.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>pre</glossterm>
 *   <glossdef><para>
 * The code in this string will be inserted before the hook point.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>post</glossterm>
 *   <glossdef><para>
 * The code in this string will be inserted after the hook point. This
 * can be used to modify the results of the builtin generated code for
 * that hook point.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>replace</glossterm>
 *   <glossdef><para>
 * If present the code in this string will replace the generated code
 * for the hook point.
 *   </para></glossdef>
 *  </glossentry>
 * </glosslist>
 *
 * All of the strings apart from the declarations string of a pipeline
 * are generated in a single function so they can share variables
 * declared from one string in another. The scope of the code is
 * limited to each snippet so local variables declared in the snippet
 * will not collide with variables declared in another
 * snippet. However, code in the 'declarations' string is global to
 * the shader so it is the application's responsibility to ensure that
 * variables declared here will not collide with those from other
 * snippets.
 *
 * The snippets can be added to a pipeline with
 * cg_pipeline_add_snippet() or
 * cg_pipeline_add_layer_snippet(). Which function to use depends on
 * which hook the snippet is targetting. The snippets are all
 * generated in the order they are added to the pipeline. That is, the
 * post strings are executed in the order they are added to the
 * pipeline and the pre strings are executed in reverse order. If any
 * replace strings are given for a snippet then any other snippets
 * with the same hook added before that snippet will be ignored. The
 * different hooks are documented under #cg_snippet_hook_t.
 *
 * For portability with GLES2, it is recommended not to use the GLSL
 * builtin names such as gl_FragColor. Instead there are replacement
 * names under the cg_* namespace which can be used instead. These
 * are:
 *
 * <glosslist>
 *  <glossentry>
 *   <glossterm>uniform mat4
 *         <emphasis>cg_modelview_matrix</emphasis></glossterm>
 *   <glossdef><para>
 *    The current modelview matrix. This is equivalent to
 *    #gl_ModelViewMatrix.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>uniform mat4
 *         <emphasis>cg_projection_matrix</emphasis></glossterm>
 *   <glossdef><para>
 *    The current projection matrix. This is equivalent to
 *    #gl_ProjectionMatrix.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>uniform mat4
 *         <emphasis>cg_modelview_projection_matrix</emphasis></glossterm>
 *   <glossdef><para>
 *    The combined modelview and projection matrix. A vertex shader
 *    would typically use this to transform the incoming vertex
 *    position. The separate modelview and projection matrices are
 *    usually only needed for lighting calculations. This is
 *    equivalent to #gl_ModelViewProjectionMatrix.
 *   </para></glossdef>
 *  </glossentry>
 * </glosslist>
 *
 * In a vertex shader, the following are also available:
 *
 * <glosslist>
 *  <glossentry>
 *   <glossterm>in vec4
 *         <emphasis>cg_position_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The incoming vertex position. This is equivalent to #gl_Vertex.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>in vec4
 *         <emphasis>cg_color_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The incoming vertex color. This is equivalent to #gl_Color.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>in vec4
 *         <emphasis>cg_tex_coord_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The texture coordinate for layer 0. This is an alternative name
 *    for #cg_tex_coord0_in.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>in vec4
 *         <emphasis>cg_tex_coord0_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The texture coordinate for the layer 0. This is equivalent to
 *    #gl_MultiTexCoord0. There will also be #cg_tex_coord1_in and
 *    so on if more layers are added to the pipeline.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>in vec3
 *         <emphasis>cg_normal_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The normal of the vertex. This is equivalent to #gl_Normal.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>vec4
 *         <emphasis>cg_position_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated position of the vertex. This must be written to
 *    in all vertex shaders. This is equivalent to #gl_Position.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>float
 *         <emphasis>cg_point_size_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The incoming point size from the cg_point_size_in attribute.
 *    This is only available if
 *    cg_pipeline_set_per_vertex_point_size() is set on the
 *    pipeline.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>float
 *         <emphasis>cg_point_size_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated size of a point. This is equivalent to #gl_PointSize.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>out vec4
 *         <emphasis>cg_color_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated color of a vertex. This is equivalent to #gl_FrontColor.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>out vec4
 *         <emphasis>cg_tex_coord0_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated texture coordinate for layer 0 of the pipeline.
 *    This is equivalent to #gl_TexCoord[0]. There will also be
 *    #cg_tex_coord1_out and so on if more layers are added to the
 *    pipeline. In the fragment shader, this varying is called
 *    #cg_tex_coord0_in.
 *   </para></glossdef>
 *  </glossentry>
 * </glosslist>
 *
 * In a fragment shader, the following are also available:
 *
 * <glosslist>
 *  <glossentry>
 *   <glossterm>in vec4 <emphasis>cg_color_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The calculated color of a vertex. This is equivalent to #gl_FrontColor.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>in vec4
 *              <emphasis>cg_tex_coord0_in</emphasis></glossterm>
 *   <glossdef><para>
 *    The texture coordinate for layer 0. This is equivalent to
 *    #gl_TexCoord[0]. There will also be #cg_tex_coord1_in and so
 *    on if more layers are added to the pipeline.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>vec4 <emphasis>cg_color_out</emphasis></glossterm>
 *   <glossdef><para>
 *    The final calculated color of the fragment. All fragment shaders
 *    must write to this variable. This is equivalent to
 *    #gl_FrontColor.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>float <emphasis>cg_depth_out</emphasis></glossterm>
 *   <glossdef><para>
 *    An optional output variable specifying the depth value to use
 *    for this fragment. This is equivalent to #gl_FragDepth.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>bool <emphasis>cg_front_facing</emphasis></glossterm>
 *   <glossdef><para>
 *    A readonly variable that will be true if the current primitive
 *    is front facing. This can be used to implement two-sided
 *    coloring algorithms. This is equivalent to #gl_FrontFacing.
 *   </para></glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>vec2 <emphasis>cg_point_coord</emphasis></glossterm>
 *   <glossdef><para>
 *    When rendering points, this will contain a vec2 which represents
 *    the position within the point of the current fragment.
 *    vec2(0.0,0.0) will be the topleft of the point and vec2(1.0,1.0)
 *    will be the bottom right. Note that there is currently a bug in
 *    CGlib where when rendering to an offscreen buffer these
 *    coordinates will be upside-down. The value is undefined when not
 *    rendering points. This builtin can only be used if the
 *    %CG_FEATURE_ID_POINT_SPRITE feature is available.
 *   </para></glossdef>
 *  </glossentry>
 * </glosslist>
 *
 * Here is an example of using a snippet to add a desaturate effect to the
 * generated color on a pipeline.
 *
 * <programlisting>
 *   cg_pipeline_t *pipeline = cg_pipeline_new ();
 *
 *   /<!-- -->* Set up the pipeline here, ie by adding a texture or other
 *      layers *<!-- -->/
 *
 *   /<!-- -->* Create the snippet. The first string is the declarations which
 *      we will use to add a uniform. The second is the 'post' string which
 *      will contain the code to perform the desaturation. *<!-- -->/
 *   cg_snippet_t *snippet =
 *     cg_snippet_new (CG_SNIPPET_HOOK_FRAGMENT,
 *                       "uniform float factor;",
 *                       "float gray = dot (vec3 (0.299, 0.587, 0.114), "
 *                       "                  cg_color_out.rgb);"
 *                       "cg_color_out.rgb = mix (vec3 (gray),"
 *                       "                          cg_color_out.rgb,"
 *                       "                          factor);");
 *
 *   /<!-- -->* Add it to the pipeline *<!-- -->/
 *   cg_pipeline_add_snippet (pipeline, snippet);
 *   /<!-- -->* The pipeline keeps a reference to the snippet
 *      so we don't need to *<!-- -->/
 *   cg_object_unref (snippet);
 *
 *   /<!-- -->* Update the custom uniform on the pipeline *<!-- -->/
 *   int location = cg_pipeline_get_uniform_location (pipeline, "factor");
 *   cg_pipeline_set_uniform_1f (pipeline, location, 0.5f);
 *
 *   /<!-- -->* Now we can render with the snippet as usual *<!-- -->/
 *   cg_framebuffer_draw_rectangle (fb, pipeline, 0, 0, 10, 10);
 * </programlisting>
 */
typedef struct _cg_snippet_t cg_snippet_t;

#define CG_SNIPPET(OBJECT) ((cg_snippet_t *)OBJECT)

/* Enumeration of all the hook points that a snippet can be attached
   to within a pipeline. */
/**
 * cg_snippet_hook_t:
 * @CG_SNIPPET_HOOK_VERTEX_GLOBALS: A hook for declaring global data
 *   that can be shared with all other snippets that are on a vertex
 *   hook.
 * @CG_SNIPPET_HOOK_FRAGMENT_GLOBALS: A hook for declaring global
 *   data wthat can be shared with all other snippets that are on a
 *   fragment hook.
 * @CG_SNIPPET_HOOK_VERTEX: A hook for the entire vertex processing
 *   stage of the pipeline.
 * @CG_SNIPPET_HOOK_VERTEX_TRANSFORM: A hook for the vertex transformation.
 * @CG_SNIPPET_HOOK_POINT_SIZE: A hook for manipulating the point
 *   size of a vertex. This is only used if
 *   cg_pipeline_set_per_vertex_point_size() is enabled on the
 *   pipeline.
 * @CG_SNIPPET_HOOK_FRAGMENT: A hook for the entire fragment
 *   processing stage of the pipeline.
 * @CG_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM: A hook for transforming
 *   the texture coordinates for a layer.
 * @CG_SNIPPET_HOOK_LAYER_FRAGMENT: A hook for the fragment
 *   processing of a particular layer.
 * @CG_SNIPPET_HOOK_TEXTURE_LOOKUP: A hook for the texture lookup
 *   stage of a given layer in a pipeline.
 *
 * #cg_snippet_hook_t is used to specify a location within a
 * #cg_pipeline_t where the code of the snippet should be used when it
 * is attached to a pipeline.
 *
 * <glosslist>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_VERTEX_GLOBALS</glossterm>
 *   <glossdef>
 * <para>
 * Adds a shader snippet at the beginning of the global section of the
 * shader for the vertex processing. Any declarations here can be
 * shared with all other snippets that are attached to a vertex hook.
 * Only the ‘declarations’ string is used and the other strings are
 * ignored.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_FRAGMENT_GLOBALS</glossterm>
 *   <glossdef>
 * <para>
 * Adds a shader snippet at the beginning of the global section of the
 * shader for the fragment processing. Any declarations here can be
 * shared with all other snippets that are attached to a fragment
 * hook. Only the ‘declarations’ string is used and the other strings
 * are ignored.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_VERTEX</glossterm>
 *   <glossdef>
 * <para>
 * Adds a shader snippet that will hook on to the vertex processing
 * stage of the pipeline. This gives a chance for the application to
 * modify the vertex attributes generated by the shader. Typically the
 * snippet will modify cg_color_out or cg_position_out builtins.
 * </para>
 * <para>
 * The ‘declarations’ string in @snippet will be inserted in the
 * global scope of the shader. Use this to declare any uniforms,
 * attributes or functions that the snippet requires.
 * </para>
 * <para>
 * The ‘pre’ string in @snippet will be inserted at the top of the
 * main() function before any vertex processing is done.
 * </para>
 * <para>
 * The ‘replace’ string in @snippet will be used instead of the
 * generated vertex processing if it is present. This can be used if
 * the application wants to provide a complete vertex shader and
 * doesn't need the generated output from CGlib.
 * </para>
 * <para>
 * The ‘post’ string in @snippet will be inserted after all of the
 * standard vertex processing is done. This can be used to modify the
 * outputs.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_VERTEX_TRANSFORM</glossterm>
 *   <glossdef>
 * <para>
 * Adds a shader snippet that will hook on to the vertex transform stage.
 * Typically the snippet will use the cg_modelview_matrix,
 * cg_projection_matrix and cg_modelview_projection_matrix matrices and the
 * cg_position_in attribute. The hook must write to cg_position_out.
 * The default processing for this hook will multiply cg_position_in by
 * the combined modelview-projection matrix and store it on cg_position_out.
 * </para>
 * <para>
 * The ‘declarations’ string in @snippet will be inserted in the
 * global scope of the shader. Use this to declare any uniforms,
 * attributes or functions that the snippet requires.
 * </para>
 * <para>
 * The ‘pre’ string in @snippet will be inserted at the top of the
 * main() function before the vertex transform is done.
 * </para>
 * <para>
 * The ‘replace’ string in @snippet will be used instead of the
 * generated vertex transform if it is present.
 * </para>
 * <para>
 * The ‘post’ string in @snippet will be inserted after all of the
 * standard vertex transformation is done. This can be used to modify the
 * cg_position_out in addition to the default processing.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_POINT_SIZE</glossterm>
 *   <glossdef>
 * <para>
 * Adds a shader snippet that will hook on to the point size
 * calculation step within the vertex shader stage. The snippet should
 * write to the builtin cg_point_size_out with the new point size.
 * The snippet can either read cg_point_size_in directly and write a
 * new value or first read an existing value in cg_point_size_out
 * that would be set by a previous snippet. Note that this hook is
 * only used if cg_pipeline_set_per_vertex_point_size() is enabled
 * on the pipeline.
 * </para>
 * <para>
 * The ‘declarations’ string in @snippet will be inserted in the
 * global scope of the shader. Use this to declare any uniforms,
 * attributes or functions that the snippet requires.
 * </para>
 * <para>
 * The ‘pre’ string in @snippet will be inserted just before
 * calculating the point size.
 * </para>
 * <para>
 * The ‘replace’ string in @snippet will be used instead of the
 * generated point size calculation if it is present.
 * </para>
 * <para>
 * The ‘post’ string in @snippet will be inserted after the
 * standard point size calculation is done. This can be used to modify
 * cg_point_size_out in addition to the default processing.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_FRAGMENT</glossterm>
 *   <glossdef>
 * <para>
 * Adds a shader snippet that will hook on to the fragment processing
 * stage of the pipeline. This gives a chance for the application to
 * modify the fragment color generated by the shader. Typically the
 * snippet will modify cg_color_out.
 * </para>
 * <para>
 * The ‘declarations’ string in @snippet will be inserted in the
 * global scope of the shader. Use this to declare any uniforms,
 * attributes or functions that the snippet requires.
 * </para>
 * <para>
 * The ‘pre’ string in @snippet will be inserted at the top of the
 * main() function before any fragment processing is done.
 * </para>
 * <para>
 * The ‘replace’ string in @snippet will be used instead of the
 * generated fragment processing if it is present. This can be used if
 * the application wants to provide a complete fragment shader and
 * doesn't need the generated output from CGlib.
 * </para>
 * <para>
 * The ‘post’ string in @snippet will be inserted after all of the
 * standard fragment processing is done. At this point the generated
 * value for the rest of the pipeline state will already be in
 * cg_color_out so the application can modify the result by altering
 * this variable.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM</glossterm>
 *    <glossdef>
 * <para>
 * Adds a shader snippet that will hook on to the texture coordinate
 * transformation of a particular layer. This can be used to replace
 * the processing for a layer or to modify the results.
 * </para>
 * <para>
 * Within the snippet code for this hook there is an extra variable
 * called cg_tex_coord and represents the incoming and outgoing
 * texture coordinate. On entry to the hook, cg_tex_coord contains
 * the value of the corresponding texture coordinate attribute for
 * this layer. The hook is expected to modify this variable. The
 * output will be passed as a varying to the fragment processing
 * stage. The default code will leave cg_tex_coord untouched.
 * </para>
 * <para>
 * The ‘declarations’ string in @snippet will be inserted in the
 * global scope of the shader. Use this to declare any uniforms,
 * attributes or functions that the snippet requires.
 * </para>
 * <para>
 * The ‘pre’ string in @snippet will be inserted just before the
 * fragment processing for this layer. At this point cg_tex_coord
 * still contains the value of the texture coordinate attribute.
 * </para>
 * <para>
 * If a ‘replace’ string is given then this will be used instead of
 * the default fragment processing for this layer. The snippet can
 * modify cg_tex_coord or leave it as is to apply no transformation.
 * </para>
 * <para>
 * The ‘post’ string in @snippet will be inserted just after the
 * transformation. At this point cg_tex_coord will contain the
 * results of the transformation but it can be further modified by the
 * snippet.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_LAYER_FRAGMENT</glossterm>
 *    <glossdef>
 * <para>
 * Adds a shader snippet that will hook on to the fragment processing
 * of a particular layer. This can be used to replace the processing
 * for a layer or to modify the results.
 * </para>
 * <para>
 * Within the snippet code for this hook there is a local vec4
 * variable called ‘frag’ for tracking the hook's fragment value.
 * Additional local variables can be inserted with the
 * 'pre' section. 'frag' can be initialized in the 'replace' section
 * or the default value can be modified in the ‘post’ section.
 * Within the 'replace' section the 'frag' variable initially
 * corresponds to the fragment value for the previous layer.
 * </para>
 * <para>
 * The ‘declarations’ string in @snippet will be inserted in the
 * global scope of the shader. Use this to declare any uniforms,
 * attributes or functions that the snippet requires.
 * </para>
 * <para>
 * The ‘pre’ string in @snippet will be inserted in just before the
 * fragment processing for this layer.
 * </para>
 * <para>
 * If a ‘replace’ string is given then this will be used instead of
 * the default fragment processing for this layer. The snippet should
 * write to the ‘frag’ variable in that case. It can sometimes also
 * be useful to set an empty 'replace' string as a way to skip
 * modulating a layer with the previous layer.
 * </para>
 * <para>
 * The ‘post’ string in @snippet will be inserted just after the
 * fragment processing for the layer. The results can be modified by
 * changing the value of the ‘frag’ variable.
 * </para>
 *   </glossdef>
 *  </glossentry>
 *  <glossentry>
 *   <glossterm>%CG_SNIPPET_HOOK_TEXTURE_LOOKUP</glossterm>
 *   <glossdef>
 * <para>
 * Adds a shader snippet that will hook on to the texture lookup part
 * of a given layer. This gives a chance for the application to modify
 * the coordinates that will be used for the texture lookup or to
 * alter the returned texel.
 * </para>
 * <para>
 * Within the snippet code for this hook there are three extra
 * variables available. ‘cg_sampler’ is a sampler object
 * representing the sampler for the layer where the snippet is
 * attached. ‘cg_tex_coord’ is a vec4 which contains the texture
 * coordinates that will be used for the texture lookup. This can be
 * modified. ‘cg_texel’ will contain the result of the texture
 * lookup. This can also be modified.
 * </para>
 * <para>
 * The ‘declarations’ string in @snippet will be inserted in the
 * global scope of the shader. Use this to declare any uniforms,
 * attributes or functions that the snippet requires.
 * </para>
 * <para>
 * The ‘pre’ string in @snippet will be inserted at the top of the
 * main() function before any fragment processing is done. This is a
 * good place to modify the cg_tex_coord variable.
 * </para>
 * <para>
 * If a ‘replace’ string is given then this will be used instead of a
 * the default texture lookup. The snippet would typically use its own
 * sampler in this case.
 * </para>
 * <para>
 * The ‘post’ string in @snippet will be inserted after texture lookup
 * has been preformed. Here the snippet can modify the cg_texel
 * variable to alter the returned texel.
 * </para>
 *   </glossdef>
 *  </glossentry>
 * </glosslist>
 *
 * Stability: Unstable
 */
typedef enum {
    /* Per pipeline vertex hooks */
    CG_SNIPPET_HOOK_VERTEX = 0,
    CG_SNIPPET_HOOK_VERTEX_TRANSFORM,
    CG_SNIPPET_HOOK_VERTEX_GLOBALS,
    CG_SNIPPET_HOOK_POINT_SIZE,

    /* Per pipeline fragment hooks */
    CG_SNIPPET_HOOK_FRAGMENT = 2048,
    CG_SNIPPET_HOOK_FRAGMENT_GLOBALS,

    /* Per layer vertex hooks */
    CG_SNIPPET_HOOK_TEXTURE_COORD_TRANSFORM = 4096,

    /* Per layer fragment hooks */
    CG_SNIPPET_HOOK_LAYER_FRAGMENT = 6144,
    CG_SNIPPET_HOOK_TEXTURE_LOOKUP
} cg_snippet_hook_t;

/**
 * cg_snippet_new:
 * @hook: The point in the pipeline that this snippet will wrap around
 *   or replace.
 * @declarations: The source code for the declarations for this
 *   snippet or %NULL. See cg_snippet_set_declarations().
 * @post: The source code to run after the hook point where this
 *   shader snippet is attached or %NULL. See cg_snippet_set_post().
 *
 * Allocates and initializes a new snippet with the given source strings.
 *
 * Return value: a pointer to a new #cg_snippet_t
 *
 * Stability: Unstable
 */
cg_snippet_t *cg_snippet_new(cg_snippet_hook_t hook,
                             const char *declarations,
                             const char *post);

/**
 * cg_snippet_get_hook:
 * @snippet: A #cg_snippet_t
 *
 * Return value: the hook that was set when cg_snippet_new() was
 *   called.
 * Stability: Unstable
 */
cg_snippet_hook_t cg_snippet_get_hook(cg_snippet_t *snippet);

/**
 * cg_is_snippet:
 * @object: A #cg_object_t pointer
 *
 * Gets whether the given @object references an existing snippet object.
 *
 * Return value: %true if the @object references a #cg_snippet_t,
 *   %false otherwise
 *
 * Stability: Unstable
 */
bool cg_is_snippet(void *object);

/**
 * cg_snippet_set_declarations:
 * @snippet: A #cg_snippet_t
 * @declarations: The new source string for the declarations section
 *   of this snippet.
 *
 * Sets a source string that will be inserted in the global scope of
 * the generated shader when this snippet is used on a pipeline. This
 * string is typically used to declare uniforms, attributes or
 * functions that will be used by the other parts of the snippets.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Stability: Unstable
 */
void cg_snippet_set_declarations(cg_snippet_t *snippet,
                                 const char *declarations);

/**
 * cg_snippet_get_declarations:
 * @snippet: A #cg_snippet_t
 *
 * Return value: the source string that was set with
 *   cg_snippet_set_declarations() or %NULL if none was set.
 *
 * Stability: Unstable
 */
const char *cg_snippet_get_declarations(cg_snippet_t *snippet);

/**
 * cg_snippet_set_pre:
 * @snippet: A #cg_snippet_t
 * @pre: The new source string for the pre section of this snippet.
 *
 * Sets a source string that will be inserted before the hook point in
 * the generated shader for the pipeline that this snippet is attached
 * to. Please see the documentation of each hook point in
 * #cg_pipeline_t for a description of how this string should be used.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Stability: Unstable
 */
void cg_snippet_set_pre(cg_snippet_t *snippet, const char *pre);

/**
 * cg_snippet_get_pre:
 * @snippet: A #cg_snippet_t
 *
 * Return value: the source string that was set with
 *   cg_snippet_set_pre() or %NULL if none was set.
 *
 * Stability: Unstable
 */
const char *cg_snippet_get_pre(cg_snippet_t *snippet);

/**
 * cg_snippet_set_replace:
 * @snippet: A #cg_snippet_t
 * @replace: The new source string for the replace section of this snippet.
 *
 * Sets a source string that will be used instead of any generated
 * source code or any previous snippets for this hook point. Please
 * see the documentation of each hook point in #cg_pipeline_t for a
 * description of how this string should be used.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Stability: Unstable
 */
void cg_snippet_set_replace(cg_snippet_t *snippet, const char *replace);

/**
 * cg_snippet_get_replace:
 * @snippet: A #cg_snippet_t
 *
 * Return value: the source string that was set with
 *   cg_snippet_set_replace() or %NULL if none was set.
 *
 * Stability: Unstable
 */
const char *cg_snippet_get_replace(cg_snippet_t *snippet);

/**
 * cg_snippet_set_post:
 * @snippet: A #cg_snippet_t
 * @post: The new source string for the post section of this snippet.
 *
 * Sets a source string that will be inserted after the hook point in
 * the generated shader for the pipeline that this snippet is attached
 * to. Please see the documentation of each hook point in
 * #cg_pipeline_t for a description of how this string should be used.
 *
 * This function should only be called before the snippet is attached
 * to its first pipeline. After that the snippet should be considered
 * immutable.
 *
 * Stability: Unstable
 */
void cg_snippet_set_post(cg_snippet_t *snippet, const char *post);

/**
 * cg_snippet_get_post:
 * @snippet: A #cg_snippet_t
 *
 * Return value: the source string that was set with
 *   cg_snippet_set_post() or %NULL if none was set.
 *
 * Stability: Unstable
 */
const char *cg_snippet_get_post(cg_snippet_t *snippet);

CG_END_DECLS

#endif /* __CG_SNIPPET_H__ */
