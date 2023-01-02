/*************************************************************************/
/*  editor_resource_picker.cpp                                           */
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

#include "editor_resource_picker.h"

#include "editor/editor_resource_preview.h"

#include "core/callable_method_pointer.h"
#include "core/class_db.h"
#include "core/method_bind_interface.h"
#include "core/method_bind.h"
#include "core/object_tooling.h"
#include "core/resource/resource_manager.h"
#include "core/string_formatter.h"
#include "core/script_language.h"
#include "editor/plugins/script_editor_plugin.h"
#include "editor/property_editor.h"
#include "editor/scene_tree_dock.h"
#include "editor_node.h"
#include "editor_scale.h"
#include "editor_settings.h"
#include "filesystem_dock.h"
#include "scene/main/viewport.h"
#include "scene/resources/dynamic_font.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"
#include "scene/resources/style_box.h"
#include "EASTL/sort.h"


HashMap<StringName, List<StringName>> EditorResourcePicker::allowed_types_cache;
IMPL_GDCLASS(EditorResourcePicker)
IMPL_GDCLASS(EditorScriptPicker)

void EditorResourcePicker::clear_caches() {
    allowed_types_cache.clear();
}

void EditorResourcePicker::_update_resource() {
    preview_rect->set_texture(Ref<Texture>());
    assign_button->set_custom_minimum_size(Size2(1, 1));

    if (edited_resource == RES()) {
        assign_button->set_button_icon(Ref<Texture>());
        assign_button->set_text(TTR("[empty]"));
    } else {
        assign_button->set_button_icon(
                EditorNode::get_singleton()->get_object_icon(edited_resource.operator->(), "Object"));

        if (edited_resource->get_name() != String()) {
            assign_button->set_text(edited_resource->get_name());
        } else if (PathUtils::is_resource_file(edited_resource->get_path())) {
            assign_button->set_text(PathUtils::get_file(edited_resource->get_path()));
            assign_button->set_tooltip(edited_resource->get_path());
        } else {
            assign_button->set_text(edited_resource->get_class());
        }

        if (PathUtils::is_resource_file(edited_resource->get_path())) {
            assign_button->set_tooltip(edited_resource->get_path());
        }
        auto lmbd = [=](const String &, const Ref<Texture> &p_preview, const Ref<Texture> &) {
            _update_resource_preview(p_preview, edited_resource->get_instance_id());
        };
        // Preview will override the above, so called at the end.
        EditorResourcePreview::get_singleton()->queue_edited_resource_preview(
                edited_resource, callable_gen(this, lmbd));
    }
}

void EditorResourcePicker::_update_resource_preview(const Ref<Texture> &p_preview, GameEntity p_obj) {
    if (!edited_resource || edited_resource->get_instance_id() != p_obj) {
        return;
    }

    StringName type = edited_resource->get_class_name();
    if (ClassDB::class_exists(type) && ClassDB::is_parent_class(type, "Script")) {
        assign_button->set_text(PathUtils::get_file(edited_resource->get_path()));
        return;
    }

    if (p_preview) {
        preview_rect->set_margin(
                Margin::Left, assign_button->get_button_icon()->get_width() +
                                      assign_button->get_theme_stylebox("normal")->get_default_margin(Margin::Left) +
                                      get_theme_constant("hseparation", "Button"));

        if (type == "GradientTexture") {
            preview_rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
            assign_button->set_custom_minimum_size(Size2(1, 1));
        } else {
            preview_rect->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
            int thumbnail_size = EditorSettings::get_singleton()->getT<int>("filesystem/file_dialog/thumbnail_size");
            thumbnail_size *= EDSCALE;
            assign_button->set_custom_minimum_size(Size2(1, thumbnail_size));
        }

        preview_rect->set_texture(p_preview);
        assign_button->set_text("");
    }
}

void EditorResourcePicker::_resource_selected() {
    if (!edited_resource) {
        edit_button->set_pressed(true);
        _update_menu();
        return;
    }

    emit_signal("resource_selected", edited_resource, false);
}

