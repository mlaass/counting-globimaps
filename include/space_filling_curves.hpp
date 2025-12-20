/*
 * Space-Filling Curves for Spatial Indexing
 *
 * Provides Morton (Z-order) and Hilbert curve encoding/decoding for 2D and 3D
 * coordinates. These curves map multidimensional coordinates to 1D values while
 * preserving spatial locality.
 *
 * Morton curves use bit-interleaving (fast, uses PDEP/PEXT when available).
 * Hilbert curves use state-machine lookup tables (better locality, more complex).
 *
 * References:
 * - Morton: G.M. Morton, "A Computer Oriented Geodetic Data Base", IBM 1966
 * - Hilbert 2D: Standard state-machine approach
 * - Hilbert 3D: Jia et al., "Efficient 3D Hilbert Curve Encoding and Decoding
 *   Algorithms", Chinese J. of Electronics, 2022 (JFK-3HE/JFK-3HD)
 */

#ifndef SPACE_FILLING_CURVES_HPP
#define SPACE_FILLING_CURVES_HPP

#include <cstdint>
#include <type_traits>

// Detect BMI2 support for PDEP/PEXT instructions
#if defined(__BMI2__) && defined(__x86_64__)
#define SFC_HAS_BMI2 1
#include <immintrin.h>
#else
#define SFC_HAS_BMI2 0
#endif

namespace sfc {

// ============================================================================
// Morton 2D - Z-order curve for 2D coordinates
// ============================================================================

/**
 * Morton (Z-order) encoding for 2D coordinates.
 * Interleaves bits: (x, y) -> ...y2x2y1x1y0x0
 *
 * Template parameter Bits: max bits per coordinate (up to 32).
 * Output is 2*Bits wide (max 64 bits).
 */
template <unsigned Bits = 16>
struct Morton2D {
    static_assert(Bits <= 32, "Morton2D supports up to 32 bits per coordinate");
    static_assert(Bits > 0, "Bits must be positive");

    /// Maximum valid coordinate value
    static constexpr uint64_t max_coord = (1ULL << Bits) - 1;

    /// Maximum Morton code value
    static constexpr uint64_t max_code = (1ULL << (2 * Bits)) - 1;

#if SFC_HAS_BMI2
    /**
     * Encode (x, y) to Morton code using BMI2 PDEP instruction.
     * PDEP spreads bits according to a mask pattern.
     */
    static inline uint64_t encode(uint32_t x, uint32_t y) {
        // 0x5555... = 0101010101... (every other bit for x)
        // 0xAAAA... = 1010101010... (every other bit for y, shifted left 1)
        return _pdep_u64(x, 0x5555555555555555ULL) |
               _pdep_u64(y, 0xAAAAAAAAAAAAAAAAULL);
    }

    /**
     * Decode Morton code to (x, y) using BMI2 PEXT instruction.
     * PEXT extracts bits according to a mask pattern.
     */
    static inline void decode(uint64_t code, uint32_t &x, uint32_t &y) {
        x = static_cast<uint32_t>(_pext_u64(code, 0x5555555555555555ULL));
        y = static_cast<uint32_t>(_pext_u64(code, 0xAAAAAAAAAAAAAAAAULL));
    }
#else
    /**
     * Portable Morton encode using magic-number bit spreading.
     * Based on: https://graphics.stanford.edu/~seander/bithacks.html
     */
    static inline uint64_t encode(uint32_t x, uint32_t y) {
        return spread_bits(x) | (spread_bits(y) << 1);
    }

    /**
     * Portable Morton decode using magic-number bit compaction.
     */
    static inline void decode(uint64_t code, uint32_t &x, uint32_t &y) {
        x = compact_bits(code);
        y = compact_bits(code >> 1);
    }

private:
    /// Spread bits of a 32-bit value to occupy every other bit position
    static inline uint64_t spread_bits(uint32_t v) {
        uint64_t w = v;
        w = (w | (w << 16)) & 0x0000FFFF0000FFFFULL;
        w = (w | (w << 8)) & 0x00FF00FF00FF00FFULL;
        w = (w | (w << 4)) & 0x0F0F0F0F0F0F0F0FULL;
        w = (w | (w << 2)) & 0x3333333333333333ULL;
        w = (w | (w << 1)) & 0x5555555555555555ULL;
        return w;
    }

