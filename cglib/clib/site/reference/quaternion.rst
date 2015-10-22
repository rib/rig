.. _quaternions-api:

==================
 Quaternions
==================




.. highlight:: c

API for initializing and manipulating
quaternions.

Quaternions have become a standard form for representing 3D
rotations and have some nice properties when compared with other
representation such as (roll,pitch,yaw) Euler angles. They can be
used to interpolate between different rotations and they don't
suffer from a problem called
`"Gimbal lock" <http://en.wikipedia.org/wiki/Gimbal_lock>`_
where two of the axis of rotation may become aligned and you loose a
degree of freedom.
.



.. c:type:: c_quaternion_t

        .. c:member:: w

           based on the angle of rotation it is cos(𝜃/2)

        .. c:member:: x

            based on the angle of rotation and x component of the axis of
            rotation it is sin(𝜃/2)*axis.x


        .. c:member:: y

            based on the angle of rotation and y component of the axis of
            rotation it is sin(𝜃/2)*axis.y


        .. c:member:: z

            based on the angle of rotation and z component of the axis of
            rotation it is sin(𝜃/2)*axis.z


        A quaternion is comprised of a scalar component and a 3D vector
        component. The scalar component is normally referred to as w and the
        vector might either be referred to as v or a (for axis) or expanded
        with the individual components: (x, y, z) A full quaternion would
        then be written as ``[w (x, y, z)]``.

        Quaternions can be considered to represent an axis and angle
        pair although these numbers are buried somewhat under some
        maths...

        For the curious you can see here that a given axis (a) and angle (𝜃)
        pair are represented in a quaternion as follows:
        ::

          [w=cos(𝜃/2) ( x=sin(𝜃/2)*a.x, y=sin(𝜃/2)*a.y, z=sin(𝜃/2)*a.x )]



        Unit Quaternions:
        When using Quaternions to represent spatial orientations for 3D
        graphics it's always assumed you have a unit quaternion. The
        magnitude of a quaternion is defined as:
        ::

          sqrt (w² + x² + y² + z²)


        and a unit quaternion satisfies this equation:
        ::

          w² + x² + y² + z² = 1



        Most of the time we don't have to worry about the maths involved
        with quaternions but if you are curious to learn more here are some
        external references:


        1.
        http://mathworld.wolfram.com/Quaternion.html

        2.
        http://www.gamedev.net/reference/articles/article1095.asp

        3.
        http://www.cprogramming.com/tutorial/3d/quaternions.html

        4.
        http://www.isner.com/tutorials/quatSpells/quaternion_spells_12.htm

        5.
        3D Maths Primer for Graphics and Game Development ISBN-10: 1556229119

        6.
        http://www.cs.caltech.edu/courses/cs171/quatut.pdf

        7.
        http://www.j3d.org/matrix_faq/matrfaq_latest.html:c:type:`Q56`









.. c:function:: void c_quaternion_init(c_quaternion_t *quaternion, float angle, float x, float y, float z)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle you want to rotate around the given axis

        :param x: The x component of your axis vector about which you want to

        rotate.
        :param y: The y component of your axis vector about which you want to

        rotate.
        :param z: The z component of your axis vector about which you want to

        rotate.

        Initializes a quaternion that rotates :c:data:`angle` degrees around the
        axis vector (:c:data:`x`, :c:data:`y`, :c:data:`z`). The axis vector does not need to be
        normalized.

        :returns:  A normalized, unit quaternion representing an orientation
        rotated :c:data:`angle` degrees around the axis vector (:c:data:`x`, :c:data:`y`, :c:data:`z`)







.. c:function:: void c_quaternion_init_from_angle_vector(c_quaternion_t *quaternion, float angle, const float *axis3f)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around :c:data:`axis3f`

        :param axis3f: your 3 component axis vector about which you want to rotate.


        Initializes a quaternion that rotates :c:data:`angle` degrees around the
        given :c:data:`axis` vector. The axis vector does not need to be
        normalized.

        :returns:  A normalized, unit quaternion representing an orientation
        rotated :c:data:`angle` degrees around the given :c:data:`axis` vector.