void EditorResourcePicker::_file_selected(const String &p_path) {
    RES loaded_resource = gResourceManager().load(p_path);
    ERR_FAIL_COND_MSG(!loaded_resource, "Cannot load resource from path '" + p_path + "'.");

    if (base_type != "") {
        bool any_type_matches = false;
        Vector<StringView> types = StringUtils::split(base_type, ',');
        for (StringView base : types) {
            if (loaded_resource->is_class(base)) {
                any_type_matches = true;
                break;
            }
        }

        if (!any_type_matches) {
            EditorNode::get_singleton()->show_warning(
                    FormatVE(TTR("The selected resource (%s) does not match any type expected for this property (%s).")
                                     .asCString(),
                            loaded_resource->get_class(), base_type.asCString()));
            return;
        }
    }

    edited_resource = loaded_resource;
    emit_signal("resource_changed", edited_resource);
    _update_resource();
}

void EditorResourcePicker::_file_quick_selected() {
    _file_selected(quick_open->get_selected());
}

void EditorResourcePicker::_update_menu() {
    _update_menu_items();

    Rect2 gt = edit_button->get_global_rect();
    edit_menu->set_as_minsize();
    int ms = edit_menu->get_combined_minimum_size().width;
    Vector2 popup_pos = gt.position + gt.size - Vector2(ms, 0);
    edit_menu->set_global_position(popup_pos);
    edit_menu->popup();
}

void EditorResourcePicker::_update_menu_items() {
    edit_menu->clear();

    // Add options for creating specific subtypes of the base resource type.
    set_create_options(edit_menu);

    // Add an option to load a resource from a file using the QuickOpen dialog.
    edit_menu->add_icon_item(get_theme_icon("Load", "EditorIcons"), TTR("Quick Load"), OBJ_MENU_QUICKLOAD);

    // Add an option to load a resource from a file using the regular file dialog.
    edit_menu->add_icon_item(get_theme_icon("Load", "EditorIcons"), TTR("Load"), OBJ_MENU_LOAD);

    // Add options for changing existing value of the resource.
    if (edited_resource) {
        edit_menu->add_icon_item(get_theme_icon("Edit", "EditorIcons"), TTR("Edit"), OBJ_MENU_EDIT);
        edit_menu->add_icon_item(get_theme_icon("Clear", "EditorIcons"), TTR("Clear"), OBJ_MENU_CLEAR);
        edit_menu->add_icon_item(get_theme_icon("Duplicate", "EditorIcons"), TTR("Make Unique"), OBJ_MENU_MAKE_UNIQUE);
        edit_menu->add_icon_item(get_theme_icon("Save", "EditorIcons"), TTR("Save"), OBJ_MENU_SAVE);

        if (PathUtils::is_resource_file(edited_resource->get_path())) {
            edit_menu->add_separator();
            edit_menu->add_item(TTR("Show in FileSystem"), OBJ_MENU_SHOW_IN_FILE_SYSTEM);
        }
    }

    // Add options to copy/paste resource.
    RES cb = EditorSettings::get_singleton()->get_resource_clipboard();
    bool paste_valid = false;
    if (cb) {
        if (base_type == "") {
            paste_valid = true;
        } else {
            Vector<StringView> types = StringUtils::split(base_type, ',');
            for (StringView t : types) {
                if (ClassDB::class_exists(cb->get_class_name()) &&
                        ClassDB::is_parent_class(cb->get_class_name(), StringName(t))) {
                    paste_valid = true;
                    break;
                }
            }
        }
    }

    if (edited_resource || paste_valid) {
        edit_menu->add_separator();

        if (edited_resource) {
            edit_menu->add_item(TTR("Copy"), OBJ_MENU_COPY);
        }

        if (paste_valid) {
            edit_menu->add_item(TTR("Paste"), OBJ_MENU_PASTE);
        }
    }

    // Add options to convert existing resource to another type of resource.
    if (edited_resource) {
        Vector<Ref<EditorResourceConversionPlugin>> conversions =
                EditorNode::get_singleton()->find_resource_conversion_plugin(edited_resource);
        if (conversions.size()) {
            edit_menu->add_separator();
        }
        for (int i = 0; i < conversions.size(); i++) {
            StringName what = conversions[i]->converts_to();
            Ref<Texture> icon;
            if (has_icon(what, "EditorIcons")) {
                icon = get_theme_icon(what, "EditorIcons");
            } else {
                icon = get_theme_icon(what, "Resource");
            }

            edit_menu->add_icon_item_utf8(
                    icon, FormatVE(TTR("Convert to %s").asCString(), what.asCString()), CONVERT_BASE_ID + i);
        }
    }
}

