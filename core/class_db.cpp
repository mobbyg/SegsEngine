/*************************************************************************/
/*  class_db.cpp                                                         */
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

#include "class_db.h"

#include "core/engine.h"
#include "core/error_macros.h"
#include "core/hashfuncs.h"
#include "core/method_bind_interface.h"
#include "core/method_info.h"
#include "core/object.h"
#include "core/version.h"

#include "EASTL/sort.h"

#define OBJTYPE_RLOCK RWLockRead _rw_lockr_(lock);
#define OBJTYPE_WLOCK RWLockWrite _rw_lockw_(lock);

#ifdef DEBUG_METHODS_ENABLED

MethodDefinition D_METHOD(const char *p_name) {

    MethodDefinition md;
    md.name = StaticCString(p_name, true);
    return md;
}

MethodDefinition D_METHOD(const char *p_name, std::initializer_list<StringName> names) {

    MethodDefinition md;
    md.name = StaticCString(p_name, true);
    md.args = names;
    return md;
}

#endif

ClassDB::APIType ClassDB::current_api = API_CORE;

void ClassDB::set_current_api(APIType p_api) {

    current_api = p_api;
}

ClassDB::APIType ClassDB::get_current_api() {

    return current_api;
}

HashMap<StringName, ClassDB::ClassInfo> ClassDB::classes;
HashMap<StringName, StringName> ClassDB::resource_base_extensions;
HashMap<StringName, StringName> ClassDB::compat_classes;

ClassDB::ClassInfo::ClassInfo() {

    api = API_NONE;
    creation_func = nullptr;
    inherits_ptr = nullptr;
    disabled = false;
    exposed = false;
}

ClassDB::ClassInfo::~ClassInfo() {}

bool ClassDB::is_parent_class(const StringName &p_class, const StringName &p_inherits) {

    RWLockRead _rw_lockr_(lock);

    StringName inherits = p_class;

    while (inherits.operator String().length()) {

        if (inherits == p_inherits) return true;
        inherits = get_parent_class(inherits);
    }

    return false;
}
void ClassDB::get_class_list(Vector<StringName> *p_classes) {

    RWLockRead _rw_lockr_(lock);

    const StringName *k = nullptr;

    while ((k = classes.next(k))) {

        p_classes->push_back(*k);
    }

    p_classes->sort();
}

void ClassDB::get_inheriters_from_class(const StringName &p_class, ListPOD<StringName> *p_classes) {

    RWLockRead _rw_lockr_(lock);

    const StringName *k = nullptr;

    while ((k = classes.next(k))) {

        if (*k != p_class && is_parent_class(*k, p_class)) p_classes->push_back(*k);
    }
}

void ClassDB::get_direct_inheriters_from_class(const StringName &p_class, ListPOD<StringName> *p_classes) {

    RWLockRead _rw_lockr_(lock);

    const StringName *k = nullptr;

    while ((k = classes.next(k))) {

        if (*k != p_class && get_parent_class(*k) == p_class) p_classes->push_back(*k);
    }
}

StringName ClassDB::get_parent_class_nocheck(const StringName &p_class) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *ti = classes.getptr(p_class);
    if (!ti) return StringName();
    return ti->inherits;
}

StringName ClassDB::get_parent_class(const StringName &p_class) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *ti = classes.getptr(p_class);
    ERR_FAIL_COND_V(!ti, StringName())
    return ti->inherits;
}

ClassDB::APIType ClassDB::get_api_type(const StringName &p_class) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *ti = classes.getptr(p_class);

    ERR_FAIL_COND_V(!ti, API_NONE)
    return ti->api;
}

