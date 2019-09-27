/*************************************************************************/
/*  visual_script_editor.cpp                                             */
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

#include "visual_script_editor.h"

#include "core/method_bind.h"
#include "core/object.h"
#include "core/object_db.h"
#include "core/os/input.h"
#include "core/os/keyboard.h"
#include "core/script_language.h"
#include "core/variant.h"
#include "editor/editor_node.h"
#include "editor/editor_resource_preview.h"
#include "scene/main/viewport.h"
#include "visual_script_expression.h"
#include "visual_script_flow_control.h"
#include "visual_script_func_nodes.h"
#include "visual_script_nodes.h"
#include "scene/resources/theme.h"
IMPL_GDCLASS(VisualScriptEditor)
IMPL_GDCLASS(_VisualScriptEditor)

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#include "scene/resources/style_box.h"


class VisualScriptEditorSignalEdit : public Object {

    GDCLASS(VisualScriptEditorSignalEdit,Object)

    StringName sig;

public:
    UndoRedo *undo_redo;
    Ref<VisualScript> script;

protected:
    static void _bind_methods() {
        MethodBinder::bind_method("_sig_changed", &VisualScriptEditorSignalEdit::_sig_changed);
        ADD_SIGNAL(MethodInfo("changed"));
    }

    void _sig_changed() {

        _change_notify();
        emit_signal("changed");
    }

    bool _set(const StringName &p_name, const Variant &p_value) {

        if (sig == StringName())
            return false;

        if (p_name == "argument_count") {

            int new_argc = p_value;
            int argc = script->custom_signal_get_argument_count(sig);
            if (argc == new_argc)
                return true;

            undo_redo->create_action(TTR("Change Signal Arguments"));

            if (new_argc < argc) {
                for (int i = new_argc; i < argc; i++) {
                    undo_redo->add_do_method(script.get(), "custom_signal_remove_argument", sig, new_argc);
                    undo_redo->add_undo_method(script.get(), "custom_signal_add_argument", sig, script->custom_signal_get_argument_name(sig, i), script->custom_signal_get_argument_type(sig, i), -1);
                }
            } else if (new_argc > argc) {

                for (int i = argc; i < new_argc; i++) {

                    undo_redo->add_do_method(script.get(), "custom_signal_add_argument", sig, VariantType::NIL, String("arg" + itos(i + 1)), -1);
                    undo_redo->add_undo_method(script.get(), "custom_signal_remove_argument", sig, argc);
                }
            }

            undo_redo->add_do_method(this, "_sig_changed");
            undo_redo->add_undo_method(this, "_sig_changed");

            undo_redo->commit_action();

            return true;
        }
        if (StringUtils::begins_with(String(p_name),"argument/")) {
            int idx = StringUtils::to_int(StringUtils::get_slice(p_name,"/", 1)) - 1;
            ERR_FAIL_INDEX_V(idx, script->custom_signal_get_argument_count(sig), false)
            String what = StringUtils::get_slice(p_name,"/", 2);
            if (what == "type") {

                int old_type = (int)script->custom_signal_get_argument_type(sig, idx);
                int new_type = p_value;
                undo_redo->create_action(TTR("Change Argument Type"));
                undo_redo->add_do_method(script.get(), "custom_signal_set_argument_type", sig, idx, new_type);
                undo_redo->add_undo_method(script.get(), "custom_signal_set_argument_type", sig, idx, old_type);
                undo_redo->commit_action();

                return true;
            }

            if (what == "name") {

                String old_name = script->custom_signal_get_argument_name(sig, idx);
                String new_name = p_value;
                undo_redo->create_action(TTR("Change Argument name"));
                undo_redo->add_do_method(script.get(), "custom_signal_set_argument_name", sig, idx, new_name);
                undo_redo->add_undo_method(script.get(), "custom_signal_set_argument_name", sig, idx, old_name);
                undo_redo->commit_action();
                return true;
            }
        }

        return false;
    }

    bool _get(const StringName &p_name, Variant &r_ret) const {

        if (sig == StringName())
            return false;

        if (p_name == "argument_count") {
            r_ret = script->custom_signal_get_argument_count(sig);
            return true;
        }
        if (StringUtils::begins_with(String(p_name),"argument/")) {
            int idx = StringUtils::to_int(StringUtils::get_slice(p_name,"/", 1)) - 1;
            ERR_FAIL_INDEX_V(idx, script->custom_signal_get_argument_count(sig), false)
            String what = StringUtils::get_slice(p_name,"/", 2);
            if (what == "type") {
                r_ret = script->custom_signal_get_argument_type(sig, idx);
                return true;
            }
            if (what == "name") {
                r_ret = script->custom_signal_get_argument_name(sig, idx);
                return true;
            }
        }

        return false;
    }
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const {

        if (sig == StringName())
            return;

        p_list->push_back(PropertyInfo(VariantType::INT, "argument_count", PROPERTY_HINT_RANGE, "0,256"));
        char argt_c[7+(longest_variant_type_name+1)*int(VariantType::VARIANT_MAX)];
        fill_with_all_variant_types("Variant",argt_c);
        int write_idx = 7;
        for (int i = 1; i < int(VariantType::VARIANT_MAX); i++) {
            write_idx+=sprintf(argt_c+write_idx,",%s",Variant::get_type_name(VariantType(i)));
        }

        for (int i = 0; i < script->custom_signal_get_argument_count(sig); i++) {
            p_list->push_back(PropertyInfo(VariantType::INT, "argument/" + itos(i + 1) + "/type", PROPERTY_HINT_ENUM, argt_c));
            p_list->push_back(PropertyInfo(VariantType::STRING, "argument/" + itos(i + 1) + "/name"));
        }
    }

public:
    void edit(const StringName &p_sig) {

        sig = p_sig;
        _change_notify();
    }

    VisualScriptEditorSignalEdit() { undo_redo = nullptr; }
};

IMPL_GDCLASS(VisualScriptEditorSignalEdit)

class VisualScriptEditorVariableEdit : public Object {

    GDCLASS(VisualScriptEditorVariableEdit,Object)

    StringName var;

public:
    UndoRedo *undo_redo;
    Ref<VisualScript> script;

protected:
    static void _bind_methods() {
        MethodBinder::bind_method("_var_changed", &VisualScriptEditorVariableEdit::_var_changed);
        MethodBinder::bind_method("_var_value_changed", &VisualScriptEditorVariableEdit::_var_value_changed);
        ADD_SIGNAL(MethodInfo("changed"));
    }

    void _var_changed() {

        _change_notify();
        emit_signal("changed");
    }
    void _var_value_changed() {

        _change_notify("value"); //so the whole tree is not redrawn, makes editing smoother in general
        emit_signal("changed");
    }

    bool _set(const StringName &p_name, const Variant &p_value) {

        if (var == StringName())
            return false;

        if (String(p_name) == "value") {
            undo_redo->create_action(TTR("Set Variable Default Value"));
            Variant current = script->get_variable_default_value(var);
            undo_redo->add_do_method(script.get(), "set_variable_default_value", var, p_value);
            undo_redo->add_undo_method(script.get(), "set_variable_default_value", var, current);
            undo_redo->add_do_method(this, "_var_value_changed");
            undo_redo->add_undo_method(this, "_var_value_changed");
            undo_redo->commit_action();
            return true;
        }

        Dictionary d = script->call("get_variable_info", var);

        if (String(p_name) == "type") {

            Dictionary dc = d.duplicate();
            dc["type"] = p_value;
            undo_redo->create_action(TTR("Set Variable Type"));
            undo_redo->add_do_method(script.get(), "set_variable_info", var, dc);
            undo_redo->add_undo_method(script.get(), "set_variable_info", var, d);
            undo_redo->add_do_method(this, "_var_changed");
            undo_redo->add_undo_method(this, "_var_changed");
            undo_redo->commit_action();
            return true;
        }

        if (String(p_name) == "hint") {

            Dictionary dc = d.duplicate();
            dc["hint"] = p_value;
            undo_redo->create_action(TTR("Set Variable Type"));
            undo_redo->add_do_method(script.get(), "set_variable_info", var, dc);
            undo_redo->add_undo_method(script.get(), "set_variable_info", var, d);
            undo_redo->add_do_method(this, "_var_changed");
            undo_redo->add_undo_method(this, "_var_changed");
            undo_redo->commit_action();
            return true;
        }

        if (String(p_name) == "hint_string") {

            Dictionary dc = d.duplicate();
            dc["hint_string"] = p_value;
            undo_redo->create_action(TTR("Set Variable Type"));
            undo_redo->add_do_method(script.get(), "set_variable_info", var, dc);
            undo_redo->add_undo_method(script.get(), "set_variable_info", var, d);
            undo_redo->add_do_method(this, "_var_changed");
            undo_redo->add_undo_method(this, "_var_changed");
            undo_redo->commit_action();
            return true;
        }

        if (String(p_name) == "export") {
            script->set_variable_export(var, p_value);
            EditorNode::get_singleton()->get_inspector()->update_tree();
            return true;
        }

        return false;
    }

    bool _get(const StringName &p_name, Variant &r_ret) const {

        if (var == StringName())
            return false;

        if (String(p_name) == "value") {
            r_ret = script->get_variable_default_value(var);
            return true;
        }

        PropertyInfo pinfo = script->get_variable_info(var);

        if (String(p_name) == "type") {
            r_ret = pinfo.type;
            return true;
        }
        if (String(p_name) == "hint") {
            r_ret = pinfo.hint;
            return true;
        }
        if (String(p_name) == "hint_string") {
            r_ret = pinfo.hint_string;
            return true;
        }

        if (String(p_name) == "export") {
            r_ret = script->get_variable_export(var);
            return true;
        }

        return false;
    }
    void _get_property_list(ListPOD<PropertyInfo> *p_list) const {

        if (var == StringName())
            return;
        char argt_c[7+(longest_variant_type_name+1)*int(VariantType::VARIANT_MAX)];
        fill_with_all_variant_types("Variant",argt_c);
        p_list->push_back(PropertyInfo(VariantType::INT, "type", PROPERTY_HINT_ENUM, argt_c));
        p_list->push_back(PropertyInfo(script->get_variable_info(var).type, "value", script->get_variable_info(var).hint,
                StringUtils::to_utf8(script->get_variable_info(var).hint_string).data(), PROPERTY_USAGE_DEFAULT));
        // Update this when PropertyHint changes
        p_list->push_back(PropertyInfo(VariantType::INT, "hint", PROPERTY_HINT_ENUM,
                "None,Range,ExpRange,Enum,ExpEasing,Length,SpriteFrame,KeyAccel,Flags,Layers2dRender,Layers2dPhysics,"
                "Layer3dRender,Layer3dPhysics,File,Dir,GlobalFile,GlobalDir,ResourceType,MultilineText,PlaceholderText,"
                "ColorNoAlpha,ImageCompressLossy,ImageCompressLossLess,ObjectId,String,NodePathToEditedNode,"
                "MethodOfVariantType,MethodOfBaseType,MethodOfInstance,MethodOfScript,PropertyOfVariantType,PropertyOfBaseType,"
                "PropertyOfInstance,PropertyOfScript,ObjectTooBig,NodePathValidTypes"));
        p_list->push_back(PropertyInfo(VariantType::STRING, "hint_string"));
        p_list->push_back(PropertyInfo(VariantType::BOOL, "export"));
    }

public:
    void edit(const StringName &p_var) {

        var = p_var;
        _change_notify();
    }

    VisualScriptEditorVariableEdit() { undo_redo = nullptr; }
};
IMPL_GDCLASS(VisualScriptEditorVariableEdit)

static Color _color_from_type(VariantType p_type, bool dark_theme = true) {
    Color color;
    if (dark_theme)
        switch (p_type) {
            case VariantType::NIL: color = Color(0.41f, 0.93f, 0.74f); break;

            case VariantType::BOOL: color = Color(0.55f, 0.65f, 0.94f); break;
            case VariantType::INT: color = Color(0.49f, 0.78f, 0.94f); break;
            case VariantType::REAL: color = Color(0.38f, 0.85f, 0.96f); break;
            case VariantType::STRING: color = Color(0.42f, 0.65f, 0.93f); break;

            case VariantType::VECTOR2: color = Color(0.74f, 0.57f, 0.95f); break;
            case VariantType::RECT2: color = Color(0.95f, 0.57f, 0.65f); break;
            case VariantType::VECTOR3: color = Color(0.84f, 0.49f, 0.93f); break;
            case VariantType::TRANSFORM2D: color = Color(0.77f, 0.93f, 0.41f); break;
            case VariantType::PLANE: color = Color(0.97f, 0.44f, 0.44f); break;
            case VariantType::QUAT: color = Color(0.93f, 0.41f, 0.64f); break;
            case VariantType::AABB: color = Color(0.93f, 0.47f, 0.57f); break;
            case VariantType::BASIS: color = Color(0.89f, 0.93f, 0.41f); break;
            case VariantType::TRANSFORM: color = Color(0.96f, 0.66f, 0.43f); break;

            case VariantType::COLOR: color = Color(0.62f, 1.0, 0.44f); break;
            case VariantType::NODE_PATH: color = Color(0.41f, 0.58f, 0.93f); break;
            case VariantType::_RID: color = Color(0.41f, 0.93f, 0.6f); break;
            case VariantType::OBJECT: color = Color(0.47f, 0.95f, 0.91f); break;
            case VariantType::DICTIONARY: color = Color(0.47f, 0.93f, 0.69f); break;

            case VariantType::ARRAY: color = Color(0.88f, 0.88f, 0.88f); break;
            case VariantType::POOL_BYTE_ARRAY: color = Color(0.67f, 0.96f, 0.78f); break;
            case VariantType::POOL_INT_ARRAY: color = Color(0.69f, 0.86f, 0.96f); break;
            case VariantType::POOL_REAL_ARRAY: color = Color(0.59f, 0.91f, 0.97f); break;
            case VariantType::POOL_STRING_ARRAY: color = Color(0.62f, 0.77f, 0.95f); break;
            case VariantType::POOL_VECTOR2_ARRAY: color = Color(0.82f, 0.7f, 0.96f); break;
            case VariantType::POOL_VECTOR3_ARRAY: color = Color(0.87f, 0.61f, 0.95f); break;
            case VariantType::POOL_COLOR_ARRAY: color = Color(0.91f, 1.0f, 0.59f); break;

            default:
                color.set_hsv((int)p_type / float(VariantType::VARIANT_MAX), 0.7f, 0.7f);
        }
    else
        switch (p_type) {
            case VariantType::NIL: color = Color(0.15f, 0.89f, 0.63f); break;

            case VariantType::BOOL: color = Color(0.43f, 0.56f, 0.92f); break;
            case VariantType::INT: color = Color(0.31f, 0.7f, 0.91f); break;
            case VariantType::REAL: color = Color(0.15f, 0.8f, 0.94f); break;
            case VariantType::STRING: color = Color(0.27f, 0.56f, 0.91f); break;

            case VariantType::VECTOR2: color = Color(0.68f, 0.46f, 0.93f); break;
            case VariantType::RECT2: color = Color(0.93f, 0.46f, 0.56f); break;
            case VariantType::VECTOR3: color = Color(0.86f, 0.42f, 0.93f); break;
            case VariantType::TRANSFORM2D: color = Color(0.59f, 0.81f, 0.1f); break;
            case VariantType::PLANE: color = Color(0.97f, 0.44f, 0.44f); break;
            case VariantType::QUAT: color = Color(0.93f, 0.41f, 0.64f); break;
            case VariantType::AABB: color = Color(0.93f, 0.47f, 0.57f); break;
            case VariantType::BASIS: color = Color(0.7f, 0.73f, 0.1f); break;
            case VariantType::TRANSFORM: color = Color(0.96f, 0.56f, 0.28f); break;

            case VariantType::COLOR: color = Color(0.24f, 0.75f, 0.0); break;
            case VariantType::NODE_PATH: color = Color(0.41f, 0.58f, 0.93f); break;
            case VariantType::_RID: color = Color(0.17f, 0.9f, 0.45f); break;
            case VariantType::OBJECT: color = Color(0.07f, 0.84f, 0.76f); break;
            case VariantType::DICTIONARY: color = Color(0.34f, 0.91f, 0.62f); break;

            case VariantType::ARRAY: color = Color(0.45f, 0.45f, 0.45f); break;
            case VariantType::POOL_BYTE_ARRAY: color = Color(0.38f, 0.92f, 0.6f); break;
            case VariantType::POOL_INT_ARRAY: color = Color(0.38f, 0.73f, 0.92f); break;
            case VariantType::POOL_REAL_ARRAY: color = Color(0.25f, 0.83f, 0.95f); break;
            case VariantType::POOL_STRING_ARRAY: color = Color(0.38f, 0.62f, 0.92f); break;
            case VariantType::POOL_VECTOR2_ARRAY: color = Color(0.62f, 0.36f, 0.92f); break;
            case VariantType::POOL_VECTOR3_ARRAY: color = Color(0.79f, 0.35f, 0.92f); break;
            case VariantType::POOL_COLOR_ARRAY: color = Color(0.57f, 0.73f, 0.0); break;

            default:
                color.set_hsv((int)p_type / float(VariantType::VARIANT_MAX), 0.3f, 0.3f);
        }

    return color;
}