void EditorResourcePicker::_edit_menu_cbk(int p_which) {
    switch (p_which) {
        case OBJ_MENU_LOAD: {
            Vector<String> extensions;
            Vector<StringView> types = StringUtils::split(base_type, ',');
            for (StringView t : types) {
                gResourceManager().get_recognized_extensions_for_type(t, extensions);
            }

            HashSet<String> valid_extensions;
            for (const String &E : extensions) {
                valid_extensions.insert(E);
            }

            if (!file_dialog) {
                file_dialog = memnew(EditorFileDialog);
                file_dialog->set_mode(EditorFileDialog::MODE_OPEN_FILE);
                add_child(file_dialog);
                file_dialog->connect("file_selected", callable_mp(this, &EditorResourcePicker::_file_selected));
            }

            file_dialog->clear_filters();
            for (const String &E : valid_extensions) {
                file_dialog->add_filter("*." + E + " ; " + StringUtils::to_upper(E));
            }

            file_dialog->popup_centered_ratio();
        } break;

        case OBJ_MENU_QUICKLOAD: {
            if (!quick_open) {
                quick_open = memnew(EditorQuickOpen);
                add_child(quick_open);
                quick_open->connect("quick_open", callable_mp(this, &EditorResourcePicker::_file_quick_selected));
            }

            quick_open->popup_dialog(base_type);
            quick_open->set_title(TTR("Resource"));
        } break;

        case OBJ_MENU_EDIT: {
            if (edited_resource) {
                emit_signal("resource_selected", edited_resource, true);
            }
        } break;

        case OBJ_MENU_CLEAR: {
            edited_resource = RES();
            emit_signal("resource_changed", edited_resource);
            _update_resource();
        } break;

        case OBJ_MENU_MAKE_UNIQUE: {
            if (!edited_resource) {
                return;
            }

            Vector<PropertyInfo> property_list;
            edited_resource->get_property_list(&property_list);
            Vector<Pair<StringName, Variant>> propvalues;
            for (const PropertyInfo &pi : property_list) {
                Pair<StringName, Variant> p;
                if (pi.usage & PROPERTY_USAGE_STORAGE) {
                    p.first = pi.name;
                    p.second = edited_resource->get(pi.name);
                }

                propvalues.push_back(p);
            }

            StringName orig_type = edited_resource->get_class_name();
            Object *inst = ClassDB::instance(orig_type);
            Ref<Resource> unique_resource = Ref<Resource>(object_cast<Resource>(inst));
            ERR_FAIL_COND(!unique_resource);

            for (const Pair<StringName, Variant> &p : propvalues) {
                unique_resource->set(p.first, p.second);
            }

            edited_resource = unique_resource;
            emit_signal("resource_changed", edited_resource);
            _update_resource();
        } break;

        case OBJ_MENU_SAVE: {
            if (!edited_resource) {
                return;
            }
            EditorNode::get_singleton()->save_resource(edited_resource);
        } break;

        case OBJ_MENU_COPY: {
            EditorSettings::get_singleton()->set_resource_clipboard(edited_resource);
        } break;

        case OBJ_MENU_PASTE: {
            edited_resource = EditorSettings::get_singleton()->get_resource_clipboard();
            emit_signal("resource_changed", edited_resource);
            _update_resource();
        } break;

        case OBJ_MENU_SHOW_IN_FILE_SYSTEM: {
            FileSystemDock *file_system_dock = EditorNode::get_singleton()->get_filesystem_dock();
            file_system_dock->navigate_to_path(edited_resource->get_path());

            // Ensure that the FileSystem dock is visible.
            TabContainer *tab_container = (TabContainer *)file_system_dock->get_parent_control();
            tab_container->set_current_tab(file_system_dock->get_index());
        } break;

        default: {
            // Allow subclasses to handle their own options first, only then fallback on the default branch logic.
            if (handle_menu_selected(p_which)) {
                break;
            }

            if (p_which >= CONVERT_BASE_ID) {
                int to_type = p_which - CONVERT_BASE_ID;
                Vector<Ref<EditorResourceConversionPlugin>> conversions =
                        EditorNode::get_singleton()->find_resource_conversion_plugin(edited_resource);
                ERR_FAIL_INDEX(to_type, conversions.size());

                edited_resource = conversions[to_type]->convert(edited_resource);
                emit_signal("resource_changed", edited_resource);
                _update_resource();
                break;
            }

            ERR_FAIL_COND(inheritors_array.empty());

            StringName intype = inheritors_array[p_which - TYPE_BASE_ID];
            Object *obj;

            if (ScriptServer::is_global_class(intype)) {
                obj = ClassDB::instance(ScriptServer::get_global_class_native_base(intype));
                if (obj) {
                    Ref<Script> script = gResourceManager().loadT<Script>(ScriptServer::get_global_class_path(intype));
                    if (script) {
                        ((Object *)obj)->set_script(script.get_ref_ptr());
                    }
                }
            } else {
                obj = ClassDB::instance(intype);
            }

            if (!obj) {
                obj = EditorNode::get_editor_data().instance_custom_type(intype, "Resource");
            }

            Resource *resp = object_cast<Resource>(obj);
            ERR_BREAK(!resp);

            edited_resource = RES(resp);
            emit_signal("resource_changed", edited_resource);
            _update_resource();
        } break;
    }
}

