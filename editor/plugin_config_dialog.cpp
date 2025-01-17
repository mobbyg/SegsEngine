/*************************************************************************/
/*  plugin_config_dialog.cpp                                             */
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

#include "plugin_config_dialog.h"

#include "core/io/config_file.h"
#include "core/callable_method_pointer.h"
#include "core/method_bind.h"
#include "core/os/dir_access.h"
#include "core/resource/resource_manager.h"
#include "core/string_utils.inl"
#include "editor/editor_node.h"
#include "editor/editor_plugin.h"
#include "editor/project_settings_editor.h"
#include "editor/editor_scale.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/text_edit.h"

IMPL_GDCLASS(PluginConfigDialog)

void PluginConfigDialog::_clear_fields() {
    name_edit->set_text("");
    subfolder_edit->set_text("");
    desc_edit->set_text_ui(UIString());
    author_edit->set_text("");
    version_edit->set_text("");
    script_edit->set_text("");
}

void PluginConfigDialog::_on_confirmed() {

    String path = String("res://addons/") + StringUtils::to_utf8(subfolder_edit->get_text_ui()).data();

    if (!_edit_mode) {
        DirAccess *d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
        if (!d || d->make_dir_recursive(path) != OK)
            return;
    }

    Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());
    cf->set_value("plugin", "name", name_edit->get_text());
    cf->set_value("plugin", "description", desc_edit->get_text_utf8());
    cf->set_value("plugin", "author", author_edit->get_text());
    cf->set_value("plugin", "version", version_edit->get_text());
    cf->set_value("plugin", "script", script_edit->get_text());

    cf->save(PathUtils::plus_file(path,"plugin.cfg"));

    if (!_edit_mode) {
        int lang_idx = script_option_edit->get_selected();
        StringName lang_name = ScriptServer::get_language(lang_idx)->get_name();

        Ref<Script> script;

        // TODO Use script templates. Right now, this code won't add the 'tool' annotation to other languages.
        // TODO Better support script languages with named classes (has_named_classes).
        String script_path(PathUtils::plus_file(path,script_edit->get_text()));
        StringView class_name(PathUtils::get_basename(PathUtils::get_file(script_path)));
        script = ScriptServer::get_language(lang_idx)->get_template(class_name, "EditorPlugin");
        script->set_path(script_path);
        gResourceManager().save(script_path, script);
        emit_signal("plugin_ready", Variant(script), active_edit->is_pressed() ? subfolder_edit->get_text() : StringView());
    } else {
        EditorNode::get_singleton()->get_project_settings()->update_plugins();
    }
    _clear_fields();
}

void PluginConfigDialog::_on_cancelled() {
    _clear_fields();
}

void PluginConfigDialog::_on_required_text_changed(StringView ) {
    int lang_idx = script_option_edit->get_selected();
    String ext(ScriptServer::get_language(lang_idx)->get_extension());
    get_ok()->set_disabled(PathUtils::get_basename(script_edit->get_text()).empty() ||
                           PathUtils::get_extension(script_edit->get_text()) != StringView(ext) ||
                           name_edit->get_text().empty());
}

void PluginConfigDialog::_notification(int p_what) {
    switch (p_what) {
        case NOTIFICATION_READY: {
            connect("confirmed",callable_mp(this, &ClassName::_on_confirmed));
            get_cancel()->connect("pressed",callable_mp(this, &ClassName::_on_cancelled));
        } break;

        case NOTIFICATION_POST_POPUP: {
            name_edit->grab_focus();
        } break;
    }
}

