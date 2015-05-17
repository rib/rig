#include <config.h>

#include <stdlib.h>

#include <clib.h>
#include <cmodule.h>

#include <test-fixtures/test.h>

int
main(int argc, char **argv)
{
  c_module_t *main_module;
  const test_t *unit_test;
  int i;

  if (argc != 2) {
      c_printerr("usage %s TEST\n", argv[0]);
      exit(1);
  }

  /* Just for convenience in case people try passing the wrapper
   * filenames for the UNIT_TEST argument we normalize '-' characters
   * to '_' characters... */
  for (i = 0; argv[1][i]; i++) {
      if (argv[1][i] == '-')
          argv[1][i] = '_';
  }

  main_module = c_module_open(NULL /* use main module */);

  if (!c_module_symbol(main_module, argv[1], (void **) &unit_test)) {
      c_printerr("Unknown test name \"%s\"\n", argv[1]);
      return 1;
  }

  unit_test->run();

  return 0;
}
