#!/usr/bin/env python3
"""
VMG Configuration Test

Config Manager 테스트
"""

import json
import os
import sys

def test_config_json_validity():
    """config.json 유효성 테스트"""
    print("\n" + "="*60)
    print("Test 1: config.json Validity")
    print("="*60)
    
    # Try multiple possible paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    possible_paths = [
        os.path.join(script_dir, '..', 'config.json'),  # From test/ directory
        'config.json',  # Current directory
        '../config.json'  # One level up
    ]
    
    config_path = None
    for path in possible_paths:
        if os.path.exists(path):
            config_path = path
            break
    
    if config_path is None:
        raise FileNotFoundError("config.json not found in any expected location")
    
    try:
        with open(config_path, 'r', encoding='utf-8') as f:
            config = json.load(f)
        
        print("✓ config.json is valid JSON")
        
        # Check required fields
        required_fields = [
            ('server', dict),
            ('vehicle', dict),
            ('device', dict),
            ('zgw', dict),
            ('ota', dict),
            ('readiness', dict),
            ('tls', dict),
            ('pqc', dict),
            ('logging', dict),
            ('monitoring', dict)
        ]
        
        for field, field_type in required_fields:
            assert field in config, f"Missing field: {field}"
            assert isinstance(config[field], field_type), f"Invalid type for {field}"
            print(f"✓ {field}: OK")
        
        print("\n✓ Test 1 PASSED")
        return config
        
    except FileNotFoundError:
        print(f"✗ config.json not found: {config_path}")
        raise
    except json.JSONDecodeError as e:
        print(f"✗ Invalid JSON: {e}")
        raise
    except AssertionError as e:
        print(f"✗ {e}")
        raise

def test_3_sector_layout(config):
    """3-Sector Layout 설정 테스트"""
    print("\n" + "="*60)
    print("Test 2: 3-Sector Layout Configuration")
    print("="*60)
    
    try:
        ota = config['ota']
        dual_partition = ota['dual_partition']
        
        # Check 3-sector fields
        assert 'partition_a' in dual_partition, "Missing partition_a"
        assert 'partition_b' in dual_partition, "Missing partition_b"
        assert 'data_partition' in dual_partition, "Missing data_partition"
        assert 'data_mount_point' in dual_partition, "Missing data_mount_point"
        
        print(f"Partition A: {dual_partition['partition_a']}")
        print(f"Partition B: {dual_partition['partition_b']}")
        print(f"Data Partition: {dual_partition['data_partition']}")
        print(f"Data Mount: {dual_partition['data_mount_point']}")
        
        # Check paths use data mount point
        assert ota['download_path'].startswith('/mnt/data'), "download_path should use /mnt/data"
        assert ota['install_path'].startswith('/mnt/data'), "install_path should use /mnt/data"
        assert config['logging']['file'].startswith('/mnt/data'), "log file should use /mnt/data"
        
        print("\n✓ 3-Sector layout properly configured")
        print("✓ All paths use data partition (/mnt/data)")
        print("\n✓ Test 2 PASSED")
        
    except KeyError as e:
        print(f"✗ Missing configuration key: {e}")
        raise
    except AssertionError as e:
        print(f"✗ {e}")
        raise

def test_vehicle_info(config):
    """Vehicle 정보 테스트"""
    print("\n" + "="*60)
    print("Test 3: Vehicle Information")
    print("="*60)
    
    try:
        vehicle = config['vehicle']
        
        required = ['vin', 'model', 'model_year', 'master_sw_version']
        for field in required:
            assert field in vehicle, f"Missing vehicle field: {field}"
            print(f"✓ {field}: {vehicle[field]}")
        
        # VIN validation
        vin = vehicle['vin']
        assert len(vin) == 17, f"VIN must be 17 characters, got {len(vin)}"
        
        # Model year validation
        model_year = vehicle['model_year']
        assert 2020 <= model_year <= 2030, f"Invalid model year: {model_year}"
        
        print("\n✓ Test 3 PASSED")
        
    except KeyError as e:
        print(f"✗ Missing key: {e}")
        raise
    except AssertionError as e:
        print(f"✗ {e}")
        raise

def test_mqtt_topics(config):
    """MQTT Topic 설정 테스트"""
    print("\n" + "="*60)
    print("Test 4: MQTT Topics Configuration")
    print("="*60)
    
    try:
        topics = config['server']['mqtt']['topics']
        
        required_topics = ['command', 'status', 'ota', 'vci', 'readiness']
        for topic in required_topics:
            assert topic in topics, f"Missing topic: {topic}"
            print(f"✓ {topic}: {topics[topic]}")
            
            # Check if topic contains device_id placeholder
            if '{device_id}' in topics[topic] or '{vin}' in topics[topic]:
                print(f"  → Contains placeholder (will be replaced at runtime)")
        
        print("\n✓ Test 4 PASSED")
        
    except KeyError as e:
        print(f"✗ Missing key: {e}")
        raise

def main():
    print("\n" + "="*60)
    print("VMG Configuration Test Suite")
    print("="*60)
    
    tests_passed = 0
    tests_failed = 0
    config = None
    
    try:
        # Test 1: JSON validity
        config = test_config_json_validity()
        tests_passed += 1
        
        # Test 2: 3-sector layout
        test_3_sector_layout(config)
        tests_passed += 1
        
        # Test 3: Vehicle info
        test_vehicle_info(config)
        tests_passed += 1
        
        # Test 4: MQTT topics
        test_mqtt_topics(config)
        tests_passed += 1
        
    except Exception as e:
        tests_failed += 1
        print(f"\n✗ Test suite failed: {e}")
    
    # Summary
    print("\n" + "="*60)
    print("Test Summary")
    print("="*60)
    print(f"Tests Passed: {tests_passed}")
    print(f"Tests Failed: {tests_failed}")
    print("="*60)
    
    if tests_failed > 0:
        sys.exit(1)
    else:
        print("\n✓ All tests passed!")
        sys.exit(0)

if __name__ == '__main__':
    main()