    /// Compact bits from every other position back to contiguous
    static inline uint32_t compact_bits(uint64_t w) {
        w &= 0x5555555555555555ULL;
        w = (w | (w >> 1)) & 0x3333333333333333ULL;
        w = (w | (w >> 2)) & 0x0F0F0F0F0F0F0F0FULL;
        w = (w | (w >> 4)) & 0x00FF00FF00FF00FFULL;
        w = (w | (w >> 8)) & 0x0000FFFF0000FFFFULL;
        w = (w | (w >> 16)) & 0x00000000FFFFFFFFULL;
        return static_cast<uint32_t>(w);
    }
#endif
};

// ============================================================================
// Morton 3D - Z-order curve for 3D coordinates
// ============================================================================

/**
 * Morton (Z-order) encoding for 3D coordinates.
 * Interleaves bits: (x, y, z) -> ...z2y2x2z1y1x1z0y0x0
 *
 * Template parameter Bits: max bits per coordinate (up to 21 for 63-bit output).
 * Output is 3*Bits wide.
 */
template <unsigned Bits = 16>
struct Morton3D {
    static_assert(Bits <= 21, "Morton3D supports up to 21 bits per coordinate (63-bit output)");
    static_assert(Bits > 0, "Bits must be positive");

    /// Maximum valid coordinate value
    static constexpr uint64_t max_coord = (1ULL << Bits) - 1;

    /// Maximum Morton code value
    static constexpr uint64_t max_code = (1ULL << (3 * Bits)) - 1;

#if SFC_HAS_BMI2
    /**
     * Encode (x, y, z) to Morton code using BMI2 PDEP instruction.
     */
    static inline uint64_t encode(uint32_t x, uint32_t y, uint32_t z) {
        // Pattern for x: bits at positions 0, 3, 6, 9, ... (every 3rd bit starting at 0)
        // Pattern for y: bits at positions 1, 4, 7, 10, ... (every 3rd bit starting at 1)
        // Pattern for z: bits at positions 2, 5, 8, 11, ... (every 3rd bit starting at 2)
        constexpr uint64_t mask_x = 0x1249249249249249ULL; // 001001001... in binary
        constexpr uint64_t mask_y = 0x2492492492492492ULL; // 010010010... in binary
        constexpr uint64_t mask_z = 0x4924924924924924ULL; // 100100100... in binary

        return _pdep_u64(x, mask_x) | _pdep_u64(y, mask_y) | _pdep_u64(z, mask_z);
    }

    /**
     * Decode Morton code to (x, y, z) using BMI2 PEXT instruction.
     */
    static inline void decode(uint64_t code, uint32_t &x, uint32_t &y, uint32_t &z) {
        constexpr uint64_t mask_x = 0x1249249249249249ULL;
        constexpr uint64_t mask_y = 0x2492492492492492ULL;
        constexpr uint64_t mask_z = 0x4924924924924924ULL;

        x = static_cast<uint32_t>(_pext_u64(code, mask_x));
        y = static_cast<uint32_t>(_pext_u64(code, mask_y));
        z = static_cast<uint32_t>(_pext_u64(code, mask_z));
    }
#else
    /**
     * Portable Morton 3D encode using magic-number bit spreading.
     */
    static inline uint64_t encode(uint32_t x, uint32_t y, uint32_t z) {
        return spread_bits_3d(x) | (spread_bits_3d(y) << 1) | (spread_bits_3d(z) << 2);
    }