void VisualScriptEditor::_update_graph_connections() {

    graph->clear_connections();

    List<VisualScript::SequenceConnection> sequence_conns;
    script->get_sequence_connection_list(edited_func, &sequence_conns);

    for (List<VisualScript::SequenceConnection>::Element *E = sequence_conns.front(); E; E = E->next()) {

        graph->connect_node(itos(E->deref().from_node), E->deref().from_output, itos(E->deref().to_node), 0);
    }

    List<VisualScript::DataConnection> data_conns;
    script->get_data_connection_list(edited_func, &data_conns);

    for (List<VisualScript::DataConnection>::Element *E = data_conns.front(); E; E = E->next()) {

        VisualScript::DataConnection dc = E->deref();

        Ref<VisualScriptNode> from_node = script->get_node(edited_func, E->deref().from_node);
        Ref<VisualScriptNode> to_node = script->get_node(edited_func, E->deref().to_node);

        if (to_node->has_input_sequence_port()) {
            dc.to_port++;
        }

        dc.from_port += from_node->get_output_sequence_port_count();

        graph->connect_node(itos(E->deref().from_node), dc.from_port, itos(E->deref().to_node), dc.to_port);
    }
}

void VisualScriptEditor::_update_graph(int p_only_id) {

    if (updating_graph)
        return;

    updating_graph = true;

    //byebye all nodes
    if (p_only_id >= 0) {
        if (graph->has_node(NodePath(itos(p_only_id)))) {
            Node *gid = graph->get_node(NodePath(itos(p_only_id)));
            if (gid)
                memdelete(gid);
        }
    } else {

        for (int i = 0; i < graph->get_child_count(); i++) {

            if (Object::cast_to<GraphNode>(graph->get_child(i))) {
                memdelete(graph->get_child(i));
                i--;
            }
        }
    }

    if (!script->has_function(edited_func)) {
        graph->hide();
        select_func_text->show();
        updating_graph = false;
        return;
    }

    graph->show();
    select_func_text->hide();

    Ref<Texture> type_icons[int(VariantType::VARIANT_MAX)] = {
        Control::get_icon("Variant", "EditorIcons"),
        Control::get_icon("bool", "EditorIcons"),
        Control::get_icon("int", "EditorIcons"),
        Control::get_icon("float", "EditorIcons"),
        Control::get_icon("String", "EditorIcons"),
        Control::get_icon("Vector2", "EditorIcons"),
        Control::get_icon("Rect2", "EditorIcons"),
        Control::get_icon("Vector3", "EditorIcons"),
        Control::get_icon("Transform2D", "EditorIcons"),
        Control::get_icon("Plane", "EditorIcons"),
        Control::get_icon("Quat", "EditorIcons"),
        Control::get_icon("AABB", "EditorIcons"),
        Control::get_icon("Basis", "EditorIcons"),
        Control::get_icon("Transform", "EditorIcons"),
        Control::get_icon("Color", "EditorIcons"),
        Control::get_icon("NodePath", "EditorIcons"),
        Control::get_icon("RID", "EditorIcons"),
        Control::get_icon("MiniObject", "EditorIcons"),
        Control::get_icon("Dictionary", "EditorIcons"),
        Control::get_icon("Array", "EditorIcons"),
        Control::get_icon("PoolByteArray", "EditorIcons"),
        Control::get_icon("PoolIntArray", "EditorIcons"),
        Control::get_icon("PoolRealArray", "EditorIcons"),
        Control::get_icon("PoolStringArray", "EditorIcons"),
        Control::get_icon("PoolVector2Array", "EditorIcons"),
        Control::get_icon("PoolVector3Array", "EditorIcons"),
        Control::get_icon("PoolColorArray", "EditorIcons")
    };

    Ref<Texture> seq_port = Control::get_icon("VisualShaderPort", "EditorIcons");

    List<int> ids;
    script->get_node_list(edited_func, &ids);
    StringName editor_icons = "EditorIcons";

    for (List<int>::Element *E = ids.front(); E; E = E->next()) {

        if (p_only_id >= 0 && p_only_id != E->deref())
            continue;

        Ref<VisualScriptNode> node = script->get_node(edited_func, E->deref());
        Vector2 pos = script->get_node_position(edited_func, E->deref());

        GraphNode *gnode = memnew(GraphNode);
        gnode->set_title(node->get_caption());
        gnode->set_offset(pos * EDSCALE);
        if (error_line == E->deref()) {
            gnode->set_overlay(GraphNode::OVERLAY_POSITION);
        } else if (node->is_breakpoint()) {
            gnode->set_overlay(GraphNode::OVERLAY_BREAKPOINT);
        }

        gnode->set_meta("__vnode", node);
        gnode->set_name(itos(E->deref()));
        gnode->connect("dragged", this, "_node_moved", varray(E->deref()));
        gnode->connect("close_request", this, "_remove_node", varray(E->deref()), ObjectNS::CONNECT_DEFERRED);

        if (E->deref() != script->get_function_node_id(edited_func)) {
            //function can't be erased
            gnode->set_show_close_button(true);
        }

        bool has_gnode_text = false;

        if (Object::cast_to<VisualScriptExpression>(node.get())) {
            has_gnode_text = true;
            LineEdit *line_edit = memnew(LineEdit);
            line_edit->set_text(node->get_text());
            line_edit->set_expand_to_text_length(true);
            line_edit->add_font_override("font", get_font("source", "EditorFonts"));
            gnode->add_child(line_edit);
            line_edit->connect("text_changed", this, "_expression_text_changed", varray(E->deref()));
        } else {
            String text = node->get_text();
            if (!text.empty()) {
                has_gnode_text = true;
                Label *label = memnew(Label);
                label->set_text(text);
                gnode->add_child(label);
            }
        }

        if (Object::cast_to<VisualScriptComment>(node.get())) {
            Ref<VisualScriptComment> vsc = dynamic_ref_cast<VisualScriptComment>(node);
            gnode->set_comment(true);
            gnode->set_resizable(true);
            gnode->set_custom_minimum_size(vsc->get_size() * EDSCALE);
            gnode->connect("resize_request", this, "_comment_node_resized", varray(E->deref()));
        }

        if (node_styles.contains(node->get_category())) {
            Ref<StyleBoxFlat> sbf = dynamic_ref_cast<StyleBoxFlat>(node_styles[node->get_category()]);
            if (gnode->is_comment())
                sbf = dynamic_ref_cast<StyleBoxFlat>(EditorNode::get_singleton()->get_theme_base()->get_theme()->get_stylebox("comment", "GraphNode"));

            Color c = sbf->get_border_color();
            c.a = 1;
            if (EditorSettings::get_singleton()->get("interface/theme/use_graph_node_headers")) {
                Color mono_color = ((c.r + c.g + c.b) / 3) < 0.7f ? Color(1.0f, 1.0f, 1.0) : Color(0.0f, 0.0f, 0.0);
                mono_color.a = 0.85f;
                c = mono_color;
            }

            gnode->add_color_override("title_color", c);
            c.a = 0.7f;
            gnode->add_color_override("close_color", c);
            gnode->add_color_override("resizer_color", c);
            gnode->add_style_override("frame", sbf);
        }

        const Color mono_color = get_color("mono_color", "Editor");

        int slot_idx = 0;

        bool single_seq_output = node->get_output_sequence_port_count() == 1 && node->get_output_sequence_port_text(0).empty();
        if ((node->has_input_sequence_port() || single_seq_output) || has_gnode_text) {
            // IF has_gnode_text is true BUT we have no sequence ports to draw (in here),
            // we still draw the disabled default ones to shift up the slots by one,
            // so the slots DON'T start with the content text.

            // IF has_gnode_text is false, but we DO want to draw default sequence ports,
            // we draw a dummy text to take up the position of the sequence nodes, so all the other ports are still aligned correctly.
            if (!has_gnode_text) {
                Label *dummy = memnew(Label);
                dummy->set_text(" ");
                gnode->add_child(dummy);
            }
            gnode->set_slot(0, node->has_input_sequence_port(), TYPE_SEQUENCE, mono_color, single_seq_output, TYPE_SEQUENCE, mono_color, seq_port, seq_port);
            slot_idx++;
        }

        int mixed_seq_ports = 0;

        if (!single_seq_output) {

            if (node->has_mixed_input_and_sequence_ports()) {
                mixed_seq_ports = node->get_output_sequence_port_count();
            } else {
                for (int i = 0; i < node->get_output_sequence_port_count(); i++) {

                    Label *text2 = memnew(Label);
                    text2->set_text(node->get_output_sequence_port_text(i));
                    text2->set_align(Label::ALIGN_RIGHT);
                    gnode->add_child(text2);
                    gnode->set_slot(slot_idx, false, 0, Color(), true, TYPE_SEQUENCE, mono_color, seq_port, seq_port);
                    slot_idx++;
                }
            }
        }

        for (int i = 0; i < MAX(node->get_output_value_port_count(), MAX(mixed_seq_ports, node->get_input_value_port_count())); i++) {

            bool left_ok = false;
            VariantType left_type = VariantType::NIL;
            String left_name;

            if (i < node->get_input_value_port_count()) {
                PropertyInfo pi = node->get_input_value_port_info(i);
                left_ok = true;
                left_type = pi.type;
                left_name = pi.name;
            }

            bool right_ok = false;
            VariantType right_type = VariantType::NIL;
            String right_name;

            if (i >= mixed_seq_ports && i < node->get_output_value_port_count() + mixed_seq_ports) {
                PropertyInfo pi = node->get_output_value_port_info(i - mixed_seq_ports);
                right_ok = true;
                right_type = pi.type;
                right_name = pi.name;
            }

            HBoxContainer *hbc = memnew(HBoxContainer);

            if (left_ok) {

                Ref<Texture> t;
                if ((int)left_type >= 0 && (int)left_type < (int)VariantType::VARIANT_MAX) {
                    t = type_icons[(int)left_type];
                }
                if (t) {
                    TextureRect *tf = memnew(TextureRect);
                    tf->set_texture(t);
                    tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
                    hbc->add_child(tf);
                }

                hbc->add_child(memnew(Label(left_name)));

                if (left_type != VariantType::NIL && !script->is_input_value_port_connected(edited_func, E->deref(), i)) {

                    PropertyInfo pi = node->get_input_value_port_info(i);
                    Button *button = memnew(Button);
                    Variant value = node->get_default_input_value(i);
                    if (value.get_type() != left_type) {
                        //different type? for now convert
                        //not the same, reconvert
                        Variant::CallError ce;
                        const Variant *existingp = &value;
                        value = Variant::construct(left_type, &existingp, 1, ce, false);
                    }

                    if (left_type == VariantType::COLOR) {
                        button->set_custom_minimum_size(Size2(30, 0) * EDSCALE);
                        button->connect("draw", this, "_draw_color_over_button", varray(Variant(button), value));
                    } else if (left_type == VariantType::OBJECT && Ref<Resource>(value)) {

                        Ref<Resource> res = refFromVariant<Resource>(value);
                        Array arr;
                        arr.push_back(button->get_instance_id());
                        arr.push_back(String(value));
                        EditorResourcePreview::get_singleton()->queue_edited_resource_preview(res, this, "_button_resource_previewed", arr);

                    } else if (pi.type == VariantType::INT && pi.hint == PROPERTY_HINT_ENUM) {

                        button->set_text(StringUtils::get_slice(pi.hint_string,",", value));
                    } else {

                        button->set_text(value);
                    }
                    button->connect("pressed", this, "_default_value_edited", varray(Variant(button), E->deref(), i));
                    hbc->add_child(button);
                }
            } else {
                Control *c = memnew(Control);
                c->set_custom_minimum_size(Size2(10, 0) * EDSCALE);
                hbc->add_child(c);
            }

            hbc->add_spacer();

            if (i < mixed_seq_ports) {

                Label *text2 = memnew(Label);
                text2->set_text(node->get_output_sequence_port_text(i));
                text2->set_align(Label::ALIGN_RIGHT);
                hbc->add_child(text2);
            }

            if (right_ok) {

                hbc->add_child(memnew(Label(right_name)));

                Ref<Texture> t;
                if ((int)right_type >= 0 && (int)right_type < (int)VariantType::VARIANT_MAX) {
                    t = type_icons[(int)right_type];
                }
                if (t) {
                    TextureRect *tf = memnew(TextureRect);
                    tf->set_texture(t);
                    tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
                    hbc->add_child(tf);
                }
            }

            gnode->add_child(hbc);

            bool dark_theme = get_constant("dark_theme", "Editor");
            if (i < mixed_seq_ports) {
                gnode->set_slot(slot_idx, left_ok, (int)left_type, _color_from_type(left_type, dark_theme), true, TYPE_SEQUENCE, mono_color, Ref<Texture>(), seq_port);
            } else {
                gnode->set_slot(slot_idx, left_ok, (int)left_type, _color_from_type(left_type, dark_theme), right_ok, (int)right_type, _color_from_type(right_type, dark_theme));
            }

            slot_idx++;
        }

        graph->add_child(gnode);

        if (gnode->is_comment()) {
            graph->move_child(gnode, 0);
        }
    }

    _update_graph_connections();
    graph->call_deferred("set_scroll_ofs", script->get_function_scroll(edited_func) * EDSCALE); //may need to adapt a bit, let it do so
    updating_graph = false;
}

void VisualScriptEditor::_update_members() {
    ERR_FAIL_COND(not script)

    updating_members = true;

    members->clear();
    TreeItem *root = members->create_item();

    TreeItem *functions = members->create_item(root);
    functions->set_selectable(0, false);
    functions->set_text(0, TTR("Functions:"));
    functions->add_button(0, Control::get_icon("Override", "EditorIcons"), 1, false, TTR("Override an existing built-in function."));
    functions->add_button(0, Control::get_icon("Add", "EditorIcons"), 0, false, TTR("Create a new function."));
    functions->set_custom_color(0, Control::get_color("mono_color", "Editor"));

    Vector<StringName> func_names;
    script->get_function_list(&func_names);
    for (int i=0,fin=func_names.size(); i<fin; ++i) {
        TreeItem *ti = members->create_item(functions);
        ti->set_text(0, func_names[i]);
        ti->set_selectable(0, true);
        ti->set_editable(0, true);
        ti->set_metadata(0, func_names[i]);
        if (selected == func_names[i])
            ti->select(0);
    }

    TreeItem *variables = members->create_item(root);
    variables->set_selectable(0, false);
    variables->set_text(0, TTR("Variables:"));
    variables->add_button(0, Control::get_icon("Add", "EditorIcons"), -1, false, TTR("Create a new variable."));
    variables->set_custom_color(0, Control::get_color("mono_color", "Editor"));

    Ref<Texture> type_icons[(int)VariantType::VARIANT_MAX] = {
        Control::get_icon("Variant", "EditorIcons"),
        Control::get_icon("bool", "EditorIcons"),
        Control::get_icon("int", "EditorIcons"),
        Control::get_icon("float", "EditorIcons"),
        Control::get_icon("String", "EditorIcons"),
        Control::get_icon("Vector2", "EditorIcons"),
        Control::get_icon("Rect2", "EditorIcons"),
        Control::get_icon("Vector3", "EditorIcons"),
        Control::get_icon("Transform2D", "EditorIcons"),
        Control::get_icon("Plane", "EditorIcons"),
        Control::get_icon("Quat", "EditorIcons"),
        Control::get_icon("AABB", "EditorIcons"),
        Control::get_icon("Basis", "EditorIcons"),
        Control::get_icon("Transform", "EditorIcons"),
        Control::get_icon("Color", "EditorIcons"),
        Control::get_icon("NodePath", "EditorIcons"),
        Control::get_icon("RID", "EditorIcons"),
        Control::get_icon("MiniObject", "EditorIcons"),
        Control::get_icon("Dictionary", "EditorIcons"),
        Control::get_icon("Array", "EditorIcons"),
        Control::get_icon("PoolByteArray", "EditorIcons"),
        Control::get_icon("PoolIntArray", "EditorIcons"),
        Control::get_icon("PoolRealArray", "EditorIcons"),
        Control::get_icon("PoolStringArray", "EditorIcons"),
        Control::get_icon("PoolVector2Array", "EditorIcons"),
        Control::get_icon("PoolVector3Array", "EditorIcons"),
        Control::get_icon("PoolColorArray", "EditorIcons")
    };

    Vector<StringName> var_names;
    script->get_variable_list(&var_names);
    for (int i=0,fin=var_names.size(); i<fin; ++i) {
        TreeItem *ti = members->create_item(variables);

        ti->set_text(0, var_names[i]);
        Variant var = script->get_variable_default_value(var_names[i]);
        ti->set_suffix(0, "= " + String(var));
        ti->set_icon(0, type_icons[(int)script->get_variable_info(var_names[i]).type]);

        ti->set_selectable(0, true);
        ti->set_editable(0, true);
        ti->set_metadata(0, var_names[i]);
        if (selected == var_names[i])
            ti->select(0);
    }

    TreeItem *_signals = members->create_item(root);
    _signals->set_selectable(0, false);
    _signals->set_text(0, TTR("Signals:"));
    _signals->add_button(0, Control::get_icon("Add", "EditorIcons"), -1, false, TTR("Create a new signal."));
    _signals->set_custom_color(0, Control::get_color("mono_color", "Editor"));

    Vector<StringName> signal_names;
    script->get_custom_signal_list(&signal_names);
    for (int i=0,fin=signal_names.size(); i<fin; ++i) {
        TreeItem *ti = members->create_item(_signals);
        ti->set_text(0, signal_names[i]);
        ti->set_selectable(0, true);
        ti->set_editable(0, true);
        ti->set_metadata(0, signal_names[i]);
        if (selected == signal_names[i])
            ti->select(0);
    }

    String base_type = script->get_instance_base_type();
    String icon_type = base_type;
    if (!Control::has_icon(base_type, "EditorIcons")) {
        icon_type = "Object";
    }

    base_type_select->set_text(base_type);
    base_type_select->set_icon(Control::get_icon(icon_type, "EditorIcons"));

    updating_members = false;
}