void EditorResourcePicker::set_create_options(Object *p_menu_node) {
    // If a subclass implements this method, use it to replace all create items.
    if (get_script_instance() && get_script_instance()->has_method("set_create_options")) {
        get_script_instance()->call("set_create_options", Variant::from(p_menu_node));
        return;
    }

    // By default provide generic "New ..." options.
    if (base_type.empty()) {
        return;
    }
    int idx = 0;

    HashSet<StringName> allowed_types;
    _get_allowed_types(false, &allowed_types);

    Vector<EditorData::CustomType> custom_resources;
    if (EditorNode::get_editor_data().get_custom_types().contains("Resource")) {
        custom_resources = EditorNode::get_editor_data().get_custom_types().at("Resource", {});
    }

    for (const StringName &t : allowed_types) {
        bool is_custom_resource = false;
        Ref<Texture> icon;
        if (!custom_resources.empty()) {
            for (int j = 0; j < custom_resources.size(); j++) {
                if (custom_resources[j].name == t) {
                    is_custom_resource = true;
                    if (custom_resources[j].icon) {
                        icon = custom_resources[j].icon;
                    }
                    break;
                }
            }
        }

        if (!is_custom_resource && !(ScriptServer::is_global_class(t) || ClassDB::can_instance(t))) {
            continue;
        }

        inheritors_array.push_back(t);

        if (!icon) {
            icon = get_theme_icon(has_icon(t, "EditorIcons") ? t : "Object", "EditorIcons");
        }

        int id = TYPE_BASE_ID + idx;
        edit_menu->add_icon_item_utf8(icon, FormatVE(TTR("New %s").asCString(), t.asCString()), id);

        idx++;
    }

    if (edit_menu->get_item_count()) {
        edit_menu->add_separator();
    }
}

bool EditorResourcePicker::handle_menu_selected(int p_which) {
    if (get_script_instance() && get_script_instance()->has_method("handle_menu_selected")) {
        return get_script_instance()->call("handle_menu_selected", p_which).as<bool>();
    }

    return false;
}

void EditorResourcePicker::_button_draw() {
    if (dropping) {
        Color color = get_theme_color("accent_color", "Editor");
        assign_button->draw_rect_stroke(Rect2(Point2(), assign_button->get_size()), color);
    }
}

void EditorResourcePicker::_button_input(const Ref<InputEvent> &p_event) {
    if (!editable) {
        return;
    }

    Ref<InputEventMouseButton> mb(dynamic_ref_cast<InputEventMouseButton>(p_event));

    if (mb) {
        if (mb->is_pressed() && mb->get_button_index() == BUTTON_RIGHT) {
            _update_menu_items();

            Vector2 pos = get_global_position() + mb->get_position();
            edit_menu->set_as_minsize();
            edit_menu->set_global_position(pos);
            edit_menu->popup();
        }
    }
}

