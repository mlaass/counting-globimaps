#!/usr/bin/env python3
"""
Tests for CountingGloBiMap Python bindings

This test suite verifies the functionality of the CountingGloBiMap Python module:
- Configuration creation
- Basic put/get operations
- Cardinality estimation
- Multi-layer behavior
- Error detection
"""

import unittest
import counting_globimap as cg


class TestFilterConfig(unittest.TestCase):
    """Test FilterConfig and LayerConfig creation"""

    def test_layer_config_creation(self):
        """Test creating LayerConfig objects"""
        layer = cg.LayerConfig()
        layer.bits = 8
        layer.logsize = 20

        self.assertEqual(layer.bits, 8)
        self.assertEqual(layer.logsize, 20)

    def test_filter_config_creation(self):
        """Test creating FilterConfig objects"""
        config = cg.FilterConfig()
        config.hash_k = 8
        config.layers = [cg.LayerConfig()]
        config.layers[0].bits = 8
        config.layers[0].logsize = 20

        self.assertEqual(config.hash_k, 8)
        self.assertEqual(len(config.layers), 1)

    def test_make_single_layer_config(self):
        """Test helper function for single-layer config"""
        config = cg.make_single_layer_config(k=8, bits=8, logsize=20)

        self.assertEqual(config.hash_k, 8)
        self.assertEqual(len(config.layers), 1)
        self.assertEqual(config.layers[0].bits, 8)
        self.assertEqual(config.layers[0].logsize, 20)

    def test_make_multi_layer_config(self):
        """Test helper function for multi-layer config"""
        config = cg.make_multi_layer_config(
            k=10,
            layer_specs=[(8, 24), (16, 20), (32, 16)]
        )

        self.assertEqual(config.hash_k, 10)
        self.assertEqual(len(config.layers), 3)
        self.assertEqual(config.layers[0].bits, 8)
        self.assertEqual(config.layers[0].logsize, 24)
        self.assertEqual(config.layers[1].bits, 16)
        self.assertEqual(config.layers[2].bits, 32)


class TestBasicOperations(unittest.TestCase):
    """Test basic CountingGloBiMap operations"""

    def setUp(self):
        """Create a filter for testing"""
        config = cg.make_single_layer_config(k=8, bits=8, logsize=20)
        self.cbf = cg.CountingGloBiMap(config, collect_input=False)

    def test_filter_creation(self):
        """Test that filter is created correctly"""
        self.assertIsNotNone(self.cbf)
        self.assertEqual(self.cbf.hashcount, 8)
        self.assertFalse(self.cbf.collect_input)

    def test_byte_size(self):
        """Test memory size reporting"""
        # Single layer with 2^20 8-bit counters = 1MB
        expected_size = 2**20
        self.assertEqual(self.cbf.byte_size(), expected_size)

    def test_put_and_get_bool(self):
        """Test inserting and querying points"""
        # Insert a point
        self.cbf.put([100, 200])

        # Should exist
        self.assertTrue(self.cbf.get_bool([100, 200]))

        # Point not inserted should likely not exist
        # (could have false positive, but unlikely with good config)
        result = self.cbf.get_bool([999, 999])
        # We don't assert False here due to probabilistic nature

    def test_put_multiple_same_point(self):
        """Test inserting the same point multiple times"""
        point = [100, 200]

        # Insert 3 times
        for _ in range(3):
            self.cbf.put(point)

        # Should exist
        self.assertTrue(self.cbf.get_bool(point))

        # Count should be at least 1 (may be higher due to hash collisions)
        count = self.cbf.get_min(point)
        self.assertGreaterEqual(count, 1)

    def test_get_min_cardinality(self):
        """Test min-count cardinality estimation"""
        point = [100, 200]

        # Insert 5 times
        for _ in range(5):
            self.cbf.put(point)

        count = self.cbf.get_min(point)

        # Should be around 5 (may have small error)
        self.assertGreaterEqual(count, 3)  # Allow some error
        self.assertLessEqual(count, 10)

    def test_get_mean_cardinality(self):
        """Test mean-count cardinality estimation"""
        point = [100, 200]

        # Insert 5 times
        for _ in range(5):
            self.cbf.put(point)

        mean_count = self.cbf.get_mean(point)

        # Should be around 5.0
        self.assertGreater(mean_count, 0.0)
        self.assertIsInstance(mean_count, float)

    def test_summary(self):
        """Test summary JSON output"""
        # Insert some points
        self.cbf.put([100, 200])
        self.cbf.put([150, 250])

        summary = self.cbf.summary()
        self.assertIsInstance(summary, str)
        self.assertIn('{', summary)  # Should be JSON

        # Try parsing as JSON
        import json
        data = json.loads(summary)
        self.assertIn('byte_size', data)
        self.assertIn('layers', data)


