/*************************************************************************/
/*  shape_bullet.h                                                       */
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
#include "rid_bullet.h"

#include "core/pool_vector.h"
#include "core/hash_map.h"
#include "core/variant.h"
#include "servers/physics_server_3d.h"

#include <LinearMath/btAlignedObjectArray.h>
#include <LinearMath/btScalar.h>
#include <LinearMath/btVector3.h>

/**
    @author AndreaCatania
*/

class ShapeBullet;
class btCollisionShape;
class ShapeOwnerBullet;
class btBvhTriangleMeshShape;
class btVector3;

class ShapeBullet : public RIDBullet {

    using OwnerStorage = HashMap<ShapeOwnerBullet *, int>;
    OwnerStorage owners;
    real_t margin;

protected:
    /// return self
    btCollisionShape *prepare(btCollisionShape *p_btShape) const;
    void notifyShapeChanged();

public:
    ShapeBullet();
    ~ShapeBullet() override;

    btCollisionShape *create_bt_shape(const Vector3 &p_implicit_scale, real_t p_extra_edge = 0);
    virtual btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) = 0;

    void add_owner(ShapeOwnerBullet *p_owner);
    void remove_owner(ShapeOwnerBullet *p_owner, bool p_permanentlyFromThisBody = false);
    bool is_owner(ShapeOwnerBullet *p_owner) const;
    const OwnerStorage &get_owners() const { return owners; }

    void set_margin(real_t p_margin);
    real_t get_margin() const;

    /// Setup the shape
    virtual void set_data(const Variant &p_data) = 0;
    virtual Variant get_data() const = 0;

    virtual PhysicsServer3D::ShapeType get_type() const = 0;

public:
    static class btEmptyShape *create_shape_empty();
    static class btStaticPlaneShape *create_shape_plane(const btVector3 &planeNormal, float planeConstant);
    static class btSphereShape *create_shape_sphere(float radius);
    static class btBoxShape *create_shape_box(const btVector3 &boxHalfExtents);
    static class btCapsuleShapeZ *create_shape_capsule(float radius, float height);
    static class btCylinderShape *create_shape_cylinder(float radius, float height);
    /// IMPORTANT: Remember to delete the shape interface by calling: delete my_shape->getMeshInterface();
    static class btConvexPointCloudShape *create_shape_convex(btAlignedObjectArray<btVector3> &p_vertices, const btVector3 &p_local_scaling = btVector3(1, 1, 1));
    static class btScaledBvhTriangleMeshShape *create_shape_concave(btBvhTriangleMeshShape *p_mesh_shape, const btVector3 &p_local_scaling = btVector3(1, 1, 1));
    static class btHeightfieldTerrainShape *create_shape_height_field(PoolVector<real_t> &p_heights, int p_width, int p_depth, real_t p_min_height, real_t p_max_height);
    static class btRayShape *create_shape_ray(real_t p_length, bool p_slips_on_slope);
};

class PlaneShapeBullet : public ShapeBullet {

    Plane plane;

public:
    PlaneShapeBullet();

    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(const Plane &p_plane);
};

class SphereShapeBullet : public ShapeBullet {

    real_t radius;

public:
    SphereShapeBullet();

    _FORCE_INLINE_ real_t get_radius() { return radius; }
    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(real_t p_radius);
};

class BoxShapeBullet : public ShapeBullet {

    btVector3 half_extents;

public:
    BoxShapeBullet();

    const btVector3 &get_half_extents() { return half_extents; }
    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(const Vector3 &p_half_extents);
};

class CapsuleShapeBullet : public ShapeBullet {

    real_t height;
    real_t radius;

public:
    CapsuleShapeBullet();

    _FORCE_INLINE_ real_t get_height() { return height; }
    _FORCE_INLINE_ real_t get_radius() { return radius; }
    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(real_t p_height, real_t p_radius);
};

class CylinderShapeBullet : public ShapeBullet {

    real_t height;
    real_t radius;

public:
    CylinderShapeBullet();

    _FORCE_INLINE_ real_t get_height() { return height; }
    _FORCE_INLINE_ real_t get_radius() { return radius; }
    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_margin = 0) override;

private:
    void setup(real_t p_height, real_t p_radius);
};

class ConvexPolygonShapeBullet : public ShapeBullet {

public:
    btAlignedObjectArray<btVector3> vertices;

    ConvexPolygonShapeBullet();

    void set_data(const Variant &p_data) override;
    void get_vertices(Vector<Vector3> &out_vertices);
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(const Vector<Vector3> &p_vertices);
};

class ConcavePolygonShapeBullet : public ShapeBullet {
    class btBvhTriangleMeshShape *meshShape;

public:
    PoolVector<Vector3> faces;

    ConcavePolygonShapeBullet();
    ~ConcavePolygonShapeBullet() override;

    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(const PoolVector<Vector3>& p_faces);
};

class HeightMapShapeBullet : public ShapeBullet {

public:
    PoolVector<real_t> heights;
    int width;
    int depth;
    real_t min_height;
    real_t max_height;

    HeightMapShapeBullet();

    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(PoolVector<real_t> &p_heights, int p_width, int p_depth, real_t p_min_height, real_t p_max_height);
};

class RayShapeBullet : public ShapeBullet {

public:
    real_t length;
    bool slips_on_slope;

    RayShapeBullet();

    void set_data(const Variant &p_data) override;
    Variant get_data() const override;
    PhysicsServer3D::ShapeType get_type() const override;
    btCollisionShape *create_bt_shape(const btVector3 &p_implicit_scale, real_t p_extra_edge = 0) override;

private:
    void setup(real_t p_length, bool p_slips_on_slope);
};
