/*************************************************************************/
/*  image.cpp                                                            */
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

#include "image.h"

#include "core/dictionary.h"
#include "core/image_enum_casters.h"
#include "core/io/image_loader.h"
#include "core/io/image_saver.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/method_bind.h"
#include "core/plugin_interfaces/ImageLoaderInterface.h"
#include "core/plugin_interfaces/PluginDeclarations.h"
#include "core/print_string.h"
#include "core/resource/resource_manager.h"
#include "plugins/plugin_registry.h"

#include "EASTL/array.h"
#include "thirdparty/misc/hq2x.h"

#include <cstdio>

namespace {
static eastl::array<ImageCodecInterface *, COMPRESS_MAX> s_codecs;

static void average_4_uint8(
        uint8_t &p_out, const uint8_t &p_a, const uint8_t &p_b, const uint8_t &p_c, const uint8_t &p_d) {
    p_out = static_cast<uint8_t>((p_a + p_b + p_c + p_d + 2) >> 2);
}

static void average_4_float(float &p_out, const float &p_a, const float &p_b, const float &p_c, const float &p_d) {
    p_out = (p_a + p_b + p_c + p_d) * 0.25f;
}

static void average_4_half(
        uint16_t &p_out, const uint16_t &p_a, const uint16_t &p_b, const uint16_t &p_c, const uint16_t &p_d) {
    p_out = Math::make_half_float((Math::half_to_float(p_a) + Math::half_to_float(p_b) + Math::half_to_float(p_c) +
                                          Math::half_to_float(p_d)) *
                                  0.25f);
}

static void average_4_rgbe9995(
        uint32_t &p_out, const uint32_t &p_a, const uint32_t &p_b, const uint32_t &p_c, const uint32_t &p_d) {
    p_out = ((Color::from_rgbe9995(p_a) + Color::from_rgbe9995(p_b) + Color::from_rgbe9995(p_c) +
                     Color::from_rgbe9995(p_d)) *
             0.25f)
                    .to_rgbe9995();
}

static void renormalize_uint8(uint8_t *p_rgb) {
    Vector3 n(p_rgb[0] / 255.0f, p_rgb[1] / 255.0f, p_rgb[2] / 255.0f);
    n *= 2.0;
    n -= Vector3(1, 1, 1);
    n.normalize();
    n += Vector3(1, 1, 1);
    n *= 0.5;
    n *= 255;
    p_rgb[0] = (uint8_t)CLAMP<int>(n.x, 0, 255);
    p_rgb[1] = (uint8_t)CLAMP<int>(n.y, 0, 255);
    p_rgb[2] = (uint8_t)CLAMP<int>(n.z, 0, 255);
}

static void renormalize_float(float *p_rgb) {
    Vector3 n(p_rgb[0], p_rgb[1], p_rgb[2]);
    n.normalize();
    p_rgb[0] = n.x;
    p_rgb[1] = n.y;
    p_rgb[2] = n.z;
}

static void renormalize_half(uint16_t *p_rgb) {
    Vector3 n(Math::half_to_float(p_rgb[0]), Math::half_to_float(p_rgb[1]), Math::half_to_float(p_rgb[2]));
    n.normalize();
    p_rgb[0] = Math::make_half_float(n.x);
    p_rgb[1] = Math::make_half_float(n.y);
    p_rgb[2] = Math::make_half_float(n.z);
}

static void renormalize_rgbe9995(uint32_t *p_rgb) {
    // Never used
}
static int _get_dst_image_size(int p_width, int p_height, Image::Format p_format, int &r_mipmaps, int p_mipmaps = -1,
        int *r_mm_width = nullptr, int *r_mm_height = nullptr) {
    int size = 0;
    int w = p_width;
    int h = p_height;
    int mm = 0;

    int pixsize = Image::get_format_pixel_size(p_format);
    int pixshift = Image::get_format_pixel_rshift(p_format);
    int block = Image::get_format_block_size(p_format);
    // technically, you can still compress up to 1 px no matter the format, so commenting this
    // int minw, minh;
    // get_format_min_pixel_size(p_format, minw, minh);
    int minw = 1, minh = 1;

    while (true) {
        int bw = w % block != 0 ? w + (block - w % block) : w;
        int bh = h % block != 0 ? h + (block - h % block) : h;

        int s = bw * bh;

        s *= pixsize;
        s >>= pixshift;

        size += s;

        if (r_mm_width) {
            *r_mm_width = bw;
        }
        if (r_mm_height) {
            *r_mm_height = bh;
        }

        if (p_mipmaps >= 0 && mm == p_mipmaps) {
            break;
        }

        if (p_mipmaps >= 0) {
            w = M_MAX(minw, w >> 1);
            h = M_MAX(minh, h >> 1);
        } else {
            if (w == minw && h == minh) {
                break;
            }
            w = M_MAX(minw, w >> 1);
            h = M_MAX(minh, h >> 1);
        }
        mm++;
    }

    r_mipmaps = mm;
    return size;
}
struct CodecPluginResolver final : public ResolverInterface {
    bool new_plugin_detected(QObject *ob, const QJsonObject &, const char *) final {
        bool res = false;
        auto interface = qobject_cast<ImageCodecInterface *>(ob);
        if (interface) {
            print_line(String("Adding image codec plugin:") + ob->metaObject()->className());
            Vector<int> modes;
            interface->fill_modes(modes);
            for (int m : modes) {
                s_codecs.at(m) = interface;
            }
            res = true;
        }
        return res;
    }
    void plugin_removed(QObject *ob) final {
        auto interface = qobject_cast<ImageCodecInterface *>(ob);
        if (interface) {
            print_line(String("Removing image codec plugin:") + ob->metaObject()->className());
            Vector<int> modes;
            interface->fill_modes(modes);
            for (int m : modes) {
                s_codecs.at(m) = nullptr;
            }
        }
    }
    ~CodecPluginResolver() final = default;
};
} // end of anonymous namespace

IMPL_GDCLASS(Image)

namespace {
constexpr const char *format_names[ImageData::FORMAT_MAX] = {
    "Lum8", // luminance
    "LumAlpha8", // luminance-alpha
    "Red8",
    "RedGreen",
    "RGB8",
    "RGBA8",
    "RGBA4444",
    "RGBA5551",
    "RFloat", // float
    "RGFloat",
    "RGBFloat",
    "RGBAFloat",
    "RHalf", // half float
    "RGHalf",
    "RGBHalf",
    "RGBAHalf",
    "RGBE9995",
    "DXT1 RGB8", // s3tc
    "DXT3 RGBA8",
    "DXT5 RGBA8",
    "RGTC Red8",
    "RGTC RedGreen8",
    "BPTC_RGBA",
    "BPTC_RGBF",
    "BPTC_RGBFU",
    "FORMAT_DXT5_RA_AS_RG",

};
}

Error Image::save_png_func(StringView p_path, const Ref<Image> &p_img) {
    Vector<uint8_t> buffer;
    Ref<Image> source_image = prepareForPngStorage(p_img);
    ERR_FAIL_COND_V(source_image == nullptr, FAILED);
    Error err = ImageSaver::save_image("png", source_image, buffer);
    ERR_FAIL_COND_V(err, err);
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);
    ERR_FAIL_COND_V(err, err);

    file->store_buffer(buffer.data(), buffer.size());
    if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
        memdelete(file);
        return ERR_CANT_CREATE;
    }

    file->close();
    memdelete(file);
    return OK;
}

Error Image::save_exr_func(StringView p_path, const Ref<Image> &source_image, bool greyscale) {
    Vector<uint8_t> buffer;
    ERR_FAIL_COND_V(source_image == nullptr, FAILED);
    Error err = ImageSaver::save_image("exr", source_image, buffer);
    ERR_FAIL_COND_V(err, err);
    FileAccess *file = FileAccess::open(p_path, FileAccess::WRITE, &err);
    ERR_FAIL_COND_V(err, err);

    file->store_buffer(buffer.data(), buffer.size());
    if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
        memdelete(file);
        return ERR_CANT_CREATE;
    }

    file->close();
    memdelete(file);
    return OK;
}

Error Image::compress_image(Image *img, CompressParams p) {
    ERR_FAIL_COND_V(s_codecs.at(int(p.mode)) == nullptr, ERR_UNAVAILABLE);
    return s_codecs.at(int(p.mode))->compress_image(img, p);
}
Error Image::decompress_image(Image *img, CompressParams p) {
    ERR_FAIL_COND_V(s_codecs.at(int(p.mode)) == nullptr, FAILED);
    return s_codecs.at(int(p.mode))->decompress_image(img);
}

void Image::_put_pixelb(int p_x, int p_y, uint32_t p_pixel_size, uint8_t *p_data, const uint8_t *p_pixel) {
    uint32_t ofs = (p_y * width + p_x) * p_pixel_size;
    memcpy(p_data + ofs, p_pixel, p_pixel_size);
}

void Image::_get_pixelb(int p_x, int p_y, uint32_t p_pixel_size, const uint8_t *p_data, uint8_t *p_pixel) {
    uint32_t ofs = (p_y * width + p_x) * p_pixel_size;
    memcpy(p_pixel, p_data + ofs, p_pixel_size);
}

int Image::get_format_pixel_size(Format p_format) {
    switch (p_format) {
        case FORMAT_L8:
            return 1; // luminance
        case FORMAT_LA8:
            return 2; // luminance-alpha
        case FORMAT_R8:
            return 1;
        case FORMAT_RG8:
            return 2;
        case FORMAT_RGB8:
            return 3;
        case FORMAT_RGBA8:
            return 4;
        case FORMAT_RGBA4444:
            return 2;
        case FORMAT_RGB565:
            return 2;
        case FORMAT_RF:
            return 4; // float
        case FORMAT_RGF:
            return 8;
        case FORMAT_RGBF:
            return 12;
        case FORMAT_RGBAF:
            return 16;
        case FORMAT_RH:
            return 2; // half float
        case FORMAT_RGH:
            return 4;
        case FORMAT_RGBH:
            return 6;
        case FORMAT_RGBAH:
            return 8;
        case FORMAT_RGBE9995:
            return 4;
        case FORMAT_DXT1:
            return 1; // s3tc bc1
        case FORMAT_DXT3:
            return 1; // bc2
        case FORMAT_DXT5:
            return 1; // bc3
        case FORMAT_RGTC_R:
            return 1; // bc4
        case FORMAT_RGTC_RG:
            return 1; // bc5
        case FORMAT_BPTC_RGBA:
            return 1; // btpc bc6h
        case FORMAT_BPTC_RGBF:
            return 1; // float /
        case FORMAT_BPTC_RGBFU:
            return 1; // unsigned float
        case FORMAT_DXT5_RA_AS_RG:
            return 1;
        case FORMAT_MAX: {
        }
    }
    return 0;
}

void Image::get_format_min_pixel_size(Format p_format, int &r_w, int &r_h) {
    switch (p_format) {
        case FORMAT_DXT1: // s3tc bc1
        case FORMAT_DXT3: // bc2
        case FORMAT_DXT5: // bc3
        case FORMAT_RGTC_R: // bc4
        case FORMAT_RGTC_RG: // bc5
        case FORMAT_BPTC_RGBA:
        case FORMAT_BPTC_RGBF:
        case FORMAT_BPTC_RGBFU:
        case FORMAT_DXT5_RA_AS_RG: {
            r_w = 4;
            r_h = 4;

        } break;

        default: {
            r_w = 1;
            r_h = 1;
        } break;
    }
}

int Image::get_format_pixel_rshift(Format p_format) {
    if (p_format == FORMAT_DXT1 || p_format == FORMAT_RGTC_R) {
        return 1;
    } else {
        return 0;
    }
}

int Image::get_format_block_size(Format p_format) {
    switch (p_format) {
        case FORMAT_DXT1: // s3tc bc1
        case FORMAT_DXT3: // bc2
        case FORMAT_DXT5: // bc3
        case FORMAT_RGTC_R: // bc4
        case FORMAT_RGTC_RG: { // bc5

            return 4;
        }
        case FORMAT_BPTC_RGBA:
        case FORMAT_BPTC_RGBF:
        case FORMAT_BPTC_RGBFU: {
            return 4;
        }
        case FORMAT_DXT5_RA_AS_RG: { // used to make basis universal happy
            return 4;
        }
        default: {
        }
    }

    return 1;
}

void Image::_get_mipmap_offset_and_size(int p_mipmap, int &r_offset, int &r_width, int &r_height) const {
    int w = width;
    int h = height;
    int ofs = 0;

    int pixel_size = get_format_pixel_size(format);
    int pixel_rshift = get_format_pixel_rshift(format);
    int block = get_format_block_size(format);
    int minw, minh;
    get_format_min_pixel_size(format, minw, minh);

    for (int i = 0; i < p_mipmap; i++) {
        int bw = w % block != 0 ? w + (block - w % block) : w;
        int bh = h % block != 0 ? h + (block - h % block) : h;

        int s = bw * bh;

        s *= pixel_size;
        s >>= pixel_rshift;
        ofs += s;
        w = M_MAX(minw, w >> 1);
        h = M_MAX(minh, h >> 1);
    }

    r_offset = ofs;
    r_width = w;
    r_height = h;
}

int Image::get_mipmap_offset(int p_mipmap) const {
    ERR_FAIL_INDEX_V(p_mipmap, get_mipmap_count() + 1, -1);

    int ofs, w, h;
    _get_mipmap_offset_and_size(p_mipmap, ofs, w, h);
    return ofs;
}
int Image::get_mipmap_byte_size(int p_mipmap) const {
    ERR_FAIL_INDEX_V(p_mipmap, get_mipmap_count() + 1, -1);

    int ofs, w, h;
    _get_mipmap_offset_and_size(p_mipmap, ofs, w, h);
    int ofs2;
    _get_mipmap_offset_and_size(p_mipmap + 1, ofs2, w, h);
    return ofs2 - ofs;
}
void Image::get_mipmap_offset_and_size(int p_mipmap, int &r_ofs, int &r_size) const {
    int ofs, w, h;
    _get_mipmap_offset_and_size(p_mipmap, ofs, w, h);
    int ofs2;
    _get_mipmap_offset_and_size(p_mipmap + 1, ofs2, w, h);
    r_ofs = ofs;
    r_size = ofs2 - ofs;
}

void Image::get_mipmap_offset_size_and_dimensions(int p_mipmap, int &r_ofs, int &r_size, int &w, int &h) const {
    int ofs;
    _get_mipmap_offset_and_size(p_mipmap, ofs, w, h);
    int ofs2, w2, h2;
    _get_mipmap_offset_and_size(p_mipmap + 1, ofs2, w2, h2);
    r_ofs = ofs;
    r_size = ofs2 - ofs;
}

int Image::get_width() const {
    return width;
}

int Image::get_height() const {
    return height;
}

Vector2 Image::get_size() const {
    return Vector2(width, height);
}

int Image::get_mipmap_count() const {
    if (mipmaps) {
        return get_image_required_mipmaps(width, height, format);
    } else {
        return 0;
    }
}

// using template generates perfectly optimized code due to constant expression reduction and unused variable removal
// present in all compilers
template <uint32_t read_bytes, bool read_alpha, uint32_t write_bytes, bool write_alpha, bool read_gray, bool write_gray>
static void _convert(int p_width, int p_height, const uint8_t *p_src, uint8_t *p_dst) {
    uint32_t max_bytes = M_MAX(read_bytes, write_bytes);

    for (int y = 0; y < p_height; y++) {
        for (int x = 0; x < p_width; x++) {
            const uint8_t *rofs = &p_src[((y * p_width) + x) * (read_bytes + (read_alpha ? 1 : 0))];
            uint8_t *wofs = &p_dst[((y * p_width) + x) * (write_bytes + (write_alpha ? 1 : 0))];

            uint8_t rgba[4] = { 0, 0, 0, 255 };

            if (read_gray) {
                rgba[0] = rofs[0];
                rgba[1] = rofs[0];
                rgba[2] = rofs[0];
            } else {
                for (uint32_t i = 0; i < max_bytes; i++) {
                    rgba[i] = (i < read_bytes) ? rofs[i] : 0;
                }
            }

            if (read_alpha || write_alpha) {
                rgba[3] = read_alpha ? rofs[read_bytes] : 255;
            }

            if (write_gray) {
                // TODO: not correct grayscale, should use fixed point version of actual weights
                wofs[0] = uint8_t((uint16_t(rgba[0]) + uint16_t(rgba[1]) + uint16_t(rgba[2])) / 3);
            } else {
                for (uint32_t i = 0; i < write_bytes; i++) {
                    wofs[i] = rgba[i];
                }
            }

            if (write_alpha) {
                wofs[write_bytes] = rgba[3];
            }
        }
    }
}