void EditorResourcePicker::_get_allowed_types(bool p_with_convert, HashSet<StringName> *p_vector) const {
    Vector<String> allowed_types = String(base_type).split(',');
    int size = allowed_types.size();

    Vector<StringName> global_classes;
    ScriptServer::get_global_class_list(&global_classes);

    for (int i = 0; i < size; i++) {
        StringName base(StringUtils::strip_edges(allowed_types[i]));
        p_vector->insert(base);

        // If we hit a familiar base type, take all the data from cache.
        if (allowed_types_cache.contains(base)) {
            List<StringName> allowed_subtypes = allowed_types_cache[base];
            for (const StringName &E : allowed_subtypes) {
                p_vector->insert(E);
            }
        } else {
            List<StringName> allowed_subtypes;

            Vector<StringName> inheriters;
            ClassDB::get_inheriters_from_class(base, &inheriters);
            for (StringName &E : inheriters) {
                p_vector->emplace(E);
                allowed_subtypes.emplace_back(E);
            }

            for (StringName &E : global_classes) {
                if (EditorNode::get_editor_data().script_class_is_parent(E, base)) {
                    p_vector->emplace(E);
                    allowed_subtypes.emplace_back(E);
                }
            }

            // Store the subtypes of the base type in the cache for future use.
            allowed_types_cache[base] = allowed_subtypes;
        }

        if (p_with_convert) {
            if (base == "SpatialMaterial") {
                p_vector->insert("Texture");
            } else if (base == "ShaderMaterial") {
                p_vector->insert("Shader");
            }
        }
    }

    if (EditorNode::get_editor_data().get_custom_types().contains("Resource")) {
        Vector<EditorData::CustomType> custom_resources =
                EditorNode::get_editor_data().get_custom_types().at("Resource", {});

        for (int i = 0; i < custom_resources.size(); i++) {
            p_vector->insert(custom_resources[i].name);
        }
    }
}

bool EditorResourcePicker::_is_drop_valid(const Dictionary &p_drag_data) const {
    if (base_type.empty()) {
        return true;
    }

    Dictionary drag_data = p_drag_data;

    Ref<Resource> res;
    if (drag_data.has("type") && String(drag_data["type"]) == "script_list_element") {
        ScriptEditorBase *se = drag_data["script_list_element"].asT<ScriptEditorBase>();
        if (se) {
            res = se->get_edited_resource();
        }
    } else if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
        res = drag_data["resource"];
    }

    HashSet<StringName> allowed_types;
    _get_allowed_types(true, &allowed_types);

    if (res && _is_type_valid(res->get_class_name(), allowed_types)) {
        return true;
    }

    if (res && !res->get_script().is_null()) {
        Ref<Script> res_script = refFromRefPtr<Script>(res->get_script());
        StringName custom_class = EditorNode::get_singleton()->get_object_custom_type_name(res_script.get());
        if (_is_type_valid(custom_class, allowed_types)) {
            return true;
        }
    }

    if (drag_data.has("type") && String(drag_data["type"]) == "files") {
        Vector<String> files = drag_data["files"].as<Vector<String>>();

        if (files.size() == 1) {
            String file = files[0];

            StringName file_type = EditorFileSystem::get_singleton()->get_file_type(file);
            if (file_type != "" && _is_type_valid(file_type, allowed_types)) {
                return true;
            }
        }
    }

    return false;
}

bool EditorResourcePicker::_is_type_valid(
        const StringName &p_type_name, const HashSet<StringName> &p_allowed_types) const {
    for (const StringName &E : p_allowed_types) {
        StringName at(StringUtils::strip_edges(E));
        if (p_type_name == at || (ClassDB::class_exists(p_type_name) && ClassDB::is_parent_class(p_type_name, at)) ||
                EditorNode::get_editor_data().script_class_is_parent(p_type_name, at)) {
            return true;
        }
    }

    return false;
}

Variant EditorResourcePicker::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
    if (edited_resource) {
        return EditorNode::get_singleton()->drag_resource(edited_resource, p_from);
    }

    return Variant();
}

bool EditorResourcePicker::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
    return editable && _is_drop_valid(p_data.as<Dictionary>());
}

