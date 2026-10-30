// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>

#include <array>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifndef SKITY_CORETEXT_REFERENCE_SOURCE_SHA256
#error "Build the native reference with its source fingerprint"
#endif

std::string JsonString(const std::string& text) {
  std::ostringstream out;
  out << '"';
  for (unsigned char c : text) {
    switch (c) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if (c < 32)
          out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
              << unsigned(c) << std::dec;
        else
          out << c;
    }
  }
  out << '"';
  return out.str();
}
std::string Utf8(CFStringRef value) {
  if (!value) return {};
  const auto count = CFStringGetMaximumSizeForEncoding(CFStringGetLength(value),
                                                       kCFStringEncodingUTF8) +
                     1;
  if (count <= 0 || count > 65536) return {};
  std::vector<char> bytes(static_cast<size_t>(count));
  return CFStringGetCString(value, bytes.data(), count, kCFStringEncodingUTF8)
             ? std::string(bytes.data())
             : std::string{};
}
std::string Name(CTFontRef font, CFStringRef key) {
  CFStringRef value = CTFontCopyName(font, key);
  auto out = Utf8(value);
  if (value) CFRelease(value);
  return out;
}
std::string DescribeFont(CTFontRef font) {
  std::ostringstream out;
  CFDataRef head =
      CTFontCopyTable(font, kCTFontTableHead, kCTFontTableOptionNoOptions);
  std::ostringstream hex;
  if (head) {
    const auto length = CFDataGetLength(head);
    if (length > 0 && length <= 4096)
      for (CFIndex i = 0; i < length; ++i)
        hex << std::hex << std::setw(2) << std::setfill('0')
            << unsigned(CFDataGetBytePtr(head)[i]);
    CFRelease(head);
  }
  CFTypeRef raw = CTFontCopyAttribute(font, kCTFontURLAttribute);
  std::string path;
  if (raw && CFGetTypeID(raw) == CFURLGetTypeID()) {
    CFStringRef value = CFURLCopyFileSystemPath(static_cast<CFURLRef>(raw),
                                                kCFURLPOSIXPathStyle);
    path = Utf8(value);
    if (value) CFRelease(value);
  }
  if (raw) CFRelease(raw);
  out << "{\"postscript\":" << JsonString(Name(font, kCTFontPostScriptNameKey))
      << ",\"version\":" << JsonString(Name(font, kCTFontVersionNameKey))
      << ",\"head_hex\":" << JsonString(hex.str())
      << ",\"file\":" << JsonString(path) << '}';
  return out.str();
}
std::string CoreTextBundleVersion() {
  CFBundleRef bundle =
      CFBundleGetBundleWithIdentifier(CFSTR("com.apple.CoreText"));
  if (!bundle) return {};
  CFTypeRef value =
      CFBundleGetValueForInfoDictionaryKey(bundle, CFSTR("CFBundleVersion"));
  return value && CFGetTypeID(value) == CFStringGetTypeID()
             ? Utf8(static_cast<CFStringRef>(value))
             : std::string{};
}
std::string SystemValue(const char* name) {
  char value[256]{};
  size_t size = sizeof(value);
  if (sysctlbyname(name, value, &size, nullptr, 0) || !size ||
      size > sizeof(value))
    return {};
  value[sizeof(value) - 1] = 0;
  return value;
}

