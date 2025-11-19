import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';
import path from 'path';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    open: true,
    // Watch for changes in public directory and trigger reload
    watch: {
      usePolling: true,
      interval: 100,
    },
  },
  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src')
    },
  },
  // Ensure WASM files are served correctly
  assetsInclude: ['**/*.wasm'],
  optimizeDeps: {
    include: ['leaflet', 'react-leaflet'],
  },
});