uint64_t ClassDB::get_api_hash(APIType p_api) {

    RWLockRead _rw_lockr_(lock);
#ifdef DEBUG_METHODS_ENABLED

    uint64_t hash = hash_djb2_one_64(Hasher<const char *>()(VERSION_FULL_CONFIG));
    // TODO: bunch of copiers are made here, the containers should just hold pointers/const references to objects ?
    PODVector<StringName> names;

    const StringName *k = nullptr;

    while ((k = classes.next(k))) {

        names.push_back(*k);
    }
    eastl::stable_sort(names.begin(), names.end(), StringName::AlphCompare);
    // must be alphabetically sorted for hash to compute
    PODVector<StringName> snames;

    for (const StringName &n : names) {

        ClassInfo *t = classes.getptr(n);
        ERR_FAIL_COND_V(!t, 0)
        if (t->api != p_api || !t->exposed) continue;
        hash = hash_djb2_one_64(t->name.hash(), hash);
        hash = hash_djb2_one_64(t->inherits.hash(), hash);

        { // methods

            snames.clear();
            k = nullptr;

            while ((k = t->method_map.next(k))) {

                snames.push_back(*k);
            }

            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (const StringName &sn : snames) {

                MethodBind *mb = t->method_map[sn];
                hash = hash_djb2_one_64(mb->get_name().hash(), hash);
                hash = hash_djb2_one_64(mb->get_argument_count(), hash);
                hash = hash_djb2_one_64(uint64_t(mb->get_argument_type(-1)), hash); // return

                for (int i = 0; i < mb->get_argument_count(); i++) {
                    const PropertyInfo info = mb->get_argument_info(i);
                    hash = hash_djb2_one_64(uint64_t(info.type), hash);
                    hash = hash_djb2_one_64(StringUtils::hash(info.name), hash);
                    hash = hash_djb2_one_64(info.hint, hash);
                    hash = hash_djb2_one_64(StringUtils::hash(info.hint_string), hash);
                }

                hash = hash_djb2_one_64(mb->get_default_argument_count(), hash);

                for (int i = 0; i < mb->get_default_argument_count(); i++) {
                    // hash should not change, i hope for tis
                    Variant da = mb->get_default_argument(i);
                    hash = hash_djb2_one_64(da.hash(), hash);
                }

                hash = hash_djb2_one_64(mb->get_hint_flags(), hash);
            }
        }

        { // constants

            snames.clear();

            k = nullptr;

            while ((k = t->constant_map.next(k))) {

                snames.push_back(*k);
            }
            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (StringName &sn : snames) {

                hash = hash_djb2_one_64(sn.hash(), hash);
                hash = hash_djb2_one_64(t->constant_map[sn], hash);
            }
        }

        { // signals

            snames.clear();
            k = nullptr;

            while ((k = t->signal_map.next(k))) {

                snames.push_back(*k);
            }

            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (StringName &sn : snames) {

                MethodInfo &mi = t->signal_map[sn];
                hash = hash_djb2_one_64(sn.hash(), hash);
                for (int i = 0; i < mi.arguments.size(); i++) {
                    hash = hash_djb2_one_64(uint64_t(mi.arguments[i].type), hash);
                }
            }
        }

        { // properties

            snames.clear();

            k = nullptr;

            while ((k = t->property_setget.next(k))) {

                snames.push_back(*k);
            }

            eastl::stable_sort(snames.begin(), snames.end(), StringName::AlphCompare);

            for (StringName &sn : snames) {

                PropertySetGet *psg = t->property_setget.getptr(sn);
                ERR_FAIL_COND_V(!psg, 0)

                hash = hash_djb2_one_64(sn.hash(), hash);
                hash = hash_djb2_one_64(psg->setter.hash(), hash);
                hash = hash_djb2_one_64(psg->getter.hash(), hash);
            }
        }

        // property list
        for (const PropertyInfo &pi : t->property_list) {
            hash = hash_djb2_one_64(StringUtils::hash(pi.name), hash);
            hash = hash_djb2_one_64(uint64_t(pi.type), hash);
            hash = hash_djb2_one_64(pi.hint, hash);
            hash = hash_djb2_one_64(StringUtils::hash(pi.hint_string), hash);
            hash = hash_djb2_one_64(pi.usage, hash);
        }
    }

    return hash;
#else
    return 0;
#endif
}

bool ClassDB::class_exists(const StringName &p_class) {

    OBJTYPE_RLOCK
    return classes.contains(p_class);
}

void ClassDB::add_compatibility_class(const StringName &p_class, const StringName &p_fallback) {

    OBJTYPE_WLOCK
    compat_classes[p_class] = p_fallback;
}

