/*************************************************************************/
/*  button.h                                                             */
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

#include "scene/gui/base_button.h"
#include "core/string_name.h"

class GODOT_EXPORT Button : public BaseButton {

    GDCLASS(Button,BaseButton)

protected:
    void _set_internal_margin(Margin p_margin, float p_value);
    void _notification(int p_what);
    static void _bind_methods();

public:
    //

    Size2 get_minimum_size() const override;

    const String &get_text() const;
    void set_text(StringView p_text);

    void set_button_icon(const Ref<Texture> &p_icon);
    Ref<Texture> get_button_icon() const;

    void set_expand_icon(bool p_expand_icon);
    bool is_expand_icon() const;

    void set_flat(bool p_flat);
    bool is_flat() const;

    void set_clip_text(bool p_clip_text);
    bool get_clip_text() const;

    void set_text_align(UiTextAlign p_align);
    UiTextAlign get_text_align() const;

    void set_icon_align(UiTextAlign p_align);
    UiTextAlign get_icon_align() const;

    Button(const StringName &p_text = StringName());
    ~Button() override;
};

