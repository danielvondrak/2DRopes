## Build Environment Setup

Building the codebase can be done in a terminal which is equipped with the
ability to call either MSVC or Clang from command line.

This is generally done by calling `vcvarsall.bat x64`, which is included in the
Microsoft C/C++ Build Tools. This script is automatically called by the `x64
Native Tools Command Prompt for VS <year>` variant of the vanilla `cmd.exe`. If
you've installed the build tools, this command prompt may be easily located by
searching for `Native` from the Windows Start Menu search.

You can ensure that the MSVC compiler is accessible from your command line by
running:

```
cl
```

If everything is set up correctly, you should have output very similar to the
following:

```
Microsoft (R) C/C++ Optimizing Compiler Version 19.29.30151 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

usage: cl [ option... ] filename... [ /link linkoption... ]
```

### Building

Within this terminal, `cd` to the root directory of the codebase, and just run
the `build.bat` script:

```
build
```

If everything worked correctly, there will be a `build` folder in the root
level of the codebase, and it will contain a freshly-built `Win32App.exe`.

---