void Image::convert(Format p_new_format) {
    if (data.size() == 0) {
        return;
    }

    if (p_new_format == format) {
        return;
    }

    ERR_FAIL_COND_MSG(write_lock.ptr(), "Cannot convert image when it is locked.");

    if (format > FORMAT_RGBE9995 || p_new_format > FORMAT_RGBE9995) {
        ERR_FAIL_MSG("Cannot convert to <-> from compressed formats. Use compress() and decompress() instead.");

    } else if (format > FORMAT_RGBA8 || p_new_format > FORMAT_RGBA8) {
        // use put/set pixel which is slower but works with non byte formats
        Image new_img(width, height, false, p_new_format);
        lock();
        new_img.lock();

        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {
                new_img.set_pixel(i, j, get_pixel(i, j));
            }
        }

        unlock();
        new_img.unlock();

        if (has_mipmaps()) {
            new_img.generate_mipmaps();
        }

        _copy_internals_from(new_img);

        return;
    }

    Image new_img(width, height, false, p_new_format);

    PoolVector<uint8_t>::Read r = data.read();
    PoolVector<uint8_t>::Write w = new_img.data.write();

    const uint8_t *rptr = r.ptr();
    uint8_t *wptr = w.ptr();

    int conversion_type = format | p_new_format << 8;

    switch (conversion_type) {
        case FORMAT_L8 | (FORMAT_LA8 << 8):
            _convert<1, false, 1, true, true, true>(width, height, rptr, wptr);
            break;
        case FORMAT_L8 | (FORMAT_R8 << 8):
            _convert<1, false, 1, false, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_L8 | (FORMAT_RG8 << 8):
            _convert<1, false, 2, false, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_L8 | (FORMAT_RGB8 << 8):
            _convert<1, false, 3, false, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_L8 | (FORMAT_RGBA8 << 8):
            _convert<1, false, 3, true, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_LA8 | (FORMAT_L8 << 8):
            _convert<1, true, 1, false, true, true>(width, height, rptr, wptr);
            break;
        case FORMAT_LA8 | (FORMAT_R8 << 8):
            _convert<1, true, 1, false, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_LA8 | (FORMAT_RG8 << 8):
            _convert<1, true, 2, false, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_LA8 | (FORMAT_RGB8 << 8):
            _convert<1, true, 3, false, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_LA8 | (FORMAT_RGBA8 << 8):
            _convert<1, true, 3, true, true, false>(width, height, rptr, wptr);
            break;
        case FORMAT_R8 | (FORMAT_L8 << 8):
            _convert<1, false, 1, false, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_R8 | (FORMAT_LA8 << 8):
            _convert<1, false, 1, true, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_R8 | (FORMAT_RG8 << 8):
            _convert<1, false, 2, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_R8 | (FORMAT_RGB8 << 8):
            _convert<1, false, 3, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_R8 | (FORMAT_RGBA8 << 8):
            _convert<1, false, 3, true, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RG8 | (FORMAT_L8 << 8):
            _convert<2, false, 1, false, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_RG8 | (FORMAT_LA8 << 8):
            _convert<2, false, 1, true, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_RG8 | (FORMAT_R8 << 8):
            _convert<2, false, 1, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RG8 | (FORMAT_RGB8 << 8):
            _convert<2, false, 3, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RG8 | (FORMAT_RGBA8 << 8):
            _convert<2, false, 3, true, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RGB8 | (FORMAT_L8 << 8):
            _convert<3, false, 1, false, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_RGB8 | (FORMAT_LA8 << 8):
            _convert<3, false, 1, true, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_RGB8 | (FORMAT_R8 << 8):
            _convert<3, false, 1, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RGB8 | (FORMAT_RG8 << 8):
            _convert<3, false, 2, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RGB8 | (FORMAT_RGBA8 << 8):
            _convert<3, false, 3, true, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RGBA8 | (FORMAT_L8 << 8):
            _convert<3, true, 1, false, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_RGBA8 | (FORMAT_LA8 << 8):
            _convert<3, true, 1, true, false, true>(width, height, rptr, wptr);
            break;
        case FORMAT_RGBA8 | (FORMAT_R8 << 8):
            _convert<3, true, 1, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RGBA8 | (FORMAT_RG8 << 8):
            _convert<3, true, 2, false, false, false>(width, height, rptr, wptr);
            break;
        case FORMAT_RGBA8 | (FORMAT_RGB8 << 8):
            _convert<3, true, 3, false, false, false>(width, height, rptr, wptr);
            break;
    }

    r.release();
    w.release();

    bool gen_mipmaps = mipmaps;

    _copy_internals_from(new_img);

    if (gen_mipmaps) {
        generate_mipmaps();
    }
}

Image::Format Image::get_format() const {
    return format;
}

static double _bicubic_interp_kernel(double x) {
    x = ABS(x);

    double bc = 0;

    if (x <= 1) {
        bc = (1.5 * x - 2.5) * x * x + 1;
    } else if (x < 2) {
        bc = ((-0.5 * x + 2.5) * x - 4) * x + 2;
    }

    return bc;
}

template <int CC, class T>
static void _scale_cubic(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width,
        uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
    // get source image size
    int width = p_src_width;
    int height = p_src_height;
    double xfac = (double)width / p_dst_width;
    double yfac = (double)height / p_dst_height;
    // coordinates of source points and coefficients
    double ox, oy, dx, dy, k1, k2;
    int ox1, oy1, ox2, oy2;
    // destination pixel values
    // width and height decreased by 1
    int ymax = height - 1;
    int xmax = width - 1;
    // temporary pointer

    for (uint32_t y = 0; y < p_dst_height; y++) {
        // Y coordinates
        oy = (double)y * yfac - 0.5f;
        oy1 = (int)oy;
        dy = oy - (double)oy1;

        for (uint32_t x = 0; x < p_dst_width; x++) {
            // X coordinates
            ox = (double)x * xfac - 0.5f;
            ox1 = (int)ox;
            dx = ox - (double)ox1;

            // initial pixel value

            T *__restrict dst = ((T *)p_dst) + (y * p_dst_width + x) * CC;

            double color[CC];
            for (int i = 0; i < CC; i++) {
                color[i] = 0;
            }

            for (int n = -1; n < 3; n++) {
                // get Y coefficient
                k1 = _bicubic_interp_kernel(dy - (double)n);

                oy2 = oy1 + n;
                if (oy2 < 0) {
                    oy2 = 0;
                }
                if (oy2 > ymax) {
                    oy2 = ymax;
                }

                for (int m = -1; m < 3; m++) {
                    // get X coefficient
                    k2 = k1 * _bicubic_interp_kernel((double)m - dx);

                    ox2 = ox1 + m;
                    if (ox2 < 0) {
                        ox2 = 0;
                    }
                    if (ox2 > xmax) {
                        ox2 = xmax;
                    }

                    // get pixel of original image
                    const T *__restrict p = ((T *)p_src) + (oy2 * p_src_width + ox2) * CC;

                    for (int i = 0; i < CC; i++) {
                        if (sizeof(T) == 2) { // half float
                            color[i] = Math::half_to_float(p[i]);
                        } else {
                            color[i] += p[i] * k2;
                        }
                    }
                }
            }

            for (int i = 0; i < CC; i++) {
                if (sizeof(T) == 1) { // byte
                    dst[i] = CLAMP(Math::fast_ftoi(color[i]), 0, 255);
                } else if (sizeof(T) == 2) { // half float
                    dst[i] = Math::make_half_float(color[i]);
                } else {
                    dst[i] = color[i];
                }
            }
        }
    }
}

template <int CC, class T>
static void _scale_bilinear(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width,
        uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
    enum {
        FRAC_BITS = 8,
        FRAC_LEN = (1 << FRAC_BITS),
        FRAC_MASK = FRAC_LEN - 1

    };

    for (uint32_t i = 0; i < p_dst_height; i++) {
        uint32_t src_yofs_up_fp = (i * p_src_height * FRAC_LEN / p_dst_height);
        uint32_t src_yofs_frac = src_yofs_up_fp & FRAC_MASK;
        uint32_t src_yofs_up = src_yofs_up_fp >> FRAC_BITS;

        uint32_t src_yofs_down = (i + 1) * p_src_height / p_dst_height;
        if (src_yofs_down >= p_src_height) {
            src_yofs_down = p_src_height - 1;
        }

        // src_yofs_up*=CC;
        // src_yofs_down*=CC;

        uint32_t y_ofs_up = src_yofs_up * p_src_width * CC;
        uint32_t y_ofs_down = src_yofs_down * p_src_width * CC;

        for (uint32_t j = 0; j < p_dst_width; j++) {
            uint32_t src_xofs_left_fp = (j * p_src_width * FRAC_LEN / p_dst_width);
            uint32_t src_xofs_frac = src_xofs_left_fp & FRAC_MASK;
            uint32_t src_xofs_left = src_xofs_left_fp >> FRAC_BITS;
            uint32_t src_xofs_right = (j + 1) * p_src_width / p_dst_width;
            if (src_xofs_right >= p_src_width) {
                src_xofs_right = p_src_width - 1;
            }

            src_xofs_left *= CC;
            src_xofs_right *= CC;

            for (uint32_t l = 0; l < CC; l++) {
                if (sizeof(T) == 1) { // uint8
                    uint32_t p00 = p_src[y_ofs_up + src_xofs_left + l] << FRAC_BITS;
                    uint32_t p10 = p_src[y_ofs_up + src_xofs_right + l] << FRAC_BITS;
                    uint32_t p01 = p_src[y_ofs_down + src_xofs_left + l] << FRAC_BITS;
                    uint32_t p11 = p_src[y_ofs_down + src_xofs_right + l] << FRAC_BITS;

                    uint32_t interp_up = p00 + (((p10 - p00) * src_xofs_frac) >> FRAC_BITS);
                    uint32_t interp_down = p01 + (((p11 - p01) * src_xofs_frac) >> FRAC_BITS);
                    uint32_t interp = interp_up + (((interp_down - interp_up) * src_yofs_frac) >> FRAC_BITS);
                    interp >>= FRAC_BITS;
                    p_dst[i * p_dst_width * CC + j * CC + l] = interp;
                } else if (sizeof(T) == 2) { // half float

                    float xofs_frac = float(src_xofs_frac) / (1 << FRAC_BITS);
                    float yofs_frac = float(src_yofs_frac) / (1 << FRAC_BITS);
                    const T *src = ((const T *)p_src);
                    T *dst = ((T *)p_dst);

                    float p00 = Math::half_to_float(src[y_ofs_up + src_xofs_left + l]);
                    float p10 = Math::half_to_float(src[y_ofs_up + src_xofs_right + l]);
                    float p01 = Math::half_to_float(src[y_ofs_down + src_xofs_left + l]);
                    float p11 = Math::half_to_float(src[y_ofs_down + src_xofs_right + l]);

                    float interp_up = p00 + (p10 - p00) * xofs_frac;
                    float interp_down = p01 + (p11 - p01) * xofs_frac;
                    float interp = interp_up + ((interp_down - interp_up) * yofs_frac);

                    dst[i * p_dst_width * CC + j * CC + l] = Math::make_half_float(interp);
                } else if (sizeof(T) == 4) { // float

                    float xofs_frac = float(src_xofs_frac) / (1 << FRAC_BITS);
                    float yofs_frac = float(src_yofs_frac) / (1 << FRAC_BITS);
                    const T *src = ((const T *)p_src);
                    T *dst = ((T *)p_dst);

                    float p00 = src[y_ofs_up + src_xofs_left + l];
                    float p10 = src[y_ofs_up + src_xofs_right + l];
                    float p01 = src[y_ofs_down + src_xofs_left + l];
                    float p11 = src[y_ofs_down + src_xofs_right + l];

                    float interp_up = p00 + (p10 - p00) * xofs_frac;
                    float interp_down = p01 + (p11 - p01) * xofs_frac;
                    float interp = interp_up + ((interp_down - interp_up) * yofs_frac);

                    dst[i * p_dst_width * CC + j * CC + l] = interp;
                }
            }
        }
    }
}

template <int CC, class T>
static void _scale_nearest(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width,
        uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
    for (uint32_t i = 0; i < p_dst_height; i++) {
        uint32_t src_yofs = i * p_src_height / p_dst_height;
        uint32_t y_ofs = src_yofs * p_src_width * CC;

        for (uint32_t j = 0; j < p_dst_width; j++) {
            uint32_t src_xofs = j * p_src_width / p_dst_width;
            src_xofs *= CC;

            for (uint32_t l = 0; l < CC; l++) {
                const T *src = ((const T *)p_src);
                T *dst = ((T *)p_dst);

                T p = src[y_ofs + src_xofs + l];
                dst[i * p_dst_width * CC + j * CC + l] = p;
            }
        }
    }
}

#define LANCZOS_TYPE 3

static float _lanczos(float p_x) {
    return Math::abs(p_x) >= LANCZOS_TYPE ? 0 : Math::sincn(p_x) * Math::sincn(p_x / LANCZOS_TYPE);
}

template <int CC, class T>
static void _scale_lanczos(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, uint32_t p_src_width,
        uint32_t p_src_height, uint32_t p_dst_width, uint32_t p_dst_height) {
    int32_t src_width = p_src_width;
    int32_t src_height = p_src_height;
    int32_t dst_height = p_dst_height;
    int32_t dst_width = p_dst_width;

    uint32_t buffer_size = src_height * dst_width * CC;
    float *buffer = memnew_arr(float, buffer_size); // Store the first pass in a buffer

    { // FIRST PASS (horizontal)

        float x_scale = float(src_width) / float(dst_width);

        float scale_factor = M_MAX(x_scale, 1); // A larger kernel is required only when downscaling
        int32_t half_kernel = int32_t(LANCZOS_TYPE * scale_factor);

        float *kernel = memnew_arr(float, half_kernel * 2);

        for (int32_t buffer_x = 0; buffer_x < dst_width; buffer_x++) {
            // The corresponding point on the source image
            float src_x = (buffer_x + 0.5f) * x_scale; // Offset by 0.5 so it uses the pixel's center
            int32_t start_x = M_MAX(0, int32_t(src_x) - half_kernel + 1);
            int32_t end_x = MIN(src_width - 1, int32_t(src_x) + half_kernel);

            // Create the kernel used by all the pixels of the column
            for (int32_t target_x = start_x; target_x <= end_x; target_x++) {
                kernel[target_x - start_x] = _lanczos((target_x + 0.5f - src_x) / scale_factor);
            }

            for (int32_t buffer_y = 0; buffer_y < src_height; buffer_y++) {
                float pixel[CC] = { 0 };
                float weight = 0;

                for (int32_t target_x = start_x; target_x <= end_x; target_x++) {
                    float lanczos_val = kernel[target_x - start_x];
                    weight += lanczos_val;

                    const T *__restrict src_data = ((const T *)p_src) + (buffer_y * src_width + target_x) * CC;

                    for (uint32_t i = 0; i < CC; i++) {
                        if (sizeof(T) == 2) { // half float
                            pixel[i] += Math::half_to_float(src_data[i]) * lanczos_val;
                        } else {
                            pixel[i] += src_data[i] * lanczos_val;
                        }
                    }
                }

                float *dst_data = ((float *)buffer) + (buffer_y * dst_width + buffer_x) * CC;

                for (uint32_t i = 0; i < CC; i++) {
                    dst_data[i] = pixel[i] / weight; // Normalize the sum of all the samples
                }
            }
        }

        memdelete_arr(kernel);
    } // End of first pass

    { // SECOND PASS (vertical + result)

        float y_scale = float(src_height) / float(dst_height);

        float scale_factor = M_MAX(y_scale, 1);
        int32_t half_kernel = int(LANCZOS_TYPE * scale_factor);

        float *kernel = memnew_arr(float, half_kernel * 2);

        for (int32_t dst_y = 0; dst_y < dst_height; dst_y++) {
            float buffer_y = (dst_y + 0.5f) * y_scale;
            int32_t start_y = M_MAX(0, int32_t(buffer_y) - half_kernel + 1);
            int32_t end_y = MIN(src_height - 1, int32_t(buffer_y) + half_kernel);

            for (int32_t target_y = start_y; target_y <= end_y; target_y++) {
                kernel[target_y - start_y] = _lanczos((target_y + 0.5f - buffer_y) / scale_factor);
            }

            for (int32_t dst_x = 0; dst_x < dst_width; dst_x++) {
                float pixel[CC] = { 0 };
                float weight = 0;

                for (int32_t target_y = start_y; target_y <= end_y; target_y++) {
                    float lanczos_val = kernel[target_y - start_y];
                    weight += lanczos_val;

                    float *buffer_data = ((float *)buffer) + (target_y * dst_width + dst_x) * CC;

                    for (uint32_t i = 0; i < CC; i++) {
                        pixel[i] += buffer_data[i] * lanczos_val;
                    }
                }

                T *dst_data = ((T *)p_dst) + (dst_y * dst_width + dst_x) * CC;

                for (uint32_t i = 0; i < CC; i++) {
                    pixel[i] /= weight;

                    if (sizeof(T) == 1) { // byte
                        dst_data[i] = CLAMP<T>(Math::fast_ftoi(pixel[i]), 0, 255);
                    } else if (sizeof(T) == 2) { // half float
                        dst_data[i] = Math::make_half_float(pixel[i]);
                    } else { // float
                        dst_data[i] = pixel[i];
                    }
                }
            }
        }

        memdelete_arr(kernel);
    } // End of second pass

    memdelete_arr(buffer);
}

