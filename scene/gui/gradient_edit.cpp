/*************************************************************************/
/*  gradient_edit.cpp                                                    */
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

#include "gradient_edit.h"

#include "core/callable_method_pointer.h"
#include "core/os/keyboard.h"
#include "core/method_bind.h"
#include "core/pool_vector.h"
#include "EASTL/sort.h"
#include "scene/resources/texture.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#define SPACING (3 * EDSCALE)
#define POINT_WIDTH (8 * EDSCALE)
#else
#define SPACING 3
#define POINT_WIDTH 8
#endif

IMPL_GDCLASS(GradientEdit)

GradientEdit::GradientEdit() {
    grabbed = -1;
    grabbing = false;
    set_focus_mode(FOCUS_ALL);

    popup = memnew(PopupPanel);
    picker = memnew(ColorPicker);
    popup->add_child(picker);

    add_child(popup);

    checker = dynamic_ref_cast<ImageTexture>(Control::get_theme_icon("bg", "GradientEdit"));
}

int GradientEdit::_get_point_from_pos(int x) {
    int result = -1;
    int total_w = get_size().width - get_size().height - SPACING;
    float min_distance = 1e20f;
    for (int i = 0; i < points.size(); i++) {
        //Check if we clicked at point
        float distance = ABS(x - points[i].offset * total_w);
        float min = (POINT_WIDTH / 2 * 1.7f); //make it easier to grab
        if (distance <= min && distance < min_distance) {
            result = i;
            min_distance = distance;
        }
    }
    return result;
}

void GradientEdit::_show_color_picker() {
    if (grabbed == -1)
        return;
    picker->set_pick_color(points[grabbed].color);
    Size2 minsize = popup->get_combined_minimum_size();
    bool show_above = false;
    if (get_global_position().y + get_size().y + minsize.y > get_viewport_rect().size.y) {
        show_above = true;
    }
    if (show_above) {
        popup->set_position(get_global_position() - Vector2(0, minsize.y));
    } else {
        popup->set_position(get_global_position() + Vector2(0, get_size().y));
    }
    popup->popup();
}

GradientEdit::~GradientEdit() {
}

