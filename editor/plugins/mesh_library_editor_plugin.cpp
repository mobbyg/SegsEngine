/*************************************************************************/
/*  mesh_library_editor_plugin.cpp                                       */
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

#include "mesh_library_editor_plugin.h"

#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/resource/resource_manager.h"
#include "core/string_formatter.h"
#include "core/translation_helpers.h"
#include "editor/editor_file_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "main/main_class.h"
#include "node_3d_editor_plugin.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation_mesh_instance.h"
#include "scene/3d/physics_body_3d.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"
#include "scene/main/viewport.h"
#include "scene/resources/packed_scene.h"

IMPL_GDCLASS(MeshLibraryEditor)
IMPL_GDCLASS(MeshLibraryEditorPlugin)

void MeshLibraryEditor::edit(const Ref<MeshLibrary> &p_mesh_library) {
    mesh_library = p_mesh_library;
    if (mesh_library) {
        menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE),
                !mesh_library->has_meta("_editor_source_scene"));
    }
}

void MeshLibraryEditor::_menu_remove_confirm() {
    switch (option) {
        case MENU_OPTION_REMOVE_ITEM: {
            mesh_library->remove_item(to_erase);
        } break;
        default: {
        };
    }
}

void MeshLibraryEditor::_menu_update_confirm(bool p_apply_xforms) {
    cd_update->hide();
    apply_xforms = p_apply_xforms;
    String existing = mesh_library->get_meta("_editor_source_scene").as<String>();
    ERR_FAIL_COND(existing == "");
    _import_scene_cbk(existing);
}

void MeshLibraryEditor::_import_scene(Node *p_scene, const Ref<MeshLibrary> &p_library, bool p_merge, bool p_apply_xforms) {
    if (!p_merge) {
        p_library->clear();
    }

    Map<int, MeshInstance3D *> mesh_instances;

    for (int i = 0; i < p_scene->get_child_count(); i++) {
        Node *child = p_scene->get_child(i);

        if (!object_cast<MeshInstance3D>(child)) {
            if (child->get_child_count() > 0) {
                child = child->get_child(0);
                if (!object_cast<MeshInstance3D>(child)) {
                    continue;
                }

            } else {
                continue;
            }
        }

        MeshInstance3D *mi = object_cast<MeshInstance3D>(child);
        Ref<Mesh> mesh = mi->get_mesh();
        if (not mesh) {
            continue;
        }

        mesh = dynamic_ref_cast<Mesh>(mesh->duplicate());
        for (int j = 0; j < mesh->get_surface_count(); ++j) {
            Ref<Material> mat = mi->get_surface_material(j);

            if (mat) {
                mesh->surface_set_material(j, mat);
            }
        }

        int id = p_library->find_item_by_name(mi->get_name());
        if (id < 0) {
            id = p_library->get_last_unused_item_id();
            p_library->create_item(id);
            p_library->set_item_name(id, mi->get_name());
        }

        p_library->set_item_mesh(id, mesh);

        if (p_apply_xforms) {
            p_library->set_item_mesh_transform(id, mi->get_transform());
        } else {
            p_library->set_item_mesh_transform(id, Transform());
        }

        mesh_instances[id] = mi;

        PoolVector<MeshLibrary::ShapeData> collisions;

        for (int j = 0; j < mi->get_child_count(); j++) {
            Node *child2 = mi->get_child(j);
            if (!object_cast<StaticBody3D>(child2)) {
                continue;
            }

            StaticBody3D *sb = object_cast<StaticBody3D>(child2);
            Vector<uint32_t> shapes;
            sb->get_shape_owners(&shapes);

            for (uint32_t E : shapes) {
                if (sb->is_shape_owner_disabled(E)) {
                    continue;
                }

                Transform shape_transform;
                if (p_apply_xforms) {
                    shape_transform = mi->get_transform();
                }
                shape_transform *= sb->get_transform() * sb->shape_owner_get_transform(E);

                for (int k = 0; k < sb->shape_owner_get_shape_count(E); k++) {
                    Ref<Shape> collision = sb->shape_owner_get_shape(E, k);
                    if (not collision) {
                        continue;
                    }
                    MeshLibrary::ShapeData shape_data;
                    shape_data.shape = collision;
                    shape_data.local_transform = shape_transform;
                    collisions.push_back(shape_data);
                }
            }
        }

        p_library->set_item_shapes(id, collisions);

        Ref<NavigationMesh> navmesh;
        Transform navmesh_transform;
        for (int j = 0; j < mi->get_child_count(); j++) {
            Node *child2 = mi->get_child(j);
            if (!object_cast<NavigationMeshInstance>(child2)) {
                continue;
            }
            NavigationMeshInstance *sb = object_cast<NavigationMeshInstance>(child2);
            navmesh = sb->get_navigation_mesh();
            navmesh_transform = sb->get_transform();
            if (navmesh) {
                break;
            }
        }
        if (navmesh) {
            p_library->set_item_navmesh(id, navmesh);
            p_library->set_item_navmesh_transform(id, navmesh_transform);
        }
    }

    // generate previews!

    if (true) {
        Vector<Ref<Mesh>> meshes;
        Vector<Transform> transforms;
        Vector<int> ids = p_library->get_item_list();
        for (int i = 0; i < ids.size(); i++) {
            if (mesh_instances.contains(ids[i])) {
                meshes.emplace_back(p_library->get_item_mesh(ids[i]));
                transforms.emplace_back(mesh_instances[ids[i]]->get_transform());
            }
        }

        Vector<Ref<Texture>> textures = EditorInterface::get_singleton()->make_mesh_previews(
                meshes, &transforms, EditorSettings::get_singleton()->getT<int>("editors/grid_map/preview_size"));
        int j = 0;
        for (int i = 0; i < ids.size(); i++) {
            if (mesh_instances.contains(ids[i])) {
                p_library->set_item_preview(ids[i], textures[j]);
                j++;
            }
        }
    }
}

