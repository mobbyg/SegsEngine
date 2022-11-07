/*************************************************************************/
/*  circle_shape_2d.cpp                                                  */
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

#include "circle_shape_2d.h"

#include "servers/physics_server_2d.h"
#include "servers/rendering_server.h"
#include "core/method_bind.h"

IMPL_GDCLASS(CircleShape2D)

#ifdef TOOLS_ENABLED
bool CircleShape2D::_edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const {

    return p_point.length() < get_radius() + p_tolerance;
}
#endif

void CircleShape2D::_update_shape() {

    PhysicsServer2D::get_singleton()->shape_set_data(get_phys_rid(), radius);
    emit_changed();
}

void CircleShape2D::set_radius(real_t p_radius) {

    radius = p_radius;
    _update_shape();
}

real_t CircleShape2D::get_radius() const {

    return radius;
}

void CircleShape2D::_bind_methods() {

    SE_BIND_METHOD(CircleShape2D,set_radius);
    SE_BIND_METHOD(CircleShape2D,get_radius);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.01,1024,0.01,or_greater"), "set_radius", "get_radius");
}

Rect2 CircleShape2D::get_rect() const {
    Rect2 rect;
    rect.position = -Point2(get_radius(), get_radius());
    rect.size = Point2(get_radius(), get_radius()) * 2.0;
    return rect;
}

void CircleShape2D::draw(RenderingEntity p_to_rid, const Color &p_color) {

    Vector2 points[24];
    for (int i = 0; i < 24; i++) {
        points[i]= Vector2(Math::cos(i * Math_PI * 2 / 24.0f), Math::sin(i * Math_PI * 2 / 24.0f)) * get_radius();
    }

    Color col[1]={p_color};
    RenderingServer::get_singleton()->canvas_item_add_polygon(p_to_rid, points, col);
    if (is_collision_outline_enabled()) {
        RenderingServer::get_singleton()->canvas_item_add_polyline(p_to_rid, points, col, 1.0, true);
        // Draw the last segment as it's not drawn by `canvas_item_add_polyline()`.
        RenderingServer::get_singleton()->canvas_item_add_line(p_to_rid, Span<Vector2>(points).back(), points[0], p_color, 1.0, true);
    }
}

CircleShape2D::CircleShape2D() :
        Shape2D(PhysicsServer2D::get_singleton()->circle_shape_create()) {

    radius = 10;
    _update_shape();
}