Object *ClassDB::instance(const StringName &p_class) {

    ClassInfo *ti;
    {
        RWLockRead _rw_lockr_(lock);
        ti = classes.getptr(p_class);
        if (!ti || ti->disabled || !ti->creation_func) {
            if (compat_classes.contains(p_class)) {
                ti = classes.getptr(compat_classes[p_class]);
            }
        }
        ERR_FAIL_COND_V(!ti, nullptr)
        ERR_FAIL_COND_V(ti->disabled, nullptr)
        ERR_FAIL_COND_V(!ti->creation_func, nullptr)
    }
#ifdef TOOLS_ENABLED
    if (ti->api == API_EDITOR && !Engine::get_singleton()->is_editor_hint()) {
        ERR_PRINTS("Class '" + String(p_class) + "' can only be instantiated by editor.")
        return nullptr;
    }
#endif
    return ti->creation_func();
}
bool ClassDB::can_instance(const StringName &p_class) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *ti = classes.getptr(p_class);
    ERR_FAIL_COND_V(!ti, false)
#ifdef TOOLS_ENABLED
    if (ti->api == API_EDITOR && !Engine::get_singleton()->is_editor_hint()) {
        return false;
    }
#endif
    return (!ti->disabled && ti->creation_func != nullptr);
}

void ClassDB::_add_class2(const StringName &p_class, const StringName &p_inherits) {

    OBJTYPE_WLOCK;

    const StringName &name = p_class;

    ERR_FAIL_COND(classes.contains(name))

    classes[name] = ClassInfo();
    ClassInfo &ti = classes[name];
    ti.name = name;
    ti.inherits = p_inherits;
    ti.api = current_api;

    if (ti.inherits) {

        ERR_FAIL_COND(!classes.contains(ti.inherits)) // it MUST be registered.
        ti.inherits_ptr = &classes[ti.inherits];

    } else {
        ti.inherits_ptr = nullptr;
    }
}

void ClassDB::get_method_list(
        const StringName& p_class, PODVector<MethodInfo> *p_methods, bool p_no_inheritance, bool p_exclude_from_properties) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);

    while (type) {

        if (type->disabled) {

            if (p_no_inheritance) break;

            type = type->inherits_ptr;
            continue;
        }

#ifdef DEBUG_METHODS_ENABLED

        for (const MethodInfo &mi : type->virtual_methods) {

            p_methods->push_back(mi);
        }

        for (const StringName &nm : type->method_order) {

            MethodBind *method = type->method_map.get(nm);
            MethodInfo minfo;
            minfo.name = nm;
            minfo.id = method->get_method_id();

            if (p_exclude_from_properties && type->methods_in_properties.contains(minfo.name)) continue;

            for (int i = 0; i < method->get_argument_count(); i++) {

                // VariantType t=method->get_argument_type(i);

                minfo.arguments.push_back(method->get_argument_info(i));
            }

            minfo.return_val = method->get_return_info();
            minfo.flags = method->get_hint_flags();

            for (int i = 0; i < method->get_argument_count(); i++) {
                if (method->has_default_argument(i))
                    minfo.default_arguments.push_back(method->get_default_argument(i));
            }

            p_methods->push_back(minfo);
        }

#else

        const StringName *K = NULL;

        while ((K = type->method_map.next(K))) {

            MethodBind *m = type->method_map[*K];
            MethodInfo mi;
            mi.name = m->get_name();
            p_methods->push_back(mi);
        }

#endif

        if (p_no_inheritance) break;

        type = type->inherits_ptr;
    }
}

MethodBind *ClassDB::get_method(StringName p_class, StringName p_name) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);

    while (type) {

        MethodBind **method = type->method_map.getptr(p_name);
        if (method && *method) return *method;
        type = type->inherits_ptr;
    }
    return nullptr;
}