static void _overlay(const uint8_t *__restrict p_src, uint8_t *__restrict p_dst, float p_alpha, uint32_t p_width,
        uint32_t p_height, uint32_t p_pixel_size) {
    uint16_t alpha = MIN((uint16_t)(p_alpha * 256.0f), 256);

    for (uint32_t i = 0; i < p_width * p_height * p_pixel_size; i++) {
        p_dst[i] = (p_dst[i] * (256 - alpha) + p_src[i] * alpha) >> 8;
    }
}

bool Image::is_size_po2() const {
    return uint32_t(width) == next_power_of_2(width) && uint32_t(height) == next_power_of_2(height);
}

void Image::resize_to_po2(bool p_square) {
    ERR_FAIL_COND_MSG(!_can_modify(format), "Cannot resize in compressed or custom image formats.");

    int w = next_power_of_2(width);
    int h = next_power_of_2(height);
    if (p_square) {
        w = h = eastl::max(w, h);
    }
    if (w == width && h == height) {
        if (!p_square || w == h) {
            return; // nothing to do
        }
    }

    resize(w, h);
}

void Image::resize(int p_width, int p_height, Interpolation p_interpolation) {
    ERR_FAIL_COND_MSG(
            data.empty(), "Cannot resize image before creating it, use create() or create_from_data() first.");
    ERR_FAIL_COND_MSG(!_can_modify(format), "Cannot resize in compressed or custom image formats.");
    ERR_FAIL_COND_MSG(write_lock.ptr(), "Cannot resize image when it is locked.");

    bool mipmap_aware = p_interpolation == INTERPOLATE_TRILINEAR /* || p_interpolation == INTERPOLATE_TRICUBIC */;

    ERR_FAIL_COND_MSG(p_width <= 0, "Image width must be greater than 0.");
    ERR_FAIL_COND_MSG(p_height <= 0, "Image height must be greater than 0.");
    ERR_FAIL_COND_MSG(p_width > MAX_WIDTH, "Image width cannot be greater than " + itos(MAX_WIDTH) + ".");
    ERR_FAIL_COND_MSG(p_height > MAX_HEIGHT, "Image height cannot be greater than " + itos(MAX_HEIGHT) + ".");

    if (p_width == width && p_height == height) {
        return;
    }

    Image dst(p_width, p_height, false, format);

    // Setup mipmap-aware scaling
    Image dst2;
    int mip1 = 0;
    int mip2 = 0;
    float mip1_weight = 0;
    if (mipmap_aware) {
        float avg_scale = ((float)p_width / width + (float)p_height / height) * 0.5f;
        if (avg_scale >= 1.0f) {
            mipmap_aware = false;
        } else {
            float level = Math::log(1.0f / avg_scale) / Math::log(2.0f);
            mip1 = CLAMP((int)Math::floor(level), 0, get_mipmap_count());
            mip2 = CLAMP((int)Math::ceil(level), 0, get_mipmap_count());
            mip1_weight = 1.0f - (level - mip1);
        }
    }
    bool interpolate_mipmaps = mipmap_aware && mip1 != mip2;
    if (interpolate_mipmaps) {
        dst2.create(p_width, p_height, false, format);
    }

    bool had_mipmaps = mipmaps;
    if (interpolate_mipmaps && !had_mipmaps) {
        generate_mipmaps();
    }
    // --

    PoolVector<uint8_t>::Read r = data.read();
    const unsigned char *r_ptr = r.ptr();

    PoolVector<uint8_t>::Write w = dst.data.write();
    unsigned char *w_ptr = w.ptr();

    switch (p_interpolation) {
        case INTERPOLATE_NEAREST: {
            if (format >= FORMAT_L8 && format <= FORMAT_RGBA8) {
                switch (get_format_pixel_size(format)) {
                    case 1:
                        _scale_nearest<1, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 2:
                        _scale_nearest<2, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 3:
                        _scale_nearest<3, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 4:
                        _scale_nearest<4, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            } else if (format >= FORMAT_RF && format <= FORMAT_RGBAF) {
                switch (get_format_pixel_size(format)) {
                    case 4:
                        _scale_nearest<1, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 8:
                        _scale_nearest<2, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 12:
                        _scale_nearest<3, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 16:
                        _scale_nearest<4, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }

            } else if (format >= FORMAT_RH && format <= FORMAT_RGBAH) {
                switch (get_format_pixel_size(format)) {
                    case 2:
                        _scale_nearest<1, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 4:
                        _scale_nearest<2, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 6:
                        _scale_nearest<3, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 8:
                        _scale_nearest<4, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            }

        } break;
        case INTERPOLATE_BILINEAR:
        case INTERPOLATE_TRILINEAR: {
            for (int i = 0; i < 2; ++i) {
                int src_width;
                int src_height;
                const unsigned char *src_ptr;

                if (!mipmap_aware) {
                    if (i == 0) {
                        // Standard behavior
                        src_width = width;
                        src_height = height;
                        src_ptr = r_ptr;
                    } else {
                        // No need for a second iteration
                        break;
                    }
                } else {
                    if (i == 0) {
                        // Read from the first mipmap that will be interpolated
                        // (if both levels are the same, we will not interpolate, but at least we'll sample from the
                        // right level)
                        int offs;
                        _get_mipmap_offset_and_size(mip1, offs, src_width, src_height);
                        src_ptr = r_ptr + offs;
                    } else if (!interpolate_mipmaps) {
                        // No need generate a second image
                        break;
                    } else {
                        // Switch to read from the second mipmap that will be interpolated
                        int offs;
                        _get_mipmap_offset_and_size(mip2, offs, src_width, src_height);
                        src_ptr = r_ptr + offs;
                        // Switch to write to the second destination image
                        w = dst2.data.write();
                        w_ptr = w.ptr();
                    }
                }

                if (format >= FORMAT_L8 && format <= FORMAT_RGBA8) {
                    switch (get_format_pixel_size(format)) {
                        case 1:
                            _scale_bilinear<1, uint8_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 2:
                            _scale_bilinear<2, uint8_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 3:
                            _scale_bilinear<3, uint8_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 4:
                            _scale_bilinear<4, uint8_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                    }
                } else if (format >= FORMAT_RF && format <= FORMAT_RGBAF) {
                    switch (get_format_pixel_size(format)) {
                        case 4:
                            _scale_bilinear<1, float>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 8:
                            _scale_bilinear<2, float>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 12:
                            _scale_bilinear<3, float>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 16:
                            _scale_bilinear<4, float>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                    }
                } else if (format >= FORMAT_RH && format <= FORMAT_RGBAH) {
                    switch (get_format_pixel_size(format)) {
                        case 2:
                            _scale_bilinear<1, uint16_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 4:
                            _scale_bilinear<2, uint16_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 6:
                            _scale_bilinear<3, uint16_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                        case 8:
                            _scale_bilinear<4, uint16_t>(src_ptr, w_ptr, src_width, src_height, p_width, p_height);
                            break;
                    }
                }
            }

            if (interpolate_mipmaps) {
                // Switch to read again from the first scaled mipmap to overlay it over the second
                r = dst.data.read();
                _overlay(r.ptr(), w.ptr(), mip1_weight, p_width, p_height, get_format_pixel_size(format));
            }

        } break;
        case INTERPOLATE_CUBIC: {
            if (format >= FORMAT_L8 && format <= FORMAT_RGBA8) {
                switch (get_format_pixel_size(format)) {
                    case 1:
                        _scale_cubic<1, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 2:
                        _scale_cubic<2, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 3:
                        _scale_cubic<3, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 4:
                        _scale_cubic<4, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            } else if (format >= FORMAT_RF && format <= FORMAT_RGBAF) {
                switch (get_format_pixel_size(format)) {
                    case 4:
                        _scale_cubic<1, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 8:
                        _scale_cubic<2, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 12:
                        _scale_cubic<3, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 16:
                        _scale_cubic<4, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            } else if (format >= FORMAT_RH && format <= FORMAT_RGBAH) {
                switch (get_format_pixel_size(format)) {
                    case 2:
                        _scale_cubic<1, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 4:
                        _scale_cubic<2, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 6:
                        _scale_cubic<3, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 8:
                        _scale_cubic<4, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            }
        } break;
        case INTERPOLATE_LANCZOS: {
            if (format >= FORMAT_L8 && format <= FORMAT_RGBA8) {
                switch (get_format_pixel_size(format)) {
                    case 1:
                        _scale_lanczos<1, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 2:
                        _scale_lanczos<2, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 3:
                        _scale_lanczos<3, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 4:
                        _scale_lanczos<4, uint8_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            } else if (format >= FORMAT_RF && format <= FORMAT_RGBAF) {
                switch (get_format_pixel_size(format)) {
                    case 4:
                        _scale_lanczos<1, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 8:
                        _scale_lanczos<2, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 12:
                        _scale_lanczos<3, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 16:
                        _scale_lanczos<4, float>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            } else if (format >= FORMAT_RH && format <= FORMAT_RGBAH) {
                switch (get_format_pixel_size(format)) {
                    case 2:
                        _scale_lanczos<1, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 4:
                        _scale_lanczos<2, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 6:
                        _scale_lanczos<3, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                    case 8:
                        _scale_lanczos<4, uint16_t>(r_ptr, w_ptr, width, height, p_width, p_height);
                        break;
                }
            }
        } break;
    }

    r.release();
    w.release();

    if (interpolate_mipmaps) {
        dst._copy_internals_from(dst2);
    }

    if (had_mipmaps) {
        dst.generate_mipmaps();
    }

    _copy_internals_from(dst);
}

void Image::crop_from_point(int p_x, int p_y, int p_width, int p_height) {
    ERR_FAIL_COND_MSG(!_can_modify(format), "Cannot crop in compressed or custom image formats.");

    ERR_FAIL_COND_MSG(p_x < 0, "Start x position cannot be smaller than 0.");
    ERR_FAIL_COND_MSG(p_y < 0, "Start y position cannot be smaller than 0.");
    ERR_FAIL_COND_MSG(p_width <= 0, "Width of image must be greater than 0.");
    ERR_FAIL_COND_MSG(p_height <= 0, "Height of image must be greater than 0.");
    ERR_FAIL_COND_MSG(p_x + p_width > MAX_WIDTH, "End x position cannot be greater than " + itos(MAX_WIDTH) + ".");
    ERR_FAIL_COND_MSG(p_y + p_height > MAX_HEIGHT, "End y position cannot be greater than " + itos(MAX_HEIGHT) + ".");

    /* to save memory, cropping should be done in-place, however, since this function
       will most likely either not be used much, or in critical areas, for now it won't, because
       it's a waste of time. */

    if (p_width == width && p_height == height && p_x == 0 && p_y == 0) {
        return;
    }

    uint8_t pdata[16]; // largest is 16
    uint32_t pixel_size = get_format_pixel_size(format);

    Image dst(p_width, p_height, false, format);

    {
        PoolVector<uint8_t>::Read r = data.read();
        PoolVector<uint8_t>::Write w = dst.data.write();

        int m_h = p_y + p_height;
        int m_w = p_x + p_width;
        for (int y = p_y; y < m_h; y++) {
            for (int x = p_x; x < m_w; x++) {
                if ((x >= width || y >= height)) {
                    for (uint32_t i = 0; i < pixel_size; i++) {
                        pdata[i] = 0;
                    }
                } else {
                    _get_pixelb(x, y, pixel_size, r.ptr(), pdata);
                }

                dst._put_pixelb(x - p_x, y - p_y, pixel_size, w.ptr(), pdata);
            }
        }
    }

    if (has_mipmaps()) {
        dst.generate_mipmaps();
    }
    _copy_internals_from(dst);
}

void Image::crop(int p_width, int p_height) {
    crop_from_point(0, 0, p_width, p_height);
}

void Image::flip_y() {
    ERR_FAIL_COND_MSG(!_can_modify(format), "Cannot flip_y in compressed or custom image formats.");

    bool used_mipmaps = has_mipmaps();
    if (used_mipmaps) {
        clear_mipmaps();
    }

    {
        PoolVector<uint8_t>::Write w = data.write();
        uint8_t up[16];
        uint8_t down[16];
        uint32_t pixel_size = get_format_pixel_size(format);

        for (int y = 0; y < height / 2; y++) {
            for (int x = 0; x < width; x++) {
                _get_pixelb(x, y, pixel_size, w.ptr(), up);
                _get_pixelb(x, height - y - 1, pixel_size, w.ptr(), down);

                _put_pixelb(x, height - y - 1, pixel_size, w.ptr(), up);
                _put_pixelb(x, y, pixel_size, w.ptr(), down);
            }
        }
    }

    if (used_mipmaps) {
        generate_mipmaps();
    }
}

void Image::flip_x() {
    ERR_FAIL_COND_MSG(!_can_modify(format), "Cannot flip_x in compressed or custom image formats.");

    bool used_mipmaps = has_mipmaps();
    if (used_mipmaps) {
        clear_mipmaps();
    }

    {
        PoolVector<uint8_t>::Write w = data.write();
        uint8_t up[16];
        uint8_t down[16];
        uint32_t pixel_size = get_format_pixel_size(format);

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width / 2; x++) {
                _get_pixelb(x, y, pixel_size, w.ptr(), up);
                _get_pixelb(width - x - 1, y, pixel_size, w.ptr(), down);

                _put_pixelb(width - x - 1, y, pixel_size, w.ptr(), up);
                _put_pixelb(x, y, pixel_size, w.ptr(), down);
            }
        }
    }

    if (used_mipmaps) {
        generate_mipmaps();
    }
}

bool Image::_can_modify(Format p_format) const {
    return p_format <= FORMAT_RGBE9995;
}

template <class Component, int CC, bool renormalize,
        void (*average_func)(Component &, const Component &, const Component &, const Component &, const Component &),
        void (*renormalize_func)(Component *)>
static void _generate_po2_mipmap(const Component *p_src, Component *p_dst, uint32_t p_width, uint32_t p_height) {
    // fast power of 2 mipmap generation
    uint32_t dst_w = M_MAX(p_width >> 1, 1);
    uint32_t dst_h = M_MAX(p_height >> 1, 1);

    int right_step = (p_width == 1) ? 0 : CC;
    int down_step = (p_height == 1) ? 0 : (p_width * CC);

    for (uint32_t i = 0; i < dst_h; i++) {
        const Component *rup_ptr = &p_src[i * 2 * down_step];
        const Component *rdown_ptr = rup_ptr + down_step;
        Component *dst_ptr = &p_dst[i * dst_w * CC];
        uint32_t count = dst_w;

        while (count--) {
            for (int j = 0; j < CC; j++) {
                average_func(dst_ptr[j], rup_ptr[j], rup_ptr[j + right_step], rdown_ptr[j], rdown_ptr[j + right_step]);
            }

            if (renormalize) {
                renormalize_func(dst_ptr);
            }

            dst_ptr += CC;
            rup_ptr += right_step * 2;
            rdown_ptr += right_step * 2;
        }
    }
}

void Image::expand_x2_hq2x() {
    ERR_FAIL_COND(!_can_modify(format));
    ERR_FAIL_COND_MSG(write_lock.ptr(), "Cannot modify image when it is locked.");

    bool used_mipmaps = has_mipmaps();
    if (used_mipmaps) {
        clear_mipmaps();
    }

    Format current = format;

    if (current != FORMAT_RGBA8) {
        convert(FORMAT_RGBA8);
    }

    PoolVector<uint8_t> dest;
    dest.resize(width * 2 * height * 2 * 4);

    {
        PoolVector<uint8_t>::Read r = data.read();
        PoolVector<uint8_t>::Write w = dest.write();

        ERR_FAIL_COND(!r.ptr());

        hq2x_resize((const uint32_t *)r.ptr(), width, height, (uint32_t *)w.ptr());
    }

    width *= 2;
    height *= 2;
    data = eastl::move(dest);

    if (current != FORMAT_RGBA8) {
        convert(current);
    }

    // FIXME: This is likely meant to use "used_mipmaps" as defined above, but if we do,
    // we end up with a regression: GH-22747
    if (mipmaps) {
        generate_mipmaps();
    }
}

void Image::shrink_x2() {
    ERR_FAIL_COND(data.size() == 0);

    if (mipmaps) {
        // just use the lower mipmap as base and copy all
        PoolVector<uint8_t> new_img;

        int ofs = get_mipmap_offset(1);

        int new_size = data.size() - ofs;
        new_img.resize(new_size);
        ERR_FAIL_COND(new_img.empty());
        ERR_FAIL_COND(data.empty());
        {
            PoolVector<uint8_t>::Write w = new_img.write();
            PoolVector<uint8_t>::Read r = data.read();

            memcpy(w.ptr(), &r[ofs], new_size);
        }

        width = M_MAX(width / 2, 1);
        height = M_MAX(height / 2, 1);
        data = new_img;

    } else {
        PoolVector<uint8_t> new_img;

        ERR_FAIL_COND(!_can_modify(format));
        int ps = get_format_pixel_size(format);
        new_img.resize((width / 2) * (height / 2) * ps);
        ERR_FAIL_COND(new_img.size() == 0);

        {
            PoolVector<uint8_t>::Write w = new_img.write();
            PoolVector<uint8_t>::Read r = data.read();

            switch (format) {
                case FORMAT_L8:
                case FORMAT_R8:
                    _generate_po2_mipmap<uint8_t, 1, false, average_4_uint8, renormalize_uint8>(
                            r.ptr(), w.ptr(), width, height);
                    break;
                case FORMAT_LA8:
                    _generate_po2_mipmap<uint8_t, 2, false, average_4_uint8, renormalize_uint8>(
                            r.ptr(), w.ptr(), width, height);
                    break;
                case FORMAT_RG8:
                    _generate_po2_mipmap<uint8_t, 2, false, average_4_uint8, renormalize_uint8>(
                            r.ptr(), w.ptr(), width, height);
                    break;
                case FORMAT_RGB8:
                    _generate_po2_mipmap<uint8_t, 3, false, average_4_uint8, renormalize_uint8>(
                            r.ptr(), w.ptr(), width, height);
                    break;
                case FORMAT_RGBA8:
                    _generate_po2_mipmap<uint8_t, 4, false, average_4_uint8, renormalize_uint8>(
                            r.ptr(), w.ptr(), width, height);
                    break;

                case FORMAT_RF:
                    _generate_po2_mipmap<float, 1, false, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(r.ptr()), reinterpret_cast<float *>(w.ptr()), width,
                            height);
                    break;
                case FORMAT_RGF:
                    _generate_po2_mipmap<float, 2, false, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(r.ptr()), reinterpret_cast<float *>(w.ptr()), width,
                            height);
                    break;
                case FORMAT_RGBF:
                    _generate_po2_mipmap<float, 3, false, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(r.ptr()), reinterpret_cast<float *>(w.ptr()), width,
                            height);
                    break;
                case FORMAT_RGBAF:
                    _generate_po2_mipmap<float, 4, false, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(r.ptr()), reinterpret_cast<float *>(w.ptr()), width,
                            height);
                    break;

                case FORMAT_RH:
                    _generate_po2_mipmap<uint16_t, 1, false, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(r.ptr()), reinterpret_cast<uint16_t *>(w.ptr()), width,
                            height);
                    break;
                case FORMAT_RGH:
                    _generate_po2_mipmap<uint16_t, 2, false, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(r.ptr()), reinterpret_cast<uint16_t *>(w.ptr()), width,
                            height);
                    break;
                case FORMAT_RGBH:
                    _generate_po2_mipmap<uint16_t, 3, false, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(r.ptr()), reinterpret_cast<uint16_t *>(w.ptr()), width,
                            height);
                    break;
                case FORMAT_RGBAH:
                    _generate_po2_mipmap<uint16_t, 4, false, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(r.ptr()), reinterpret_cast<uint16_t *>(w.ptr()), width,
                            height);
                    break;

                case FORMAT_RGBE9995:
                    _generate_po2_mipmap<uint32_t, 1, false, average_4_rgbe9995, renormalize_rgbe9995>(
                            reinterpret_cast<const uint32_t *>(r.ptr()), reinterpret_cast<uint32_t *>(w.ptr()), width,
                            height);
                    break;
                default: {
                }
            }
        }

        width /= 2;
        height /= 2;
        data = new_img;
    }
}

