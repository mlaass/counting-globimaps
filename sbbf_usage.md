 # 2D dataset - auto-detects dimensions
  ./sbbf_benchmark --scenario sweep --dataset ../datasets/hdf5/gdelt_events.h5 \
    --output ../sbbf_results/gdelt.json

  # 3D dataset - auto-detects dimensions
  ./sbbf_benchmark --scenario sweep --dataset ../datasets/hdf5/gdelt_events_multicategory.h5 \
    --output ../sbbf_results/gdelt_3d.json

  # Synthetic data - still works
  ./sbbf_benchmark --suite quick --output ../sbbf_results/quick.json

  Results are saved in sbbf_results/ with proper JSON metadata showing distribution: "dataset" and the actual element count.
