/*************************************************************************/
/*  dependency_editor.cpp                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
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

#include "dependency_editor.h"

#include "core/method_bind.h"
#include "core/io/resource_loader.h"
#include "core/os/file_access.h"
#include "editor_node.h"
#include "scene/gui/margin_container.h"
#include "editor/editor_file_system.h"

IMPL_GDCLASS(DependencyEditor)
IMPL_GDCLASS(DependencyEditorOwners)
IMPL_GDCLASS(DependencyRemoveDialog)
IMPL_GDCLASS(DependencyErrorDialog)
IMPL_GDCLASS(OrphanResourcesDialog)

void DependencyEditor::_searched(const String &p_path) {

    Map<String, String> dep_rename;
    dep_rename[replacing] = p_path;

    ResourceLoader::rename_dependencies(editing, dep_rename);

    _update_list();
    _update_file();
}

void DependencyEditor::_load_pressed(Object *p_item, int p_cell, int p_button) {

    TreeItem *ti = Object::cast_to<TreeItem>(p_item);
    String fname = ti->get_text(0);
    replacing = ti->get_text(1);

    search->set_title(TTR("Search Replacement For:") + " " + PathUtils::get_file(replacing));

    search->clear_filters();
    ListPOD<String> ext;
    ResourceLoader::get_recognized_extensions_for_type(ti->get_metadata(0), &ext);
    for (const String &E : ext) {
        search->add_filter("*" + E);
    }
    search->popup_centered_ratio(0.65f); // So it doesn't completely cover the dialog below it.
}

void DependencyEditor::_fix_and_find(EditorFileSystemDirectory *efsd, Map<String, Map<String, String> > &candidates) {

    for (int i = 0; i < efsd->get_subdir_count(); i++) {
        _fix_and_find(efsd->get_subdir(i), candidates);
    }

    for (int i = 0; i < efsd->get_file_count(); i++) {

        String file = efsd->get_file(i);
        if (!candidates.contains(file))
            continue;

        String path = efsd->get_file_path(i);

        for (auto & E : candidates[file]) {

            if (E.second.empty()) {
                E.second = path;
                continue;
            }

            //must match the best, using subdirs
            String existing = StringUtils::replace_first(E.second,"res://", "");
            String current = StringUtils::replace_first(path,"res://", "");
            String lost = StringUtils::replace_first(E.first,"res://", "");

            Vector<String> existingv = StringUtils::split(existing,"/");
            existingv.invert();
            Vector<String> currentv = StringUtils::split(current,"/");
            currentv.invert();
            Vector<String> lostv = StringUtils::split(lost,"/");
            lostv.invert();

            int existing_score = 0;
            int current_score = 0;

            for (int j = 0; j < lostv.size(); j++) {

                if (j < existingv.size() && lostv[j] == existingv[j]) {
                    existing_score++;
                }
                if (j < currentv.size() && lostv[j] == currentv[j]) {
                    current_score++;
                }
            }

            if (current_score > existing_score) {

                //if it was the same, could track distance to new path but..

                E.second = path; //replace by more accurate
            }
        }
    }
}

void DependencyEditor::_fix_all() {

    if (!EditorFileSystem::get_singleton()->get_filesystem())
        return;

    Map<String, Map<String, String> > candidates;

    for (List<String>::Element *E = missing.front(); E; E = E->next()) {

        String base = PathUtils::get_file(E->deref());
        if (!candidates.contains(base)) {
            candidates[base] = Map<String, String>();
        }

        candidates[base][E->deref()] = "";
    }

    _fix_and_find(EditorFileSystem::get_singleton()->get_filesystem(), candidates);

    Map<String, String> remaps;

    for (eastl::pair<const String, Map<String, String> > &E : candidates) {

        for (eastl::pair<const String, String> &F : E.second) {

            if (!F.second.empty()) {
                remaps[F.first] = F.second;
            }
        }
    }

    if (!remaps.empty()) {

        ResourceLoader::rename_dependencies(editing, remaps);

        _update_list();
        _update_file();
    }
}

void DependencyEditor::_update_file() {

    EditorFileSystem::get_singleton()->update_file(editing);
}

void DependencyEditor::_update_list() {

    ListPOD<String> deps;
    ResourceLoader::get_dependencies(editing, &deps, true);

    tree->clear();
    missing.clear();

    TreeItem *root = tree->create_item();

    Ref<Texture> folder = get_icon("folder", "FileDialog");

    bool broken = false;

    for (const String &n : deps) {

        TreeItem *item = tree->create_item(root);

        String path;
        String type;

        if (StringUtils::contains(n,"::") ) {
            path = StringUtils::get_slice(n,"::", 0);
            type = StringUtils::get_slice(n,"::", 1);
        } else {
            path = n;
            type = "Resource";
        }
        String name = PathUtils::get_file(path);

        Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(type);
        item->set_text(0, name);
        item->set_icon(0, icon);
        item->set_metadata(0, type);
        item->set_text(1, path);

        if (!FileAccess::exists(path)) {
            item->set_custom_color(1, Color(1, 0.4f, 0.3f));
            missing.push_back(path);
            broken = true;
        }

        item->add_button(1, folder, 0);
    }

    fixdeps->set_disabled(!broken);
}

void DependencyEditor::edit(const String &p_path) {

    editing = p_path;
    set_title(TTR("Dependencies For:") + " " + PathUtils::get_file(p_path));

    _update_list();
    popup_centered_ratio(0.7); // So it doesn't completely cover the dialog below it.

    if (EditorNode::get_singleton()->is_scene_open(p_path)) {
        EditorNode::get_singleton()->show_warning(vformat(TTR("Scene '%s' is currently being edited.\nChanges will only take effect when reloaded."), PathUtils::get_file(p_path)));
    } else if (ResourceCache::has(p_path)) {
        EditorNode::get_singleton()->show_warning(vformat(TTR("Resource '%s' is in use.\nChanges will only take effect when reloaded."), PathUtils::get_file(p_path)));
    }
}

void DependencyEditor::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_searched"), &DependencyEditor::_searched);
    MethodBinder::bind_method(D_METHOD("_load_pressed"), &DependencyEditor::_load_pressed);
    MethodBinder::bind_method(D_METHOD("_fix_all"), &DependencyEditor::_fix_all);
}

DependencyEditor::DependencyEditor() {

    VBoxContainer *vb = memnew(VBoxContainer);
    vb->set_name(TTR("Dependencies"));
    add_child(vb);

    tree = memnew(Tree);
    tree->set_columns(2);
    tree->set_column_titles_visible(true);
    tree->set_column_title(0, TTR("Resource"));
    tree->set_column_title(1, TTR("Path"));
    tree->set_hide_root(true);
    tree->connect("button_pressed", this, "_load_pressed");

    HBoxContainer *hbc = memnew(HBoxContainer);
    Label *label = memnew(Label(TTR("Dependencies:")));
    hbc->add_child(label);
    hbc->add_spacer();
    fixdeps = memnew(Button(TTR("Fix Broken")));
    hbc->add_child(fixdeps);
    fixdeps->connect("pressed", this, "_fix_all");

    vb->add_child(hbc);

    MarginContainer *mc = memnew(MarginContainer);
    mc->set_v_size_flags(SIZE_EXPAND_FILL);

    mc->add_child(tree);
    vb->add_child(mc);

    set_title(TTR("Dependency Editor"));
    search = memnew(EditorFileDialog);
    search->connect("file_selected", this, "_searched");
    search->set_mode(EditorFileDialog::MODE_OPEN_FILE);
    search->set_title(TTR("Search Replacement Resource:"));
    add_child(search);
}

/////////////////////////////////////
void DependencyEditorOwners::_list_rmb_select(int p_item, const Vector2 &p_pos) {

    file_options->clear();
    file_options->set_size(Size2(1, 1));
    if (p_item >= 0) {
        file_options->add_item(TTR("Open"), FILE_OPEN);
    }

    file_options->set_position(owners->get_global_position() + p_pos);
    file_options->popup();
}

void DependencyEditorOwners::_select_file(int p_idx) {

    String fpath = owners->get_item_text(p_idx);

    if (ResourceLoader::get_resource_type(fpath) == "PackedScene") {
        editor->open_request(fpath);
        hide();
        emit_signal("confirmed");
    }
}

void DependencyEditorOwners::_file_option(int p_option) {

    switch (p_option) {
        case FILE_OPEN: {
            int idx = owners->get_current();
            if (idx < 0 || idx >= owners->get_item_count())
                break;
            _select_file(idx);
        } break;
    }
}

void DependencyEditorOwners::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_list_rmb_select"), &DependencyEditorOwners::_list_rmb_select);
    MethodBinder::bind_method(D_METHOD("_file_option"), &DependencyEditorOwners::_file_option);
    MethodBinder::bind_method(D_METHOD("_select_file"), &DependencyEditorOwners::_select_file);
}

void DependencyEditorOwners::_fill_owners(EditorFileSystemDirectory *efsd) {

    if (!efsd)
        return;

    for (int i = 0; i < efsd->get_subdir_count(); i++) {
        _fill_owners(efsd->get_subdir(i));
    }

    for (int i = 0; i < efsd->get_file_count(); i++) {

        Vector<String> deps = efsd->get_file_deps(i);
        bool found = false;
        for (int j = 0; j < deps.size(); j++) {
            if (deps[j] == editing) {
                found = true;
                break;
            }
        }
        if (!found)
            continue;

        Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(efsd->get_file_type(i));

        owners->add_item(efsd->get_file_path(i), icon);
    }
}

void DependencyEditorOwners::show(const String &p_path) {

    editing = p_path;
    owners->clear();
    _fill_owners(EditorFileSystem::get_singleton()->get_filesystem());
    popup_centered_ratio();

    set_title(TTR("Owners Of:") + " " + PathUtils::get_file(p_path));
}

DependencyEditorOwners::DependencyEditorOwners(EditorNode *p_editor) {

    editor = p_editor;

    file_options = memnew(PopupMenu);
    add_child(file_options);
    file_options->connect("id_pressed", this, "_file_option");

    owners = memnew(ItemList);
    owners->set_select_mode(ItemList::SELECT_SINGLE);
    owners->connect("item_rmb_selected", this, "_list_rmb_select");
    owners->connect("item_activated", this, "_select_file");
    owners->set_allow_rmb_select(true);
    add_child(owners);
}

///////////////////////

void DependencyRemoveDialog::_find_files_in_removed_folder(EditorFileSystemDirectory *efsd, const String &p_folder) {
    if (!efsd)
        return;

    for (int i = 0; i < efsd->get_subdir_count(); ++i) {
        _find_files_in_removed_folder(efsd->get_subdir(i), p_folder);
    }
    for (int i = 0; i < efsd->get_file_count(); i++) {
        String file = efsd->get_file_path(i);
        ERR_FAIL_COND(all_remove_files.contains(file)) //We are deleting a directory which is contained in a directory we are deleting...
        all_remove_files[file] = p_folder; //Point the file to the ancestor directory we are deleting so we know what to parent it under in the tree.
    }
}

void DependencyRemoveDialog::_find_all_removed_dependencies(EditorFileSystemDirectory *efsd, Vector<RemovedDependency> &p_removed) {
    if (!efsd)
        return;

    for (int i = 0; i < efsd->get_subdir_count(); i++) {
        _find_all_removed_dependencies(efsd->get_subdir(i), p_removed);
    }

    for (int i = 0; i < efsd->get_file_count(); i++) {
        const String path = efsd->get_file_path(i);

        //It doesn't matter if a file we are about to delete will have some of its dependencies removed too
        if (all_remove_files.contains(path))
            continue;

        Vector<String> all_deps = efsd->get_file_deps(i);
        for (int j = 0; j < all_deps.size(); ++j) {
            if (all_remove_files.contains(all_deps[j])) {
                RemovedDependency dep;
                dep.file = path;
                dep.file_type = efsd->get_file_type(i);
                dep.dependency = all_deps[j];
                dep.dependency_folder = all_remove_files[all_deps[j]];
                p_removed.push_back(dep);
            }
        }
    }
}

void DependencyRemoveDialog::_build_removed_dependency_tree(const Vector<RemovedDependency> &p_removed) {
    owners->clear();
    owners->create_item(); // root

    Map<String, TreeItem *> tree_items;
    for (int i = 0; i < p_removed.size(); i++) {
        RemovedDependency rd = p_removed[i];

        //Ensure that the dependency is already in the tree
        if (!tree_items.contains(rd.dependency)) {
            if (rd.dependency_folder.length() > 0) {
                //Ensure the ancestor folder is already in the tree
                if (!tree_items.contains(rd.dependency_folder)) {
                    TreeItem *folder_item = owners->create_item(owners->get_root());
                    folder_item->set_text(0, rd.dependency_folder);
                    folder_item->set_icon(0, get_icon("Folder", "EditorIcons"));
                    tree_items[rd.dependency_folder] = folder_item;
                }
                TreeItem *dependency_item = owners->create_item(tree_items[rd.dependency_folder]);
                dependency_item->set_text(0, rd.dependency);
                dependency_item->set_icon(0, get_icon("Warning", "EditorIcons"));
                tree_items[rd.dependency] = dependency_item;
            } else {
                TreeItem *dependency_item = owners->create_item(owners->get_root());
                dependency_item->set_text(0, rd.dependency);
                dependency_item->set_icon(0, get_icon("Warning", "EditorIcons"));
                tree_items[rd.dependency] = dependency_item;
            }
        }

        //List this file under this dependency
        Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(rd.file_type);
        TreeItem *file_item = owners->create_item(tree_items[rd.dependency]);
        file_item->set_text(0, rd.file);
        file_item->set_icon(0, icon);
    }
}

void DependencyRemoveDialog::show(const Vector<String> &p_folders, const Vector<String> &p_files) {
    all_remove_files.clear();
    dirs_to_delete.clear();
    files_to_delete.clear();
    owners->clear();

    for (int i = 0; i < p_folders.size(); ++i) {
        String folder = StringUtils::ends_with(p_folders[i],"/") ? p_folders[i] : (p_folders[i] + "/");
        _find_files_in_removed_folder(EditorFileSystem::get_singleton()->get_filesystem_path(folder), folder);
        dirs_to_delete.push_back(folder);
    }
    for (int i = 0; i < p_files.size(); ++i) {
        all_remove_files[p_files[i]] = String();
        files_to_delete.push_back(p_files[i]);
    }

    Vector<RemovedDependency> removed_deps;
    _find_all_removed_dependencies(EditorFileSystem::get_singleton()->get_filesystem(), removed_deps);
    removed_deps.sort();
    if (removed_deps.empty()) {
        owners->hide();
        text->set_text(TTR("Remove selected files from the project? (Can't be restored)"));
        set_size(Size2());
        popup_centered();
    } else {
        _build_removed_dependency_tree(removed_deps);
        owners->show();
        text->set_text(TTR("The files being removed are required by other resources in order for them to work.\nRemove them anyway? (no undo)"));
        popup_centered(Size2(500, 350));
    }
    EditorFileSystem::get_singleton()->scan_changes();
}

void DependencyRemoveDialog::ok_pressed() {

    for (int i = 0; i < files_to_delete.size(); ++i) {
        if (ResourceCache::has(files_to_delete[i])) {
            Resource *res = ResourceCache::get(files_to_delete[i]);
            res->set_path("");
        }
        String file=files_to_delete[i];
        // If the file we are deleting for e.g. the main scene, default environment,
        // or audio bus layout, we must clear its definition in Project Settings.
        if (file == ProjectSettings::get_singleton()->getT<String>("application/config/icon")) {
            ProjectSettings::get_singleton()->set("application/config/icon", "");
        }
        if (file == ProjectSettings::get_singleton()->getT<String>("application/run/main_scene")) {
            ProjectSettings::get_singleton()->set("application/run/main_scene", "");
        }
        if (file == ProjectSettings::get_singleton()->getT<String>("application/boot_splash/image")) {
            ProjectSettings::get_singleton()->set("application/boot_splash/image", "");
        }
        if (file == ProjectSettings::get_singleton()->getT<String>("rendering/environment/default_environment")) {
            ProjectSettings::get_singleton()->set("rendering/environment/default_environment", "");
        }
        if (file == ProjectSettings::get_singleton()->getT<String>("display/mouse_cursor/custom_image")) {
            ProjectSettings::get_singleton()->set("display/mouse_cursor/custom_image", "");
        }
        if (file == ProjectSettings::get_singleton()->getT<String>("gui/theme/custom")) {
            ProjectSettings::get_singleton()->set("gui/theme/custom", "");
        }
        if (file == ProjectSettings::get_singleton()->getT<String>("gui/theme/custom_font")) {
            ProjectSettings::get_singleton()->set("gui/theme/custom_font", "");
        }
        if (file == ProjectSettings::get_singleton()->getT<String>("audio/default_bus_layout")) {
            ProjectSettings::get_singleton()->set("audio/default_bus_layout", "");
        }

        String path = OS::get_singleton()->get_resource_dir() + StringUtils::replace_first(file,"res://", "/");
        print_verbose("Moving to trash: " + path);
        Error err = OS::get_singleton()->move_to_trash(path);
        if (err != OK) {
            EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + file + "\n");
        } else {
            emit_signal("file_removed", file);
        }
    }

    if (dirs_to_delete.empty()) {
        // If we only deleted files we should only need to tell the file system about the files we touched.
        for (int i = 0; i < files_to_delete.size(); ++i)
            EditorFileSystem::get_singleton()->update_file(files_to_delete[i]);
    } else {

        for (int i = 0; i < dirs_to_delete.size(); ++i) {
            String path = OS::get_singleton()->get_resource_dir() + StringUtils::replace_first(dirs_to_delete[i],"res://", "/");
            print_verbose("Moving to trash: " + path);
            Error err = OS::get_singleton()->move_to_trash(path);
            if (err != OK) {
                EditorNode::get_singleton()->add_io_error(TTR("Cannot remove:") + "\n" + dirs_to_delete[i] + "\n");
            } else {
                emit_signal("folder_removed", dirs_to_delete[i]);
            }
        }

        EditorFileSystem::get_singleton()->scan_changes();
    }

    // If some files/dirs would be deleted, favorite dirs need to be updated
    Vector<String> previous_favorites = EditorSettings::get_singleton()->get_favorites();
    Vector<String> new_favorites;

    for (int i = 0; i < previous_favorites.size(); ++i) {
        if (StringUtils::ends_with(previous_favorites[i],"/")) {
            if (dirs_to_delete.find(previous_favorites[i]) < 0)
                new_favorites.push_back(previous_favorites[i]);
        } else {
            if (files_to_delete.find(previous_favorites[i]) < 0)
                new_favorites.push_back(previous_favorites[i]);
        }
    }

    if (new_favorites.size() < previous_favorites.size()) {
        EditorSettings::get_singleton()->set_favorites(new_favorites);
    }
}

void DependencyRemoveDialog::_bind_methods() {
    ADD_SIGNAL(MethodInfo("file_removed", PropertyInfo(VariantType::STRING, "file")));
    ADD_SIGNAL(MethodInfo("folder_removed", PropertyInfo(VariantType::STRING, "folder")));
}

DependencyRemoveDialog::DependencyRemoveDialog() {

    get_ok()->set_text(TTR("Remove"));

    VBoxContainer *vb = memnew(VBoxContainer);
    add_child(vb);

    text = memnew(Label);
    vb->add_child(text);

    owners = memnew(Tree);
    owners->set_hide_root(true);
    vb->add_child(owners);
    owners->set_v_size_flags(SIZE_EXPAND_FILL);
}

//////////////

void DependencyErrorDialog::show(Mode p_mode, const String &p_for_file, const Vector<String> &report) {

    mode = p_mode;
    for_file = p_for_file;
    set_title(TTR("Error loading:") + " " + PathUtils::get_file(p_for_file));
    files->clear();

    TreeItem *root = files->create_item(nullptr);
    for (int i = 0; i < report.size(); i++) {

        String dep;
        String type = "Object";
        dep = StringUtils::get_slice(report[i],"::", 0);
        if (StringUtils::get_slice_count(report[i],"::") > 0)
            type = StringUtils::get_slice(report[i],"::", 1);

        Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(type);

        TreeItem *ti = files->create_item(root);
        ti->set_text(0, dep);
        ti->set_icon(0, icon);
    }

    popup_centered();
}

void DependencyErrorDialog::ok_pressed() {

    switch (mode) {
        case MODE_SCENE:
            EditorNode::get_singleton()->load_scene(for_file, true);
            break;
        case MODE_RESOURCE:
            EditorNode::get_singleton()->load_resource(for_file, true);
            break;
    }
}

void DependencyErrorDialog::custom_action(const String &) {

    EditorNode::get_singleton()->fix_dependencies(for_file);
}

DependencyErrorDialog::DependencyErrorDialog() {

    VBoxContainer *vb = memnew(VBoxContainer);
    add_child(vb);

    files = memnew(Tree);
    files->set_hide_root(true);
    vb->add_margin_child(TTR("Load failed due to missing dependencies:"), files, true);
    files->set_v_size_flags(SIZE_EXPAND_FILL);

    set_custom_minimum_size(Size2(500, 220));
    get_ok()->set_text(TTR("Open Anyway"));
    get_cancel()->set_text(TTR("Close"));

    text = memnew(Label);
    vb->add_child(text);
    text->set_text(TTR("Which action should be taken?"));

    fdep = add_button(TTR("Fix Dependencies"), true, "fixdeps");

    set_title(TTR("Errors loading!"));
}

//////////////////////////////////////////////////////////////////////

void OrphanResourcesDialog::ok_pressed() {

    paths.clear();

    _find_to_delete(files->get_root(), paths);
    if (paths.empty())
        return;

    delete_confirm->set_text(vformat(TTR("Permanently delete %d item(s)? (No undo!)"), paths.size()));
    delete_confirm->popup_centered_clamped(delete_confirm->get_minimum_size());
}

bool OrphanResourcesDialog::_fill_owners(EditorFileSystemDirectory *efsd, HashMap<String, int> &refs, TreeItem *p_parent) {

    if (!efsd)
        return false;

    bool has_children = false;

    for (int i = 0; i < efsd->get_subdir_count(); i++) {

        TreeItem *dir_item = nullptr;
        if (p_parent) {
            dir_item = files->create_item(p_parent);
            dir_item->set_text(0, efsd->get_subdir(i)->get_name());
            dir_item->set_icon(0, get_icon("folder", "FileDialog"));
        }
        bool children = _fill_owners(efsd->get_subdir(i), refs, dir_item);

        if (p_parent) {
            if (!children) {
                memdelete(dir_item);
            } else {
                has_children = true;
            }
        }
    }

    for (int i = 0; i < efsd->get_file_count(); i++) {

        if (!p_parent) {
            Vector<String> deps = efsd->get_file_deps(i);
            for (int j = 0; j < deps.size(); j++) {

                if (!refs.contains(deps[j])) {
                    refs[deps[j]] = 1;
                }
            }
        } else {

            String path = efsd->get_file_path(i);
            if (!refs.contains(path)) {
                TreeItem *ti = files->create_item(p_parent);
                ti->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
                ti->set_text(0, efsd->get_file(i));
                ti->set_editable(0, true);

                String type = efsd->get_file_type(i);

                Ref<Texture> icon = EditorNode::get_singleton()->get_class_icon(type);
                ti->set_icon(0, icon);
                int ds = efsd->get_file_deps(i).size();
                ti->set_text(1, itos(ds));
                if (ds) {
                    ti->add_button(1, get_icon("GuiVisibilityVisible", "EditorIcons"), -1, false, TTR("Show Dependencies"));
                }
                ti->set_metadata(0, path);
                has_children = true;
            }
        }
    }

    return has_children;
}

void OrphanResourcesDialog::refresh() {
    HashMap<String, int> refs;
    _fill_owners(EditorFileSystem::get_singleton()->get_filesystem(), refs, nullptr);
    files->clear();
    TreeItem *root = files->create_item();
    _fill_owners(EditorFileSystem::get_singleton()->get_filesystem(), refs, root);
}

void OrphanResourcesDialog::show() {

    refresh();
    popup_centered_ratio();
}

void OrphanResourcesDialog::_find_to_delete(TreeItem *p_item, List<String> &paths) {

    while (p_item) {

        if (p_item->get_cell_mode(0) == TreeItem::CELL_MODE_CHECK && p_item->is_checked(0)) {
            paths.push_back(p_item->get_metadata(0));
        }

        if (p_item->get_children()) {
            _find_to_delete(p_item->get_children(), paths);
        }

        p_item = p_item->get_next();
    }
}

void OrphanResourcesDialog::_delete_confirm() {

    DirAccess *da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    for (List<String>::Element *E = paths.front(); E; E = E->next()) {

        da->remove(E->deref());
        EditorFileSystem::get_singleton()->update_file(E->deref());
    }
    memdelete(da);
    refresh();
}

void OrphanResourcesDialog::_button_pressed(Object *p_item, int p_column, int p_id) {

    TreeItem *ti = Object::cast_to<TreeItem>(p_item);

    String path = ti->get_metadata(0);
    dep_edit->edit(path);
}

void OrphanResourcesDialog::_bind_methods() {

    MethodBinder::bind_method(D_METHOD("_delete_confirm"), &OrphanResourcesDialog::_delete_confirm);
    MethodBinder::bind_method(D_METHOD("_button_pressed"), &OrphanResourcesDialog::_button_pressed);
}

OrphanResourcesDialog::OrphanResourcesDialog() {

    set_title(TTR("Orphan Resource Explorer"));
    delete_confirm = memnew(ConfirmationDialog);
    get_ok()->set_text(TTR("Delete"));
    add_child(delete_confirm);
    dep_edit = memnew(DependencyEditor);
    add_child(dep_edit);
    delete_confirm->connect("confirmed", this, "_delete_confirm");
    set_hide_on_ok(false);

    VBoxContainer *vbc = memnew(VBoxContainer);
    add_child(vbc);

    files = memnew(Tree);
    files->set_columns(2);
    files->set_column_titles_visible(true);
    files->set_column_min_width(1, 100);
    files->set_column_expand(0, true);
    files->set_column_expand(1, false);
    files->set_column_title(0, TTR("Resource"));
    files->set_column_title(1, TTR("Owns"));
    files->set_hide_root(true);
    vbc->add_margin_child(TTR("Resources Without Explicit Ownership:"), files, true);
    files->connect("button_pressed", this, "_button_pressed");
}