.. c:function:: void c_quaternion_init_identity(c_quaternion_t *quaternion)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`


        Initializes the quaternion with the canonical quaternion identity
        [1 (0, 0, 0)] which represents no rotation. Multiplying a
        quaternion with this identity leaves the quaternion unchanged.

        You might also want to consider using
        cg_get_static_identity_quaternion().







.. c:function:: void c_quaternion_init_from_array(c_quaternion_t *quaternion, const float *array)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param array: An array of 4 floats w,(x,y,z)


        Initializes a [w (x, y,z)] quaternion directly from an array of 4
        floats: [w,x,y,z].







.. c:function:: void c_quaternion_init_from_x_rotation(c_quaternion_t *quaternion, float angle)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around the x axis


        XXX: check which direction this rotates







.. c:function:: void c_quaternion_init_from_y_rotation(c_quaternion_t *quaternion, float angle)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around the y axis









.. c:function:: void c_quaternion_init_from_z_rotation(c_quaternion_t *quaternion, float angle)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around the z axis









.. c:function:: void c_quaternion_init_from_euler(c_quaternion_t *quaternion, const c_euler_t *euler)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param euler: A :c:type:`c_euler_t` with which to initialize the quaternion








.. c:function:: void c_quaternion_init_from_matrix(c_quaternion_t *quaternion, const c_matrix_t *matrix)

        :param quaternion: A Quaternion

        :param matrix: A rotation matrix with which to initialize the quaternion


        Initializes a quaternion from a rotation matrix.






.. c:function:: _Bool c_quaternion_equal(const void *v1, const void *v2)

        :param v1: A :c:type:`c_quaternion_t`

        :param v2: A :c:type:`c_quaternion_t`


        Compares that all the components of quaternions :c:data:`a` and :c:data:`b` are
        equal.

        An epsilon value is not used to compare the float components, but
        the == operator is at least used so that 0 and -0 are considered
        equal.

        :returns:  ``true`` if the quaternions are equal else ``false``.







.. c:function:: c_quaternion_t *c_quaternion_copy(const c_quaternion_t *src)

        :param src: A :c:type:`c_quaternion_t`


        Allocates a new :c:type:`c_quaternion_t` on the stack and initializes it with
        the same values as :c:data:`src`.

        :returns:  A newly allocated :c:type:`c_quaternion_t` which should be freed
        using c_quaternion_free()







.. c:function:: void c_quaternion_free(c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`


        Frees a :c:type:`c_quaternion_t` that was previously allocated via
        c_quaternion_copy().







