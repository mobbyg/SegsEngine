/*************************************************************************/
/*  spring_arm_3d.cpp                                                    */
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

#include "spring_arm_3d.h"
#include "core/engine.h"
#include "scene/3d/collision_object_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/resources/sphere_shape_3d.h"
#include "servers/physics_server_3d.h"
#include "core/method_bind.h"

IMPL_GDCLASS(SpringArm3D)

SpringArm3D::SpringArm3D() = default;

void SpringArm3D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE:
            if (!Engine::get_singleton()->is_editor_hint()) {
                set_physics_process_internal(true);
            }
            break;
        case NOTIFICATION_EXIT_TREE:
            if (!Engine::get_singleton()->is_editor_hint()) {
                set_physics_process_internal(false);
            }
            break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS:
            process_spring();
            break;
    }
}

void SpringArm3D::_bind_methods() {

    SE_BIND_METHOD(SpringArm3D,get_hit_length);

    SE_BIND_METHOD(SpringArm3D,set_length);
    SE_BIND_METHOD(SpringArm3D,get_length);

    SE_BIND_METHOD(SpringArm3D,set_shape);
    SE_BIND_METHOD(SpringArm3D,get_shape);

    SE_BIND_METHOD(SpringArm3D,add_excluded_object);
    SE_BIND_METHOD(SpringArm3D,remove_excluded_object);
    SE_BIND_METHOD(SpringArm3D,clear_excluded_objects);

    SE_BIND_METHOD(SpringArm3D,set_collision_mask);
    SE_BIND_METHOD(SpringArm3D,get_collision_mask);

    SE_BIND_METHOD(SpringArm3D,set_margin);
    SE_BIND_METHOD(SpringArm3D,get_margin);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "collision_mask", PropertyHint::Layers3DPhysics), "set_collision_mask", "get_collision_mask");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "shape", PropertyHint::ResourceType, "Shape"), "set_shape", "get_shape");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "spring_length"), "set_length", "get_length");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "margin"), "set_margin", "get_margin");
}

float SpringArm3D::get_length() const {
    return spring_length;
}

void SpringArm3D::set_length(float p_length) {
    if (is_inside_tree() && (Engine::get_singleton()->is_editor_hint() || get_tree()->is_debugging_collisions_hint()))
        update_gizmo();

    spring_length = p_length;
}

void SpringArm3D::set_shape(const Ref<Shape>& p_shape) {
    shape = p_shape;
}

Ref<Shape> SpringArm3D::get_shape() const {
    return shape;
}

void SpringArm3D::set_collision_mask(uint32_t p_mask) {
    mask = p_mask;
}

void SpringArm3D::set_margin(float p_margin) {
    margin = p_margin;
}

void SpringArm3D::add_excluded_object(RID p_rid) {
    excluded_objects.insert(p_rid);
}

bool SpringArm3D::remove_excluded_object(RID p_rid) {
    return excluded_objects.erase(p_rid);
}

void SpringArm3D::clear_excluded_objects() {
    excluded_objects.clear();
}

void SpringArm3D::process_spring() {
    // From
    real_t motion_delta(1);
    real_t motion_delta_unsafe(1);

    Vector3 motion;
    const Vector3 cast_direction(get_global_transform().basis.xform(Vector3(0, 0, 1)));

    if (not shape) {
        motion = Vector3(cast_direction * (spring_length));
        PhysicsDirectSpaceState3D::RayResult r;
        bool intersected = get_world_3d()->get_direct_space_state()->intersect_ray(get_global_transform().origin, get_global_transform().origin + motion, r, excluded_objects, mask);
        if (intersected) {
            float dist = get_global_transform().origin.distance_to(r.position);
            dist -= margin;
            motion_delta = dist / (spring_length);
        }
    } else {
        motion = Vector3(cast_direction * spring_length);
        get_world_3d()->get_direct_space_state()->cast_motion(shape->get_phys_rid(), get_global_transform(), motion, 0, motion_delta, motion_delta_unsafe, excluded_objects, mask);
    }

    current_spring_length = spring_length * motion_delta;
    Transform childs_transform;
    childs_transform.origin = get_global_transform().origin + cast_direction * (spring_length * motion_delta);

    for (int i = get_child_count() - 1; 0 <= i; --i) {

        Node3D *child = object_cast<Node3D>(get_child(i));
        if (child) {
            childs_transform.basis = child->get_global_transform().basis;
            child->set_global_transform(childs_transform);
        }
    }
}
