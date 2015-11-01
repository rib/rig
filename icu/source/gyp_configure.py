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
icu_root = os.path.normpath(script_dir)
output_dir = os.path.join(os.path.abspath(icu_root), 'out')

sys.path.insert(0, os.path.join(icu_root, 'build', 'gyp', 'pylib'))
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

def pkg_config_variable(pkg_name, variable):
    return subprocess.check_output(["pkg-config", "--variable=" + variable, pkg_name]).strip()

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
                    help="install architecture-independent files in PREFIX",
                    default="/usr/local")
parser.add_argument("--host",
                    help="cross-compile to build programs to run on HOST")
parser.add_argument("--enable-shared", action='store_true',
                    help="build shared libraries")

subst = {
}
enabled = {}
options = {
        "debug": {
            "help": "debug support",
            "enabled": True,
            "defines": { "U_DEBUG=1" }
        },
}

output = {
}

for name, opt in options.items():
    if "help" in opt:
        add_option(parser, name, opt)
    if "defines" not in opt:
        opt["defines"] = []
    if "includes" not in opt:
        opt["includes"] = []
    if "ldflags" not in opt:
        opt["ldflags"] = []

opt_args = parser.parse_args()
opt_args_dict = vars(opt_args)

gyp_args = []

#options['_']["defines"].append("ICU_DATA_DIR=\"" + pkg_config_variable("icu-uc", "prefix") + "/share\"")

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
    if "pkg-config" in opt:
        cflags = pkg_config_get(opt["pkg-config"], "--cflags")
        tokens = cflags.split()
        if len(tokens):
            for tok in tokens:
                if tok[:2] == "-I":
                    opt["includes"].append(tok[2:])
                elif tok[:2] == "-D":
                    opt["defines"].append(tok[2:])
        ldflags = pkg_config_get(opt["pkg-config"], "--libs")
        tokens = ldflags.split()
        if len(tokens):
            for tok in tokens:
                opt["ldflags"].append(tok)


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
    ],
    'ldflags': [
""")

for name, opt in enabled.items():
    if "ldflags" in opt:
        gyp_config.write("      # " + name + " ldflags...\n")
        for flag in opt["ldflags"]:
            gyp_config.write("      '" + flag + "',\n")

gyp_config.write("""
  ]}
""")
gyp_config.write("}\n")

for name, opt in enabled.items():
    if "public_defines" in opt:
        defines = opt["public_defines"]
        for define in defines:
            subst["CG_DEFINES"] = subst["CG_DEFINES"] + "#define " + define + "\n"

gyp_args.append(os.path.join(icu_root, 'icu.gyp'))


gyp_args.append('-Dlibrary=shared_library')
gyp_args.append('--debug=all')
gyp_args.append('--check')

gyp_args.extend(['-I', "config.gypi"])

options_fn = os.path.join(icu_root, 'options.gypi')
if os.path.exists(options_fn):
    gyp_args.extend(['-I', options_fn])

gyp_args.append('--depth=' + icu_root)

if opt_args.enable_shared:
    gyp_args.append('-Dicu_library=shared_library')

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