void MeshLibraryEditor::_import_scene_cbk(StringView p_str) {
    Ref<PackedScene> ps = dynamic_ref_cast<PackedScene>(gResourceManager().load(p_str, "PackedScene"));
    ERR_FAIL_COND(not ps);
    Node *scene = ps->instance();

    ERR_FAIL_COND_MSG(!scene, "Cannot create an instance from PackedScene '" + String(p_str) + "'.");

    _import_scene(scene, mesh_library, option == MENU_OPTION_UPDATE_FROM_SCENE, apply_xforms);

    memdelete(scene);
    mesh_library->set_meta("_editor_source_scene", p_str);
    menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE), false);
}

Error MeshLibraryEditor::update_library_file(Node *p_base_scene, const Ref<MeshLibrary> &ml, bool p_merge, bool p_apply_xforms) {
    _import_scene(p_base_scene, ml, p_merge, p_apply_xforms);
    return OK;
}

void MeshLibraryEditor::_menu_cbk(int p_option) {
    option = p_option;
    switch (p_option) {
        case MENU_OPTION_ADD_ITEM: {
            mesh_library->create_item(mesh_library->get_last_unused_item_id());
        } break;
        case MENU_OPTION_REMOVE_ITEM: {
            String p(editor->get_inspector()->get_selected_path());
            if (StringUtils::begins_with(p, "/MeshLibrary/item") && StringUtils::get_slice_count(p, '/') >= 3) {
                to_erase = StringUtils::to_int(StringUtils::get_slice(p, '/', 3));
                cd_remove->set_text(FormatVE(TTR("Remove item %d?").asCString(), to_erase));
                cd_remove->popup_centered(Size2(300, 60));
            }
        } break;
    case MENU_OPTION_IMPORT_FROM_SCENE: {
        apply_xforms = false;
        file->popup_centered_ratio();
    } break;
    case MENU_OPTION_IMPORT_FROM_SCENE_APPLY_XFORMS: {
        apply_xforms = true;
        file->popup_centered_ratio();
    } break;
        case MENU_OPTION_UPDATE_FROM_SCENE: {
        cd_update->set_text(FormatVE(TTR("Update from existing scene?:\n%s").asCString(), mesh_library->get_meta("_editor_source_scene").as<String>().c_str()));
        cd_update->popup_centered(Size2(500, 60));
        } break;
    }
}