void GradientEdit::_gui_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventKey> k = dynamic_ref_cast<InputEventKey>(p_event);

    if (k && k->is_pressed() && k->get_keycode() == KEY_DELETE && grabbed != -1) {

        points.erase_at(grabbed);
        grabbed = -1;
        grabbing = false;
        update();
        emit_signal("ramp_changed");
        accept_event();
    }

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);
    //Show color picker on double click.
    if (mb && mb->get_button_index() == 1 && mb->is_doubleclick() && mb->is_pressed()) {
        grabbed = _get_point_from_pos(mb->get_position().x);
        _show_color_picker();
        accept_event();
    }

    //Delete point on right click
    if (mb && mb->get_button_index() == 2 && mb->is_pressed()) {
        grabbed = _get_point_from_pos(mb->get_position().x);
        if (grabbed != -1) {
            points.erase_at(grabbed);
            grabbed = -1;
            grabbing = false;
            update();
            emit_signal("ramp_changed");
            accept_event();
        }
    }

    //Hold alt key to duplicate selected color
    if (mb && mb->get_button_index() == 1 && mb->is_pressed() && mb->get_alt()) {

        int x = mb->get_position().x;
        grabbed = _get_point_from_pos(x);

        if (grabbed != -1) {
            int total_w = get_size().width - get_size().height - SPACING;
            Gradient::Point newPoint = points[grabbed];
            newPoint.offset = CLAMP(x / float(total_w), 0.0f, 1.0f);

            points.push_back(newPoint);
            eastl::sort(points.begin(),points.end());
            for (int i = 0; i < points.size(); ++i) {
                if (points[i].offset == newPoint.offset) {
                    grabbed = i;
                    break;
                }
            }

            emit_signal("ramp_changed");
            update();
        }
    }

    //select
    if (mb && mb->get_button_index() == 1 && mb->is_pressed()) {

        update();
        int x = mb->get_position().x;
        int total_w = get_size().width - get_size().height - SPACING;

        //Check if color selector was clicked.
        if (x > total_w + SPACING) {
            _show_color_picker();
            return;
        }

        grabbing = true;

        grabbed = _get_point_from_pos(x);
        //grab or select
        if (grabbed != -1) {
            return;
        }

        //insert
        Gradient::Point newPoint;
        newPoint.offset = CLAMP(x / float(total_w), 0.0f, 1.0f);

        Gradient::Point prev;
        Gradient::Point next;

        int pos = -1;
        for (int i = 0; i < points.size(); i++) {
            if (points[i].offset < newPoint.offset)
                pos = i;
        }

        if (pos == -1) {

            prev.color = Color(0, 0, 0);
            prev.offset = 0;
            if (!points.empty()) {
                next = points[0];
            } else {
                next.color = Color(1, 1, 1);
                next.offset = 1.0;
            }
        } else {

            if (pos == points.size() - 1) {
                next.color = Color(1, 1, 1);
                next.offset = 1.0;
            } else {
                next = points[pos + 1];
            }
            prev = points[pos];
        }

        newPoint.color = prev.color.linear_interpolate(next.color, (newPoint.offset - prev.offset) / (next.offset - prev.offset));

        points.push_back(newPoint);
        eastl::sort(points.begin(),points.end());

        for (int i = 0; i < points.size(); i++) {
            if (points[i].offset == newPoint.offset) {
                grabbed = i;
                break;
            }
        }

        emit_signal("ramp_changed");
    }

    if (mb && mb->get_button_index() == 1 && !mb->is_pressed()) {

        if (grabbing) {
            grabbing = false;
            emit_signal("ramp_changed");
        }
        update();
    }

    Ref<InputEventMouseMotion> mm = dynamic_ref_cast<InputEventMouseMotion>(p_event);

    if (mm && grabbing) {

        int total_w = get_size().width - get_size().height - SPACING;

        int x = mm->get_position().x;

        float newofs = CLAMP(x / float(total_w), 0.0f, 1.0f);

        // Snap to "round" coordinates if holding Ctrl.
        // Be more precise if holding Shift as well
        if (mm->get_control()) {
            newofs = Math::stepify(newofs, mm->get_shift() ? 0.025f : 0.1f);
        } else if (mm->get_shift()) {
            // Snap to nearest point if holding just Shift
            const float snap_threshold = 0.03f;
            float smallest_ofs = snap_threshold;
            bool found = false;
            int nearest_point = 0;
            for (int i = 0; i < points.size(); ++i) {
                if (i != grabbed) {
                    float temp_ofs = ABS(points[i].offset - newofs);
                    if (temp_ofs < smallest_ofs) {
                        smallest_ofs = temp_ofs;
                        nearest_point = i;
                        if (found)
                            break;
                        found = true;
                    }
                }
            }
            if (found) {
                if (points[nearest_point].offset < newofs)
                    newofs = points[nearest_point].offset + 0.00001f;
                else
                    newofs = points[nearest_point].offset - 0.00001f;
                newofs = CLAMP(newofs, 0.0f, 1.0f);
            }
        }

        bool valid = true;
        for (int i = 0; i < points.size(); i++) {

            if (points[i].offset == newofs && i != grabbed) {
                valid = false;
                break;
            }
        }

        if (!valid || grabbed == -1) {
            return;
        }

        points[grabbed].offset = newofs;

        eastl::sort(points.begin(),points.end());

        for (int i = 0; i < points.size(); i++) {
            if (points[i].offset == newofs) {
                grabbed = i;
                break;
            }
        }

        emit_signal("ramp_changed");

        update();
    }
}