void VisualScriptEditor::_member_selected() {

    if (updating_members)
        return;

    TreeItem *ti = members->get_selected();
    ERR_FAIL_COND(!ti)

    selected = ti->get_metadata(0);

    if (ti->get_parent() == members->get_root()->get_children()) {

        if (edited_func != selected) {

            revert_on_drag = edited_func;
            edited_func = selected;
            _update_members();
            _update_graph();
        }

        return; //or crash because it will become invalid
    }
}

void VisualScriptEditor::_member_edited() {

    if (updating_members)
        return;

    TreeItem *ti = members->get_edited();
    ERR_FAIL_COND(!ti)

    String name = ti->get_metadata(0);
    String new_name = ti->get_text(0);

    if (name == new_name)
        return;

    if (!StringUtils::is_valid_identifier(new_name)) {

        EditorNode::get_singleton()->show_warning(TTR("Name is not a valid identifier:") + " " + new_name);
        updating_members = true;
        ti->set_text(0, name);
        updating_members = false;
        return;
    }

    if (script->has_function(new_name) || script->has_variable(new_name) || script->has_custom_signal(new_name)) {

        EditorNode::get_singleton()->show_warning(TTR("Name already in use by another func/var/signal:") + " " + new_name);
        updating_members = true;
        ti->set_text(0, name);
        updating_members = false;
        return;
    }

    TreeItem *root = members->get_root();

    if (ti->get_parent() == root->get_children()) {

        if (edited_func == selected) {
            edited_func = new_name;
        }
        selected = new_name;

        int node_id = script->get_function_node_id(name);
        Ref<VisualScriptFunction> func;
        if (script->has_node(name, node_id)) {
            func = dynamic_ref_cast<VisualScriptFunction>(script->get_node(name, node_id));
        }
        undo_redo->create_action(TTR("Rename Function"));
        undo_redo->add_do_method(script.get(), "rename_function", name, new_name);
        undo_redo->add_undo_method(script.get(), "rename_function", new_name, name);
        if (func) {

            undo_redo->add_do_method(func.get(), "set_name", new_name);
            undo_redo->add_undo_method(func.get(), "set_name", name);
        }
        undo_redo->add_do_method(this, "_update_members");
        undo_redo->add_undo_method(this, "_update_members");
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
        undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
        undo_redo->commit_action();

        //		_update_graph();

        return; //or crash because it will become invalid
    }

    if (ti->get_parent() == root->get_children()->get_next()) {

        selected = new_name;
        undo_redo->create_action(TTR("Rename Variable"));
        undo_redo->add_do_method(script.get(), "rename_variable", name, new_name);
        undo_redo->add_undo_method(script.get(), "rename_variable", new_name, name);
        undo_redo->add_do_method(this, "_update_members");
        undo_redo->add_undo_method(this, "_update_members");
        undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
        undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
        undo_redo->commit_action();

        return; //or crash because it will become invalid
    }

    if (ti->get_parent() == root->get_children()->get_next()->get_next()) {

        selected = new_name;
        undo_redo->create_action(TTR("Rename Signal"));
        undo_redo->add_do_method(script.get(), "rename_custom_signal", name, new_name);
        undo_redo->add_undo_method(script.get(), "rename_custom_signal", new_name, name);
        undo_redo->add_do_method(this, "_update_members");
        undo_redo->add_undo_method(this, "_update_members");
        undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
        undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
        undo_redo->commit_action();

        return; //or crash because it will become invalid
    }
}

void VisualScriptEditor::_member_button(Object *p_item, int p_column, int p_button) {

    TreeItem *ti = Object::cast_to<TreeItem>(p_item);

    TreeItem *root = members->get_root();

    if (ti->get_parent() == root) {
        //main buttons
        if (ti == root->get_children()) {
            //add function, this one uses menu

            if (p_button == 1) {

                new_virtual_method_select->select_method_from_base_type(script->get_instance_base_type(), String(), true);

                return;
            } else if (p_button == 0) {

                String name = _validate_name("new_function");
                selected = name;
                edited_func = selected;

                Ref<VisualScriptFunction> func_node(make_ref_counted<VisualScriptFunction>());
                func_node->set_name(name);

                undo_redo->create_action(TTR("Add Function"));
                undo_redo->add_do_method(script.get(), "add_function", name);
                undo_redo->add_do_method(script.get(), "add_node", name, script->get_available_id(), func_node);
                undo_redo->add_undo_method(script.get(), "remove_function", name);
                undo_redo->add_do_method(this, "_update_members");
                undo_redo->add_undo_method(this, "_update_members");
                undo_redo->add_do_method(this, "_update_graph");
                undo_redo->add_undo_method(this, "_update_graph");
                undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
                undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
                undo_redo->commit_action();

                _update_graph();
            }

            return; //or crash because it will become invalid
        }

        if (ti == root->get_children()->get_next()) {
            //add variable
            String name = _validate_name("new_variable");
            selected = name;

            undo_redo->create_action(TTR("Add Variable"));
            undo_redo->add_do_method(script.get(), "add_variable", name);
            undo_redo->add_undo_method(script.get(), "remove_variable", name);
            undo_redo->add_do_method(this, "_update_members");
            undo_redo->add_undo_method(this, "_update_members");
            undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
            undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
            undo_redo->commit_action();
            return; //or crash because it will become invalid
        }

        if (ti == root->get_children()->get_next()->get_next()) {
            //add variable
            String name = _validate_name("new_signal");
            selected = name;

            undo_redo->create_action(TTR("Add Signal"));
            undo_redo->add_do_method(script.get(), "add_custom_signal", name);
            undo_redo->add_undo_method(script.get(), "remove_custom_signal", name);
            undo_redo->add_do_method(this, "_update_members");
            undo_redo->add_undo_method(this, "_update_members");
            undo_redo->add_do_method(this, "emit_signal", "edited_script_changed");
            undo_redo->add_undo_method(this, "emit_signal", "edited_script_changed");
            undo_redo->commit_action();
            return; //or crash because it will become invalid
        }
    }
}

void VisualScriptEditor::_expression_text_changed(const String &p_text, int p_id) {

    Ref<VisualScriptExpression> vse = dynamic_ref_cast<VisualScriptExpression>(script->get_node(edited_func, p_id));
    if (not vse)
        return;

    updating_graph = true;

    undo_redo->create_action(TTR("Change Expression"), UndoRedo::MERGE_ENDS);
    undo_redo->add_do_property(vse.get(), "expression", p_text);
    undo_redo->add_undo_property(vse.get(), "expression", vse->get("expression"));
    undo_redo->add_do_method(this, "_update_graph", p_id);
    undo_redo->add_undo_method(this, "_update_graph", p_id);
    undo_redo->commit_action();

    Node *node = graph->get_node(NodePath(itos(p_id)));
    if (Object::cast_to<Control>(node))
        Object::cast_to<Control>(node)->set_size(Vector2(1, 1)); //shrink if text is smaller

    updating_graph = false;
}

void VisualScriptEditor::_available_node_doubleclicked() {

    if (edited_func == String())
        return;

    TreeItem *item = nodes->get_selected();

    if (!item)
        return;

    String which = item->get_metadata(0);
    if (which.empty())
        return;
    Vector2 ofs = graph->get_scroll_ofs() + graph->get_size() * 0.5;

    if (graph->is_using_snap()) {
        int snap = graph->get_snap();
        ofs = ofs.snapped(Vector2(snap, snap));
    }

    ofs /= EDSCALE;

    while (true) {
        bool exists = false;
        List<int> existing;
        script->get_node_list(edited_func, &existing);
        for (List<int>::Element *E = existing.front(); E; E = E->next()) {
            Point2 pos = script->get_node_position(edited_func, E->deref());
            if (pos.distance_to(ofs) < 15) {
                ofs += Vector2(graph->get_snap(), graph->get_snap());
                exists = true;
                break;
            }
        }

        if (exists)
            continue;
        break;
    }

    Ref<VisualScriptNode> vnode = VisualScriptLanguage::singleton->create_node_from_name(which);
    int new_id = script->get_available_id();

    undo_redo->create_action(TTR("Add Node"));
    undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode, ofs);
    undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();

    Node *node = graph->get_node(NodePath(itos(new_id)));
    if (node) {
        graph->set_selected(node);
        _node_selected(node);
    }
}

void VisualScriptEditor::_update_available_nodes() {

    nodes->clear();

    TreeItem *root = nodes->create_item();

    Map<String, TreeItem *> path_cache;

    String filter = node_filter->get_text();

    List<String> fnodes;
    VisualScriptLanguage::singleton->get_registered_node_names(&fnodes);

    for (List<String>::Element *E = fnodes.front(); E; E = E->next()) {

        Vector<String> path = StringUtils::split(E->deref(),"/");

        if (not filter.empty() && !path.empty() && StringUtils::findn(path[path.size() - 1],filter) == -1)
            continue;

        String sp;
        TreeItem *parent = root;

        for (int i = 0; i < path.size() - 1; i++) {

            if (i > 0)
                sp += ",";
            sp += path[i];
            if (!path_cache.contains(sp)) {
                TreeItem *pathn = nodes->create_item(parent);
                pathn->set_selectable(0, false);
                pathn->set_text(0, StringUtils::capitalize(path[i]));
                path_cache[sp] = pathn;
                parent = pathn;
                if (filter.empty()) {
                    pathn->set_collapsed(true); //should remember state
                }
            } else {
                parent = path_cache[sp];
            }
        }

        TreeItem *item = nodes->create_item(parent);
        item->set_text(0, StringUtils::capitalize(path[path.size() - 1]));
        item->set_selectable(0, true);
        item->set_metadata(0, E->deref());
    }
}

String VisualScriptEditor::_validate_name(const String &p_name) const {

    String valid = p_name;

    int counter = 1;
    while (true) {

        bool exists = script->has_function(valid) || script->has_variable(valid) || script->has_custom_signal(valid);

        if (exists) {
            counter++;
            valid = p_name + "_" + itos(counter);
            continue;
        }

        break;
    }

    return valid;
}

void VisualScriptEditor::_on_nodes_delete() {

    List<int> to_erase;

    for (int i = 0; i < graph->get_child_count(); i++) {
        GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
        if (gn) {
            if (gn->is_selected() && gn->is_close_button_visible()) {
                to_erase.push_back(StringUtils::to_int(gn->get_name()));
            }
        }
    }

    if (to_erase.empty())
        return;

    undo_redo->create_action(TTR("Remove VisualScript Nodes"));

    for (List<int>::Element *F = to_erase.front(); F; F = F->next()) {

        undo_redo->add_do_method(script.get(), "remove_node", edited_func, F->deref());
        undo_redo->add_undo_method(script.get(), "add_node", edited_func, F->deref(), script->get_node(edited_func, F->deref()), script->get_node_position(edited_func, F->deref()));

        List<VisualScript::SequenceConnection> sequence_conns;
        script->get_sequence_connection_list(edited_func, &sequence_conns);

        for (List<VisualScript::SequenceConnection>::Element *E = sequence_conns.front(); E; E = E->next()) {

            if (E->deref().from_node == F->deref() || E->deref().to_node == F->deref()) {
                undo_redo->add_undo_method(script.get(), "sequence_connect", edited_func, E->deref().from_node, E->deref().from_output, E->deref().to_node);
            }
        }

        List<VisualScript::DataConnection> data_conns;
        script->get_data_connection_list(edited_func, &data_conns);

        for (List<VisualScript::DataConnection>::Element *E = data_conns.front(); E; E = E->next()) {

            if (E->deref().from_node == F->deref() || E->deref().to_node == F->deref()) {
                undo_redo->add_undo_method(script.get(), "data_connect", edited_func, E->deref().from_node, E->deref().from_port, E->deref().to_node, E->deref().to_port);
            }
        }
    }
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");

    undo_redo->commit_action();
}

void VisualScriptEditor::_on_nodes_duplicate() {

    List<int> to_duplicate;

    for (int i = 0; i < graph->get_child_count(); i++) {
        GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
        if (gn) {
            if (gn->is_selected() && gn->is_close_button_visible()) {
                to_duplicate.push_back(StringUtils::to_int(gn->get_name()));
            }
        }
    }

    if (to_duplicate.empty())
        return;

    undo_redo->create_action(TTR("Duplicate VisualScript Nodes"));
    int idc = script->get_available_id() + 1;

    Set<int> to_select;

    for (List<int>::Element *F = to_duplicate.front(); F; F = F->next()) {

        Ref<VisualScriptNode> node = script->get_node(edited_func, F->deref());

        Ref<VisualScriptNode> dupe = dynamic_ref_cast<VisualScriptNode>(node->duplicate(true));

        int new_id = idc++;
        to_select.insert(new_id);
        undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, dupe, script->get_node_position(edited_func, F->deref()) + Vector2(20, 20));
        undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
    }
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");

    undo_redo->commit_action();

    for (int i = 0; i < graph->get_child_count(); i++) {
        GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
        if (gn) {
            int id = StringUtils::to_int(gn->get_name());
            gn->set_selected(to_select.contains(id));
        }
    }

    if (!to_select.empty()) {
        EditorNode::get_singleton()->push_item(script->get_node(edited_func, *to_select.begin()).get());
    }
}

void VisualScriptEditor::_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventMouseButton> mb = dynamic_ref_cast<InputEventMouseButton>(p_event);

    if (mb && !mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
        revert_on_drag = String(); //so we can still drag functions
    }
}

void VisualScriptEditor::_generic_search(String p_base_type) {
    port_action_pos = graph->get_viewport()->get_mouse_position() - graph->get_global_position();
    new_connect_node_select->select_from_visual_script(p_base_type, false);
}

void VisualScriptEditor::_members_gui_input(const Ref<InputEvent> &p_event) {

    Ref<InputEventKey> key = dynamic_ref_cast<InputEventKey>(p_event);
    if (key && key->is_pressed() && !key->is_echo()) {
        if (members->has_focus()) {
            TreeItem *ti = members->get_selected();
            if (ti) {
                TreeItem *root = members->get_root();
                if (ti->get_parent() == root->get_children()) {
                    member_type = MEMBER_FUNCTION;
                }
                if (ti->get_parent() == root->get_children()->get_next()) {
                    member_type = MEMBER_VARIABLE;
                }
                if (ti->get_parent() == root->get_children()->get_next()->get_next()) {
                    member_type = MEMBER_SIGNAL;
                }
                member_name = ti->get_text(0);
            }
            if (ED_IS_SHORTCUT("visual_script_editor/delete_selected", p_event)) {
                _member_option(MEMBER_REMOVE);
            }
            if (ED_IS_SHORTCUT("visual_script_editor/edit_member", p_event)) {
                _member_option(MEMBER_EDIT);
            }
        }
    }
}

Variant VisualScriptEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {

    if (p_from == nodes) {

        TreeItem *it = nodes->get_item_at_position(p_point);
        if (!it)
            return Variant();
        String type = it->get_metadata(0);
        if (type.empty())
            return Variant();

        Dictionary dd;
        dd["type"] = "visual_script_node_drag";
        dd["node_type"] = type;

        Label *label = memnew(Label);
        label->set_text(it->get_text(0));
        set_drag_preview(label);
        return dd;
    }

    if (p_from == members) {

        TreeItem *it = members->get_item_at_position(p_point);
        if (!it)
            return Variant();

        String type = it->get_metadata(0);

        if (type.empty())
            return Variant();

        Dictionary dd;
        TreeItem *root = members->get_root();

        if (it->get_parent() == root->get_children()) {

            dd["type"] = "visual_script_function_drag";
            dd["function"] = type;
            if (!revert_on_drag.empty()) {
                edited_func = revert_on_drag; //revert so function does not change
                revert_on_drag = String();
                _update_graph();
            }
        } else if (it->get_parent() == root->get_children()->get_next()) {

            dd["type"] = "visual_script_variable_drag";
            dd["variable"] = type;
        } else if (it->get_parent() == root->get_children()->get_next()->get_next()) {

            dd["type"] = "visual_script_signal_drag";
            dd["signal"] = type;

        } else {
            return Variant();
        }

        Label *label = memnew(Label);
        label->set_text(it->get_text(0));
        set_drag_preview(label);
        return dd;
    }
    return Variant();
}

