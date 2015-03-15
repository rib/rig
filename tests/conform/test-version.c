#include "config.h"

#include <cogl/cogl.h>

#include "test-cg-fixtures.h"

/* So we can use _C_STATIC_ASSERT we include the internal
 * cogl-util.h header. Since internal headers explicitly guard against
 * applications including them directly instead of including
 * <cogl/cogl.h> we define __CG_H_INSIDE__ here to subvert those
 * guards in this case... */
#define __CG_H_INSIDE__
#include <cogl/cogl-util.h>
#undef __CG_H_INSIDE__

_C_STATIC_ASSERT (CG_VERSION_ENCODE (CG_VERSION_MAJOR,
                                          CG_VERSION_MINOR,
                                          CG_VERSION_MICRO) ==
                     CG_VERSION,
                     "The pre-encoded Cogl version does not match the version "
                     "encoding macro");

_C_STATIC_ASSERT (CG_VERSION_GET_MAJOR (CG_VERSION_ENCODE (100,
                                                                  200,
                                                                  300)) ==
                     100,
                     "Getting the major component out of a encoded version "
                     "does not work");
_C_STATIC_ASSERT (CG_VERSION_GET_MINOR (CG_VERSION_ENCODE (100,
                                                                  200,
                                                                  300)) ==
                     200,
                     "Getting the minor component out of a encoded version "
                     "does not work");
_C_STATIC_ASSERT (CG_VERSION_GET_MICRO (CG_VERSION_ENCODE (100,
                                                                  200,
                                                                  300)) ==
                     300,
                     "Getting the micro component out of a encoded version "
                     "does not work");

_C_STATIC_ASSERT (CG_VERSION_CHECK (CG_VERSION_MAJOR,
                                         CG_VERSION_MINOR,
                                         CG_VERSION_MICRO),
                     "Checking the Cogl version against the current version "
                     "does not pass");
_C_STATIC_ASSERT (!CG_VERSION_CHECK (CG_VERSION_MAJOR,
                                          CG_VERSION_MINOR,
                                          CG_VERSION_MICRO + 1),
                     "Checking the Cogl version against a later micro version "
                     "should not pass");
_C_STATIC_ASSERT (!CG_VERSION_CHECK (CG_VERSION_MAJOR,
                                          CG_VERSION_MINOR + 1,
                                          CG_VERSION_MICRO),
                     "Checking the Cogl version against a later minor version "
                     "should not pass");
_C_STATIC_ASSERT (!CG_VERSION_CHECK (CG_VERSION_MAJOR + 1,
                                          CG_VERSION_MINOR,
                                          CG_VERSION_MICRO),
                     "Checking the Cogl version against a later major version "
                     "should not pass");

_C_STATIC_ASSERT (CG_VERSION_CHECK (CG_VERSION_MAJOR - 1,
                                         CG_VERSION_MINOR,
                                         CG_VERSION_MICRO),
                     "Checking the Cogl version against a older major version "
                     "should pass");

void
test_version (void)
{
  const char *version = c_strdup_printf ("version = %i.%i.%i",
                                         CG_VERSION_MAJOR,
                                         CG_VERSION_MINOR,
                                         CG_VERSION_MICRO);

  c_assert_cmpstr (version, ==, "version = " CG_VERSION_STRING);

  if (test_verbose ())
    c_print ("OK\n");
}

