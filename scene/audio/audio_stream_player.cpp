/*************************************************************************/
/*  audio_stream_player.cpp                                              */
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

#include "audio_stream_player.h"

#include "core/callable_method_pointer.h"
#include "core/object_tooling.h"
#include "core/method_bind.h"
#include "core/engine.h"

IMPL_GDCLASS(AudioStreamPlayer)
VARIANT_ENUM_CAST(AudioStreamPlayer::MixTarget);

void AudioStreamPlayer::_mix_to_bus(const AudioFrame *p_frames, int p_amount) {

    int bus_index = AudioServer::get_singleton()->thread_find_bus_index(bus);

    AudioFrame *targets[4] = { nullptr, nullptr, nullptr, nullptr };

    if (AudioServer::get_singleton()->get_speaker_mode() == AudioServer::SPEAKER_MODE_STEREO) {
        targets[0] = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, 0);
    } else {
        switch (mix_target) {
            case MIX_TARGET_STEREO: {
                targets[0] = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, 0);
            } break;
            case MIX_TARGET_SURROUND: {
                for (int i = 0; i < AudioServer::get_singleton()->get_channel_count(); i++) {
                    targets[i] = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, i);
                }
            } break;
            case MIX_TARGET_CENTER: {
                targets[0] = AudioServer::get_singleton()->thread_get_channel_mix_buffer(bus_index, 1);
            } break;
        }
    }

    for (int c = 0; c < 4; c++) {
        if (!targets[c])
            break;
        for (int i = 0; i < p_amount; i++) {
            targets[c][i] += p_frames[i];
        }
    }
}

void AudioStreamPlayer::_mix_internal(bool p_fadeout) {

    //get data
    AudioFrame *buffer = mix_buffer.data();
    int buffer_size = mix_buffer.size();

    if (p_fadeout) {
        // Short fadeout ramp
        buffer_size = MIN(buffer_size, 128);
    }

    stream_playback->mix(buffer, pitch_scale, buffer_size);

    //multiply volume interpolating to avoid clicks if this changes
    float target_volume = p_fadeout ? -80.0 : volume_db;
    float vol = Math::db2linear(mix_volume_db);
    float vol_inc = (Math::db2linear(target_volume) - vol) / float(buffer_size);

    for (int i = 0; i < buffer_size; i++) {
        buffer[i] *= vol;
        vol += vol_inc;
    }

    //set volume for next mix
    mix_volume_db = target_volume;

    _mix_to_bus(buffer, buffer_size);
}

void AudioStreamPlayer::_mix_audio() {

    if (use_fadeout) {
        _mix_to_bus(fadeout_buffer.data(), fadeout_buffer.size());
        use_fadeout = false;
    }

    if (not stream_playback || !active.is_set() ||
            (stream_paused && !stream_paused_fade)) {
        return;
    }

    if (stream_paused) {
        if (stream_paused_fade && stream_playback->is_playing()) {
            _mix_internal(true);
            stream_paused_fade = false;
        }
        return;
    }

    if (setstop.is_set()) {
        _mix_internal(true);
        stream_playback->stop();
        setstop.clear();
    }

    if (setseek.get() >= 0.0f && !stop_has_priority.is_set()) {
        if (stream_playback->is_playing()) {

            //fade out to avoid pops
            _mix_internal(true);
        }

        stream_playback->start(setseek.get());
        setseek.set(-1.0f); //reset seek
        mix_volume_db = volume_db; //reset ramp
    }

    stop_has_priority.clear();

    _mix_internal(false);
}

void AudioStreamPlayer::_notification(int p_what) {

    if (p_what == NOTIFICATION_ENTER_TREE) {

        AudioServer::get_singleton()->add_callback(_mix_audios, this);
        if (autoplay && !Engine::get_singleton()->is_editor_hint()) {
            play();
        }
    }

    if (p_what == NOTIFICATION_INTERNAL_PROCESS) {

        if (!active.is_set() || (setseek.get() < 0 && !stream_playback->is_playing())) {
            active.clear();
            set_process_internal(false);
            emit_signal("finished");
        }
    }

    if (p_what == NOTIFICATION_EXIT_TREE) {

        AudioServer::get_singleton()->remove_callback(_mix_audios, this);
    }

    if (p_what == NOTIFICATION_PAUSED) {
        if (!can_process()) {
            // Node can't process so we start fading out to silence
            set_stream_paused(true);
        }
    }

    if (p_what == NOTIFICATION_UNPAUSED) {
        set_stream_paused(false);
    }
}

