#!/usr/bin/env python

import glob
import platform
import os
import subprocess
import sys
import argparse

try:
    import multiprocessing.synchronize
    gyp_parallel_support = True
except ImportError:
    gyp_parallel_support = False


CC = os.environ.get('CC', 'cc')
script_dir = os.path.dirname(__file__)
cglib_root = os.path.normpath(script_dir)
output_dir = os.path.join(os.path.abspath(cglib_root), 'out')

sys.path.insert(0, os.path.join(cglib_root, 'build', 'gyp', 'pylib'))
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
    return machine  # Return as-is and hope for the best.

def pkg_config_exists(pkg_name):
    try:
        subprocess.check_call(["pkg-config", "--exists", pkg_name])
        return True
    except:
        return False

def pkg_config_get(pkg_name, arg):
    return subprocess.check_output(["pkg-config", arg, pkg_name])

def run_gyp(args):
    rc = gyp.main(args)
    if rc != 0:
        print 'Error running GYP'
        sys.exit(rc)

def add_option(parser, name, opt):
    if opt["enabled"] and "pkg-config" in opt and platform.system() == "Linux":
        opt["enabled"] = pkg_config_exists(opt["pkg-config"])
    if not opt["enabled"]:
        enable_help = "Enable " + opt["help"]
        disable_help = argparse.SUPPRESS
    else:
        enable_help = argparse.SUPPRESS
        disable_help = "Disable " + opt["help"]
    hyphen_name = name.replace('_', '-')
    parser.add_argument("--enable-" + hyphen_name, dest="enable_" + name, action="store_true", default=opt["enabled"], help=enable_help)
    parser.add_argument("--disable-" + hyphen_name, dest="enable_" + name, action="store_false", default=not opt["enabled"], help=disable_help)


parser = argparse.ArgumentParser()

parser.add_argument("--prefix",
                    help="install architecture-independent files in PREFIX")
parser.add_argument("--host",
                    help="cross-compile to build programs to run on HOST")
parser.add_argument("--enable-shared", action='store_true',
                    help="build shared libraries")

subst = {
        "CG_DEFINES": "",
}
enabled = {}
options = {
        "uv": {
            "help": "libuv support",
            "enabled": True,
            "pkg-config": "libuv",
            "public_defines": { "CG_HAS_UV_SUPPORT" }
        },
        "glib": {
            "help": "GLib support",
            "enabled": False,
            "pkg-config": "glib-2.0",
            "public_defines": { "CG_HAS_GLIB_SUPPORT" }
        },
        "x11": {
            "enabled": True,
            "pkg-config": "x11",
            "public_defines": { "CG_HAS_XLIB_SUPPORT", "CG_HAS_X11_SUPPORT" }
        },
        "glx": {
            "requires": { "x11" }, #TODO check
            "help": "GLX support",
            "enabled": True,
            "pkg-config": "gl",
            "public_defines": { "CG_HAS_GLX_SUPPORT", "CG_HAS_GL_SUPPORT", "CG_HAS_XLIB_SUPPORT", "CG_HAS_X11_SUPPORT" },
            "conditionals": { "enable_glx", "enable_gl" }
        },
        "egl": {
            "help": "EGL support",
            "enabled": True,
            "pkg-config": "egl",
            "public_defines": { "CG_HAS_EGL_SUPPORT" },
            "conditionals": { "enable_gl" }
        },
        "egl_wayland": {
            "help": "Wayland EGL support",
            "enabled": True,
            "pkg-config": "wayland-egl",
            "public_defines": { "CG_HAS_EGL_PLATFORM_WAYLAND_SUPPORT" },
        },
        "egl_kms": {
            "help": "Kernel Mode Setting EGL support",
            "enabled": True,
            "pkg-config": "egl gbm libdrm",
            "public_defines": { "CG_HAS_EGL_PLATFORM_KMS_SUPPORT" },
        },
        "egl_xlib": {
            "help": "XLib EGL support",
            "enabled": True,
            "pkg-config": "egl x11",
            "public_defines": { "CG_HAS_EGL_PLATFORM_XLIB_SUPPORT" },
        },
        "sdl": {
            "help": "SDL 2.0 support",
            "enabled": True,
            "pkg-config": "sdl2",
            "public_defines": { "CG_HAS_SDL_SUPPORT" },
        },
        "gles2": {
            "help": "GLES 2.0 support",
            "enabled": True,
            "pkg-config": "glesv2",
            "public_defines": { "CG_HAS_GLES2_SUPPORT" },
        },
}

output = {
        "cglib/cg-defines.h",
}