void Image::normalize() {
    bool used_mipmaps = has_mipmaps();
    if (used_mipmaps) {
        clear_mipmaps();
    }

    lock();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            Color c = get_pixel(x, y);
            Vector3 v(c.r * 2.0f - 1.0f, c.g * 2.0f - 1.0f, c.b * 2.0f - 1.0f);
            v.normalize();
            c.r = v.x * 0.5f + 0.5f;
            c.g = v.y * 0.5f + 0.5f;
            c.b = v.z * 0.5f + 0.5f;
            set_pixel(x, y, c);
        }
    }

    unlock();

    if (used_mipmaps) {
        generate_mipmaps(true);
    }
}

Error Image::generate_mipmaps(bool p_renormalize) {
    ERR_FAIL_COND_V_MSG(
            !_can_modify(format), ERR_UNAVAILABLE, "Cannot generate mipmaps in compressed or custom image formats.");

    ERR_FAIL_COND_V_MSG(
            width == 0 || height == 0, ERR_UNCONFIGURED, "Cannot generate mipmaps with width or height equal to 0.");

    int mmcount;

    int size = _get_dst_image_size(width, height, format, mmcount);

    data.resize(size);

    PoolVector<uint8_t>::Write wp = data.write();

    int prev_ofs = 0;
    int prev_h = height;
    int prev_w = width;

    for (int i = 1; i <= mmcount; i++) {
        int ofs, w, h;
        _get_mipmap_offset_and_size(i, ofs, w, h);

        switch (format) {
            case FORMAT_L8:
            case FORMAT_R8:
                _generate_po2_mipmap<uint8_t, 1, false, average_4_uint8, renormalize_uint8>(
                        &wp[prev_ofs], &wp[ofs], prev_w, prev_h);
                break;
            case FORMAT_LA8:
            case FORMAT_RG8:
                _generate_po2_mipmap<uint8_t, 2, false, average_4_uint8, renormalize_uint8>(
                        &wp[prev_ofs], &wp[ofs], prev_w, prev_h);
                break;
            case FORMAT_RGB8:
                if (p_renormalize) {
                    _generate_po2_mipmap<uint8_t, 3, true, average_4_uint8, renormalize_uint8>(
                            &wp[prev_ofs], &wp[ofs], prev_w, prev_h);
                } else {
                    _generate_po2_mipmap<uint8_t, 3, false, average_4_uint8, renormalize_uint8>(
                            &wp[prev_ofs], &wp[ofs], prev_w, prev_h);
                }

                break;
            case FORMAT_RGBA8:
                if (p_renormalize) {
                    _generate_po2_mipmap<uint8_t, 4, true, average_4_uint8, renormalize_uint8>(
                            &wp[prev_ofs], &wp[ofs], prev_w, prev_h);
                } else {
                    _generate_po2_mipmap<uint8_t, 4, false, average_4_uint8, renormalize_uint8>(
                            &wp[prev_ofs], &wp[ofs], prev_w, prev_h);
                }
                break;
            case FORMAT_RF:
                _generate_po2_mipmap<float, 1, false, average_4_float, renormalize_float>(
                        reinterpret_cast<const float *>(&wp[prev_ofs]), reinterpret_cast<float *>(&wp[ofs]), prev_w,
                        prev_h);
                break;
            case FORMAT_RGF:
                _generate_po2_mipmap<float, 2, false, average_4_float, renormalize_float>(
                        reinterpret_cast<const float *>(&wp[prev_ofs]), reinterpret_cast<float *>(&wp[ofs]), prev_w,
                        prev_h);
                break;
            case FORMAT_RGBF:
                if (p_renormalize) {
                    _generate_po2_mipmap<float, 3, true, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(&wp[prev_ofs]), reinterpret_cast<float *>(&wp[ofs]), prev_w,
                            prev_h);
                } else {
                    _generate_po2_mipmap<float, 3, false, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(&wp[prev_ofs]), reinterpret_cast<float *>(&wp[ofs]), prev_w,
                            prev_h);
                }

                break;
            case FORMAT_RGBAF:
                if (p_renormalize) {
                    _generate_po2_mipmap<float, 4, true, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(&wp[prev_ofs]), reinterpret_cast<float *>(&wp[ofs]), prev_w,
                            prev_h);
                } else {
                    _generate_po2_mipmap<float, 4, false, average_4_float, renormalize_float>(
                            reinterpret_cast<const float *>(&wp[prev_ofs]), reinterpret_cast<float *>(&wp[ofs]), prev_w,
                            prev_h);
                }

                break;
            case FORMAT_RH:
                _generate_po2_mipmap<uint16_t, 1, false, average_4_half, renormalize_half>(
                        reinterpret_cast<const uint16_t *>(&wp[prev_ofs]), reinterpret_cast<uint16_t *>(&wp[ofs]),
                        prev_w, prev_h);
                break;
            case FORMAT_RGH:
                _generate_po2_mipmap<uint16_t, 2, false, average_4_half, renormalize_half>(
                        reinterpret_cast<const uint16_t *>(&wp[prev_ofs]), reinterpret_cast<uint16_t *>(&wp[ofs]),
                        prev_w, prev_h);
                break;
            case FORMAT_RGBH:
                if (p_renormalize) {
                    _generate_po2_mipmap<uint16_t, 3, true, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(&wp[prev_ofs]), reinterpret_cast<uint16_t *>(&wp[ofs]),
                            prev_w, prev_h);
                } else {
                    _generate_po2_mipmap<uint16_t, 3, false, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(&wp[prev_ofs]), reinterpret_cast<uint16_t *>(&wp[ofs]),
                            prev_w, prev_h);
                }

                break;
            case FORMAT_RGBAH:
                if (p_renormalize) {
                    _generate_po2_mipmap<uint16_t, 4, true, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(&wp[prev_ofs]), reinterpret_cast<uint16_t *>(&wp[ofs]),
                            prev_w, prev_h);
                } else {
                    _generate_po2_mipmap<uint16_t, 4, false, average_4_half, renormalize_half>(
                            reinterpret_cast<const uint16_t *>(&wp[prev_ofs]), reinterpret_cast<uint16_t *>(&wp[ofs]),
                            prev_w, prev_h);
                }

                break;
            case FORMAT_RGBE9995:
                if (p_renormalize) {
                    _generate_po2_mipmap<uint32_t, 1, true, average_4_rgbe9995, renormalize_rgbe9995>(
                            reinterpret_cast<const uint32_t *>(&wp[prev_ofs]), reinterpret_cast<uint32_t *>(&wp[ofs]),
                            prev_w, prev_h);
                } else {
                    _generate_po2_mipmap<uint32_t, 1, false, average_4_rgbe9995, renormalize_rgbe9995>(
                            reinterpret_cast<const uint32_t *>(&wp[prev_ofs]), reinterpret_cast<uint32_t *>(&wp[ofs]),
                            prev_w, prev_h);
                }

                break;
            default: {
            }
        }

        prev_ofs = ofs;
        prev_w = w;
        prev_h = h;
    }

    mipmaps = true;

    return OK;
}
Error Image::generate_mipmap_roughness(RoughnessChannel p_roughness_channel, const Ref<Image> &p_normal_map) {
    Vector<double> normal_sat_vec; // summed area table
    double *normal_sat = nullptr; // summed area table for normalmap
    int normal_w = 0, normal_h = 0;

    ERR_FAIL_COND_V_MSG(!p_normal_map || p_normal_map->is_empty(), ERR_INVALID_PARAMETER,
            "Must provide a valid normalmap for roughness mipmaps");

    Ref<Image> nm((Image *)p_normal_map->duplicate().get());
    if (nm->is_compressed()) {
        nm->decompress();
    }

    normal_w = nm->get_width();
    normal_h = nm->get_height();

    normal_sat_vec.resize(normal_w * normal_h * 3);

    normal_sat = normal_sat_vec.data();

    // create summed area table
    nm->lock();

    for (int y = 0; y < normal_h; y++) {
        double line_sum[3] = { 0, 0, 0 };
        for (int x = 0; x < normal_w; x++) {
            double normal[3];
            Color color = nm->get_pixel(x, y);
            normal[0] = color.r * 2.0 - 1.0;
            normal[1] = color.g * 2.0 - 1.0;
            normal[2] = Math::sqrt(
                    M_MAX(0.0, 1.0 - (normal[0] * normal[0] + normal[1] * normal[1]))); // reconstruct if missing

            line_sum[0] += normal[0];
            line_sum[1] += normal[1];
            line_sum[2] += normal[2];

            uint32_t ofs = (y * normal_w + x) * 3;

            normal_sat[ofs + 0] = line_sum[0];
            normal_sat[ofs + 1] = line_sum[1];
            normal_sat[ofs + 2] = line_sum[2];

            if (y > 0) {
                uint32_t prev_ofs = ((y - 1) * normal_w + x) * 3;
                normal_sat[ofs + 0] += normal_sat[prev_ofs + 0];
                normal_sat[ofs + 1] += normal_sat[prev_ofs + 1];
                normal_sat[ofs + 2] += normal_sat[prev_ofs + 2];
            }
        }
    }

#if 0
    {
        Vector3 beg(normal_sat_vec[0], normal_sat_vec[1], normal_sat_vec[2]);
        Vector3 end(normal_sat_vec[normal_sat_vec.size() - 3], normal_sat_vec[normal_sat_vec.size() - 2], normal_sat_vec[normal_sat_vec.size() - 1]);
        Vector3 avg = (end - beg) / (normal_w * normal_h);
        print_line("average: " + avg);
    }
#endif

    int mmcount;

    _get_dst_image_size(width, height, format, mmcount);

    lock();

    uint8_t *base_ptr = write_lock.ptr();

    for (int i = 1; i <= mmcount; i++) {
        int ofs, w, h;
        _get_mipmap_offset_and_size(i, ofs, w, h);
        uint8_t *ptr = &base_ptr[ofs];

        for (int x = 0; x < w; x++) {
            for (int y = 0; y < h; y++) {
                int from_x = x * normal_w / w;
                int from_y = y * normal_h / h;
                int to_x = (x + 1) * normal_w / w;
                int to_y = (y + 1) * normal_h / h;
                to_x = MIN(to_x - 1, normal_w);
                to_y = MIN(to_y - 1, normal_h);

                int size_x = (to_x - from_x) + 1;
                int size_y = (to_y - from_y) + 1;

                // summed area table version (much faster)

                double avg[3] = { 0, 0, 0 };

                if (from_x > 0 && from_y > 0) {
                    uint32_t tofs = ((from_y - 1) * normal_w + (from_x - 1)) * 3;
                    avg[0] += normal_sat[tofs + 0];
                    avg[1] += normal_sat[tofs + 1];
                    avg[2] += normal_sat[tofs + 2];
                }

                if (from_y > 0) {
                    uint32_t tofs = ((from_y - 1) * normal_w + to_x) * 3;
                    avg[0] -= normal_sat[tofs + 0];
                    avg[1] -= normal_sat[tofs + 1];
                    avg[2] -= normal_sat[tofs + 2];
                }

                if (from_x > 0) {
                    uint32_t tofs = (to_y * normal_w + (from_x - 1)) * 3;
                    avg[0] -= normal_sat[tofs + 0];
                    avg[1] -= normal_sat[tofs + 1];
                    avg[2] -= normal_sat[tofs + 2];
                }

                uint32_t tofs = (to_y * normal_w + to_x) * 3;
                avg[0] += normal_sat[tofs + 0];
                avg[1] += normal_sat[tofs + 1];
                avg[2] += normal_sat[tofs + 2];

                double div = double(size_x * size_y);
                Vector3 vec(avg[0] / div, avg[1] / div, avg[2] / div);

                float r = vec.length();

                int pixel_ofs = y * w + x;
                Color c = _get_color_at_ofs(ptr, pixel_ofs);

                float roughness;

                switch (p_roughness_channel) {
                    case ROUGHNESS_CHANNEL_R: {
                        roughness = c.r;
                    } break;
                    case ROUGHNESS_CHANNEL_G: {
                        roughness = c.g;
                    } break;
                    case ROUGHNESS_CHANNEL_B: {
                        roughness = c.b;
                    } break;
                    case ROUGHNESS_CHANNEL_L: {
                        roughness = c.get_v();
                    } break;
                    case ROUGHNESS_CHANNEL_A: {
                        roughness = c.a;
                    } break;
                }

                float variance = 0;
                if (r < 1.0f) {
                    float r2 = r * r;
                    float kappa = (3.0f * r - r * r2) / (1.0f - r2);
                    variance = 0.25f / kappa;
                }

                float threshold = 0.4f;
                roughness = Math::sqrt(roughness * roughness + MIN(3.0f * variance, threshold * threshold));

                switch (p_roughness_channel) {
                    case ROUGHNESS_CHANNEL_R: {
                        c.r = roughness;
                    } break;
                    case ROUGHNESS_CHANNEL_G: {
                        c.g = roughness;
                    } break;
                    case ROUGHNESS_CHANNEL_B: {
                        c.b = roughness;
                    } break;
                    case ROUGHNESS_CHANNEL_L: {
                        c.r = roughness;
                        c.g = roughness;
                        c.b = roughness;
                    } break;
                    case ROUGHNESS_CHANNEL_A: {
                        c.a = roughness;
                    } break;
                }

                _set_color_at_ofs(ptr, pixel_ofs, c);
            }
        }
#if 0
        {
            int size = get_mipmap_byte_size(i);
            print_line("size for mimpap " + itos(i) + ": " + itos(size));
            PoolVector<uint8_t> imgdata;
            imgdata.resize(size);
            PoolVector<uint8_t>::Write wr = imgdata.write();
            copymem(wr.ptr(), ptr, size);
            wr = PoolVector<uint8_t>::Write();
            Ref<Image> im;
            im.instance();
            im->create(w, h, false, format, imgdata);
            im->save_png("res://mipmap_" + itos(i) + ".png");
        }
#endif
    }

    unlock();
    nm->unlock();

    return OK;
}
void Image::clear_mipmaps() {
    if (!mipmaps) {
        return;
    }

    if (is_empty()) {
        return;
    }

    int ofs, w, h;
    _get_mipmap_offset_and_size(1, ofs, w, h);
    data.resize(ofs);

    mipmaps = false;
}

