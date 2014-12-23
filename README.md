# Overview

Rig is an experimental graphics and multimedia technology with a
guiding philosophy that dedicated tooling and interactive feedback
from real hardware is the ideal way to create interactive graphics.

## Highlights

* Editor and graphics engine developed in tandem
* Editor can connect to multiple slave devices immediately reflecting
  edits across all those devices which you can interact with
* Toggle between an edit and preview mode directly in the editor too
* Designed around a flexible and intuitive Entity/Component system
* Rendering and logic de-coupled (either thread, process or remote
  boundaries)
* Designed to keep the rendering system replaceable, either
  to specialise for a unique artistic style or optimise for a
  specific platform
* First renderer is a traditional forward renderer, supporting real
  time lighting, shadows, depth of field, one specialised
  pointillism effect and shell based furr rendering.
* Cross platform with progress made on Linux, OSX, Android and
  Emscripten + WebGL
* Rig is written in C/C++ with minimal required dependencies
* Open source, MIT license.

# Building Rig

It is currently only recommended to try building Rig on Linux, as
other platforms are still a work in progress (though of course if you
want to help out with another platform please get in touch and I'd be
glad to help you get started).

On Linux the recommended way to build Rig (at least if it's your first
time) is to use the build/linux/build.sh script which will take care
to fetch and build all necessary dependencies for Rig.

The script needs to be given a path to the Rig source code itself and
a bashrc file that may contain certain build variables that may be
tuned (see the example in build/linux/bashrc)

When you are ready you can build Rig like this:

        $ cd build/linux
        $ ./build.sh ../../ ./bashrc

# Running

Building rig will result in three binaries, rig, rig-device and rig-slave:

1. **Editor Mode**

    This mode is for interactively creating something. The editor is
    used by running the 'rig' binary giving it a path to a UI.rig file
    like:

        rig /path/to/my/ui.rig

    If the file doesn't exist then it will be created when you
    save by pressing `Ctrl-S` so long as the parent directory already
    exists.

    **Note:** When Rig is run in editor mode it will automatically
    search for image assets under the parent directory of your ui
    file. (So under `/path/to/my` for the above example) Because of
    this, it would not be recommended to create a new ui.rig at the
    top of your home directory.


2. **Device Mode**

    When you've created something you are happy with then you can run
    it on Linux via the 'rig-device' command like:

        rig-device /path/to/my/ui.rig

    **Note:** This is a stop-gap solution really. The intention is
    that from within the Rig editor it should be possible to output
    a dedicated binary appropriate for a chosen platform.

3. **Slave Mode**

    The aim of Rig is to directly connect the prototyping and creative
    process to the technology (hardware and software) that will
    eventually run whatever interactive experience you want to build.

    Rig can be run in a slave mode that is similar to device-mode but
    also has network connectivity to an editor. By running Rig in
    slave mode on a device the editor will be able to discover the
    device via the Zeroconf protocol and connect to it. While connected
    then changes made in the editor will be immediately synchronized
    with the slave devices. This way changes, such as timing changes
    for an animation can immediately be tested for their performance,
    quality and responsiveness on real hardware. Experience working
    directly with the hardware should be able to influence you
    at the earliest stages when ideas are easiest to change.

    Running in slave mode is just done like this:

        rig-slave

    No *.rig path need be given since the editor will send everything
    necessary for the slave once connected.
