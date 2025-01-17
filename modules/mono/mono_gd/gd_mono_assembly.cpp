/*************************************************************************/
/*  gd_mono_assembly.cpp                                                 */
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

#include "gd_mono_assembly.h"

#include <mono/metadata/mono-debug.h>
#include <mono/metadata/tokentype.h>

#include "core/io/file_access_pack.h"
#include "core/list.h"
#include "core/os/file_access.h"
#include "core/os/os.h"
#include "core/project_settings.h"

#include "../godotsharp_dirs.h"
#include "gd_mono_cache.h"
#include "gd_mono_class.h"

#include "EASTL/deque.h"

Vector<String> GDMonoAssembly::search_dirs;

void GDMonoAssembly::fill_search_dirs(Vector<String> &r_search_dirs,StringView p_custom_config, StringView p_custom_bcl_dir) {

    String framework_dir;

    if (!p_custom_bcl_dir.empty()) {
        framework_dir = p_custom_bcl_dir;
    } else if (mono_assembly_getrootdir()) {
        framework_dir = PathUtils::plus_file(PathUtils::plus_file(mono_assembly_getrootdir(),"mono"),"4.5");
    }

    if (!framework_dir.empty()) {
        r_search_dirs.push_back(framework_dir);
        r_search_dirs.push_back(PathUtils::plus_file(framework_dir,"Facades"));
    }

#if !defined(TOOLS_ENABLED)
    String data_game_assemblies_dir = GodotSharpDirs::get_data_game_assemblies_dir();
    if (!data_game_assemblies_dir.empty()) {
        r_search_dirs.push_back(data_game_assemblies_dir);
    }
#endif

    if (p_custom_config.length()) {
        r_search_dirs.push_back(PathUtils::plus_file(GodotSharpDirs::get_res_temp_assemblies_base_dir(),p_custom_config));
    } else {
        r_search_dirs.push_back(GodotSharpDirs::get_res_temp_assemblies_dir());
    }

    if (p_custom_config.empty()) {
        r_search_dirs.push_back(GodotSharpDirs::get_res_assemblies_dir());
    } else {
        String api_config = p_custom_config == StringView("ExportRelease") ? "Release" : "Debug";
        r_search_dirs.push_back(PathUtils::plus_file(GodotSharpDirs::get_res_assemblies_base_dir(),api_config));
    }

    r_search_dirs.push_back(GodotSharpDirs::get_res_assemblies_base_dir());
    r_search_dirs.push_back(OS::get_singleton()->get_resource_dir());
    r_search_dirs.push_back(PathUtils::get_base_dir(OS::get_singleton()->get_executable_path()));
    r_search_dirs.push_back(PathUtils::get_base_dir(OS::get_singleton()->working_directory()));

#ifdef TOOLS_ENABLED
    r_search_dirs.push_back(GodotSharpDirs::get_data_editor_tools_dir());

    // For GodotTools to find the api assemblies
    r_search_dirs.push_back(PathUtils::plus_file(GodotSharpDirs::get_data_editor_prebuilt_api_dir(),"Debug"));
#endif
}

// This is how these assembly loading hooks work:
//
// - The 'search' hook checks if the assembly has already been loaded, to avoid loading again.
// - The 'preload' hook does the actual loading and is only called if the
//   'search' hook didn't find the assembly in the list of loaded assemblies.
// - The 'load' hook is called after the assembly has been loaded. Its job is to add the
//   assembly to the list of loaded assemblies so that the 'search' hook can look it up.

void GDMonoAssembly::assembly_load_hook(MonoAssembly *assembly, [[maybe_unused]] void *user_data) {

    StringView name = mono_assembly_name_get_name(mono_assembly_get_name(assembly));

    MonoImage *image = mono_assembly_get_image(assembly);

    GDMonoAssembly *gdassembly = memnew(GDMonoAssembly(name, image, assembly));

#ifdef GD_MONO_HOT_RELOAD
    //TODO: consider a case where mono_image_get_filename returns an unicode string?
    const char *path = mono_image_get_filename(image);
    if (FileAccess::exists(path))
        gdassembly->modified_time = FileAccess::get_modified_time(path);
#endif

    MonoDomain *domain = mono_domain_get();
    GDMono::get_singleton()->add_assembly(domain ? mono_domain_get_id(domain) : 0, gdassembly);
}

MonoAssembly *GDMonoAssembly::assembly_search_hook(MonoAssemblyName *aname, void *user_data) {
    return GDMonoAssembly::_search_hook(aname, user_data, false);
}

MonoAssembly *GDMonoAssembly::assembly_refonly_search_hook(MonoAssemblyName *aname, void *user_data) {
    return GDMonoAssembly::_search_hook(aname, user_data, true);
}

