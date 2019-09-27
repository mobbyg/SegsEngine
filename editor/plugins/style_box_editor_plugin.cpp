/*************************************************************************/
/*  style_box_editor_plugin.cpp                                          */
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

#include "style_box_editor_plugin.h"
#include "editor/editor_scale.h"
#include "core/method_bind.h"

IMPL_GDCLASS(StyleBoxPreview)
IMPL_GDCLASS(EditorInspectorPluginStyleBox)
IMPL_GDCLASS(StyleBoxEditorPlugin)

bool EditorInspectorPluginStyleBox::can_handle(Object *p_object) {

    return Object::cast_to<StyleBox>(p_object) != nullptr;
}

void EditorInspectorPluginStyleBox::parse_begin(Object *p_object) {

    Ref<StyleBox> sb = Ref<StyleBox>(Object::cast_to<StyleBox>(p_object));

    StyleBoxPreview *preview = memnew(StyleBoxPreview);
    preview->edit(sb);
    add_custom_control(preview);
}
bool EditorInspectorPluginStyleBox::parse_property(Object *p_object, VariantType p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, int p_usage) {
    return false; //do not want
}
void EditorInspectorPluginStyleBox::parse_end() {
}

void StyleBoxPreview::edit(const Ref<StyleBox> &p_stylebox) {

    if (stylebox)
        stylebox->disconnect("changed", this, "_sb_changed");
    stylebox = p_stylebox;
    if (p_stylebox) {
        preview->add_style_override("panel", stylebox);
        stylebox->connect("changed", this, "_sb_changed");
    }
    _sb_changed();
}

void StyleBoxPreview::_sb_changed() {

    preview->update();
    if (stylebox) {
        Size2 ms = stylebox->get_minimum_size() * 4 / 3;
        ms.height = MAX(ms.height, 150 * EDSCALE);
        preview->set_custom_minimum_size(ms);
    }
}

void StyleBoxPreview::_bind_methods() {

    MethodBinder::bind_method("_sb_changed", &StyleBoxPreview::_sb_changed);
}

StyleBoxPreview::StyleBoxPreview() {

    preview = memnew(Panel);
    add_margin_child(TTR("Preview:"), preview);
}

StyleBoxEditorPlugin::StyleBoxEditorPlugin(EditorNode *p_node) {

    Ref<EditorInspectorPluginStyleBox> inspector_plugin(make_ref_counted<EditorInspectorPluginStyleBox>());
    add_inspector_plugin(inspector_plugin);
}