bool VisualScriptEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {

    if (p_from == graph) {

        Dictionary d = p_data;
        if (d.has("type") &&
                (String(d["type"]) == "visual_script_node_drag" ||
                        String(d["type"]) == "visual_script_function_drag" ||
                        String(d["type"]) == "visual_script_variable_drag" ||
                        String(d["type"]) == "visual_script_signal_drag" ||
                        String(d["type"]) == "obj_property" ||
                        String(d["type"]) == "resource" ||
                        String(d["type"]) == "files" ||
                        String(d["type"]) == "nodes")) {

            if (String(d["type"]) == "obj_property") {

#ifdef OSX_ENABLED
                const_cast<VisualScriptEditor *>(this)->_show_hint(vformat(TTR("Hold %s to drop a Getter. Hold Shift to drop a generic signature."), find_keycode_name(KEY_META)));
#else
                const_cast<VisualScriptEditor *>(this)->_show_hint(TTR("Hold Ctrl to drop a Getter. Hold Shift to drop a generic signature."));
#endif
            }

            if (String(d["type"]) == "nodes") {

#ifdef OSX_ENABLED
                const_cast<VisualScriptEditor *>(this)->_show_hint(vformat(TTR("Hold %s to drop a simple reference to the node."), find_keycode_name(KEY_META)));
#else
                const_cast<VisualScriptEditor *>(this)->_show_hint(TTR("Hold Ctrl to drop a simple reference to the node."));
#endif
            }

            if (String(d["type"]) == "visual_script_variable_drag") {

#ifdef OSX_ENABLED
                const_cast<VisualScriptEditor *>(this)->_show_hint(vformat(TTR("Hold %s to drop a Variable Setter."), find_keycode_name(KEY_META)));
#else
                const_cast<VisualScriptEditor *>(this)->_show_hint(TTR("Hold Ctrl to drop a Variable Setter."));
#endif
            }

            return true;
        }
    }

    return false;
}

#ifdef TOOLS_ENABLED

static Node *_find_script_node(Node *p_edited_scene, Node *p_current_node, const Ref<Script> &script) {

    if (p_edited_scene != p_current_node && p_current_node->get_owner() != p_edited_scene)
        return nullptr;

    Ref<Script> scr = refFromRefPtr<Script>(p_current_node->get_script());

    if (scr && scr == script)
        return p_current_node;

    for (int i = 0; i < p_current_node->get_child_count(); i++) {
        Node *n = _find_script_node(p_edited_scene, p_current_node->get_child(i), script);
        if (n)
            return n;
    }

    return nullptr;
}

#endif

void VisualScriptEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {

    if (p_from != graph) {
        return;
    }

    Dictionary d = p_data;

    if (!d.has("type")) {
        return;
    }

    if (String(d["type"]) == "visual_script_node_drag") {
        if (!d.has("node_type") || String(d["node_type"]) == "Null") {
            return;
        }

        Vector2 ofs = graph->get_scroll_ofs() + p_point;

        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }

        ofs /= EDSCALE;

        Ref<VisualScriptNode> vnode = VisualScriptLanguage::singleton->create_node_from_name(d["node_type"]);
        int new_id = script->get_available_id();

        undo_redo->create_action(TTR("Add Node"));
        undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode, ofs);
        undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->commit_action();

        Node *node = graph->get_node(NodePath(itos(new_id)));
        if (node) {
            graph->set_selected(node);
            _node_selected(node);
        }
    }

    if (String(d["type"]) == "visual_script_variable_drag") {

#ifdef OSX_ENABLED
        bool use_set = Input::get_singleton()->is_key_pressed(KEY_META);
#else
        bool use_set = Input::get_singleton()->is_key_pressed(KEY_CONTROL);
#endif
        Vector2 ofs = graph->get_scroll_ofs() + p_point;
        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }

        ofs /= EDSCALE;

        Ref<VisualScriptNode> vnode;
        if (use_set) {
            Ref<VisualScriptVariableSet> vnodes(make_ref_counted<VisualScriptVariableSet>());
            vnodes->set_variable(d["variable"]);
            vnode = vnodes;
        } else {

            Ref<VisualScriptVariableGet> vnodeg(make_ref_counted<VisualScriptVariableGet>());
            vnodeg->set_variable(d["variable"]);
            vnode = vnodeg;
        }

        int new_id = script->get_available_id();

        undo_redo->create_action(TTR("Add Node"));
        undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode, ofs);
        undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->commit_action();

        Node *node = graph->get_node(NodePath(itos(new_id)));
        if (node) {
            graph->set_selected(node);
            _node_selected(node);
        }
    }

    if (String(d["type"]) == "visual_script_function_drag") {

        Vector2 ofs = graph->get_scroll_ofs() + p_point;
        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }

        ofs /= EDSCALE;

        Ref<VisualScriptFunctionCall> vnode(make_ref_counted<VisualScriptFunctionCall>());
        vnode->set_call_mode(VisualScriptFunctionCall::CALL_MODE_SELF);

        int new_id = script->get_available_id();

        undo_redo->create_action(TTR("Add Node"));
        undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode, ofs);
        undo_redo->add_do_method(vnode.get(), "set_base_type", script->get_instance_base_type());
        undo_redo->add_do_method(vnode.get(), "set_function", d["function"]);

        undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->commit_action();

        Node *node = graph->get_node(NodePath(itos(new_id)));
        if (node) {
            graph->set_selected(node);
            _node_selected(node);
        }
    }

    if (String(d["type"]) == "visual_script_signal_drag") {

        Vector2 ofs = graph->get_scroll_ofs() + p_point;
        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }

        ofs /= EDSCALE;

        Ref<VisualScriptEmitSignal> vnode(make_ref_counted<VisualScriptEmitSignal>());
        vnode->set_signal(d["signal"]);

        int new_id = script->get_available_id();

        undo_redo->create_action(TTR("Add Node"));
        undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode, ofs);
        undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->commit_action();

        Node *node = graph->get_node(NodePath(itos(new_id)));
        if (node) {
            graph->set_selected(node);
            _node_selected(node);
        }
    }

    if (String(d["type"]) == "resource") {

        Vector2 ofs = graph->get_scroll_ofs() + p_point;
        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }

        ofs /= EDSCALE;

        Ref<VisualScriptPreload> prnode(make_ref_counted<VisualScriptPreload>());
        prnode->set_preload(refFromVariant<Resource>(d["resource"]));

        int new_id = script->get_available_id();

        undo_redo->create_action(TTR("Add Preload Node"));
        undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, prnode, ofs);
        undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->commit_action();

        Node *node = graph->get_node(NodePath(itos(new_id)));
        if (node) {
            graph->set_selected(node);
            _node_selected(node);
        }
    }

    if (String(d["type"]) == "files") {

        Vector2 ofs = graph->get_scroll_ofs() + p_point;
        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }

        ofs /= EDSCALE;

        Array files = d["files"];

        List<int> new_ids;
        int new_id = script->get_available_id();

        if (!files.empty()) {
            undo_redo->create_action(TTR("Add Preload Node"));

            for (int i = 0; i < files.size(); i++) {

                Ref<Resource> res = ResourceLoader::load(files[i]);
                if (not res)
                    continue;

                Ref<VisualScriptPreload> prnode(make_ref_counted<VisualScriptPreload>());
                prnode->set_preload(res);

                undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, prnode, ofs);
                undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
                new_ids.push_back(new_id);
                new_id++;
                ofs += Vector2(20, 20) * EDSCALE;
            }

            undo_redo->add_do_method(this, "_update_graph");
            undo_redo->add_undo_method(this, "_update_graph");
            undo_redo->commit_action();
        }

        for (List<int>::Element *E = new_ids.front(); E; E = E->next()) {

            Node *node = graph->get_node(NodePath(itos(E->deref())));
            if (node) {
                graph->set_selected(node);
                _node_selected(node);
            }
        }
    }

    if (String(d["type"]) == "nodes") {

        Node *sn = _find_script_node(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root(), script);

        if (!sn) {
            EditorNode::get_singleton()->show_warning("Can't drop nodes because script '" + get_name() + "' is not used in this scene.");
            return;
        }

#ifdef OSX_ENABLED
        bool use_node = Input::get_singleton()->is_key_pressed(KEY_META);
#else
        bool use_node = Input::get_singleton()->is_key_pressed(KEY_CONTROL);
#endif

        Array nodes = d["nodes"];

        Vector2 ofs = graph->get_scroll_ofs() + p_point;

        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }
        ofs /= EDSCALE;

        undo_redo->create_action(TTR("Add Node(s) From Tree"));
        int base_id = script->get_available_id();

        if (nodes.size() > 1) {
            use_node = true;
        }

        for (int i = 0; i < nodes.size(); i++) {

            NodePath np = nodes[i];
            Node *node = get_node(np);
            if (!node) {
                continue;
            }

            Ref<VisualScriptNode> n;

            if (use_node) {
                Ref<VisualScriptSceneNode> scene_node(make_ref_counted<VisualScriptSceneNode>());
                scene_node->set_node_path(sn->get_path_to(node));
                n = scene_node;

            } else {
                Ref<VisualScriptFunctionCall> call(make_ref_counted<VisualScriptFunctionCall>());
                call->set_call_mode(VisualScriptFunctionCall::CALL_MODE_NODE_PATH);
                call->set_base_path(sn->get_path_to(node));
                call->set_base_type(node->get_class_name());
                n = call;
                method_select->select_from_instance(node);
                selecting_method_id = base_id;
            }

            undo_redo->add_do_method(script.get(), "add_node", edited_func, base_id, n, ofs);
            undo_redo->add_undo_method(script.get(), "remove_node", edited_func, base_id);

            base_id++;
            ofs += Vector2(25, 25);
        }
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->commit_action();
    }

    if (String(d["type"]) == "obj_property") {

        Node *sn = _find_script_node(get_tree()->get_edited_scene_root(), get_tree()->get_edited_scene_root(), script);

        if (!sn && !Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {
            EditorNode::get_singleton()->show_warning("Can't drop properties because script '" + get_name() + "' is not used in this scene.\nDrop holding 'Shift' to just copy the signature.");
            return;
        }

        Object *obj = d["object"];

        if (!obj)
            return;

        Node *node = Object::cast_to<Node>(obj);
        Vector2 ofs = graph->get_scroll_ofs() + p_point;

        if (graph->is_using_snap()) {
            int snap = graph->get_snap();
            ofs = ofs.snapped(Vector2(snap, snap));
        }

        ofs /= EDSCALE;
#ifdef OSX_ENABLED
        bool use_get = Input::get_singleton()->is_key_pressed(KEY_META);
#else
        bool use_get = Input::get_singleton()->is_key_pressed(KEY_CONTROL);
#endif

        if (!node || Input::get_singleton()->is_key_pressed(KEY_SHIFT)) {

            if (use_get)
                undo_redo->create_action(TTR("Add Getter Property"));
            else
                undo_redo->create_action(TTR("Add Setter Property"));

            int base_id = script->get_available_id();

            Ref<VisualScriptNode> vnode;

            if (!use_get) {

                Ref<VisualScriptPropertySet> pset(make_ref_counted<VisualScriptPropertySet>());
                pset->set_call_mode(VisualScriptPropertySet::CALL_MODE_INSTANCE);
                pset->set_base_type(obj->get_class_name());
                /*if (use_value) {
                        pset->set_use_builtin_value(true);
                        pset->set_builtin_value(d["value"]);
                    }*/
                vnode = pset;
            } else {

                Ref<VisualScriptPropertyGet> pget(make_ref_counted<VisualScriptPropertyGet>());
                pget->set_call_mode(VisualScriptPropertyGet::CALL_MODE_INSTANCE);
                pget->set_base_type(obj->get_class_name());

                vnode = pget;
            }

            undo_redo->add_do_method(script.get(), "add_node", edited_func, base_id, vnode, ofs);
            undo_redo->add_do_method(vnode.get(), "set_property", d["property"]);
            if (!use_get) {
                undo_redo->add_do_method(vnode.get(), "set_default_input_value", 0, d["value"]);
            }

            undo_redo->add_undo_method(script.get(), "remove_node", edited_func, base_id);

            undo_redo->add_do_method(this, "_update_graph");
            undo_redo->add_undo_method(this, "_update_graph");
            undo_redo->commit_action();

        } else {

            if (use_get)
                undo_redo->create_action(TTR("Add Getter Property"));
            else
                undo_redo->create_action(TTR("Add Setter Property"));

            int base_id = script->get_available_id();

            Ref<VisualScriptNode> vnode;

            if (!use_get) {

                Ref<VisualScriptPropertySet> pset(make_ref_counted<VisualScriptPropertySet>());
                if (sn == node) {
                    pset->set_call_mode(VisualScriptPropertySet::CALL_MODE_SELF);
                } else {
                    pset->set_call_mode(VisualScriptPropertySet::CALL_MODE_NODE_PATH);
                    pset->set_base_path(sn->get_path_to(node));
                }

                vnode = pset;
            } else {

                Ref<VisualScriptPropertyGet> pget(make_ref_counted<VisualScriptPropertyGet>());
                if (sn == node) {
                    pget->set_call_mode(VisualScriptPropertyGet::CALL_MODE_SELF);
                } else {
                    pget->set_call_mode(VisualScriptPropertyGet::CALL_MODE_NODE_PATH);
                    pget->set_base_path(sn->get_path_to(node));
                }
                vnode = pget;
            }
            undo_redo->add_do_method(script.get(), "add_node", edited_func, base_id, vnode, ofs);
            undo_redo->add_do_method(vnode.get(), "set_property", d["property"]);
            if (!use_get) {
                undo_redo->add_do_method(vnode.get(), "set_default_input_value", 0, d["value"]);
            }
            undo_redo->add_undo_method(script.get(), "remove_node", edited_func, base_id);

            undo_redo->add_do_method(this, "_update_graph");
            undo_redo->add_undo_method(this, "_update_graph");
            undo_redo->commit_action();
        }
    }
}

void VisualScriptEditor::_selected_method(const String &p_method, const String &p_type, const bool p_connecting) {

    Ref<VisualScriptFunctionCall> vsfc = dynamic_ref_cast<VisualScriptFunctionCall>(script->get_node(edited_func, selecting_method_id));
    if (!vsfc)
        return;
    vsfc->set_function(p_method);
}

void VisualScriptEditor::_draw_color_over_button(Object *obj, Color p_color) {

    Button *button = Object::cast_to<Button>(obj);
    if (!button)
        return;

    Ref<StyleBox> normal = get_stylebox("normal", "Button");
    button->draw_rect(Rect2(normal->get_offset(), button->get_size() - normal->get_minimum_size()), p_color);
}

void VisualScriptEditor::_button_resource_previewed(const String &p_path, const Ref<Texture> &p_preview, const Ref<Texture> &p_small_preview, Variant p_ud) {

    Array ud = p_ud;
    ERR_FAIL_COND(ud.size() != 2)

    ObjectID id = ud[0];
    Object *obj = ObjectDB::get_instance(id);

    if (!obj)
        return;

    Button *b = Object::cast_to<Button>(obj);
    ERR_FAIL_COND(!b)

    if (not p_preview) {
        b->set_text(ud[1]);
    } else {

        b->set_icon(p_preview);
    }
}

/////////////////////////

void VisualScriptEditor::apply_code() {
}

RES VisualScriptEditor::get_edited_resource() const {
    return script;
}

void VisualScriptEditor::set_edited_resource(const RES &p_res) {

    script = dynamic_ref_cast<VisualScript>(p_res);
    signal_editor->script = script;
    signal_editor->undo_redo = undo_redo;
    variable_editor->script = script;
    variable_editor->undo_redo = undo_redo;

    script->connect("node_ports_changed", this, "_node_ports_changed");

    _update_members();
    _update_available_nodes();
}

Vector<String> VisualScriptEditor::get_functions() {

    return Vector<String>();
}

void VisualScriptEditor::reload_text() {
}

String VisualScriptEditor::get_name() {

    String name;
    //TODO: use PathUtils::is_internal_path ?
    if (not PathUtils::is_internal_path(script->get_path()) ) {
        name = PathUtils::get_file(script->get_path());
        if (is_unsaved()) {
            name += "(*)";
        }
    } else if (!script->get_name().empty())
        name = script->get_name();
    else
        name = String(script->get_class()) + "(" + itos(script->get_instance_id()) + ")";

    return name;
}

Ref<Texture> VisualScriptEditor::get_icon() {

    return Control::get_icon("VisualScript", "EditorIcons");
}

bool VisualScriptEditor::is_unsaved() {
#ifdef TOOLS_ENABLED

    return script->is_edited() || script->are_subnodes_edited();
#else
    return false;
#endif
}

Variant VisualScriptEditor::get_edit_state() {

    Dictionary d;
    d["function"] = edited_func;
    d["scroll"] = graph->get_scroll_ofs();
    d["zoom"] = graph->get_zoom();
    d["using_snap"] = graph->is_using_snap();
    d["snap"] = graph->get_snap();
    return d;
}

void VisualScriptEditor::set_edit_state(const Variant &p_state) {

    Dictionary d = p_state;
    if (d.has("function")) {
        edited_func = d["function"];
        selected = edited_func;
    }

    _update_graph();
    _update_members();

    if (d.has("scroll")) {
        graph->set_scroll_ofs(d["scroll"]);
    }
    if (d.has("zoom")) {
        graph->set_zoom(d["zoom"]);
    }
    if (d.has("snap")) {
        graph->set_snap(d["snap"]);
    }
    if (d.has("snap_enabled")) {
        graph->set_use_snap(d["snap_enabled"]);
    }
}

