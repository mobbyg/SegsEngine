/*************************************************************************/
/*  navigation_agent_2d.cpp                                              */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2020 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2020 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "navigation_agent_2d.h"


#include "core/callable_method_pointer.h"
#include "core/engine.h"
#include "core/math/geometry.h"
#include "scene/2d/navigation_2d.h"
#include "servers/navigation_2d_server.h"
#include "core/translation_helpers.h"
#include "core/method_bind_interface.h"
#include "core/method_bind.h"

IMPL_GDCLASS(NavigationAgent2D)

void NavigationAgent2D::_bind_methods() {

    SE_BIND_METHOD(NavigationAgent2D,set_target_desired_distance);
    SE_BIND_METHOD(NavigationAgent2D,get_target_desired_distance);

    SE_BIND_METHOD(NavigationAgent2D,set_radius);
    SE_BIND_METHOD(NavigationAgent2D,get_radius);

    MethodBinder::bind_method(D_METHOD("set_navigation", {"navigation"}),&NavigationAgent2D::set_navigation_node);
    MethodBinder::bind_method(D_METHOD("get_navigation"), &NavigationAgent2D::get_navigation_node);

    SE_BIND_METHOD(NavigationAgent2D,set_neighbor_dist);
    SE_BIND_METHOD(NavigationAgent2D,get_neighbor_dist);

    SE_BIND_METHOD(NavigationAgent2D,set_max_neighbors);
    SE_BIND_METHOD(NavigationAgent2D,get_max_neighbors);

    SE_BIND_METHOD(NavigationAgent2D,set_time_horizon);
    SE_BIND_METHOD(NavigationAgent2D,get_time_horizon);

    SE_BIND_METHOD(NavigationAgent2D,set_max_speed);
    SE_BIND_METHOD(NavigationAgent2D,get_max_speed);

    SE_BIND_METHOD(NavigationAgent2D,set_path_max_distance);
    SE_BIND_METHOD(NavigationAgent2D,get_path_max_distance);

    SE_BIND_METHOD(NavigationAgent2D,set_target_location);
    SE_BIND_METHOD(NavigationAgent2D,get_target_location);
    SE_BIND_METHOD(NavigationAgent2D,get_next_location);
    SE_BIND_METHOD(NavigationAgent2D,distance_to_target);
    SE_BIND_METHOD(NavigationAgent2D,set_velocity);
    SE_BIND_METHOD(NavigationAgent2D,get_nav_path);
    SE_BIND_METHOD(NavigationAgent2D,get_nav_path_index);
    SE_BIND_METHOD(NavigationAgent2D,is_target_reached);
    SE_BIND_METHOD(NavigationAgent2D,is_target_reachable);
    SE_BIND_METHOD(NavigationAgent2D,is_navigation_finished);
    SE_BIND_METHOD(NavigationAgent2D,get_final_location);

    SE_BIND_METHOD(NavigationAgent2D,_avoidance_done);

    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "target_desired_distance", PropertyHint::Range, "0.1,100,0.01"), "set_target_desired_distance", "get_target_desired_distance");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "radius", PropertyHint::Range, "0.1,500,0.01"), "set_radius", "get_radius");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "neighbor_dist", PropertyHint::Range, "0.1,100000,0.01"), "set_neighbor_dist", "get_neighbor_dist");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "max_neighbors", PropertyHint::Range, "1,10000,1"), "set_max_neighbors", "get_max_neighbors");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "time_horizon", PropertyHint::Range, "0.1,10000,0.01"), "set_time_horizon", "get_time_horizon");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "max_speed", PropertyHint::Range, "0.1,100000,0.01"), "set_max_speed", "get_max_speed");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "path_max_distance", PropertyHint::Range, "10,100,1"), "set_path_max_distance", "get_path_max_distance");

    ADD_SIGNAL(MethodInfo("path_changed"));
    ADD_SIGNAL(MethodInfo("target_reached"));
    ADD_SIGNAL(MethodInfo("navigation_finished"));
    ADD_SIGNAL(MethodInfo("velocity_computed", PropertyInfo(VariantType::VECTOR3, "safe_velocity")));
}

