.. _vectors-api:

==================
 Vectors
==================




.. highlight:: c

API for handling single precision float vectors.

This exposes a utility API that can be used for basic manipulation of 3
component float vectors.





.. c:function:: void c_vector3_init(float *vector, float x, float y, float z)

        :param vector: The 3 component vector you want to initialize

        :param x: The x component

        :param y: The y component

        :param z: The z component


        Initializes a 3 component, single precision float vector which can
        then be manipulated with the c_vector convenience APIs. Vectors
        can also be used in places where a "point" is often desired.






.. c:function:: void c_vector3_init_zero(float *vector)

        :param vector: The 3 component vector you want to initialize


        Initializes a 3 component, single precision float vector with zero
        for each component.






.. c:function:: _Bool c_vector3_equal(const void *v1, const void *v2)

        :param v1: The first 3 component vector you want to compare

        :param v2: The second 3 component vector you want to compare


        Compares the components of two vectors and returns true if they are
        the same.

        The comparison of the components is done with the '==' operator
        such that -0 is considered equal to 0, but otherwise there is no
        fuzziness such as an epsilon to consider vectors that are
        essentially identical except for some minor precision error
        differences due to the way they have been manipulated.

        :returns:  true if the vectors are equal else false.






.. c:function:: _Bool c_vector3_equal_with_epsilon(const float *vector0, const float *vector1, float epsilon)

        :param vector0: The first 3 component vector you want to compare

        :param vector1: The second 3 component vector you want to compare

        :param epsilon: The allowable difference between components to still be
                  considered equal


        Compares the components of two vectors using the given epsilon and
        returns true if they are the same, using an internal epsilon for
        comparing the floats.

        Each component is compared against the epsilon value in this way:
        ::

          if (fabsf (vector0->x - vector1->x) < epsilon)


        :returns:  true if the vectors are equal else false.






.. c:function:: float *c_vector3_copy(const float *vector)

        :param vector: The 3 component vector you want to copy


        Allocates a new 3 component float vector on the heap initializing
        the components from the given :c:data:`vector` and returns a pointer to the
        newly allocated vector. You should free the memory using
        c_vector3_free()

        :returns:  A newly allocated 3 component float vector






.. c:function:: void c_vector3_free(float *vector)

        :param vector: The 3 component you want to free


        Frees a 3 component vector that was previously allocated with
        c_vector3_copy()






.. c:function:: void c_vector3_invert(float *vector)

        :param vector: The 3 component vector you want to manipulate


        Inverts/negates all the components of the given :c:data:`vector`.






.. c:function:: void c_vector3_add(float *result, const float *a, const float *b)

        :param result: Where you want the result written

        :param a: The first vector operand

        :param b: The second vector operand


        Adds each of the corresponding components in vectors :c:data:`a` and :c:data:`b`
        storing the results in :c:data:`result`.






.. c:function:: void c_vector3_subtract(float *result, const float *a, const float *b)

        :param result: Where you want the result written

        :param a: The first vector operand

        :param b: The second vector operand


        Subtracts each of the corresponding components in vector :c:data:`b` from
        :c:data:`a` storing the results in :c:data:`result`.






.. c:function:: void c_vector3_multiply_scalar(float *vector, float scalar)

        :param vector: The 3 component vector you want to manipulate

        :param scalar: The scalar you want to multiply the vector components by


        Multiplies each of the :c:data:`vector` components by the given scalar.






.. c:function:: void c_vector3_divide_scalar(float *vector, float scalar)

        :param vector: The 3 component vector you want to manipulate

        :param scalar: The scalar you want to divide the vector components by


        Divides each of the :c:data:`vector` components by the given scalar.






.. c:function:: void c_vector3_normalize(float *vector)

        :param vector: The 3 component vector you want to manipulate


        Updates the vector so it is a "unit vector" such that the
        :c:data:`vector`<!-- -->s magnitude or length is equal to 1.


        .. note::
                It's safe to use this function with the [0, 0, 0] vector, it will not
                try to divide components by 0 (its norm) and will leave the vector
                untouched.








.. c:function:: float c_vector3_magnitude(const float *vector)

        :param vector: The 3 component vector you want the magnitude for


        Calculates the scalar magnitude or length of :c:data:`vector`.

        :returns:  The magnitude of :c:data:`vector`.