bool Image::is_empty() const {
    return data.empty();
}

void Image::create(int p_width, int p_height, bool p_use_mipmaps, Format p_format) {
    ERR_FAIL_INDEX(p_width - 1, MAX_WIDTH);
    ERR_FAIL_INDEX(p_height - 1, MAX_HEIGHT);

    int mm = 0;
    int size = _get_dst_image_size(p_width, p_height, p_format, mm, p_use_mipmaps ? -1 : 0);
    data.resize(size);
    {
        PoolVector<uint8_t>::Write w = data.write();
        memset(w.ptr(), 0, size);
    }

    width = p_width;
    height = p_height;
    mipmaps = p_use_mipmaps;
    format = p_format;
}
void Image::create(ImageData &&src) {
    ERR_FAIL_INDEX(src.width - 1, MAX_WIDTH);
    ERR_FAIL_INDEX(src.height - 1, MAX_HEIGHT);

    int mm = 0;
    int size_without_mips = _get_dst_image_size(src.width, src.height, src.format, mm, 0);
    int size = _get_dst_image_size(src.width, src.height, src.format, mm, src.mipmaps ? -1 : 0);
    data = eastl::move(src.data);
    data.resize(size); // make space for optional mipmaps
    if (src.mipmaps) {
        PoolVector<uint8_t>::Write w = data.write();
        memset(w.ptr() + size_without_mips, 0, size - size_without_mips);
    }

    width = src.width;
    height = src.height;
    mipmaps = src.mipmaps;
    format = src.format;

    src.width = 0;
    src.height = 0;
    src.mipmaps = false;
    src.format = Format(0);
}
void Image::create(int p_width, int p_height, bool p_use_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data) {
    ERR_FAIL_INDEX(p_width - 1, MAX_WIDTH);
    ERR_FAIL_INDEX(p_height - 1, MAX_HEIGHT);

    int mm;
    int size = _get_dst_image_size(p_width, p_height, p_format, mm, p_use_mipmaps ? -1 : 0);

    ERR_FAIL_COND_MSG(p_data.size() != size, "Expected data size of " + itos(size) +
                                                     " bytes in Image::create(), got instead " + itos(p_data.size()) +
                                                     " bytes.");

    height = p_height;
    width = p_width;
    format = p_format;
    data = p_data;
    mipmaps = p_use_mipmaps;
}

#define DETECT_ALPHA_MAX_THRESHOLD 254
#define DETECT_ALPHA_MIN_THRESHOLD 2

#define DETECT_ALPHA(m_value)                                                                                          \
    {                                                                                                                  \
        uint8_t value = m_value;                                                                                       \
        if (value < DETECT_ALPHA_MIN_THRESHOLD)                                                                        \
            bit = true;                                                                                                \
        else if (value < DETECT_ALPHA_MAX_THRESHOLD) {                                                                 \
            detected = true;                                                                                           \
            break;                                                                                                     \
        }                                                                                                              \
    }

#define DETECT_NON_ALPHA(m_value)                                                                                      \
    {                                                                                                                  \
        uint8_t value = m_value;                                                                                       \
        if (value > 0) {                                                                                               \
            detected = true;                                                                                           \
            break;                                                                                                     \
        }                                                                                                              \
    }

bool Image::is_invisible() const {
    if (format == FORMAT_L8 || format == FORMAT_RGB8 || format == FORMAT_RG8) {
        return false;
    }

    int len = data.size();

    if (len == 0) {
        return true;
    }

    int w, h;
    _get_mipmap_offset_and_size(1, len, w, h);

    PoolVector<uint8_t>::Read r = data.read();
    const uint8_t *data_ptr = r.ptr();

    bool detected = false;

    switch (format) {
        case FORMAT_LA8: {
            for (int i = 0; i < (len >> 1); i++) {
                DETECT_NON_ALPHA(data_ptr[(i << 1) + 1]);
            }

        } break;
        case FORMAT_RGBA8: {
            for (int i = 0; i < (len >> 2); i++) {
                DETECT_NON_ALPHA(data_ptr[(i << 2) + 3])
            }

        } break;

        case FORMAT_DXT3:
        case FORMAT_DXT5: {
            detected = true;
        } break;
        default: {
        }
    }

    return !detected;
}

Image::AlphaMode Image::detect_alpha() const {
    int len = data.size();

    if (len == 0) {
        return ALPHA_NONE;
    }

    int w, h;
    _get_mipmap_offset_and_size(1, len, w, h);

    PoolVector<uint8_t>::Read r = data.read();
    const unsigned char *data_ptr = r.ptr();

    bool bit = false;
    bool detected = false;

    switch (format) {
        case FORMAT_LA8: {
            for (int i = 0; i < (len >> 1); i++) {
                DETECT_ALPHA(data_ptr[(i << 1) + 1]);
            }

        } break;
        case FORMAT_RGBA8: {
            for (int i = 0; i < (len >> 2); i++) {
                DETECT_ALPHA(data_ptr[(i << 2) + 3])
            }

        } break;
        case FORMAT_DXT3:
        case FORMAT_DXT5: {
            detected = true;
        } break;
        default: {
        }
    }

    if (detected) {
        return ALPHA_BLEND;
    } else if (bit) {
        return ALPHA_BIT;
    } else {
        return ALPHA_NONE;
    }
}

Error Image::load(StringView p_path) {
#ifdef DEBUG_ENABLED
    if (StringUtils::begins_with(p_path, "res://") && gResourceManager().exists(p_path)) {
        WARN_PRINT("Loaded resource as image file, this will not work on export: '" + String(p_path) +
                   "'. Instead, import the image file as an Image resource and load it normally as a resource.");
    }
#endif
    return ImageLoader::load_image(p_path, Ref<Image>(this));
}

Error Image::save_png(StringView p_path) const {
    return save_png_func(p_path, Ref<Image>((Image *)this));
}

Error Image::save_exr(StringView p_path, bool p_grayscale) const {
    return save_exr_func(p_path, Ref<Image>((Image *)this), p_grayscale);
}

int Image::get_image_data_size(int p_width, int p_height, Format p_format, bool p_mipmaps) {
    int mm;
    return _get_dst_image_size(p_width, p_height, p_format, mm, p_mipmaps ? -1 : 0);
}

int Image::get_image_required_mipmaps(int p_width, int p_height, Format p_format) {
    int mm;
    _get_dst_image_size(p_width, p_height, p_format, mm, -1);
    return mm;
}

int Image::get_image_mipmap_offset(int p_width, int p_height, Format p_format, int p_mipmap) {
    if (p_mipmap <= 0) {
        return 0;
    }
    int mm;
    return _get_dst_image_size(p_width, p_height, p_format, mm, p_mipmap - 1);
}

bool Image::is_compressed() const {
    return format > FORMAT_RGBE9995;
}

Error Image::decompress() {
    ImageCompressMode mode;

    if (format >= FORMAT_DXT1 && format <= FORMAT_RGTC_RG) {
        mode = COMPRESS_S3TC;
    } else if (format >= FORMAT_BPTC_RGBA && format <= FORMAT_BPTC_RGBFU) {
        mode = COMPRESS_BPTC;
    } else {
        return ERR_UNAVAILABLE;
    }
    return decompress_image(this, CompressParams{ 1.0, mode });
}
Error Image::compress(ImageCompressMode p_mode, ImageCompressSource p_source, float p_lossy_quality) {
    ERR_FAIL_INDEX_V_MSG(p_mode, COMPRESS_MAX, ERR_INVALID_PARAMETER, "Invalid compress mode.");
    ERR_FAIL_INDEX_V_MSG((int)p_source, (int)ImageCompressSource::COMPRESS_SOURCE_MAX, ERR_INVALID_PARAMETER,
            "Invalid compress source.");
    return compress_from_channels(p_mode, detect_used_channels(p_source), p_lossy_quality);
}
Error Image::compress_from_channels(ImageCompressMode p_mode, ImageUsedChannels p_channels, float p_lossy_quality) {
    switch (p_mode) {
        case COMPRESS_S3TC:
        case COMPRESS_BPTC:
            return Image::compress_image(this, { p_lossy_quality, p_mode, p_channels });
        case COMPRESS_MAX: {
            ERR_FAIL_V(ERR_INVALID_PARAMETER);
        } break;
        default:
            ERR_PRINT("Unsupported compress mode");
            return ERR_UNAVAILABLE;
    }

    return OK;
}


Image::Image(int p_width, int p_height, bool p_use_mipmaps, Format p_format) {
    width = 0;
    height = 0;
    mipmaps = p_use_mipmaps;
    format = FORMAT_L8;

    create(p_width, p_height, p_use_mipmaps, p_format);
}

Image::Image(int p_width, int p_height, bool p_mipmaps, Format p_format, const PoolVector<uint8_t> &p_data) {
    width = 0;
    height = 0;
    mipmaps = p_mipmaps;
    format = FORMAT_L8;

    create(p_width, p_height, p_mipmaps, p_format, p_data);
}

Rect2 Image::get_used_rect() const {
    if (format != FORMAT_LA8 && format != FORMAT_RGBA8 && format != FORMAT_RGBAF && format != FORMAT_RGBAH &&
            format != FORMAT_RGBA4444 && format != FORMAT_RGB565) {
        return Rect2(Point2(), Size2(width, height));
    }

    int len = data.size();

    if (len == 0) {
        return Rect2();
    }

    const_cast<Image *>(this)->lock();
    int minx = 0xFFFFFF, miny = 0xFFFFFFF;
    int maxx = -1, maxy = -1;
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            if (!(get_pixel(i, j).a > 0)) {
                continue;
            }
            if (i > maxx) {
                maxx = i;
            }
            if (j > maxy) {
                maxy = j;
            }
            if (i < minx) {
                minx = i;
            }
            if (j < miny) {
                miny = j;
            }
        }
    }

    const_cast<Image *>(this)->unlock();

    if (maxx == -1) {
        return Rect2();
    } else {
        return Rect2(minx, miny, maxx - minx + 1, maxy - miny + 1);
    }
}

Ref<Image> Image::get_rect(const Rect2 &p_area) const {
    Ref<Image> img(make_ref_counted<Image>(p_area.size.x, p_area.size.y, mipmaps, format));
    img->blit_rect(Ref<Image>((Image *)this), p_area, Point2(0, 0));
    return img;
}

void Image::blit_rect(const Ref<Image> &p_src, const Rect2 &p_src_rect, const Point2 &p_dest) {
    ERR_FAIL_COND_MSG(not p_src, "It's not a reference to a valid Image object.");
    int dsize = data.size();
    int srcdsize = p_src->data.size();
    ERR_FAIL_COND(dsize == 0);
    ERR_FAIL_COND(srcdsize == 0);
    ERR_FAIL_COND(format != p_src->format);
    ERR_FAIL_COND_MSG(!_can_modify(format), "Cannot blit_rect in compressed or custom image formats.");

    Rect2i clipped_src_rect = Rect2i(0, 0, p_src->width, p_src->height).clip(p_src_rect);

    if (p_dest.x < 0) {
        clipped_src_rect.position.x = std::abs(p_dest.x);
    }
    if (p_dest.y < 0) {
        clipped_src_rect.position.y = std::abs(p_dest.y);
    }

    if (clipped_src_rect.size.x <= 0 || clipped_src_rect.size.y <= 0) {
        return;
    }

    Point2 src_underscan = Point2(MIN(0, p_src_rect.position.x), MIN(0, p_src_rect.position.y));
    Rect2i dest_rect = Rect2i(0, 0, width, height).clip(Rect2i(p_dest - src_underscan, clipped_src_rect.size));

    PoolVector<uint8_t>::Write wp = data.write();
    uint8_t *dst_data_ptr = wp.ptr();

    PoolVector<uint8_t>::Read rp = p_src->data.read();
    const uint8_t *src_data_ptr = rp.ptr();

    int pixel_size = get_format_pixel_size(format);

    for (int i = 0; i < dest_rect.size.y; i++) {
        for (int j = 0; j < dest_rect.size.x; j++) {
            int src_x = clipped_src_rect.position.x + j;
            int src_y = clipped_src_rect.position.y + i;

            int dst_x = dest_rect.position.x + j;
            int dst_y = dest_rect.position.y + i;

            const uint8_t *src = &src_data_ptr[(src_y * p_src->width + src_x) * pixel_size];
            uint8_t *dst = &dst_data_ptr[(dst_y * width + dst_x) * pixel_size];

            for (int k = 0; k < pixel_size; k++) {
                dst[k] = src[k];
            }
        }
    }
}