    /**
     * Portable Morton 3D decode.
     */
    static inline void decode(uint64_t code, uint32_t &x, uint32_t &y, uint32_t &z) {
        x = compact_bits_3d(code);
        y = compact_bits_3d(code >> 1);
        z = compact_bits_3d(code >> 2);
    }

private:
    /// Spread bits of a 21-bit value to occupy every 3rd bit position
    static inline uint64_t spread_bits_3d(uint32_t v) {
        uint64_t w = v & 0x1FFFFF; // Mask to 21 bits
        w = (w | (w << 32)) & 0x1F00000000FFFFULL;
        w = (w | (w << 16)) & 0x1F0000FF0000FFULL;
        w = (w | (w << 8)) & 0x100F00F00F00F00FULL;
        w = (w | (w << 4)) & 0x10C30C30C30C30C3ULL;
        w = (w | (w << 2)) & 0x1249249249249249ULL;
        return w;
    }

    /// Compact bits from every 3rd position back to contiguous
    static inline uint32_t compact_bits_3d(uint64_t w) {
        w &= 0x1249249249249249ULL;
        w = (w | (w >> 2)) & 0x10C30C30C30C30C3ULL;
        w = (w | (w >> 4)) & 0x100F00F00F00F00FULL;
        w = (w | (w >> 8)) & 0x1F0000FF0000FFULL;
        w = (w | (w >> 16)) & 0x1F00000000FFFFULL;
        w = (w | (w >> 32)) & 0x1FFFFFULL;
        return static_cast<uint32_t>(w);
    }
#endif
};

// ============================================================================
// Hilbert 2D - Hilbert curve for 2D coordinates
// ============================================================================

/**
 * Hilbert curve encoding for 2D coordinates.
 * Uses state-machine approach with 4 states.
 * Better locality preservation than Morton at the cost of complexity.
 *
 * Template parameter Bits: max bits per coordinate (up to 32).
 * Output is 2*Bits wide.
 */
template <unsigned Bits = 16>
struct Hilbert2D {
    static_assert(Bits <= 32, "Hilbert2D supports up to 32 bits per coordinate");
    static_assert(Bits > 0, "Bits must be positive");

    /// Maximum valid coordinate value
    static constexpr uint64_t max_coord = (1ULL << Bits) - 1;

    /// Maximum Hilbert code value
    static constexpr uint64_t max_code = (1ULL << (2 * Bits)) - 1;

    /**
     * Encode (x, y) to Hilbert code.
     *
     * The curve starts at (0,0), goes through all 2^(2*Bits) points,
     * and ends at (2^Bits-1, 0).
     */
    static inline uint64_t encode(uint32_t x, uint32_t y) {
        uint64_t code = 0;
        uint32_t rx, ry, s;

        // Process from most significant bits to least
        for (s = (1U << (Bits - 1)); s > 0; s >>= 1) {
            rx = (x & s) > 0 ? 1 : 0;
            ry = (y & s) > 0 ? 1 : 0;
            code += s * s * ((3 * rx) ^ ry);
            rotate(s, x, y, rx, ry);
        }
        return code;
    }

    /**
     * Decode Hilbert code to (x, y).
     */
    static inline void decode(uint64_t code, uint32_t &x, uint32_t &y) {
        uint32_t rx, ry, s, t = code;
        x = y = 0;

        for (s = 1; s < (1U << Bits); s <<= 1) {
            rx = 1 & (t / 2);
            ry = 1 & (t ^ rx);
            rotate(s, x, y, rx, ry);
            x += s * rx;
            y += s * ry;
            t /= 4;
        }
    }

private:
    /// Rotate/flip quadrant appropriately
    static inline void rotate(uint32_t n, uint32_t &x, uint32_t &y, uint32_t rx, uint32_t ry) {
        if (ry == 0) {
            if (rx == 1) {
                x = n - 1 - x;
                y = n - 1 - y;
            }
            // Swap x and y
            uint32_t t = x;
            x = y;
            y = t;
        }
    }
};

// ============================================================================
// Hilbert 3D - Hilbert curve for 3D coordinates
// ============================================================================

/**
 * 3D Hilbert curve using rotation-based algorithm.
 * Based on the approach from "Programming the Hilbert Curve" by John Skilling.
 *
 * This implementation uses coordinate transformations (rotations and reflections)
 * to recursively construct the 3D Hilbert curve.
 *
 * Template parameter Bits: max bits per coordinate (up to 21 for 63-bit output).
 * Output is 3*Bits wide.
 */
template <unsigned Bits = 10>
struct Hilbert3D {
    static_assert(Bits <= 21, "Hilbert3D supports up to 21 bits per coordinate");
    static_assert(Bits > 0, "Bits must be positive");