.. c:function:: void c_vector3_cross_product(float *result, const float *u, const float *v)

        :param result: Where you want the result written

        :param u: Your first 3 component vector

        :param v: Your second 3 component vector


        Calculates the cross product between the two vectors :c:data:`u` and :c:data:`v`.

        The cross product is a vector perpendicular to both :c:data:`u` and :c:data:`v`. This
        can be useful for calculating the normal of a polygon by creating
        two vectors in its plane using the polygons vertices and taking
        their cross product.

        If the two vectors are parallel then the cross product is 0.

        You can use a right hand rule to determine which direction the
        perpendicular vector will point: If you place the two vectors tail,
        to tail and imagine grabbing the perpendicular line that extends
        through the common tail with your right hand such that you fingers
        rotate in the direction from :c:data:`u` to :c:data:`v` then the resulting vector
        points along your extended thumb.

        :returns:  The cross product between two vectors :c:data:`u` and :c:data:`v`.






.. c:function:: float c_vector3_dot_product(const float *a, const float *b)

        :param a: Your first 3 component vector

        :param b: Your second 3 component vector


        Calculates the dot product of the two 3 component vectors. This
        can be used to determine the magnitude of one vector projected onto
        another. (for example a surface normal)

        For example if you have a polygon with a given normal vector and
        some other point for which you want to calculate its distance from
        the polygon, you can create a vector between one of the polygon
        vertices and that point and use the dot product to calculate the
        magnitude for that vector but projected onto the normal of the
        polygon. This way you don't just get the distance from the point to
        the edge of the polygon you get the distance from the point to the
        nearest part of the polygon.


        .. note::
                If you don't use a unit length normal in the above example
                then you would then also have to divide the result by the magnitude
                of the normal



        The dot product is calculated as:
        ::

         (a->x * b->x + a->y * b->y + a->z * b->z)


        For reference, the dot product can also be calculated from the
        angle between two vectors as:
        ::

         |a||b|cos𝜃


        :returns:  The dot product of two vectors.






.. c:function:: float c_vector3_distance(const float *a, const float *b)

        :param a: The first point

        :param b: The second point


        If you consider the two given vectors as (x,y,z) points instead
        then this will compute the distance between those two points.

        :returns:  The distance between two points given as 3 component
                 vectors.






.. c:function:: void c_vector3_init(float *vector, float x, float y, float z)

        :param vector: The 3 component vector you want to initialize

        :param x: The x component

        :param y: The y component

        :param z: The z component


        Initializes a 3 component, single precision float vector which can
        then be manipulated with the c_vector convenience APIs. Vectors
        can also be used in places where a "point" is often desired.






.. c:function:: void c_vector3_init_zero(float *vector)

        :param vector: The 3 component vector you want to initialize


        Initializes a 3 component, single precision float vector with zero
        for each component.






.. c:function:: _Bool c_vector3_equal(const void *v1, const void *v2)

        :param v1: The first 3 component vector you want to compare

        :param v2: The second 3 component vector you want to compare


        Compares the components of two vectors and returns true if they are
        the same.

        The comparison of the components is done with the '==' operator
        such that -0 is considered equal to 0, but otherwise there is no
        fuzziness such as an epsilon to consider vectors that are
        essentially identical except for some minor precision error
        differences due to the way they have been manipulated.

        :returns:  true if the vectors are equal else false.






.. c:function:: _Bool c_vector3_equal_with_epsilon(const float *vector0, const float *vector1, float epsilon)

        :param vector0: The first 3 component vector you want to compare

        :param vector1: The second 3 component vector you want to compare

        :param epsilon: The allowable difference between components to still be
                  considered equal


        Compares the components of two vectors using the given epsilon and
        returns true if they are the same, using an internal epsilon for
        comparing the floats.

        Each component is compared against the epsilon value in this way:
        ::

          if (fabsf (vector0->x - vector1->x) < epsilon)


        :returns:  true if the vectors are equal else false.






.. c:function:: float *c_vector3_copy(const float *vector)

        :param vector: The 3 component vector you want to copy


        Allocates a new 3 component float vector on the heap initializing
        the components from the given :c:data:`vector` and returns a pointer to the
        newly allocated vector. You should free the memory using
        c_vector3_free()

        :returns:  A newly allocated 3 component float vector