.. c:function:: float c_quaternion_get_rotation_angle(const c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`









.. c:function:: void c_quaternion_get_rotation_axis(const c_quaternion_t *quaternion, float *vector3)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param vector3: (out): an allocated 3-float array








.. c:function:: void c_quaternion_normalize(c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`









.. c:function:: float c_quaternion_dot_product(const c_quaternion_t *a, const c_quaternion_t *b)

        :param a: A :c:type:`c_quaternion_t`

        :param b: A :c:type:`c_quaternion_t`








.. c:function:: void c_quaternion_invert(c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`









.. c:function:: void c_quaternion_multiply(c_quaternion_t *result, const c_quaternion_t *left, const c_quaternion_t *right)

        :param result: The destination :c:type:`c_quaternion_t`

        :param left: The second :c:type:`c_quaternion_t` rotation to apply

        :param right: The first :c:type:`c_quaternion_t` rotation to apply


        This combines the rotations of two quaternions into :c:data:`result`. The
        operation is not commutative so the order is important; AxB
        != BxA. Clib follows the standard convention for quaternions here
        so the rotations are applied :c:data:`right` to :c:data:`left`. This is similar to the
        combining of matrices.


        .. note::
                It is possible to multiply the :c:data:`a` quaternion in-place, so
                :c:data:`result` can be equal to :c:data:`a` but can't be equal to :c:data:`b`.









.. c:function:: void c_quaternion_pow(c_quaternion_t *quaternion, float exponent)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param exponent: the exponent









.. c:function:: void c_quaternion_slerp(c_quaternion_t *result, const c_quaternion_t *a, const c_quaternion_t *b, float t)

        :param result: The destination :c:type:`c_quaternion_t`

        :param a: The first :c:type:`c_quaternion_t`

        :param b: The second :c:type:`c_quaternion_t`

        :param t: The factor in the range [0,1] used to interpolate between

        quaternion :c:data:`a` and :c:data:`b`.

        Performs a spherical linear interpolation between two quaternions.

        Noteable properties:

        1.
        commutative: No

        2.
        constant velocity: Yes

        3.
        torque minimal (travels along the surface of the 4-sphere): Yes

        4.
        more expensive than c_quaternion_nlerp()








.. c:function:: void c_quaternion_nlerp(c_quaternion_t *result, const c_quaternion_t *a, const c_quaternion_t *b, float t)

        :param result: The destination :c:type:`c_quaternion_t`

        :param a: The first :c:type:`c_quaternion_t`

        :param b: The second :c:type:`c_quaternion_t`

        :param t: The factor in the range [0,1] used to interpolate between

        quaterion :c:data:`a` and :c:data:`b`.

        Performs a normalized linear interpolation between two quaternions.
        That is it does a linear interpolation of the quaternion components
        and then normalizes the result. This will follow the shortest arc
        between the two orientations (just like the slerp() function) but
        will not progress at a constant speed. Unlike slerp() nlerp is
        commutative which is useful if you are blending animations
        together. (I.e. nlerp (tmp, a, b) followed by nlerp (result, tmp,
        d) is the same as nlerp (tmp, a, d) followed by nlerp (result, tmp,
        b)). Finally nlerp is cheaper than slerp so it can be a good choice
        if you don't need the constant speed property of the slerp() function.

        Notable properties:

        1.
        commutative: Yes

        2.
        constant velocity: No

        3.
        torque minimal (travels along the surface of the 4-sphere): Yes

        4.
        faster than c_quaternion_slerp()








.. c:function:: void c_quaternion_squad(c_quaternion_t *result, const c_quaternion_t *prev, const c_quaternion_t *a, const c_quaternion_t *b, const c_quaternion_t *next, float t)

        :param result: The destination :c:type:`c_quaternion_t`

        :param prev: A :c:type:`c_quaternion_t` used before :c:data:`a`

        :param a: The first :c:type:`c_quaternion_t`

        :param b: The second :c:type:`c_quaternion_t`

        :param next: A :c:type:`c_quaternion_t` that will be used after :c:data:`b`

        :param t: The factor in the range [0,1] used to interpolate between

        quaternion :c:data:`a` and :c:data:`b`.








.. c:function:: void c_quaternion_init(c_quaternion_t *quaternion, float angle, float x, float y, float z)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle you want to rotate around the given axis

        :param x: The x component of your axis vector about which you want to

        rotate.
        :param y: The y component of your axis vector about which you want to

        rotate.
        :param z: The z component of your axis vector about which you want to

        rotate.

        Initializes a quaternion that rotates :c:data:`angle` degrees around the
        axis vector (:c:data:`x`, :c:data:`y`, :c:data:`z`). The axis vector does not need to be
        normalized.

        :returns:  A normalized, unit quaternion representing an orientation
        rotated :c:data:`angle` degrees around the axis vector (:c:data:`x`, :c:data:`y`, :c:data:`z`)







.. c:function:: void c_quaternion_init_from_angle_vector(c_quaternion_t *quaternion, float angle, const float *axis3f)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around :c:data:`axis3f`

        :param axis3f: your 3 component axis vector about which you want to rotate.


        Initializes a quaternion that rotates :c:data:`angle` degrees around the
        given :c:data:`axis` vector. The axis vector does not need to be
        normalized.

        :returns:  A normalized, unit quaternion representing an orientation
        rotated :c:data:`angle` degrees around the given :c:data:`axis` vector.







.. c:function:: void c_quaternion_init_identity(c_quaternion_t *quaternion)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`


        Initializes the quaternion with the canonical quaternion identity
        [1 (0, 0, 0)] which represents no rotation. Multiplying a
        quaternion with this identity leaves the quaternion unchanged.

        You might also want to consider using
        cg_get_static_identity_quaternion().







.. c:function:: void c_quaternion_init_from_array(c_quaternion_t *quaternion, const float *array)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param array: An array of 4 floats w,(x,y,z)


        Initializes a [w (x, y,z)] quaternion directly from an array of 4
        floats: [w,x,y,z].







.. c:function:: void c_quaternion_init_from_x_rotation(c_quaternion_t *quaternion, float angle)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around the x axis


        XXX: check which direction this rotates







.. c:function:: void c_quaternion_init_from_y_rotation(c_quaternion_t *quaternion, float angle)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around the y axis









.. c:function:: void c_quaternion_init_from_z_rotation(c_quaternion_t *quaternion, float angle)

        :param quaternion: An uninitialized :c:type:`c_quaternion_t`

        :param angle: The angle to rotate around the z axis









.. c:function:: void c_quaternion_init_from_euler(c_quaternion_t *quaternion, const c_euler_t *euler)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param euler: A :c:type:`c_euler_t` with which to initialize the quaternion








.. c:function:: void c_quaternion_init_from_matrix(c_quaternion_t *quaternion, const c_matrix_t *matrix)

        :param quaternion: A Quaternion

        :param matrix: A rotation matrix with which to initialize the quaternion


        Initializes a quaternion from a rotation matrix.






.. c:function:: _Bool c_quaternion_equal(const void *v1, const void *v2)

        :param v1: A :c:type:`c_quaternion_t`

        :param v2: A :c:type:`c_quaternion_t`


        Compares that all the components of quaternions :c:data:`a` and :c:data:`b` are
        equal.

        An epsilon value is not used to compare the float components, but
        the == operator is at least used so that 0 and -0 are considered
        equal.

        :returns:  ``true`` if the quaternions are equal else ``false``.







.. c:function:: c_quaternion_t *c_quaternion_copy(const c_quaternion_t *src)

        :param src: A :c:type:`c_quaternion_t`


        Allocates a new :c:type:`c_quaternion_t` on the stack and initializes it with
        the same values as :c:data:`src`.

        :returns:  A newly allocated :c:type:`c_quaternion_t` which should be freed
        using c_quaternion_free()







.. c:function:: void c_quaternion_free(c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`


        Frees a :c:type:`c_quaternion_t` that was previously allocated via
        c_quaternion_copy().







.. c:function:: float c_quaternion_get_rotation_angle(const c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`









.. c:function:: void c_quaternion_get_rotation_axis(const c_quaternion_t *quaternion, float *vector3)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param vector3: (out): an allocated 3-float array








.. c:function:: void c_quaternion_normalize(c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`









.. c:function:: float c_quaternion_dot_product(const c_quaternion_t *a, const c_quaternion_t *b)

        :param a: A :c:type:`c_quaternion_t`

        :param b: A :c:type:`c_quaternion_t`








.. c:function:: void c_quaternion_invert(c_quaternion_t *quaternion)

        :param quaternion: A :c:type:`c_quaternion_t`









.. c:function:: void c_quaternion_multiply(c_quaternion_t *result, const c_quaternion_t *left, const c_quaternion_t *right)

        :param result: The destination :c:type:`c_quaternion_t`

        :param left: The second :c:type:`c_quaternion_t` rotation to apply

        :param right: The first :c:type:`c_quaternion_t` rotation to apply


        This combines the rotations of two quaternions into :c:data:`result`. The
        operation is not commutative so the order is important; AxB
        != BxA. Clib follows the standard convention for quaternions here
        so the rotations are applied :c:data:`right` to :c:data:`left`. This is similar to the
        combining of matrices.


        .. note::
                It is possible to multiply the :c:data:`a` quaternion in-place, so
                :c:data:`result` can be equal to :c:data:`a` but can't be equal to :c:data:`b`.









.. c:function:: void c_quaternion_pow(c_quaternion_t *quaternion, float exponent)

        :param quaternion: A :c:type:`c_quaternion_t`

        :param exponent: the exponent









.. c:function:: void c_quaternion_slerp(c_quaternion_t *result, const c_quaternion_t *a, const c_quaternion_t *b, float t)

        :param result: The destination :c:type:`c_quaternion_t`

        :param a: The first :c:type:`c_quaternion_t`

        :param b: The second :c:type:`c_quaternion_t`

        :param t: The factor in the range [0,1] used to interpolate between

        quaternion :c:data:`a` and :c:data:`b`.

        Performs a spherical linear interpolation between two quaternions.

        Noteable properties:

        1.
        commutative: No

        2.
        constant velocity: Yes

        3.
        torque minimal (travels along the surface of the 4-sphere): Yes

        4.
        more expensive than c_quaternion_nlerp()








.. c:function:: void c_quaternion_nlerp(c_quaternion_t *result, const c_quaternion_t *a, const c_quaternion_t *b, float t)

        :param result: The destination :c:type:`c_quaternion_t`

        :param a: The first :c:type:`c_quaternion_t`

        :param b: The second :c:type:`c_quaternion_t`

        :param t: The factor in the range [0,1] used to interpolate between

        quaterion :c:data:`a` and :c:data:`b`.

        Performs a normalized linear interpolation between two quaternions.
        That is it does a linear interpolation of the quaternion components
        and then normalizes the result. This will follow the shortest arc
        between the two orientations (just like the slerp() function) but
        will not progress at a constant speed. Unlike slerp() nlerp is
        commutative which is useful if you are blending animations
        together. (I.e. nlerp (tmp, a, b) followed by nlerp (result, tmp,
        d) is the same as nlerp (tmp, a, d) followed by nlerp (result, tmp,
        b)). Finally nlerp is cheaper than slerp so it can be a good choice
        if you don't need the constant speed property of the slerp() function.

        Notable properties:

        1.
        commutative: Yes

        2.
        constant velocity: No

        3.
        torque minimal (travels along the surface of the 4-sphere): Yes

        4.
        faster than c_quaternion_slerp()








.. c:function:: void c_quaternion_squad(c_quaternion_t *result, const c_quaternion_t *prev, const c_quaternion_t *a, const c_quaternion_t *b, const c_quaternion_t *next, float t)

        :param result: The destination :c:type:`c_quaternion_t`

        :param prev: A :c:type:`c_quaternion_t` used before :c:data:`a`

        :param a: The first :c:type:`c_quaternion_t`

        :param b: The second :c:type:`c_quaternion_t`

        :param next: A :c:type:`c_quaternion_t` that will be used after :c:data:`b`

        :param t: The factor in the range [0,1] used to interpolate between

        quaternion :c:data:`a` and :c:data:`b`.






