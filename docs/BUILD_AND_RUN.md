# Build Skity from source

This document will guide you through the process of building the Skity project from source.

## Prerequisites

The third-party libraries used by Skity is managed by [habitat](https://github.com/lynx-family/habitat). A lightweight dependency management tool developed by [lynx-family](https://github.com/lynx-family). You can install it manually or use the helper script located in [tools/hab](../tools/hab) (Windows user can use [tools/hab.ps1](../tools/hab.ps1)) to sync the dependencies.

The Skity project use [CMake](https://cmake.org/) as the build system. Make sure you have CMake version 3.15 or higher installed on your system.

Although Skity supports CPU rendering, it is recommended to ensure that the GPU environment currently supported by Skity is installed.
This is the support GPU backends on different platforms:
* Windows and Linux:
    * OpenGL at least version 3.3
    * Vulkan (experimental) at least version 1.1
* MacOS:
    * Metal (recommend to use Metal since OpenGL is deprecated by Apple)
    * OpenGL at least version 3.3
* Android:
    * OpenGL ES at least version 3.0
    * Vulkan (experimental) at least version 1.1
* iOS (11.0 +):
    * Metal (recommend to use Metal since OpenGLES is deprecated by Apple)
    * OpenGL ES at least version 3.0

## Sync Dependencies

These three-party dependencies are divided into three parts:
* The core dependencies which are required to build the core library. These dependencies are located in [deps/](../hab/DEPS).
* Other dependencies which are only required when developing Skity (e.g. testing, example, etc.). These dependencies are located in [deps/dev/](../hab/DEPS.dev).
* CI dependencies which are only required when running CI. These dependencies are located in [deps/ci/](../hab/DEPS.ci).


As described above.

If only need to build the core library, you can sync the core dependencies by running the following command in the root directory of the project:
```bash
# Linux & MacOS
./tools/hab sync # sync core dependencies

# Windows
./tools/hab.ps1 sync
```

For developers, you can sync the dev dependencies by running the following command:
```bash
# Linux & MacOS
./tools/hab sync --target dev # sync dev dependencies

# Windows
./tools/hab.ps1 sync --target dev
```

## Build Skity

Use CMake to build the project.

```
cmake -B <build_dir> <options>
cmake --build <build_dir>
```

Replace `<build_dir>` with the path to the build directory you want to create.

For example:
```
cmake -B build
cmake --build build
```

There are several options you can use to configure the build:

* Options for building Skity itself:
    * `SKITY_HW_RENDERER` - Option for GPU raster. Default is ON. Pass `-DSKITY_HW_RENDERER=OFF` in command line to disable it. (It is recommended not to turn this option off)
    * `SKITY_SW_RENDERER` - Option for CPU raster. Default is ON. Pass `-DSKITY_SW_RENDERER=OFF` in command line to disable it if does not needs the CPU raster pipeline.
    * `SKITY_GL_BACKEND` - Option for OpenGL backend. Default is ON. Pass `-DSKITY_GL_BACKEND=OFF` in command line to disable it. (OpenGL/GLES is almost supported by all platforms, so it is recommended not to turn this option off.)
    * `SKITY_MTL_BACKEND` - Option for Metal backend. Not available on Windows and Linux but open by default on Apple platform.
    * `SKITY_VK_BACKEND` - Option for Vulkan backend (experimental). Default is OFF. Pass `-DSKITY_VK_BACKEND=ON` in command line to enable it. Requires Vulkan at least version 1.1. Skity will attempt to create the highest available Vulkan version and leverage extensions such as dynamic rendering when supported.
    * `WGX_SPIRV_BACKEND` - Option for WGX SPIR-V generation. Defaults to the value of `SKITY_VK_BACKEND`, but can be enabled independently without a Vulkan runtime by passing `-DWGX_SPIRV_BACKEND=ON`.
    * `SKITY_FONTCONFIG` - Option for the native Linux Fontconfig font manager. Default is ON on Linux with the FreeType backend and OFF on other platforms. Requires Fontconfig development files, version 2.13.1 or newer. Pass `-DSKITY_FONTCONFIG=OFF` to disable it.
    * `SKITY_LOG` - Option for logging. Default is OFF since print log may slow down the performance. Pass `-DSKITY_LOG=ON` in command line to enable it.

* Options for build optional modules:
    * `SKITY_CAPI_MODULE` - Option for building the C API module. Default is ON.
    * `SKITY_CODEC_MODULE` - Option for building codec module. Default is ON for some historical reasons. Pass `-DSKITY_CODEC_MODULE=OFF` in command line to disable it. (Requires `SKITY_CAPI_MODULE`; so does `SKITY_IO_MODULE`.)
    * `SKITY_UNIFIED_LIB` - Option for the unified single-library shape. Default is OFF. Pass `-DSKITY_UNIFIED_LIB=ON` to compile the C API sources into `libskity`, hide the legacy C++ API (only the `skity_*` C ABI is exported) and make the header-only C++ wrapper under `module/capi/include/skity/hpp/` the C++ surface. In this shape the legacy-API tests, golden tests, benchmarks and legacy examples are not built; the unit test entry becomes `skity_wrapper_unit_test`. See [module/capi/README.md](../module/capi/README.md) for details.

* Options for build other development components (Make sure you sync dev dependencies):
    * `SKITY_EXAMPLE` - Option for building example code. Default is OFF. Pass `-DSKITY_EXAMPLE=ON` in command line to enable it.
    * `SKITY_TEST` - Option for building test code. Default is OFF. Pass `-DSKITY_TEST=ON` in command line to enable it.
    * `SKITY_TEST_UT` - Option for building unit tests when `SKITY_TEST=ON`. Default is ON.
    * `SKITY_TEST_BENCH` - Option for building benchmarks when `SKITY_TEST=ON`. Default is ON.
    * `SKITY_TEST_GOLDEN` - Option for building golden tests when `SKITY_TEST=ON`. Default is ON.

> Note: Other options not mentioned above are used internally and may be changed or removed in the future.

## Run Examples

Make sure sync the dev dependencies and open `SKITY_EXAMPLE` option during cmake config stage.

After build, you can find all example executable in `<build_dir>/example/case` directory.
Windows user can find the executable in `<build_dir>/<Debug/Release>/` directory. Based on the build type you choose.

You can run the example code like this:
```bash
# run the basic example
<build_dir>/example/case/basic_example <backend> [--aa=<mode>]
```

The available backend name for `<backend>` is:
* `gl` - OpenGL backend
* `vulkan` - Vulkan backend (experimental)
* `metal` - Metal backend
* `software` - Software renderer backend

The optional AA mode can be:
* `native` - Use the platform-native display configuration. This is the same
  behavior as omitting `--aa`.
* `none` - Render at 1x with no MSAA and nearest presentation, useful for
  inspecting raw pixel edges.
* `contour` - Use the same 1x nearest presentation as `none`, but enable
  contour AA.
* `msaa` - Use the same 1x nearest presentation as `none`, but enable MSAA.

## Run Tests

Make sure sync the dev dependencies and open `SKITY_TEST` option during cmake config stage.

After build, you can find the  `<build_dir>/test/ut/skity_unit_test` executable. Windows user can find the executable in `<build_dir>/<Debug/Release>/skity_unit_test.exe` directory. Based on the build type you choose.

It can be run directly. For example:
```bash
# run the unit test
<build_dir>/test/ut/skity_unit_test
```

Or use `ctest` to run the test. For example:
```bash
# run the unit test
ctest --test-dir <build_dir>/test/ut
```

> Note: the golden test is only available on MacOS since it uses Metal backend.
> The detail of golden test can be found in [golden/README.md](../test/golden/README.md).


## Verify a relocated installed C++ package

Sync core, dev and module dependencies first. This test uses the existing
GoogleTest dependency only for its external consumer; installed core/codec
headers and libraries are the consumer's only Skity inputs.

```sh
cmake -S . -B build-product -G Ninja -DSKITY_TEST=OFF \
  -DSKITY_CAPI_MODULE=ON -DSKITY_CODEC_MODULE=ON -DSKITY_BUILD_INSTALL=ON
python3 tools/test-installed-package.py --producer-build-dir build-product \
  --work-dir build-installed-package-default
python3 tools/test-installed-package.py --producer-build-dir build-product \
  --work-dir build-installed-package-custom --includedir include/skity-sdk
```

Each work directory must be new. The tool configures the producer's installation
prefix, builds and installs it, moves the prefix, verifies the old path is absent,
and configures a separate consumer through find_package. The public consumer
checks caller canvas state and exact PNG round-trip pixels through the unified
unit runner. Commands, logs, the runner's structured reports and result.json
stay in the work directory. A failed configure/build, missing case or skipped
case is not a passing installed-package test. Private IO is not an installed
SDK input and is outside this fixture's scope.


## C++ canvas allocation recovery

`SKITY_CPP_ALLOCATION_RECOVERY=ON` enables exception unwinding and transactional
canvas saves for C++ callers. The default remains OFF. Set both
`SKITY_CAPI_MODULE=OFF` and `SKITY_UNIFIED_LIB=OFF`; incompatible export
configurations are refused at configure time. This mode defines no new C error
conversion or foreign-unwinding contract. Public C++ signatures stay unchanged.

Non-MSVC recovery core, codec and IO builds and their C++ test clients retain
RTTI for standard exception type identity across shared library boundaries. In particular, Darwin hidden-visibility code compiled with
`-fno-rtti` can throw a local `std::bad_alloc` type that an ordinary external
C++ caller cannot match. The default no-exception profile is unchanged.

The permanent consumer regression deliberately uses a separate RTTI-enabled
executable and a hidden image with the actual core compile options. Reusing
legacy no-RTTI unit objects in that executable can conceal the mismatch. With
a recovery unit build, run it through the same unified runner:

```sh
cmake --build build-recovery --target skity-recovery-exception-consumer
python3 tools/test-runner.py --suite=unit --run-only \
  --build-dir=build-recovery/recovery-exception-consumer
```

The same-image control and external `std::bad_alloc`/`std::length_error` catches
must all pass. These test-only images have no product installation rules.

Catch allocation failures at the owning C++ transaction boundary. A refused
Save preserves the caller's matrix, clip and save depth; successful Save keeps
its ordinary return value. This is not a promise of general allocation recovery
for every rendering operation. Retain the frame/resource and backend-specific
failure checks appropriate to the operation being used.

The unit framework registers the strict allocation-recovery fixture in this
profile. C-API tests retain their supported default configuration; shared IO
regressions run through both bridges. Absence of the recovery case in default
builds is not evidence of recovery. Validate both
configurations through `tools/test-runner.py`, plus applicable sanitizer,
rendering and platform checks.

In that recovery profile, codec and IO retain their existing algorithms and
formats using private C++ core bridges. Codec, stream, picture, SVG-path and
recording operations do not link the C API or export its entry points. Default
builds retain the C bridges. Do not disable codec/IO to enable recovery; keep
`SKITY_CODEC_MODULE=ON` and `SKITY_IO_MODULE=ON` for the complete profile.
Module internals are private and do not become installed public headers.

Recovery-mode Data factories consume transferred storage before allocating the
object or shared control block. Their release callback runs exactly once even
when construction throws. Recording Save operations commit their restore
bookkeeping and operation bytes together; a refused Save appends no operation.

For recovery unit tests, set `SKITY_BUILD_INSTALL=OFF`. Those test images link
private replacement allocation operators and one shared thread-local fault
counter so faults reach the actual core/codec code even with Darwin ASan.
The unit-test/install combination is refused rather than packaging this test
allocator. Product builds with `SKITY_TEST=OFF` never contain these sources,
operators or support-library dependency. The ordinary default test profile
keeps its allocator and installation behavior.

The reusable [production consumer fixtures](../test/fixtures/recovery_cpp_consumer/README.md)
separately exercise installed public headers/libraries and the existing internal
IO archive. Run them through the unified runner against a relocated production
prefix, keeping the public-SDK-only and private-IO scopes distinct.

### Bounded software temporaries in C++ recovery mode

The opt-in recovery profile stages integral axis-aligned rectangle raster spans
with one bounded output allocation. Coordinates and clipping stay within the
existing fixed-point safe bounds; delegate order and full-coverage spans stay
unchanged. Fractional and other paths retain the original scan converter.
Nearest-neighbor NEON batches use eight fixed scratch colors instead of a heap
allocation for each row. Default-profile code and sampling/blending arithmetic
remain unchanged.

Permanent unified-runner cases in `canvas_save_allocation_test.cc` refuse every
allocation beyond the rectangle output and all nearest-row scratch allocations.
`sw_raster_test.cc` checks both fill rules/directions, scaling, translation,
reflection, clipping, large coordinates and exact delegate order. These cases
supplement, rather than relax, every original save-state/allocation assertion.

Recovery-mode layer recording prepares restore-stack, paint-table and complete
stream capacity before publication, preserving the existing wire operations.
Allocator bookkeeping refusal releases unregistered raw blocks, optional block
cache metadata failure falls back to direct release, and failed finalizer
registration destroys an already constructed owner. Permanent allocation-fault
cases verify these ownership transitions and a null-block refusal. Legacy
configuration behavior stays unchanged; recovery mode remains an explicit C++
choice with the documented C-API/unified refusal.

GPU surfaces retire their canvas before releasing arena-backed stack storage.
An abandoned, unflushed frame is safe to destroy with live nested layers.
The ordinary Metal unit registry includes a deterministic surface-lifetime
regression, in addition to the recovery profile's allocation-refusal cases.


## Unified runner process contracts

Run the permanent report/process-outcome regressions without a native build:

```sh
python3 tools/test-runner.py --suite=runner-contract --run-only
```

A nonzero or signal process status still fails after a successful JSON report
has been written. Completed case results remain available, and the additional
infrastructure record retains the actual process status. Ordinary assertion
failures and missing-report outcomes keep their existing behavior. The suite
includes real completed child processes and needs only the Python standard
library; it does not configure CMake or require a graphics context.