void EditorResourcePicker::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
    ERR_FAIL_COND(!_is_drop_valid(p_data.as<Dictionary>()));

    Dictionary drag_data = p_data.as<Dictionary>();

    Ref<Resource> dropped_resource;
    if (drag_data.has("type") && String(drag_data["type"]) == "script_list_element") {
        ScriptEditorBase *se = drag_data["script_list_element"].asT<ScriptEditorBase>();
        if (se) {
            dropped_resource = se->get_edited_resource();
        }
    } else if (drag_data.has("type") && String(drag_data["type"]) == "resource") {
        dropped_resource = drag_data["resource"];
    }

    if (!dropped_resource && drag_data.has("type") && String(drag_data["type"]) == "files") {
        Vector<String> files = drag_data["files"].as<Vector<String>>();

        if (files.size() == 1) {
            String file = files[0];
            dropped_resource = gResourceManager().load(file);
        }
    }

    if (dropped_resource) {
        HashSet<StringName> allowed_types;
        _get_allowed_types(false, &allowed_types);

        // If the accepted dropped resource is from the extended list, it requires conversion.
        if (!_is_type_valid(dropped_resource->get_class_name(), allowed_types)) {
            for (const StringName &E : allowed_types) {
                String at(StringUtils::strip_edges(E));

                if (at == "SpatialMaterial" &&
                        ClassDB::is_parent_class(dropped_resource->get_class_name(), "Texture")) {
                    Ref<SpatialMaterial> mat = make_ref_counted<SpatialMaterial>();
                    mat->set_texture(SpatialMaterial::TextureParam::TEXTURE_ALBEDO, dynamic_ref_cast<Texture>(dropped_resource));
                    dropped_resource = mat;
                    break;
                }

                if (at == "ShaderMaterial" && ClassDB::is_parent_class(dropped_resource->get_class_name(), "Shader")) {
                    Ref<ShaderMaterial> mat = make_ref_counted<ShaderMaterial>();
                    mat->set_shader(dynamic_ref_cast<Shader>(dropped_resource));
                    dropped_resource = mat;
                    break;
                }
            }
        }

        edited_resource = dropped_resource;
        emit_signal("resource_changed", edited_resource);
        _update_resource();
    }
}

void EditorResourcePicker::_bind_methods() {
    // Internal binds.
    //    BIND_METHOD(EditorResourcePicker,_file_selected);
    //    BIND_METHOD(EditorResourcePicker,_file_quick_selected);
    //    BIND_METHOD(EditorResourcePicker,_resource_selected);
    //    BIND_METHOD(EditorResourcePicker,_button_draw);
    //    BIND_METHOD(EditorResourcePicker,_button_input);
    //    BIND_METHOD(EditorResourcePicker,_update_menu);
    //    BIND_METHOD(EditorResourcePicker,_edit_menu_cbk);

    // Public binds.
    SE_BIND_METHOD(EditorResourcePicker,_update_resource_preview);
    MethodBinder::bind_method(
            D_METHOD("get_drag_data_fw", { "position", "from" }), &EditorResourcePicker::get_drag_data_fw);
    MethodBinder::bind_method(
            D_METHOD("can_drop_data_fw", { "position", "data", "from" }), &EditorResourcePicker::can_drop_data_fw);
    MethodBinder::bind_method(
            D_METHOD("drop_data_fw", { "position", "data", "from" }), &EditorResourcePicker::drop_data_fw);

    SE_BIND_METHOD(EditorResourcePicker,set_base_type);
    SE_BIND_METHOD(EditorResourcePicker,get_base_type);
    SE_BIND_METHOD(EditorResourcePicker,get_allowed_types);
    MethodBinder::bind_method(
            D_METHOD("set_edited_resource", { "resource" }), &EditorResourcePicker::set_edited_resource);
    SE_BIND_METHOD(EditorResourcePicker,get_edited_resource);
    SE_BIND_METHOD(EditorResourcePicker,set_toggle_mode);
    SE_BIND_METHOD(EditorResourcePicker,is_toggle_mode);
    SE_BIND_METHOD(EditorResourcePicker,set_toggle_pressed);
    SE_BIND_METHOD(EditorResourcePicker,set_editable);
    SE_BIND_METHOD(EditorResourcePicker,is_editable);

    BIND_VMETHOD(MethodInfo("set_create_options", PropertyInfo(VariantType::OBJECT, "menu_node")));
    BIND_VMETHOD(MethodInfo(VariantType::BOOL,"handle_menu_selected", PropertyInfo(VariantType::INT, "id")));

    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "base_type"), "set_base_type", "get_base_type");
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "edited_resource", PropertyHint::ResourceType, "Resource", 0),
            "set_edited_resource", "get_edited_resource");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "editable"), "set_editable", "is_editable");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "toggle_mode"), "set_toggle_mode", "is_toggle_mode");

    ADD_SIGNAL(MethodInfo("resource_selected",
            PropertyInfo(VariantType::OBJECT, "resource", PropertyHint::ResourceType, "Resource"),
            PropertyInfo(VariantType::BOOL, "edit")));
    ADD_SIGNAL(MethodInfo(
            "resource_changed", PropertyInfo(VariantType::OBJECT, "resource", PropertyHint::ResourceType, "Resource")));
}

