.. _matrices-api:

==================
 Matrices
==================




.. highlight:: c

API for initializing and manipulating 4x4 matrices




.. c:type:: c_matrix_t

        A c_matrix_t holds a 4x4 transform matrix. This is a single precision,
        column-major matrix which means it is compatible with what OpenGL expects.

        A c_matrix_t can represent transforms such as, rotations, scaling,
        translation, sheering, and linear projections. You can combine these
        transforms by multiplying multiple matrices in the order you want them
        applied.

        The transformation of a vertex (x, y, z, w) by a c_matrix_t is given by:

        ::

          x_new = xx * x + xy * y + xz * z + xw * w
          y_new = yx * x + yy * y + yz * z + yw * w
          z_new = zx * x + zy * y + zz * z + zw * w
          w_new = wx * x + wy * y + wz * z + ww * w


        Where w is normally 1


        .. note::
                You must consider the members of the c_matrix_t structure read only,
                and all matrix modifications must be done via the c_matrix API. This
                allows clib to annotate the matrices internally. Violation of this will give
                undefined results. If you need to initialize a matrix with a constant other
                than the identity matrix you can use c_matrix_init_from_array().








.. c:function:: void c_matrix_init_identity(c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix


        Resets matrix to the identity matrix:

        ::

          .xx=1; .xy=0; .xz=0; .xw=0;
          .yx=0; .yy=1; .yz=0; .yw=0;
          .zx=0; .zy=0; .zz=1; .zw=0;
          .wx=0; .wy=0; .wz=0; .ww=1;







.. c:function:: void c_matrix_init_translation(c_matrix_t *matrix, float tx, float ty, float tz)

        :param matrix: A 4x4 transformation matrix

        :param tx: x coordinate of the translation vector

        :param ty: y coordinate of the translation vector

        :param tz: z coordinate of the translation vector


        Resets matrix to the (tx, ty, tz) translation matrix:

        ::

          .xx=1; .xy=0; .xz=0; .xw=tx;
          .yx=0; .yy=1; .yz=0; .yw=ty;
          .zx=0; .zy=0; .zz=1; .zw=tz;
          .wx=0; .wy=0; .wz=0; .ww=1;








.. c:function:: void c_matrix_multiply(c_matrix_t *result, const c_matrix_t *a, const c_matrix_t *b)

        :param result: The address of a 4x4 matrix to store the result in

        :param a: A 4x4 transformation matrix

        :param b: A 4x4 transformation matrix


        Multiplies the two supplied matrices together and stores
        the resulting matrix inside :c:data:`result`.


        .. note::
                It is possible to multiply the :c:data:`a` matrix in-place, so
                :c:data:`result` can be equal to :c:data:`a` but can't be equal to :c:data:`b`.








.. c:function:: void c_matrix_rotate(c_matrix_t *matrix, float angle, float x, float y, float z)

        :param matrix: A 4x4 transformation matrix

        :param angle: The angle you want to rotate in degrees

        :param x: X component of your rotation vector

        :param y: Y component of your rotation vector

        :param z: Z component of your rotation vector


        Multiplies :c:data:`matrix` with a rotation matrix that applies a rotation
        of :c:data:`angle` degrees around the specified 3D vector.






.. c:function:: void c_matrix_rotate_quaternion(c_matrix_t *matrix, const c_quaternion_t *quaternion)

        :param matrix: A 4x4 transformation matrix

        :param quaternion: A quaternion describing a rotation


        Multiplies :c:data:`matrix` with a rotation transformation described by the
        given :c:type:`c_quaternion_t`.







.. c:function:: void c_matrix_rotate_euler(c_matrix_t *matrix, const c_euler_t *euler)

        :param matrix: A 4x4 transformation matrix

        :param euler: A euler describing a rotation


        Multiplies :c:data:`matrix` with a rotation transformation described by the
        given :c:type:`c_euler_t`.







.. c:function:: void c_matrix_translate(c_matrix_t *matrix, float x, float y, float z)

        :param matrix: A 4x4 transformation matrix

        :param x: The X translation you want to apply

        :param y: The Y translation you want to apply

        :param z: The Z translation you want to apply


        Multiplies :c:data:`matrix` with a transform matrix that translates along
        the X, Y and Z axis.






.. c:function:: void c_matrix_scale(c_matrix_t *matrix, float sx, float sy, float sz)

        :param matrix: A 4x4 transformation matrix

        :param sx: The X scale factor

        :param sy: The Y scale factor

        :param sz: The Z scale factor


        Multiplies :c:data:`matrix` with a transform matrix that scales along the X,
        Y and Z axis.






.. c:function:: void c_matrix_look_at(c_matrix_t *matrix, float eye_position_x, float eye_position_y, float eye_position_z, float object_x, float object_y, float object_z, float world_up_x, float world_up_y, float world_up_z)

        :param matrix: A 4x4 transformation matrix

        :param eye_position_x: The X coordinate to look from

        :param eye_position_y: The Y coordinate to look from

        :param eye_position_z: The Z coordinate to look from

        :param object_x: The X coordinate of the object to look at

        :param object_y: The Y coordinate of the object to look at

        :param object_z: The Z coordinate of the object to look at

        :param world_up_x: The X component of the world's up direction vector

        :param world_up_y: The Y component of the world's up direction vector

        :param world_up_z: The Z component of the world's up direction vector


        Applies a view transform :c:data:`matrix` that positions the camera at
        the coordinate (:c:data:`eye_position_x`, :c:data:`eye_position_y`, :c:data:`eye_position_z`)
        looking towards an object at the coordinate (:c:data:`object_x`, :c:data:`object_y`,
        :c:data:`object_z`). The top of the camera is aligned to the given world up
        vector, which is normally simply (0, 1, 0) to map up to the
        positive direction of the y axis.

        Because there is a lot of missleading documentation online for
        gluLookAt regarding the up vector we want to try and be a bit
        clearer here.

        The up vector should simply be relative to your world coordinates
        and does not need to change as you move the eye and object
        positions.  Many online sources may claim that the up vector needs
        to be perpendicular to the vector between the eye and object
        position (partly because the man page is somewhat missleading) but
        that is not necessary for this function.


        .. note::
                You should never look directly along the world-up
                vector.




        .. note::
                It is assumed you are using a typical projection matrix where
                your origin maps to the center of your viewport.




        .. note::
                Almost always when you use this function it should be the first
                transform applied to a new modelview transform








.. c:function:: void c_matrix_frustum(c_matrix_t *matrix, float left, float right, float bottom, float top, float z_near, float z_far)

        :param matrix: A 4x4 transformation matrix

        :param left: X position of the left clipping plane where it
          intersects the near clipping plane

        :param right: X position of the right clipping plane where it
          intersects the near clipping plane

        :param bottom: Y position of the bottom clipping plane where it
          intersects the near clipping plane

        :param top: Y position of the top clipping plane where it intersects
          the near clipping plane

        :param z_near: The distance to the near clipping plane (Must be positive)

        :param z_far: The distance to the far clipping plane (Must be positive)


        Multiplies :c:data:`matrix` by the given frustum perspective matrix.






.. c:function:: void c_matrix_perspective(c_matrix_t *matrix, float fov_y, float aspect, float z_near, float z_far)

        :param matrix: A 4x4 transformation matrix

        :param fov_y: Vertical field of view angle in degrees.

        :param aspect: The (width over height) aspect ratio for display

        :param z_near: The distance to the near clipping plane (Must be positive,
          and must not be 0)

        :param z_far: The distance to the far clipping plane (Must be positive)


        Multiplies :c:data:`matrix` by the described perspective matrix


        .. note::
                You should be careful not to have to great a :c:data:`z_far` / :c:data:`z_near`
                ratio since that will reduce the effectiveness of depth testing
                since there wont be enough precision to identify the depth of
                objects near to each other.








.. c:function:: void c_matrix_orthographic(c_matrix_t *matrix, float x_1, float y_1, float x_2, float y_2, float near, float far)

        :param matrix: A 4x4 transformation matrix

        :param x_1: The x coordinate for the first vertical clipping plane

        :param y_1: The y coordinate for the first horizontal clipping plane

        :param x_2: The x coordinate for the second vertical clipping plane

        :param y_2: The y coordinate for the second horizontal clipping plane

        :param near: The *distance* to the near clipping
          plane (will be *negative* if the plane is
          behind the viewer)

        :param far: The *distance* to the far clipping
          plane (will be *negative* if the plane is
          behind the viewer)


        Multiplies :c:data:`matrix` by a parallel projection matrix.






.. c:function:: void c_matrix_view_2d_in_frustum(c_matrix_t *matrix, float left, float right, float bottom, float top, float z_near, float z_2d, float width_2d, float height_2d)

        :param matrix: A 4x4 transformation matrix

        :param left: coord of left vertical clipping plane

        :param right: coord of right vertical clipping plane

        :param bottom: coord of bottom horizontal clipping plane

        :param top: coord of top horizontal clipping plane

        :param z_near: The distance to the near clip plane. Never pass 0 and always pass
          a positive number.

        :param z_2d: The distance to the 2D plane. (Should always be positive and
          be between :c:data:`z_near` and the z_far value that was passed to
          c_matrix_frustum())

        :param width_2d: The width of the 2D coordinate system

        :param height_2d: The height of the 2D coordinate system


        Multiplies :c:data:`matrix` by a view transform that maps the 2D coordinates
        (0,0) top left and (:c:data:`width_2d`,:c:data:`height_2d`) bottom right the full viewport
        size. Geometry at a depth of 0 will now lie on this 2D plane.


        .. note::
                this doesn't multiply the matrix by any projection matrix,
                but it assumes you have a perspective projection as defined by
                passing the corresponding arguments to c_matrix_frustum().

        Toolkits such as Clutter that mix 2D and 3D drawing can use this to
        create a 2D coordinate system within a 3D perspective projected
        view frustum.






.. c:function:: void c_matrix_view_2d_in_perspective(c_matrix_t *matrix, float fov_y, float aspect, float z_near, float z_2d, float width_2d, float height_2d)

        :param fov_y: A field of view angle for the Y axis

        :param aspect: The ratio of width to height determining the field of view angle
          for the x axis.

        :param z_near: The distance to the near clip plane. Never pass 0 and always pass
          a positive number.

        :param z_2d: The distance to the 2D plane. (Should always be positive and
          be between :c:data:`z_near` and the z_far value that was passed to
          c_matrix_frustum())

        :param width_2d: The width of the 2D coordinate system

        :param height_2d: The height of the 2D coordinate system


        Multiplies :c:data:`matrix` by a view transform that maps the 2D coordinates
        (0,0) top left and (:c:data:`width_2d`,:c:data:`height_2d`) bottom right the full viewport
        size. Geometry at a depth of 0 will now lie on this 2D plane.


        .. note::
                this doesn't multiply the matrix by any projection matrix,
                but it assumes you have a perspective projection as defined by
                passing the corresponding arguments to c_matrix_perspective().

        Toolkits such as Clutter that mix 2D and 3D drawing can use this to
        create a 2D coordinate system within a 3D perspective projected
        view frustum.






.. c:function:: void c_matrix_init_from_array(c_matrix_t *matrix, const float *array)

        :param matrix: A 4x4 transformation matrix

        :param array: A linear array of 16 floats (column-major order)


        Initializes :c:data:`matrix` with the contents of :c:data:`array`






.. c:function:: const float *c_matrix_get_array(const c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix


        Casts :c:data:`matrix` to a float array which can be directly passed to OpenGL.

        :returns:  a pointer to the float array






.. c:function:: void c_matrix_init_from_quaternion(c_matrix_t *matrix, const c_quaternion_t *quaternion)

        :param matrix: A 4x4 transformation matrix

        :param quaternion: A :c:type:`c_quaternion_t`


        Initializes :c:data:`matrix` from a :c:type:`c_quaternion_t` rotation.






.. c:function:: void c_matrix_init_from_euler(c_matrix_t *matrix, const c_euler_t *euler)

        :param matrix: A 4x4 transformation matrix

        :param euler: A :c:type:`c_euler_t`


        Initializes :c:data:`matrix` from a :c:type:`c_euler_t` rotation.






.. c:function:: _Bool c_matrix_equal(const void *v1, const void *v2)

        :param v1: A 4x4 transformation matrix

        :param v2: A 4x4 transformation matrix


        Compares two matrices to see if they represent the same
        transformation. Although internally the matrices may have different
        annotations associated with them and may potentially have a cached
        inverse matrix these are not considered in the comparison.







.. c:function:: c_matrix_t *c_matrix_copy(const c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix you want to copy


        Allocates a new :c:type:`c_matrix_t` on the heap and initializes it with
        the same values as :c:data:`matrix`.

        :returns:  (transfer full): A newly allocated :c:type:`c_matrix_t` which
        should be freed using c_matrix_free()







.. c:function:: void c_matrix_free(c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix you want to free


        Frees a :c:type:`c_matrix_t` that was previously allocated via a call to
        c_matrix_copy().







.. c:function:: _Bool c_matrix_get_inverse(const c_matrix_t *matrix, c_matrix_t *inverse)

        :param matrix: A 4x4 transformation matrix

        :param inverse: (out): The destination for a 4x4 inverse transformation matrix


        Gets the inverse transform of a given matrix and uses it to initialize
        a new :c:type:`c_matrix_t`.


        .. note::
                Although the first parameter is annotated as const to indicate
                that the transform it represents isn't modified this function may
                technically save a copy of the inverse transform within the given
                :c:type:`c_matrix_t` so that subsequent requests for the inverse transform may
                avoid costly inversion calculations.



        :returns:  ``true`` if the inverse was successfully calculated or ``false``
          for degenerate transformations that can't be inverted (in this case the
          :c:data:`inverse` matrix will simply be initialized with the identity matrix)







.. c:function:: void c_matrix_transform_point(const c_matrix_t *matrix, float *x, float *y, float *z, float *w)

        :param matrix: A 4x4 transformation matrix

        :param x: (inout): The X component of your points position

        :param y: (inout): The Y component of your points position

        :param z: (inout): The Z component of your points position

        :param w: (inout): The W component of your points position


        Transforms a point whos position is given and returned as four float
        components.






.. c:function:: void c_matrix_transform_points(const c_matrix_t *matrix, int n_components, size_t stride_in, const void *points_in, size_t stride_out, void *points_out, int n_points)

        :param matrix: A transformation matrix

        :param n_components: The number of position components for each input point.
                       (either 2 or 3)

        :param stride_in: The stride in bytes between input points.

        :param points_in: A pointer to the first component of the first input point.

        :param stride_out: The stride in bytes between output points.

        :param points_out: A pointer to the first component of the first output point.

        :param n_points: The number of points to transform.


        Transforms an array of input points and writes the result to
        another array of output points. The input points can either have 2
        or 3 components each. The output points always have 3 components.
        The output array can simply point to the input array to do the
        transform in-place.

        If you need to transform 4 component points see
        c_matrix_project_points().

        Here's an example with differing input/output strides:
        ::

          typedef struct {
            float x,y;
            uint8_t r,g,b,a;
            float s,t,p;
          } MyInVertex;
          typedef struct {
            uint8_t r,g,b,a;
            float x,y,z;
          } MyOutVertex;
          MyInVertex vertices[N_VERTICES];
          MyOutVertex results[N_VERTICES];
          c_matrix_t matrix;

          my_load_vertices (vertices);
          my_get_matrix (&matrix);

          c_matrix_transform_points (&matrix,
                                        2,
                                        sizeof (MyInVertex),
                                        &vertices[0].x,
                                        sizeof (MyOutVertex),
                                        &results[0].x,
                                        N_VERTICES);








.. c:function:: void c_matrix_project_points(const c_matrix_t *matrix, int n_components, size_t stride_in, const void *points_in, size_t stride_out, void *points_out, int n_points)

        :param matrix: A projection matrix

        :param n_components: The number of position components for each input point.
                       (either 2, 3 or 4)

        :param stride_in: The stride in bytes between input points.

        :param points_in: A pointer to the first component of the first input point.

        :param stride_out: The stride in bytes between output points.

        :param points_out: A pointer to the first component of the first output point.

        :param n_points: The number of points to transform.


        Projects an array of input points and writes the result to another
        array of output points. The input points can either have 2, 3 or 4
        components each. The output points always have 4 components (known
        as homogenous coordinates). The output array can simply point to
        the input array to do the transform in-place.

        Here's an example with differing input/output strides:
        ::

          typedef struct {
            float x,y;
            uint8_t r,g,b,a;
            float s,t,p;
          } MyInVertex;
          typedef struct {
            uint8_t r,g,b,a;
            float x,y,z;
          } MyOutVertex;
          MyInVertex vertices[N_VERTICES];
          MyOutVertex results[N_VERTICES];
          c_matrix_t matrix;

          my_load_vertices (vertices);
          my_get_matrix (&matrix);

          c_matrix_project_points (&matrix,
                                      2,
                                      sizeof (MyInVertex),
                                      &vertices[0].x,
                                      sizeof (MyOutVertex),
                                      &results[0].x,
                                      N_VERTICES);








.. c:function:: _Bool c_matrix_is_identity(const c_matrix_t *matrix)

        :param matrix: A :c:type:`c_matrix_t`


        Determines if the given matrix is an identity matrix.

        :returns:  ``true`` if :c:data:`matrix` is an identity matrix else ``false``






.. c:function:: void c_matrix_transpose(c_matrix_t *matrix)

        :param matrix: A :c:type:`c_matrix_t`


        Replaces :c:data:`matrix` with its transpose. Ie, every element (i,j) in the
        new matrix is taken from element (j,i) in the old matrix.







.. c:function:: void c_matrix_print(const c_matrix_t *matrix)

        :param matrix: 
        :type matrix: const c_matrix_t *


.. c:function:: void c_matrix_prefix_print(const char *prefix, const c_matrix_t *matrix)

        :param prefix: 
        :type prefix: const char *
        :param matrix: 
        :type matrix: const c_matrix_t *


.. c:function:: void c_matrix_init_identity(c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix


        Resets matrix to the identity matrix:

        ::

          .xx=1; .xy=0; .xz=0; .xw=0;
          .yx=0; .yy=1; .yz=0; .yw=0;
          .zx=0; .zy=0; .zz=1; .zw=0;
          .wx=0; .wy=0; .wz=0; .ww=1;







.. c:function:: void c_matrix_init_translation(c_matrix_t *matrix, float tx, float ty, float tz)

        :param matrix: A 4x4 transformation matrix

        :param tx: x coordinate of the translation vector

        :param ty: y coordinate of the translation vector

        :param tz: z coordinate of the translation vector


        Resets matrix to the (tx, ty, tz) translation matrix:

        ::

          .xx=1; .xy=0; .xz=0; .xw=tx;
          .yx=0; .yy=1; .yz=0; .yw=ty;
          .zx=0; .zy=0; .zz=1; .zw=tz;
          .wx=0; .wy=0; .wz=0; .ww=1;








.. c:function:: void c_matrix_multiply(c_matrix_t *result, const c_matrix_t *a, const c_matrix_t *b)

        :param result: The address of a 4x4 matrix to store the result in

        :param a: A 4x4 transformation matrix

        :param b: A 4x4 transformation matrix


        Multiplies the two supplied matrices together and stores
        the resulting matrix inside :c:data:`result`.


        .. note::
                It is possible to multiply the :c:data:`a` matrix in-place, so
                :c:data:`result` can be equal to :c:data:`a` but can't be equal to :c:data:`b`.








.. c:function:: void c_matrix_rotate(c_matrix_t *matrix, float angle, float x, float y, float z)

        :param matrix: A 4x4 transformation matrix

        :param angle: The angle you want to rotate in degrees

        :param x: X component of your rotation vector

        :param y: Y component of your rotation vector

        :param z: Z component of your rotation vector


        Multiplies :c:data:`matrix` with a rotation matrix that applies a rotation
        of :c:data:`angle` degrees around the specified 3D vector.






.. c:function:: void c_matrix_rotate_quaternion(c_matrix_t *matrix, const c_quaternion_t *quaternion)

        :param matrix: A 4x4 transformation matrix

        :param quaternion: A quaternion describing a rotation


        Multiplies :c:data:`matrix` with a rotation transformation described by the
        given :c:type:`c_quaternion_t`.







.. c:function:: void c_matrix_rotate_euler(c_matrix_t *matrix, const c_euler_t *euler)

        :param matrix: A 4x4 transformation matrix

        :param euler: A euler describing a rotation


        Multiplies :c:data:`matrix` with a rotation transformation described by the
        given :c:type:`c_euler_t`.







.. c:function:: void c_matrix_translate(c_matrix_t *matrix, float x, float y, float z)

        :param matrix: A 4x4 transformation matrix

        :param x: The X translation you want to apply

        :param y: The Y translation you want to apply

        :param z: The Z translation you want to apply


        Multiplies :c:data:`matrix` with a transform matrix that translates along
        the X, Y and Z axis.






.. c:function:: void c_matrix_scale(c_matrix_t *matrix, float sx, float sy, float sz)

        :param matrix: A 4x4 transformation matrix

        :param sx: The X scale factor

        :param sy: The Y scale factor

        :param sz: The Z scale factor


        Multiplies :c:data:`matrix` with a transform matrix that scales along the X,
        Y and Z axis.






.. c:function:: void c_matrix_look_at(c_matrix_t *matrix, float eye_position_x, float eye_position_y, float eye_position_z, float object_x, float object_y, float object_z, float world_up_x, float world_up_y, float world_up_z)

        :param matrix: A 4x4 transformation matrix

        :param eye_position_x: The X coordinate to look from

        :param eye_position_y: The Y coordinate to look from

        :param eye_position_z: The Z coordinate to look from

        :param object_x: The X coordinate of the object to look at

        :param object_y: The Y coordinate of the object to look at

        :param object_z: The Z coordinate of the object to look at

        :param world_up_x: The X component of the world's up direction vector

        :param world_up_y: The Y component of the world's up direction vector

        :param world_up_z: The Z component of the world's up direction vector


        Applies a view transform :c:data:`matrix` that positions the camera at
        the coordinate (:c:data:`eye_position_x`, :c:data:`eye_position_y`, :c:data:`eye_position_z`)
        looking towards an object at the coordinate (:c:data:`object_x`, :c:data:`object_y`,
        :c:data:`object_z`). The top of the camera is aligned to the given world up
        vector, which is normally simply (0, 1, 0) to map up to the
        positive direction of the y axis.

        Because there is a lot of missleading documentation online for
        gluLookAt regarding the up vector we want to try and be a bit
        clearer here.

        The up vector should simply be relative to your world coordinates
        and does not need to change as you move the eye and object
        positions.  Many online sources may claim that the up vector needs
        to be perpendicular to the vector between the eye and object
        position (partly because the man page is somewhat missleading) but
        that is not necessary for this function.


        .. note::
                You should never look directly along the world-up
                vector.




        .. note::
                It is assumed you are using a typical projection matrix where
                your origin maps to the center of your viewport.




        .. note::
                Almost always when you use this function it should be the first
                transform applied to a new modelview transform








.. c:function:: void c_matrix_frustum(c_matrix_t *matrix, float left, float right, float bottom, float top, float z_near, float z_far)

        :param matrix: A 4x4 transformation matrix

        :param left: X position of the left clipping plane where it
          intersects the near clipping plane

        :param right: X position of the right clipping plane where it
          intersects the near clipping plane

        :param bottom: Y position of the bottom clipping plane where it
          intersects the near clipping plane

        :param top: Y position of the top clipping plane where it intersects
          the near clipping plane

        :param z_near: The distance to the near clipping plane (Must be positive)

        :param z_far: The distance to the far clipping plane (Must be positive)


        Multiplies :c:data:`matrix` by the given frustum perspective matrix.






.. c:function:: void c_matrix_perspective(c_matrix_t *matrix, float fov_y, float aspect, float z_near, float z_far)

        :param matrix: A 4x4 transformation matrix

        :param fov_y: Vertical field of view angle in degrees.

        :param aspect: The (width over height) aspect ratio for display

        :param z_near: The distance to the near clipping plane (Must be positive,
          and must not be 0)

        :param z_far: The distance to the far clipping plane (Must be positive)


        Multiplies :c:data:`matrix` by the described perspective matrix


        .. note::
                You should be careful not to have to great a :c:data:`z_far` / :c:data:`z_near`
                ratio since that will reduce the effectiveness of depth testing
                since there wont be enough precision to identify the depth of
                objects near to each other.








.. c:function:: void c_matrix_orthographic(c_matrix_t *matrix, float x_1, float y_1, float x_2, float y_2, float near, float far)

        :param matrix: A 4x4 transformation matrix

        :param x_1: The x coordinate for the first vertical clipping plane

        :param y_1: The y coordinate for the first horizontal clipping plane

        :param x_2: The x coordinate for the second vertical clipping plane

        :param y_2: The y coordinate for the second horizontal clipping plane

        :param near: The *distance* to the near clipping
          plane (will be *negative* if the plane is
          behind the viewer)

        :param far: The *distance* to the far clipping
          plane (will be *negative* if the plane is
          behind the viewer)


        Multiplies :c:data:`matrix` by a parallel projection matrix.






.. c:function:: void c_matrix_view_2d_in_frustum(c_matrix_t *matrix, float left, float right, float bottom, float top, float z_near, float z_2d, float width_2d, float height_2d)

        :param matrix: A 4x4 transformation matrix

        :param left: coord of left vertical clipping plane

        :param right: coord of right vertical clipping plane

        :param bottom: coord of bottom horizontal clipping plane

        :param top: coord of top horizontal clipping plane

        :param z_near: The distance to the near clip plane. Never pass 0 and always pass
          a positive number.

        :param z_2d: The distance to the 2D plane. (Should always be positive and
          be between :c:data:`z_near` and the z_far value that was passed to
          c_matrix_frustum())

        :param width_2d: The width of the 2D coordinate system

        :param height_2d: The height of the 2D coordinate system


        Multiplies :c:data:`matrix` by a view transform that maps the 2D coordinates
        (0,0) top left and (:c:data:`width_2d`,:c:data:`height_2d`) bottom right the full viewport
        size. Geometry at a depth of 0 will now lie on this 2D plane.


        .. note::
                this doesn't multiply the matrix by any projection matrix,
                but it assumes you have a perspective projection as defined by
                passing the corresponding arguments to c_matrix_frustum().

        Toolkits such as Clutter that mix 2D and 3D drawing can use this to
        create a 2D coordinate system within a 3D perspective projected
        view frustum.






.. c:function:: void c_matrix_view_2d_in_perspective(c_matrix_t *matrix, float fov_y, float aspect, float z_near, float z_2d, float width_2d, float height_2d)

        :param fov_y: A field of view angle for the Y axis

        :param aspect: The ratio of width to height determining the field of view angle
          for the x axis.

        :param z_near: The distance to the near clip plane. Never pass 0 and always pass
          a positive number.

        :param z_2d: The distance to the 2D plane. (Should always be positive and
          be between :c:data:`z_near` and the z_far value that was passed to
          c_matrix_frustum())

        :param width_2d: The width of the 2D coordinate system

        :param height_2d: The height of the 2D coordinate system


        Multiplies :c:data:`matrix` by a view transform that maps the 2D coordinates
        (0,0) top left and (:c:data:`width_2d`,:c:data:`height_2d`) bottom right the full viewport
        size. Geometry at a depth of 0 will now lie on this 2D plane.


        .. note::
                this doesn't multiply the matrix by any projection matrix,
                but it assumes you have a perspective projection as defined by
                passing the corresponding arguments to c_matrix_perspective().

        Toolkits such as Clutter that mix 2D and 3D drawing can use this to
        create a 2D coordinate system within a 3D perspective projected
        view frustum.






.. c:function:: void c_matrix_init_from_array(c_matrix_t *matrix, const float *array)

        :param matrix: A 4x4 transformation matrix

        :param array: A linear array of 16 floats (column-major order)


        Initializes :c:data:`matrix` with the contents of :c:data:`array`






.. c:function:: const float *c_matrix_get_array(const c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix


        Casts :c:data:`matrix` to a float array which can be directly passed to OpenGL.

        :returns:  a pointer to the float array






.. c:function:: void c_matrix_init_from_quaternion(c_matrix_t *matrix, const c_quaternion_t *quaternion)

        :param matrix: A 4x4 transformation matrix

        :param quaternion: A :c:type:`c_quaternion_t`


        Initializes :c:data:`matrix` from a :c:type:`c_quaternion_t` rotation.






.. c:function:: void c_matrix_init_from_euler(c_matrix_t *matrix, const c_euler_t *euler)

        :param matrix: A 4x4 transformation matrix

        :param euler: A :c:type:`c_euler_t`


        Initializes :c:data:`matrix` from a :c:type:`c_euler_t` rotation.






.. c:function:: _Bool c_matrix_equal(const void *v1, const void *v2)

        :param v1: A 4x4 transformation matrix

        :param v2: A 4x4 transformation matrix


        Compares two matrices to see if they represent the same
        transformation. Although internally the matrices may have different
        annotations associated with them and may potentially have a cached
        inverse matrix these are not considered in the comparison.







.. c:function:: c_matrix_t *c_matrix_copy(const c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix you want to copy


        Allocates a new :c:type:`c_matrix_t` on the heap and initializes it with
        the same values as :c:data:`matrix`.

        :returns:  (transfer full): A newly allocated :c:type:`c_matrix_t` which
        should be freed using c_matrix_free()







.. c:function:: void c_matrix_free(c_matrix_t *matrix)

        :param matrix: A 4x4 transformation matrix you want to free


        Frees a :c:type:`c_matrix_t` that was previously allocated via a call to
        c_matrix_copy().







.. c:function:: _Bool c_matrix_get_inverse(const c_matrix_t *matrix, c_matrix_t *inverse)

        :param matrix: A 4x4 transformation matrix

        :param inverse: (out): The destination for a 4x4 inverse transformation matrix


        Gets the inverse transform of a given matrix and uses it to initialize
        a new :c:type:`c_matrix_t`.


        .. note::
                Although the first parameter is annotated as const to indicate
                that the transform it represents isn't modified this function may
                technically save a copy of the inverse transform within the given
                :c:type:`c_matrix_t` so that subsequent requests for the inverse transform may
                avoid costly inversion calculations.



        :returns:  ``true`` if the inverse was successfully calculated or ``false``
          for degenerate transformations that can't be inverted (in this case the
          :c:data:`inverse` matrix will simply be initialized with the identity matrix)







.. c:function:: void c_matrix_transform_point(const c_matrix_t *matrix, float *x, float *y, float *z, float *w)

        :param matrix: A 4x4 transformation matrix

        :param x: (inout): The X component of your points position

        :param y: (inout): The Y component of your points position

        :param z: (inout): The Z component of your points position

        :param w: (inout): The W component of your points position


        Transforms a point whos position is given and returned as four float
        components.






.. c:function:: void c_matrix_transform_points(const c_matrix_t *matrix, int n_components, size_t stride_in, const void *points_in, size_t stride_out, void *points_out, int n_points)

        :param matrix: A transformation matrix

        :param n_components: The number of position components for each input point.
                       (either 2 or 3)

        :param stride_in: The stride in bytes between input points.

        :param points_in: A pointer to the first component of the first input point.

        :param stride_out: The stride in bytes between output points.

        :param points_out: A pointer to the first component of the first output point.

        :param n_points: The number of points to transform.


        Transforms an array of input points and writes the result to
        another array of output points. The input points can either have 2
        or 3 components each. The output points always have 3 components.
        The output array can simply point to the input array to do the
        transform in-place.

        If you need to transform 4 component points see
        c_matrix_project_points().

        Here's an example with differing input/output strides:
        ::

          typedef struct {
            float x,y;
            uint8_t r,g,b,a;
            float s,t,p;
          } MyInVertex;
          typedef struct {
            uint8_t r,g,b,a;
            float x,y,z;
          } MyOutVertex;
          MyInVertex vertices[N_VERTICES];
          MyOutVertex results[N_VERTICES];
          c_matrix_t matrix;

          my_load_vertices (vertices);
          my_get_matrix (&matrix);

          c_matrix_transform_points (&matrix,
                                        2,
                                        sizeof (MyInVertex),
                                        &vertices[0].x,
                                        sizeof (MyOutVertex),
                                        &results[0].x,
                                        N_VERTICES);








.. c:function:: void c_matrix_project_points(const c_matrix_t *matrix, int n_components, size_t stride_in, const void *points_in, size_t stride_out, void *points_out, int n_points)

        :param matrix: A projection matrix

        :param n_components: The number of position components for each input point.
                       (either 2, 3 or 4)

        :param stride_in: The stride in bytes between input points.

        :param points_in: A pointer to the first component of the first input point.

        :param stride_out: The stride in bytes between output points.

        :param points_out: A pointer to the first component of the first output point.

        :param n_points: The number of points to transform.


        Projects an array of input points and writes the result to another
        array of output points. The input points can either have 2, 3 or 4
        components each. The output points always have 4 components (known
        as homogenous coordinates). The output array can simply point to
        the input array to do the transform in-place.

        Here's an example with differing input/output strides:
        ::

          typedef struct {
            float x,y;
            uint8_t r,g,b,a;
            float s,t,p;
          } MyInVertex;
          typedef struct {
            uint8_t r,g,b,a;
            float x,y,z;
          } MyOutVertex;
          MyInVertex vertices[N_VERTICES];
          MyOutVertex results[N_VERTICES];
          c_matrix_t matrix;

          my_load_vertices (vertices);
          my_get_matrix (&matrix);

          c_matrix_project_points (&matrix,
                                      2,
                                      sizeof (MyInVertex),
                                      &vertices[0].x,
                                      sizeof (MyOutVertex),
                                      &results[0].x,
                                      N_VERTICES);








.. c:function:: _Bool c_matrix_is_identity(const c_matrix_t *matrix)

        :param matrix: A :c:type:`c_matrix_t`


        Determines if the given matrix is an identity matrix.

        :returns:  ``true`` if :c:data:`matrix` is an identity matrix else ``false``






.. c:function:: void c_matrix_transpose(c_matrix_t *matrix)

        :param matrix: A :c:type:`c_matrix_t`


        Replaces :c:data:`matrix` with its transpose. Ie, every element (i,j) in the
        new matrix is taken from element (j,i) in the old matrix.







.. c:function:: void c_matrix_print(const c_matrix_t *matrix)

        :param matrix: 
        :type matrix: const c_matrix_t *


.. c:function:: void c_matrix_prefix_print(const char *prefix, const c_matrix_t *matrix)

        :param prefix: 
        :type prefix: const char *
        :param matrix: 
        :type matrix: const c_matrix_t *