MonoAssembly *GDMonoAssembly::assembly_preload_hook(MonoAssemblyName *aname, char **assemblies_path, void *user_data) {
    return GDMonoAssembly::_preload_hook(aname, assemblies_path, user_data, false);
}

MonoAssembly *GDMonoAssembly::assembly_refonly_preload_hook(MonoAssemblyName *aname, char **assemblies_path, void *user_data) {
    return GDMonoAssembly::_preload_hook(aname, assemblies_path, user_data, true);
}

MonoAssembly *GDMonoAssembly::_search_hook(MonoAssemblyName *aname, [[maybe_unused]] void *user_data, bool refonly) {

    StringView name = mono_assembly_name_get_name(aname);
    bool has_extension = name.ends_with(".dll") || name.ends_with(".exe");

    GDMonoAssembly *loaded_asm = GDMono::get_singleton()->get_loaded_assembly(has_extension ? PathUtils::get_basename(name) : name);
    if (loaded_asm)
        return loaded_asm->get_assembly();

    return nullptr;
}

MonoAssembly *GDMonoAssembly::_preload_hook(MonoAssemblyName *aname, char **, [[maybe_unused]] void *user_data, bool refonly) {

    StringView name = mono_assembly_name_get_name(aname);
    return _load_assembly_search(name, aname, refonly, search_dirs);
}

MonoAssembly *GDMonoAssembly::_load_assembly_search(StringView p_name, MonoAssemblyName *p_aname, bool p_refonly, const Vector<String> &p_search_dirs) {

    MonoAssembly *res = nullptr;
    String path;

    bool has_extension = p_name.ends_with(".dll") || p_name.ends_with(".exe");

    for (int i = 0; i < p_search_dirs.size(); i++) {
        const String &search_dir = p_search_dirs[i];

        if (has_extension) {
            path = PathUtils::plus_file(search_dir,p_name);
            if (FileAccess::exists(path)) {
                res = _real_load_assembly_from(path, p_refonly, p_aname);
                if (res != nullptr)
                    return res;
            }
        } else {
            path = PathUtils::plus_file(search_dir,String(p_name) + ".dll");
            if (FileAccess::exists(path)) {
                res = _real_load_assembly_from(path, p_refonly, p_aname);
                if (res != nullptr)
                    return res;
            }

            path = PathUtils::plus_file(search_dir,String(p_name) + ".exe");
            if (FileAccess::exists(path)) {
                res = _real_load_assembly_from(path, p_refonly, p_aname);
                if (res != nullptr)
                    return res;
            }
        }
    }

    return nullptr;
}

String GDMonoAssembly::find_assembly(const String &p_name) {

    String path;

    bool has_extension = p_name.ends_with(".dll") || p_name.ends_with(".exe");

    for (int i = 0; i < search_dirs.size(); i++) {
        const String &search_dir = search_dirs[i];

        if (has_extension) {
            path = PathUtils::plus_file(search_dir,p_name);
            if (FileAccess::exists(path))
                return path;
        } else {
            path = PathUtils::plus_file(search_dir,p_name + ".dll");
            if (FileAccess::exists(path))
                return path;

            path = PathUtils::plus_file(search_dir,p_name + ".exe");
            if (FileAccess::exists(path))
                return path;
        }
    }

    return String();
}

void GDMonoAssembly::initialize() {

    fill_search_dirs(search_dirs);

    mono_install_assembly_search_hook(&assembly_search_hook, nullptr);
    mono_install_assembly_refonly_search_hook(&assembly_refonly_search_hook, nullptr);
    mono_install_assembly_preload_hook(&assembly_preload_hook, nullptr);
    mono_install_assembly_refonly_preload_hook(&assembly_refonly_preload_hook, nullptr);
    mono_install_assembly_load_hook(&assembly_load_hook, nullptr);
}