void VisualScriptEditor::_center_on_node(int p_id) {

    Node *n = graph->get_node(NodePath(itos(p_id)));
    GraphNode *gn = Object::cast_to<GraphNode>(n);
    if (gn) {
        gn->set_selected(true);
        Vector2 new_scroll = gn->get_offset() - graph->get_size() * 0.5 + gn->get_size() * 0.5;
        graph->set_scroll_ofs(new_scroll);
        script->set_function_scroll(edited_func, new_scroll / EDSCALE);
        script->set_edited(true); //so it's saved
    }
}

void VisualScriptEditor::goto_line(int p_line, bool p_with_error) {

    p_line += 1; //add one because script lines begin from 0.

    if (p_with_error)
        error_line = p_line;

    Vector<StringName> functions;
    script->get_function_list(&functions);
    for (int i=0,fin=functions.size(); i<fin; ++i) {

        if (script->has_node(functions[i], p_line)) {

            edited_func = functions[i];
            selected = edited_func;
            _update_graph();
            _update_members();

            call_deferred("call_deferred", "_center_on_node", p_line); //editor might be just created and size might not exist yet

            return;
        }
    }
}

void VisualScriptEditor::set_executing_line(int p_line) {
    // todo: add a way to show which node is executing right now.
}

void VisualScriptEditor::clear_executing_line() {
    // todo: add a way to show which node is executing right now.
}

void VisualScriptEditor::trim_trailing_whitespace() {
}

void VisualScriptEditor::insert_final_newline() {
}

void VisualScriptEditor::convert_indent_to_spaces() {
}

void VisualScriptEditor::convert_indent_to_tabs() {
}

void VisualScriptEditor::ensure_focus() {

    graph->grab_focus();
}

void VisualScriptEditor::tag_saved_version() {
}

void VisualScriptEditor::reload(bool p_soft) {
}

void VisualScriptEditor::get_breakpoints(List<int> *p_breakpoints) {

    Vector<StringName> functions;
    script->get_function_list(&functions);
    for (int i=0,fin=functions.size(); i<fin; ++i) {

        List<int> nodes;
        script->get_node_list(functions[i], &nodes);
        for (List<int>::Element *F = nodes.front(); F; F = F->next()) {

            Ref<VisualScriptNode> vsn = script->get_node(functions[i], F->deref());
            if (vsn->is_breakpoint()) {
                p_breakpoints->push_back(F->deref() - 1); //subtract 1 because breakpoints in text start from zero
            }
        }
    }
}

void VisualScriptEditor::add_callback(const String &p_function, PoolStringArray p_args) {

    if (script->has_function(p_function)) {
        edited_func = p_function;
        selected = edited_func;
        _update_members();
        _update_graph();
        return;
    }

    Ref<VisualScriptFunction> func(make_ref_counted<VisualScriptFunction>());
    for (int i = 0; i < p_args.size(); i++) {

        String name = p_args[i];
        VariantType type = VariantType::NIL;

        if ( StringUtils::contains(name,':') ) {
            String tt = StringUtils::get_slice(name,":", 1);
            name = StringUtils::get_slice(name,":", 0);
            for (int j = 0; j < (int)VariantType::VARIANT_MAX; j++) {

                String tname = Variant::get_type_name(VariantType(j));
                if (tname == tt) {
                    type = VariantType(j);
                    break;
                }
            }
        }

        func->add_argument(type, name);
    }

    func->set_name(p_function);
    script->add_function(p_function);
    script->add_node(p_function, script->get_available_id(), func);

    edited_func = p_function;
    selected = edited_func;
    _update_members();
    _update_graph();
    graph->call_deferred("set_scroll_ofs", script->get_function_scroll(edited_func)); //for first time it might need to be later

    //undo_redo->clear_history();
}

bool VisualScriptEditor::show_members_overview() {
    return false;
}

void VisualScriptEditor::update_settings() {

    _update_graph();
}

void VisualScriptEditor::set_debugger_active(bool p_active) {
    if (!p_active) {
        error_line = -1;
        _update_graph(); //clear line break
    }
}

void VisualScriptEditor::set_tooltip_request_func(String p_method, Object *p_obj) {
}

Control *VisualScriptEditor::get_edit_menu() {

    return edit_menu;
}

void VisualScriptEditor::_change_base_type() {

    select_base_type->popup_create(true, true);
}

void VisualScriptEditor::_toggle_tool_script() {
    script->set_tool_enabled(!script->is_tool());
}

void VisualScriptEditor::clear_edit_menu() {
    memdelete(edit_menu);
    memdelete(left_vsplit);
}

void VisualScriptEditor::_change_base_type_callback() {

    String bt = select_base_type->get_selected_type();

    ERR_FAIL_COND(bt.empty())
    undo_redo->create_action(TTR("Change Base Type"));
    undo_redo->add_do_method(script.get(), "set_instance_base_type", bt);
    undo_redo->add_undo_method(script.get(), "set_instance_base_type", script->get_instance_base_type());
    undo_redo->add_do_method(this, "_update_members");
    undo_redo->add_undo_method(this, "_update_members");
    undo_redo->commit_action();
}

void VisualScriptEditor::_node_selected(Node *p_node) {

    Ref<VisualScriptNode> vnode = refFromVariant<VisualScriptNode>(p_node->get_meta("__vnode"));
    if (not vnode)
        return;

    EditorNode::get_singleton()->push_item(vnode.get()); //edit node in inspector
}

static bool _get_out_slot(const Ref<VisualScriptNode> &p_node, int p_slot, int &r_real_slot, bool &r_sequence) {

    if (p_slot < p_node->get_output_sequence_port_count()) {
        r_sequence = true;
        r_real_slot = p_slot;

        return true;
    }

    r_real_slot = p_slot - p_node->get_output_sequence_port_count();
    r_sequence = false;

    return (r_real_slot < p_node->get_output_value_port_count());
}

static bool _get_in_slot(const Ref<VisualScriptNode> &p_node, int p_slot, int &r_real_slot, bool &r_sequence) {

    if (p_slot == 0 && p_node->has_input_sequence_port()) {
        r_sequence = true;
        r_real_slot = 0;
        return true;
    }

    r_real_slot = p_slot - (p_node->has_input_sequence_port() ? 1 : 0);
    r_sequence = false;

    return r_real_slot < p_node->get_input_value_port_count();
}

void VisualScriptEditor::_begin_node_move() {

    undo_redo->create_action(TTR("Move Node(s)"));
}

void VisualScriptEditor::_end_node_move() {

    undo_redo->commit_action();
}

void VisualScriptEditor::_move_node(String func, int p_id, const Vector2 &p_to) {

    if (func == String(edited_func)) {
        Node *node = graph->get_node(NodePath(itos(p_id)));
        if (Object::cast_to<GraphNode>(node))
            Object::cast_to<GraphNode>(node)->set_offset(p_to);
    }
    script->set_node_position(edited_func, p_id, p_to / EDSCALE);
}

void VisualScriptEditor::_node_moved(Vector2 p_from, Vector2 p_to, int p_id) {

    undo_redo->add_do_method(this, "_move_node", String(edited_func), p_id, p_to);
    undo_redo->add_undo_method(this, "_move_node", String(edited_func), p_id, p_from);
}

void VisualScriptEditor::_remove_node(int p_id) {

    undo_redo->create_action(TTR("Remove VisualScript Node"));

    undo_redo->add_do_method(script.get(), "remove_node", edited_func, p_id);
    undo_redo->add_undo_method(script.get(), "add_node", edited_func, p_id, script->get_node(edited_func, p_id), script->get_node_position(edited_func, p_id));

    List<VisualScript::SequenceConnection> sequence_conns;
    script->get_sequence_connection_list(edited_func, &sequence_conns);

    for (List<VisualScript::SequenceConnection>::Element *E = sequence_conns.front(); E; E = E->next()) {

        if (E->deref().from_node == p_id || E->deref().to_node == p_id) {
            undo_redo->add_undo_method(script.get(), "sequence_connect", edited_func, E->deref().from_node, E->deref().from_output, E->deref().to_node);
        }
    }

    List<VisualScript::DataConnection> data_conns;
    script->get_data_connection_list(edited_func, &data_conns);

    for (List<VisualScript::DataConnection>::Element *E = data_conns.front(); E; E = E->next()) {

        if (E->deref().from_node == p_id || E->deref().to_node == p_id) {
            undo_redo->add_undo_method(script.get(), "data_connect", edited_func, E->deref().from_node, E->deref().from_port, E->deref().to_node, E->deref().to_port);
        }
    }

    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");

    undo_redo->commit_action();
}

void VisualScriptEditor::_node_ports_changed(const String &p_func, int p_id) {

    if (p_func != String(edited_func))
        return;

    _update_graph(p_id);
}

void VisualScriptEditor::_graph_connected(const String &p_from, int p_from_slot, const String &p_to, int p_to_slot) {

    Ref<VisualScriptNode> from_node = script->get_node(edited_func, StringUtils::to_int(p_from));
    ERR_FAIL_COND(!from_node)

    bool from_seq;
    int from_port;

    if (!_get_out_slot(from_node, p_from_slot, from_port, from_seq))
        return; //can't connect this, it's invalid

    Ref<VisualScriptNode> to_node = script->get_node(edited_func, StringUtils::to_int(p_to));
    ERR_FAIL_COND(!to_node)

    bool to_seq;
    int to_port;

    if (!_get_in_slot(to_node, p_to_slot, to_port, to_seq))
        return; //can't connect this, it's invalid

    ERR_FAIL_COND(from_seq != to_seq)

    undo_redo->create_action(TTR("Connect Nodes"));

    if (from_seq) {
        undo_redo->add_do_method(script.get(), "sequence_connect", edited_func, StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to));
        undo_redo->add_undo_method(script.get(), "sequence_disconnect", edited_func, StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to));
    } else {

        // disconnect current, and connect the new one
        if (script->is_input_value_port_connected(edited_func, StringUtils::to_int(p_to), to_port)) {
            int conn_from;
            int conn_port;
            script->get_input_value_port_connection_source(edited_func, StringUtils::to_int(p_to), to_port, &conn_from, &conn_port);
            undo_redo->add_do_method(script.get(), "data_disconnect", edited_func, conn_from, conn_port, StringUtils::to_int(p_to), to_port);
            undo_redo->add_undo_method(script.get(), "data_connect", edited_func, conn_from, conn_port, StringUtils::to_int(p_to), to_port);
        }

        undo_redo->add_do_method(script.get(), "data_connect", edited_func, StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to), to_port);
        undo_redo->add_undo_method(script.get(), "data_disconnect", edited_func, StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to), to_port);
        //update nodes in sgraph
        undo_redo->add_do_method(this, "_update_graph", StringUtils::to_int(p_from));
        undo_redo->add_do_method(this, "_update_graph", StringUtils::to_int(p_to));
        undo_redo->add_undo_method(this, "_update_graph", StringUtils::to_int(p_from));
        undo_redo->add_undo_method(this, "_update_graph", StringUtils::to_int(p_to));
    }

    undo_redo->add_do_method(this, "_update_graph_connections");
    undo_redo->add_undo_method(this, "_update_graph_connections");

    undo_redo->commit_action();
}

void VisualScriptEditor::_graph_disconnected(const String &p_from, int p_from_slot, const String &p_to, int p_to_slot) {

    Ref<VisualScriptNode> from_node = script->get_node(edited_func, StringUtils::to_int(p_from));
    ERR_FAIL_COND(!from_node)

    bool from_seq;
    int from_port;

    if (!_get_out_slot(from_node, p_from_slot, from_port, from_seq))
        return; //can't connect this, it's invalid

    Ref<VisualScriptNode> to_node = script->get_node(edited_func, StringUtils::to_int(p_to));
    ERR_FAIL_COND(!to_node)

    bool to_seq;
    int to_port;

    if (!_get_in_slot(to_node, p_to_slot, to_port, to_seq))
        return; //can't connect this, it's invalid

    ERR_FAIL_COND(from_seq != to_seq)

    undo_redo->create_action(TTR("Connect Nodes"));

    if (from_seq) {
        undo_redo->add_do_method(script.get(), "sequence_disconnect", edited_func, StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to));
        undo_redo->add_undo_method(script.get(), "sequence_connect", edited_func, StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to));
    } else {
        undo_redo->add_do_method(script.get(), "data_disconnect", edited_func,StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to), to_port);
        undo_redo->add_undo_method(script.get(), "data_connect", edited_func, StringUtils::to_int(p_from), from_port, StringUtils::to_int(p_to), to_port);
        //update nodes in sgraph
        undo_redo->add_do_method(this, "_update_graph", StringUtils::to_int(p_from));
        undo_redo->add_do_method(this, "_update_graph", StringUtils::to_int(p_to));
        undo_redo->add_undo_method(this, "_update_graph", StringUtils::to_int(p_from));
        undo_redo->add_undo_method(this, "_update_graph", StringUtils::to_int(p_to));
    }
    undo_redo->add_do_method(this, "_update_graph_connections");
    undo_redo->add_undo_method(this, "_update_graph_connections");

    undo_redo->commit_action();
}

void VisualScriptEditor::_graph_connect_to_empty(const String &p_from, int p_from_slot, const Vector2 &p_release_pos) {

    Node *node = graph->get_node(NodePath(p_from));
    GraphNode *gn = Object::cast_to<GraphNode>(node);
    if (!gn)
        return;

    Ref<VisualScriptNode> vsn = script->get_node(edited_func, StringUtils::to_int(p_from));
    if (!vsn)
        return;

    port_action_pos = p_release_pos;

    if (p_from_slot < vsn->get_output_sequence_port_count()) {

        port_action_node = StringUtils::to_int(p_from);
        port_action_output = p_from_slot;
        _port_action_menu(CREATE_ACTION);
    } else {

        port_action_output = p_from_slot - vsn->get_output_sequence_port_count();
        port_action_node = StringUtils::to_int(p_from);
        _port_action_menu(CREATE_CALL_SET_GET);
    }
}

VisualScriptNode::TypeGuess VisualScriptEditor::_guess_output_type(int p_port_action_node, int p_port_action_output, Set<int> &visited_nodes) {

    VisualScriptNode::TypeGuess tg;
    tg.type = VariantType::NIL;

    if (visited_nodes.contains(p_port_action_node))
        return tg; //no loop

    visited_nodes.insert(p_port_action_node);

    Ref<VisualScriptNode> node = script->get_node(edited_func, p_port_action_node);

    if (!node) {

        return tg;
    }

    Vector<VisualScriptNode::TypeGuess> in_guesses;

    for (int i = 0; i < node->get_input_value_port_count(); i++) {
        PropertyInfo pi = node->get_input_value_port_info(i);
        VisualScriptNode::TypeGuess g;
        g.type = pi.type;

        if (g.type == VariantType::NIL || g.type == VariantType::OBJECT) {
            //any or object input, must further guess what this is
            int from_node;
            int from_port;

            if (script->get_input_value_port_connection_source(edited_func, p_port_action_node, i, &from_node, &from_port)) {

                g = _guess_output_type(from_node, from_port, visited_nodes);
            } else {
                Variant defval = node->get_default_input_value(i);
                if (defval.get_type() == VariantType::OBJECT) {

                    Object *obj = defval;

                    if (obj) {

                        g.type = VariantType::OBJECT;
                        g.gdclass = obj->get_class_name();
                        g.script = refFromRefPtr<Script>(obj->get_script());
                    }
                }
            }
        }

        in_guesses.push_back(g);
    }

    return node->guess_output_type(in_guesses.ptrw(), p_port_action_output);
}

void VisualScriptEditor::_port_action_menu(int p_option) {

    Vector2 ofs = graph->get_scroll_ofs() + port_action_pos;
    if (graph->is_using_snap()) {
        int snap = graph->get_snap();
        ofs = ofs.snapped(Vector2(snap, snap));
    }
    ofs /= EDSCALE;

    Set<int> vn;

    switch (p_option) {

        case CREATE_CALL_SET_GET: {
            Ref<VisualScriptFunctionCall> n(make_ref_counted<VisualScriptFunctionCall>());

            VisualScriptNode::TypeGuess tg = _guess_output_type(port_action_node, port_action_output, vn);

            if (tg.gdclass != StringName()) {
                n->set_base_type(tg.gdclass);
            } else {
                n->set_base_type("Object");
            }

            String type_string = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint_string;
            if (tg.type == VariantType::OBJECT) {
                if (tg.script) {
                    new_connect_node_select->select_from_script(tg.script, "");
                } else if (!type_string.empty()) {
                    new_connect_node_select->select_from_base_type(type_string);
                } else {
                    new_connect_node_select->select_from_base_type(n->get_base_type());
                }
            } else if (tg.type == VariantType::NIL) {
                new_connect_node_select->select_from_base_type("");
            } else {
                new_connect_node_select->select_from_basic_type(tg.type);
            }
        } break;
        case CREATE_ACTION: {
            VisualScriptNode::TypeGuess tg = _guess_output_type(port_action_node, port_action_output, vn);
            PropertyInfo property_info = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output);
            if (tg.type == VariantType::OBJECT) {
                if (property_info.type == VariantType::OBJECT && !property_info.hint_string.empty()) {
                    new_connect_node_select->select_from_action(property_info.hint_string);
                } else {
                    new_connect_node_select->select_from_action("");
                }
            } else if (tg.type == VariantType::NIL) {
                new_connect_node_select->select_from_action("");
            } else {
                new_connect_node_select->select_from_action(Variant::get_type_name(tg.type));
            }
        } break;
    }
}

