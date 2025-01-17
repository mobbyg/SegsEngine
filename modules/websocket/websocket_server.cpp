/*************************************************************************/
/*  websocket_server.cpp                                                 */
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

#include "websocket_server.h"
#include "core/method_bind.h"
#include "core/crypto/crypto.h"
#include "core/io/ip_address.h"

GDCINULL(WebSocketServer)
IMPL_GDCLASS(WebSocketServer)

WebSocketServer::WebSocketServer() {
    _peer_id = 1;
    bind_ip = IP_Address("*");
}

WebSocketServer::~WebSocketServer() {
}

void WebSocketServer::_bind_methods() {

    SE_BIND_METHOD(WebSocketServer,is_listening);
    MethodBinder::bind_method(D_METHOD("listen", {"port", "protocols", "gd_mp_api"}), &WebSocketServer::listen, {DEFVAL(PoolVector<String>()), DEFVAL(false)});
    SE_BIND_METHOD(WebSocketServer,stop);
    SE_BIND_METHOD(WebSocketServer,has_peer);
    SE_BIND_METHOD(WebSocketServer,get_peer_address);
    SE_BIND_METHOD(WebSocketServer,get_peer_port);
    MethodBinder::bind_method(D_METHOD("disconnect_peer", {"id", "code", "reason"}), &WebSocketServer::disconnect_peer, {DEFVAL(1000), DEFVAL("")});

    SE_BIND_METHOD(WebSocketServer,get_bind_ip);
    MethodBinder::bind_method(D_METHOD("set_bind_ip"), (void (WebSocketServer::*)(StringView))&WebSocketServer::set_bind_ip);
    ADD_PROPERTY(PropertyInfo(VariantType::STRING, "bind_ip"), "set_bind_ip", "get_bind_ip");

    SE_BIND_METHOD(WebSocketServer,get_private_key);
    SE_BIND_METHOD(WebSocketServer,set_private_key);
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "private_key", PropertyHint::ResourceType, "CryptoKey", 0), "set_private_key", "get_private_key");

    SE_BIND_METHOD(WebSocketServer,get_ssl_certificate);
    SE_BIND_METHOD(WebSocketServer,set_ssl_certificate);
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "ssl_certificate", PropertyHint::ResourceType, "X509Certificate", 0), "set_ssl_certificate", "get_ssl_certificate");

    SE_BIND_METHOD(WebSocketServer,get_ca_chain);
    SE_BIND_METHOD(WebSocketServer,set_ca_chain);
    ADD_PROPERTY(PropertyInfo(VariantType::OBJECT, "ca_chain", PropertyHint::ResourceType, "X509Certificate", 0), "set_ca_chain", "get_ca_chain");

    ADD_SIGNAL(MethodInfo("client_close_request", PropertyInfo(VariantType::INT, "id"), PropertyInfo(VariantType::INT, "code"), PropertyInfo(VariantType::STRING, "reason")));
    ADD_SIGNAL(MethodInfo("client_disconnected", PropertyInfo(VariantType::INT, "id"), PropertyInfo(VariantType::BOOL, "was_clean_close")));
    ADD_SIGNAL(MethodInfo("client_connected", PropertyInfo(VariantType::INT, "id"), PropertyInfo(VariantType::STRING, "protocol")));
    ADD_SIGNAL(MethodInfo("data_received", PropertyInfo(VariantType::INT, "id")));
}

IP_Address WebSocketServer::get_bind_ip() const {
    return bind_ip;
}

void WebSocketServer::set_bind_ip(const IP_Address &p_bind_ip) {
    ERR_FAIL_COND(is_listening());
    ERR_FAIL_COND(!p_bind_ip.is_valid() && !p_bind_ip.is_wildcard());
    bind_ip = p_bind_ip;
}

Ref<CryptoKey> WebSocketServer::get_private_key() const {
    return private_key;
}

void WebSocketServer::set_private_key(Ref<CryptoKey> p_key) {
    ERR_FAIL_COND(is_listening());
    private_key = p_key;
}

Ref<X509Certificate> WebSocketServer::get_ssl_certificate() const {
    return ssl_cert;
}

void WebSocketServer::set_ssl_certificate(Ref<X509Certificate> p_cert) {
    ERR_FAIL_COND(is_listening());
    ssl_cert = p_cert;
}

Ref<X509Certificate> WebSocketServer::get_ca_chain() const {
    return ca_chain;
}

void WebSocketServer::set_ca_chain(Ref<X509Certificate> p_ca_chain) {
    ERR_FAIL_COND(is_listening());
    ca_chain = p_ca_chain;
}
NetworkedMultiplayerPeer::ConnectionStatus WebSocketServer::get_connection_status() const {
    if (is_listening())
        return CONNECTION_CONNECTED;

    return CONNECTION_DISCONNECTED;
}

bool WebSocketServer::is_server() const {

    return true;
}

void WebSocketServer::_on_peer_packet(int32_t p_peer_id) {

    if (_is_multiplayer) {
        _process_multiplayer(get_peer(p_peer_id), p_peer_id);
    } else {
        emit_signal("data_received", p_peer_id);
    }
}

void WebSocketServer::_on_connect(int32_t p_peer_id, StringView p_protocol) {

    if (_is_multiplayer) {
        // Send add to clients
        _send_add(p_peer_id);
        emit_signal("peer_connected", p_peer_id);
    } else {
        emit_signal("client_connected", p_peer_id, p_protocol);
    }
}

void WebSocketServer::_on_disconnect(int32_t p_peer_id, bool p_was_clean) {

    if (_is_multiplayer) {
        // Send delete to clients
        _send_del(p_peer_id);
        emit_signal("peer_disconnected", p_peer_id);
    } else {
        emit_signal("client_disconnected", p_peer_id, p_was_clean);
    }
}

void WebSocketServer::_on_close_request(int32_t p_peer_id, int p_code, StringView p_reason) {

    emit_signal("client_close_request", p_peer_id, p_code, p_reason);
}
