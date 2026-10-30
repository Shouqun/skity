# C++ recovery product consumers

Build the complete recovery product with `SKITY_TEST=OFF`,
`SKITY_BUILD_INSTALL=ON`, C-API/unified exports off and codec/IO on, then install
and relocate its prefix. This fixture uses the existing pinned GoogleTest
harness; it adds no product dependency.

Configure this directory with `CMAKE_PREFIX_PATH` pointing only at the relocated
installed prefix and `GOOGLETEST_SOURCE_DIR` at the synced `third_party/gtest`.
Build `skity_unit_test`, then run through the component's unified runner:

```sh
python3 tools/test-runner.py --suite=unit --run-only --build-dir=<consumer-build>
```

The default consumer includes only installed public Skity headers. In a separate
build directory, set `SKITY_CONSUMER_PRIVATE_IO=ON`,
`SKITY_PRIVATE_IO_SOURCE_DIR=<source>/module/io` and
`SKITY_PRIVATE_IO_LIBRARY=<product-build>/module/io/libskity-io.a` to check the
existing noninstalled IO implementation against the same production libraries.
That second check is internal IO qualification, not a public-SDK-only claim.
Keep both configurations and their executed products distinguishable. Verify
production/installed symbol tables and dependencies exclude every private
allocation-test helper; test binaries are never installed or redistributed.

For the default product, leave its existing C-API module enabled and set
`SKITY_CONSUMER_CPP_RECOVERY=OFF` in the private IO consumer. It then uses the
matching default private header view and links the installed `skity::capi`
target required by the ordinary IO archive. The recovery consumer requires
a product without that target. Both use the same unchanged round-trip and
caller-state assertions; no test support enters either product.
