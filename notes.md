https://save-buffer.github.io/bloom_filter.html#org74c8589
git@github.com:save-buffer/bloomfilter_benchmarks.git

## different hash functions

XXH3 (xxHash family):

Why: Currently widely considered the "beat-to-beat" champion for general-purpose speed. It leverages modern vector instruction sets (SSE2, AVX2, NEON) automatically but runs fast even without them.

Speed: Benchmarks often show it running at 30+ GB/s, whereas Murmur3 typically tops out around 3-5 GB/s.

Latency: Excellent performance on both small keys (short strings/IDs) and large data.

Recommendation: Use the 64-bit or 128-bit variant of XXH3.

Wyhash:

Why: A very simple, portable algorithm that is surprisingly fast. It often beats XXH3 on very short keys (small inputs), which is common for Bloom filters (e.g., hashing UUIDs, IPs, or user IDs).

Qualities: It passes SMHasher (a rigorous hash quality test suite), so its distribution is actually excellent, not mediocre.

Code Size: The implementation is extremely small (essentially a single header file).


MeowHash (Meow Hash):

Why: It relies heavily on AES-NI hardware instructions (usually used for encryption) to churn data incredibly fast.

Trade-off: It is not portable. If you try to run it on a machine without AES hardware support, it won't work or will be slow.

Speed: Can exceed 50 GB/s.
