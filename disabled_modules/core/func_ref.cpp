/*************************************************************************/
/*  func_ref.cpp                                                         */
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

#include "func_ref.h"
#include "core/object_db.h"
#include "core/method_bind.h"

IMPL_GDCLASS(FuncRef)

Variant FuncRef::call_func(const Variant **p_args, int p_argcount, Callable::CallError &r_error) {

    if (id.is_null()) {
        r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
        return Variant();
    }
    Object *obj = object_for_entity(id);

    if (!obj) {
        r_error.error = Callable::CallError::CALL_ERROR_INSTANCE_IS_NULL;
        return Variant();
    }

    return obj->call(function, p_args, p_argcount, r_error);
}

Variant FuncRef::call_funcv(const Array &p_args) {

    ERR_FAIL_COND_V(id.is_null(), Variant());

    Object *obj = object_for_entity(id);

    ERR_FAIL_COND_V(!obj, Variant());

    return obj->callv(function, p_args);
}

void FuncRef::set_instance(Object *p_obj) {

    ERR_FAIL_NULL(p_obj);
    id = p_obj->get_instance_id();
}

void FuncRef::set_function(const StringName &p_func) {

    function = p_func;
}

bool FuncRef::is_valid() const {
    if (id.is_null())
        return false;

    Object *obj = object_for_entity(id);
    if (!obj)
        return false;

    return obj->has_method(function);
}

void FuncRef::_bind_methods() {

    {
        MethodInfo mi("call_func");
        MethodBinder::bind_vararg_method(StaticCString("call_func"), &FuncRef::call_func, eastl::move(mi));
    }

    MethodBinder::bind_method(D_METHOD("call_funcv", {"arg_array"}), &FuncRef::call_funcv);

    MethodBinder::bind_method(D_METHOD("set_instance", {"instance"}), &FuncRef::set_instance);
    MethodBinder::bind_method(D_METHOD("set_function", {"name"}), &FuncRef::set_function);
    MethodBinder::bind_method(D_METHOD("is_valid"), &FuncRef::is_valid);
}

