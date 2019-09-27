#pragma once

#include "core/list.h"
#include "core/string_name.h"
#include "core/variant.h"
#include "core/method_info.h"
#include "core/type_info.h"
#include "core/typesystem_decls.h"

class MethodBind {

    int method_id;
    uint32_t hint_flags;
    StringName name;
    Vector<Variant> default_arguments;
    int default_argument_count;
    int argument_count;
    template<class T, class RESULT,typename ...Args>
    friend class MethodBindVA;
protected:
    const char *instance_class_name=nullptr;
    bool _const;
    bool _returns;
    bool _is_vararg=false;

#ifdef DEBUG_METHODS_ENABLED
    VariantType *argument_types=nullptr;
    PODVector<StringName> arg_names;
#endif
#ifdef DEBUG_METHODS_ENABLED
    bool checkArgs(const Variant** p_args,int p_arg_count,bool (*const verifiers[])(const Variant &), int max_args, Variant::CallError& r_error)
    {
        int max_arg_to_check= p_arg_count<max_args ? p_arg_count : max_args;
        for(int i=0; i<max_arg_to_check; ++i)
        {
            VariantType argtype = argument_types[i+1]; // argument types[0] is return type
            if (!Variant::can_convert_strict(p_args[i]->get_type(), argtype) ||
                    !verifiers[i](*p_args[i])) //!VariantObjectClassChecker<P##m_arg>::check(*p_args[i]))
                     {
                r_error.error = Variant::CallError::CALL_ERROR_INVALID_ARGUMENT;
                r_error.argument = i;
                r_error.expected = argtype;
                return false;
            }
        }
        return true;
    }
#endif
    void _set_const(bool p_const);
    void _set_returns(bool p_returns);
#ifdef DEBUG_METHODS_ENABLED
    virtual PropertyInfo _gen_argument_type_info(int p_arg) const = 0;
#endif
    void set_argument_count(int p_count) { argument_count = p_count; }
    virtual Variant do_call(Object *p_object, const Variant **p_args, int p_arg_count, Variant::CallError &r_error)=0;
    virtual GodotTypeInfo::Metadata do_get_argument_meta(int p_arg) const = 0;
public:
    const Vector<Variant> &get_default_arguments() const { return default_arguments; }
    _FORCE_INLINE_ int get_default_argument_count() const { return default_argument_count; }

    _FORCE_INLINE_ bool has_default_argument(int p_arg) const {

        int idx = argument_count - p_arg - 1;

        if (idx < 0 || idx >= default_arguments.size())
            return false;
        else
            return true;
    }

    _FORCE_INLINE_ Variant get_default_argument(int p_arg) const {

        int idx = argument_count - p_arg - 1;

        if (idx < 0 || idx >= default_arguments.size())
            return Variant();
        else
            return default_arguments[idx];
    }

#ifdef DEBUG_METHODS_ENABLED

    _FORCE_INLINE_ VariantType get_argument_type(int p_argument) const {

        ERR_FAIL_COND_V(p_argument < -1 || p_argument > argument_count, VariantType::NIL)
        return argument_types[p_argument + 1];
    }

    PropertyInfo get_argument_info(int p_argument) const;
    PropertyInfo get_return_info() const;

    void set_argument_names(const PODVector<StringName> &p_names); //set by class, db, can't be inferred otherwise
    PODVector<StringName> get_argument_names() const;

    GodotTypeInfo::Metadata get_argument_meta(int p_arg) const;

#endif
    void set_hint_flags(uint32_t p_hint) { hint_flags = p_hint; }
    uint32_t get_hint_flags() const { return hint_flags | (is_const() ? METHOD_FLAG_CONST : 0) | (is_vararg() ? METHOD_FLAG_VARARG : 0); }
    const char *get_instance_class() const { return  instance_class_name; }

    _FORCE_INLINE_ int get_argument_count() const { return argument_count; }

    Variant call(Object *p_object, const Variant **p_args, int p_arg_count, Variant::CallError &r_error);

#ifdef PTRCALL_ENABLED
    virtual void ptrcall(Object *p_object, const void **p_args, void *r_ret) = 0;
#endif

    StringName get_name() const;
    void set_name(const StringName &p_name);
    _FORCE_INLINE_ int get_method_id() const { return method_id; }
    _FORCE_INLINE_ bool is_const() const { return _const; }
    _FORCE_INLINE_ bool has_return() const { return _returns; }
    _FORCE_INLINE_ bool is_vararg() const { return _is_vararg; }

    void set_default_arguments(const Vector<Variant> &p_defargs);

    MethodBind();
    virtual ~MethodBind();
};

