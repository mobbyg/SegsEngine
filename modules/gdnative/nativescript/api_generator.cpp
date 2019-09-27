/*************************************************************************/
/*  api_generator.cpp                                                    */
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

#include "api_generator.h"

#ifdef TOOLS_ENABLED

#include "core/class_db.h"
#include "core/engine.h"
#include "core/global_constants.h"
#include "core/os/file_access.h"
#include "core/property_info.h"
#include "core/method_info.h"
#include "core/method_bind_interface.h"
#include "core/pair.h"
#include "core/ustring.h"

#include "EASTL/sort.h"
// helper stuff

static Error save_file(const String &p_path, const List<String> &p_content) {

    FileAccessRef file = FileAccess::open(p_path, FileAccess::WRITE);

    ERR_FAIL_COND_V(!file, ERR_FILE_CANT_WRITE)

    for (const List<String>::Element *e = p_content.front(); e != nullptr; e = e->next()) {
        file->store_string(e->deref());
    }

    file->close();

    return OK;
}

// helper stuff end

struct MethodAPI {
    String method_name;
    String return_type;

    List<String> argument_types;
    List<String> argument_names;

    Map<int, Variant> default_arguments;

    int argument_count;
    bool has_varargs;
    bool is_editor;
    bool is_noscript;
    bool is_const;
    bool is_reverse;
    bool is_virtual;
    bool is_from_script;
};

struct PropertyAPI {
    String name;
    String getter;
    String setter;
    String type;
    int index;
};

struct ConstantAPI {
    String constant_name;
    int constant_value;
};

struct SignalAPI {
    String name;
    List<String> argument_types;
    List<String> argument_names;
    Map<int, Variant> default_arguments;
};

struct EnumAPI {
    String name;
    List<Pair<int, String> > values;
};

struct ClassAPI {
    String class_name;
    String super_class_name;

    ClassDB::APIType api_type;

    bool is_singleton;
    bool is_instanciable;
    // @Unclear
    bool is_reference;

    List<MethodAPI> methods;
    List<PropertyAPI> properties;
    List<ConstantAPI> constants;
    List<SignalAPI> signals_;
    List<EnumAPI> enums;
};

static String get_type_name(const PropertyInfo &info) {
    if (info.type == VariantType::INT && (info.usage & PROPERTY_USAGE_CLASS_IS_ENUM)) {
        return String("enum.") + StringUtils::replace(info.class_name,".", "::");
    }
    if (info.class_name != StringName()) {
        return info.class_name;
    }
    if (info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
        return info.hint_string;
    }
    if (info.type == VariantType::NIL && (info.usage & PROPERTY_USAGE_NIL_IS_VARIANT)) {
        return "Variant";
    }
    if (info.type == VariantType::NIL) {
        return "void";
    }
    return Variant::get_type_name(info.type);
}

/*
 * Some comparison helper functions we need
 */

struct MethodInfoComparator {
    bool operator()(const MethodInfo &p_a, const MethodInfo &p_b) const {

        return StringName::AlphCompare(p_a.name, p_b.name);
    }
};

struct PropertyInfoComparator {
    bool operator()(const PropertyInfo &p_a, const PropertyInfo &p_b) const {

        return StringName::AlphCompare(p_a.name, p_b.name);
    }
};

struct ConstantAPIComparator {
    NoCaseComparator compare;
    bool operator()(const ConstantAPI &p_a, const ConstantAPI &p_b) const {

        return compare(p_a.constant_name, p_b.constant_name);
    }
};

/*
 * Reads the entire Godot API to a list
 */