void AudioStreamPlayer::set_stream(Ref<AudioStream> p_stream) {
    // Instancing audio streams can cause large memory allocations, do it prior to AudioServer::lock.
    Ref<AudioStreamPlayback> pre_instanced_playback;
    if (p_stream) {
        pre_instanced_playback = p_stream->instance_playback();
    }

    AudioServer::get_singleton()->lock();

    if (active.is_set() && stream_playback && !stream_paused) {
        //changing streams out of the blue is not a great idea, but at least
        //lets try to somehow avoid a click

        AudioFrame *buffer = fadeout_buffer.data();
        int buffer_size = fadeout_buffer.size();

        stream_playback->mix(buffer, pitch_scale, buffer_size);

        //multiply volume interpolating to avoid clicks if this changes
        float target_volume = -80.0;
        float vol = Math::db2linear(mix_volume_db);
        float vol_inc = (Math::db2linear(target_volume) - vol) / float(buffer_size);

        for (int i = 0; i < buffer_size; i++) {
            buffer[i] *= vol;
            vol += vol_inc;
        }

        use_fadeout = true;
    }

    mix_buffer.resize(AudioServer::get_singleton()->thread_get_mix_buffer_size());

    if (stream_playback) {
        stream_playback.unref();
        stream.unref();
        active.clear();
        setseek.set(-1);
        setstop.clear();
    }

    if (p_stream) {
        stream = p_stream;
        stream_playback = pre_instanced_playback;
    }

    AudioServer::get_singleton()->unlock();

    if (p_stream && not stream_playback) {
        stream.unref();
    }
}

Ref<AudioStream> AudioStreamPlayer::get_stream() const {

    return stream;
}

void AudioStreamPlayer::set_volume_db(float p_volume) {

    volume_db = p_volume;
}
float AudioStreamPlayer::get_volume_db() const {

    return volume_db;
}

void AudioStreamPlayer::set_pitch_scale(float p_pitch_scale) {
    ERR_FAIL_COND(p_pitch_scale <= 0.0);
    pitch_scale = p_pitch_scale;
}
float AudioStreamPlayer::get_pitch_scale() const {
    return pitch_scale;
}

void AudioStreamPlayer::play(float p_from_pos) {

    if (stream_playback) {
        //mix_volume_db = volume_db; do not reset volume ramp here, can cause clicks
        setseek.set(p_from_pos);
        stop_has_priority.clear();
        active.set();
        set_process_internal(true);
    }
}

void AudioStreamPlayer::seek(float p_seconds) {

    if (stream_playback) {
        setseek.set(p_seconds);
    }
}

void AudioStreamPlayer::stop() {

    if (stream_playback && active.is_set()) {
        setstop.set();
        stop_has_priority.set();
    }
}

bool AudioStreamPlayer::is_playing() const {

    if (stream_playback) {
        return active.is_set() && !setstop.is_set(); //&& stream_playback->is_playing();
    }

    return false;
}

float AudioStreamPlayer::get_playback_position() {

    if (stream_playback) {
        float ss = setseek.get();
        if (ss >= 0.0) {
            return ss;
        }
        return stream_playback->get_playback_position();
    }

    return 0;
}

void AudioStreamPlayer::set_bus(const StringName &p_bus) {

    //if audio is active, must lock this
    AudioServer::get_singleton()->lock();
    bus = p_bus;
    AudioServer::get_singleton()->unlock();
}
StringName AudioStreamPlayer::get_bus() const {

    for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
        if (AudioServer::get_singleton()->get_bus_name(i) == bus) {
            return bus;
        }
    }
    return "Master";
}

void AudioStreamPlayer::set_autoplay(bool p_enable) {

    autoplay = p_enable;
}
bool AudioStreamPlayer::is_autoplay_enabled() {

    return autoplay;
}

void AudioStreamPlayer::set_mix_target(MixTarget p_target) {

    mix_target = p_target;
}

AudioStreamPlayer::MixTarget AudioStreamPlayer::get_mix_target() const {

    return mix_target;
}

void AudioStreamPlayer::_set_playing(bool p_enable) {

    if (p_enable)
        play();
    else
        stop();
}
bool AudioStreamPlayer::_is_active() const {

    return active.is_set();
}

void AudioStreamPlayer::set_stream_paused(bool p_pause) {

    if (p_pause != stream_paused) {
        stream_paused = p_pause;
        stream_paused_fade = p_pause;
    }
}

bool AudioStreamPlayer::get_stream_paused() const {

    return stream_paused;
}

