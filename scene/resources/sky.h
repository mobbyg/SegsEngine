/*************************************************************************/
/*  sky.h                                                                */
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

#pragma once

#include "core/resource.h"
#include "core/rid.h"
#include "core/color.h"
#include "core/os/thread.h"

class Texture;
class Image;
class Thread;

class GODOT_EXPORT Sky : public Resource {
    GDCLASS(Sky,Resource)

public:
    enum RadianceSize {
        RADIANCE_SIZE_32,
        RADIANCE_SIZE_64,
        RADIANCE_SIZE_128,
        RADIANCE_SIZE_256,
        RADIANCE_SIZE_512,
        RADIANCE_SIZE_1024,
        RADIANCE_SIZE_2048,
        RADIANCE_SIZE_MAX
    };

private:
    RadianceSize radiance_size;

protected:
    static void _bind_methods();
    virtual void _radiance_changed() = 0;

public:
    void set_radiance_size(RadianceSize p_size);
    RadianceSize get_radiance_size() const;
    Sky();
};



class GODOT_EXPORT PanoramaSky : public Sky {
    GDCLASS(PanoramaSky,Sky)

private:
    RenderingEntity sky;
    Ref<Texture> panorama;

protected:
    static void _bind_methods();
    void _radiance_changed() override;

public:
    void set_panorama(const Ref<Texture> &p_panorama);
    Ref<Texture> get_panorama() const;

    RenderingEntity get_rid() const override;

    PanoramaSky();
    ~PanoramaSky() override;
};

class GODOT_EXPORT ProceduralSky : public Sky {
    GDCLASS(ProceduralSky,Sky)

public:
    enum TextureSize {
        TEXTURE_SIZE_256,
        TEXTURE_SIZE_512,
        TEXTURE_SIZE_1024,
        TEXTURE_SIZE_2048,
        TEXTURE_SIZE_4096,
        TEXTURE_SIZE_MAX
    };

private:
    Thread sky_thread;
    Color sky_top_color;
    Color sky_horizon_color;
    float sky_curve;
    float sky_energy;

    Color ground_bottom_color;
    Color ground_horizon_color;
    float ground_curve;
    float ground_energy;

    Color sun_color;
    float sun_latitude;
    float sun_longitude;
    float sun_angle_min;
    float sun_angle_max;
    float sun_curve;
    float sun_energy;

    TextureSize texture_size;

    RenderingEntity sky;
    RenderingEntity texture;
    Ref<Image> panorama;

    bool update_queued;
    bool regen_queued;

    bool first_time;

    void _thread_done(const Ref<Image> &p_image);
    static void _thread_function(void *p_ud);

protected:
    static void _bind_methods();
    void _radiance_changed() override;

    Ref<Image> _generate_sky();
    void _update_sky();

    void _queue_update();

public:
    void set_sky_top_color(const Color &p_sky_top);
    Color get_sky_top_color() const;

    void set_sky_horizon_color(const Color &p_sky_horizon);
    Color get_sky_horizon_color() const;

    void set_sky_curve(float p_curve);
    float get_sky_curve() const;

    void set_sky_energy(float p_energy);
    float get_sky_energy() const;

    void set_ground_bottom_color(const Color &p_ground_bottom);
    Color get_ground_bottom_color() const;

    void set_ground_horizon_color(const Color &p_ground_horizon);
    Color get_ground_horizon_color() const;

    void set_ground_curve(float p_curve);
    float get_ground_curve() const;

    void set_ground_energy(float p_energy);
    float get_ground_energy() const;

    void set_sun_color(const Color &p_sun);
    Color get_sun_color() const;

    void set_sun_latitude(float p_angle);
    float get_sun_latitude() const;

    void set_sun_longitude(float p_angle);
    float get_sun_longitude() const;

    void set_sun_angle_min(float p_angle);
    float get_sun_angle_min() const;

    void set_sun_angle_max(float p_angle);
    float get_sun_angle_max() const;

    void set_sun_curve(float p_curve);
    float get_sun_curve() const;

    void set_sun_energy(float p_energy);
    float get_sun_energy() const;

    void set_texture_size(TextureSize p_size);
    TextureSize get_texture_size() const;

    Ref<Image> get_data() const;

    RenderingEntity get_rid() const override;

    ProceduralSky(bool p_desaturate=false);
    ~ProceduralSky() override;
};