void NavigationAgent2D::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {

            agent_parent = object_cast<Node2D>(get_parent());

            Navigation2DServer::get_singleton()->agent_set_callback(agent, callable_mp(this, &NavigationAgent2D::_avoidance_done));

            // Search the navigation node and set it
            {
                Navigation2D *nav = nullptr;
                Node *p = get_parent();
                while (p != nullptr) {
                    nav = object_cast<Navigation2D>(p);
                    if (nav != nullptr)
                        p = nullptr;
                    else
                        p = p->get_parent();
                }

                set_navigation(nav);
            }

            set_physics_process_internal(true);
        } break;
        case NOTIFICATION_EXIT_TREE: {
            agent_parent = nullptr;
            set_navigation(nullptr);
            set_physics_process_internal(false);
        } break;
        case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
            if (agent_parent) {

                Navigation2DServer::get_singleton()->agent_set_position(agent, agent_parent->get_global_transform().get_origin());
                if (!target_reached) {
                    if (distance_to_target() < target_desired_distance) {
                        emit_signal("target_reached");
                        target_reached = true;
                    }
                }
            }
        } break;
    }
}

NavigationAgent2D::NavigationAgent2D() :
        agent_parent(nullptr),
        navigation(nullptr),
        agent(RID()),
        target_desired_distance(1.0),
        path_max_distance(3.0),
        velocity_submitted(false),
        target_reached(false),
        navigation_finished(true) {
    agent = Navigation2DServer::get_singleton()->agent_create();
    set_neighbor_dist(500.0);
    set_max_neighbors(10);
    set_time_horizon(20.0);
    set_radius(10.0);
    set_max_speed(200.0);
}

NavigationAgent2D::~NavigationAgent2D() {
    Navigation2DServer::get_singleton()->free_rid(agent);
    agent = RID(); // Pointless
}

void NavigationAgent2D::set_navigation(Navigation2D *p_nav) {
    if (navigation == p_nav)
        return; // Pointless

    navigation = p_nav;
    Navigation2DServer::get_singleton()->agent_set_map(agent, navigation == nullptr ? RID() : navigation->get_rid());
}

void NavigationAgent2D::set_navigation_node(Node *p_nav) {
    Navigation2D *nav = object_cast<Navigation2D>(p_nav);
    ERR_FAIL_COND(nav == nullptr);
    set_navigation(nav);
}

Node *NavigationAgent2D::get_navigation_node() const {
    return object_cast<Node>(navigation);
}

void NavigationAgent2D::set_target_desired_distance(real_t p_dd) {
    target_desired_distance = p_dd;
}

void NavigationAgent2D::set_radius(real_t p_radius) {
    radius = p_radius;
    Navigation2DServer::get_singleton()->agent_set_radius(agent, radius);
}

void NavigationAgent2D::set_neighbor_dist(real_t p_dist) {
    neighbor_dist = p_dist;
    Navigation2DServer::get_singleton()->agent_set_neighbor_dist(agent, neighbor_dist);
}

void NavigationAgent2D::set_max_neighbors(int p_count) {
    max_neighbors = p_count;
    Navigation2DServer::get_singleton()->agent_set_max_neighbors(agent, max_neighbors);
}

void NavigationAgent2D::set_time_horizon(real_t p_time) {
    time_horizon = p_time;
    Navigation2DServer::get_singleton()->agent_set_time_horizon(agent, time_horizon);
}

void NavigationAgent2D::set_max_speed(real_t p_max_speed) {
    max_speed = p_max_speed;
    Navigation2DServer::get_singleton()->agent_set_max_speed(agent, max_speed);
}

void NavigationAgent2D::set_path_max_distance(real_t p_pmd) {
    path_max_distance = p_pmd;
}

real_t NavigationAgent2D::get_path_max_distance() {
    return path_max_distance;
}