void VisualScriptEditor::new_node(Ref<VisualScriptNode> vnode, Vector2 ofs) {
    Set<int> vn;
    Ref<VisualScriptNode> vnode_old = script->get_node(edited_func, port_action_node);
    int new_id = script->get_available_id();
    undo_redo->create_action(TTR("Add Node"));
    undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode, ofs);
    undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
    undo_redo->add_do_method(this, "_update_graph", new_id);
    undo_redo->add_undo_method(this, "_update_graph", new_id);
    undo_redo->commit_action();

    port_action_new_node = new_id;
}

void VisualScriptEditor::connect_data(Ref<VisualScriptNode> vnode_old, Ref<VisualScriptNode> vnode, int new_id) {
    undo_redo->create_action(TTR("Connect Node Data"));
    VisualScriptReturn *vnode_return = Object::cast_to<VisualScriptReturn>(vnode.get());
    if (vnode_return != nullptr && vnode_old->get_output_value_port_count() > 0) {
        vnode_return->set_enable_return_value(true);
    }
    if (vnode_old->get_output_value_port_count() <= 0) {
        undo_redo->commit_action();
        return;
    }
    if (vnode->get_input_value_port_count() <= 0) {
        undo_redo->commit_action();
        return;
    }
    int port = port_action_output;
    int value_count = vnode_old->get_output_value_port_count();
    if (port >= value_count) {
        port = 0;
    }
    undo_redo->add_do_method(script.get(), "data_connect", edited_func, port_action_node, port, new_id, 0);
    undo_redo->add_undo_method(script.get(), "data_disconnect", edited_func, port_action_node, port, new_id, 0);
    undo_redo->commit_action();
}

void VisualScriptEditor::_selected_connect_node(const String &p_text, const String &p_category, const bool p_connecting) {
    Vector2 ofs = graph->get_scroll_ofs() + port_action_pos;
    if (graph->is_using_snap()) {
        int snap = graph->get_snap();
        ofs = ofs.snapped(Vector2(snap, snap));
    }
    ofs /= EDSCALE;

    Set<int> vn;

    if (p_category == "visualscript") {
        Ref<VisualScriptNode> vnode_new = VisualScriptLanguage::singleton->create_node_from_name(p_text);
        Ref<VisualScriptNode> vnode_old = script->get_node(edited_func, port_action_node);
        int new_id = script->get_available_id();

        if (Object::cast_to<VisualScriptOperator>(vnode_new.get()) && script->get_node(edited_func, port_action_node)) {
            VariantType type = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).type;
            Object::cast_to<VisualScriptOperator>(vnode_new.get())->set_typed(type);
        }

        if (Object::cast_to<VisualScriptTypeCast>(vnode_new.get()) && script->get_node(edited_func, port_action_node)) {
            VariantType type = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).type;
            String hint_name = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint_string;

            if (type == VariantType::OBJECT) {
                Object::cast_to<VisualScriptTypeCast>(vnode_new.get())->set_base_type(hint_name);
            } else if (type == VariantType::NIL) {
                Object::cast_to<VisualScriptTypeCast>(vnode_new.get())->set_base_type("");
            } else {
                Object::cast_to<VisualScriptTypeCast>(vnode_new.get())->set_base_type(StaticCString(Variant::get_type_name(type),true));
            }
        }
        undo_redo->create_action(TTR("Add Node"));
        undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode_new, ofs);
        if (vnode_old && p_connecting) {
            connect_seq(vnode_old, vnode_new, new_id);
            connect_data(vnode_old, vnode_new, new_id);
        }

        undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
        undo_redo->add_do_method(this, "_update_graph");
        undo_redo->add_undo_method(this, "_update_graph");
        undo_redo->commit_action();
        return;
    }

    Ref<VisualScriptNode> vnode;

    if (p_category == String("method")) {

        Ref<VisualScriptFunctionCall> n(make_ref_counted<VisualScriptFunctionCall>());
        vnode = n;
    } else if (p_category == String("set")) {

        Ref<VisualScriptPropertySet> n(make_ref_counted<VisualScriptPropertySet>());
        n->set_property(p_text);
        vnode = n;
    } else if (p_category == String("get")) {

        Ref<VisualScriptPropertyGet> n(make_ref_counted<VisualScriptPropertyGet>());
        n->set_property(p_text);
        vnode = n;
    }

    if (p_category == String("action")) {
        if (p_text == "VisualScriptCondition") {

            Ref<VisualScriptCondition> n(make_ref_counted<VisualScriptCondition>());
            vnode = n;
        }
        if (p_text == "VisualScriptSwitch") {

            Ref<VisualScriptSwitch> n(make_ref_counted<VisualScriptSwitch>());
            vnode = n;
        } else if (p_text == "VisualScriptSequence") {

            Ref<VisualScriptSequence> n(make_ref_counted<VisualScriptSequence>());
            vnode = n;
        } else if (p_text == "VisualScriptIterator") {

            Ref<VisualScriptIterator> n(make_ref_counted<VisualScriptIterator>());
            vnode = n;
        } else if (p_text == "VisualScriptWhile") {

            Ref<VisualScriptWhile> n(make_ref_counted<VisualScriptWhile>());
            vnode = n;
        } else if (p_text == "VisualScriptReturn") {

            Ref<VisualScriptReturn> n(make_ref_counted<VisualScriptReturn>());
            vnode = n;
        }
    }

    new_node(vnode, ofs);

    Ref<VisualScriptNode> vsn = script->get_node(edited_func, port_action_new_node);

    if (Object::cast_to<VisualScriptFunctionCall>(vsn.get())) {

        Ref<VisualScriptFunctionCall> vsfc = dynamic_ref_cast<VisualScriptFunctionCall>(vsn);
        vsfc->set_function(p_text);

        if (p_connecting) {
            VisualScriptNode::TypeGuess tg = _guess_output_type(port_action_node, port_action_output, vn);

            if (tg.type == VariantType::OBJECT) {
                vsfc->set_call_mode(VisualScriptFunctionCall::CALL_MODE_INSTANCE);
                vsfc->set_base_type(String(""));
                if (tg.gdclass != StringName()) {
                    vsfc->set_base_type(tg.gdclass);

                } else if (script->get_node(edited_func, port_action_node)) {
                    PropertyHint hint = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint;
                    String base_type = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint_string;

                    if (!base_type.empty() && hint == PROPERTY_HINT_TYPE_STRING) {
                        vsfc->set_base_type(base_type);
                    }
                    if (p_text == "call" || p_text == "call_deferred") {
                        vsfc->set_function(String(""));
                    }
                }
                if (tg.script) {
                    vsfc->set_base_script(tg.script->get_path());
                }
            } else if (tg.type == VariantType::NIL) {
                vsfc->set_call_mode(VisualScriptFunctionCall::CALL_MODE_INSTANCE);
                vsfc->set_base_type(String(""));
            } else {
                vsfc->set_call_mode(VisualScriptFunctionCall::CALL_MODE_BASIC_TYPE);
                vsfc->set_basic_type(tg.type);
            }
        }
    }

    // if connecting from another node the call mode shouldn't be self
    if (p_connecting) {
        if (Object::cast_to<VisualScriptPropertySet>(vsn.get())) {
            Ref<VisualScriptPropertySet> vsp = dynamic_ref_cast<VisualScriptPropertySet>(vsn);

            VisualScriptNode::TypeGuess tg = _guess_output_type(port_action_node, port_action_output, vn);
            if (tg.type == VariantType::OBJECT) {
                vsp->set_call_mode(VisualScriptPropertySet::CALL_MODE_INSTANCE);
                vsp->set_base_type(String(""));
                if (tg.gdclass != StringName()) {
                    vsp->set_base_type(tg.gdclass);

                } else if (script->get_node(edited_func, port_action_node)) {
                    PropertyHint hint = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint;
                    String base_type = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint_string;

                    if (!base_type.empty() && hint == PROPERTY_HINT_TYPE_STRING) {
                        vsp->set_base_type(base_type);
                    }
                }
                if (tg.script) {
                    vsp->set_base_script(tg.script->get_path());
                }
            } else if (tg.type == VariantType::NIL) {
                vsp->set_call_mode(VisualScriptPropertySet::CALL_MODE_INSTANCE);
                vsp->set_base_type(String(""));
            } else {
                vsp->set_call_mode(VisualScriptPropertySet::CALL_MODE_BASIC_TYPE);
                vsp->set_basic_type(tg.type);
            }
        }

        if (Object::cast_to<VisualScriptPropertyGet>(vsn.get())) {
            Ref<VisualScriptPropertyGet> vsp = dynamic_ref_cast<VisualScriptPropertyGet>(vsn);

            VisualScriptNode::TypeGuess tg = _guess_output_type(port_action_node, port_action_output, vn);
            if (tg.type == VariantType::OBJECT) {
                vsp->set_call_mode(VisualScriptPropertyGet::CALL_MODE_INSTANCE);
                vsp->set_base_type(String(""));
                if (tg.gdclass != StringName()) {
                    vsp->set_base_type(tg.gdclass);

                } else if (script->get_node(edited_func, port_action_node)) {
                    PropertyHint hint = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint;
                    String base_type = script->get_node(edited_func, port_action_node)->get_output_value_port_info(port_action_output).hint_string;
                    if (!base_type.empty() && hint == PROPERTY_HINT_TYPE_STRING) {
                        vsp->set_base_type(base_type);
                    }
                }
                if (tg.script) {
                    vsp->set_base_script(tg.script->get_path());
                }
            } else if (tg.type == VariantType::NIL) {
                vsp->set_call_mode(VisualScriptPropertyGet::CALL_MODE_INSTANCE);
                vsp->set_base_type(String(""));
            } else {
                vsp->set_call_mode(VisualScriptPropertyGet::CALL_MODE_BASIC_TYPE);
                vsp->set_basic_type(tg.type);
            }
        }
    }
    Ref<VisualScriptNode> vnode_old = script->get_node(edited_func, port_action_node);
    if (vnode_old && p_connecting) {
        connect_seq(vnode_old, vnode, port_action_new_node);
        connect_data(vnode_old, vnode, port_action_new_node);
    }
    _update_graph(port_action_new_node);
    _update_graph_connections();
}

void VisualScriptEditor::connect_seq(Ref<VisualScriptNode> vnode_old, Ref<VisualScriptNode> vnode_new, int new_id) {
    VisualScriptOperator *vnode_operator = Object::cast_to<VisualScriptOperator>(vnode_new.get());
    if (vnode_operator != nullptr && !vnode_operator->has_input_sequence_port()) {
        return;
    }
    VisualScriptConstructor *vnode_constructor = Object::cast_to<VisualScriptConstructor>(vnode_new.get());
    if (vnode_constructor != nullptr) {
        return;
    }
    if (vnode_old->get_output_sequence_port_count() <= 0) {
        return;
    }
    if (!vnode_new->has_input_sequence_port()) {
        return;
    }

    undo_redo->create_action(TTR("Connect Node Sequence"));
    int pass_port = -vnode_old->get_output_sequence_port_count() + 1;
    int return_port = port_action_output - 1;
    if (vnode_old->get_output_value_port_info(port_action_output).name == String("pass") &&
            !script->get_output_sequence_ports_connected(edited_func, port_action_node).contains(pass_port)) {
        undo_redo->add_do_method(script.get(), "sequence_connect", edited_func, port_action_node, pass_port, new_id);
        undo_redo->add_undo_method(script.get(), "sequence_disconnect", edited_func, port_action_node, pass_port, new_id);
    } else if (vnode_old->get_output_value_port_info(port_action_output).name == String("return") &&
               !script->get_output_sequence_ports_connected(edited_func, port_action_node).contains(return_port)) {
        undo_redo->add_do_method(script.get(), "sequence_connect", edited_func, port_action_node, return_port, new_id);
        undo_redo->add_undo_method(script.get(), "sequence_disconnect", edited_func, port_action_node, return_port, new_id);
    } else {
        for (int port = 0; port < vnode_old->get_output_sequence_port_count(); port++) {
            int count = vnode_old->get_output_sequence_port_count();
            if (port_action_output < count && !script->get_output_sequence_ports_connected(edited_func, port_action_node).contains(port_action_output)) {
                undo_redo->add_do_method(script.get(), "sequence_connect", edited_func, port_action_node, port_action_output, new_id);
                undo_redo->add_undo_method(script.get(), "sequence_disconnect", edited_func, port_action_node, port_action_output, new_id);
                break;
            } else if (!script->get_output_sequence_ports_connected(edited_func, port_action_node).contains(port)) {
                undo_redo->add_do_method(script.get(), "sequence_connect", edited_func, port_action_node, port, new_id);
                undo_redo->add_undo_method(script.get(), "sequence_disconnect", edited_func, port_action_node, port, new_id);
                break;
            }
        }
    }

    undo_redo->commit_action();
}

void VisualScriptEditor::_selected_new_virtual_method(const String &p_text, const String &p_category, const bool p_connecting) {

    String name = p_text;
    if (script->has_function(name)) {
        EditorNode::get_singleton()->show_warning(vformat(TTR("Script already has function '%s'"), name));
        return;
    }

    MethodInfo minfo;
    {
        PODVector<MethodInfo> methods;
        bool found = false;
        ClassDB::get_virtual_methods(script->get_instance_base_type(), &methods);
        for(const MethodInfo & E : methods) {
            if (E.name == name) {
                minfo = E;
                found = true;
            }
        }

        ERR_FAIL_COND(!found)
    }

    selected = name;
    edited_func = selected;
    Ref<VisualScriptFunction> func_node(make_ref_counted<VisualScriptFunction>());
    func_node->set_name(name);

    undo_redo->create_action(TTR("Add Function"));
    undo_redo->add_do_method(script.get(), "add_function", name);

    for (int i = 0; i < minfo.arguments.size(); i++) {
        func_node->add_argument(minfo.arguments[i].type, minfo.arguments[i].name, -1, minfo.arguments[i].hint, minfo.arguments[i].hint_string);
    }

    undo_redo->add_do_method(script.get(), "add_node", name, script->get_available_id(), func_node);
    if (minfo.return_val.type != VariantType::NIL || minfo.return_val.usage & PROPERTY_USAGE_NIL_IS_VARIANT) {
        Ref<VisualScriptReturn> ret_node(make_ref_counted<VisualScriptReturn>());
        ret_node->set_return_type(minfo.return_val.type);
        ret_node->set_enable_return_value(true);
        ret_node->set_name(name);
        undo_redo->add_do_method(script.get(), "add_node", name, script->get_available_id() + 1, ret_node, Vector2(500, 0));
    }

    undo_redo->add_undo_method(script.get(), "remove_function", name);
    undo_redo->add_do_method(this, "_update_members");
    undo_redo->add_undo_method(this, "_update_members");
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");

    undo_redo->commit_action();

    _update_graph();
}

void VisualScriptEditor::_cancel_connect_node() {
    // Causes crashes
    //script->remove_node(edited_func, port_action_new_node);
    _update_graph();
}

void VisualScriptEditor::_create_new_node(const String &p_text, const String &p_category, const Vector2 &p_point) {
    Vector2 ofs = graph->get_scroll_ofs() + p_point;
    if (graph->is_using_snap()) {
        int snap = graph->get_snap();
        ofs = ofs.snapped(Vector2(snap, snap));
    }
    ofs /= EDSCALE;
    Ref<VisualScriptNode> vnode = VisualScriptLanguage::singleton->create_node_from_name(p_text);
    int new_id = script->get_available_id();
    undo_redo->create_action(TTR("Add Node"));
    undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, vnode, ofs);
    undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
    undo_redo->add_do_method(this, "_update_graph");
    undo_redo->add_undo_method(this, "_update_graph");
    undo_redo->commit_action();
}

void VisualScriptEditor::_default_value_changed() {

    Ref<VisualScriptNode> vsn = script->get_node(edited_func, editing_id);
    if (not vsn)
        return;

    undo_redo->create_action(TTR("Change Input Value"));
    undo_redo->add_do_method(vsn.get(), "set_default_input_value", editing_input, default_value_edit->get_variant());
    undo_redo->add_undo_method(vsn.get(), "set_default_input_value", editing_input, vsn->get_default_input_value(editing_input));

    undo_redo->add_do_method(this, "_update_graph", editing_id);
    undo_redo->add_undo_method(this, "_update_graph", editing_id);
    undo_redo->commit_action();
}