    /// Maximum valid coordinate value
    static constexpr uint64_t max_coord = (1ULL << Bits) - 1;

    /// Maximum Hilbert code value
    static constexpr uint64_t max_code = (1ULL << (3 * Bits)) - 1;

    /**
     * Encode (x, y, z) to 3D Hilbert code.
     */
    static inline uint64_t encode(uint32_t x, uint32_t y, uint32_t z) {
        uint32_t coords[3] = {x, y, z};
        uint64_t code = 0;

        for (uint32_t s = (1U << (Bits - 1)); s > 0; s >>= 1) {
            uint32_t rx = (coords[0] & s) > 0 ? 1 : 0;
            uint32_t ry = (coords[1] & s) > 0 ? 1 : 0;
            uint32_t rz = (coords[2] & s) > 0 ? 1 : 0;

            // Calculate the 3-bit index for this iteration
            uint32_t idx = rx | (ry << 1) | (rz << 2);

            // Gray code transformation for Hilbert ordering
            // The index determines which subcube we're in
            code = (code << 3) | gray_encode_3d(idx);

            // Rotate coordinates for next level
            rotate_3d(s, coords, rx, ry, rz);
        }
        return code;
    }

    /**
     * Decode 3D Hilbert code to (x, y, z).
     */
    static inline void decode(uint64_t code, uint32_t &x, uint32_t &y, uint32_t &z) {
        uint32_t coords[3] = {0, 0, 0};

        for (uint32_t s = 1; s < (1U << Bits); s <<= 1) {
            // Extract 3 bits from code
            uint32_t gray = code & 7;
            code >>= 3;

            // Inverse Gray code
            uint32_t idx = gray_decode_3d(gray);

            uint32_t rx = idx & 1;
            uint32_t ry = (idx >> 1) & 1;
            uint32_t rz = (idx >> 2) & 1;

            // Inverse rotation
            rotate_3d_inv(s, coords, rx, ry, rz);

            coords[0] += s * rx;
            coords[1] += s * ry;
            coords[2] += s * rz;
        }

        x = coords[0];
        y = coords[1];
        z = coords[2];
    }

private:
    /// 3D Gray code encode (for Hilbert ordering)
    static inline uint32_t gray_encode_3d(uint32_t i) {
        return i ^ (i >> 1);
    }

    /// 3D Gray code decode (inverse)
    static inline uint32_t gray_decode_3d(uint32_t g) {
        uint32_t i = g;
        i ^= (i >> 1);
        i ^= (i >> 2);
        return i;
    }

