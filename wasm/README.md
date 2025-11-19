# CBF WASM Module

WebAssembly module for counting bloom filter implementations. Compiles C++ counting bloom filters to WebAssembly for use in browser-based applications.

## Features

- **Count-Min Sketch** - Probabilistic frequency estimator with error bounds
- **Spectral Bloom Filter** - Multi-variant filter with conservative updates
- **CountingGloBiMap** - Multi-layer hierarchical filter
- **d-Left Counting BF** - Cache-friendly deterministic lookups
- **Multi-category support** - Track categories at spatial locations
- **Serialization** - Load pre-encoded filters from binary files

## Prerequisites

1. **Emscripten SDK** - Required for compiling C++ to WebAssembly

```bash
# Install Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh  # Run this in every new terminal session
```

2. **Dependencies** - Git submodules (already initialized if you followed main README)

```bash
# From project root
git submodule update --init --recursive
```

## Building

```bash
# From wasm/ directory
./build.sh
```

This will:
1. Configure CMake with Emscripten
2. Build the WASM module
3. Generate `cbf_wasm.js` and `cbf_wasm.wasm` in `build/`

### Output Files

- `cbf_wasm.wasm` - WebAssembly binary (C++ code)
- `cbf_wasm.js` - JavaScript glue code (Emscripten-generated)

## Installation

Copy the built files to your frontend application:

```bash
# From wasm/build/ directory
cp cbf_wasm.{js,wasm} ../../frontend/public/wasm/
```

Or use the CMake install target:

```bash
cd build
make install  # Copies to frontend/public/wasm/
```

## JavaScript API

The WASM module exports the following functions:

### Loading Filters

```javascript
// Load WASM module (do this once at app start)
const module = await createCBFModule();

// Load filter from binary data
const filterHandle = module.loadFilter(binaryString);

// Create test filter (for development)
const testHandle = module.createTestCMS();
```

### Querying

```javascript
// Query a point (x, y, optional category)
const count = module.queryFilter(filterHandle, x, y, category);
// category = -1 for no category (2D query)
```

### Metadata

```javascript
// Get filter type
const type = module.getType(filterHandle);
// Returns: 0=CMS, 1=Spectral, 2=GloBiMap, 3=dLeft

// Get memory usage
const bytes = module.getMemory(filterHandle);

// Get filter info string
const info = module.getInfo(filterHandle);
```

### Cleanup

```javascript
// Free filter when done
module.freeFilter(filterHandle);
```

## TypeScript Integration

For TypeScript projects, see `../frontend/src/wasm/loader.ts` for a complete typed wrapper.

Example:

```typescript
import { CBFFilter } from './wasm/loader';

// Load from URL
const filter = await CBFFilter.fromURL('/datasets/gdelt.cbf');

// Query
const result = filter.query({ x: 100, y: 200, category: 1 });
console.log(`Count: ${result.count}, Time: ${result.queryTime}ms`);

// Get metadata
const metadata = filter.getMetadata();
console.log(`Type: ${metadata.typeName}, Memory: ${metadata.memoryUsage}`);

// Cleanup
filter.free();
```

## Development

### Adding New Filter Types

1. Implement `to_bytes()` / `from_bytes()` in the filter header
2. Add handle class in `wasm_bindings.cpp`
3. Update `load_filter()` to detect and load the new type
4. Rebuild WASM module

### Debugging

Enable debug mode in `CMakeLists.txt`:

```cmake
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -g -s ASSERTIONS=2")
```

Then rebuild. This adds debug symbols and assertions (larger file size).

### Performance Optimization

Current flags (`-O3 -s WASM=1`) provide good balance of size and speed.

For smaller bundle:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Oz")  # Optimize for size
```

For better performance:
```cmake
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -flto")  # Link-time optimization
```

## Troubleshooting

### "Failed to load WASM module"

- Check that `cbf_wasm.wasm` is in `public/wasm/` directory
- Verify CORS headers if loading from different origin
- Check browser console for detailed error

### "Invalid filter data"

- Ensure .cbf file was generated with compatible encoder
- Check binary format version matches
- Try loading test filter to verify WASM works

### Build Errors

- Ensure Emscripten is activated: `source emsdk/emsdk_env.sh`
- Check CMake version >= 3.10
- Verify git submodules are initialized

## File Structure

```
wasm/
├── src/
│   ├── wasm_bindings.cpp      # Emscripten bindings
│   └── serialization.hpp      # Binary format (not used - methods in headers now)
├── CMakeLists.txt             # Emscripten build config
├── build.sh                   # Build script
├── build/                     # Build output (generated)
└── README.md                  # This file
```

## Next Steps

1. **Build the WASM module** - Run `./build.sh`
2. **Copy to frontend** - Install files to `frontend/public/wasm/`
3. **Encode a dataset** - Use `../build/encode_dataset` to create .cbf files
4. **Launch frontend** - See `../frontend/README.md`

## Related Documentation

- [Main Project README](../README.md)
- [Frontend README](../frontend/README.md)
- [Encoder README](../encoder/README.md)
- [CLAUDE.md](../CLAUDE.md) - Full project documentation