void AudioStreamPlayer::_validate_property(PropertyInfo &property) const {

    if (property.name == "bus") {

        String options;
        for (int i = 0; i < AudioServer::get_singleton()->get_bus_count(); i++) {
            if (i > 0)
                options += ',';
            String name(AudioServer::get_singleton()->get_bus_name(i));
            options += name;
        }

        property.hint_string = options;
    }
}

void AudioStreamPlayer::_bus_layout_changed() {

    Object_change_notify(this);
}

Ref<AudioStreamPlayback> AudioStreamPlayer::get_stream_playback() {
    return stream_playback;
}

void AudioStreamPlayer::_bind_methods() {

    SE_BIND_METHOD(AudioStreamPlayer,set_stream);
    SE_BIND_METHOD(AudioStreamPlayer,get_stream);

    SE_BIND_METHOD(AudioStreamPlayer,set_volume_db);
    SE_BIND_METHOD(AudioStreamPlayer,get_volume_db);

    SE_BIND_METHOD(AudioStreamPlayer,set_pitch_scale);
    SE_BIND_METHOD(AudioStreamPlayer,get_pitch_scale);

    MethodBinder::bind_method(D_METHOD("play", {"from_position"}), &AudioStreamPlayer::play, {DEFVAL(0.0)});
    SE_BIND_METHOD(AudioStreamPlayer,seek);
    SE_BIND_METHOD(AudioStreamPlayer,stop);

    SE_BIND_METHOD(AudioStreamPlayer,is_playing);
    SE_BIND_METHOD(AudioStreamPlayer,get_playback_position);

    SE_BIND_METHOD(AudioStreamPlayer,set_bus);
    SE_BIND_METHOD(AudioStreamPlayer,get_bus);

    SE_BIND_METHOD(AudioStreamPlayer,set_autoplay);
    SE_BIND_METHOD(AudioStreamPlayer,is_autoplay_enabled);

    SE_BIND_METHOD(AudioStreamPlayer,set_mix_target);
    SE_BIND_METHOD(AudioStreamPlayer,get_mix_target);

    SE_BIND_METHOD(AudioStreamPlayer,_set_playing);
    SE_BIND_METHOD(AudioStreamPlayer,_is_active);

    SE_BIND_METHOD(AudioStreamPlayer,_bus_layout_changed);

    SE_BIND_METHOD(AudioStreamPlayer,set_stream_paused);
    SE_BIND_METHOD(AudioStreamPlayer,get_stream_paused);

    SE_BIND_METHOD(AudioStreamPlayer,get_stream_playback);

    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "stream", PropertyHint::ResourceType, "AudioStream"), "set_stream", "get_stream");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "volume_db", PropertyHint::Range, "-80,24"), "set_volume_db", "get_volume_db");
    ADD_PROPERTY(PropertyInfo(VariantType::FLOAT, "pitch_scale", PropertyHint::Range, "0.01,4,0.01,or_greater"), "set_pitch_scale", "get_pitch_scale");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "playing", PropertyHint::None, "", PROPERTY_USAGE_EDITOR), "_set_playing", "is_playing");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "autoplay"), "set_autoplay", "is_autoplay_enabled");
    ADD_PROPERTY(PropertyInfo(VariantType::BOOL, "stream_paused", PropertyHint::None, ""), "set_stream_paused", "get_stream_paused");
    ADD_PROPERTY(PropertyInfo(VariantType::INT, "mix_target", PropertyHint::Enum, "Stereo,Surround,Center"), "set_mix_target", "get_mix_target");
    ADD_PROPERTY(PropertyInfo(VariantType::STRING_NAME, "bus", PropertyHint::Enum, ""), "set_bus", "get_bus");

    ADD_SIGNAL(MethodInfo("finished"));

    BIND_ENUM_CONSTANT(MIX_TARGET_STEREO);
    BIND_ENUM_CONSTANT(MIX_TARGET_SURROUND);
    BIND_ENUM_CONSTANT(MIX_TARGET_CENTER);
}

AudioStreamPlayer::AudioStreamPlayer() {

    mix_volume_db = 0;
    pitch_scale = 1.0;
    volume_db = 0;
    autoplay = false;
    setseek.set(-1);
    stream_paused = false;
    stream_paused_fade = false;
    mix_target = MIX_TARGET_STEREO;
    fadeout_buffer.resize(512);

    AudioServer::get_singleton()->connect("bus_layout_changed",callable_mp(this, &ClassName::_bus_layout_changed));
}

AudioStreamPlayer::~AudioStreamPlayer() {
}