void ClassDB::bind_integer_constant(
        const StringName &p_class, const StringName &p_enum, const StringName &p_name, int p_constant) {

    OBJTYPE_WLOCK;

    ClassInfo *type = classes.getptr(p_class);

    ERR_FAIL_COND(!type)

    if (type->constant_map.contains(p_name)) {

        ERR_FAIL();
    }

    type->constant_map[p_name] = p_constant;

    String enum_name = p_enum;
    if (!enum_name.empty()) {
        if (StringUtils::contains(enum_name, '.') ) {
            enum_name = StringUtils::get_slice(enum_name, '.', 1);
        }

        ListPOD<StringName> *constants_list = type->enum_map.getptr(enum_name);

        if (constants_list) {
            constants_list->push_back(p_name);
        } else {
            ListPOD<StringName> new_list {p_name};

            type->enum_map[enum_name] = new_list;
        }
    }

#ifdef DEBUG_METHODS_ENABLED
    type->constant_order.push_back(p_name);
#endif
}

void ClassDB::get_integer_constant_list(const StringName &p_class, ListPOD<String> *p_constants, bool p_no_inheritance) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);

    while (type) {

#ifdef DEBUG_METHODS_ENABLED
        for (const StringName &name : type->constant_order) {
            p_constants->push_back(name);
        }
#else
        const StringName *K = NULL;

        while ((K = type->constant_map.next(K))) {
            p_constants->push_back(*K);
        }

#endif
        if (p_no_inheritance) break;

        type = type->inherits_ptr;
    }
}

int ClassDB::get_integer_constant(const StringName &p_class, const StringName &p_name, bool *p_success) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);

    while (type) {

        int *constant = type->constant_map.getptr(p_name);
        if (constant) {

            if (p_success) *p_success = true;
            return *constant;
        }

        type = type->inherits_ptr;
    }

    if (p_success) *p_success = false;

    return 0;
}

StringName ClassDB::get_integer_constant_enum(
        const StringName &p_class, const StringName &p_name, bool p_no_inheritance) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);

    while (type) {

        const StringName *k = nullptr;
        while ((k = type->enum_map.next(k))) {

            ListPOD<StringName> &constants_list = type->enum_map.get(*k);
            if (constants_list.contains(p_name)) return *k;
        }

        if (p_no_inheritance) break;

        type = type->inherits_ptr;
    }

    return StringName();
}

void ClassDB::get_enum_list(const StringName &p_class, ListPOD<StringName> *p_enums, bool p_no_inheritance) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);

    while (type) {

        const StringName *k = nullptr;
        while ((k = type->enum_map.next(k))) {
            p_enums->push_back(*k);
        }

        if (p_no_inheritance) break;

        type = type->inherits_ptr;
    }
}

void ClassDB::get_enum_constants(
        const StringName &p_class, const StringName &p_enum, ListPOD<StringName> *p_constants, bool p_no_inheritance) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);

    while (type) {

        const ListPOD<StringName> *constants = type->enum_map.getptr(p_enum);

        if (constants) {
            for (const StringName &name : *constants) {
                p_constants->push_back(name);
            }
        }

        if (p_no_inheritance) break;

        type = type->inherits_ptr;
    }
}

void ClassDB::add_signal(StringName p_class, const MethodInfo &p_signal) {

    OBJTYPE_WLOCK;

    ClassInfo *type = classes.getptr(p_class);
    ERR_FAIL_COND(!type)

    StringName sname = p_signal.name;

#ifdef DEBUG_METHODS_ENABLED
    ClassInfo *check = type;
    while (check) {
        ERR_FAIL_COND_MSG(check->signal_map.contains(sname),
                "Type " + String(p_class) + " already has signal: " + String(sname) + ".");
        check = check->inherits_ptr;
    }
#endif

    type->signal_map[sname] = p_signal;
}

void ClassDB::get_signal_list(StringName p_class, ListPOD<MethodInfo> *p_signals, bool p_no_inheritance) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);
    ERR_FAIL_COND(!type)

    ClassInfo *check = type;

    while (check) {

        const StringName *S = nullptr;
        while ((S = check->signal_map.next(S))) {

            p_signals->push_back(check->signal_map[*S]);
        }

        if (p_no_inheritance) return;

        check = check->inherits_ptr;
    }
}

bool ClassDB::has_signal(StringName p_class, StringName p_signal) {

    RWLockRead _rw_lockr_(lock);
    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        if (check->signal_map.contains(p_signal)) return true;
        check = check->inherits_ptr;
    }

    return false;
}