    /// Rotate/flip coordinates for encoding
    static inline void rotate_3d(uint32_t n, uint32_t *coords, uint32_t rx, uint32_t ry, uint32_t rz) {
        if (rz == 0) {
            if (ry == 0) {
                if (rx == 0) {
                    // Rotation A: swap x and y
                    uint32_t t = coords[0];
                    coords[0] = coords[1];
                    coords[1] = t;
                } else {
                    // Rotation B: swap y and z, flip x
                    coords[0] = n - 1 - coords[0];
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                }
            } else {
                if (rx == 0) {
                    // Rotation C: swap x and z
                    uint32_t t = coords[0];
                    coords[0] = coords[2];
                    coords[2] = t;
                } else {
                    // Rotation D: swap y and z
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                }
            }
        } else {
            if (ry == 0) {
                if (rx == 0) {
                    // Rotation E: swap x and y, flip z
                    coords[2] = n - 1 - coords[2];
                    uint32_t t = coords[0];
                    coords[0] = coords[1];
                    coords[1] = t;
                } else {
                    // Rotation F: flip y, swap y and z
                    coords[1] = n - 1 - coords[1];
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                }
            } else {
                if (rx == 0) {
                    // Rotation G: flip x, swap x and z
                    coords[0] = n - 1 - coords[0];
                    uint32_t t = coords[0];
                    coords[0] = coords[2];
                    coords[2] = t;
                } else {
                    // Rotation H: flip all, swap y and z
                    coords[0] = n - 1 - coords[0];
                    coords[1] = n - 1 - coords[1];
                    coords[2] = n - 1 - coords[2];
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                }
            }
        }
    }

    /// Inverse rotation for decoding
    static inline void rotate_3d_inv(uint32_t n, uint32_t *coords, uint32_t rx, uint32_t ry, uint32_t rz) {
        if (rz == 0) {
            if (ry == 0) {
                if (rx == 0) {
                    // Inverse of A: swap x and y
                    uint32_t t = coords[0];
                    coords[0] = coords[1];
                    coords[1] = t;
                } else {
                    // Inverse of B
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                    coords[0] = n - 1 - coords[0];
                }
            } else {
                if (rx == 0) {
                    // Inverse of C: swap x and z
                    uint32_t t = coords[0];
                    coords[0] = coords[2];
                    coords[2] = t;
                } else {
                    // Inverse of D: swap y and z
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                }
            }
        } else {
            if (ry == 0) {
                if (rx == 0) {
                    // Inverse of E
                    uint32_t t = coords[0];
                    coords[0] = coords[1];
                    coords[1] = t;
                    coords[2] = n - 1 - coords[2];
                } else {
                    // Inverse of F
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                    coords[1] = n - 1 - coords[1];
                }
            } else {
                if (rx == 0) {
                    // Inverse of G
                    uint32_t t = coords[0];
                    coords[0] = coords[2];
                    coords[2] = t;
                    coords[0] = n - 1 - coords[0];
                } else {
                    // Inverse of H
                    uint32_t t = coords[1];
                    coords[1] = coords[2];
                    coords[2] = t;
                    coords[0] = n - 1 - coords[0];
                    coords[1] = n - 1 - coords[1];
                    coords[2] = n - 1 - coords[2];
                }
            }
        }
    }
};

// ============================================================================
// SFC Type Enumeration and Utilities
// ============================================================================

/// Space-filling curve type selector
enum class SFCType { MORTON_2D, MORTON_3D, HILBERT_2D, HILBERT_3D };

/**
 * Compute the Manhattan distance between two 2D points.
 * Useful for measuring SFC locality.
 */
inline uint64_t manhattan_distance_2d(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2) {
    int64_t dx = static_cast<int64_t>(x1) - static_cast<int64_t>(x2);
    int64_t dy = static_cast<int64_t>(y1) - static_cast<int64_t>(y2);
    return static_cast<uint64_t>((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
}

/**
 * Compute the Manhattan distance between two 3D points.
 */
inline uint64_t manhattan_distance_3d(uint32_t x1, uint32_t y1, uint32_t z1,
                                       uint32_t x2, uint32_t y2, uint32_t z2) {
    int64_t dx = static_cast<int64_t>(x1) - static_cast<int64_t>(x2);
    int64_t dy = static_cast<int64_t>(y1) - static_cast<int64_t>(y2);
    int64_t dz = static_cast<int64_t>(z1) - static_cast<int64_t>(z2);
    return static_cast<uint64_t>((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy) + (dz < 0 ? -dz : dz));
}

} // namespace sfc

#endif // SPACE_FILLING_CURVES_HPP