void VisualScriptEditor::_default_value_edited(Node *p_button, int p_id, int p_input_port) {

    Ref<VisualScriptNode> vsn = script->get_node(edited_func, p_id);
    if (not vsn)
        return;

    PropertyInfo pinfo = vsn->get_input_value_port_info(p_input_port);
    Variant existing = vsn->get_default_input_value(p_input_port);
    if (pinfo.type != VariantType::NIL && existing.get_type() != pinfo.type) {

        Variant::CallError ce;
        const Variant *existingp = &existing;
        existing = Variant::construct(pinfo.type, &existingp, 1, ce, false);
    }

    default_value_edit->set_position(Object::cast_to<Control>(p_button)->get_global_position() + Vector2(0, Object::cast_to<Control>(p_button)->get_size().y));
    default_value_edit->set_size(Size2(1, 1));

    if (pinfo.type == VariantType::NODE_PATH) {

        Node *edited_scene = get_tree()->get_edited_scene_root();
        Node *script_node = _find_script_node(edited_scene, edited_scene, script);

        if (script_node) {
            //pick a node relative to the script, IF the script exists
            pinfo.hint = PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE;
            pinfo.hint_string = (String)script_node->get_path();
        } else {
            //pick a path relative to edited scene
            pinfo.hint = PROPERTY_HINT_NODE_PATH_TO_EDITED_NODE;
            pinfo.hint_string = (String)get_tree()->get_edited_scene_root()->get_path();
        }
    }

    if (default_value_edit->edit(nullptr, pinfo.name, pinfo.type, existing, pinfo.hint, pinfo.hint_string)) {
        if (pinfo.hint == PROPERTY_HINT_MULTILINE_TEXT)
            default_value_edit->popup_centered_ratio();
        else
            default_value_edit->popup();
    }

    editing_id = p_id;
    editing_input = p_input_port;
}

void VisualScriptEditor::_show_hint(const String &p_hint) {

    hint_text->set_text(p_hint);
    hint_text->show();
    hint_text_timer->start();
}

void VisualScriptEditor::_hide_timer() {

    hint_text->hide();
}

void VisualScriptEditor::_node_filter_changed(const String &p_text) {

    _update_available_nodes();
}

void VisualScriptEditor::_notification(int p_what) {

    if (p_what == NOTIFICATION_READY || (p_what == NOTIFICATION_THEME_CHANGED && is_visible_in_tree())) {

        node_filter->set_right_icon(Control::get_icon("Search", "EditorIcons"));
        node_filter->set_clear_button_enabled(true);

        if (p_what == NOTIFICATION_READY) {
            variable_editor->connect("changed", this, "_update_members");
            signal_editor->connect("changed", this, "_update_members");
        }

        Ref<Theme> tm = EditorNode::get_singleton()->get_theme_base()->get_theme();

        bool dark_theme = tm->get_constant("dark_theme", "Editor");

        List<Pair<String, Color> > colors;
        if (dark_theme) {
            colors.push_back(Pair<String, Color>("flow_control", Color(0.96f, 0.96f, 0.96f)));
            colors.push_back(Pair<String, Color>("functions", Color(0.96f, 0.52f, 0.51f)));
            colors.push_back(Pair<String, Color>("data", Color(0.5, 0.96f, 0.81f)));
            colors.push_back(Pair<String, Color>("operators", Color(0.67f, 0.59f, 0.87f)));
            colors.push_back(Pair<String, Color>("custom", Color(0.5, 0.73f, 0.96f)));
            colors.push_back(Pair<String, Color>("constants", Color(0.96f, 0.5, 0.69f)));
        } else {
            colors.push_back(Pair<String, Color>("flow_control", Color(0.26f, 0.26f, 0.26f)));
            colors.push_back(Pair<String, Color>("functions", Color(0.95f, 0.4f, 0.38f)));
            colors.push_back(Pair<String, Color>("data", Color(0.07f, 0.73f, 0.51f)));
            colors.push_back(Pair<String, Color>("operators", Color(0.51f, 0.4f, 0.82f)));
            colors.push_back(Pair<String, Color>("custom", Color(0.31f, 0.63f, 0.95f)));
            colors.push_back(Pair<String, Color>("constants", Color(0.94f, 0.18f, 0.49f)));
        }

        for (List<Pair<String, Color> >::Element *E = colors.front(); E; E = E->next()) {
            Ref<StyleBoxFlat> sb = dynamic_ref_cast<StyleBoxFlat>(tm->get_stylebox("frame", "GraphNode"));
            if (sb) {
                Ref<StyleBoxFlat> frame_style = dynamic_ref_cast<StyleBoxFlat>(sb->duplicate());
                Color c = sb->get_border_color();
                Color cn = E->deref().second;
                cn.a = c.a;
                frame_style->set_border_color(cn);
                node_styles[E->deref().first] = frame_style;
            }
        }

        if (is_visible_in_tree() && script) {
            _update_members();
            _update_graph();
        }
    } else if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
        left_vsplit->set_visible(is_visible_in_tree());
    }
}

void VisualScriptEditor::_graph_ofs_changed(const Vector2 &p_ofs) {

    if (updating_graph || !script)
        return;

    updating_graph = true;

    if (script->has_function(edited_func)) {
        script->set_function_scroll(edited_func, graph->get_scroll_ofs() / EDSCALE);
        script->set_edited(true);
    }
    updating_graph = false;
}

void VisualScriptEditor::_comment_node_resized(const Vector2 &p_new_size, int p_node) {

    if (updating_graph)
        return;

    Ref<VisualScriptComment> vsc = dynamic_ref_cast<VisualScriptComment>(script->get_node(edited_func, p_node));
    if (not vsc)
        return;

    Node *node = graph->get_node(NodePath(itos(p_node)));
    GraphNode *gn = Object::cast_to<GraphNode>(node);
    if (!gn)
        return;

    updating_graph = true;

    graph->set_block_minimum_size_adjust(true); //faster resize

    undo_redo->create_action(TTR("Resize Comment"), UndoRedo::MERGE_ENDS);
    undo_redo->add_do_method(vsc.get(), "set_size", p_new_size / EDSCALE);
    undo_redo->add_undo_method(vsc.get(), "set_size", vsc->get_size());
    undo_redo->commit_action();

    gn->set_custom_minimum_size(p_new_size); //for this time since graph update is blocked
    gn->set_size(Size2(1, 1));
    graph->set_block_minimum_size_adjust(false);
    updating_graph = false;
}

void VisualScriptEditor::_menu_option(int p_what) {

    switch (p_what) {
        case EDIT_DELETE_NODES: {
            _on_nodes_delete();
        } break;
        case EDIT_TOGGLE_BREAKPOINT: {

            List<String> reselect;
            for (int i = 0; i < graph->get_child_count(); i++) {
                GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
                if (gn) {
                    if (gn->is_selected()) {
                        int id = StringUtils::to_int(gn->get_name());
                        Ref<VisualScriptNode> vsn = script->get_node(edited_func, id);
                        if (vsn) {
                            vsn->set_breakpoint(!vsn->is_breakpoint());
                            reselect.push_back(gn->get_name());
                        }
                    }
                }
            }

            _update_graph();

            for (List<String>::Element *E = reselect.front(); E; E = E->next()) {
                GraphNode *gn = Object::cast_to<GraphNode>(graph->get_node(NodePath(E->deref())));
                gn->set_selected(true);
            }

        } break;
        case EDIT_FIND_NODE_TYPE: {
            _generic_search(script->get_instance_base_type());
        } break;
        case EDIT_COPY_NODES:
        case EDIT_CUT_NODES: {

            if (!script->has_function(edited_func))
                break;

            clipboard->nodes.clear();
            clipboard->data_connections.clear();
            clipboard->sequence_connections.clear();

            for (int i = 0; i < graph->get_child_count(); i++) {
                GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
                if (gn) {
                    if (gn->is_selected()) {

                        int id = StringUtils::to_int(gn->get_name());
                        Ref<VisualScriptNode> node = script->get_node(edited_func, id);
                        if (Object::cast_to<VisualScriptFunction>(node.get())) {
                            EditorNode::get_singleton()->show_warning(TTR("Can't copy the function node."));
                            return;
                        }
                        if (node) {
                            clipboard->nodes[id] = dynamic_ref_cast<VisualScriptNode>(node->duplicate(true));
                            clipboard->nodes_positions[id] = script->get_node_position(edited_func, id);
                        }
                    }
                }
            }

            if (clipboard->nodes.empty())
                break;

            List<VisualScript::SequenceConnection> sequence_connections;

            script->get_sequence_connection_list(edited_func, &sequence_connections);

            for (List<VisualScript::SequenceConnection>::Element *E = sequence_connections.front(); E; E = E->next()) {

                if (clipboard->nodes.contains(E->deref().from_node) && clipboard->nodes.contains(E->deref().to_node)) {

                    clipboard->sequence_connections.insert(E->deref());
                }
            }

            List<VisualScript::DataConnection> data_connections;

            script->get_data_connection_list(edited_func, &data_connections);

            for (List<VisualScript::DataConnection>::Element *E = data_connections.front(); E; E = E->next()) {

                if (clipboard->nodes.contains(E->deref().from_node) && clipboard->nodes.contains(E->deref().to_node)) {

                    clipboard->data_connections.insert(E->deref());
                }
            }

            if (p_what == EDIT_CUT_NODES) {
                _on_nodes_delete(); // oh yeah, also delete on cut
            }

        } break;
        case EDIT_PASTE_NODES: {
            if (!script->has_function(edited_func))
                break;

            if (clipboard->nodes.empty()) {
                EditorNode::get_singleton()->show_warning(TTR("Clipboard is empty!"));
                break;
            }

            Map<int, int> remap;

            undo_redo->create_action(TTR("Paste VisualScript Nodes"));
            int idc = script->get_available_id() + 1;

            Set<int> to_select;

            Set<Vector2> existing_positions;

            {
                List<int> nodes;
                script->get_node_list(edited_func, &nodes);
                for (List<int>::Element *E = nodes.front(); E; E = E->next()) {
                    Vector2 pos = script->get_node_position(edited_func, E->deref()).snapped(Vector2(2, 2));
                    existing_positions.insert(pos);
                }
            }

            for (eastl::pair<const int,Ref<VisualScriptNode> > &E : clipboard->nodes) {

                Ref<VisualScriptNode> node = dynamic_ref_cast<VisualScriptNode>(E.second->duplicate());

                int new_id = idc++;
                to_select.insert(new_id);

                remap[E.first] = new_id;

                Vector2 paste_pos = clipboard->nodes_positions[E.first];

                while (existing_positions.contains(paste_pos.snapped(Vector2(2, 2)))) {
                    paste_pos += Vector2(20, 20) * EDSCALE;
                }

                undo_redo->add_do_method(script.get(), "add_node", edited_func, new_id, node, paste_pos);
                undo_redo->add_undo_method(script.get(), "remove_node", edited_func, new_id);
            }

            for (const VisualScript::SequenceConnection &E : clipboard->sequence_connections) {

                undo_redo->add_do_method(script.get(), "sequence_connect", edited_func, remap[E.from_node], E.from_output, remap[E.to_node]);
                undo_redo->add_undo_method(script.get(), "sequence_disconnect", edited_func, remap[E.from_node], E.from_output, remap[E.to_node]);
            }

            for (const VisualScript::DataConnection &E : clipboard->data_connections) {

                undo_redo->add_do_method(script.get(), "data_connect", edited_func, remap[E.from_node], E.from_port, remap[E.to_node], E.to_port);
                undo_redo->add_undo_method(script.get(), "data_disconnect", edited_func, remap[E.from_node], E.from_port, remap[E.to_node], E.to_port);
            }

            undo_redo->add_do_method(this, "_update_graph");
            undo_redo->add_undo_method(this, "_update_graph");

            undo_redo->commit_action();

            for (int i = 0; i < graph->get_child_count(); i++) {
                GraphNode *gn = Object::cast_to<GraphNode>(graph->get_child(i));
                if (gn) {
                    int id = StringUtils::to_int(gn->get_name());
                    gn->set_selected(to_select.contains(id));
                }
            }
        } break;
    }
}

void VisualScriptEditor::_member_rmb_selected(const Vector2 &p_pos) {

    TreeItem *ti = members->get_selected();
    ERR_FAIL_COND(!ti)

    member_popup->clear();
    member_popup->set_position(members->get_global_position() + p_pos);
    member_popup->set_size(Vector2());

    TreeItem *root = members->get_root();

    Ref<Texture> del_icon = Control::get_icon("Remove", "EditorIcons");

    Ref<Texture> edit_icon = Control::get_icon("Edit", "EditorIcons");

    if (ti->get_parent() == root->get_children()) {

        member_type = MEMBER_FUNCTION;
        member_name = ti->get_text(0);
        member_popup->add_icon_shortcut(del_icon, ED_GET_SHORTCUT("visual_script_editor/delete_selected"), MEMBER_REMOVE);
        member_popup->popup();
        return;
    }

    if (ti->get_parent() == root->get_children()->get_next()) {

        member_type = MEMBER_VARIABLE;
        member_name = ti->get_text(0);
        member_popup->add_icon_shortcut(edit_icon, ED_GET_SHORTCUT("visual_script_editor/edit_member"), MEMBER_EDIT);
        member_popup->add_separator();
        member_popup->add_icon_shortcut(del_icon, ED_GET_SHORTCUT("visual_script_editor/delete_selected"), MEMBER_REMOVE);
        member_popup->popup();
        return;
    }

    if (ti->get_parent() == root->get_children()->get_next()->get_next()) {

        member_type = MEMBER_SIGNAL;
        member_name = ti->get_text(0);
        member_popup->add_icon_shortcut(edit_icon, ED_GET_SHORTCUT("visual_script_editor/edit_member"), MEMBER_EDIT);
        member_popup->add_separator();
        member_popup->add_icon_shortcut(del_icon, ED_GET_SHORTCUT("visual_script_editor/delete_selected"), MEMBER_REMOVE);
        member_popup->popup();
        return;
    }
}

void VisualScriptEditor::_member_option(int p_option) {

    switch (member_type) {
        case MEMBER_FUNCTION: {

            if (p_option == MEMBER_REMOVE) {
                //delete the function
                String name = member_name;

                undo_redo->create_action(TTR("Remove Function"));
                undo_redo->add_do_method(script.get(), "remove_function", name);
                undo_redo->add_undo_method(script.get(), "add_function", name);
                List<int> nodes;
                script->get_node_list(name, &nodes);
                for (List<int>::Element *E = nodes.front(); E; E = E->next()) {
                    undo_redo->add_undo_method(script.get(), "add_node", name, E->deref(), script->get_node(name, E->deref()), script->get_node_position(name, E->deref()));
                }

                List<VisualScript::SequenceConnection> seq_connections;

                script->get_sequence_connection_list(name, &seq_connections);

                for (List<VisualScript::SequenceConnection>::Element *E = seq_connections.front(); E; E = E->next()) {
                    undo_redo->add_undo_method(script.get(), "sequence_connect", name, E->deref().from_node, E->deref().from_output, E->deref().to_node);
                }

                List<VisualScript::DataConnection> data_connections;

                script->get_data_connection_list(name, &data_connections);

                for (List<VisualScript::DataConnection>::Element *E = data_connections.front(); E; E = E->next()) {
                    undo_redo->add_undo_method(script.get(), "data_connect", name, E->deref().from_node, E->deref().from_port, E->deref().to_node, E->deref().to_port);
                }

                undo_redo->add_do_method(this, "_update_members");
                undo_redo->add_undo_method(this, "_update_members");
                undo_redo->add_do_method(this, "_update_graph");
                undo_redo->add_undo_method(this, "_update_graph");
                undo_redo->commit_action();
            }
        } break;
        case MEMBER_VARIABLE: {

            String name = member_name;

            if (p_option == MEMBER_REMOVE) {
                undo_redo->create_action(TTR("Remove Variable"));
                undo_redo->add_do_method(script.get(), "remove_variable", name);
                undo_redo->add_undo_method(script.get(), "add_variable", name, script->get_variable_default_value(name));
                undo_redo->add_undo_method(script.get(), "set_variable_info", name, script->call("get_variable_info", name)); //return as dict
                undo_redo->add_do_method(this, "_update_members");
                undo_redo->add_undo_method(this, "_update_members");
                undo_redo->commit_action();
            } else if (p_option == MEMBER_EDIT) {
                variable_editor->edit(name);
                edit_variable_dialog->set_title(TTR("Editing Variable:") + " " + name);
                edit_variable_dialog->popup_centered_minsize(Size2(400, 200) * EDSCALE);
            }
        } break;
        case MEMBER_SIGNAL: {
            String name = member_name;

            if (p_option == MEMBER_REMOVE) {
                undo_redo->create_action(TTR("Remove Signal"));
                undo_redo->add_do_method(script.get(), "remove_custom_signal", name);
                undo_redo->add_undo_method(script.get(), "add_custom_signal", name);

                for (int i = 0; i < script->custom_signal_get_argument_count(name); i++) {
                    undo_redo->add_undo_method(script.get(), "custom_signal_add_argument", name, script->custom_signal_get_argument_name(name, i), script->custom_signal_get_argument_type(name, i));
                }

                undo_redo->add_do_method(this, "_update_members");
                undo_redo->add_undo_method(this, "_update_members");
                undo_redo->commit_action();
            } else if (p_option == MEMBER_EDIT) {

                signal_editor->edit(name);
                edit_signal_dialog->set_title(TTR("Editing Signal:") + " " + name);
                edit_signal_dialog->popup_centered_minsize(Size2(400, 300) * EDSCALE);
            }
        } break;
    }
}