List<ClassAPI> generate_c_api_classes() {

    List<ClassAPI> api;

    Vector<StringName> classes;
    ClassDB::get_class_list(&classes);

    std::sort(classes.ptrw(),classes.ptrw()+classes.size(),StringName::AlphCompare);

    // Register global constants as a fake GlobalConstants singleton class
    {
        ClassAPI global_constants_api;
        global_constants_api.class_name = "GlobalConstants";
        global_constants_api.api_type = ClassDB::API_CORE;
        global_constants_api.is_singleton = true;
        global_constants_api.is_instanciable = false;
        const int constants_count = GlobalConstants::get_global_constant_count();
        for (int i = 0; i < constants_count; ++i) {
            ConstantAPI constant_api;
            constant_api.constant_name = GlobalConstants::get_global_constant_name(i);
            constant_api.constant_value = GlobalConstants::get_global_constant_value(i);
            global_constants_api.constants.push_back(constant_api);
        }
        global_constants_api.constants.sort_custom<ConstantAPIComparator>();
        api.push_back(global_constants_api);
    }

    for (int i=0,fin=classes.size(); i<fin; ++i) {
        StringName class_name = classes[i];

        ClassAPI class_api;
        class_api.api_type = ClassDB::get_api_type(classes[i]);
        class_api.class_name = class_name;
        class_api.super_class_name = ClassDB::get_parent_class(class_name);
        {
            String name = class_name;
            if (StringUtils::begins_with(name,"_")) {
                StringUtils::erase(name,0,1);
            }
            class_api.is_singleton = Engine::get_singleton()->has_singleton(name);
        }
        class_api.is_instanciable = !class_api.is_singleton && ClassDB::can_instance(class_name);

        {
            ListPOD<StringName> inheriters;
            ClassDB::get_inheriters_from_class("Reference", &inheriters);
            bool is_reference = inheriters.contains(class_name);
            // @Unclear
            class_api.is_reference = !class_api.is_singleton && is_reference;
        }

        // constants
        {
            ListPOD<String> constant;
            ClassDB::get_integer_constant_list(class_name, &constant, true);
            constant.sort(NoCaseComparator());
            for (const String &c : constant) {
                ConstantAPI constant_api;
                constant_api.constant_name = c;
                constant_api.constant_value = ClassDB::get_integer_constant(class_name, c);

                class_api.constants.push_back(constant_api);
            }
        }

        // signals
        {
            ListPOD<MethodInfo> signals_;
            ClassDB::get_signal_list(class_name, &signals_, true);
            signals_.sort(MethodInfoComparator());

            for (const MethodInfo &method_info : signals_) {
                SignalAPI signal;

                signal.name = method_info.name;

                for (const PropertyInfo &argument : method_info.arguments) {
                    String type;
                    String name = argument.name;

                    if (StringUtils::contains(argument.name,':')) {
                        type = StringUtils::get_slice(argument.name,":", 1);
                        name = StringUtils::get_slice(argument.name,":", 0);
                    } else {
                        type = get_type_name(argument);
                    }

                    signal.argument_names.push_back(name);
                    signal.argument_types.push_back(type);
                }

                const PODVector<Variant> &default_arguments(method_info.default_arguments);

                int default_start = signal.argument_names.size() - default_arguments.size();

                for (size_t j = 0; j < default_arguments.size(); j++) {
                    signal.default_arguments[default_start + j] = default_arguments[j];
                }

                class_api.signals_.push_back(signal);
            }
        }

        //properties
        {
            ListPOD<PropertyInfo> properties;
            ClassDB::get_property_list(class_name, &properties, true);
            properties.sort(PropertyInfoComparator());

            for (const PropertyInfo &p : properties) {
                PropertyAPI property_api;

                property_api.name = p.name;
                property_api.getter = ClassDB::get_property_getter(class_name, p.name);
                property_api.setter = ClassDB::get_property_setter(class_name, p.name);

                if( StringUtils::contains(p.name,":") ) {
                    property_api.type = StringUtils::get_slice(p.name,":", 1);
                    property_api.name = StringUtils::get_slice(p.name,":", 0);
                } else {
                    property_api.type = get_type_name(p);
                }

                property_api.index = ClassDB::get_property_index(class_name, p.name);

                if (!property_api.setter.empty() || !property_api.getter.empty()) {
                    class_api.properties.push_back(property_api);
                }
            }
        }

        //methods
        {
            PODVector<MethodInfo> methods;
            ClassDB::get_method_list(class_name, &methods, true);
            eastl::sort(methods.begin(),methods.end(),MethodInfoComparator());

            for (MethodInfo &method_info : methods) {
                MethodAPI method_api;
                MethodBind *method_bind = ClassDB::get_method(class_name, method_info.name);

                //method name
                method_api.method_name = method_info.name;
                //method return type
                if( StringUtils::contains(method_api.method_name,":") ) {
                    method_api.return_type = StringUtils::get_slice(method_api.method_name,":", 1);
                    method_api.method_name = StringUtils::get_slice(method_api.method_name,":", 0);
                } else {
                    method_api.return_type = get_type_name(method_info.return_val);
                }

                method_api.argument_count = method_info.arguments.size();
                method_api.has_varargs = method_bind && method_bind->is_vararg();

                // Method flags
                method_api.is_virtual = false;
                if (method_info.flags) {
                    const uint32_t flags = method_info.flags;
                    method_api.is_editor = flags & METHOD_FLAG_EDITOR;
                    method_api.is_noscript = flags & METHOD_FLAG_NOSCRIPT;
                    method_api.is_const = flags & METHOD_FLAG_CONST;
                    method_api.is_reverse = flags & METHOD_FLAG_REVERSE;
                    method_api.is_virtual = flags & METHOD_FLAG_VIRTUAL;
                    method_api.is_from_script = flags & METHOD_FLAG_FROM_SCRIPT;
                }

                method_api.is_virtual = method_api.is_virtual || method_api.method_name[0] == '_';

                // method argument name and type

                for (int i = 0; i < method_api.argument_count; i++) {
                    String arg_name;
                    String arg_type;
                    PropertyInfo arg_info = method_info.arguments[i];

                    arg_name = arg_info.name;

                    if (StringUtils::contains(arg_info.name,":") ) {
                        arg_type = StringUtils::get_slice(arg_info.name,":", 1);
                        arg_name = StringUtils::get_slice(arg_info.name,":", 0);
                    } else if (arg_info.hint == PROPERTY_HINT_RESOURCE_TYPE) {
                        arg_type = arg_info.hint_string;
                    } else if (arg_info.type == VariantType::NIL) {
                        arg_type = "Variant";
                    } else if (arg_info.type == VariantType::OBJECT) {
                        arg_type = arg_info.class_name;
                        if (arg_type.empty()) {
                            arg_type = Variant::get_type_name(arg_info.type);
                        }
                    } else {
                        arg_type = Variant::get_type_name(arg_info.type);
                    }

                    method_api.argument_names.push_back(arg_name);
                    method_api.argument_types.push_back(arg_type);

                    if (method_bind && method_bind->has_default_argument(i)) {
                        method_api.default_arguments[i] = method_bind->get_default_argument(i);
                    }
                }

                class_api.methods.push_back(method_api);
            }
        }

        // enums
        {
            List<EnumAPI> enums;
            ListPOD<StringName> enum_names;
            ClassDB::get_enum_list(class_name, &enum_names, true);
            for(const StringName & E : enum_names) {
                ListPOD<StringName> value_names;
                EnumAPI enum_api;
                enum_api.name = E;
                ClassDB::get_enum_constants(class_name, E, &value_names, true);
                for(const StringName & val_e : value_names) {
                    int int_val = ClassDB::get_integer_constant(class_name, val_e, nullptr);
                    enum_api.values.push_back(Pair<int, String>(int_val, val_e));
                }
                enum_api.values.sort_custom<PairSort<int, String> >();
                class_api.enums.push_back(enum_api);
            }
        }

        api.push_back(class_api);
    }

    return api;
}

