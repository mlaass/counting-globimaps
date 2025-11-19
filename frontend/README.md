# CBF Dataset Explorer Frontend

React + TypeScript web application for exploring spatial datasets using WebAssembly-powered counting bloom filters.

## Features

- 🗺️ **Interactive Query Interface** - Query coordinates and categories
- 📊 **Filter Metadata Display** - View filter type, memory usage, configuration
- 🚀 **WebAssembly Performance** - Native C++ speed in the browser
- 📁 **File Upload** - Load `.cbf` files or use test data
- 🎨 **Responsive Design** - Works on desktop and mobile

## Quick Start

### 1. Install Dependencies

```bash
npm install
```

### 2. Build WASM Module

```bash
cd ../wasm
./build.sh
cd build
make install  # Copies to frontend/public/wasm/
```

### 3. Start Development Server

```bash
cd ../../frontend
npm start
```

Open [http://localhost:3000](http://localhost:3000) to view the app.

## Available Scripts

### `npm start`

Runs the app in development mode.
The page will reload if you make edits.

### `npm test`

Launches the test runner in interactive watch mode.

### `npm run build`

Builds the app for production to the `build/` folder.
Optimized and minified for deployment.

### `npm run eject`

**Warning: This is a one-way operation!**
Ejects from Create React App configuration.

## Project Structure

```
frontend/
├── public/
│   ├── wasm/                  # WASM binaries
│   │   ├── cbf_wasm.js
│   │   └── cbf_wasm.wasm
│   ├── datasets/              # Pre-encoded .cbf files
│   │   └── gdelt.cbf
│   └── index.html
├── src/
│   ├── wasm/                  # WASM integration
│   │   ├── loader.ts          # WASM loader & wrapper
│   │   └── types.ts           # TypeScript types
│   ├── components/            # React components
│   │   ├── QueryPanel.tsx     # Query interface
│   │   └── FilterInfo.tsx     # Metadata display
│   ├── App.tsx                # Main app
│   └── App.css                # Styles
└── package.json
```

## Usage

### Load a Filter

**Option A: Test Filter**
- Click "Load Test Filter"
- Pre-populated sample data
- No .cbf file needed

**Option B: Upload .cbf File**
- Click "Load .cbf File"
- Select encoded dataset
- See encoder docs for creating .cbf files

### Query Coordinates

1. Enter X, Y coordinates
2. Optionally add category
3. Click "Query"
4. View count and timing

## Creating Datasets

Encode datasets using the encoder tool:

```bash
cd ../build
./encode_dataset \
  ../datasets/hdf5/gdelt_events_multicategory.h5 \
  ../frontend/public/datasets/gdelt.cbf \
  --type cms \
  --verbose
```

See `../encoder/README.md` for details.

## Deployment

### Static Hosting

Build and deploy to:
- **GitHub Pages** - Static site hosting
- **Netlify** - Auto-deploy from Git
- **Vercel** - Serverless deployment

```bash
npm run build
# Deploy build/ directory
```

### Server Configuration

For loading .cbf files:
- MIME type: `application/octet-stream`
- Enable CORS if cross-origin
- Use gzip/brotli compression

## API Reference

### CBFFilter Class

```typescript
// Load from URL
const filter = await CBFFilter.fromURL('/datasets/gdelt.cbf');

// Query
const result = filter.query({ x: 100, y: 200, category: 1 });
console.log(`Count: ${result.count}, Time: ${result.queryTime}ms`);

// Metadata
const meta = filter.getMetadata();

// Cleanup
filter.free();
```

## Troubleshooting

### WASM Module Not Found
- Check `public/wasm/` contains WASM files
- Run `make install` from `wasm/build/`
- Clear browser cache

### Invalid Filter Data
- Re-encode with current encoder
- Try test filter first

### TypeScript Errors
```bash
rm -rf node_modules
npm install
```

## Learn More

- [WASM Module](../wasm/README.md) - Building the WASM module
- [Encoder](../encoder/README.md) - Creating .cbf files
- [Main Project](../README.md) - Overall documentation
- [Create React App Docs](https://create-react-app.dev/)
- [React Documentation](https://reactjs.org/)

## Next Steps

See `../wasm/README.md` and `../encoder/README.md` to:
1. Compile WASM module
2. Encode datasets
3. Load in this frontend
