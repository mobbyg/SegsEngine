/*************************************************************************/
/*  animation_tree.h                                                     */
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

#include "scene/animation/animation_player.h"
#include "core/hash_map.h"
#include "core/hash_set.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/animation.h"

class AnimationNodeBlendTree;
class AnimationPlayer;
class AnimationTree;

class GODOT_EXPORT AnimationNode : public Resource {
    GDCLASS(AnimationNode,Resource)

public:
    enum FilterAction {
        FILTER_IGNORE,
        FILTER_PASS,
        FILTER_STOP,
        FILTER_BLEND
    };

    struct Input {
        String name;
    };

    Vector<Input> inputs;


    friend class AnimationTree;

    struct AnimationState {

        Ref<Animation> animation;
        float time;
        float delta;
        const Vector<float> *track_blends;
        float blend;
        bool seeked;
    };

    struct State {

        int track_count;
        HashMap<NodePath, int> track_map;
        Vector<AnimationState> animation_states;
        bool valid;
        AnimationPlayer *player;
        AnimationTree *tree;
        String invalid_reasons;
        uint64_t last_pass;
    };

    Vector<float> blends;
    State *state;

    float _pre_process(const StringName &p_base_path, AnimationNode *p_parent, State *p_state, float p_time, bool p_seek, const Vector<StringName> &p_connections);

    //all this is temporary
    StringName base_path;
    Vector<StringName> connections;
    AnimationNode *parent;

    HashSet<NodePath> filter;
    bool filter_enabled;

    Array _get_filters() const;
    void _set_filters(const Array &p_filters);
    friend class AnimationNodeBlendTree;
    float _blend_node(const StringName &p_subpath, const Vector<StringName> &p_connections, AnimationNode *p_new_parent, Ref<AnimationNode> p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter = FILTER_IGNORE, bool p_optimize = true, float *r_max = nullptr);
public:
    void blend_animation(const StringName &p_animation, float p_time, float p_delta, bool p_seeked, float p_blend);
    float blend_node(const StringName &p_sub_path, const Ref<AnimationNode>& p_node, float p_time, bool p_seek, float p_blend, FilterAction p_filter = FILTER_IGNORE, bool p_optimize = true);
    float blend_input(int p_input, float p_time, bool p_seek, float p_blend, FilterAction p_filter = FILTER_IGNORE, bool p_optimize = true);
    void make_invalid(const String &p_reason);

protected:
    static void _bind_methods();

    void _validate_property(PropertyInfo &property) const override;


public:
    virtual void get_parameter_list(Vector<PropertyInfo> *r_list) const;
    virtual Variant get_parameter_default_value(const StringName &p_parameter) const;

    void set_parameter(const StringName &p_name, const Variant &p_value);
    Variant get_parameter(const StringName &p_name) const;

    struct ChildNode {
        StringName name;
        Ref<AnimationNode> node;
    };

    virtual void get_child_nodes(Vector<ChildNode> *r_child_nodes);

    virtual float process(float p_time, bool p_seek);
    virtual StringView get_caption() const;

    int get_input_count() const;
    String get_input_name(int p_input);

    void add_input(const String &p_name);
    void set_input_name(int p_input, StringView p_name);
    void remove_input(int p_index);

    void set_filter_path(const NodePath &p_path, bool p_enable);
    bool is_path_filtered(const NodePath &p_path) const;

    void set_filter_enabled(bool p_enable);
    bool is_filter_enabled() const;

    virtual bool has_filter() const;

    virtual Ref<AnimationNode> get_child_by_name(const StringName &p_name);

    AnimationNode();
};


//root node does not allow inputs
class GODOT_EXPORT AnimationRootNode : public AnimationNode {
    GDCLASS(AnimationRootNode,AnimationNode)

public:
    AnimationRootNode() {}
};

class GODOT_EXPORT AnimationTree : public Node {
    GDCLASS(AnimationTree,Node)

public:
    enum AnimationProcessMode {
        ANIMATION_PROCESS_PHYSICS,
        ANIMATION_PROCESS_IDLE,
        ANIMATION_PROCESS_MANUAL,
    };

private:
    struct TrackCache {

