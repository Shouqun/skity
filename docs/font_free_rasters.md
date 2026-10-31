# Font-free raster quads

`GlyphRaster::MakeCopy` creates immutable A8 coverage, RGBA or BGRA texels
without retaining a font, callback, pixmap or graphics context. It copies logical
rows, omitting padding. Supply the actual readable source span and an explicit
retained-byte bound; invalid formats, alpha contracts, overflow and allocation
refusal return an empty result. `RetainedBytes()` reports object and packed pixel
allocation requests, excluding allocator bookkeeping and later backend storage.

Use `Canvas::CanDrawGlyphRaster` for validation and `DrawGlyphRaster` to submit a
quad. Bounds are in the current affine canvas coordinates. Nearest and linear
sampling are supported. Coverage uses solid fill paint; intrinsic color uses its
own RGB and the paint alpha. Per-draw path/image/mask effects are refused; use an
outer layer. Both calls require a separate conservative pixel-staging bound.
Software and picture recording preserve the corresponding image policy. Hardware
uses the existing text geometry and atlas filtering policy without font access.

A successful draw means the backend accepted the draw, not that the GPU completed
it. The canvas/backend owns queued resources after the raster object is released.
Applications remain responsible for target failure handling, resource limits and
backend retirement. Under the explicit C++ allocation-recovery profile, allocation
exceptions can propagate to that owning boundary. Rebuild C++ consumers with the
updated headers and library; this additive virtual interface is not binary
compatible with previously built C++ canvas subclasses.

Permanent unit coverage is in `text/glyph_raster_test.cc`; Metal sampling,
original-text equivalence, intrinsic color and resource refusal checks are in
`render/hw/text_atlas_sampling_test.mm`. Run them through the unified runner:

```sh
python3 tools/test-runner.py --suite=unit \
  --filter='GlyphRasterTest.*:TextAtlasSamplingTest.*'
```