void DrawLatin(CGContextRef context, CTFontRef font, double baseline) {
  const UniChar text[] = {'S', 'K', 'I', 'T', 'Y', ' ',
                          's', 'k', 'i', 't', 'y'};
  CGGlyph glyphs[11]{};
  CGSize advances[11]{};
  CGPoint positions[11]{};
  CTFontGetGlyphsForCharacters(font, text, glyphs, 11);
  CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, glyphs,
                             advances, 11);
  double x = 20;
  for (size_t i = 0; i < 11; ++i) {
    positions[i] = CGPointMake(std::round(x * 4) / 4, baseline);
    x += advances[i].width;
  }
  CTFontDrawGlyphs(font, glyphs, positions, 11, context);
}

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
  std::array<unsigned char, 32> calibration{};
  CGContextRef check = CGBitmapContextCreate(calibration.data(), 1, 2, 8, 16,
                                             gray, kCGImageAlphaOnly);
  if (!check) return 3;
  CGContextSetGrayFillColor(check, 0, 1);
  CGContextFillRect(check, CGRectMake(0, 0, 1, 1));
  CGContextFlush(check);
  const bool native_y0_at_row0 = calibration[0] == 255;
  if (native_y0_at_row0 == (calibration[16] == 255)) return 4;
  CGContextRelease(check);

  constexpr size_t width = 400, height = 400;
  std::vector<unsigned char> pixels(width * height);
  CGContextRef context = CGBitmapContextCreate(pixels.data(), width, height, 8,
                                               width, gray, kCGImageAlphaOnly);
  CGColorSpaceRelease(gray);
  if (!context) return 5;
  CGContextSetTextMatrix(context, CGAffineTransformIdentity);
  CGContextSetAllowsFontSubpixelQuantization(context, false);
  CGContextSetShouldSubpixelQuantizeFonts(context, false);
  CGContextSetAllowsFontSubpixelPositioning(context, true);
  CGContextSetShouldSubpixelPositionFonts(context, true);
  CGContextSetShouldAntialias(context, true);
  CGContextSetGrayFillColor(context, 0, 1);

  CTFontRef base =
      CTFontCreateWithName(CFSTR("PingFangSC-Regular"), 12, nullptr);
  const double optical = 12;
  CFNumberRef optical_number =
      CFNumberCreate(nullptr, kCFNumberDoubleType, &optical);
  int tracking = 0;
  CFNumberRef tracking_number =
      CFNumberCreate(nullptr, kCFNumberIntType, &tracking);
  const void* keys[] = {CFSTR("NSCTFontOpticalSizeAttribute"),
                        CFSTR("NSCTFontUnscaledTrackingAttribute")};
  const void* values[] = {optical_number, tracking_number};
  CFDictionaryRef attributes = CFDictionaryCreate(
      nullptr, keys, values, 2, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
  CTFontDescriptorRef descriptor =
      CTFontDescriptorCreateWithAttributes(attributes);
  CTFontRef font =
      base ? CTFontCreateCopyWithAttributes(base, 64, nullptr, descriptor)
           : nullptr;
  CTFontRef latin_base = CTFontCreateWithName(CFSTR("Helvetica"), 12, nullptr);
  CTFontRef latin = latin_base ? CTFontCreateCopyWithAttributes(
                                     latin_base, 64, nullptr, descriptor)
                               : nullptr;
  if (latin_base) CFRelease(latin_base);
  CFRelease(descriptor);
  CFRelease(attributes);
  CFRelease(tracking_number);
  CFRelease(optical_number);
  if (base) CFRelease(base);
  if (!font || !latin) return 6;
  const UniChar characters[] = {0x4f60, 0x597d};
  CGGlyph glyphs[2]{};
  if (!CTFontGetGlyphsForCharacters(font, characters, glyphs, 2)) return 7;
  CGSize advances[2]{};
  CTFontGetAdvancesForGlyphs(font, kCTFontOrientationHorizontal, glyphs,
                             advances, 2);
  const CGPoint positions[] = {
      {20, 250}, {20 + std::round(advances[0].width * 4) / 4, 250}};
  CTFontDrawGlyphs(font, glyphs, positions, 2, context);
  DrawLatin(context, latin, 350);
  DrawLatin(context, font, 150);
  const CGPoint repeated[] = {{20, 50},
                              {20 + std::round(advances[0].width * 4) / 4, 50}};
  CTFontDrawGlyphs(font, glyphs, repeated, 2, context);
  CGContextFlush(context);
  const auto latin_metadata = DescribeFont(latin);
  const auto cjk_metadata = DescribeFont(font);
  CFRelease(latin);
  CFRelease(font);
  CGContextRelease(context);
  FILE* file = std::fopen(argv[1], "wb");
  if (!file) return 8;
  for (size_t y = 0; y < height; ++y) {
    const size_t source_y = native_y0_at_row0 ? height - 1 - y : y;
    if (std::fwrite(pixels.data() + source_y * width, width, 1, file) != 1)
      return 9;
  }
  if (std::fclose(file)) return 10;
  struct utsname system {};
  if (uname(&system)) return 11;
  std::cout << "{\"schema_version\":1,\"producer_source_sha256\":"
            << JsonString(SKITY_CORETEXT_REFERENCE_SOURCE_SHA256)
            << ",\"platform\":{\"system\":" << JsonString(system.sysname)
            << ",\"machine\":" << JsonString(system.machine)
            << ",\"product_version\":"
            << JsonString(SystemValue("kern.osproductversion"))
            << ",\"build_version\":"
            << JsonString(SystemValue("kern.osversion"))
            << ",\"coretext_bundle_version\":"
            << JsonString(CoreTextBundleVersion()) << '}'
            << ",\"fonts\":{\"latin\":" << latin_metadata
            << ",\"cjk\":" << cjk_metadata << '}'
            << ",\"width\":400,\"height\":400,\"format\":\"A8\","
            << "\"text_size\":64,\"optical_size\":12,\"tracking\":0,\"phase_"
               "steps\":4,"
            << "\"cjk_glyphs\":[" << glyphs[0] << ',' << glyphs[1] << "],"
            << "\"cjk_advances\":[" << advances[0].width << ','
            << advances[1].width << "],"
            << "\"native_y0_at_row0\":"
            << (native_y0_at_row0 ? "true" : "false") << "}\n";
}