        uint64_t setup_pass=0;
        uint64_t process_pass=0;
        Object *object=nullptr;
        GameEntity object_id {entt::null};
        Animation::TrackType type;
        bool root_motion=false;

        virtual ~TrackCache() {}
    };

    struct TrackCacheTransform : public TrackCache {
        Node3D *spatial;
        Skeleton *skeleton;
        int bone_idx;
        Quat rot;
        Vector3 loc;
        float rot_blend_accum;
        Vector3 scale;

        TrackCacheTransform() {
            type = Animation::TYPE_TRANSFORM;
            spatial = nullptr;
            bone_idx = -1;
            skeleton = nullptr;
        }
    };

    struct TrackCacheValue : public TrackCache {

        Variant value;
        Vector<StringName> subpath;
        TrackCacheValue() { type = Animation::TYPE_VALUE; }
    };

    struct TrackCacheMethod : public TrackCache {

        TrackCacheMethod() { type = Animation::TYPE_METHOD; }
    };

    struct TrackCacheBezier : public TrackCache {

        float value;
        Vector<StringName> subpath;
        TrackCacheBezier() {
            type = Animation::TYPE_BEZIER;
            value = 0;
        }
    };

    struct TrackCacheAudio : public TrackCache {

        bool playing;
        float start;
        float len;

        TrackCacheAudio() {
            type = Animation::TYPE_AUDIO;
            playing = false;
            start = 0;
            len = 0;
        }
    };

    struct TrackCacheAnimation : public TrackCache {

        bool playing;

        TrackCacheAnimation() {
            type = Animation::TYPE_ANIMATION;
            playing = false;
        }
    };
    struct Activity {
        uint64_t last_pass;
        float activity;
    };

    HashMap<NodePath, TrackCache *> track_cache;
    HashSet<TrackCache *> playing_caches;

    Ref<AnimationNode> root;

    AnimationProcessMode process_mode=ANIMATION_PROCESS_IDLE;
    NodePath animation_player;

    AnimationNode::State state;

    NodePath root_motion_track;
    Transform root_motion_transform;

    Vector<PropertyInfo> properties;
    HashMap<StringName, HashMap<StringName, StringName> > property_parent_map;
    HashMap<StringName, Variant> property_map;


    HashMap<StringName, Vector<Activity> > input_activity_map;
    HashMap<StringName, Vector<Activity> *> input_activity_map_get;
    uint64_t setup_pass=1;
    uint64_t process_pass=1;
    GameEntity last_animation_player;
    bool active = false;
    bool cache_valid = false;
    bool started = true;
    bool properties_dirty=true;

    friend class AnimationNode;

    void _node_removed(Node *p_node);

    void _clear_caches();
    bool _update_caches(AnimationPlayer *player);
    void _process_graph(float p_delta);
    void _tree_changed();
    void _update_properties();
    void _update_properties_for_node(const StringName &p_base_path, Ref<AnimationNode> node);


protected:
    bool _set(const StringName &p_name, const Variant &p_value);
    bool _get(const StringName &p_name, Variant &r_ret) const;
    void _get_property_list(Vector<PropertyInfo> *p_list) const;

    void _notification(int p_what);
    static void _bind_methods();

public:
    void set_tree_root(const Ref<AnimationNode> &p_root);
    Ref<AnimationNode> get_tree_root() const;

    void set_active(bool p_active);
    bool is_active() const;

    void set_process_mode(AnimationProcessMode p_mode);
    AnimationProcessMode get_process_mode() const;

    void set_animation_player(const NodePath &p_player);
    NodePath get_animation_player() const;

    String get_configuration_warning() const override;

    bool is_state_invalid() const;
    String get_invalid_state_reason() const;

    void set_root_motion_track(const NodePath &p_track);
    NodePath get_root_motion_track() const;

    Transform get_root_motion_transform() const;

    float get_connection_activity(const StringName &p_path, int p_connection) const;
    void advance(float p_time);

    void rename_parameter(StringView p_base, StringView p_new_base);

    uint64_t get_last_process_pass() const;
    AnimationTree();
    ~AnimationTree() override;
};
