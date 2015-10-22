#!/usr/bin/env python

import glob
import platform
import os
import subprocess
import sys

try:
  import multiprocessing.synchronize
  gyp_parallel_support = True
except ImportError:
  gyp_parallel_support = False


CC = os.environ.get('CC', 'cc')
script_dir = os.path.dirname(__file__)
clib_root = os.path.normpath(script_dir)
output_dir = os.path.join(os.path.abspath(clib_root), 'out')

sys.path.insert(0, os.path.join(clib_root, 'build', 'gyp', 'pylib'))
try:
  import gyp
except ImportError:
  print('You need to install gyp in build/gyp first. See the README.')
  sys.exit(42)


def host_arch():
  machine = platform.machine()
  if machine == 'i386': return 'ia32'
  if machine == 'x86_64': return 'x64'
  if machine.startswith('arm'): return 'arm'
  if machine.startswith('mips'): return 'mips'
  return machine  # Return as-is and hope for the best.


def run_gyp(args):
  rc = gyp.main(args)
  if rc != 0:
    print 'Error running GYP'
    sys.exit(rc)


if __name__ == '__main__':
  args = sys.argv[1:]

  args.append(os.path.join(clib_root, 'clib.gyp'))

  options_fn = os.path.join(clib_root, 'options.gypi')
  if os.path.exists(options_fn):
    args.extend(['-I', options_fn])

  args.append('--depth=' + clib_root)

  # There's a bug with windows which doesn't allow this feature.
  if sys.platform != 'win32':
    if '-f' not in args:
      args.extend('-f ninja'.split())
    if 'eclipse' not in args and 'ninja' not in args:
      args.extend(['-Goutput_dir=' + output_dir])
      args.extend(['--generator-output', output_dir])

  if not any(a.startswith('-Dhost_arch=') for a in args):
    args.append('-Dhost_arch=%s' % host_arch())

  if not any(a.startswith('-Dtarget_arch=') for a in args):
    args.append('-Dtarget_arch=%s' % host_arch())

  if sys.platform == 'win32':
    # we force vs 2010 over 2008 which would otherwise be the default for gyp
    if not os.environ.get('GYP_MSVS_VERSION'):
      os.environ['GYP_MSVS_VERSION'] = '2010'

  gyp_args = list(args)
  print "Running gyp:"
  print "$ gyp " + " ".join(gyp_args)
  run_gyp(gyp_args)

  if "ninja" in args:
      print "\n\nReady to build:"
      print "$ ninja -C ./out/Debug"