template <class T>
class MethodBindVarArg final : public MethodBind {
public:
    using NativeCall = Variant (T::*)(const Variant **, int, Variant::CallError &);

protected:
    NativeCall call_method;
#ifdef DEBUG_METHODS_ENABLED

    MethodInfo arguments;

#endif
public:
#ifdef DEBUG_METHODS_ENABLED

    PropertyInfo _gen_argument_type_info(int p_arg) const override {

        if (p_arg < 0) {
            return arguments.return_val;
        } else if (p_arg < int(arguments.arguments.size())) {
            return arguments.arguments[p_arg];
        } else {
            //TODO: use a simple char [32] buffer as conversion area.
            return PropertyInfo(VariantType::NIL, StringUtils::to_utf8("arg_" + itos(p_arg)).data(),
                    PROPERTY_HINT_NONE, nullptr, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_NIL_IS_VARIANT);
        }
    }

    VariantType _gen_argument_type(int p_arg) const {
        return _gen_argument_type_info(p_arg).type;
    }

    GodotTypeInfo::Metadata do_get_argument_meta(int) const override {
        return GodotTypeInfo::METADATA_NONE;
    }

#else

    virtual VariantType _gen_argument_type(int p_arg) const {
        return VariantType::NIL;
    }

#endif
    Variant do_call(Object *p_object, const Variant **p_args, int p_arg_count, Variant::CallError &r_error) override {

        T *instance = static_cast<T *>(p_object);
        return (instance->*call_method)(p_args, p_arg_count, r_error);
    }

    void set_method_info(const MethodInfo &p_info) {

        set_argument_count(static_cast<int>(p_info.arguments.size()));
#ifdef DEBUG_METHODS_ENABLED
        VariantType *at = memnew_arr(VariantType, p_info.arguments.size() + 1);
        at[0] = p_info.return_val.type;
        if (!p_info.arguments.empty()) {

            PODVector<StringName> names;
            names.resize(p_info.arguments.size());
            int i=0;
            for (const PropertyInfo & pi : p_info.arguments) {

                names[i] = pi.name;
                at[++i] = pi.type;
            }

            set_argument_names(names);
        }
        argument_types = at;
        arguments = p_info;
        arguments.return_val.usage |= PROPERTY_USAGE_NIL_IS_VARIANT;
#endif
    }

#ifdef PTRCALL_ENABLED
    void ptrcall(Object * /*p_object*/, const void ** /*p_args*/, void * /*r_ret*/) override {
        ERR_FAIL(); //can't call
    } //todo
#endif

    void set_method(NativeCall p_method) { call_method = p_method; }

    MethodBindVarArg() {
        instance_class_name = T::get_class_static();
        call_method = nullptr;
        _is_vararg = true;
        _set_returns(true);
    }
};


#include "core/class_db.h"
template<class T, class RESULT,typename ...Args>
class MethodBindVA;

struct MethodBinder {
    template<class T  ,typename R, typename ...Args>
    static MethodBind* create_method_bind_va( R (T::*p_method)(Args...)  ) {

        MethodBindVA<T  , R,  Args...> * a = memnew_args( (MethodBindVA<T, R, Args...>),p_method );
        return a;
    }
    template<class T  ,typename R, typename ...Args>
    static MethodBind* create_method_bind_va( R (T::*p_method)(Args...) const ) {

        MethodBindVA<const T  , R,  Args...> * a = memnew_args( (MethodBindVA<const T, R, Args...>),p_method );
        return a;
    }
    template <class N, class M>
    static MethodBind *bind_method(N p_method_name, M p_method) {

        MethodBind *bind = create_method_bind_va(p_method);

        return ClassDB::bind_methodfi(METHOD_FLAGS_DEFAULT, bind, p_method_name, {}); //use static function, much smaller binary usage
    }

    template <class N, class M>
    static MethodBind *bind_method(N p_method_name, M p_method, std::initializer_list<Variant> args) {

        MethodBind *bind = create_method_bind_va(p_method);
        return ClassDB::bind_methodfi(METHOD_FLAGS_DEFAULT, bind, p_method_name, args);
    }

    template <class M>
    static MethodBind *bind_vararg_method(const StringName &p_name, M p_method, const MethodInfo &p_info = MethodInfo(), const Vector<Variant> &p_default_args = Vector<Variant>()) {

        GLOBAL_LOCK_FUNCTION

        MethodBind *bind = create_vararg_method_bind(p_method, p_info);
        ERR_FAIL_COND_V(!bind, nullptr)

        bind->set_name(p_name);
        bind->set_default_arguments(p_default_args);

        const char * instance_type = bind->get_instance_class();
        if(!ClassDB::bind_helper(bind,instance_type,p_name) )
        {
            memdelete(bind);
            return nullptr;
        }
        return bind;
    }
};