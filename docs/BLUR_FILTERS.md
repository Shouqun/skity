# Blur conventions

`MaskFilter::MakeBlur(style, radius)` keeps the legacy radius convention.
Its GPU kernel derives sigma as `0.57735 * radius + 0.5`, with the existing
radius-based support, downsampling and blur styles. `ImageFilters::Blur` and
`ImageFilters::DropShadow` retain their existing sigma-to-radius conversion.
Their software implementation remains the legacy stack blur.

Use `MaskFilter::MakeGaussianBlur(sigma)` for an explicit normal-mask
Gaussian standard deviation in local coordinates. Its finite support is
three times sigma, with fractional support rounded outward for convolution.
The GPU scales sigma and support together and selects downsampling from
sigma. The software path convolves premultiplied RGBA, including colored
offscreen content and direct text, without reapplying source alpha or color
filter during compositing. Zero, negative, nonfinite sigma or nonfinite
three-sigma support returns null. Very small positive sigma approaches the
identity discrete kernel.

The corresponding C factory is
`skity_mask_filter_create_gaussian_blur(sigma)`; release its owning handle
with `skity_mask_filter_destroy`. Existing C entries keep their behavior.
Rebuild C++ consumers when updating the library: the additive class fields
do not provide binary compatibility with an older C++ class layout.

Legacy serialization keeps `SkBlurMaskFilterImpl` and its existing bytes.
The explicit path uses `SkityGaussianMaskFilter` with an eight-byte payload:
32-bit version `1`, followed by the float sigma. Unknown versions, invalid
sigma, mismatched payload lengths and truncated payloads are rejected.
Readers predating this factory reject the unknown type instead of treating
sigma as a legacy radius.

Run the unit and unchanged shape/text suites with `tools/test-runner.py`.
The `GaussianMaskFilter`, `SWCanvasGaussianMask`,
`GaussianMaskSerialization` and `GaussianMaskHardware` tests cover the new
contract independently of stored golden images. Existing golden references
remain unchanged.