void NavigationAgent2D::set_target_location(Vector2 p_location) {
    target_location = p_location;
    navigation_path.clear();
    target_reached = false;
    navigation_finished = false;
}

Vector2 NavigationAgent2D::get_target_location() const {
    return target_location;
}

Vector2 NavigationAgent2D::get_next_location() {
    update_navigation();
    if (navigation_path.size() == 0) {
        ERR_FAIL_COND_V(agent_parent == nullptr, Vector2());
        return agent_parent->get_global_transform().get_origin();
    } else {
        return navigation_path[nav_path_index];
    }
}

real_t NavigationAgent2D::distance_to_target() const {
    ERR_FAIL_COND_V(agent_parent == nullptr, 0.0);
    return agent_parent->get_global_transform().get_origin().distance_to(target_location);
}

bool NavigationAgent2D::is_target_reached() const {
    return target_reached;
}

bool NavigationAgent2D::is_target_reachable() {
    return target_desired_distance >= get_final_location().distance_to(target_location);
}

bool NavigationAgent2D::is_navigation_finished() {
    update_navigation();
    return navigation_finished;
}

Vector2 NavigationAgent2D::get_final_location() {
    update_navigation();
    if (navigation_path.size() == 0) {
        return Vector2();
    }
    return navigation_path[navigation_path.size() - 1];
}

void NavigationAgent2D::set_velocity(Vector2 p_velocity) {
    target_velocity = p_velocity;
    Navigation2DServer::get_singleton()->agent_set_target_velocity(agent, target_velocity);
    Navigation2DServer::get_singleton()->agent_set_velocity(agent, prev_safe_velocity);
    velocity_submitted = true;
}

void NavigationAgent2D::_avoidance_done(Vector3 p_new_velocity) {
    const Vector2 velocity = Vector2(p_new_velocity.x, p_new_velocity.z);
    prev_safe_velocity = velocity;

    if (!velocity_submitted) {
        target_velocity = Vector2();
        return;
    }
    velocity_submitted = false;

    emit_signal("velocity_computed", velocity);
}

String NavigationAgent2D::get_configuration_warning() const {
    if (!object_cast<Node2D>(get_parent())) {
        return TTRS("The NavigationAgent2D can be used only under a Node2D node");
    }

    return String();
}

void NavigationAgent2D::update_navigation() {

    if (agent_parent == nullptr) return;
    if (navigation == nullptr) return;
    if (update_frame_id == Engine::get_singleton()->get_physics_frames()) return;

    update_frame_id = Engine::get_singleton()->get_physics_frames();

    Vector2 o = agent_parent->get_global_transform().get_origin();

    bool reload_path = false;

    if (Navigation2DServer::get_singleton()->agent_is_map_changed(agent)) {
        reload_path = true;
    } else if (navigation_path.size() == 0) {
        reload_path = true;
    } else {
        // Check if too far from the navigation path
        if (nav_path_index > 0) {
            Vector2 segment[2];
            segment[0] = navigation_path[nav_path_index - 1];
            segment[1] = navigation_path[nav_path_index];
            Vector2 p = Geometry::get_closest_point_to_segment_2d(o, segment);
            if (o.distance_to(p) >= path_max_distance) {
                // To faraway, reload path
                reload_path = true;
            }
        }
    }

    if (reload_path) {
        navigation_path = Navigation2DServer::get_singleton()->map_get_path(navigation->get_rid(), o, target_location, true);
        navigation_finished = false;
        nav_path_index = 0;
        emit_signal("path_changed");
    }

    if (navigation_path.size() == 0)
        return;

    // Check if we can advance the navigation path
    if (navigation_finished == false) {
        // Advances to the next far away location.
        while (o.distance_to(navigation_path[nav_path_index]) < target_desired_distance) {
            nav_path_index += 1;
            if (nav_path_index == navigation_path.size()) {
                nav_path_index -= 1;
                navigation_finished = true;
                emit_signal("navigation_finished");
                break;
            }
        }
    }
}