.. c:function:: void c_vector3_free(float *vector)

        :param vector: The 3 component you want to free


        Frees a 3 component vector that was previously allocated with
        c_vector3_copy()






.. c:function:: void c_vector3_invert(float *vector)

        :param vector: The 3 component vector you want to manipulate


        Inverts/negates all the components of the given :c:data:`vector`.






.. c:function:: void c_vector3_add(float *result, const float *a, const float *b)

        :param result: Where you want the result written

        :param a: The first vector operand

        :param b: The second vector operand


        Adds each of the corresponding components in vectors :c:data:`a` and :c:data:`b`
        storing the results in :c:data:`result`.






.. c:function:: void c_vector3_subtract(float *result, const float *a, const float *b)

        :param result: Where you want the result written

        :param a: The first vector operand

        :param b: The second vector operand


        Subtracts each of the corresponding components in vector :c:data:`b` from
        :c:data:`a` storing the results in :c:data:`result`.






.. c:function:: void c_vector3_multiply_scalar(float *vector, float scalar)

        :param vector: The 3 component vector you want to manipulate

        :param scalar: The scalar you want to multiply the vector components by


        Multiplies each of the :c:data:`vector` components by the given scalar.






.. c:function:: void c_vector3_divide_scalar(float *vector, float scalar)

        :param vector: The 3 component vector you want to manipulate

        :param scalar: The scalar you want to divide the vector components by


        Divides each of the :c:data:`vector` components by the given scalar.






.. c:function:: void c_vector3_normalize(float *vector)

        :param vector: The 3 component vector you want to manipulate


        Updates the vector so it is a "unit vector" such that the
        :c:data:`vector`<!-- -->s magnitude or length is equal to 1.


        .. note::
                It's safe to use this function with the [0, 0, 0] vector, it will not
                try to divide components by 0 (its norm) and will leave the vector
                untouched.








.. c:function:: float c_vector3_magnitude(const float *vector)

        :param vector: The 3 component vector you want the magnitude for


        Calculates the scalar magnitude or length of :c:data:`vector`.

        :returns:  The magnitude of :c:data:`vector`.






.. c:function:: void c_vector3_cross_product(float *result, const float *u, const float *v)

        :param result: Where you want the result written

        :param u: Your first 3 component vector

        :param v: Your second 3 component vector


        Calculates the cross product between the two vectors :c:data:`u` and :c:data:`v`.

        The cross product is a vector perpendicular to both :c:data:`u` and :c:data:`v`. This
        can be useful for calculating the normal of a polygon by creating
        two vectors in its plane using the polygons vertices and taking
        their cross product.

        If the two vectors are parallel then the cross product is 0.

        You can use a right hand rule to determine which direction the
        perpendicular vector will point: If you place the two vectors tail,
        to tail and imagine grabbing the perpendicular line that extends
        through the common tail with your right hand such that you fingers
        rotate in the direction from :c:data:`u` to :c:data:`v` then the resulting vector
        points along your extended thumb.

        :returns:  The cross product between two vectors :c:data:`u` and :c:data:`v`.






.. c:function:: float c_vector3_dot_product(const float *a, const float *b)

        :param a: Your first 3 component vector

        :param b: Your second 3 component vector


        Calculates the dot product of the two 3 component vectors. This
        can be used to determine the magnitude of one vector projected onto
        another. (for example a surface normal)

        For example if you have a polygon with a given normal vector and
        some other point for which you want to calculate its distance from
        the polygon, you can create a vector between one of the polygon
        vertices and that point and use the dot product to calculate the
        magnitude for that vector but projected onto the normal of the
        polygon. This way you don't just get the distance from the point to
        the edge of the polygon you get the distance from the point to the
        nearest part of the polygon.


        .. note::
                If you don't use a unit length normal in the above example
                then you would then also have to divide the result by the magnitude
                of the normal



        The dot product is calculated as:
        ::

         (a->x * b->x + a->y * b->y + a->z * b->z)


        For reference, the dot product can also be calculated from the
        angle between two vectors as:
        ::

         |a||b|cos𝜃


        :returns:  The dot product of two vectors.






.. c:function:: float c_vector3_distance(const float *a, const float *b)

        :param a: The first point

        :param b: The second point


        If you consider the two given vectors as (x,y,z) points instead
        then this will compute the distance between those two points.

        :returns:  The distance between two points given as 3 component
                 vectors.