void GradientEdit::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {
        if (!picker->is_connected("color_changed",callable_mp(this, &ClassName::_color_changed))) {
            picker->connect("color_changed",callable_mp(this, &ClassName::_color_changed));
        }
    }
    if (p_what == NOTIFICATION_DRAW) {

        int w = get_size().x;
        int h = get_size().y;

        if (w == 0 || h == 0)
            return; //Safety check. We have division by 'h'. And in any case there is nothing to draw with such size

        int total_w = get_size().width - get_size().height - SPACING;

        //Draw checker pattern for ramp
        _draw_checker(0, 0, total_w, h);

        //Draw color ramp
        Gradient::Point prev;
        prev.offset = 0;
        if (points.empty())
            prev.color = Color(0, 0, 0); //Draw black rectangle if we have no points
        else
            prev.color = points[0].color; //Extend color of first point to the beginning.

        for (int i = -1; i < points.size(); i++) {

            Gradient::Point next;
            //If there is no next point
            if (i + 1 == points.size()) {
                if (points.empty())
                    next.color = Color(0, 0, 0); //Draw black rectangle if we have no points
                else
                    next.color = points[i].color; //Extend color of last point to the end.
                next.offset = 1;
            } else {
                next = points[i + 1];
            }

            if (prev.offset == next.offset) {
                prev = next;
                continue;
            }

            const Vector2 points[4]= {
                Vector2(prev.offset * total_w, h),
                Vector2(prev.offset * total_w, 0),
                Vector2(next.offset * total_w, 0),
                Vector2(next.offset * total_w, h),
            };
            const Color colors[4] = {
                prev.color,
                prev.color,
                next.color,
                next.color
            };
            draw_primitive(points, colors, PoolVector<Point2>());
            prev = next;
        }

        //Draw point markers
        for (int i = 0; i < points.size(); i++) {

            Color col = points[i].color.contrasted();
            col.a = 0.9f;

            draw_line(Vector2(points[i].offset * total_w, 0), Vector2(points[i].offset * total_w, h / 2), col);
            Rect2 rect = Rect2(points[i].offset * total_w - POINT_WIDTH / 2, h / 2, POINT_WIDTH, h / 2);
            draw_rect_filled(rect, points[i].color);
            draw_rect_stroke(rect, col);
            if (grabbed == i) {
                rect.grow_by(-1);
                if (has_focus()) {
                    draw_rect_stroke(rect, Color(1, 0, 0, 0.9f));
                } else {
                    draw_rect_stroke(rect, Color(0.6f, 0, 0, 0.9f));
                }

                rect.grow_by(-1);
                draw_rect_stroke(rect, col);
            }
        }

        //Draw "button" for color selector
        _draw_checker(total_w + SPACING, 0, h, h);
        if (grabbed != -1) {
            //Draw with selection color
            draw_rect_filled(Rect2(total_w + SPACING, 0, h, h), points[grabbed].color);
        } else {
            //if no color selected draw grey color with 'X' on top.
            draw_rect_filled(Rect2(total_w + SPACING, 0, h, h), Color(0.5, 0.5, 0.5, 1));
            draw_line(Vector2(total_w + SPACING, 0), Vector2(total_w + SPACING + h, h), Color(1, 1, 1, 0.6f));
            draw_line(Vector2(total_w + SPACING, h), Vector2(total_w + SPACING + h, 0), Color(1, 1, 1, 0.6f));
        }

        //Draw borders around color ramp if in focus
        if (has_focus()) {
            Color ramp_color(1,1,1,0.6f);
            draw_line(Vector2(-1, -1), Vector2(total_w + 1, -1), ramp_color);
            draw_line(Vector2(total_w + 1, -1), Vector2(total_w + 1, h + 1), ramp_color);
            draw_line(Vector2(total_w + 1, h + 1), Vector2(-1, h + 1), ramp_color);
            draw_line(Vector2(-1, -1), Vector2(-1, h + 1), ramp_color);
        }
    }

    if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {

        if (!is_visible()) {
            grabbing = false;
        }
    }
}

void GradientEdit::_draw_checker(int x, int y, int w, int h) {
    //Draw it with polygon to insert UVs for scale
    const Vector2 backPoints[4] {
        Vector2(x, y),
        Vector2(x, y + h),
        Vector2(x + w, y + h),
        Vector2(x + w, y)
    };
    constexpr Color colorPoints[4]= {
        Color(1, 1, 1, 1),
        Color(1, 1, 1, 1),
        Color(1, 1, 1, 1),
        Color(1, 1, 1, 1)
    };
    //Draw checker pattern pixel-perfect and scale it by 2.
    const Vector2 uvPoints[4] = {
        Vector2(x, y),
        Vector2(x, y + h * .5f / checker->get_height()),
        Vector2(x + w * .5f / checker->get_width(), y + h * .5f / checker->get_height()),
        Vector2(x + w * .5f / checker->get_width(), y),
    };
    draw_textured_polygon(backPoints, colorPoints, uvPoints, checker, Ref<Texture>(), false);
}

Size2 GradientEdit::get_minimum_size() const {

    return Vector2(0, 16);
}

void GradientEdit::_color_changed(const Color &p_color) {

    if (grabbed == -1)
        return;
    points[grabbed].color = p_color;
    update();
    emit_signal("ramp_changed");
}

void GradientEdit::set_ramp(Span<const float> p_offsets, const Vector<Color> &p_colors) {

    ERR_FAIL_COND(p_offsets.size() != p_colors.size());
    points.clear();
    points.reserve(p_offsets.size());
    for (int i = 0; i < p_offsets.size(); i++) {
        Gradient::Point p;
        p.offset = p_offsets[i];
        p.color = p_colors[i];
        points.emplace_back(p);
    }

    eastl::sort(points.begin(),points.end());
    update();
}

Vector<float> GradientEdit::get_offsets() const {
    Vector<float> ret;
    ret.reserve(points.size());
    for (int i = 0; i < points.size(); i++)
        ret.push_back(points[i].offset);
    return ret;
}

Vector<Color> GradientEdit::get_colors() const {
    Vector<Color> ret;
    ret.reserve(points.size());
    for (int i = 0; i < points.size(); i++)
        ret.push_back(points[i].color);
    return ret;
}

void GradientEdit::set_points(const Vector<Gradient::Point> &p_points) {
    if (points.size() != p_points.size())
        grabbed = -1;
    points.clear();
    points = p_points;
}

Vector<Gradient::Point> &GradientEdit::get_points() {
    return points;
}

void GradientEdit::_bind_methods() {
    SE_BIND_METHOD(GradientEdit,_gui_input);
    ADD_SIGNAL(MethodInfo("ramp_changed"));
}
