/*************************************************************************/
/*  check_box.cpp                                                        */
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

#include "check_box.h"

#include "core/class_db.h"
#include "core/property_info.h"
#include "core/string_formatter.h"
#include "scene/resources/style_box.h"
#include "servers/rendering_server.h"

IMPL_GDCLASS(CheckBox)

Size2 CheckBox::get_icon_size() const {
    Ref<Texture> checked = Control::get_theme_icon("checked");
    Ref<Texture> checked_disabled = Control::get_theme_icon("checked_disabled");
    Ref<Texture> unchecked = Control::get_theme_icon("unchecked");
    Ref<Texture> unchecked_disabled = Control::get_theme_icon("unchecked_disabled");
    Ref<Texture> radio_checked = Control::get_theme_icon("radio_checked");
    Ref<Texture> radio_unchecked = Control::get_theme_icon("radio_unchecked");
    Ref<Texture> radio_checked_disabled = Control::get_theme_icon("radio_checked_disabled");
    Ref<Texture> radio_unchecked_disabled = Control::get_theme_icon("radio_unchecked_disabled");

    Size2 tex_size = Size2(0, 0);
    if (checked) {
        tex_size = Size2(checked->get_width(), checked->get_height());
    }
    if (unchecked) {
        tex_size =
                Size2(M_MAX(tex_size.width, unchecked->get_width()), M_MAX(tex_size.height, unchecked->get_height()));
    }
    if (radio_checked) {
        tex_size = Size2(
                M_MAX(tex_size.width, radio_checked->get_width()), M_MAX(tex_size.height, radio_checked->get_height()));
    }
    if (radio_unchecked) {
        tex_size = Size2(M_MAX(tex_size.width, radio_unchecked->get_width()),
                M_MAX(tex_size.height, radio_unchecked->get_height()));
    }
    if (checked_disabled) {
        tex_size = Size2(M_MAX(tex_size.width, checked_disabled->get_width()),
                M_MAX(tex_size.height, checked_disabled->get_height()));
    }
    if (unchecked_disabled) {
        tex_size = Size2(M_MAX(tex_size.width, unchecked_disabled->get_width()),
                M_MAX(tex_size.height, unchecked_disabled->get_height()));
    }
    if (radio_checked_disabled) {
        tex_size = Size2(M_MAX(tex_size.width, radio_checked_disabled->get_width()),
                M_MAX(tex_size.height, radio_checked_disabled->get_height()));
    }
    if (radio_unchecked_disabled) {
        tex_size = Size2(M_MAX(tex_size.width, radio_unchecked_disabled->get_width()),
                M_MAX(tex_size.height, radio_unchecked_disabled->get_height()));
    }
    return tex_size;
}

Size2 CheckBox::get_minimum_size() const {

    Size2 minsize = Button::get_minimum_size();
    Size2 tex_size = get_icon_size();
    minsize.width += tex_size.width;
    if (not get_text().empty()) {
        minsize.width += get_theme_constant("hseparation");
    }
    Ref<StyleBox> sb = get_theme_stylebox("normal");
    minsize.height = M_MAX(minsize.height, tex_size.height + sb->get_margin(Margin::Top) + sb->get_margin(Margin::Bottom));

    return minsize;
}

void CheckBox::_notification(int p_what) {
    if (p_what == NOTIFICATION_THEME_CHANGED) {
        _set_internal_margin(Margin::Left, get_icon_size().width);
    } else if (p_what == NOTIFICATION_DRAW) {
        RenderingEntity ci = get_canvas_item();

        Ref<Texture> on = Control::get_theme_icon(FormatSN("%s%s", is_radio() ? "radio_checked" : "checked", is_disabled() ? "_disabled" : ""));
        Ref<Texture> off = Control::get_theme_icon(FormatSN("%s%s", is_radio() ? "radio_unchecked" : "unchecked", is_disabled() ? "_disabled" : ""));
        Ref<StyleBox> sb = get_theme_stylebox("normal");

        Vector2 ofs;
        ofs.x = sb->get_margin(Margin::Left);
        ofs.y = int((get_size().height - get_icon_size().height) / 2) + get_theme_constant("check_vadjust");

        if (is_pressed()) {
            on->draw(ci, ofs);
        } else {
            off->draw(ci, ofs);
        }
    }
}

bool CheckBox::is_radio() {

    return get_button_group();
}

CheckBox::CheckBox(const StringName &p_text) :
        Button(p_text) {
    set_toggle_mode(true);
    set_text_align(UiTextAlign::ALIGN_LEFT);
    _set_internal_margin(Margin::Left, get_icon_size().width);
}

CheckBox::~CheckBox() {
}
