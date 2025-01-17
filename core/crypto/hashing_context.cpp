/*************************************************************************/
/*  hashing_context.cpp                                                  */
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

#include "hashing_context.h"

#include "core/crypto/crypto_core.h"
#include "core/method_bind.h"
#include "core/pool_vector.h"


IMPL_GDCLASS(HashingContext)

Error HashingContext::start(HashType p_type) {
    ERR_FAIL_COND_V(ctx != nullptr, ERR_ALREADY_IN_USE);
    _create_ctx(p_type);
    ERR_FAIL_COND_V(ctx == nullptr, ERR_UNAVAILABLE);
    switch (type) {
        case HASH_MD5:
            return ((CryptoCore::MD5Context *)ctx)->start();
        case HASH_SHA1:
            return ((CryptoCore::SHA1Context *)ctx)->start();
        case HASH_SHA256:
            return ((CryptoCore::SHA256Context *)ctx)->start();
    }
    return ERR_UNAVAILABLE;
}

Error HashingContext::update(const PoolByteArray& p_chunk) {
    ERR_FAIL_COND_V(ctx == nullptr, ERR_UNCONFIGURED);
    size_t len = p_chunk.size();
    ERR_FAIL_COND_V(len == 0, FAILED);

    PoolByteArray::Read r = p_chunk.read();
    switch (type) {
        case HASH_MD5:
            return ((CryptoCore::MD5Context *)ctx)->update(&r[0], len);
        case HASH_SHA1:
            return ((CryptoCore::SHA1Context *)ctx)->update(&r[0], len);
        case HASH_SHA256:
            return ((CryptoCore::SHA256Context *)ctx)->update(&r[0], len);
    }
    return ERR_UNAVAILABLE;
}

PoolByteArray HashingContext::finish() {
    ERR_FAIL_COND_V(ctx == nullptr, PoolByteArray());
    PoolByteArray out;
    Error err = FAILED;
    switch (type) {
        case HASH_MD5:
            out.resize(16);
            err = ((CryptoCore::MD5Context *)ctx)->finish(out.write().ptr());
            break;
        case HASH_SHA1:
            out.resize(20);
            err = ((CryptoCore::SHA1Context *)ctx)->finish(out.write().ptr());
            break;
        case HASH_SHA256:
            out.resize(32);
            err = ((CryptoCore::SHA256Context *)ctx)->finish(out.write().ptr());
            break;
    }
    _delete_ctx();
    ERR_FAIL_COND_V(err != OK, PoolByteArray());
    return out;
}

void HashingContext::_create_ctx(HashType p_type) {
    type = p_type;
    switch (type) {
        case HASH_MD5:
            ctx = memnew(CryptoCore::MD5Context);
            break;
        case HASH_SHA1:
            ctx = memnew(CryptoCore::SHA1Context);
            break;
        case HASH_SHA256:
            ctx = memnew(CryptoCore::SHA256Context);
            break;
        default:
            ctx = nullptr;
    }
}

void HashingContext::_delete_ctx() {
    switch (type) {
        case HASH_MD5:
            memdelete((CryptoCore::MD5Context *)ctx);
            break;
        case HASH_SHA1:
            memdelete((CryptoCore::SHA1Context *)ctx);
            break;
        case HASH_SHA256:
            memdelete((CryptoCore::SHA256Context *)ctx);
            break;
    }
    ctx = nullptr;
}

void HashingContext::_bind_methods() {
    SE_BIND_METHOD(HashingContext,start);
    SE_BIND_METHOD(HashingContext,update);
    SE_BIND_METHOD(HashingContext,finish);
    BIND_ENUM_CONSTANT(HASH_MD5);
    BIND_ENUM_CONSTANT(HASH_SHA1);
    BIND_ENUM_CONSTANT(HASH_SHA256);
}

HashingContext::HashingContext() {
    ctx = nullptr;
}

HashingContext::~HashingContext() {
    if (ctx != nullptr) {
        _delete_ctx();
}
}