/*
 * Generates the JSON source from the API in p_api
 */
static List<String> generate_c_api_json(const List<ClassAPI> &p_api) {

    // I'm sorry for the \t mess

    List<String> source;

    source.push_back("[\n");

    for (const List<ClassAPI>::Element *c = p_api.front(); c != nullptr; c = c->next()) {
        ClassAPI api = c->deref();

        source.push_back("\t{\n");

        source.push_back("\t\t\"name\": \"" + api.class_name + "\",\n");
        source.push_back("\t\t\"base_class\": \"" + api.super_class_name + "\",\n");
        source.push_back(String("\t\t\"api_type\": \"") + (api.api_type == ClassDB::API_CORE ? "core" : (api.api_type == ClassDB::API_EDITOR ? "tools" : "none")) + "\",\n");
        source.push_back(String("\t\t\"singleton\": ") + (api.is_singleton ? "true" : "false") + ",\n");
        source.push_back(String("\t\t\"instanciable\": ") + (api.is_instanciable ? "true" : "false") + ",\n");
        source.push_back(String("\t\t\"is_reference\": ") + (api.is_reference ? "true" : "false") + ",\n");
        // @Unclear

        source.push_back("\t\t\"constants\": {\n");
        for (List<ConstantAPI>::Element *e = api.constants.front(); e; e = e->next()) {
            source.push_back("\t\t\t\"" + e->deref().constant_name + "\": " + StringUtils::num_int64(e->deref().constant_value) + (e->next() ? "," : "") + "\n");
        }
        source.push_back("\t\t},\n");

        source.push_back("\t\t\"properties\": [\n");
        for (List<PropertyAPI>::Element *e = api.properties.front(); e; e = e->next()) {
            source.push_back("\t\t\t{\n");
            source.push_back("\t\t\t\t\"name\": \"" + e->deref().name + "\",\n");
            source.push_back("\t\t\t\t\"type\": \"" + e->deref().type + "\",\n");
            source.push_back("\t\t\t\t\"getter\": \"" + e->deref().getter + "\",\n");
            source.push_back("\t\t\t\t\"setter\": \"" + e->deref().setter + "\",\n");
            source.push_back(String("\t\t\t\t\"index\": ") + itos(e->deref().index) + "\n");
            source.push_back(String("\t\t\t}") + (e->next() ? "," : "") + "\n");
        }
        source.push_back("\t\t],\n");

        source.push_back("\t\t\"signals\": [\n");
        for (List<SignalAPI>::Element *e = api.signals_.front(); e; e = e->next()) {
            source.push_back("\t\t\t{\n");
            source.push_back("\t\t\t\t\"name\": \"" + e->deref().name + "\",\n");
            source.push_back("\t\t\t\t\"arguments\": [\n");
            for (int i = 0; i < e->deref().argument_names.size(); i++) {
                source.push_back("\t\t\t\t\t{\n");
                source.push_back("\t\t\t\t\t\t\"name\": \"" + e->deref().argument_names[i] + "\",\n");
                source.push_back("\t\t\t\t\t\t\"type\": \"" + e->deref().argument_types[i] + "\",\n");
                source.push_back("\t\t\t\t\t\t\"default_value\": \"" + (e->deref().default_arguments.contains(i) ? (String)e->deref().default_arguments[i] : "") + "\"\n");
                source.push_back(String("\t\t\t\t\t}") + ((i < e->deref().argument_names.size() - 1) ? "," : "") + "\n");
            }
            source.push_back("\t\t\t\t]\n");
            source.push_back(String("\t\t\t}") + (e->next() ? "," : "") + "\n");
        }
        source.push_back("\t\t],\n");

        source.push_back("\t\t\"methods\": [\n");
        for (List<MethodAPI>::Element *e = api.methods.front(); e; e = e->next()) {
            source.push_back("\t\t\t{\n");
            source.push_back("\t\t\t\t\"name\": \"" + e->deref().method_name + "\",\n");
            source.push_back("\t\t\t\t\"return_type\": \"" + e->deref().return_type + "\",\n");
            source.push_back(String("\t\t\t\t\"is_editor\": ") + (e->deref().is_editor ? "true" : "false") + ",\n");
            source.push_back(String("\t\t\t\t\"is_noscript\": ") + (e->deref().is_noscript ? "true" : "false") + ",\n");
            source.push_back(String("\t\t\t\t\"is_const\": ") + (e->deref().is_const ? "true" : "false") + ",\n");
            source.push_back(String("\t\t\t\t\"is_reverse\": ") + (e->deref().is_reverse ? "true" : "false") + ",\n");
            source.push_back(String("\t\t\t\t\"is_virtual\": ") + (e->deref().is_virtual ? "true" : "false") + ",\n");
            source.push_back(String("\t\t\t\t\"has_varargs\": ") + (e->deref().has_varargs ? "true" : "false") + ",\n");
            source.push_back(String("\t\t\t\t\"is_from_script\": ") + (e->deref().is_from_script ? "true" : "false") + ",\n");
            source.push_back("\t\t\t\t\"arguments\": [\n");
            for (int i = 0; i < e->deref().argument_names.size(); i++) {
                source.push_back("\t\t\t\t\t{\n");
                source.push_back("\t\t\t\t\t\t\"name\": \"" + e->deref().argument_names[i] + "\",\n");
                source.push_back("\t\t\t\t\t\t\"type\": \"" + e->deref().argument_types[i] + "\",\n");
                source.push_back(String("\t\t\t\t\t\t\"has_default_value\": ") + (e->deref().default_arguments.contains(i) ? "true" : "false") + ",\n");
                source.push_back("\t\t\t\t\t\t\"default_value\": \"" + (e->deref().default_arguments.contains(i) ? (String)e->deref().default_arguments[i] : "") + "\"\n");
                source.push_back(String("\t\t\t\t\t}") + ((i < e->deref().argument_names.size() - 1) ? "," : "") + "\n");
            }
            source.push_back("\t\t\t\t]\n");
            source.push_back(String("\t\t\t}") + (e->next() ? "," : "") + "\n");
        }
        source.push_back("\t\t],\n");

        source.push_back("\t\t\"enums\": [\n");
        for (List<EnumAPI>::Element *e = api.enums.front(); e; e = e->next()) {
            source.push_back("\t\t\t{\n");
            source.push_back("\t\t\t\t\"name\": \"" + e->deref().name + "\",\n");
            source.push_back("\t\t\t\t\"values\": {\n");
            for (List<Pair<int, String> >::Element *val_e = e->deref().values.front(); val_e; val_e = val_e->next()) {
                source.push_back("\t\t\t\t\t\"" + val_e->deref().second + "\": " + itos(val_e->deref().first));
                source.push_back(String((val_e->next() ? "," : "")) + "\n");
            }
            source.push_back("\t\t\t\t}\n");
            source.push_back(String("\t\t\t}") + (e->next() ? "," : "") + "\n");
        }
        source.push_back("\t\t]\n");

        source.push_back(String("\t}") + (c->next() ? "," : "") + "\n");
    }
    source.push_back("]");

    return source;
}

#endif

/*
 * Saves the whole Godot API to a JSON file located at
 *  p_path
 */
Error generate_c_api(const String &p_path) {

#ifndef TOOLS_ENABLED
    return ERR_BUG;
#else

    List<ClassAPI> api = generate_c_api_classes();

    List<String> json_source = generate_c_api_json(api);

    return save_file(p_path, json_source);
#endif
}