void Image::blit_rect_mask(
        const Ref<Image> &p_src, const Ref<Image> &p_mask, const Rect2 &p_src_rect, const Point2 &p_dest) {
    ERR_FAIL_COND_MSG(not p_src, "It's not a reference to a valid Image object.");
    ERR_FAIL_COND_MSG(not p_mask, "It's not a reference to a valid Image object.");
    int dsize = data.size();
    int srcdsize = p_src->data.size();
    int maskdsize = p_mask->data.size();
    ERR_FAIL_COND(dsize == 0);
    ERR_FAIL_COND(srcdsize == 0);
    ERR_FAIL_COND(maskdsize == 0);
    ERR_FAIL_COND_MSG(p_src->width != p_mask->width, "Source image width is different from mask width.");
    ERR_FAIL_COND_MSG(p_src->height != p_mask->height, "Source image height is different from mask height.");
    ERR_FAIL_COND(format != p_src->format);

    Rect2i clipped_src_rect = Rect2i(0, 0, p_src->width, p_src->height).clip(p_src_rect);

    if (p_dest.x < 0) {
        clipped_src_rect.position.x = ABS(p_dest.x);
    }
    if (p_dest.y < 0) {
        clipped_src_rect.position.y = ABS(p_dest.y);
    }

    if (clipped_src_rect.size.x <= 0 || clipped_src_rect.size.y <= 0) {
        return;
    }

    Point2 src_underscan = Point2(MIN(0, p_src_rect.position.x), MIN(0, p_src_rect.position.y));
    Rect2i dest_rect = Rect2i(0, 0, width, height).clip(Rect2i(p_dest - src_underscan, clipped_src_rect.size));

    PoolVector<uint8_t>::Write wp = data.write();
    uint8_t *dst_data_ptr = wp.ptr();

    PoolVector<uint8_t>::Read rp = p_src->data.read();
    const uint8_t *src_data_ptr = rp.ptr();

    int pixel_size = get_format_pixel_size(format);

    const Ref<Image> &msk(p_mask);
    msk->lock();

    for (int i = 0; i < dest_rect.size.y; i++) {
        for (int j = 0; j < dest_rect.size.x; j++) {
            int src_x = clipped_src_rect.position.x + j;
            int src_y = clipped_src_rect.position.y + i;

            if (msk->get_pixel(src_x, src_y).a != 0.0f) {
                int dst_x = dest_rect.position.x + j;
                int dst_y = dest_rect.position.y + i;

                const uint8_t *src = &src_data_ptr[(src_y * p_src->width + src_x) * pixel_size];
                uint8_t *dst = &dst_data_ptr[(dst_y * width + dst_x) * pixel_size];

                for (int k = 0; k < pixel_size; k++) {
                    dst[k] = src[k];
                }
            }
        }
    }

    msk->unlock();
}

void Image::blend_rect(const Ref<Image> &p_src, const Rect2 &p_src_rect, const Point2 &p_dest) {
    ERR_FAIL_COND(not p_src);
    int dsize = data.size();
    int srcdsize = p_src->data.size();
    ERR_FAIL_COND(dsize == 0);
    ERR_FAIL_COND(srcdsize == 0);
    ERR_FAIL_COND(format != p_src->format);

    Rect2i clipped_src_rect = Rect2i(0, 0, p_src->width, p_src->height).clip(p_src_rect);

    if (p_dest.x < 0) {
        clipped_src_rect.position.x = std::abs(p_dest.x);
    }
    if (p_dest.y < 0) {
        clipped_src_rect.position.y = std::abs(p_dest.y);
    }

    if (clipped_src_rect.size.x <= 0 || clipped_src_rect.size.y <= 0) {
        return;
    }

    Point2 src_underscan = Point2(MIN(0, p_src_rect.position.x), MIN(0, p_src_rect.position.y));
    Rect2i dest_rect = Rect2i(0, 0, width, height).clip(Rect2i(p_dest - src_underscan, clipped_src_rect.size));

    lock();
    Image *img = p_src.get();
    img->lock();

    for (int i = 0; i < dest_rect.size.y; i++) {
        for (int j = 0; j < dest_rect.size.x; j++) {
            int src_x = clipped_src_rect.position.x + j;
            int src_y = clipped_src_rect.position.y + i;

            int dst_x = dest_rect.position.x + j;
            int dst_y = dest_rect.position.y + i;

            Color sc = img->get_pixel(src_x, src_y);
            Color dc = get_pixel(dst_x, dst_y);
            dc.r = (sc.a * sc.r + dc.a * (1.0f - sc.a) * dc.r);
            dc.g = (sc.a * sc.g + dc.a * (1.0f - sc.a) * dc.g);
            dc.b = (sc.a * sc.b + dc.a * (1.0f - sc.a) * dc.b);
            dc.a = (sc.a + dc.a * (1.0f - sc.a));
            set_pixel(dst_x, dst_y, dc);
        }
    }

    img->unlock();
    unlock();
}

void Image::blend_rect_mask(
        const Ref<Image> &p_src, const Ref<Image> &p_mask, const Rect2 &p_src_rect, const Point2 &p_dest) {
    ERR_FAIL_COND(not p_src);
    ERR_FAIL_COND(not p_mask);
    int dsize = data.size();
    int srcdsize = p_src->data.size();
    int maskdsize = p_mask->data.size();
    ERR_FAIL_COND(dsize == 0);
    ERR_FAIL_COND(srcdsize == 0);
    ERR_FAIL_COND(maskdsize == 0);
    ERR_FAIL_COND(p_src->width != p_mask->width);
    ERR_FAIL_COND(p_src->height != p_mask->height);
    ERR_FAIL_COND(format != p_src->format);

    Rect2i clipped_src_rect = Rect2i(0, 0, p_src->width, p_src->height).clip(p_src_rect);

    if (p_dest.x < 0) {
        clipped_src_rect.position.x = ABS(p_dest.x);
    }
    if (p_dest.y < 0) {
        clipped_src_rect.position.y = ABS(p_dest.y);
    }

    if (clipped_src_rect.size.x <= 0 || clipped_src_rect.size.y <= 0) {
        return;
    }

    Point2 src_underscan = Point2(MIN(0, p_src_rect.position.x), MIN(0, p_src_rect.position.y));
    Rect2i dest_rect = Rect2i(0, 0, width, height).clip(Rect2i(p_dest - src_underscan, clipped_src_rect.size));

    lock();
    Ref<Image> img = p_src;
    Ref<Image> msk = p_mask;
    img->lock();
    msk->lock();

    for (int i = 0; i < dest_rect.size.y; i++) {
        for (int j = 0; j < dest_rect.size.x; j++) {
            int src_x = clipped_src_rect.position.x + j;
            int src_y = clipped_src_rect.position.y + i;

            // If the mask's pixel is transparent then we skip it
            // Color c = msk->get_pixel(src_x, src_y);
            // if (c.a == 0) continue;
            if (msk->get_pixel(src_x, src_y).a != 0) {
                int dst_x = dest_rect.position.x + j;
                int dst_y = dest_rect.position.y + i;

                Color sc = img->get_pixel(src_x, src_y);
                Color dc = get_pixel(dst_x, dst_y);
                dc.r = (double)(sc.a * sc.r + dc.a * (1.0f - sc.a) * dc.r);
                dc.g = (double)(sc.a * sc.g + dc.a * (1.0f - sc.a) * dc.g);
                dc.b = (double)(sc.a * sc.b + dc.a * (1.0f - sc.a) * dc.b);
                dc.a = (double)(sc.a + dc.a * (1.0f - sc.a));
                set_pixel(dst_x, dst_y, dc);
            }
        }
    }

    msk->unlock();
    img->unlock();
    unlock();
}

void Image::fill(const Color &p_color) {
    ERR_FAIL_COND_MSG(!_can_modify(format), "Cannot fill in compressed or custom image formats.");

    lock();

    PoolVector<uint8_t>::Write wp = data.write();
    uint8_t *dst_data_ptr = wp.ptr();

    int pixel_size = get_format_pixel_size(format);

    // put first pixel with the format-aware API
    set_pixel(0, 0, p_color);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t *dst = &dst_data_ptr[(y * width + x) * pixel_size];

            for (int k = 0; k < pixel_size; k++) {
                dst[k] = dst_data_ptr[k];
            }
        }
    }

    unlock();
}

Vector<uint8_t> Image::lossy_packer(const Ref<Image> &p_image, float qualt) {
    Ref<Image> img = p_image;
    if (p_image->get_format() != ImageData::FORMAT_RGBA8 && p_image->get_format() != ImageData::FORMAT_RGB8) {
        img = dynamic_ref_cast<Image>(p_image->duplicate());
        if (img->detect_alpha()) {
            img->convert(ImageData::FORMAT_RGBA8);
        } else {
            img->convert(ImageData::FORMAT_RGB8);
        }
    }
    Vector<uint8_t> tmp;

    if (OK != ImageSaver::save_image("webp", p_image, tmp, qualt)) {
        return {};
    }
    return tmp;
}
Ref<Image> Image::webp_unpacker(const Vector<uint8_t> &p_buffer) {
    int size = p_buffer.size() - 4;
    ERR_FAIL_COND_V(size <= 0, Ref<Image>());
    const uint8_t *r = p_buffer.data();

    ERR_FAIL_COND_V(r[0] != 'W' || r[1] != 'E' || r[2] != 'B' || r[3] != 'P', Ref<Image>());

    Ref<Image> res(make_ref_counted<Image>());

    if (OK != res->_load_from_buffer(&r[4], size - 4, "webp")) {
        return {};
    }
    return res;
}
Vector<uint8_t> Image::lossless_packer(const Ref<Image> &p_image) {
    Ref<Image> img = prepareForPngStorage(p_image);
    Vector<uint8_t> tmp = { 'P', 'N', 'G', ' ' }; // Header marker bytes.

    if (OK != ImageSaver::save_image("png", p_image, tmp, 1.0f)) {
        return {};
    }
    return tmp;
}
Ref<Image> Image::png_unpacker(const Vector<uint8_t> &p_data) {
    const int len = p_data.size();
    ERR_FAIL_COND_V(len < 4, {});
    const uint8_t *r = p_data.data();
    ERR_FAIL_COND_V(r[0] != 'P' || r[1] != 'N' || r[2] != 'G' || r[3] != ' ', {});
    Ref<Image> res(make_ref_counted<Image>());

    if (OK != res->_load_from_buffer(&r[4], len - 4, "png")) {
        return {};
    }
    return res;
}

void Image::_set_data(const Dictionary &p_data) {
    ERR_FAIL_COND(!p_data.has("width"));
    ERR_FAIL_COND(!p_data.has("height"));
    ERR_FAIL_COND(!p_data.has("format"));
    ERR_FAIL_COND(!p_data.has("mipmaps"));
    ERR_FAIL_COND(!p_data.has("data"));

    int dwidth = p_data["width"].as<int>();
    int dheight = p_data["height"].as<int>();
    String dformat = p_data["format"].as<String>();
    bool dmipmaps = p_data["mipmaps"].as<bool>();
    PoolVector<uint8_t> ddata = p_data["data"].as<PoolVector<uint8_t>>();
    Format ddformat = FORMAT_MAX;
    for (int i = 0; i < FORMAT_MAX; i++) {
        if (dformat == get_format_name(Format(i))) {
            ddformat = Format(i);
            break;
        }
    }

    ERR_FAIL_COND(ddformat == FORMAT_MAX);

    create(dwidth, dheight, dmipmaps, ddformat, ddata);
}

Dictionary Image::_get_data() const {
    Dictionary d;
    d["width"] = width;
    d["height"] = height;
    d["format"] = get_format_name(format);
    d["mipmaps"] = Variant(mipmaps);
    d["data"] = data;
    return d;
}

void Image::lock() {
    ERR_FAIL_COND(data.size() == 0);
    write_lock = data.write();
}

void Image::unlock() {
    write_lock.release();
}

Color Image::get_pixelv(const Point2 &p_src) const {
    return get_pixel(p_src.x, p_src.y);
}

Color Image::_get_color_at_ofs(uint8_t *ptr, uint32_t ofs) const {
    switch (format) {
        case FORMAT_L8: {
            float l = ptr[ofs] / 255.0f;
            return Color(l, l, l, 1);
        }
        case FORMAT_LA8: {
            float l = ptr[ofs * 2 + 0] / 255.0f;
            float a = ptr[ofs * 2 + 1] / 255.0f;
            return Color(l, l, l, a);
        }
        case FORMAT_R8: {
            float r = ptr[ofs] / 255.0f;
            return Color(r, 0, 0, 1);
        }
        case FORMAT_RG8: {
            float r = ptr[ofs * 2 + 0] / 255.0f;
            float g = ptr[ofs * 2 + 1] / 255.0f;
            return Color(r, g, 0, 1);
        }
        case FORMAT_RGB8: {
            float r = ptr[ofs * 3 + 0] / 255.0f;
            float g = ptr[ofs * 3 + 1] / 255.0f;
            float b = ptr[ofs * 3 + 2] / 255.0f;
            return Color(r, g, b, 1);
        }
        case FORMAT_RGBA8: {
            float r = ptr[ofs * 4 + 0] / 255.0f;
            float g = ptr[ofs * 4 + 1] / 255.0f;
            float b = ptr[ofs * 4 + 2] / 255.0f;
            float a = ptr[ofs * 4 + 3] / 255.0f;
            return Color(r, g, b, a);
        }
        case FORMAT_RGBA4444: {
            uint16_t u = ((uint16_t *)ptr)[ofs];
            float r = ((u >> 12) & 0xF) / 15.0f;
            float g = ((u >> 8) & 0xF) / 15.0f;
            float b = ((u >> 4) & 0xF) / 15.0f;
            float a = (u & 0xF) / 15.0f;
            return Color(r, g, b, a);
        }
        case FORMAT_RGB565: {
            uint16_t u = ((uint16_t *)ptr)[ofs];
            float r = (u & 0x1F) / 31.0;
            float g = ((u >> 5) & 0x3F) / 63.0;
            float b = ((u >> 11) & 0x1F) / 31.0;
            return Color(r, g, b, 1.0);
        }
        case FORMAT_RF: {
            float r = ((float *)ptr)[ofs];
            return Color(r, 0, 0, 1);
        }
        case FORMAT_RGF: {
            float r = ((float *)ptr)[ofs * 2 + 0];
            float g = ((float *)ptr)[ofs * 2 + 1];
            return Color(r, g, 0, 1);
        }
        case FORMAT_RGBF: {
            float r = ((float *)ptr)[ofs * 3 + 0];
            float g = ((float *)ptr)[ofs * 3 + 1];
            float b = ((float *)ptr)[ofs * 3 + 2];
            return Color(r, g, b, 1);
        }
        case FORMAT_RGBAF: {
            float r = ((float *)ptr)[ofs * 4 + 0];
            float g = ((float *)ptr)[ofs * 4 + 1];
            float b = ((float *)ptr)[ofs * 4 + 2];
            float a = ((float *)ptr)[ofs * 4 + 3];
            return Color(r, g, b, a);
        }
        case FORMAT_RH: {
            uint16_t r = ((uint16_t *)ptr)[ofs];
            return Color(Math::half_to_float(r), 0, 0, 1);
        }
        case FORMAT_RGH: {
            uint16_t r = ((uint16_t *)ptr)[ofs * 2 + 0];
            uint16_t g = ((uint16_t *)ptr)[ofs * 2 + 1];
            return Color(Math::half_to_float(r), Math::half_to_float(g), 0, 1);
        }
        case FORMAT_RGBH: {
            uint16_t r = ((uint16_t *)ptr)[ofs * 3 + 0];
            uint16_t g = ((uint16_t *)ptr)[ofs * 3 + 1];
            uint16_t b = ((uint16_t *)ptr)[ofs * 3 + 2];
            return Color(Math::half_to_float(r), Math::half_to_float(g), Math::half_to_float(b), 1);
        }
        case FORMAT_RGBAH: {
            uint16_t r = ((uint16_t *)ptr)[ofs * 4 + 0];
            uint16_t g = ((uint16_t *)ptr)[ofs * 4 + 1];
            uint16_t b = ((uint16_t *)ptr)[ofs * 4 + 2];
            uint16_t a = ((uint16_t *)ptr)[ofs * 4 + 3];
            return Color(
                    Math::half_to_float(r), Math::half_to_float(g), Math::half_to_float(b), Math::half_to_float(a));
        }
        case FORMAT_RGBE9995: {
            return Color::from_rgbe9995(((uint32_t *)ptr)[ofs]);
        }
        default: {
            ERR_FAIL_V_MSG(Color(), "Can't get_pixel() on compressed image, sorry.");
        }
    }
}

