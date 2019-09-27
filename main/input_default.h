/*************************************************************************/
/*  input_default.h                                                      */
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

#pragma once

#include "core/os/input.h"
#include "core/os/input_event.h"
#include "core/list.h"
#include "core/map.h"
#include "core/ustring.h"
#include "core/set.h"

class InputDefault : public Input {

	GDCLASS(InputDefault,Input)

	_THREAD_SAFE_CLASS_

	int mouse_button_mask;

	Set<int> keys_pressed;
	Set<int> joy_buttons_pressed;
	Map<int, float> _joy_axis;
	//Map<StringName,int> custom_action_press;
	Vector3 gravity;
	Vector3 accelerometer;
	Vector3 magnetometer;
	Vector3 gyroscope;
	Vector2 mouse_pos;
	MainLoop *main_loop;

	struct Action {
		uint64_t physics_frame;
		uint64_t idle_frame;
		bool pressed;
		float strength;
	};

	Map<StringName, Action> action_state;

	bool emulate_touch_from_mouse;
	bool emulate_mouse_from_touch;

	int mouse_from_touch_index;

	struct VibrationInfo {
		float weak_magnitude;
		float strong_magnitude;
		float duration; // Duration in seconds
		uint64_t timestamp;
	};

	Map<int, VibrationInfo> joy_vibration;

	struct SpeedTrack {

		uint64_t last_tick;
		Vector2 speed;
		Vector2 accum;
		float accum_t;
		float min_ref_frame;
		float max_ref_frame;

		void update(const Vector2 &p_delta_p);
		void reset();
		SpeedTrack();
	};

	struct Joypad {
		StringName name;
		StringName uid;
		bool connected;
		bool last_buttons[JOY_BUTTON_MAX + 19]; //apparently SDL specifies 35 possible buttons on android
		float last_axis[JOY_AXIS_MAX];
		float filter;
		int last_hat;
		int mapping;
		int hat_current;

		Joypad() {
			for (int i = 0; i < JOY_AXIS_MAX; i++) {

				last_axis[i] = 0.0f;
			}
			for (int i = 0; i < JOY_BUTTON_MAX + 19; i++) {

				last_buttons[i] = false;
			}
			connected = false;
			last_hat = HAT_MASK_CENTER;
			filter = 0.01f;
			mapping = -1;
			hat_current = 0;
		}
	};

	SpeedTrack mouse_speed_track;
	Map<int, SpeedTrack> touch_speed_track;
	Map<int, Joypad> joy_names;
	int fallback_mapping;

	CursorShape default_shape;

public:
	enum HatMask {
		HAT_MASK_CENTER = 0,
		HAT_MASK_UP = 1,
		HAT_MASK_RIGHT = 2,
		HAT_MASK_DOWN = 4,
		HAT_MASK_LEFT = 8,
	};

	enum HatDir {
		HAT_UP,
		HAT_RIGHT,
		HAT_DOWN,
		HAT_LEFT,
		HAT_MAX,
	};

	enum {
		JOYPADS_MAX = 16,
	};

	struct JoyAxis {
		int min;
		float value;
	};

private:
	enum JoyType {
		TYPE_BUTTON,
		TYPE_AXIS,
		TYPE_HAT,
		TYPE_MAX,
	};

	struct JoyEvent {
		int type;
		int index;
		int value;
	};

	struct JoyDeviceMapping {

		String uid;
		String name;
		Map<int, JoyEvent> buttons;
		Map<int, JoyEvent> axis;
		JoyEvent hat[HAT_MAX];
	};

	JoyEvent hat_map_default[HAT_MAX];

	Vector<JoyDeviceMapping> map_db;

	JoyEvent _find_to_event(const String& p_to);
	void _button_event(int p_device, int p_index, bool p_pressed);
	void _axis_event(int p_device, int p_axis, float p_value);
	float _handle_deadzone(int p_device, int p_axis, float p_value);

	void _parse_input_event_impl(const Ref<InputEvent> &p_event, bool p_is_emulated);