bool ClassDB::get_signal(StringName p_class, StringName p_signal, MethodInfo *r_signal) {

    RWLockRead _rw_lockr_(lock);
    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        if (check->signal_map.contains(p_signal)) {
            if (r_signal) {
                *r_signal = check->signal_map[p_signal];
            }
            return true;
        }
        check = check->inherits_ptr;
    }

    return false;
}

void ClassDB::add_property_group(StringName p_class, const char *p_name, const char *p_prefix) {

    OBJTYPE_WLOCK
    ClassInfo *type = classes.getptr(p_class);
    ERR_FAIL_COND(!type)

    type->property_list.push_back(
            PropertyInfo(VariantType::NIL, p_name, PROPERTY_HINT_NONE, p_prefix, PROPERTY_USAGE_GROUP));
}

void ClassDB::add_property(StringName p_class, const PropertyInfo &p_pinfo, const StringName &p_setter,
        const StringName &p_getter, int p_index) {

    lock->read_lock();
    ClassInfo *type = classes.getptr(p_class);
    lock->read_unlock();

    ERR_FAIL_COND(!type)

    MethodBind *mb_set = nullptr;
    if (p_setter) {
        mb_set = get_method(p_class, p_setter);
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_MSG(
                !mb_set, "Invalid setter: " + p_class + "::" + p_setter + " for property: " + p_pinfo.name + ".");

        int exp_args = 1 + (p_index >= 0 ? 1 : 0);
        ERR_FAIL_COND_MSG(mb_set->get_argument_count() != exp_args,
                "Invalid function for setter: " + p_class + "::" + p_setter + " for property: " + p_pinfo.name + ".");
#endif
    }

    MethodBind *mb_get = nullptr;
    if (p_getter) {

        mb_get = get_method(p_class, p_getter);
#ifdef DEBUG_METHODS_ENABLED

        ERR_FAIL_COND_MSG(
                !mb_get, "Invalid getter: " + p_class + "::" + p_getter + " for property: " + p_pinfo.name + ".");

        int exp_args = 0 + (p_index >= 0 ? 1 : 0);
        ERR_FAIL_COND_MSG(mb_get->get_argument_count() != exp_args,
                "Invalid function for getter: " + p_class + "::" + p_getter + " for property: " + p_pinfo.name + ".");
#endif
    }

#ifdef DEBUG_METHODS_ENABLED
    ERR_FAIL_COND_MSG(type->property_setget.contains(p_pinfo.name),
            "Object " + p_class + " already has property: " + p_pinfo.name + ".");
#endif

    OBJTYPE_WLOCK

    type->property_list.push_back(p_pinfo);
#ifdef DEBUG_METHODS_ENABLED
    if (mb_get) {
        type->methods_in_properties.insert(p_getter);
    }
    if (mb_set) {
        type->methods_in_properties.insert(p_setter);
    }
#endif
    PropertySetGet psg;
    psg.setter = p_setter;
    psg.getter = p_getter;
    psg._setptr = mb_set;
    psg._getptr = mb_get;
    psg.index = p_index;
    psg.type = p_pinfo.type;

    type->property_setget[p_pinfo.name] = psg;
}

void ClassDB::set_property_default_value(StringName p_class, const StringName &p_name, const Variant &p_default) {
    if (!default_values.contains(p_class)) {
        default_values[p_class] = HashMap<StringName, Variant>();
    }
    default_values[p_class][p_name] = p_default;
}