MonoAssembly *GDMonoAssembly::_real_load_assembly_from(StringView p_path, bool p_refonly, MonoAssemblyName *p_aname) {

    Vector<uint8_t> data = FileAccess::get_file_as_array(p_path);
    ERR_FAIL_COND_V_MSG(data.empty(), nullptr, "Could read the assembly in the specified location");

    String image_filename;

    // FIXME: globalize_path does not work on exported games
    image_filename = ProjectSettings::get_singleton()->globalize_path(p_path);

    MonoImageOpenStatus status = MONO_IMAGE_OK;

    MonoImage *image = mono_image_open_from_data_with_name(
            (char *)&data[0], data.size(),
            true, &status, p_refonly,
            image_filename.c_str());

    ERR_FAIL_COND_V_MSG(status != MONO_IMAGE_OK || !image, nullptr, "Failed to open assembly image from memory: '" + p_path + "'.");

    if (p_aname != nullptr) {
        // Check assembly version
        const MonoTableInfo *table = mono_image_get_table_info(image, MONO_TABLE_ASSEMBLY);

        ERR_FAIL_NULL_V(table, nullptr);

        if (mono_table_info_get_rows(table)) {
            uint32_t cols[MONO_ASSEMBLY_SIZE];
            mono_metadata_decode_row(table, 0, cols, MONO_ASSEMBLY_SIZE);

            // Not sure about .NET's policy. We will only ensure major and minor are equal, and ignore build and revision.
            uint16_t major = cols[MONO_ASSEMBLY_MAJOR_VERSION];
            uint16_t minor = cols[MONO_ASSEMBLY_MINOR_VERSION];

            uint16_t required_minor;
            uint16_t required_major = mono_assembly_name_get_version(p_aname, &required_minor, nullptr, nullptr);

            if (required_major != 0) {
                if (major != required_major && minor != required_minor) {
                    mono_image_close(image);
                    return nullptr;
                }
            }
        }
    }

#ifdef DEBUG_ENABLED
    Vector<uint8_t> pdb_data;
    String pdb_path = String(p_path) + ".pdb";

    if (!FileAccess::exists(pdb_path)) {
        pdb_path = PathUtils::get_base_dir(p_path) +"/"+ String(PathUtils::get_basename(p_path)) + ".pdb"; // without .dll

        if (!FileAccess::exists(pdb_path))
            goto no_pdb;
    }

    pdb_data = FileAccess::get_file_as_array(pdb_path);

    // mono_debug_close_image doesn't seem to be needed
    mono_debug_open_image_from_memory(image, &pdb_data[0], pdb_data.size());

no_pdb:

#endif
    bool need_manual_load_hook = mono_image_get_assembly(image) != nullptr; // Re-using an existing image with an assembly loaded

    status = MONO_IMAGE_OK;

    MonoAssembly *assembly = mono_assembly_load_from_full(image, image_filename.c_str(), &status, p_refonly);

    ERR_FAIL_COND_V_MSG(status != MONO_IMAGE_OK || !assembly, nullptr, "Failed to load assembly for image");

    if (need_manual_load_hook) {
        // For some reason if an assembly survived domain reloading (maybe because it's referenced somewhere else),
        // the mono internal search hook don't detect it, yet mono_image_open_from_data_with_name re-uses the image
        // and assembly, and mono_assembly_load_from_full doesn't call the load hook. We need to call it manually.
        String name(mono_assembly_name_get_name(mono_assembly_get_name(assembly)));
        bool has_extension = name.ends_with(".dll") || name.ends_with(".exe");
        GDMonoAssembly *loaded_asm = GDMono::get_singleton()->get_loaded_assembly(has_extension ? PathUtils::get_basename(name) : name);
        if (!loaded_asm)
            assembly_load_hook(assembly, nullptr);
    }

    // Decrement refcount which was previously incremented by mono_image_open_from_data_with_name
    mono_image_close(image);

    return assembly;
}

void GDMonoAssembly::unload() {

    ERR_FAIL_NULL(image); // Should not be called if already unloaded

    for (auto &E : cached_raw) {
        memdelete(E.second);
    }

    cached_classes.clear();
    cached_raw.clear();

    assembly = nullptr;
    image = nullptr;
}
String GDMonoAssembly::get_path() const {
    return String(mono_image_get_filename(image));
}

GDMonoClass *GDMonoAssembly::get_class(const StringName &p_namespace, const StringName &p_name) {

    ERR_FAIL_NULL_V(image, nullptr);

    ClassKey key(p_namespace, p_name);

    auto match = cached_classes.find(key);

    if (match!=cached_classes.end())
        return match->second;

    MonoClass *mono_class = mono_class_from_name(image, p_namespace.asCString(), p_name.asCString());

    if (!mono_class)
        return nullptr;

    GDMonoClass *wrapped_class = memnew(GDMonoClass(p_namespace, p_name, mono_class, this));

    cached_classes[key] = wrapped_class;
    cached_raw[mono_class] = wrapped_class;

    return wrapped_class;
}

