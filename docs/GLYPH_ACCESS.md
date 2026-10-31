# Scoped glyph access

`Font::VisitGlyphPaths` and `Font::VisitGlyphBitmap` keep the actual source
owners alive while a synchronous callback consumes glyph data. Use these
operations when the callback can make another glyph request or purge a scaler
cache. Copy any data needed after the callback returns. The callback executes
outside the scaler lock; it must not mutate the borrowed glyph or pixels.

`VisitGlyphBitmap` uses a request-local image owner rather than replacing the
legacy thread-local bitmap. It reports the validated readable pixel span,
including initialized destination row padding. Empty bitmaps deliver null pixels
and zero bytes. A whitespace glyph can have padded native raster storage; the
scoped operation preserves that original backend representation. Null callbacks, invalid storage, absent pixels for nonempty
glyphs, zero allowances and spans exceeding `maximum_readable_bytes` are refused
without invoking the callback. A callback returning false propagates refusal.

The allowance bounds the delivered snapshot and any private borrowed-pixel copy.
Native rasterization may allocate before its image size is known. The allowance
is not a bound on native scratch, font/cache storage or allocator overhead, and
is not a retained-storage accounting certificate. Callback exceptions follow the
selected C++ build profile, as with `VisitGlyphPaths`.

Existing `LoadGlyphBitmap` behavior remains a single thread-local snapshot valid
until the next bitmap load on that thread. It does not gain scoped ownership.
Concurrent calls on separate immutable Font values can use independent scoped
views; callers must still synchronize mutation of shared Font objects.

Run the permanent ownership and image tests with:

```sh
python3 tools/test-runner.py --suite=unit \
  --filter="FontBitmapVisitTest.*:GlyphImageTest.*"
```

The tests compare legacy pixels, purge source caches during callbacks, make
reentrant loads, verify empty/refused views and exercise concurrent requests.

## Packed and native raster phases

`Font::VisitGlyphBitmapRaster` takes `Font::GlyphBitmapRasterOptions` to visit
the original packed raster source while preserving the same request-local owner and readable
span contract. Ordinary packed X/Y phases are quarters in the range 0..3.
`native_raster_phase` is zero for ordinary rasterization or 1..4 for Apple's
native grayscale quarters 0..3/4. Invalid values refuse before invoking the
visitor. Explicit native requests refuse unsupported density, transform, font
or paint rather than falling back to ordinary rasterization. Translation and
glyph placement are separate from these source parameters.

`VisitGlyphBitmap` remains the ordinary phase-zero request and retains an
unambiguous member address. Neither API assigns a new alpha/sampling policy, changes Font layout or certifies native
scratch, cache memory, retained graphics storage or the caller's final quota.
The source Font/paint must remain serialized against mutation; returned views
must be copied before the callback returns.

Run the original-source, phase, span, owner and concurrency regressions with:

```sh
python3 tools/test-runner.py --suite=unit --filter='FontRasterVisitTest.*'
```
