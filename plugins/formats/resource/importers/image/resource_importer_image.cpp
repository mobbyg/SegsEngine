/*************************************************************************/
/*  resource_importer_image.cpp                                          */
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

#include "resource_importer_image.h"

#include "core/io/image_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/file_access.h"
#include "core/string_utils.h"
#include "EASTL/unique_ptr.h"

const char *ResourceImporterImage::get_importer_name() const {

    return "image";
}

const char *ResourceImporterImage::get_visible_name() const {

    return "Image";
}
void ResourceImporterImage::get_recognized_extensions(Vector<String> &p_extensions) const {

    ImageLoader::get_recognized_extensions(p_extensions);
}

StringName ResourceImporterImage::get_save_extension() const {
    return "image";
}

StringName ResourceImporterImage::get_resource_type() const {

    return "Image";
}

bool ResourceImporterImage::get_option_visibility(const StringName &p_option, const HashMap<StringName, Variant> &p_options) const {

    return true;
}

int ResourceImporterImage::get_preset_count() const {
    return 0;
}
StringName ResourceImporterImage::get_preset_name(int p_idx) const {

    return StringName();
}

void ResourceImporterImage::get_import_options(Vector<ImportOption> *r_options, int p_preset) const {
}

Error ResourceImporterImage::import(StringView p_source_file, StringView p_save_path, const HashMap<StringName, Variant> &p_options, Vector<String> &r_missing_deps,
                                    Vector<String> *r_platform_variants, Vector<String> *r_gen_files, Variant *r_metadata) {

    FileAccess *f = FileAccess::open(p_source_file, FileAccess::READ);

    ERR_FAIL_COND_V_MSG(!f, ERR_CANT_OPEN, "Cannot open file from path '" + p_source_file + "'.");

    uint64_t len = f->get_len();
    auto data = eastl::make_unique<uint8_t[]>(len);

    f->get_buffer(data.get(), len);

    memdelete(f);

    f = FileAccess::open(String(p_save_path) + ".image", FileAccess::WRITE);
    ERR_FAIL_COND_V_MSG(!f, ERR_CANT_CREATE, "Cannot create file in path '" + p_save_path + ".image'.");

    //save the header GDIM
    const uint8_t header[4] = { 'G', 'D', 'I', 'M' };
    f->store_buffer(header, 4);
    //SAVE the extension (so it can be recognized by the loader later
    f->store_pascal_string(StringUtils::to_lower(PathUtils::get_extension(p_source_file)));
    //SAVE the actual image
    f->store_buffer(data.get(), len);

    memdelete(f);

    return OK;
}

ResourceImporterImage::ResourceImporterImage() = default;