void VisualScriptEditor::add_syntax_highlighter(SyntaxHighlighter *p_highlighter) {
}

void VisualScriptEditor::set_syntax_highlighter(SyntaxHighlighter *p_highlighter) {
}

void VisualScriptEditor::_bind_methods() {

    MethodBinder::bind_method("_member_button", &VisualScriptEditor::_member_button);
    MethodBinder::bind_method("_member_edited", &VisualScriptEditor::_member_edited);
    MethodBinder::bind_method("_member_selected", &VisualScriptEditor::_member_selected);
    MethodBinder::bind_method("_update_members", &VisualScriptEditor::_update_members);
    MethodBinder::bind_method("_change_base_type", &VisualScriptEditor::_change_base_type);
    MethodBinder::bind_method("_change_base_type_callback", &VisualScriptEditor::_change_base_type_callback);
    MethodBinder::bind_method("_toggle_tool_script", &VisualScriptEditor::_toggle_tool_script);
    MethodBinder::bind_method("_node_selected", &VisualScriptEditor::_node_selected);
    MethodBinder::bind_method("_node_moved", &VisualScriptEditor::_node_moved);
    MethodBinder::bind_method("_move_node", &VisualScriptEditor::_move_node);
    MethodBinder::bind_method("_begin_node_move", &VisualScriptEditor::_begin_node_move);
    MethodBinder::bind_method("_end_node_move", &VisualScriptEditor::_end_node_move);
    MethodBinder::bind_method("_remove_node", &VisualScriptEditor::_remove_node);
    MethodBinder::bind_method("_update_graph", &VisualScriptEditor::_update_graph, {DEFVAL(-1)});
    MethodBinder::bind_method("_node_ports_changed", &VisualScriptEditor::_node_ports_changed);
    MethodBinder::bind_method("_available_node_doubleclicked", &VisualScriptEditor::_available_node_doubleclicked);
    MethodBinder::bind_method("_default_value_edited", &VisualScriptEditor::_default_value_edited);
    MethodBinder::bind_method("_default_value_changed", &VisualScriptEditor::_default_value_changed);
    MethodBinder::bind_method("_menu_option", &VisualScriptEditor::_menu_option);
    MethodBinder::bind_method("_graph_ofs_changed", &VisualScriptEditor::_graph_ofs_changed);
    MethodBinder::bind_method("_center_on_node", &VisualScriptEditor::_center_on_node);
    MethodBinder::bind_method("_comment_node_resized", &VisualScriptEditor::_comment_node_resized);
    MethodBinder::bind_method("_button_resource_previewed", &VisualScriptEditor::_button_resource_previewed);
    MethodBinder::bind_method("_port_action_menu", &VisualScriptEditor::_port_action_menu);
    MethodBinder::bind_method("_selected_connect_node", &VisualScriptEditor::_selected_connect_node);
    MethodBinder::bind_method("_selected_new_virtual_method", &VisualScriptEditor::_selected_new_virtual_method);

    MethodBinder::bind_method("_cancel_connect_node", &VisualScriptEditor::_cancel_connect_node);
    MethodBinder::bind_method("_create_new_node", &VisualScriptEditor::_create_new_node);
    MethodBinder::bind_method("_expression_text_changed", &VisualScriptEditor::_expression_text_changed);

    MethodBinder::bind_method("get_drag_data_fw", &VisualScriptEditor::get_drag_data_fw);
    MethodBinder::bind_method("can_drop_data_fw", &VisualScriptEditor::can_drop_data_fw);
    MethodBinder::bind_method("drop_data_fw", &VisualScriptEditor::drop_data_fw);

    MethodBinder::bind_method("_input", &VisualScriptEditor::_input);
    MethodBinder::bind_method("_members_gui_input", &VisualScriptEditor::_members_gui_input);
    MethodBinder::bind_method("_on_nodes_delete", &VisualScriptEditor::_on_nodes_delete);
    MethodBinder::bind_method("_on_nodes_duplicate", &VisualScriptEditor::_on_nodes_duplicate);

    MethodBinder::bind_method("_hide_timer", &VisualScriptEditor::_hide_timer);

    MethodBinder::bind_method("_graph_connected", &VisualScriptEditor::_graph_connected);
    MethodBinder::bind_method("_graph_disconnected", &VisualScriptEditor::_graph_disconnected);
    MethodBinder::bind_method("_graph_connect_to_empty", &VisualScriptEditor::_graph_connect_to_empty);

    MethodBinder::bind_method("_update_graph_connections", &VisualScriptEditor::_update_graph_connections);
    MethodBinder::bind_method("_node_filter_changed", &VisualScriptEditor::_node_filter_changed);

    MethodBinder::bind_method("_selected_method", &VisualScriptEditor::_selected_method);
    MethodBinder::bind_method("_draw_color_over_button", &VisualScriptEditor::_draw_color_over_button);

    MethodBinder::bind_method("_member_rmb_selected", &VisualScriptEditor::_member_rmb_selected);

    MethodBinder::bind_method("_member_option", &VisualScriptEditor::_member_option);

    MethodBinder::bind_method("_update_available_nodes", &VisualScriptEditor::_update_available_nodes);

    MethodBinder::bind_method("_generic_search", &VisualScriptEditor::_generic_search);
}

VisualScriptEditor::VisualScriptEditor() {

    if (!clipboard) {
        clipboard = memnew(Clipboard);
    }
    updating_graph = false;

    edit_menu = memnew(MenuButton);
    edit_menu->set_text(TTR("Edit"));
    edit_menu->set_switch_on_hover(true);
    edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("visual_script_editor/delete_selected"), EDIT_DELETE_NODES);
    edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("visual_script_editor/toggle_breakpoint"), EDIT_TOGGLE_BREAKPOINT);
    edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("visual_script_editor/find_node_type"), EDIT_FIND_NODE_TYPE);
    edit_menu->get_popup()->add_separator();
    edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("visual_script_editor/copy_nodes"), EDIT_COPY_NODES);
    edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("visual_script_editor/cut_nodes"), EDIT_CUT_NODES);
    edit_menu->get_popup()->add_shortcut(ED_GET_SHORTCUT("visual_script_editor/paste_nodes"), EDIT_PASTE_NODES);

    edit_menu->get_popup()->connect("id_pressed", this, "_menu_option");

    left_vsplit = memnew(VSplitContainer);
    ScriptEditor::get_singleton()->get_left_list_split()->call_deferred("add_child", Variant(left_vsplit)); //add but wait until done settig up this
    left_vsplit->set_v_size_flags(SIZE_EXPAND_FILL);
    left_vsplit->set_stretch_ratio(2);
    left_vsplit->hide();

    VBoxContainer *left_vb = memnew(VBoxContainer);
    left_vsplit->add_child(left_vb);
    left_vb->set_v_size_flags(SIZE_EXPAND_FILL);
    //left_vb->set_custom_minimum_size(Size2(230, 1) * EDSCALE);

    CheckButton *tool_script_check = memnew(CheckButton);
    tool_script_check->set_text(TTR("Make Tool:"));
    left_vb->add_child(tool_script_check);
    tool_script_check->connect("pressed", this, "_toggle_tool_script");

    base_type_select = memnew(Button);
    left_vb->add_margin_child(TTR("Base Type:"), base_type_select);
    base_type_select->connect("pressed", this, "_change_base_type");

    members = memnew(Tree);
    left_vb->add_margin_child(TTR("Members:"), members, true);
    members->set_hide_root(true);
    members->connect("button_pressed", this, "_member_button");
    members->connect("item_edited", this, "_member_edited");
    members->connect("cell_selected", this, "_member_selected", varray(), ObjectNS::CONNECT_DEFERRED);
    members->connect("gui_input", this, "_members_gui_input");
    members->set_allow_reselect(true);
    members->set_hide_folding(true);
    members->set_drag_forwarding(this);

    VBoxContainer *left_vb2 = memnew(VBoxContainer);
    left_vsplit->add_child(left_vb2);
    left_vb2->set_v_size_flags(SIZE_EXPAND_FILL);

    VBoxContainer *vbc_nodes = memnew(VBoxContainer);
    HBoxContainer *hbc_nodes = memnew(HBoxContainer);
    node_filter = memnew(LineEdit);
    node_filter->connect("text_changed", this, "_node_filter_changed");
    hbc_nodes->add_child(node_filter);
    node_filter->set_h_size_flags(SIZE_EXPAND_FILL);
    vbc_nodes->add_child(hbc_nodes);

    nodes = memnew(Tree);
    vbc_nodes->add_child(nodes);
    nodes->set_v_size_flags(SIZE_EXPAND_FILL);

    left_vb2->add_margin_child(TTR("Available Nodes:"), vbc_nodes, true);

    nodes->set_hide_root(true);
    nodes->connect("item_activated", this, "_available_node_doubleclicked");
    nodes->set_drag_forwarding(this);

    graph = memnew(GraphEdit);
    add_child(graph);
    graph->set_v_size_flags(Control::SIZE_EXPAND_FILL);
    graph->set_anchors_and_margins_preset(Control::PRESET_WIDE);
    graph->connect("node_selected", this, "_node_selected");
    graph->connect("_begin_node_move", this, "_begin_node_move");
    graph->connect("_end_node_move", this, "_end_node_move");
    graph->connect("delete_nodes_request", this, "_on_nodes_delete");
    graph->connect("duplicate_nodes_request", this, "_on_nodes_duplicate");
    graph->set_drag_forwarding(this);
    graph->hide();
    graph->connect("scroll_offset_changed", this, "_graph_ofs_changed");

    select_func_text = memnew(Label);
    select_func_text->set_text(TTR("Select or create a function to edit its graph."));
    select_func_text->set_align(Label::ALIGN_CENTER);
    select_func_text->set_valign(Label::VALIGN_CENTER);
    select_func_text->set_h_size_flags(SIZE_EXPAND_FILL);
    add_child(select_func_text);

    hint_text = memnew(Label);
    hint_text->set_anchor_and_margin(MARGIN_TOP, ANCHOR_END, -100);
    hint_text->set_anchor_and_margin(MARGIN_BOTTOM, ANCHOR_END, 0);
    hint_text->set_anchor_and_margin(MARGIN_RIGHT, ANCHOR_END, 0);
    hint_text->set_align(Label::ALIGN_CENTER);
    hint_text->set_valign(Label::VALIGN_CENTER);
    graph->add_child(hint_text);

    hint_text_timer = memnew(Timer);
    hint_text_timer->set_wait_time(4);
    hint_text_timer->connect("timeout", this, "_hide_timer");
    add_child(hint_text_timer);

    //allowed casts (connections)
    for (int i = 0; i < (int)VariantType::VARIANT_MAX; i++) {
        graph->add_valid_connection_type((int)VariantType::NIL, i);
        graph->add_valid_connection_type(i, (int)VariantType::NIL);
        for (int j = 0; j < (int)VariantType::VARIANT_MAX; j++) {
            if (Variant::can_convert(VariantType(i), VariantType(j))) {
                graph->add_valid_connection_type(i, j);
            }
        }

        graph->add_valid_right_disconnect_type(i);
    }

    graph->add_valid_left_disconnect_type(TYPE_SEQUENCE);

    graph->connect("connection_request", this, "_graph_connected");
    graph->connect("disconnection_request", this, "_graph_disconnected");
    graph->connect("connection_to_empty", this, "_graph_connect_to_empty");

    edit_signal_dialog = memnew(AcceptDialog);
    edit_signal_dialog->get_ok()->set_text(TTR("Close"));
    add_child(edit_signal_dialog);

    signal_editor = memnew(VisualScriptEditorSignalEdit);
    edit_signal_edit = memnew(EditorInspector);
    edit_signal_dialog->add_child(edit_signal_edit);

    edit_signal_edit->edit(signal_editor);

    edit_variable_dialog = memnew(AcceptDialog);
    edit_variable_dialog->get_ok()->set_text(TTR("Close"));
    add_child(edit_variable_dialog);

    variable_editor = memnew(VisualScriptEditorVariableEdit);
    edit_variable_edit = memnew(EditorInspector);
    edit_variable_dialog->add_child(edit_variable_edit);

    edit_variable_edit->edit(variable_editor);

    select_base_type = memnew(CreateDialog);
    select_base_type->set_base_type("Object"); //anything goes
    select_base_type->connect("create", this, "_change_base_type_callback");
    add_child(select_base_type);

    undo_redo = EditorNode::get_singleton()->get_undo_redo();

    updating_members = false;

    set_process_input(true); //for revert on drag
    set_process_unhandled_input(true); //for revert on drag

    default_value_edit = memnew(CustomPropertyEditor);
    add_child(default_value_edit);
    default_value_edit->connect("variant_changed", this, "_default_value_changed");

    method_select = memnew(VisualScriptPropertySelector);
    add_child(method_select);
    method_select->connect("selected", this, "_selected_method");
    error_line = -1;

    new_connect_node_select = memnew(VisualScriptPropertySelector);
    add_child(new_connect_node_select);
    new_connect_node_select->connect("selected", this, "_selected_connect_node");
    new_connect_node_select->get_cancel()->connect("pressed", this, "_cancel_connect_node");

    new_virtual_method_select = memnew(VisualScriptPropertySelector);
    add_child(new_virtual_method_select);
    new_virtual_method_select->connect("selected", this, "_selected_new_virtual_method");

    member_popup = memnew(PopupMenu);
    add_child(member_popup);
    members->connect("item_rmb_selected", this, "_member_rmb_selected");
    members->set_allow_rmb_select(true);
    member_popup->connect("id_pressed", this, "_member_option");

    _VisualScriptEditor::get_singleton()->connect("custom_nodes_updated", this, "_update_available_nodes");
}

VisualScriptEditor::~VisualScriptEditor() {

    undo_redo->clear_history(); //avoid crashes
    memdelete(signal_editor);
    memdelete(variable_editor);
}

static ScriptEditorBase *create_editor(const RES &p_resource) {

    if (Object::cast_to<VisualScript>(p_resource.get())) {
        return memnew(VisualScriptEditor);
    }

    return nullptr;
}

VisualScriptEditor::Clipboard *VisualScriptEditor::clipboard = nullptr;

void VisualScriptEditor::free_clipboard() {
    if (clipboard)
        memdelete(clipboard);
}

static void register_editor_callback() {

    ScriptEditor::register_create_script_editor_function(create_editor);

    ED_SHORTCUT("visual_script_editor/delete_selected", TTR("Delete Selected"), KEY_DELETE);
    ED_SHORTCUT("visual_script_editor/toggle_breakpoint", TTR("Toggle Breakpoint"), KEY_F9);
    ED_SHORTCUT("visual_script_editor/find_node_type", TTR("Find Node Type"), KEY_MASK_CMD + KEY_F);
    ED_SHORTCUT("visual_script_editor/copy_nodes", TTR("Copy Nodes"), KEY_MASK_CMD + KEY_C);
    ED_SHORTCUT("visual_script_editor/cut_nodes", TTR("Cut Nodes"), KEY_MASK_CMD + KEY_X);
    ED_SHORTCUT("visual_script_editor/paste_nodes", TTR("Paste Nodes"), KEY_MASK_CMD + KEY_V);
    ED_SHORTCUT("visual_script_editor/edit_member", TTR("Edit Member"), KEY_MASK_CMD + KEY_E);
}

void VisualScriptEditor::register_editor() {

    //too early to register stuff here, request a callback
    EditorNode::add_plugin_init_callback(register_editor_callback);
}

Ref<VisualScriptNode> _VisualScriptEditor::create_node_custom(const String &p_name) {

    Ref<VisualScriptCustomNode> node(make_ref_counted<VisualScriptCustomNode>());
    node->set_script(singleton->custom_nodes[p_name]);
    return node;
}

_VisualScriptEditor *_VisualScriptEditor::singleton = nullptr;
Map<String, RefPtr> _VisualScriptEditor::custom_nodes;

_VisualScriptEditor::_VisualScriptEditor() {
    singleton = this;
}

_VisualScriptEditor::~_VisualScriptEditor() {
    custom_nodes.clear();
}

void _VisualScriptEditor::add_custom_node(const String &p_name, const String &p_category, const Ref<Script> &p_script) {
    String node_name = "custom/" + p_category + "/" + p_name;
    custom_nodes.emplace(node_name, p_script.get_ref_ptr());
    VisualScriptLanguage::singleton->add_register_func(node_name, &_VisualScriptEditor::create_node_custom);
    emit_signal("custom_nodes_updated");
}

void _VisualScriptEditor::remove_custom_node(const String &p_name, const String &p_category) {
    String node_name = "custom/" + p_category + "/" + p_name;
    custom_nodes.erase(node_name);
    VisualScriptLanguage::singleton->remove_register_func(node_name);
    emit_signal("custom_nodes_updated");
}

void _VisualScriptEditor::_bind_methods() {
    MethodBinder::bind_method(D_METHOD("add_custom_node", {"name", "category", "script"}), &_VisualScriptEditor::add_custom_node);
    MethodBinder::bind_method(D_METHOD("remove_custom_node", {"name", "category"}), &_VisualScriptEditor::remove_custom_node);
    ADD_SIGNAL(MethodInfo("custom_nodes_updated"));
}

void VisualScriptEditor::validate() {
}
#endif