for name, opt in options.items():
    if "help" in opt:
        add_option(parser, name, opt)

opt_args = parser.parse_args()
opt_args_dict = vars(opt_args)

gyp_args = []


for name, opt in options.items():
    if "enable_" + name in opt_args_dict:
        if opt_args_dict["enable_" + name]:
            if "pkg-config" in opt:
                if not pkg_config_exists(opt["pkg-config"]):
                    sys.exit(opt["pkg-config"] + " required for enabling " + name + " support is missing")
            opt["enabled"] = True
            enabled[name] = opt
    else:
        if opt["enabled"]:
            enabled[name] = opt

for name, opt in enabled.items():
        cflags = pkg_config_get(opt["pkg-config"], "--cflags")
        tokens = cflags.split()
        if len(tokens):
            includes = []
            defines = []
            for tok in tokens:
                if tok[:2] == "-I":
                    includes.append(tok[2:])
                elif tok[:2] == "-D":
                    defines.append(tok[2:])
            if len(includes):
                opt["includes"] = includes
            if len(defines):
                opt["defines"] = defines


gyp_config = open("config.gypi", "w")

gyp_config.write("{\n")
gyp_config.write("  'variables': {\n")

for name, opt in options.items():
    gyp_config.write("    'enable_" + name + "%': '0',\n")

gyp_config.write("  },\n")

gyp_config.write("""
  'target_defaults': {
    'include_dirs': [
""")

for name, opt in enabled.items():
    if "includes" in opt:
        gyp_config.write("      # " + name + " includes...\n")
        for include in opt["includes"]:
            gyp_config.write("      '" + include + "',\n")

gyp_config.write("""
    ],
    'defines': [
""")
for name, opt in enabled.items():
    if "defines" in opt:
        gyp_config.write("      # " + name + " defines...\n")
        for define in opt["defines"]:
            gyp_config.write("      '" + define + "',\n")

gyp_config.write("""
    ]
  }
""")
gyp_config.write("}\n")

for name, opt in enabled.items():
    if "public_defines" in opt:
        defines = opt["public_defines"]
        for define in defines:
            subst["CG_DEFINES"] = subst["CG_DEFINES"] + "#define " + define + "\n"

gyp_args.append(os.path.join(cglib_root, 'cglib.gyp'))

gyp_args.extend(['-I', "config.gypi"])

options_fn = os.path.join(cglib_root, 'options.gypi')
if os.path.exists(options_fn):
    gyp_args.extend(['-I', options_fn])

gyp_args.append('--depth=' + cglib_root)

if opt_args.enable_shared:
    gyp_args.append('-Dcglib_library=shared_library')

# There's a bug with windows which doesn't allow this feature.
if sys.platform != 'win32':
    if '-f' not in gyp_args:
        gyp_args.extend('-f ninja'.split())
    if 'eclipse' not in gyp_args and 'ninja' not in gyp_args:
        gyp_args.extend(['-Goutput_dir=' + output_dir])
        gyp_args.extend(['--generator-output', output_dir])

if not any(a.startswith('-Dhost_arch=') for a in gyp_args):
    gyp_args.append('-Dhost_arch=%s' % host_arch())

if not any(a.startswith('-Dtarget_arch=') for a in gyp_args):
    gyp_args.append('-Dtarget_arch=%s' % host_arch())

if sys.platform == 'win32':
    # Override the Gyp default and require a half-modern C compiler...
    if not os.environ.get('GYP_MSVS_VERSION'):
        os.environ['GYP_MSVS_VERSION'] = '2015'

if opt_args.host and platform.system() == "Linux":
    if "emscripten" in opt_args.host:
        gyp_args.append('-DOS=emscripten')

for name in enabled:
    gyp_args.append("-Denable_" + name)


gyp_config.close()

for file in output:
    file_contents = None

    with open(file + ".in", 'r') as fp:
        file_contents = fp.read()

    for key, val in subst.items():
        file_contents = file_contents.replace("@" + key + "@", val)

    try:
        fp = open(file, 'r')
        current_contents = fp.read()
    except:
        current_contents = ""
    finally:
        fp.close()

    if current_contents == file_contents:
        print(file + " unchanged")
    else:
        with open(file, 'w') as fp:
            print("creating " + file)
            fp.write(file_contents)
print("")

gyp_args = list(gyp_args)
print "Running gyp:"
print "$ gyp " + " ".join(gyp_args)
run_gyp(gyp_args)

if "ninja" in gyp_args:
    print "\n\nReady to build:"
    print "$ ninja -C ./out/Debug"
