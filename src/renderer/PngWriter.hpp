#pragma once
// renderer/PngWriter.hpp
// Tiny dependency-free RGBA8 PNG writer using uncompressed DEFLATE blocks.

#include "math/Scalars.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace ndde::renderer {

namespace detail {
inline void write_be32(std::vector<byte>& out, std::uint32_t v) {
    out.push_back(static_cast<byte>((v >> 24) & 0xffu));
    out.push_back(static_cast<byte>((v >> 16) & 0xffu));
    out.push_back(static_cast<byte>((v >>  8) & 0xffu));
    out.push_back(static_cast<byte>( v        & 0xffu));
}

inline std::uint32_t crc32(const byte* data, std::size_t size) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc & 1u) ? (crc >> 1u) ^ 0xedb88320u : (crc >> 1u);
    }
    return crc ^ 0xffffffffu;
}

inline std::uint32_t adler32(const byte* data, std::size_t size) {
    constexpr std::uint32_t mod = 65521u;
    std::uint32_t a = 1u;
    std::uint32_t b = 0u;
    for (std::size_t i = 0; i < size; ++i) {
        a = (a + data[i]) % mod;
        b = (b + a) % mod;
    }
    return (b << 16u) | a;
}

inline void append_chunk(std::vector<byte>& png,
                         const char type[4],
                         const std::vector<byte>& payload) {
    write_be32(png, static_cast<std::uint32_t>(payload.size()));
    const std::size_t type_pos = png.size();
    png.insert(png.end(), type, type + 4);
    png.insert(png.end(), payload.begin(), payload.end());
    const std::uint32_t crc = crc32(png.data() + type_pos, png.size() - type_pos);
    write_be32(png, crc);
}
} // namespace detail

inline void write_png_rgba8(const std::filesystem::path& path,
                            u32 width,
                            u32 height,
                            const std::vector<byte>& rgba) {
    if (width == 0 || height == 0)
        throw std::runtime_error("[PngWriter] Empty image");
    const std::size_t expected = static_cast<std::size_t>(width) * height * 4u;
    if (rgba.size() != expected)
        throw std::runtime_error("[PngWriter] RGBA buffer size mismatch");

    std::vector<byte> scanlines;
    scanlines.reserve(static_cast<std::size_t>(height) * (static_cast<std::size_t>(width) * 4u + 1u));
    for (u32 y = 0; y < height; ++y) {
        scanlines.push_back(0); // filter type 0
        const auto* row = rgba.data() + static_cast<std::size_t>(y) * width * 4u;
        scanlines.insert(scanlines.end(), row, row + static_cast<std::size_t>(width) * 4u);
    }

    std::vector<byte> zlib;
    zlib.reserve(scanlines.size() + scanlines.size() / 65535u * 5u + 16u);
    zlib.push_back(0x78u);
    zlib.push_back(0x01u); // zlib header: no compression/fastest

    std::size_t pos = 0;
    while (pos < scanlines.size()) {
        const std::size_t remaining = scanlines.size() - pos;
        const std::uint16_t block_len = static_cast<std::uint16_t>(
            remaining > 65535u ? 65535u : remaining);
        const bool final = (pos + block_len) == scanlines.size();
        zlib.push_back(final ? 0x01u : 0x00u);
        zlib.push_back(static_cast<byte>(block_len & 0xffu));
        zlib.push_back(static_cast<byte>((block_len >> 8u) & 0xffu));
        const std::uint16_t nlen = static_cast<std::uint16_t>(~block_len);
        zlib.push_back(static_cast<byte>(nlen & 0xffu));
        zlib.push_back(static_cast<byte>((nlen >> 8u) & 0xffu));
        zlib.insert(zlib.end(), scanlines.begin() + static_cast<std::ptrdiff_t>(pos),
                    scanlines.begin() + static_cast<std::ptrdiff_t>(pos + block_len));
        pos += block_len;
    }

    detail::write_be32(zlib, detail::adler32(scanlines.data(), scanlines.size()));

    std::vector<byte> png{
        0x89u, 'P', 'N', 'G', '\r', '\n', 0x1au, '\n'
    };

    std::vector<byte> ihdr;
    detail::write_be32(ihdr, width);
    detail::write_be32(ihdr, height);
    ihdr.push_back(8); // bit depth
    ihdr.push_back(6); // RGBA
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    detail::append_chunk(png, "IHDR", ihdr);
    detail::append_chunk(png, "IDAT", zlib);
    detail::append_chunk(png, "IEND", {});

    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("[PngWriter] Cannot open output file");
    file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
}

} // namespace ndde::renderer
