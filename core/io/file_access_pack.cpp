/*************************************************************************/
/*  file_access_pack.cpp                                                 */
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

#include "file_access_pack.h"

#include "core/version.h"

#include <cstdio>

Error PackedData::add_pack(const String &p_path) {

    for (int i = 0; i < sources.size(); i++) {

        if (sources[i]->try_open_pack(p_path)) {

            return OK;
        }
    }

    return ERR_FILE_UNRECOGNIZED;
};

void PackedData::add_path(const String &pkg_path, const String &path, uint64_t ofs, uint64_t size, const uint8_t *p_md5, PackSourceInterface *p_src) {

    PathMD5 pmd5(StringUtils::md5_buffer(path));
    //printf("adding path %ls, %lli, %lli\n", path.c_str(), pmd5.a, pmd5.b);

    bool exists = files.contains(pmd5);

    PackedDataFile pf;
    pf.pack = pkg_path;
    pf.offset = ofs;
    pf.size = size;
    for (int i = 0; i < 16; i++)
        pf.md5[i] = p_md5[i];
    pf.src = p_src;

    files[pmd5] = pf;

    if (!exists) {
        //search for dir
        String p = StringUtils::replace_first(path,"res://", "");
        PackedDir *cd = root;

        if (StringUtils::contains(p,"/")) { //in a subdir

            Vector<String> ds = StringUtils::split(PathUtils::get_base_dir(p),"/");

            for (int j = 0; j < ds.size(); j++) {

                if (!cd->subdirs.contains(ds[j])) {

                    PackedDir *pd = memnew(PackedDir);
                    pd->name = ds[j];
                    pd->parent = cd;
                    cd->subdirs[pd->name] = pd;
                    cd = pd;
                } else {
                    cd = cd->subdirs[ds[j]];
                }
            }
        }
        String filename = PathUtils::get_file(path);
        // Don't add as a file if the path points to a directory
        if (!filename.empty()) {
            cd->files.insert(filename);
        }
    }
}

void PackedData::add_pack_source(PackSourceInterface *p_source) {

    if (p_source != nullptr) {
        sources.push_back(p_source);
    }
}
/**
 * @brief PackedData::remove_pack_source will remove a source of pack files from available list.
 * @param p_source will be removed from the internal list, but will not be freed.
 */
void PackedData::remove_pack_source(PackSourceInterface *p_source)
{
    if (p_source != nullptr) {
        sources.erase(p_source);
    }

};

PackedData *PackedData::singleton = nullptr;

PackedData::PackedData() {

    singleton = this;
    root = memnew(PackedDir);
    root->parent = nullptr;
    disabled = false;
}

void PackedData::_free_packed_dirs(PackedDir *p_dir) {

    for (eastl::pair<const String, PackedDir *> &E : p_dir->subdirs)
        _free_packed_dirs(E.second);
    memdelete(p_dir);
}

PackedData::~PackedData() {

    //TODO: inform all sources that PackedData interface is being deleted ?
    sources.clear();
    _free_packed_dirs(root);
}


//////////////////////////////////////////////////////////////////////////////////
// DIR ACCESS
//////////////////////////////////////////////////////////////////////////////////

Error DirAccessPack::list_dir_begin() {

    list_dirs.clear();
    list_files.clear();

    for (eastl::pair<const String, PackedData::PackedDir *> &E : current->subdirs) {

        list_dirs.push_back(E.first);
    }

    for (const String &E : current->files) {

        list_files.push_back(E);
    }

    return OK;
}

String DirAccessPack::get_next() {

    if (!list_dirs.empty()) {
        cdir = true;
        String d = list_dirs.front()->deref();
        list_dirs.pop_front();
        return d;
    } else if (!list_files.empty()) {
        cdir = false;
        String f = list_files.front()->deref();
        list_files.pop_front();
        return f;
    } else {
        return String();
    }

}
bool DirAccessPack::current_is_dir() const {

    return cdir;
}
bool DirAccessPack::current_is_hidden() const {

    return false;
}
void DirAccessPack::list_dir_end() {

    list_dirs.clear();
    list_files.clear();
}

int DirAccessPack::get_drive_count() {

    return 0;
}
String DirAccessPack::get_drive(int p_drive) {

    return "";
}

Error DirAccessPack::change_dir(String p_dir) {

    String nd = PathUtils::from_native_path(p_dir);
    bool absolute = false;
    if (StringUtils::begins_with(nd,"res://")) {
        nd = StringUtils::replace_first(nd,"res://", "");
        absolute = true;
    }

    nd = PathUtils::simplify_path(nd);

    if (nd.empty())
        nd = ".";

    if (StringUtils::begins_with(nd,"/")) {
        nd = StringUtils::replace_first(nd,"/", "");
        absolute = true;
    }

    Vector<String> paths = StringUtils::split(nd,"/");

    PackedData::PackedDir *pd;

    if (absolute)
        pd = PackedData::get_singleton()->root;
    else
        pd = current;

    for (int i = 0; i < paths.size(); i++) {

        String p = paths[i];
        if (p == ".") {
            continue;
        } else if (p == "..") {
            if (pd->parent) {
                pd = pd->parent;
            }
        } else if (pd->subdirs.contains(p)) {

            pd = pd->subdirs[p];

        } else {

            return ERR_INVALID_PARAMETER;
        }
    }

    current = pd;

    return OK;
}

String DirAccessPack::get_current_dir() {

    PackedData::PackedDir *pd = current;
    String p = current->name;

    while (pd->parent) {
        pd = pd->parent;
        p = PathUtils::plus_file(pd->name,p);
    }

    return "res://" + p;
}

bool DirAccessPack::file_exists(String p_file) {
    p_file = fix_path(p_file);

    return current->files.contains(p_file);
}

bool DirAccessPack::dir_exists(String p_dir) {
    p_dir = fix_path(p_dir);

    return current->subdirs.contains(p_dir);
}

Error DirAccessPack::make_dir(String p_dir) {

    return ERR_UNAVAILABLE;
}

Error DirAccessPack::rename(String p_from, String p_to) {

    return ERR_UNAVAILABLE;
}
Error DirAccessPack::remove(String p_name) {

    return ERR_UNAVAILABLE;
}

size_t DirAccessPack::get_space_left() {

    return 0;
}

String DirAccessPack::get_filesystem_type() const {
    return "PCK";
}

DirAccessPack::DirAccessPack() {

    current = PackedData::get_singleton()->root;
    cdir = false;
}

DirAccessPack::~DirAccessPack() {}