MeshLibraryEditor::MeshLibraryEditor(EditorNode *p_editor) {
    file = memnew(EditorFileDialog);
    file->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    // not for now?
    Vector<String> extensions;
    gResourceManager().get_recognized_extensions_for_type("PackedScene", extensions);
    file->clear_filters();
    file->set_title(TTR("Import Scene"));
    for (const String &ext : extensions) {
        file->add_filter("*." + ext + " ; " + StringUtils::to_upper(ext));
    }
    add_child(file);
    file->connect("file_selected", callable_mp(this, &ClassName::_import_scene_cbk));

    menu = memnew(MenuButton);
    Node3DEditor::get_singleton()->add_control_to_menu_panel(menu);
    menu->set_position(Point2(1, 1));
    menu->set_text(TTR("Mesh Library"));
    menu->set_button_icon(EditorNode::get_singleton()->get_gui_base()->get_theme_icon("MeshLibrary", "EditorIcons"));
    menu->get_popup()->add_item(TTR("Add Item"), MENU_OPTION_ADD_ITEM);
    menu->get_popup()->add_item(TTR("Remove Selected Item"), MENU_OPTION_REMOVE_ITEM);
    menu->get_popup()->add_separator();
    menu->get_popup()->add_item(TTR("Import from Scene (Ignore Transforms)"), MENU_OPTION_IMPORT_FROM_SCENE);
    menu->get_popup()->add_item(TTR("Import from Scene (Apply Transforms)"), MENU_OPTION_IMPORT_FROM_SCENE_APPLY_XFORMS);
    menu->get_popup()->add_item(TTR("Update from Scene"), MENU_OPTION_UPDATE_FROM_SCENE);
    menu->get_popup()->set_item_disabled(menu->get_popup()->get_item_index(MENU_OPTION_UPDATE_FROM_SCENE), true);
    menu->get_popup()->connect("id_pressed", callable_mp(this, &ClassName::_menu_cbk));
    menu->hide();

    editor = p_editor;
    cd_remove = memnew(ConfirmationDialog);
    add_child(cd_remove);
    cd_remove->get_ok()->connect("pressed", callable_mp(this, &ClassName::_menu_remove_confirm));
    cd_update = memnew(ConfirmationDialog);
    add_child(cd_update);
    cd_update->get_ok()->set_text(TTR("Apply without Transforms"));
    cd_update->get_ok()->connect("pressed", callable_gen(this,[this]() { _menu_update_confirm(false); }));
    cd_update->add_button(TTR("Apply with Transforms"))->connect("pressed", callable_gen(this,[this]() { _menu_update_confirm(true); }));
}

void MeshLibraryEditorPlugin::edit(Object *p_node) {
    if (object_cast<MeshLibrary>(p_node)) {
        mesh_library_editor->edit(Ref<MeshLibrary>(object_cast<MeshLibrary>(p_node)));
        mesh_library_editor->show();
    } else {
        mesh_library_editor->hide();
    }
}

bool MeshLibraryEditorPlugin::handles(Object *p_node) const {
    return p_node->is_class("MeshLibrary");
}

void MeshLibraryEditorPlugin::make_visible(bool p_visible) {
    if (p_visible) {
        mesh_library_editor->show();
        mesh_library_editor->get_menu_button()->show();
    } else {
        mesh_library_editor->hide();
        mesh_library_editor->get_menu_button()->hide();
    }
}

MeshLibraryEditorPlugin::MeshLibraryEditorPlugin(EditorNode *p_node) {
    EDITOR_DEF("editors/grid_map/preview_size", 64);
    mesh_library_editor = memnew(MeshLibraryEditor(p_node));

    p_node->get_viewport()->add_child(mesh_library_editor);
    mesh_library_editor->set_anchors_and_margins_preset(Control::PRESET_TOP_WIDE);
    mesh_library_editor->set_end(Point2(0, 22));
    mesh_library_editor->hide();
}
