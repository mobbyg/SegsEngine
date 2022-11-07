/*************************************************************************/
/*  capsule_shape_2d.h                                                   */
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

#pragma once

#include "scene/resources/shape_2d.h"

class GODOT_EXPORT CapsuleShape2D : public Shape2D {
    GDCLASS(CapsuleShape2D,Shape2D)

    real_t height;
    real_t radius;

    void _update_shape();
    Vector<Vector2> _get_points() const;

protected:
    static void _bind_methods();

public:
#ifdef TOOLS_ENABLED
    bool _edit_is_selected_on_click(const Point2 &p_point, float p_tolerance) const override;
#endif

    void set_height(real_t p_height);
    real_t get_height() const;

    void set_radius(real_t p_radius);
    real_t get_radius() const { return radius; }

    real_t get_enclosing_radius() const override {
        return radius + height * 0.5f;
    }
    void draw(RenderingEntity p_to_rid, const Color &p_color) override;
    Rect2 get_rect() const override;

    CapsuleShape2D();
};

