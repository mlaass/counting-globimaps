/**
 * WASM Module Loader for CBF implementations
 *
 * Wraps Emscripten-generated module in a TypeScript-friendly interface.
 */

import { FilterType, FilterMetadata, QueryResult, Point } from './types';

/**
 * Native WASM module interface (from Emscripten)
 */
interface NativeCBFModule {
  loadFilter: (bytes: string) => number; // Returns FilterHandle pointer
  loadFilterBase64: (base64: string) => number; // Returns FilterHandle pointer (safe binary transfer)
  queryFilter: (handle: number, x: number, y: number, category: number) => number | bigint;
  getMemory: (handle: number) => number | bigint;
  getInfo: (handle: number) => string;
  getType: (handle: number) => number;
  freeFilter: (handle: number) => void;
  createTestCMS: () => number; // For development
  FilterType: typeof FilterType;
}

/**
 * Promise that resolves to loaded WASM module
 */
let wasmModule: NativeCBFModule | null = null;
let wasmLoading: Promise<NativeCBFModule> | null = null;

/**
 * Load WASM module (singleton pattern)
 */
async function loadWasmModule(): Promise<NativeCBFModule> {
  if (wasmModule) {
    return wasmModule;
  }

  if (wasmLoading) {
    return wasmLoading;
  }

  wasmLoading = new Promise((resolve, reject) => {
    // Load the Emscripten-generated module
    // Note: cbf_wasm.js should export createCBFModule function
    const script = document.createElement('script');
    script.src = '/wasm/cbf_wasm.js';
    script.async = true;

    script.onload = () => {
      // @ts-ignore - Emscripten adds global function
      const createModule = window.createCBFModule;

      if (!createModule) {
        reject(new Error('WASM module not found. Did you compile the WASM module?'));
        return;
      }

      createModule().then((module: NativeCBFModule) => {
        wasmModule = module;
        console.log('CBF WASM module loaded successfully');
        resolve(module);
      }).catch((error: Error) => {
        reject(new Error(`Failed to initialize WASM module: ${error.message}`));
      });
    };

    script.onerror = () => {
      reject(new Error('Failed to load WASM script'));
    };

    document.body.appendChild(script);
  });

  return wasmLoading;
}

/**
 * CBF Filter wrapper class
 *
 * Provides high-level TypeScript interface for querying filters.
 */
export class CBFFilter {
  private handle: number;
  private module: NativeCBFModule;
  private metadata: FilterMetadata;

  private constructor(
    handle: number,
    module: NativeCBFModule,
    metadata: FilterMetadata
  ) {
    this.handle = handle;
    this.module = module;
    this.metadata = metadata;
  }

  /**
   * Load a filter from binary data (Uint8Array from .cbf file)
   */
  static async fromBytes(bytes: Uint8Array): Promise<CBFFilter> {
    const module = await loadWasmModule();

    console.log('Loading filter from bytes, size:', bytes.length);

    // Convert Uint8Array to base64 to safely pass through string
    const base64 = btoa(String.fromCharCode(...bytes));

    console.log('Converted to base64, length:', base64.length);
    console.log('First 32 bytes (hex):', Array.from(bytes.slice(0, 32))
      .map(b => b.toString(16).padStart(2, '0')).join(' '));

    // Load filter
    let handle;
    try {
      handle = module.loadFilterBase64(base64);
    } catch (err) {
      console.error('WASM loadFilter threw exception:', err);
      throw err;
    }

    if (handle === 0 || handle === null) {
      throw new Error('Failed to load filter from bytes');
    }

    // Get metadata
    const type = module.getType(handle) as FilterType;
    const memoryUsageRaw = module.getMemory(handle);
    const info = module.getInfo(handle);

    // Convert BigInt to Number (WASM returns uint64_t as BigInt)
    const memoryUsage = typeof memoryUsageRaw === 'bigint' ? Number(memoryUsageRaw) : memoryUsageRaw;

    const metadata: FilterMetadata = {
      type,
      typeName: FilterType[type] || 'Unknown',
      memoryUsage,
      info,
    };

    return new CBFFilter(handle, module, metadata);
  }

  /**
   * Load a filter from URL (.cbf file)
   */
  static async fromURL(url: string): Promise<CBFFilter> {
    console.log('Fetching filter from URL:', url);

    const response = await fetch(url);

    if (!response.ok) {
      console.error('Failed to fetch:', response.status, response.statusText);
      throw new Error(`Failed to fetch filter: ${response.statusText}`);
    }

    const arrayBuffer = await response.arrayBuffer();
    const bytes = new Uint8Array(arrayBuffer);

    console.log('Fetched', bytes.length, 'bytes from', url);

    return CBFFilter.fromBytes(bytes);
  }

  /**
   * Create a test filter (for development without .cbf file)
   */
  static async createTest(): Promise<CBFFilter> {
    const module = await loadWasmModule();
    const handle = module.createTestCMS();

    const memoryUsageRaw = module.getMemory(handle);
    const memoryUsage = typeof memoryUsageRaw === 'bigint' ? Number(memoryUsageRaw) : memoryUsageRaw;

    const metadata: FilterMetadata = {
      type: FilterType.COUNT_MIN_SKETCH,
      typeName: 'Count-Min Sketch (Test)',
      memoryUsage,
      info: module.getInfo(handle),
    };

    return new CBFFilter(handle, module, metadata);
  }

  /**
   * Query a point in the filter
   */
  query(point: Point): QueryResult {
    const startTime = performance.now();

    const countRaw = this.module.queryFilter(
      this.handle,
      point.x,
      point.y,
      point.category ?? -1
    );

    const queryTime = performance.now() - startTime;

    // Convert BigInt to Number (WASM returns uint64_t as BigInt)
    const count = typeof countRaw === 'bigint' ? Number(countRaw) : countRaw;

    return { count, queryTime };
  }

  /**
   * Query multiple points (batch)
   */
  queryBatch(points: Point[]): QueryResult[] {
    return points.map((point) => this.query(point));
  }

  /**
   * Get filter metadata
   */
  getMetadata(): FilterMetadata {
    return { ...this.metadata };
  }

  /**
   * Free the filter (must be called when done)
   */
  free(): void {
    if (this.handle !== 0) {
      this.module.freeFilter(this.handle);
      this.handle = 0;
    }
  }
}

/**
 * Preload WASM module (call on app initialization)
 */
export async function preloadWasm(): Promise<void> {
  await loadWasmModule();
}