void ClassDB::get_property_list(
        StringName p_class, ListPOD<PropertyInfo> *p_list, bool p_no_inheritance, const Object *p_validator) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {

        for (const PropertyInfo &pi : check->property_list) {

            if (p_validator) {
                PropertyInfo pimod = pi;
                p_validator->_validate_property(pimod);
                p_list->push_back(pimod);
            } else {
                p_list->push_back(pi);
            }
        }

        if (p_no_inheritance) return;
        check = check->inherits_ptr;
    }
}
bool ClassDB::set_property(Object *p_object, const StringName &p_property, const Variant &p_value, bool *r_valid) {

    ClassInfo *type = classes.getptr(p_object->get_class_name());
    ClassInfo *check = type;
    while (check) {
        const PropertySetGet *psg = check->property_setget.getptr(p_property);
        if (psg) {

            if (!psg->setter) {
                if (r_valid) *r_valid = false;
                return true; // return true but do nothing
            }

            Variant::CallError ce;

            if (psg->index >= 0) {
                Variant index = psg->index;
                const Variant *arg[2] = { &index, &p_value };
                // p_object->call(psg->setter,arg,2,ce);
                if (psg->_setptr) {
                    psg->_setptr->call(p_object, arg, 2, ce);
                } else {
                    p_object->call(psg->setter, arg, 2, ce);
                }

            } else {
                const Variant *arg[1] = { &p_value };
                if (psg->_setptr) {
                    psg->_setptr->call(p_object, arg, 1, ce);
                } else {
                    p_object->call(psg->setter, arg, 1, ce);
                }
            }

            if (r_valid) *r_valid = ce.error == Variant::CallError::CALL_OK;

            return true;
        }

        check = check->inherits_ptr;
    }

    return false;
}
bool ClassDB::get_property(Object *p_object, const StringName &p_property, Variant &r_value) {

    ClassInfo *type = classes.getptr(p_object->get_class_name());
    ClassInfo *check = type;
    while (check) {
        const PropertySetGet *psg = check->property_setget.getptr(p_property);
        if (psg) {
            if (!psg->getter) return true; // return true but do nothing

            if (psg->index >= 0) {
                Variant index = psg->index;
                const Variant *arg[1] = { &index };
                Variant::CallError ce;
                r_value = p_object->call(psg->getter, arg, 1, ce);

            } else {

                Variant::CallError ce;
                if (psg->_getptr) {

                    r_value = psg->_getptr->call(p_object, nullptr, 0, ce);
                } else {
                    r_value = p_object->call(psg->getter, nullptr, 0, ce);
                }
            }
            return true;
        }

        const int *c = check->constant_map.getptr(p_property);
        if (c) {

            r_value = *c;
            return true;
        }

        check = check->inherits_ptr;
    }

    return false;
}

int ClassDB::get_property_index(const StringName &p_class, const StringName &p_property, bool *r_is_valid) {

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        const PropertySetGet *psg = check->property_setget.getptr(p_property);
        if (psg) {

            if (r_is_valid) *r_is_valid = true;

            return psg->index;
        }

        check = check->inherits_ptr;
    }
    if (r_is_valid) *r_is_valid = false;

    return -1;
}

VariantType ClassDB::get_property_type(const StringName &p_class, const StringName &p_property, bool *r_is_valid) {

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        const PropertySetGet *psg = check->property_setget.getptr(p_property);
        if (psg) {

            if (r_is_valid) *r_is_valid = true;

            return psg->type;
        }

        check = check->inherits_ptr;
    }
    if (r_is_valid) *r_is_valid = false;

    return VariantType::NIL;
}

StringName ClassDB::get_property_setter(StringName p_class, const StringName &p_property) {

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        const PropertySetGet *psg = check->property_setget.getptr(p_property);
        if (psg) {

            return psg->setter;
        }

        check = check->inherits_ptr;
    }

    return StringName();
}

StringName ClassDB::get_property_getter(StringName p_class, const StringName &p_property) {

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        const PropertySetGet *psg = check->property_setget.getptr(p_property);
        if (psg) {

            return psg->getter;
        }

        check = check->inherits_ptr;
    }

    return StringName();
}

bool ClassDB::has_property(const StringName &p_class, const StringName &p_property, bool p_no_inheritance) {

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        if (check->property_setget.contains(p_property)) return true;

        if (p_no_inheritance) break;
        check = check->inherits_ptr;
    }

    return false;
}

void ClassDB::set_method_flags(StringName p_class, StringName p_method, int p_flags) {

    OBJTYPE_WLOCK;
    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    ERR_FAIL_COND(!check)
    ERR_FAIL_COND(!check->method_map.contains(p_method))
    check->method_map[p_method]->set_hint_flags(p_flags);
}

bool ClassDB::has_method(StringName p_class, StringName p_method, bool p_no_inheritance) {

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {
        if (check->method_map.contains(p_method)) return true;
        if (p_no_inheritance) return false;
        check = check->inherits_ptr;
    }

    return false;
}