	List<Ref<InputEvent> > accumulated_events;
	bool use_accumulated_input;

public:
	bool is_key_pressed(int p_scancode) const override;
	bool is_mouse_button_pressed(int p_button) const override;
	bool is_joy_button_pressed(int p_device, int p_button) const override;
	bool is_action_pressed(const StringName &p_action) const override;
	bool is_action_just_pressed(const StringName &p_action) const override;
	bool is_action_just_released(const StringName &p_action) const override;
	float get_action_strength(const StringName &p_action) const override;

	float get_joy_axis(int p_device, int p_axis) const override;
	String get_joy_name(int p_idx) override;
	Array get_connected_joypads() override;
	Vector2 get_joy_vibration_strength(int p_device) override;
	float get_joy_vibration_duration(int p_device) override;
	uint64_t get_joy_vibration_timestamp(int p_device) override;
	void joy_connection_changed(int p_idx, bool p_connected, String p_name, String p_guid = "") override;
	void parse_joypad_mapping(String p_mapping, bool p_update_existing);

	Vector3 get_gravity() const override;
	Vector3 get_accelerometer() const override;
	Vector3 get_magnetometer() const override;
	Vector3 get_gyroscope() const override;

	Point2 get_mouse_position() const override;
	Point2 get_last_mouse_speed() const override;
	int get_mouse_button_mask() const override;

	void warp_mouse_position(const Vector2 &p_to) override;
	Point2i warp_mouse_motion(const Ref<InputEventMouseMotion> &p_motion, const Rect2 &p_rect) override;

	void parse_input_event(const Ref<InputEvent> &p_event) override;

	void set_gravity(const Vector3 &p_gravity);
	void set_accelerometer(const Vector3 &p_accel);
	void set_magnetometer(const Vector3 &p_magnetometer);
	void set_gyroscope(const Vector3 &p_gyroscope);
	void set_joy_axis(int p_device, int p_axis, float p_value);

	void start_joy_vibration(int p_device, float p_weak_magnitude, float p_strong_magnitude, float p_duration = 0) override;
	void stop_joy_vibration(int p_device) override;
	void vibrate_handheld(int p_duration_ms = 500) override;

	void set_main_loop(MainLoop *p_main_loop);
	void set_mouse_position(const Point2 &p_posf);

	void action_press(const StringName &p_action, float p_strength = 1.f) override;
	void action_release(const StringName &p_action) override;

	void iteration(float p_step);

	void set_emulate_touch_from_mouse(bool p_emulate);
	bool is_emulating_touch_from_mouse() const override;
	void ensure_touch_mouse_raised();

	void set_emulate_mouse_from_touch(bool p_emulate);
	bool is_emulating_mouse_from_touch() const override;

	CursorShape get_default_cursor_shape() const override;
	void set_default_cursor_shape(CursorShape p_shape) override;
	CursorShape get_current_cursor_shape() const override;
	void set_custom_mouse_cursor(const RES &p_cursor, CursorShape p_shape = Input::CURSOR_ARROW, const Vector2 &p_hotspot = Vector2()) override;

	void parse_mapping(const String& p_mapping);
	void joy_button(int p_device, int p_button, bool p_pressed);
	void joy_axis(int p_device, int p_axis, const JoyAxis &p_value);
	void joy_hat(int p_device, int p_val);

	void add_joy_mapping(String p_mapping, bool p_update_existing = false) override;
	void remove_joy_mapping(String p_guid) override;
	bool is_joy_known(int p_device) override;
	String get_joy_guid(int p_device) const override;

	String get_joy_button_string(int p_button) override;
	String get_joy_axis_string(int p_axis) override;
	int get_joy_axis_index_from_string(String p_axis) override;
	int get_joy_button_index_from_string(String p_button) override;

	int get_unused_joy_id();

	bool is_joy_mapped(int p_device);
	String get_joy_guid_remapped(int p_device) const;
	void set_fallback_mapping(const String& p_guid);

	void accumulate_input_event(const Ref<InputEvent> &p_event) override;
	void flush_accumulated_events() override;
	void set_use_accumulated_input(bool p_enable) override;

	virtual void release_pressed_events();
	InputDefault();
};