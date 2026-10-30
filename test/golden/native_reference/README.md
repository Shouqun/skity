# Explicit native CoreText reference profiles

Legacy verification is the default. Its checked-in PNGs, command bodies and
pixel comparator are unchanged:

```sh
python3 -B tools/test-runner.py --suite=golden-text
```

CoreText and system fonts vary between macOS installations. A reviewed explicit
profile can additionally verify the original `TextGolden.Basic` and
`TextGolden.TextLinearGradientFlags` commands against an independent native
reference on its exact declared host. It does not turn historical failures into
passes or establish a unique cause for a historical difference. Run and retain
legacy results separately.

Configure the normal CoreText/Metal test build with `SKITY_CT_FONT=ON` and
`SKITY_GOLDEN_GUI=OFF`, then build it. The reference executable links only system
CoreText/CoreGraphics/CoreFoundation and the C++ runtime; it links no Skity
renderer. It draws native A8 glyph coverage using the original four-command
recipe. Standard-library Python applies independent mirror-gradient,
interpolation and source-over arithmetic. No product image, alignment search,
fitting, font replacement or comparator adjustment enters reference generation.
Instrumented verification additionally admits only the configured compiler's
ASan runtime: its actual first loader resolution, compiler/cache and runtime
file identities are recorded and rechecked. Shadow or unconfigured runtimes
are refused. Loader override variables are cleared for the native producer.
The shared golden harness requires GL support even for Metal verification.

```sh
python3 -B tools/test-runner.py --suite=coretext-profile-contract
python3 -B tools/test-runner.py --suite=coretext-native-inspect --run-only
python3 -B tools/test-runner.py --suite=golden-text --run-only \
  --coretext-profile=macos-26.6.2-pf21-helvetica17
python3 -B tools/test-runner.py --suite=coretext-profile-negative --run-only \
  --coretext-profile=macos-26.6.2-pf21-helvetica17
```

Use `--build-dir` consistently for a nondefault build location. Inspection only
records observations; it never creates or selects a profile. The explicit
profile requires both original covered cases to execute successfully, with
acknowledged reference paths and build-bound recipe/checker identities. A filter
that omits either case cannot qualify the profile. Uncovered cases continue
using their original references. Ambient profile environment variables are
cleared by the runner; selection requires its explicit argument.

The declaration pins OS/build/architecture/CoreText bundle, actual selected
PostScript names, font versions and `head` tables, complete font-file hashes,
original native recipe and source hashes. Font files stay in the operating
system and are never copied into this repository or its generated artifacts.
Generation records the exact executable, source, font, coverage and PNG
identities. Runtime verifies them again. Unknown platforms/fonts/profiles,
missing references, stale builds/sources, skipped cases and changed inputs fail
closed. There is no automatic rebaseline or fallback to a different profile.
Interactive missing-golden generation is disabled for profile verification.

The negative suite runs the unchanged original commands and comparator against
four deliberately incorrect references: horizontal glyph displacement, vertical
baseline displacement, coverage alpha and swapped gradient interpolation flags.
Each control must produce its specific pixel failures above the original
comparator limit, plus refusal of the changed reference identity. Raw failing
reports, expected/actual images and exact pixel differences are preserved in
unique directories. A control PASS means that the incorrect reference was
rejected; it is not a product golden PASS.

Adding or refreshing a profile requires review of independently observed native
identities, original recipe/source equality, positive verification, all four
specific pixel negatives, contract tests and the complete applicable ordinary
and sanitizer suites. Never refresh source pins merely to accept a changed
workload, comparator or failed product output. Keep historical references and
their actual results. A matching local profile does not prove another host or
replace historical-host CI, packaging, native consumer or release gates.