#ifdef DEBUG_METHODS_ENABLED
MethodBind *ClassDB::bind_methodfi(uint32_t p_flags, MethodBind *p_bind, const MethodDefinition &method_name,
        std::initializer_list<Variant> def_vals) {
    StringName mdname = method_name.name;
#else
MethodBind *ClassDB::bind_methodfi(
        uint32_t p_flags, MethodBind *p_bind, const char *method_name, std::initializer_list<Variant> p_defcount) {
    StringName mdname = StaticCString(method_name, true);
#endif

    OBJTYPE_WLOCK;
    ERR_FAIL_COND_V(!p_bind, nullptr)
    p_bind->set_name(mdname);

    const char *instance_type = p_bind->get_instance_class();

#ifdef DEBUG_ENABLED

    ERR_FAIL_COND_V_MSG(has_method(StringName(instance_type), mdname), nullptr,
            "Class " + String(instance_type) + " already has a method " + String(mdname) + ".")
#endif

    ClassInfo *type = classes.getptr(StringName(instance_type));
    if (!type) {
        memdelete(p_bind);
        ERR_FAIL_V_MSG(nullptr, "Couldn't bind method '" + mdname + "' for instance: " + instance_type + ".")
    }

    if (type->method_map.contains(mdname)) {
        memdelete(p_bind);
        // overloading not supported
        ERR_FAIL_V_MSG(nullptr, String("Method already bound: ") + instance_type + "::" + mdname + ".")
    }

#ifdef DEBUG_METHODS_ENABLED

    if (method_name.args.size() > p_bind->get_argument_count()) {
        memdelete(p_bind);
        ERR_FAIL_V_MSG(nullptr, String("Method definition provides more arguments than the method actually has: ") +
                                        instance_type + "::" + mdname + ".");
    }

    p_bind->set_argument_names(method_name.args);

    type->method_order.push_back(mdname);
#endif

    type->method_map[mdname] = p_bind;

    Vector<Variant> defvals;

    defvals.resize(def_vals.size());
    for (int i = 0; i < def_vals.size(); i++) {

        defvals.write[i] = def_vals.begin()[def_vals.size() - i - 1];
    }

    p_bind->set_default_arguments(defvals);
    p_bind->set_hint_flags(p_flags);
    return p_bind;
}

void ClassDB::add_virtual_method(const StringName &p_class, const MethodInfo &p_method, bool p_virtual) {
    ERR_FAIL_COND(!classes.contains(p_class))

    OBJTYPE_WLOCK

#ifdef DEBUG_METHODS_ENABLED
    MethodInfo mi = p_method;
    if (p_virtual) mi.flags |= METHOD_FLAG_VIRTUAL;
    classes[p_class].virtual_methods.push_back(mi);

#endif
}

void ClassDB::get_virtual_methods(const StringName &p_class, PODVector<MethodInfo> *p_methods, bool p_no_inheritance) {

    ERR_FAIL_COND(!classes.contains(p_class))

#ifdef DEBUG_METHODS_ENABLED

    ClassInfo *type = classes.getptr(p_class);
    ClassInfo *check = type;
    while (check) {

        for (const MethodInfo &mi : check->virtual_methods) {
            p_methods->push_back(mi);
        }

        if (p_no_inheritance) return;
        check = check->inherits_ptr;
    }

#endif
}

void ClassDB::set_class_enabled(StringName p_class, bool p_enable) {

    OBJTYPE_WLOCK;

    ERR_FAIL_COND(!classes.contains(p_class))
    classes[p_class].disabled = !p_enable;
}

bool ClassDB::is_class_enabled(StringName p_class) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *ti = classes.getptr(p_class);
    if (!ti || !ti->creation_func) {
        if (compat_classes.contains(p_class)) {
            ti = classes.getptr(compat_classes[p_class]);
        }
    }

    ERR_FAIL_COND_V(!ti, false)
    return !ti->disabled;
}

bool ClassDB::is_class_exposed(StringName p_class) {

    RWLockRead _rw_lockr_(lock);

    ClassInfo *ti = classes.getptr(p_class);
    ERR_FAIL_COND_V(!ti, false)
    return ti->exposed;
}

