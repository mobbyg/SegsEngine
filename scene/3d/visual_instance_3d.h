/*************************************************************************/
/*  visual_instance_3d.h                                                    */
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

#include "core/math/face3.h"
#include "core/rid.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/cull_instance_component.h"
#include "scene/resources/material.h"

class GODOT_EXPORT VisualInstance3D : public Node3D {

    GDCLASS(VisualInstance3D,Node3D)

    OBJ_CATEGORY("3D Visual Nodes")

    RenderingEntity base=entt::null;
    RenderingEntity instance;
    uint32_t layers;

protected:
    void _update_visibility();
    void set_instance_use_identity_transform(bool p_enable);

    void _notification(int p_what);
    static void _bind_methods();

public:
    enum GetFacesFlags {
        FACES_SOLID = 1, // solid geometry
        FACES_ENCLOSING = 2,
        FACES_DYNAMIC = 4 // dynamic object geometry
    };

    RenderingEntity get_instance() const { return instance; }
    virtual AABB get_aabb() const = 0;
    virtual Vector<Face3> get_faces(uint32_t p_usage_flags) const = 0;

    virtual AABB get_transformed_aabb() const; // helper

    void set_base(RenderingEntity p_base);
    RenderingEntity get_base() const { return base; }

    void set_layer_mask(uint32_t p_mask);
    uint32_t get_layer_mask() const { return layers; }

    void set_layer_mask_bit(int p_layer, bool p_enable);
    bool get_layer_mask_bit(int p_layer) const;

    VisualInstance3D();
    ~VisualInstance3D() override;
};

class GODOT_EXPORT GeometryInstance : public VisualInstance3D {

    GDCLASS(GeometryInstance,VisualInstance3D)

public:
    enum Flags {
        FLAG_USE_BAKED_LIGHT = RS::INSTANCE_FLAG_USE_BAKED_LIGHT,
        FLAG_DRAW_NEXT_FRAME_IF_VISIBLE = RS::INSTANCE_FLAG_DRAW_NEXT_FRAME_IF_VISIBLE,
        FLAG_MAX = RS::INSTANCE_FLAG_MAX,
    };
    enum LightmapScale {
        LIGHTMAP_SCALE_1X,
        LIGHTMAP_SCALE_2X,
        LIGHTMAP_SCALE_4X,
        LIGHTMAP_SCALE_8X,
        LIGHTMAP_SCALE_MAX,
    };
    enum ShadowCastingSetting {
        SHADOW_CASTING_SETTING_OFF = RS::SHADOW_CASTING_SETTING_OFF,
        SHADOW_CASTING_SETTING_ON = RS::SHADOW_CASTING_SETTING_ON,
        SHADOW_CASTING_SETTING_DOUBLE_SIDED = RS::SHADOW_CASTING_SETTING_DOUBLE_SIDED,
        SHADOW_CASTING_SETTING_SHADOWS_ONLY = RS::SHADOW_CASTING_SETTING_SHADOWS_ONLY
    };

private:
    bool flags[FLAG_MAX];
    bool generate_lightmap;
    LightmapScale lightmap_scale;
    ShadowCastingSetting shadow_casting_setting;
    Ref<Material> material_override;
    Ref<Material> material_overlay;
    float lod_min_distance;
    float lod_max_distance;
    float lod_min_hysteresis;
    float lod_max_hysteresis;

    float extra_cull_margin;

protected:
    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_flag(Flags p_flag, bool p_value);
    bool get_flag(Flags p_flag) const;

    void set_cast_shadows_setting(ShadowCastingSetting p_shadow_casting_setting);
    ShadowCastingSetting get_cast_shadows_setting() const;

    void set_generate_lightmap(bool p_enabled);
    bool get_generate_lightmap();

    void set_lightmap_scale(LightmapScale p_scale);
    LightmapScale get_lightmap_scale() const;
    void set_lod_min_distance(float p_dist);
    float get_lod_min_distance() const { return lod_min_distance; }

    void set_lod_max_distance(float p_dist);
    float get_lod_max_distance() const;

    void set_lod_min_hysteresis(float p_dist);
    float get_lod_min_hysteresis() const;

    void set_lod_max_hysteresis(float p_dist);
    float get_lod_max_hysteresis() const;

    virtual void set_material_override(const Ref<Material> &p_material);
    Ref<Material> get_material_override() const;

    virtual void set_material_overlay(const Ref<Material> &p_material);
    const Ref<Material> &get_material_overlay() const { return material_overlay; }

    void set_extra_cull_margin(float p_margin);
    float get_extra_cull_margin() const;

    void set_custom_aabb(AABB aabb);

    GeometryInstance();
};
