/*************************************************************************/
/*  cpu_particles_editor_plugin.cpp                                      */
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

#include "cpu_particles_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/translation_helpers.h"
#include "editor/editor_node.h"
#include "editor/plugins/node_3d_editor_plugin.h"
#include "editor/scene_tree_editor.h"
#include "scene/gui/menu_button.h"
#include "scene/resources/environment.h"

IMPL_GDCLASS(CPUParticles3DEditor)
IMPL_GDCLASS(CPUParticles3DEditorPlugin)

void CPUParticles3DEditor::_node_removed(Node *p_node) {

    if (p_node == node) {
        node = nullptr;
        hide();
    }
}

void CPUParticles3DEditor::_notification(int p_notification) {

    if (p_notification == NOTIFICATION_ENTER_TREE) {
        options->set_button_icon(options->get_popup()->get_theme_icon("CPUParticles3D", "EditorIcons"));
    }
}

void CPUParticles3DEditor::_menu_option(int p_option) {

    switch (p_option) {

        case MENU_OPTION_CREATE_EMISSION_VOLUME_FROM_NODE: {

            emission_tree_dialog->popup_centered_ratio();

        } break;

        case MENU_OPTION_RESTART: {

            node->restart();

        } break;
    }
}

void CPUParticles3DEditor::edit(CPUParticles3D *p_particles) {

    base_node = p_particles;
    node = p_particles;
}

void CPUParticles3DEditor::_generate_emission_points() {

    /// hacer codigo aca
    PoolVector<Vector3> points;
    PoolVector<Vector3> normals;

    if (!_generate(points, normals)) {
        return;
    }

    if (normals.size() == 0) {
        node->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_POINTS);
        node->set_emission_points(points);
    } else {
        node->set_emission_shape(CPUParticles3D::EMISSION_SHAPE_DIRECTED_POINTS);
        node->set_emission_points(points);
        node->set_emission_normals(normals);
    }
}

void CPUParticles3DEditor::_bind_methods() {
}

CPUParticles3DEditor::CPUParticles3DEditor() {

    particles_editor_hb = memnew(HBoxContainer);
    Node3DEditor::get_singleton()->add_control_to_menu_panel(particles_editor_hb);
    options = memnew(MenuButton);
    options->set_switch_on_hover(true);
    particles_editor_hb->add_child(options);
    particles_editor_hb->hide();

    options->set_text(TTR("CPUParticles3D"));
    auto *popup(options->get_popup());
    popup->add_item(TTR("Create Emission Points From Node"), MENU_OPTION_CREATE_EMISSION_VOLUME_FROM_NODE);
    popup->add_separator();
    popup->add_item(TTR("Restart"), MENU_OPTION_RESTART);
    popup->connect("id_pressed",callable_mp(this, &ClassName::_menu_option));
}

void CPUParticles3DEditorPlugin::edit(Object *p_object) {

    particles_editor->edit(object_cast<CPUParticles3D>(p_object));
}

bool CPUParticles3DEditorPlugin::handles(Object *p_object) const {

    return p_object->is_class("CPUParticles3D");
}

void CPUParticles3DEditorPlugin::make_visible(bool p_visible) {

    if (p_visible) {
        particles_editor->show();
        particles_editor->particles_editor_hb->show();
    } else {
        particles_editor->particles_editor_hb->hide();
        particles_editor->hide();
        particles_editor->edit(nullptr);
    }
}

CPUParticles3DEditorPlugin::CPUParticles3DEditorPlugin(EditorNode *p_node) {

    editor = p_node;
    particles_editor = memnew(CPUParticles3DEditor);
    editor->get_viewport()->add_child(particles_editor);

    particles_editor->hide();
}

CPUParticles3DEditorPlugin::~CPUParticles3DEditorPlugin() {
}
