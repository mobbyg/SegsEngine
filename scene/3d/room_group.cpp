/*************************************************************************/
/*  room_group.cpp                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
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

#include "room_group.h"

#include "room.h"
#include "room_manager.h"

#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "servers/rendering/rendering_server_scene.h"
#include "scene/resources/world_3d.h"

IMPL_GDCLASS(RoomGroup)

void RoomGroup::_bind_methods() {
    SE_BIND_METHOD(RoomGroup,set_roomgroup_priority);
    SE_BIND_METHOD(RoomGroup,get_roomgroup_priority);

    ADD_PROPERTY(PropertyInfo(VariantType::INT, "roomgroup_priority", PropertyHint::Range, "-16,16,1", PROPERTY_USAGE_DEFAULT), "set_roomgroup_priority", "get_roomgroup_priority");
}

RoomGroup::RoomGroup() {
    _room_group_rid = RoomAPI::roomgroup_create();
}

RoomGroup::~RoomGroup() {
    if (_room_group_rid != entt::null) {
        RenderingServer::get_singleton()->free_rid(_room_group_rid);
    }
}

String RoomGroup::get_configuration_warning() const {
    String warning = Node3D::get_configuration_warning();

    if (Room::detect_nodes_of_type<RoomManager>(this)) {
        if (!warning.empty()) {
            warning += "\n\n";
        }
        warning += TTR("The RoomManager should not be placed inside a RoomGroup.");
    }

    return warning;
}

void RoomGroup::clear() {
    _roomgroup_ID = -1;
}

void RoomGroup::add_room(Room *p_room) {
    RoomAPI::roomgroup_add_room(_room_group_rid, p_room->_room_rid);
}

// extra editor links to the room manager to allow unloading
// on change, or re-converting
void RoomGroup::_changed() {
#ifdef TOOLS_ENABLED
    RoomManager *rm = RoomManager::active_room_manager;
    if (!rm) {
        return;
    }

    rm->_rooms_changed(String("changed RoomGroup ") + get_name());
#endif
}

void RoomGroup::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_WORLD: {
            ERR_FAIL_COND(!get_world_3d());
            RoomAPI::roomgroup_set_scenario(_room_group_rid, get_world_3d()->get_scenario());
        } break;
        case NOTIFICATION_EXIT_WORLD: {
            RoomAPI::roomgroup_set_scenario(_room_group_rid, entt::null);
        } break;
    }
}