class TestMultiLayer(unittest.TestCase):
    """Test multi-layer CountingGloBiMap functionality"""

    def setUp(self):
        """Create a multi-layer filter"""
        config = cg.make_multi_layer_config(
            k=8,
            layer_specs=[(8, 20), (16, 16), (32, 12)]
        )
        self.cbf = cg.CountingGloBiMap(config, collect_input=False)

    def test_multi_layer_creation(self):
        """Test that multi-layer filter is created"""
        self.assertEqual(self.cbf.hashcount, 8)

        # Check memory size
        # Layer 0: 2^20 * 1 byte = 1MB
        # Layer 1: 2^16 * 2 bytes = 128KB
        # Layer 2: 2^12 * 4 bytes = 16KB
        expected_size = 2**20 + 2**16 * 2 + 2**12 * 4
        self.assertEqual(self.cbf.byte_size(), expected_size)

    def test_high_count_estimation(self):
        """Test cardinality estimation with high counts"""
        point = [100, 200]

        # Insert many times
        for _ in range(500):
            self.cbf.put(point)

        count = self.cbf.get_min(point)

        # Should be around 500 (may have error)
        self.assertGreater(count, 400)  # Allow some error
        self.assertLess(count, 600)


class TestErrorDetection(unittest.TestCase):
    """Test error detection functionality"""

    def setUp(self):
        """Create a filter with input collection enabled"""
        config = cg.make_single_layer_config(k=8, bits=8, logsize=18)
        self.cbf = cg.CountingGloBiMap(config, collect_input=True)

    def test_collect_input_enabled(self):
        """Test that input collection is enabled"""
        self.assertTrue(self.cbf.collect_input)

    def test_error_detection(self):
        """Test error detection functionality"""
        # Insert some points
        for i in range(100):
            self.cbf.put([i, i])

        # Detect errors in region
        self.cbf.detect_errors(0, 0, 200, 200)

        # Error rate should be a float
        self.assertIsInstance(self.cbf.error_rate, float)
        self.assertGreaterEqual(self.cbf.error_rate, 0.0)
        self.assertLessEqual(self.cbf.error_rate, 1.0)

    def test_error_summary(self):
        """Test error summary JSON output"""
        # Insert points
        for i in range(50):
            self.cbf.put([i, i])

        # Detect errors
        self.cbf.detect_errors(0, 0, 100, 100)

        summary = self.cbf.error_summary()
        self.assertIsInstance(summary, str)

        # Parse JSON
        import json
        data = json.loads(summary)
        self.assertIn('unique_input', data)
        self.assertIn('errors', data)
        self.assertIn('error_rate', data)

    def test_error_magnitudes(self):
        """Test error magnitude retrieval"""
        # Insert points
        for i in range(20):
            self.cbf.put([i, i])

        # Detect errors
        self.cbf.detect_errors(0, 0, 50, 50)

        magnitudes = self.cbf.error_magnitudes()
        self.assertIsInstance(magnitudes, list)


class TestBulkOperations(unittest.TestCase):
    """Test bulk operations"""

    def setUp(self):
        """Create a filter for testing"""
        config = cg.make_single_layer_config(k=8, bits=8, logsize=20)
        self.cbf = cg.CountingGloBiMap(config, collect_input=False)

    def test_put_all(self):
        """Test bulk insert operation"""
        # Flat list of coordinates: [x1, y1, x2, y2, ...]
        points = [100, 200, 150, 250, 200, 300]

        self.cbf.put_all(points)

        # Check that points were inserted
        self.assertTrue(self.cbf.get_bool([100, 200]))
        self.assertTrue(self.cbf.get_bool([150, 250]))
        self.assertTrue(self.cbf.get_bool([200, 300]))

    def test_to_hashfn(self):
        """Test converting points to hash values"""
        points = [100, 200, 150, 250]

        hashfn = self.cbf.to_hashfn(points)

        # Should return hash values
        self.assertIsInstance(hashfn, list)
        self.assertEqual(len(hashfn), len(points))


def run_tests():
    """Run all tests"""
    unittest.main()


if __name__ == '__main__':
    run_tests()
