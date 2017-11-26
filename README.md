# A wrapper to conditionally override some Linux system libs with newer ones

This is a simple wrapper for your Linux x86 or x86_64 application that
checks the versions of some system libs and compares it with the versions
of the same libs you supply, and sets LD_LIBRARY_PATH in such a way that
your version of the libs is used if it's newer than the one on the system
and then executes your application.  
(If an LD_LIBRARY_PATH is already set, the changes from the wrapper will
 be prepended to it)
 
 I wrote a lengthy blogpost about this: http://blog.gibson.sh/2017/11/26/creating-portable-linux-binaries/

In short, this is helpful to create programs for Linux that work on a wide range of
distributions (incl. older and bleeding edge ones), even if they need
fairly recent versions of the supported libs (e.g. to support C++11 or C++14).  
By making sure you only override the system libs if the supplied version is newer,
the wrapper prevents conflicts with other system libs that depend on the
overriden ones, for example the Mesa libGL uses libstdc++, and if it gets one
that's older than the one it's linked against (available on the system)
it fails to load.

Currently supported libs:
- libstdc++.so.6
- libgcc_so.so.1
- libSDL2.so.0
- libcurl.so.4 (only used if not available on the system at all)

It has no dependencies except for a C compiler that supports C99 (any GCC
version of the last decade should work) and a libc incl. libdl (glibc tested).  
The wrapper can be built with `$ gcc -std=gnu99 -o YourGameWrapper wrapper.c -ldl`

It can be configured by changing/commenting out some #defines at the beginning
of the source file.

When executing the wrapper and the environment variable WRAPPER_DEBUG
is set 1, some helpful messages about the detected versions and the used
LD_LIBRARY_PATH will be printed. This is helpful to debug problems,
especially when your users are having them.  
e.g. `$ WRAPPER_DEBUG=1 ./YourGameWrapper`

### License

(C) 2017 Daniel Gibson

This software is released to the Public Domain, see header of source code for details.
