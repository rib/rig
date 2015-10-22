.. _euler-api:

==================
 Euler
==================



.. highlight:: c

API for initializing and manipulating
euler angles.

Euler angles are a simple representation of a 3 dimensional
rotation; comprised of 3 ordered heading, pitch and roll rotations.
An important thing to understand is that the axis of rotation
belong to the object being rotated and so they also rotate as each
of the heading, pitch and roll rotations are applied.

One way to consider euler angles is to imagine controlling an
aeroplane, where you first choose a heading (Such as flying south
east), then you set the pitch (such as 30 degrees to take off) and
then you might set a roll, by dipping the left, wing as you prepare
to turn.

They have some advantages and limitations that it helps to be
aware of:

Advantages:

1.
Easy to understand and use, compared to quaternions and matrices,
so may be a good choice for a user interface.

2.
Efficient storage, needing only 3 components any rotation can be
represented.



Disadvantages:

1.
Aliasing: it's possible to represent some rotations with multiple
different heading, pitch and roll rotations.

2.
They can suffer from a problem called Gimbal Lock. A good
explanation of this can be seen on wikipedia here:
http://en.wikipedia.org/wiki/Gimbal_lock but basically two
of the axis of rotation may become aligned and so you loose a
degree of freedom. For example a pitch of +-90° would mean that
heading and bank rotate around the same axis.

3.
If you use euler angles to orient something in 3D space and try to
transition between orientations by interpolating the component
angles you probably wont get the transitions you expect as they may
not follow the shortest path between the two orientations.

4.
There's no standard to what order the component axis rotations are
applied. The most common convention seems to be what we do in clib
with heading (y-axis), pitch (x-axis) and then roll (z-axis), but
other software might apply x-axis, y-axis then z-axis or any other
order so you need to consider this if you are accepting euler
rotations from some other software. Other software may also use
slightly different aeronautical terms, such as "yaw" instead of
"heading" or "bank" instead of "roll".



To minimize the aliasing issue we may refer to "Canonical Euler"
angles where heading and roll are restricted to +- 180° and pitch is
restricted to +- 90°. If pitch is +- 90° bank is set to 0°.

Quaternions don't suffer from Gimbal Lock and they can be nicely
interpolated between, their disadvantage is that they don't have an
intuitive representation.

A common practice is to accept angles in the intuitive Euler form
and convert them to quaternions internally to avoid Gimbal Lock and
handle interpolations. See c_quaternion_init_from_euler().



.. c:type:: c_euler_t

        .. c:member:: heading

           Angle to rotate around an object's y axis

        .. c:member:: pitch

           Angle to rotate around an object's x axis

        .. c:member:: roll

         Angle to rotate around an object's z axis


        Represents an ordered rotation first of @heading degrees around an
        object's y axis, then @pitch degrees around an object's x axis and
        finally @roll degrees around an object's z axis.


        .. note::
                It's important to understand the that axis are associated
                with the object being rotated, so the axis also rotate in sequence
                with the rotations being applied.



        The members of a :c:type:`c_euler_t` can be initialized, for example, with
        c_euler_init() and c_euler_init_from_quaternion ().

        You may also want to look at c_quaternion_init_from_euler() if
        you want to do interpolation between 3d rotations.







.. c:function:: void c_euler_init(c_euler_t *euler, float heading, float pitch, float roll)

        :param euler: The :c:type:`c_euler_t` angle to initialize

        :param heading: Angle to rotate around an object's y axis

        :param pitch: Angle to rotate around an object's x axis

        :param roll: Angle to rotate around an object's z axis


        Initializes :c:data:`euler` to represent a rotation of :c:data:`x_angle` degrees
        around the x axis, then :c:data:`y_angle` degrees around the y_axis and
        :c:data:`z_angle` degrees around the z axis.







.. c:function:: void c_euler_init_from_matrix(c_euler_t *euler, const c_matrix_t *matrix)

        :param euler: The :c:type:`c_euler_t` angle to initialize

        :param matrix: A :c:type:`c_matrix_t` containing a rotation, but no scaling,
                 mirroring or skewing.


        Extracts a euler rotation from the given :c:data:`matrix` and
        initializses :c:data:`euler` with the component x, y and z rotation angles.