void EditorResourcePicker::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_ENTER_TREE: {
            _update_resource();
            [[fallthrough]];
        }
        case NOTIFICATION_THEME_CHANGED: {
            edit_button->set_button_icon(get_theme_icon("select_arrow", "Tree"));
        } break;

        case NOTIFICATION_DRAW: {
            draw_style_box(get_theme_stylebox("bg", "Tree"), Rect2(Point2(), get_size()));
        } break;

        case NOTIFICATION_DRAG_BEGIN: {
            if (editable && _is_drop_valid(get_viewport()->gui_get_drag_data().as<Dictionary>())) {
                dropping = true;
                assign_button->update();
            }
        } break;

        case NOTIFICATION_DRAG_END: {
            if (dropping) {
                dropping = false;
                assign_button->update();
            }
        } break;
    }
}

void EditorResourcePicker::set_base_type(const StringName &p_base_type) {
    base_type = p_base_type;
    HashSet<StringName> allowed_types;

    // There is a possibility that the new base type is conflicting with the existing value.
    // Keep the value, but warn the user that there is a potential mistake.
    if (!base_type.empty() && edited_resource) {
        _get_allowed_types(true, &allowed_types);

        StringName custom_class;
        bool is_custom = false;
        if (!edited_resource->get_script().is_null()) {
            Ref<Script> res_script = refFromRefPtr<Script>(edited_resource->get_script());
            custom_class = EditorNode::get_singleton()->get_object_custom_type_name(res_script.get());
            is_custom = _is_type_valid(custom_class, allowed_types);
        }

        if (!is_custom && !_is_type_valid(edited_resource->get_class_name(), allowed_types)) {
            String class_str = (custom_class == StringName() ?
                                        edited_resource->get_class() :
                                        FormatVE("%s (%s)", custom_class.asCString(), edited_resource->get_class()));
            WARN_PRINT(FormatVE("Value mismatch between the new base type of this EditorResourcePicker, '%s', and the "
                                "type of the value it already has, '%s'.",
                    base_type.asCString(), class_str.c_str()));
        }
    } else {
        // Call the method to build the cache immediately.
        HashSet<StringName> allowed_types;
        _get_allowed_types(false, &allowed_types);
    }
}

StringName EditorResourcePicker::get_base_type() const {
    return base_type;
}

Vector<String> EditorResourcePicker::get_allowed_types() const {
    HashSet<StringName> allowed_types;
    _get_allowed_types(false, &allowed_types);

    Vector<String> types;
    types.reserve(allowed_types.size());

    for (const StringName &E : allowed_types) {
        types.emplace_back(E.asCString());
    }
    eastl::sort<String>(types);
    return types;
}

void EditorResourcePicker::set_edited_resource(RES p_resource) {
    if (!p_resource) {
        edited_resource = RES();
        _update_resource();
        return;
    }

    if (!base_type.empty()) {
        HashSet<StringName> allowed_types;
        _get_allowed_types(true, &allowed_types);

        StringName custom_class;
        bool is_custom = false;
        if (!p_resource->get_script().is_null()) {
            Ref<Script> res_script = refFromRefPtr<Script>(p_resource->get_script());
            custom_class = EditorNode::get_singleton()->get_object_custom_type_name(res_script.get());
            is_custom = _is_type_valid(custom_class, allowed_types);
        }

        if (!is_custom && !_is_type_valid(p_resource->get_class_name(), allowed_types)) {
            String class_str = (custom_class == StringName() ?
                                        p_resource->get_class() :
                                        FormatVE("%s (%s)", custom_class.asCString(), p_resource->get_class()));
            ERR_FAIL_MSG(FormatVE("Failed to set a resource of the type '%s' because this EditorResourcePicker only "
                                  "accepts '%s' and its derivatives.",
                    class_str.c_str(), base_type.asCString()));
        }
    }

    edited_resource = p_resource;
    _update_resource();
}

RES EditorResourcePicker::get_edited_resource() {
    return edited_resource;
}