void Image::_set_color_at_ofs(uint8_t *ptr, uint32_t ofs, const Color &p_color) {
    switch (format) {
        case FORMAT_L8: {
            ptr[ofs] = uint8_t(CLAMP<float>(p_color.get_v() * 255.0f, 0, 255));
        } break;
        case FORMAT_LA8: {
            ptr[ofs * 2 + 0] = uint8_t(CLAMP<float>(p_color.get_v() * 255.0f, 0, 255));
            ptr[ofs * 2 + 1] = uint8_t(CLAMP<float>(p_color.a * 255.0f, 0, 255));
        } break;
        case FORMAT_R8: {
            ptr[ofs] = uint8_t(CLAMP<float>(p_color.r * 255.0f, 0, 255));
        } break;
        case FORMAT_RG8: {
            ptr[ofs * 2 + 0] = uint8_t(CLAMP<float>(p_color.r * 255.0f, 0, 255));
            ptr[ofs * 2 + 1] = uint8_t(CLAMP<float>(p_color.g * 255.0f, 0, 255));
        } break;
        case FORMAT_RGB8: {
            ptr[ofs * 3 + 0] = uint8_t(CLAMP<float>(p_color.r * 255.0f, 0, 255));
            ptr[ofs * 3 + 1] = uint8_t(CLAMP<float>(p_color.g * 255.0f, 0, 255));
            ptr[ofs * 3 + 2] = uint8_t(CLAMP<float>(p_color.b * 255.0f, 0, 255));
        } break;
        case FORMAT_RGBA8: {
            ptr[ofs * 4 + 0] = uint8_t(CLAMP<float>(p_color.r * 255.0f, 0, 255));
            ptr[ofs * 4 + 1] = uint8_t(CLAMP<float>(p_color.g * 255.0f, 0, 255));
            ptr[ofs * 4 + 2] = uint8_t(CLAMP<float>(p_color.b * 255.0f, 0, 255));
            ptr[ofs * 4 + 3] = uint8_t(CLAMP<float>(p_color.a * 255.0f, 0, 255));

        } break;
        case FORMAT_RGBA4444: {
            uint16_t rgba = 0;

            rgba = uint16_t(CLAMP<float>(p_color.r * 15.0f, 0, 15)) << 12;
            rgba |= uint16_t(CLAMP<float>(p_color.g * 15.0f, 0, 15)) << 8;
            rgba |= uint16_t(CLAMP<float>(p_color.b * 15.0f, 0, 15)) << 4;
            rgba |= uint16_t(CLAMP<float>(p_color.a * 15.0f, 0, 15));
            ((uint16_t *)ptr)[ofs] = rgba;

        } break;
        case FORMAT_RGB565: {
            uint16_t rgba = 0;

            rgba = uint16_t(CLAMP<float>(p_color.r * 31.0, 0, 31));
            rgba |= uint16_t(CLAMP<float>(p_color.g * 63.0, 0, 33)) << 5;
            rgba |= uint16_t(CLAMP<float>(p_color.b * 31.0, 0, 31)) << 11;

            ((uint16_t *)ptr)[ofs] = rgba;

        } break;
        case FORMAT_RF: {
            ((float *)ptr)[ofs] = p_color.r;
        } break;
        case FORMAT_RGF: {
            ((float *)ptr)[ofs * 2 + 0] = p_color.r;
            ((float *)ptr)[ofs * 2 + 1] = p_color.g;
        } break;
        case FORMAT_RGBF: {
            ((float *)ptr)[ofs * 3 + 0] = p_color.r;
            ((float *)ptr)[ofs * 3 + 1] = p_color.g;
            ((float *)ptr)[ofs * 3 + 2] = p_color.b;
        } break;
        case FORMAT_RGBAF: {
            ((float *)ptr)[ofs * 4 + 0] = p_color.r;
            ((float *)ptr)[ofs * 4 + 1] = p_color.g;
            ((float *)ptr)[ofs * 4 + 2] = p_color.b;
            ((float *)ptr)[ofs * 4 + 3] = p_color.a;
        } break;
        case FORMAT_RH: {
            ((uint16_t *)ptr)[ofs] = Math::make_half_float(p_color.r);
        } break;
        case FORMAT_RGH: {
            ((uint16_t *)ptr)[ofs * 2 + 0] = Math::make_half_float(p_color.r);
            ((uint16_t *)ptr)[ofs * 2 + 1] = Math::make_half_float(p_color.g);
        } break;
        case FORMAT_RGBH: {
            ((uint16_t *)ptr)[ofs * 3 + 0] = Math::make_half_float(p_color.r);
            ((uint16_t *)ptr)[ofs * 3 + 1] = Math::make_half_float(p_color.g);
            ((uint16_t *)ptr)[ofs * 3 + 2] = Math::make_half_float(p_color.b);
        } break;
        case FORMAT_RGBAH: {
            ((uint16_t *)ptr)[ofs * 4 + 0] = Math::make_half_float(p_color.r);
            ((uint16_t *)ptr)[ofs * 4 + 1] = Math::make_half_float(p_color.g);
            ((uint16_t *)ptr)[ofs * 4 + 2] = Math::make_half_float(p_color.b);
            ((uint16_t *)ptr)[ofs * 4 + 3] = Math::make_half_float(p_color.a);
        } break;
        case FORMAT_RGBE9995: {
            ((uint32_t *)ptr)[ofs] = p_color.to_rgbe9995();

        } break;
        default: {
            ERR_FAIL_MSG("Can't set_pixel() on compressed image, sorry.");
        }
    }
}
Color Image::get_pixel(int p_x, int p_y) const {
    uint8_t *ptr = write_lock.ptr();
#ifdef DEBUG_ENABLED
    ERR_FAIL_COND_V_MSG(!ptr, Color(), "Image must be locked with 'lock()' before using get_pixel().");

    ERR_FAIL_INDEX_V(p_x, width, Color());
    ERR_FAIL_INDEX_V(p_y, height, Color());

#endif

    uint32_t ofs = p_y * width + p_x;
    return _get_color_at_ofs(ptr, ofs);
}

void Image::set_pixelv(const Point2 &p_dst, const Color &p_color) {
    set_pixel(p_dst.x, p_dst.y, p_color);
}

void Image::set_pixel(int p_x, int p_y, const Color &p_color) {
    uint8_t *ptr = write_lock.ptr();
#ifdef DEBUG_ENABLED
    ERR_FAIL_COND_MSG(!ptr, "Image must be locked with 'lock()' before using set_pixel().");

    ERR_FAIL_INDEX(p_x, width);
    ERR_FAIL_INDEX(p_y, height);

#endif

    uint32_t ofs = p_y * width + p_x;
    _set_color_at_ofs(ptr, ofs, p_color);
}

ImageUsedChannels Image::detect_used_channels(ImageCompressSource p_source) {
    ERR_FAIL_COND_V(data.size() == 0, ImageUsedChannels::USED_CHANNELS_RGBA);
    ERR_FAIL_COND_V(is_compressed(), ImageUsedChannels::USED_CHANNELS_RGBA);
    bool r = false, g = false, b = false, a = false, c = false;
    lock();
    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            Color col = get_pixel(i, j);

            if (col.r > 0.001f) {
                r = true;
            }
            if (col.g > 0.001f) {
                g = true;
            }
            if (col.b > 0.001f) {
                b = true;
            }
            if (col.a < 0.999f) {
                a = true;
            }

            if (col.r != col.b || col.r != col.g || col.b != col.g) {
                c = true;
            }
        }
    }

    unlock();

    ImageUsedChannels used_channels;

    if (!c && !a) {
        used_channels = ImageUsedChannels::USED_CHANNELS_L;
    } else if (!c && a) {
        used_channels = ImageUsedChannels::USED_CHANNELS_LA;
    } else if (r && !g && !b && !a) {
        used_channels = ImageUsedChannels::USED_CHANNELS_R;
    } else if (r && g && !b && !a) {
        used_channels = ImageUsedChannels::USED_CHANNELS_RG;
    } else if (r && g && b && !a) {
        used_channels = ImageUsedChannels::USED_CHANNELS_RGB;
    } else {
        used_channels = ImageUsedChannels::USED_CHANNELS_RGBA;
    }

    if (p_source == ImageCompressSource::COMPRESS_SOURCE_SRGB &&
            (used_channels == ImageUsedChannels::USED_CHANNELS_R ||
                    used_channels == ImageUsedChannels::USED_CHANNELS_RG)) {
        // R and RG do not support SRGB
        used_channels = ImageUsedChannels::USED_CHANNELS_RGB;
    }

    if (p_source == ImageCompressSource::COMPRESS_SOURCE_NORMAL) {
        // use RG channels only for normal
        used_channels = ImageUsedChannels::USED_CHANNELS_RG;
    }

    return used_channels;
}

void Image::optimize_channels() {
    switch (detect_used_channels()) {
        case ImageUsedChannels::USED_CHANNELS_L:
            convert(FORMAT_L8);
            break;
        case ImageUsedChannels::USED_CHANNELS_LA:
            convert(FORMAT_LA8);
            break;
        case ImageUsedChannels::USED_CHANNELS_R:
            convert(FORMAT_R8);
            break;
        case ImageUsedChannels::USED_CHANNELS_RG:
            convert(FORMAT_RG8);
            break;
        case ImageUsedChannels::USED_CHANNELS_RGB:
            convert(FORMAT_RGB8);
            break;
        case ImageUsedChannels::USED_CHANNELS_RGBA:
            convert(FORMAT_RGBA8);
            break;
    }
}

void Image::_bind_methods() {
    add_plugin_resolver(new CodecPluginResolver);

    SE_BIND_METHOD(Image,get_width);
    SE_BIND_METHOD(Image,get_height);
    SE_BIND_METHOD(Image,get_size);
    SE_BIND_METHOD(Image,has_mipmaps);
    SE_BIND_METHOD(Image,get_format);
    SE_BIND_METHOD(Image,get_data);

    SE_BIND_METHOD(Image,convert);

    SE_BIND_METHOD(Image,get_mipmap_offset);

    MethodBinder::bind_method(D_METHOD("resize_to_po2", { "square" }), &Image::resize_to_po2, { DEFVAL(false) });
    MethodBinder::bind_method(D_METHOD("resize", { "width", "height", "interpolation" }), &Image::resize,{ DEFVAL(INTERPOLATE_BILINEAR) });
    SE_BIND_METHOD(Image,shrink_x2);
    SE_BIND_METHOD(Image,expand_x2_hq2x);

    SE_BIND_METHOD(Image,crop);
    SE_BIND_METHOD(Image,flip_x);
    SE_BIND_METHOD(Image,flip_y);
    MethodBinder::bind_method(D_METHOD("generate_mipmaps", { "renormalize" }), &Image::generate_mipmaps, { DEFVAL(false) });
    SE_BIND_METHOD(Image,clear_mipmaps);

    MethodBinder::bind_method(D_METHOD("create", { "width", "height", "use_mipmaps", "format" }), &Image::_create_empty);
    MethodBinder::bind_method(D_METHOD("create_from_data", { "width", "height", "use_mipmaps", "format", "data" }),
            &Image::_create_from_data);

    SE_BIND_METHOD(Image,is_empty);

    SE_BIND_METHOD(Image,load);
    SE_BIND_METHOD(Image,save_png);
    MethodBinder::bind_method(D_METHOD("save_exr", { "path", "grayscale" }), &Image::save_exr, { DEFVAL(false) });

    SE_BIND_METHOD(Image,detect_alpha);
    SE_BIND_METHOD(Image,is_invisible);

    SE_BIND_METHOD_WITH_DEFAULTS(Image,detect_used_channels, DEFVAL(int(ImageCompressSource::COMPRESS_SOURCE_GENERIC)));
    SE_BIND_METHOD_WITH_DEFAULTS(Image, compress, DEFVAL(int(ImageCompressSource::COMPRESS_SOURCE_GENERIC)), DEFVAL(0.7f));
    SE_BIND_METHOD_WITH_DEFAULTS(Image, compress_from_channels, DEFVAL(0.7f));
    SE_BIND_METHOD(Image,decompress);
    SE_BIND_METHOD(Image,is_compressed);

    SE_BIND_METHOD(Image,fix_alpha_edges);
    SE_BIND_METHOD(Image,premultiply_alpha);
    SE_BIND_METHOD(Image,srgb_to_linear);
    SE_BIND_METHOD(Image,normalmap_to_xy);
    SE_BIND_METHOD(Image,rgbe_to_srgb);
    MethodBinder::bind_method(
            D_METHOD("bumpmap_to_normalmap", { "bump_scale" }), &Image::bumpmap_to_normalmap, { DEFVAL(1.0) });

    SE_BIND_METHOD(Image,blit_rect);
    SE_BIND_METHOD(Image,blit_rect_mask);
    SE_BIND_METHOD(Image,blend_rect);
    MethodBinder::bind_method(
            D_METHOD("blend_rect_mask", { "src", "mask", "src_rect", "dst" }), &Image::blend_rect_mask);
    SE_BIND_METHOD(Image,fill);

    SE_BIND_METHOD(Image,get_used_rect);
    SE_BIND_METHOD(Image,get_rect);

    MethodBinder::bind_method(D_METHOD("copy_from", { "src" }), &Image::copy_internals_from);

    SE_BIND_METHOD(Image,_set_data);
    SE_BIND_METHOD(Image,_get_data);

    SE_BIND_METHOD(Image,lock);
    SE_BIND_METHOD(Image,unlock);
    SE_BIND_METHOD(Image,get_pixelv);
    SE_BIND_METHOD(Image,get_pixel);
    SE_BIND_METHOD(Image,set_pixelv);
    SE_BIND_METHOD(Image,set_pixel);

    SE_BIND_METHOD(Image,load_png_from_buffer);
    SE_BIND_METHOD(Image,load_jpg_from_buffer);
    SE_BIND_METHOD(Image,load_webp_from_buffer);

    ADD_PROPERTY(PropertyInfo(VariantType::DICTIONARY, "data", PropertyHint::None, "", PROPERTY_USAGE_STORAGE),
            "_set_data", "_get_data");

    BIND_CONSTANT(MAX_WIDTH)
    BIND_CONSTANT(MAX_HEIGHT)

    BIND_ENUM_CONSTANT(FORMAT_L8); // luminance
    BIND_ENUM_CONSTANT(FORMAT_LA8); // luminance-alpha
    BIND_ENUM_CONSTANT(FORMAT_R8);
    BIND_ENUM_CONSTANT(FORMAT_RG8);
    BIND_ENUM_CONSTANT(FORMAT_RGB8);
    BIND_ENUM_CONSTANT(FORMAT_RGBA8);
    BIND_ENUM_CONSTANT(FORMAT_RGBA4444);
    BIND_ENUM_CONSTANT(FORMAT_RGB565);
    BIND_ENUM_CONSTANT(FORMAT_RF); // float
    BIND_ENUM_CONSTANT(FORMAT_RGF);
    BIND_ENUM_CONSTANT(FORMAT_RGBF);
    BIND_ENUM_CONSTANT(FORMAT_RGBAF);
    BIND_ENUM_CONSTANT(FORMAT_RH); // half float
    BIND_ENUM_CONSTANT(FORMAT_RGH);
    BIND_ENUM_CONSTANT(FORMAT_RGBH);
    BIND_ENUM_CONSTANT(FORMAT_RGBAH);
    BIND_ENUM_CONSTANT(FORMAT_RGBE9995);
    BIND_ENUM_CONSTANT(FORMAT_DXT1); // s3tc bc1
    BIND_ENUM_CONSTANT(FORMAT_DXT3); // bc2
    BIND_ENUM_CONSTANT(FORMAT_DXT5); // bc3
    BIND_ENUM_CONSTANT(FORMAT_RGTC_R);
    BIND_ENUM_CONSTANT(FORMAT_RGTC_RG);
    BIND_ENUM_CONSTANT(FORMAT_BPTC_RGBA); // btpc bc6h
    BIND_ENUM_CONSTANT(FORMAT_BPTC_RGBF); // float /
    BIND_ENUM_CONSTANT(FORMAT_BPTC_RGBFU); // unsigned float
    BIND_ENUM_CONSTANT(FORMAT_DXT5_RA_AS_RG);
    BIND_ENUM_CONSTANT(FORMAT_MAX);

    BIND_ENUM_CONSTANT(INTERPOLATE_NEAREST);
    BIND_ENUM_CONSTANT(INTERPOLATE_BILINEAR);
    BIND_ENUM_CONSTANT(INTERPOLATE_CUBIC);
    BIND_ENUM_CONSTANT(INTERPOLATE_TRILINEAR);
    BIND_ENUM_CONSTANT(INTERPOLATE_LANCZOS);

    BIND_ENUM_CONSTANT(ALPHA_NONE);
    BIND_ENUM_CONSTANT(ALPHA_BIT);
    BIND_ENUM_CONSTANT(ALPHA_BLEND);

    BIND_GLOBAL_ENUM_CONSTANT(ImageCompressMode::COMPRESS_S3TC);
    BIND_GLOBAL_ENUM_CONSTANT(ImageCompressMode::COMPRESS_BPTC);
    BIND_GLOBAL_ENUM_CONSTANT(ImageCompressMode::COMPRESS_MAX);

    BIND_GLOBAL_ENUM_CONSTANT(ImageUsedChannels::USED_CHANNELS_L);
    BIND_GLOBAL_ENUM_CONSTANT(ImageUsedChannels::USED_CHANNELS_LA);
    BIND_GLOBAL_ENUM_CONSTANT(ImageUsedChannels::USED_CHANNELS_R);
    BIND_GLOBAL_ENUM_CONSTANT(ImageUsedChannels::USED_CHANNELS_RG);
    BIND_GLOBAL_ENUM_CONSTANT(ImageUsedChannels::USED_CHANNELS_RGB);
    BIND_GLOBAL_ENUM_CONSTANT(ImageUsedChannels::USED_CHANNELS_RGBA);

    BIND_GLOBAL_ENUM_CONSTANT(ImageCompressSource::COMPRESS_SOURCE_GENERIC);
    BIND_GLOBAL_ENUM_CONSTANT(ImageCompressSource::COMPRESS_SOURCE_SRGB);
    BIND_GLOBAL_ENUM_CONSTANT(ImageCompressSource::COMPRESS_SOURCE_NORMAL);
    BIND_GLOBAL_ENUM_CONSTANT(ImageCompressSource::COMPRESS_SOURCE_LAYERED);
}

