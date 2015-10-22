[![Build Status](https://travis-ci.org/rig-project/clib.png)](https://travis-ci.org/rig-project/clib)

##Clib: A C utilities library

Clib is a collection of C utilities under the MIT license

Clib doesn't offer strong API/ABI guarantees so its recommended to
import clib into a project to be decoupled from any API changes at
inopportune times.

Clib's build scripts are designed to be embeddable. For autotool based
projects there is an AM_CLIB macro provided in build/autotools/clib.m4
to handle the appropriate build time checks.  There's also a GYP
description that's well suited to embedding.  Since it's quite a small
library it should be pretty straightforward to integrate into most
build systems.

The general idea of recommending embedding clib is to avoid some
maintenance limitations that come from having to strictly maintain a
stable API + ABI which can eventually get in the way of fixing old
mistakes, addressing discovered performance issues or adapting to new
environments and development practices.  This also helps encourage
only building the apis you need for your project.

##Platform Support:
Clib has been tested on Linux, Emscripten, Windows and OSX recently
but should also support Android and iOS.

##Scope
Clib has been spun out of the
[Rig](https://github.com/rig-project/rig) project in case it might be
useful for others. Rig is currently the primary user influencing
what's included in the api.

####Data Types
* Arrays
* Pointer Arrays
* Byte Arrays
* Queues
* Linked lists (Both Linux kernel style intrusive lists and glib style)
* Hash tables (general void * values)
* Red Black Trees
* Math utilities
  * Vectors
  * Matrices
  * Eulers
  * Quaternions

####Utilities
* Infallible memory allocation
* C style exceptions (similar to glib's GError api)
* String manipulation
* String objects
* Ascii (locale independent) string formatting
* Unicode support
  * utf8 validation and handling
  * Iconv like api for character set conversions (ascii, latin1, utf8, utf16(be/le), utf32(be/le), OS-iconv fallback)
* String to quark api
* Random Number Generator
* Quicksort
* XDG directories
* High precision monotonic timestamps
* Timers
* Hookable Logging

####Portability
* Module loading API (trivial wrapper of libuv API - may be removed)
* Non-recursive Mutex API
* Thread Local Storage
* Portable fmemopen()
* Path manipulation

##Based on:
Clib was originally derived from eglib that was created as an MIT
licensed clone of many glib APIs. Now it follows a different K&R like
style, some apis have diverged from the original glib ones and new
apis have been added.

##API Reference
And initial API reference can be found [here](http://rig-project.github.io/clib/index.html).

##Similar projects:
* [glib](https://developer.gnome.org/glib/stable/)
* [klib](https://github.com/attractivechaos/klib)
* [eglib](https://github.com/mono/mono/tree/master/eglib)
* [APR](https://apr.apache.org/)
* [sglib](http://sglib.sourceforge.net/)
