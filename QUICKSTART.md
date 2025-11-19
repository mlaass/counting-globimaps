# Quick Start Guide - CBF WASM Dataset Explorer

## ✅ Status: READY TO RUN!

All components have been successfully built and configured:

- ✅ WASM module compiled (162 KB)
- ✅ Encoder tool built and tested
- ✅ GDELT dataset encoded (1.9M points → 2.7 KB)
- ✅ React frontend ready

## 🚀 Start the Application

```bash
cd frontend
npm start
```

Open http://localhost:3000 and click **"Load Test Filter"** to test immediately!

## 📊 Example Queries

### Test Filter
- X: 100, Y: 200, Category: 1 → count ~2
- X: 100, Y: 200, Category: 2 → count ~3

### GDELT Dataset (load gdelt.cbf)
1.9M global news events with 4 categories on 3600×1800 grid

## 📚 Full Documentation

- `wasm/README.md` - WASM compilation
- `encoder/README.md` - Dataset encoding
- `frontend/README.md` - Frontend usage
- `CLAUDE.md` - Complete project docs