GDMonoClass *GDMonoAssembly::get_class(MonoClass *p_mono_class) {

    ERR_FAIL_NULL_V(image, nullptr);

    HashMap<MonoClass *, GDMonoClass *>::iterator match = cached_raw.find(p_mono_class);

    if (match!=cached_raw.end())
        return match->second;

    StringName namespace_name(mono_class_get_namespace(p_mono_class));
    StringName class_name(mono_class_get_name(p_mono_class));

    GDMonoClass *wrapped_class = memnew(GDMonoClass(namespace_name, class_name, p_mono_class, this));

    cached_classes[ClassKey(namespace_name, class_name)] = wrapped_class;
    cached_raw[p_mono_class] = wrapped_class;

    return wrapped_class;
}

GDMonoClass *GDMonoAssembly::get_object_derived_class(const StringName &p_class) {

    GDMonoClass *match = nullptr;

    if (gdobject_class_cache_updated) {
        HashMap<StringName, GDMonoClass *>::iterator result = gdobject_class_cache.find(p_class);

        if (result!=gdobject_class_cache.end())
            match = result->second;
    } else {
        Dequeue<GDMonoClass *> nested_classes;

        int rows = mono_image_get_table_rows(image, MONO_TABLE_TYPEDEF);

        for (int i = 1; i < rows; i++) {
            MonoClass *mono_class = mono_class_get(image, (i + 1) | MONO_TOKEN_TYPE_DEF);

            if (!mono_class_is_assignable_from(CACHED_CLASS_RAW(GodotObject), mono_class)) {
                continue;
            }

            GDMonoClass *current = get_class(mono_class);

            if (!current) {
                continue;
            }

            nested_classes.push_back(current);

            if (!match && current->get_name() == p_class) {
                match = current;
            }

            while (!nested_classes.empty()) {
                GDMonoClass *current_nested = nested_classes.front();
                nested_classes.pop_front();

                void *iter = nullptr;

                while (true) {
                    MonoClass *raw_nested = mono_class_get_nested_types(current_nested->get_mono_ptr(), &iter);

                    if (!raw_nested) {
                        break;
                    }

                    GDMonoClass *nested_class = get_class(raw_nested);

                    if (nested_class) {
                        gdobject_class_cache.emplace(nested_class->get_name(), nested_class);
                        nested_classes.push_back(nested_class);
                    }
                }
            }

            gdobject_class_cache.emplace(current->get_name(), current);
        }

        gdobject_class_cache_updated = true;
    }

    return match;
}

GDMonoAssembly *GDMonoAssembly::load(StringView p_name, MonoAssemblyName *p_aname, bool p_refonly, const Vector<String> &p_search_dirs) {

    if (GDMono::get_singleton()->get_corlib_assembly() && (p_name == "mscorlib" || p_name == "mscorlib.dll"))
        return GDMono::get_singleton()->get_corlib_assembly();

    // We need to manually call the search hook in this case, as it won't be called in the next step
    MonoAssembly *assembly = mono_assembly_invoke_search_hook(p_aname);

    if (!assembly) {
        assembly = _load_assembly_search(p_name, p_aname, p_refonly, p_search_dirs);
        if (!assembly) {
            return nullptr;
        }
    }

    GDMonoAssembly *loaded_asm = GDMono::get_singleton()->get_loaded_assembly(p_name);
    ERR_FAIL_NULL_V_MSG(loaded_asm, nullptr, "Loaded assembly missing from table. Did we not receive the load hook?");
    ERR_FAIL_COND_V(loaded_asm->get_assembly() != assembly, nullptr);

    return loaded_asm;
}

GDMonoAssembly *GDMonoAssembly::load_from(StringView p_name, StringView p_path, bool p_refonly) {

    if (p_name == "mscorlib" || p_name == "mscorlib.dll")
        return GDMono::get_singleton()->get_corlib_assembly();

    // We need to manually call the search hook in this case, as it won't be called in the next step
    MonoAssemblyName *aname = mono_assembly_name_new(String(p_name).c_str());
    MonoAssembly *assembly = mono_assembly_invoke_search_hook(aname);
    mono_assembly_name_free(aname);
    mono_free(aname);

    if (!assembly) {
        assembly = _real_load_assembly_from(p_path, p_refonly);
        if (!assembly) {
            return nullptr;
        }
    }

    GDMonoAssembly *loaded_asm = GDMono::get_singleton()->get_loaded_assembly(p_name);
    ERR_FAIL_NULL_V_MSG(loaded_asm, nullptr, "Loaded assembly missing from table. Did we not receive the load hook?");

    return loaded_asm;
}

GDMonoAssembly::GDMonoAssembly(StringView p_name, MonoImage *p_image, MonoAssembly *p_assembly) :
        name(p_name),
        image(p_image),
        assembly(p_assembly),
        gdobject_class_cache_updated(false) {
}

GDMonoAssembly::~GDMonoAssembly() {

    if (image)
        unload();
}