void PluginConfigDialog::config(StringView p_config_path) {
    if (p_config_path.length()) {
        Ref<ConfigFile> cf(make_ref_counted<ConfigFile>());
        Error err = cf->load(p_config_path);
        ERR_FAIL_COND_MSG(err != OK, "Cannot load config file from path '" + String(p_config_path) + "'.");

        name_edit->set_text(cf->get_value("plugin", "name", "").as<String>());
        subfolder_edit->set_text(PathUtils::get_file(PathUtils::get_basename(PathUtils::get_base_dir(p_config_path))));
        desc_edit->set_text(cf->get_value("plugin", "description", "").as<String>());
        author_edit->set_text(cf->get_value("plugin", "author", "").as<String>());
        version_edit->set_text(cf->get_value("plugin", "version", "").as<String>());
        script_edit->set_text(cf->get_value("plugin", "script", "").as<String>());

        _edit_mode = true;
        active_edit->hide();
        object_cast<Label>(active_edit->get_parent()->get_child(active_edit->get_index() - 1))->hide();
        subfolder_edit->hide();
        object_cast<Label>(subfolder_edit->get_parent()->get_child(subfolder_edit->get_index() - 1))->hide();
        set_title(TTR("Edit a Plugin"));
    } else {
        _clear_fields();
        _edit_mode = false;
        active_edit->show();
        object_cast<Label>(active_edit->get_parent()->get_child(active_edit->get_index() - 1))->show();
        subfolder_edit->show();
        object_cast<Label>(subfolder_edit->get_parent()->get_child(subfolder_edit->get_index() - 1))->show();
        set_title(TTR("Create a Plugin"));
    }
    get_ok()->set_disabled(!_edit_mode);
    get_ok()->set_text(_edit_mode ? TTR("Update") : TTR("Create"));
}

void PluginConfigDialog::_bind_methods() {
    ADD_SIGNAL(MethodInfo("plugin_ready", PropertyInfo(VariantType::STRING, "script_path", PropertyHint::None, ""), PropertyInfo(VariantType::STRING, "activate_name")));
}

PluginConfigDialog::PluginConfigDialog() {
    get_ok()->set_disabled(true);
    set_hide_on_ok(true);

    GridContainer *grid = memnew(GridContainer);
    grid->set_columns(2);
    add_child(grid);

    Label *name_lb = memnew(Label);
    name_lb->set_text(TTR("Plugin Name:"));
    grid->add_child(name_lb);

    name_edit = memnew(LineEdit);
    name_edit->connect("text_changed",callable_mp(this, &ClassName::_on_required_text_changed));
    name_edit->set_placeholder("MyPlugin");
    grid->add_child(name_edit);

    Label *subfolder_lb = memnew(Label);
    subfolder_lb->set_text(TTR("Subfolder:"));
    grid->add_child(subfolder_lb);

    subfolder_edit = memnew(LineEdit);
    subfolder_edit->set_placeholder("\"my_plugin\" -> res://addons/my_plugin");
    grid->add_child(subfolder_edit);

    Label *desc_lb = memnew(Label);
    desc_lb->set_text(TTR("Description:"));
    grid->add_child(desc_lb);

    desc_edit = memnew(TextEdit);
    desc_edit->set_custom_minimum_size(Size2(400, 80) * EDSCALE);
    desc_edit->set_wrap_enabled(true);
    grid->add_child(desc_edit);

    Label *author_lb = memnew(Label);
    author_lb->set_text(TTR("Author:"));
    grid->add_child(author_lb);

    author_edit = memnew(LineEdit);
    author_edit->set_placeholder("Godette");
    grid->add_child(author_edit);

    Label *version_lb = memnew(Label);
    version_lb->set_text(TTR("Version:"));
    grid->add_child(version_lb);

    version_edit = memnew(LineEdit);
    version_edit->set_placeholder("1.0");
    grid->add_child(version_edit);

    Label *script_option_lb = memnew(Label);
    script_option_lb->set_text(TTR("Language:"));
    grid->add_child(script_option_lb);

    script_option_edit = memnew(OptionButton);
    int default_lang = 0;
    for (int i = 0; i < ScriptServer::get_language_count(); i++) {
        ScriptLanguage *lang = ScriptServer::get_language(i);
        script_option_edit->add_item(lang->get_name());
    }
    if(ScriptServer::get_language_count()>0)
        script_option_edit->select(default_lang);
    grid->add_child(script_option_edit);

    Label *script_lb = memnew(Label);
    script_lb->set_text(TTR("Script Name:"));
    grid->add_child(script_lb);

    script_edit = memnew(LineEdit);
    script_edit->connect("text_changed",callable_mp(this, &ClassName::_on_required_text_changed));
    script_edit->set_placeholder("\"plugin.gd\" -> res://addons/my_plugin/plugin.gd");
    grid->add_child(script_edit);

    // TODO Make this option work better with languages like C#. Right now, it does not work because the C# project must be compiled first.
    Label *active_lb = memnew(Label);
    active_lb->set_text(TTR("Activate now?"));
    grid->add_child(active_lb);

    active_edit = memnew(CheckBox);
    active_edit->set_pressed(true);
    grid->add_child(active_edit);
}

PluginConfigDialog::~PluginConfigDialog() = default;