void Image::normalmap_to_xy() {
    convert(ImageData::FORMAT_RGBA8);

    {
        int len = data.size() / 4;
        PoolVector<uint8_t>::Write wp = data.write();
        unsigned char *data_ptr = wp.ptr();

        for (int i = 0; i < len; i++) {
            data_ptr[(i << 2) + 3] = data_ptr[(i << 2) + 0]; // x to w
            data_ptr[(i << 2) + 0] = data_ptr[(i << 2) + 1]; // y to xz
            data_ptr[(i << 2) + 2] = data_ptr[(i << 2) + 1];
        }
    }

    convert(ImageData::FORMAT_LA8);
}

Ref<Image> Image::rgbe_to_srgb() {
    if (data.size() == 0) {
        return Ref<Image>();
    }

    ERR_FAIL_COND_V(format != FORMAT_RGBE9995, Ref<Image>());

    Ref<Image> new_image(make_ref_counted<Image>());

    new_image->create(width, height, false, ImageData::FORMAT_RGB8);

    lock();

    new_image->lock();

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            new_image->set_pixel(col, row, get_pixel(col, row).to_srgb());
        }
    }

    unlock();
    new_image->unlock();

    if (has_mipmaps()) {
        new_image->generate_mipmaps();
    }

    return new_image;
}

Ref<Image> Image::get_image_from_mipmap(int p_mipamp) const {
    int ofs, size, w, h;
    get_mipmap_offset_size_and_dimensions(p_mipamp, ofs, size, w, h);

    PoolVector<uint8_t> new_data;
    new_data.resize(size);
    {
        PoolVector<uint8_t>::Write wr = new_data.write();
        PoolVector<uint8_t>::Read rd = data.read();
        memcpy(wr.ptr(), rd.ptr() + ofs, size);
    }

    Ref<Image> image(make_ref_counted<Image>());
    image->width = w;
    image->height = h;
    image->format = format;
    image->data = eastl::move(new_data);
    image->mipmaps = false;
    return image;
}

void Image::bumpmap_to_normalmap(float bump_scale) {
    ERR_FAIL_COND(!_can_modify(format));
    ERR_FAIL_COND_MSG(write_lock.ptr(), "Cannot modify image when it is locked.");

    convert(ImageData::FORMAT_RF);

    PoolVector<uint8_t> result_image; // rgba output
    result_image.resize(width * height * 4);

    {
        PoolVector<uint8_t>::Read rp = data.read();
        PoolVector<uint8_t>::Write wp = result_image.write();
        ERR_FAIL_COND(!rp.ptr());

        unsigned char *write_ptr = wp.ptr();
        float *read_ptr = (float *)rp.ptr();
        for (int ty = 0; ty < height; ty++) {
            int py = ty + 1;
            if (py >= height) {
                py -= height;
            }

            for (int tx = 0; tx < width; tx++) {
                int px = tx + 1;
                if (px >= width) {
                    px -= width;
                }
                float here = read_ptr[ty * width + tx];
                float to_right = read_ptr[ty * width + px];
                float above = read_ptr[py * width + tx];
                Vector3 up = Vector3(0, 1, (here - above) * bump_scale);
                Vector3 across = Vector3(1, 0, (to_right - here) * bump_scale);

                Vector3 normal = across.cross(up);
                normal.normalize();

                write_ptr[((ty * width + tx) << 2) + 0] = (127.5 + normal.x * 127.5);
                write_ptr[((ty * width + tx) << 2) + 1] = (127.5 + normal.y * 127.5);
                write_ptr[((ty * width + tx) << 2) + 2] = (127.5 + normal.z * 127.5);
                write_ptr[((ty * width + tx) << 2) + 3] = 255;
            }
        }
    }
    format = FORMAT_RGBA8;
    data = result_image;
}

void Image::srgb_to_linear() {
    if (data.size() == 0) {
        return;
    }

    static const uint8_t srgb2lin[256] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2,
        2, 2, 2, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10, 10, 10, 11, 11,
        11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 22, 22, 23, 23, 24, 24,
        25, 26, 26, 27, 27, 28, 29, 29, 30, 31, 31, 32, 33, 33, 34, 35, 36, 36, 37, 38, 38, 39, 40, 41, 42, 42, 43, 44,
        45, 46, 47, 47, 48, 49, 50, 51, 52, 53, 54, 55, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 70, 71,
        72, 73, 74, 75, 76, 77, 78, 80, 81, 82, 83, 84, 85, 87, 88, 89, 90, 92, 93, 94, 95, 97, 98, 99, 101, 102, 103,
        105, 106, 107, 109, 110, 112, 113, 114, 116, 117, 119, 120, 122, 123, 125, 126, 128, 129, 131, 132, 134, 135,
        137, 139, 140, 142, 144, 145, 147, 148, 150, 152, 153, 155, 157, 159, 160, 162, 164, 166, 167, 169, 171, 173,
        175, 176, 178, 180, 182, 184, 186, 188, 190, 192, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215,
        218, 220, 222, 224, 226, 228, 230, 232, 235, 237, 239, 241, 243, 245, 248, 250, 252, 255 };

    ERR_FAIL_COND(format != FORMAT_RGB8 && format != FORMAT_RGBA8);

    if (format == FORMAT_RGBA8) {
        int len = data.size() / 4;
        PoolVector<uint8_t>::Write wp = data.write();
        unsigned char *data_ptr = wp.ptr();

        for (int i = 0; i < len; i++) {
            data_ptr[(i << 2) + 0] = srgb2lin[data_ptr[(i << 2) + 0]];
            data_ptr[(i << 2) + 1] = srgb2lin[data_ptr[(i << 2) + 1]];
            data_ptr[(i << 2) + 2] = srgb2lin[data_ptr[(i << 2) + 2]];
        }

    } else if (format == FORMAT_RGB8) {
        int len = data.size() / 3;
        PoolVector<uint8_t>::Write wp = data.write();
        unsigned char *data_ptr = wp.ptr();

        for (int i = 0; i < len; i++) {
            data_ptr[(i * 3) + 0] = srgb2lin[data_ptr[(i * 3) + 0]];
            data_ptr[(i * 3) + 1] = srgb2lin[data_ptr[(i * 3) + 1]];
            data_ptr[(i * 3) + 2] = srgb2lin[data_ptr[(i * 3) + 2]];
        }
    }
}

void Image::premultiply_alpha() {
    if (data.size() == 0) {
        return;
    }

    if (format != FORMAT_RGBA8) {
        return; // not needed
    }

    PoolVector<uint8_t>::Write wp = data.write();
    unsigned char *data_ptr = wp.ptr();

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            uint8_t *ptr = &data_ptr[(i * width + j) * 4];

            ptr[0] = (uint16_t(ptr[0]) * uint16_t(ptr[3])) >> 8;
            ptr[1] = (uint16_t(ptr[1]) * uint16_t(ptr[3])) >> 8;
            ptr[2] = (uint16_t(ptr[2]) * uint16_t(ptr[3])) >> 8;
        }
    }
}

void Image::fix_alpha_edges() {
    if (data.size() == 0) {
        return;
    }

    if (format != FORMAT_RGBA8) {
        return; // not needed
    }

    PoolVector<uint8_t> dcopy = data;
    PoolVector<uint8_t>::Read rp = dcopy.read();
    const uint8_t *srcptr = rp.ptr();

    PoolVector<uint8_t>::Write wp = data.write();
    unsigned char *data_ptr = wp.ptr();

    const int max_radius = 4;
    const int alpha_threshold = 20;
    const int max_dist = 0x7FFFFFFF;

    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            const uint8_t *rptr = &srcptr[(i * width + j) * 4];
            uint8_t *wptr = &data_ptr[(i * width + j) * 4];

            if (rptr[3] >= alpha_threshold) {
                continue;
            }

            int closest_dist = max_dist;
            uint8_t closest_color[3];

            int from_x = M_MAX(0, j - max_radius);
            int to_x = MIN(width - 1, j + max_radius);
            int from_y = M_MAX(0, i - max_radius);
            int to_y = MIN(height - 1, i + max_radius);

            for (int k = from_y; k <= to_y; k++) {
                for (int l = from_x; l <= to_x; l++) {
                    int dy = i - k;
                    int dx = j - l;
                    int dist = dy * dy + dx * dx;
                    if (dist >= closest_dist) {
                        continue;
                    }

                    const uint8_t *rp2 = &srcptr[(k * width + l) << 2];

                    if (rp2[3] < alpha_threshold) {
                        continue;
                    }

                    closest_dist = dist;
                    closest_color[0] = rp2[0];
                    closest_color[1] = rp2[1];
                    closest_color[2] = rp2[2];
                }
            }

            if (closest_dist != max_dist) {
                wptr[0] = closest_color[0];
                wptr[1] = closest_color[1];
                wptr[2] = closest_color[2];
            }
        }
    }
}

StringView Image::get_format_name(Format p_format) {
    ERR_FAIL_INDEX_V(p_format, FORMAT_MAX, StringView());
    return format_names[p_format];
}

Error Image::load_png_from_buffer(const PoolVector<uint8_t> &p_array) {
    return _load_from_buffer(p_array, "png");
}

Error Image::load_jpg_from_buffer(const PoolVector<uint8_t> &p_array) {
    return _load_from_buffer(p_array, "jpg");
}

Error Image::load_webp_from_buffer(const PoolVector<uint8_t> &p_array) {
    return _load_from_buffer(p_array, "webp");
}

void Image::convert_rg_to_ra_rgba8() {
    ERR_FAIL_COND(format != FORMAT_RGBA8);
    ERR_FAIL_COND(!data.size());

    int s = data.size();
    PoolVector<uint8_t>::Write w = data.write();
    for (int i = 0; i < s; i += 4) {
        w[i + 3] = w[i + 1];
        w[i + 1] = 0;
        w[i + 2] = 0;
    }
}
void Image::convert_ra_rgba8_to_rg() {
    ERR_FAIL_COND(format != FORMAT_RGBA8);
    ERR_FAIL_COND(!data.size());

    int s = data.size();
    PoolVector<uint8_t>::Write w = data.write();
    for (int i = 0; i < s; i += 4) {
        w[i + 1] = w[i + 3];
        w[i + 2] = 0;
        w[i + 3] = 255;
    }
}
Error Image::load_from_buffer(const uint8_t *p_array, int size, const char *ext) {
    return _load_from_buffer(p_array, size, ext);
}

Error Image::_load_from_buffer(const PoolVector<uint8_t> &p_array, const char *ext) {
    int buffer_size = p_array.size();
    PoolVector<uint8_t>::Read r = p_array.read();
    return _load_from_buffer(r.ptr(), buffer_size, ext);
}

Error Image::_load_from_buffer(const uint8_t *p_array, int buffer_size, const char *ext) {
    ERR_FAIL_COND_V(buffer_size == 0, ERR_INVALID_PARAMETER);
    ERR_FAIL_COND_V(!ext, ERR_INVALID_PARAMETER);

    ImageData d = ImageLoader::load_image(ext, p_array, buffer_size);
    ERR_FAIL_COND_V(d.data.size() == 0, ERR_PARSE_ERROR);

    create(eastl::move(d));

    return OK;
}

Image::Image(const uint8_t *p_mem_png_jpg, int p_len) {
    width = 0;
    height = 0;
    mipmaps = false;
    format = FORMAT_L8;

    if (OK != _load_from_buffer(p_mem_png_jpg, p_len, "png")) {
        // retry as jpg
        _load_from_buffer(p_mem_png_jpg, p_len, "jpg");
    }
}

Ref<Resource> Image::duplicate(bool p_subresources) const {
    Ref<Image> copy(make_ref_counted<Image>());

    copy->_copy_internals_from(*this);
    return copy;
}

Image::Image() {
    width = 0;
    height = 0;
    mipmaps = false;
    format = FORMAT_L8;
}

Image::~Image() {
    if (write_lock.ptr()) {
        unlock();
    }
}

Ref<Image> prepareForPngStorage(const Ref<Image> &img) {
    Ref<Image> source_image = img;

    if (source_image->is_compressed()) {
        source_image = dynamic_ref_cast<Image>(source_image->duplicate());
        source_image->decompress();
    }
    ERR_FAIL_COND_V(source_image->is_compressed(), Ref<Image>());
    bool need_convert = source_image->get_format() != ImageData::FORMAT_L8 &&
                        source_image->get_format() != ImageData::FORMAT_LA8 &&
                        source_image->get_format() != ImageData::FORMAT_RGB8 &&
                        source_image->get_format() != ImageData::FORMAT_RGBA8;
    if (!need_convert) {
        return source_image;
    }
    if (img == source_image) { // create a copy to prevent modifications to img contents
        source_image = dynamic_ref_cast<Image>(source_image->duplicate());
    }

    if (source_image->detect_alpha()) {
        source_image->convert(ImageData::FORMAT_RGBA8);
    } else {
        source_image->convert(ImageData::FORMAT_RGB8);
    }
    return source_image;
}