void EditorResourcePicker::set_toggle_mode(bool p_enable) {
    assign_button->set_toggle_mode(p_enable);
}

bool EditorResourcePicker::is_toggle_mode() const {
    return assign_button->is_toggle_mode();
}

void EditorResourcePicker::set_toggle_pressed(bool p_pressed) {
    if (!is_toggle_mode()) {
        return;
    }

    assign_button->set_pressed(p_pressed);
}

void EditorResourcePicker::set_editable(bool p_editable) {
    editable = p_editable;
    assign_button->set_disabled(!editable);
    edit_button->set_visible(editable);
}

bool EditorResourcePicker::is_editable() const {
    return editable;
}

EditorResourcePicker::EditorResourcePicker() {
    assign_button = memnew(Button);
    assign_button->set_flat(true);
    assign_button->set_h_size_flags(SIZE_EXPAND_FILL);
    assign_button->set_clip_text(true);
    assign_button->set_drag_forwarding(this);
    add_child(assign_button);
    assign_button->connect("pressed", callable_mp(this, &EditorScriptPicker::_resource_selected));
    assign_button->connect("draw", callable_mp(this, &EditorScriptPicker::_button_draw));
    assign_button->connect("gui_input", callable_mp(this, &EditorScriptPicker::_button_input));

    preview_rect = memnew(TextureRect);
    preview_rect->set_expand(true);
    preview_rect->set_anchors_and_margins_preset(PRESET_WIDE);
    preview_rect->set_margin(Margin::Top, 1);
    preview_rect->set_margin(Margin::Bottom, -1);
    preview_rect->set_margin(Margin::Right, -1);
    assign_button->add_child(preview_rect);

    edit_button = memnew(Button);
    edit_button->set_flat(true);
    edit_button->set_toggle_mode(true);
    edit_button->connect("pressed", callable_mp(this, &EditorScriptPicker::_update_menu));
    add_child(edit_button);
    edit_button->connect("gui_input", callable_mp(this, &EditorScriptPicker::_button_input));
    edit_menu = memnew(PopupMenu);
    add_child(edit_menu);
    edit_menu->connect("id_pressed", callable_mp(this, &EditorScriptPicker::_edit_menu_cbk));
    edit_menu->connectF("popup_hide", edit_button, [this]() { edit_button->set_pressed(false); });
}

void EditorScriptPicker::set_create_options(Object *p_menu_node) {
    PopupMenu *menu_node = object_cast<PopupMenu>(p_menu_node);
    if (!menu_node) {
        return;
    }

    menu_node->add_icon_item(get_theme_icon("ScriptCreate", "EditorIcons"), TTR("New Script"), OBJ_MENU_NEW_SCRIPT);
    if (script_owner) {
        Ref<Script> script = refFromRefPtr<Script>(script_owner->get_script());
        if (script) {
            menu_node->add_icon_item(get_theme_icon("ScriptExtend", "EditorIcons"), TTR("Extend Script"), OBJ_MENU_EXTEND_SCRIPT);
        }
    }
    menu_node->add_icon_item(
            get_theme_icon("ScriptExtend", "EditorIcons"), TTR("Extend Script"), OBJ_MENU_EXTEND_SCRIPT);
    menu_node->add_separator();
}

bool EditorScriptPicker::handle_menu_selected(int p_which) {
    switch (p_which) {
        case OBJ_MENU_NEW_SCRIPT: {
            if (script_owner) {
                EditorNode::get_singleton()->get_scene_tree_dock()->open_script_dialog(script_owner, false);
            }
            return true;
        }

        case OBJ_MENU_EXTEND_SCRIPT: {
            if (script_owner) {
                EditorNode::get_singleton()->get_scene_tree_dock()->open_script_dialog(script_owner, true);
            }
            return true;
        }
    }

    return false;
}

void EditorScriptPicker::set_script_owner(Node *p_owner) {
    script_owner = p_owner;
}

Node *EditorScriptPicker::get_script_owner() const {
    return script_owner;
}

void EditorScriptPicker::_bind_methods() {
    SE_BIND_METHOD(EditorScriptPicker,set_script_owner);
    SE_BIND_METHOD(EditorScriptPicker,get_script_owner);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "script_owner", PropertyHint::ResourceType, "Node", 0),
            "set_script_owner", "get_script_owner");
}

EditorScriptPicker::EditorScriptPicker() {}