.. c:function:: void c_euler_init_from_quaternion(c_euler_t *euler, const c_quaternion_t *quaternion)

        :param euler: The :c:type:`c_euler_t` angle to initialize

        :param quaternion: A :c:type:`c_euler_t` with the rotation to initialize with


        Initializes a :c:data:`euler` rotation with the equivalent rotation
        represented by the given :c:data:`quaternion`.






.. c:function:: _Bool c_euler_equal(const void *v1, const void *v2)

        :param v1: The first euler angle to compare

        :param v2: The second euler angle to compare


        Compares the two given euler angles :c:data:`v1` and :c:data:`v1` and it they are
        equal returns ``true`` else ``false``.


        .. note::
                This function only checks that all three components rotations
                are numerically equal, it does not consider that some rotations
                can be represented with different component rotations



        :returns:  ``true`` if :c:data:`v1` and :c:data:`v2` are equal else ``false``.






.. c:function:: c_euler_t *c_euler_copy(const c_euler_t *src)

        :param src: A :c:type:`c_euler_t` to copy


        Allocates a new :c:type:`c_euler_t` and initilizes it with the component
        angles of :c:data:`src`. The newly allocated euler should be freed using
        c_euler_free().

        :returns:  A newly allocated :c:type:`c_euler_t`






.. c:function:: void c_euler_free(c_euler_t *euler)

        :param euler: A :c:type:`c_euler_t` allocated via c_euler_copy()


        Frees a :c:type:`c_euler_t` that was previously allocated using
        c_euler_copy().







.. c:function:: void c_euler_init(c_euler_t *euler, float heading, float pitch, float roll)

        :param euler: The :c:type:`c_euler_t` angle to initialize

        :param heading: Angle to rotate around an object's y axis

        :param pitch: Angle to rotate around an object's x axis

        :param roll: Angle to rotate around an object's z axis


        Initializes :c:data:`euler` to represent a rotation of :c:data:`x_angle` degrees
        around the x axis, then :c:data:`y_angle` degrees around the y_axis and
        :c:data:`z_angle` degrees around the z axis.







.. c:function:: void c_euler_init_from_matrix(c_euler_t *euler, const c_matrix_t *matrix)

        :param euler: The :c:type:`c_euler_t` angle to initialize

        :param matrix: A :c:type:`c_matrix_t` containing a rotation, but no scaling,
                 mirroring or skewing.


        Extracts a euler rotation from the given :c:data:`matrix` and
        initializses :c:data:`euler` with the component x, y and z rotation angles.






.. c:function:: void c_euler_init_from_quaternion(c_euler_t *euler, const c_quaternion_t *quaternion)

        :param euler: The :c:type:`c_euler_t` angle to initialize

        :param quaternion: A :c:type:`c_euler_t` with the rotation to initialize with


        Initializes a :c:data:`euler` rotation with the equivalent rotation
        represented by the given :c:data:`quaternion`.






.. c:function:: _Bool c_euler_equal(const void *v1, const void *v2)

        :param v1: The first euler angle to compare

        :param v2: The second euler angle to compare


        Compares the two given euler angles :c:data:`v1` and :c:data:`v1` and it they are
        equal returns ``true`` else ``false``.


        .. note::
                This function only checks that all three components rotations
                are numerically equal, it does not consider that some rotations
                can be represented with different component rotations



        :returns:  ``true`` if :c:data:`v1` and :c:data:`v2` are equal else ``false``.






.. c:function:: c_euler_t *c_euler_copy(const c_euler_t *src)

        :param src: A :c:type:`c_euler_t` to copy


        Allocates a new :c:type:`c_euler_t` and initilizes it with the component
        angles of :c:data:`src`. The newly allocated euler should be freed using
        c_euler_free().

        :returns:  A newly allocated :c:type:`c_euler_t`






.. c:function:: void c_euler_free(c_euler_t *euler)

        :param euler: A :c:type:`c_euler_t` allocated via c_euler_copy()


        Frees a :c:type:`c_euler_t` that was previously allocated using
        c_euler_copy().