StringName ClassDB::get_category(const StringName &p_node) {

    ERR_FAIL_COND_V(!classes.contains(p_node), StringName())
#ifdef DEBUG_ENABLED
    return classes[p_node].category;
#else
    return StringName();
#endif
}

void ClassDB::add_resource_base_extension(const StringName &p_extension, const StringName &p_class) {

    if (resource_base_extensions.contains(p_extension)) return;

    resource_base_extensions[p_extension] = p_class;
}

void ClassDB::get_resource_base_extensions(ListPOD<String> *p_extensions) {

    const StringName *K = nullptr;

    while ((K = resource_base_extensions.next(K))) {

        p_extensions->push_back(*K);
    }
}

void ClassDB::get_extensions_for_type(const StringName &p_class, ListPOD<String> *p_extensions) {

    const StringName *K = nullptr;

    while ((K = resource_base_extensions.next(K))) {
        StringName cmp = resource_base_extensions[*K];
        if (is_parent_class(p_class, cmp) || is_parent_class(cmp, p_class)) p_extensions->push_back(*K);
    }
}

HashMap<StringName, HashMap<StringName, Variant>> ClassDB::default_values;
Set<StringName> ClassDB::default_values_cached;

Variant ClassDB::class_get_default_property_value(
        const StringName &p_class, const StringName &p_property, bool *r_valid) {

    if (!default_values_cached.contains(p_class)) {

        if (!default_values.contains(p_class)) {
            default_values[p_class] = HashMap<StringName, Variant>();
        }

        Object *c = nullptr;
        bool cleanup_c = false;

        if (Engine::get_singleton()->has_singleton(p_class)) {
            c = Engine::get_singleton()->get_singleton_object(p_class);
            cleanup_c = false;
        } else if (ClassDB::can_instance(p_class)) {
            c = ClassDB::instance(p_class);
            cleanup_c = true;
        }

        if (c) {

            ListPOD<PropertyInfo> plist;
            c->get_property_list(&plist);
            for (const PropertyInfo &pi : plist) {
                if (pi.usage & (PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR)) {

                    if (!default_values[p_class].contains(pi.name)) {
                        Variant v = c->get(pi.name);
                        default_values[p_class][pi.name] = v;
                    }
                }
            }

            if (cleanup_c) {
                memdelete(c);
            }
        }

        default_values_cached.insert(p_class);
    }

    if (!default_values.contains(p_class)) {
        if (r_valid != nullptr) *r_valid = false;
        return Variant();
    }

    if (!default_values[p_class].contains(p_property)) {
        if (r_valid != nullptr) *r_valid = false;
        return Variant();
    }

    if (r_valid != nullptr) *r_valid = true;
    return default_values[p_class][p_property];
}

RWLock *ClassDB::lock = nullptr;

void ClassDB::init() {

    lock = RWLock::create();
}

void ClassDB::cleanup_defaults() {

    default_values.clear();
    default_values_cached.clear();
}

void ClassDB::cleanup() {

    // OBJTYPE_LOCK; hah not here

    const StringName *k = nullptr;

    while ((k = classes.next(k))) {

        ClassInfo &ti = classes[*k];

        const StringName *m = nullptr;
        while ((m = ti.method_map.next(m))) {

            memdelete(ti.method_map[*m]);
        }
    }
    classes.clear();
    resource_base_extensions.clear();
    compat_classes.clear();

    memdelete(lock);
}

//

bool ClassDB::bind_helper(MethodBind *bind, const char *instance_type, const StringName &p_name) {
    ClassInfo *type = ClassDB::classes.getptr(StaticCString(bind->get_instance_class(), true));
    if (!type) {
        memdelete(bind);
    }
    ERR_FAIL_COND_V(!type, false)

    if (type->method_map.contains(p_name)) {
        memdelete(bind);
        // overloading not supported
        ERR_FAIL_V_MSG(false, String("Method already bound: ") + instance_type + "::" + p_name + ".")
    }
    type->method_map[p_name] = bind;
#ifdef DEBUG_METHODS_ENABLED
    // FIXME: <reduz> set_return_type is no longer in MethodBind, so I guess it should be moved to vararg method bind
    // bind->set_return_type("Variant");
    type->method_order.push_back(p_name);
#endif
    return true;
}