/*************************************************************************/
/*  line_builder.cpp                                                     */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "line_builder.h"
#include "scene/resources/curve.h"

//----------------------------------------------------------------------------
// Util
//----------------------------------------------------------------------------

namespace  {
enum JointOrientation {
    UP = 0,
    DOWN = 1
};
enum SegmentIntersectionResult {
    SEGMENT_PARALLEL = 0,
    SEGMENT_NO_INTERSECT = 1,
    SEGMENT_INTERSECT = 2
};

struct LineBuilderCtx {
    bool _interpolate_color;
    int _last_index[2]; // Index of last up and down vertices of the strip
    LineBuilderCtx() {
        _interpolate_color = false;
        _last_index[0] = 0;
        _last_index[1] = 0;
    }
};
static SegmentIntersectionResult segment_intersection(
        Vector2 a, Vector2 b, Vector2 c, Vector2 d, Vector2 *out_intersection) {
    // http://paulbourke.net/geometry/pointlineplane/ <-- Good stuff
    Vector2 cd = d - c;
    Vector2 ab = b - a;
    float div = cd.y * ab.x - cd.x * ab.y;

    if (Math::abs(div) > 0.001f) {
        float ua = (cd.x * (a.y - c.y) - cd.y * (a.x - c.x)) / div;
        float ub = (ab.x * (a.y - c.y) - ab.y * (a.x - c.x)) / div;
        *out_intersection = a + ua * ab;
        if (ua >= 0.f && ua <= 1.f && ub >= 0.f && ub <= 1.f) {
            return SEGMENT_INTERSECT;
        }
        return SEGMENT_NO_INTERSECT;
    }

    return SEGMENT_PARALLEL;
}

static float calculate_total_distance(Span<const Vector2> points) {
    float d = 0.f;
    for (int i = 1; i < points.size(); ++i) {
        d += points[i].distance_to(points[i - 1]);
    }
    return d;
}

static inline Vector2 rotate90(const Vector2 &v) {
    // Note: the 2D referential is X-right, Y-down
    return Vector2(v.y, -v.x);
}

static inline Vector2 interpolate(const Rect2 &r, const Vector2 &v) {
    return Vector2(
            Math::lerp(r.position.x, r.position.x + r.get_size().x, v.x),
            Math::lerp(r.position.y, r.position.y + r.get_size().y, v.y));
}


// Triangle-strip methods
//void strip_begin(Vector2 up, Vector2 down, Color color, float uvx);
//void strip_new_quad(Vector2 up, Vector2 down, Color color, float uvx);
//void strip_add_quad(Vector2 up, Vector2 down, Color color, float uvx);
//void strip_add_tri(Vector2 up, Orientation orientation);
//void strip_add_arc(Vector2 center, float angle_delta, Orientation orientation);

//void new_arc(Vector2 center, Vector2 vbegin, float angle_delta, Color color, Rect2 uv_rect);

static void new_arc(const Line2DDrawableComponent & from,LineBuildOutput &to,LineBuilderCtx &ctx,
                    Vector2 center, Vector2 vbegin, float angle_delta, Color color, Rect2 uv_rect) {
    // Make a standalone arc that doesn't use existing vertices,
    // with undistorted UVs from within a square section

    float radius = vbegin.length();
    float angle_step = Math_PI / static_cast<float>(from._round_precision);
    float steps = Math::abs(angle_delta) / angle_step;

    if (angle_delta < 0.f) {
        angle_step = -angle_step;
}

    float t = Vector2(1, 0).angle_to(vbegin);
    float end_angle = t + angle_delta;
    Vector2 rpos(0, 0);
    float tt_begin = -Math_PI / 2.f;
    float tt = tt_begin;

    // Center vertice
    int vi = to.vertices.size();
    to.vertices.push_back(center);
    if (ctx._interpolate_color) {
        to.colors.push_back(color);
    }
    if (from._texture_mode != Line2DTextureMode::LINE_TEXTURE_NONE) {
        to.uvs.push_back(interpolate(uv_rect, Vector2(0.5f, 0.5f)));
}

    // Arc vertices
    for (int ti = 0; ti < steps; ++ti, t += angle_step) {
        Vector2 sc = Vector2(Math::cos(t), Math::sin(t));
        rpos = center + sc * radius;

        to.vertices.push_back(rpos);
        if (ctx._interpolate_color) {
            to.colors.push_back(color);
        }
        if (from._texture_mode != Line2DTextureMode::LINE_TEXTURE_NONE) {
            Vector2 tsc = Vector2(Math::cos(tt), Math::sin(tt));
            to.uvs.push_back(interpolate(uv_rect, 0.5f * (tsc + Vector2(1.f, 1.f))));
            tt += angle_step;
        }
    }

    // Last arc vertice
    Vector2 sc = Vector2(Math::cos(end_angle), Math::sin(end_angle));
    rpos = center + sc * radius;
    to.vertices.push_back(rpos);
    if (ctx._interpolate_color) {
        to.colors.push_back(color);
    }
    if (from._texture_mode != Line2DTextureMode::LINE_TEXTURE_NONE) {
        tt = tt_begin + angle_delta;
        Vector2 tsc = Vector2(Math::cos(tt), Math::sin(tt));
        to.uvs.push_back(interpolate(uv_rect, 0.5f * (tsc + Vector2(1.f, 1.f))));
    }

    // Make up triangles
    int vi0 = vi;
    for (int ti = 0; ti < steps; ++ti) {
        to.indices.emplace_back(vi0);
        to.indices.emplace_back(++vi);
        to.indices.emplace_back(vi + 1);
    }
}

static void strip_begin(const Line2DDrawableComponent & from,LineBuildOutput &to,LineBuilderCtx &ctx, Vector2 up, Vector2 down, Color color, float uvx) {
    int vi = to.vertices.size();

    to.vertices.push_back(up);
    to.vertices.push_back(down);

    if (ctx._interpolate_color) {
        to.colors.push_back(color);
        to.colors.push_back(color);
    }

    if (from._texture_mode != Line2DTextureMode::LINE_TEXTURE_NONE) {
        to.uvs.push_back(Vector2(uvx, 0.f));
        to.uvs.push_back(Vector2(uvx, 1.f));
    }

    ctx._last_index[UP] = vi;
    ctx._last_index[DOWN] = vi + 1;
}

static void strip_add_quad(const Line2DDrawableComponent & from,LineBuildOutput &to,LineBuilderCtx &ctx,Vector2 up, Vector2 down, Color color, float uvx) {
    int vi = to.vertices.size();

    to.vertices.push_back(up);
    to.vertices.push_back(down);

    if (ctx._interpolate_color) {
        to.colors.push_back(color);
        to.colors.push_back(color);
    }

    if (from._texture_mode != Line2DTextureMode::LINE_TEXTURE_NONE) {
        to.uvs.push_back(Vector2(uvx, 0.f));
        to.uvs.push_back(Vector2(uvx, 1.f));
    }

    to.indices.push_back(ctx._last_index[UP]);
    to.indices.push_back(vi + 1);
    to.indices.push_back(ctx._last_index[DOWN]);
    to.indices.push_back(ctx._last_index[UP]);
    to.indices.push_back(vi);
    to.indices.push_back(vi + 1);

    ctx._last_index[UP] = vi;
    ctx._last_index[DOWN] = vi + 1;
}

static void strip_new_quad(const Line2DDrawableComponent & from,LineBuildOutput &to,LineBuilderCtx &ctx,Vector2 up, Vector2 down, Color color, float uvx) {
    int vi = to.vertices.size();

    to.vertices.push_back(to.vertices[ctx._last_index[UP]]);
    to.vertices.push_back(to.vertices[ctx._last_index[DOWN]]);
    to.vertices.push_back(up);
    to.vertices.push_back(down);

    if (ctx._interpolate_color) {
        to.colors.push_back(color);
        to.colors.push_back(color);
        to.colors.push_back(color);
        to.colors.push_back(color);
    }

    if (from._texture_mode != Line2DTextureMode::LINE_TEXTURE_NONE) {
        to.uvs.push_back(to.uvs[ctx._last_index[UP]]);
        to.uvs.push_back(to.uvs[ctx._last_index[DOWN]]);
        to.uvs.push_back(Vector2(uvx, UP));
        to.uvs.push_back(Vector2(uvx, DOWN));
    }

    to.indices.push_back(vi);
    to.indices.push_back(vi + 3);
    to.indices.push_back(vi + 1);
    to.indices.push_back(vi);
    to.indices.push_back(vi + 2);
    to.indices.push_back(vi + 3);

    ctx._last_index[UP] = vi + 2;
    ctx._last_index[DOWN] = vi + 3;
}

void strip_add_tri(const Line2DDrawableComponent & from,LineBuildOutput &to,LineBuilderCtx &ctx,Vector2 up, JointOrientation orientation) {
    int vi = to.vertices.size();

    to.vertices.push_back(up);

    if (ctx._interpolate_color) {
        to.colors.emplace_back(to.colors.back());
    }

    JointOrientation opposite_orientation = orientation == UP ? DOWN : UP;

    if (from._texture_mode != Line2DTextureMode::LINE_TEXTURE_NONE) {
        // UVs are just one slice of the texture all along
        // (otherwise we can't share the bottom vertice)
        to.uvs.push_back(to.uvs[ctx._last_index[opposite_orientation]]);
    }

    to.indices.push_back(ctx._last_index[opposite_orientation]);
    to.indices.push_back(vi);
    to.indices.push_back(ctx._last_index[orientation]);

    ctx._last_index[opposite_orientation] = vi;
}

void strip_add_arc(const Line2DDrawableComponent & from,LineBuildOutput &to,LineBuilderCtx &ctx,Vector2 center, float angle_delta, JointOrientation orientation) {
    // Take the two last vertices and extrude an arc made of triangles
    // that all share one of the initial vertices

    JointOrientation opposite_orientation = orientation == UP ? DOWN : UP;
    Vector2 vbegin = to.vertices[ctx._last_index[opposite_orientation]] - center;
    float radius = vbegin.length();
    float angle_step = Math_PI / static_cast<float>(from._round_precision);
    float steps = Math::abs(angle_delta) / angle_step;

    if (angle_delta < 0.f) {
        angle_step = -angle_step;
    }

    float t = Vector2(1, 0).angle_to(vbegin);
    float end_angle = t + angle_delta;
    Vector2 rpos(0, 0);

    // Arc vertices
    for (int ti = 0; ti < steps; ++ti, t += angle_step) {
        rpos = center + Vector2(Math::cos(t), Math::sin(t)) * radius;
        strip_add_tri(from,to,ctx,rpos, orientation);
    }

    // Last arc vertice
    rpos = center + Vector2(Math::cos(end_angle), Math::sin(end_angle)) * radius;
    strip_add_tri(from,to,ctx,rpos, orientation);
}
//----------------------------------------------------------------------------
// LineBuilder
//----------------------------------------------------------------------------

static void build_single(const Line2DDrawableComponent & from,LineBuildOutput &to) {
    // Need at least 2 points to draw a line
    if (from._points.size() < 2) {
        return;
    }
    LineBuilderCtx ctx;
    const float tile_aspect = from._texture ? from._texture->get_size().aspect() : 1.0f; // w/h

    ERR_FAIL_COND(tile_aspect <= 0.f);

    const float hw = from._width / 2.f;
    const float hw_sq = hw * hw;
    const float sharp_limit_sq = from._sharp_limit * from._sharp_limit;
    const int len = from._points.size();

    // Initial values

    Vector2 pos0 = from._points[0];
    Vector2 pos1 = from._points[1];
    Vector2 f0 = (pos1 - pos0).normalized();
    Vector2 u0 = rotate90(f0);
    Vector2 pos_up0 = pos0;
    Vector2 pos_down0 = pos0;

    Color color0;
    Color color1;

    float current_distance0 = 0.f;
    float current_distance1 = 0.f;
    float total_distance = 0.f;
    float width_factor = 1.f;
    ctx._interpolate_color = from._gradient != nullptr;
    bool retrieve_curve = from._curve != nullptr;
    bool distance_required = ctx._interpolate_color || retrieve_curve || from._texture_mode == Line2DTextureMode::LINE_TEXTURE_TILE ||
                             from._texture_mode == Line2DTextureMode::LINE_TEXTURE_STRETCH;
    if (distance_required) {
        total_distance = calculate_total_distance(from._points);
        // Adjust totalDistance.
        // The line's outer length will be a little higher due to begin and end caps
        if (from._begin_cap_mode == Line2DCapMode::LINE_CAP_BOX || from._begin_cap_mode == Line2DCapMode::LINE_CAP_ROUND) {
            if (retrieve_curve) {
                total_distance += from._width * from._curve->interpolate_baked(0.f) * 0.5f;
            } else {
                total_distance += from._width * 0.5f;
            }
        }
        if (from._end_cap_mode == Line2DCapMode::LINE_CAP_BOX || from._end_cap_mode == Line2DCapMode::LINE_CAP_ROUND) {
            if (retrieve_curve) {
                total_distance += from._width * from._curve->interpolate_baked(1.f) * 0.5f;
            } else {
                total_distance += from._width * 0.5f;
            }
        }
    }
    if (ctx._interpolate_color) {
        color0 = from._gradient->get_color(0);
    } else {
        to.colors.push_back(from._default_color);
    }

    float uvx0 = 0.f;
    float uvx1 = 0.f;

    if (retrieve_curve) {
        width_factor = from._curve->interpolate_baked(0.f);
    }

    pos_up0 += u0 * hw * width_factor;
    pos_down0 -= u0 * hw * width_factor;

    // Begin cap
    if (from._begin_cap_mode == Line2DCapMode::LINE_CAP_BOX) {
        // Push back first vertices a little bit
        pos_up0 -= f0 * hw * width_factor;
        pos_down0 -= f0 * hw * width_factor;

        current_distance0 += hw * width_factor;
        current_distance1 = current_distance0;
    } else if (from._begin_cap_mode == Line2DCapMode::LINE_CAP_ROUND) {
        if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_TILE) {
            uvx0 = width_factor * 0.5f / tile_aspect;
        } else if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_STRETCH) {
            uvx0 = from._width * width_factor / total_distance;
        }
        new_arc(from,to,ctx,pos0, pos_up0 - pos0, -Math_PI, color0, Rect2(0.f, 0.f, uvx0 * 2, 1.f));
        current_distance0 += hw * width_factor;
        current_distance1 = current_distance0;
    }

    strip_begin(from,to,ctx,pos_up0, pos_down0, color0, uvx0);

    /*
     *  pos_up0 ------------- pos_up1 --------------------
     *     |                     |
     *   pos0 - - - - - - - - - pos1 - - - - - - - - - pos2
     *     |                     |
     * pos_down0 ------------ pos_down1 ------------------
     *
     *   i-1                     i                      i+1
     */

    // http://labs.hyperandroid.com/tag/opengl-lines
    // (not the same implementation but visuals help a lot)

    // For each additional segment
    for (int i = 1; i < len - 1; ++i) {

        pos1 = from._points[i];
        Vector2 pos2 = from._points[i + 1];

        Vector2 f1 = (pos2 - pos1).normalized();
        Vector2 u1 = rotate90(f1);

        // Determine joint orientation
        const float dp = u0.dot(f1);
        const JointOrientation orientation = (dp > 0.f ? UP : DOWN);

        if (distance_required) {
            current_distance1 += pos0.distance_to(pos1);
        }
        if (ctx._interpolate_color) {
            color1 = from._gradient->get_color_at_offset(current_distance1 / total_distance);
        }
        if (retrieve_curve) {
            width_factor = from._curve->interpolate_baked(current_distance1 / total_distance);
        }

        Vector2 inner_normal0, inner_normal1;
        if (orientation == UP) {
            inner_normal0 = u0 * hw * width_factor;
            inner_normal1 = u1 * hw * width_factor;
        } else {
            inner_normal0 = -u0 * hw * width_factor;
            inner_normal1 = -u1 * hw * width_factor;
        }

        /*
         * ---------------------------
         *                        /
         * 0                     /    1
         *                      /          /
         * --------------------x------    /
         *                    /          /    (here shown with orientation == DOWN)
         *                   /          /
         *                  /          /
         *                 /          /
         *                     2     /
         *                          /
         */

        // Find inner intersection at the joint
        Vector2 corner_pos_in, corner_pos_out;
        SegmentIntersectionResult intersection_result = segment_intersection(
                pos0 + inner_normal0, pos1 + inner_normal0,
                pos1 + inner_normal1, pos2 + inner_normal1,
                &corner_pos_in);

        if (intersection_result == SEGMENT_INTERSECT) {
            // Inner parts of the segments intersect
            corner_pos_out = 2.f * pos1 - corner_pos_in;
        } else {
            // No intersection, segments are either parallel or too sharp
            corner_pos_in = pos1 + inner_normal0;
            corner_pos_out = pos1 - inner_normal0;
        }

        Vector2 corner_pos_up, corner_pos_down;
        if (orientation == UP) {
            corner_pos_up = corner_pos_in;
            corner_pos_down = corner_pos_out;
        } else {
            corner_pos_up = corner_pos_out;
            corner_pos_down = corner_pos_in;
        }

        Line2DJointMode current_joint_mode = from._joint_mode;

        Vector2 pos_up1, pos_down1;
        if (intersection_result == SEGMENT_INTERSECT) {
            // Fallback on bevel if sharp angle is too high (because it would produce very long miters)
            float width_factor_sq = width_factor * width_factor;
            if (current_joint_mode == Line2DJointMode::LINE_JOINT_SHARP && corner_pos_out.distance_squared_to(pos1) / (hw_sq * width_factor_sq) > sharp_limit_sq) {
                current_joint_mode = Line2DJointMode::LINE_JOINT_BEVEL;
            }
            if (current_joint_mode == Line2DJointMode::LINE_JOINT_SHARP) {
                // In this case, we won't create joint geometry,
                // The previous and next line quads will directly share an edge.
                pos_up1 = corner_pos_up;
                pos_down1 = corner_pos_down;
            } else {
                // Bevel or round
                if (orientation == UP) {
                    pos_up1 = corner_pos_up;
                    pos_down1 = pos1 - u0 * hw * width_factor;
                } else {
                    pos_up1 = pos1 + u0 * hw * width_factor;
                    pos_down1 = corner_pos_down;
                }
            }
        } else {
            // No intersection: fallback
            if (current_joint_mode == Line2DJointMode::LINE_JOINT_SHARP) {
                // There is no fallback implementation for LINE_JOINT_SHARP so switch to the LINE_JOINT_BEVEL
                current_joint_mode = Line2DJointMode::LINE_JOINT_BEVEL;
            }
            pos_up1 = corner_pos_up;
            pos_down1 = corner_pos_down;
        }

        // Add current line body quad
        // Triangles are clockwise
        if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_TILE) {
            uvx1 = current_distance1 / (from._width * tile_aspect);
        } else if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_STRETCH) {
            uvx1 = current_distance1 / total_distance;
        }

        strip_add_quad(from,to,ctx,pos_up1, pos_down1, color1, uvx1);

        // Swap vars for use in the next line
        color0 = color1;
        u0 = u1;
        f0 = f1;
        pos0 = pos1;
        if (intersection_result == SEGMENT_INTERSECT) {
            if (current_joint_mode == Line2DJointMode::LINE_JOINT_SHARP) {
                pos_up0 = pos_up1;
                pos_down0 = pos_down1;
            } else {
                if (orientation == UP) {
                    pos_up0 = corner_pos_up;
                    pos_down0 = pos1 - u1 * hw * width_factor;
                } else {
                    pos_up0 = pos1 + u1 * hw * width_factor;
                    pos_down0 = corner_pos_down;
                }
            }
        } else {
            pos_up0 = pos1 + u1 * hw * width_factor;
            pos_down0 = pos1 - u1 * hw * width_factor;
        }
        // From this point, bu0 and bd0 concern the next segment

        // Add joint geometry
        if (current_joint_mode != Line2DJointMode::LINE_JOINT_SHARP) {

            /* ________________ cbegin
             *               / \
             *              /   \
             * ____________/_ _ _\ cend
             *             |     |
             *             |     |
             *             |     |
             */

            Vector2 cbegin, cend;
            if (orientation == UP) {
                cbegin = pos_down1;
                cend = pos_down0;
            } else {
                cbegin = pos_up1;
                cend = pos_up0;
            }

            if (current_joint_mode == Line2DJointMode::LINE_JOINT_BEVEL) {
                strip_add_tri(from,to,ctx,cend, orientation);
            } else if (current_joint_mode == Line2DJointMode::LINE_JOINT_ROUND) {
                Vector2 vbegin = cbegin - pos1;
                Vector2 vend = cend - pos1;
                strip_add_arc(from,to,ctx,pos1, vbegin.angle_to(vend), orientation);
            }

            if (intersection_result != SEGMENT_INTERSECT) {
                // In this case the joint is too corrputed to be re-used,
                // start again the strip with fallback points
                strip_begin(from,to,ctx,pos_up0, pos_down0, color1, uvx1);
            }
        }
    }
    // Last (or only) segment
    pos1 = from._points.back();

    if (distance_required) {
        current_distance1 += pos0.distance_to(pos1);
    }
    if (ctx._interpolate_color) {
        color1 = from._gradient->get_color(from._gradient->get_point_count() - 1);
    }
    if (retrieve_curve) {
        width_factor = from._curve->interpolate_baked(1.f);
    }

    Vector2 pos_up1 = pos1 + u0 * hw * width_factor;
    Vector2 pos_down1 = pos1 - u0 * hw * width_factor;

    // End cap (box)
    if (from._end_cap_mode == Line2DCapMode::LINE_CAP_BOX) {
        pos_up1 += f0 * hw * width_factor;
        pos_down1 += f0 * hw * width_factor;
    }

    if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_TILE) {
        uvx1 = current_distance1 / (from._width * tile_aspect);
    } else if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_STRETCH) {
        uvx1 = current_distance1 / total_distance;
    }

    strip_add_quad(from,to,ctx,pos_up1, pos_down1, color1, uvx1);

    // End cap (round)
    if (from._end_cap_mode == Line2DCapMode::LINE_CAP_ROUND) {
        // Note: color is not used in case we don't interpolate...
        Color color = ctx._interpolate_color ? from._gradient->get_color(from._gradient->get_point_count() - 1) : Color(0, 0, 0);
        float dist = 0;
        if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_TILE) {
            dist = width_factor / tile_aspect;
        } else if (from._texture_mode == Line2DTextureMode::LINE_TEXTURE_STRETCH) {
            dist = from._width * width_factor / total_distance;
        }
        new_arc(from,to,ctx,pos1, pos_up1 - pos1, Math_PI, color, Rect2(uvx1 - 0.5f * dist, 0.f, dist, 1.f));
}

    }

    }

void build_2d_line_buffers(Span<const Line2DDrawableComponent> d, Span<LineBuildOutput> target) {
    ERR_FAIL_COND(d.size()!=target.size());
    for(size_t idx=0,fin=d.size(); idx<fin; ++idx) {
        build_single(d[idx],target[idx]);
}

    }

