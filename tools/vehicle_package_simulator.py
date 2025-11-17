#!/usr/bin/env python3
"""
Vehicle Package Simulator for VMG OTA Testing

서버에서 VMG로 전송되는 전체 Vehicle Package를 생성합니다.

ECU Configuration:
- ECU_091: ZGW (Zonal Gateway)
- ECU_011: Zone 1, ECU #1 (BCM - Body Control Module)
- ECU_012: Zone 1, ECU #2 (DCM - Door Control Module)
- ECU_021: Zone 2, ECU #1 (Camera Module)
- ECU_022: Zone 2, ECU #2 (Radar Module)
"""

import struct
import zlib
import time
import os
import argparse
from pathlib import Path

# ==================== Magic Numbers ====================

VEHICLE_PACKAGE_MAGIC = 0x5650504B  # "VPPK"
ZONE_PACKAGE_MAGIC = 0x5A4F4E45     # "ZONE"
ECU_METADATA_MAGIC = 0x4543554D     # "ECUM"

MAX_ZONES_IN_VEHICLE = 16
MAX_ECUS_IN_ZONE = 16

# ==================== ECU Configuration ====================

ECU_CONFIG = {
    # Zone 1 ECUs (Front)
    'zone_1': {
        'zone_id': 'Zone_Front',
        'zone_name': 'Zone_Front_Left',
        'zone_number': 1,
        'ecus': [
            {'ecu_id': 'ECU_011', 'name': 'BCM', 'version': 'v2.0.1', 'hw_ver': 'v1.0.0', 'priority': 0, 'size_kb': 256},
            {'ecu_id': 'ECU_012', 'name': 'DCM', 'version': 'v1.5.0', 'hw_ver': 'v1.0.0', 'priority': 1, 'size_kb': 128},
        ]
    },
    # Zone 2 ECUs (Rear)
    'zone_2': {
        'zone_id': 'Zone_Rear',
        'zone_name': 'Zone_Rear_Left',
        'zone_number': 2,
        'ecus': [
            {'ecu_id': 'ECU_021', 'name': 'Camera', 'version': 'v1.0.0', 'hw_ver': 'v1.0.0', 'priority': 0, 'size_kb': 512},
            {'ecu_id': 'ECU_022', 'name': 'Radar', 'version': 'v1.0.0', 'hw_ver': 'v1.0.0', 'priority': 1, 'size_kb': 384},
        ]
    },
    # Zone 9 ECU (Gateway)
    'zone_9': {
        'zone_id': 'Zone_Gateway',
        'zone_name': 'Zone_Central_Gateway',
        'zone_number': 9,
        'ecus': [
            {'ecu_id': 'ECU_091', 'name': 'ZGW', 'version': 'v2.0.0', 'hw_ver': 'v1.0.0', 'priority': 10, 'size_kb': 1024},
        ]
    }
}

# ==================== Helper Functions ====================

def version_to_int(version_str):
    """버전 문자열을 정수로 변환 (v1.2.3 → 0x00010203)"""
    parts = version_str.strip('v').split('.')
    major = int(parts[0]) if len(parts) > 0 else 0
    minor = int(parts[1]) if len(parts) > 1 else 0
    patch = int(parts[2]) if len(parts) > 2 else 0
    return (major << 16) | (minor << 8) | patch

def crc32_calculate(data):
    """CRC32 계산"""
    return zlib.crc32(data) & 0xFFFFFFFF

def generate_dummy_firmware(ecu_id, size_kb):
    """더미 펌웨어 생성 (테스트용)"""
    firmware = bytearray()
    
    # 헤더: ECU ID + Size
    header = f"FIRMWARE_{ecu_id}".ljust(64, '\0').encode('ascii')
    firmware.extend(header)
    
    # 패턴 데이터
    pattern = bytes([i % 256 for i in range(1024)])
    remaining = (size_kb * 1024) - len(firmware)
    
    while remaining > 0:
        chunk_size = min(len(pattern), remaining)
        firmware.extend(pattern[:chunk_size])
        remaining -= chunk_size
    
    return bytes(firmware)

# ==================== ECU Package ====================

def create_ecu_package(ecu_config):
    """
    ECU Package 생성
    
    Structure:
        - ECU Metadata (256 bytes)
        - Firmware Binary (variable)
    """
    ecu_id = ecu_config['ecu_id']
    version = version_to_int(ecu_config['version'])
    hw_version = version_to_int(ecu_config['hw_ver'])
    
    print(f"  [ECU] Creating {ecu_id} package...")
    
    # Generate firmware
    firmware = generate_dummy_firmware(ecu_id, ecu_config['size_kb'])
    firmware_size = len(firmware)
    firmware_crc32 = crc32_calculate(firmware)
    
    # Build ECU Metadata (256 bytes)
    metadata = bytearray(256)
    
    # Magic number
    struct.pack_into('<I', metadata, 0, ECU_METADATA_MAGIC)
    
    # ECU ID
    ecu_id_bytes = ecu_id.encode('ascii').ljust(16, b'\x00')
    metadata[4:20] = ecu_id_bytes
    
    # Versions
    struct.pack_into('<I', metadata, 20, version)
    struct.pack_into('<I', metadata, 24, hw_version)
    struct.pack_into('<I', metadata, 28, firmware_size)
    struct.pack_into('<I', metadata, 32, firmware_crc32)
    struct.pack_into('<I', metadata, 36, int(time.time()))
    
    # Version string
    version_str = ecu_config['version'].ljust(32, '\0').encode('ascii')
    metadata[40:72] = version_str
    
    # Dependency count (0 for now)
    metadata[72] = 0
    
    # Dependencies (8 × 32 bytes)
    # (reserved for now)
    
    # Reserved2 (144 bytes)
    # (already zeroed)
    
    # Combine metadata + firmware
    ecu_package = bytes(metadata) + firmware
    
    print(f"    Version: {ecu_config['version']}")
    print(f"    Size: {len(ecu_package)} bytes ({len(ecu_package)/1024:.1f} KB)")
    print(f"    CRC32: 0x{firmware_crc32:08X}")
    
    return ecu_package, firmware_size, firmware_crc32, version

# ==================== Zone Package ====================

def create_zone_package(zone_config):
    """
    Zone Package 생성
    
    Structure:
        - Zone Package Header (1KB)
        - ECU Package #1
        - ECU Package #2
        - ...
    """
    zone_id = zone_config['zone_id']
    zone_name = zone_config['zone_name']
    zone_number = zone_config['zone_number']
    
    print(f"\n[Zone] Creating Zone {zone_number} ({zone_name})...")
    
    # Create ECU packages
    ecu_packages = []
    current_offset = 1024  # Zone header size
    
    for ecu_cfg in zone_config['ecus']:
        ecu_pkg, fw_size, fw_crc, version = create_ecu_package(ecu_cfg)
        ecu_pkg_crc = crc32_calculate(ecu_pkg)
        
        ecu_packages.append({
            'ecu_id': ecu_cfg['ecu_id'],
            'data': ecu_pkg,
            'offset': current_offset,
            'size': len(ecu_pkg),
            'metadata_size': 256,
            'firmware_size': fw_size,
            'firmware_version': version,
            'crc32': ecu_pkg_crc,
            'priority': ecu_cfg['priority']
        })
        
        current_offset += len(ecu_pkg)
    
    # Build Zone Package Header (1KB)
    header = bytearray(1024)
    
    # Magic number
    struct.pack_into('<I', header, 0, ZONE_PACKAGE_MAGIC)
    
    # Version
    struct.pack_into('<I', header, 4, 0x00010000)  # v1.0.0
    
    # Total size (will update later)
    total_size = current_offset
    struct.pack_into('<I', header, 8, total_size)
    
    # Zone ID
    zone_id_bytes = zone_id.encode('ascii').ljust(16, b'\x00')
    header[12:28] = zone_id_bytes
    
    # Zone number
    header[28] = zone_number
    
    # Package count
    header[29] = len(ecu_packages)
    
    # CRC32 (will calculate later)
    # struct.pack_into('<I', header, 32, zone_crc32)
    
    # Timestamp
    struct.pack_into('<I', header, 36, int(time.time()))
    
    # Zone name
    zone_name_bytes = zone_name.encode('ascii').ljust(32, b'\x00')
    header[40:72] = zone_name_bytes
    
    # ECU Table (starts at offset 256)
    ecu_table_offset = 256
    for i, ecu in enumerate(ecu_packages):
        entry_offset = ecu_table_offset + (i * 64)
        
        # ECU ID
        ecu_id_bytes = ecu['ecu_id'].encode('ascii').ljust(16, b'\x00')
        header[entry_offset:entry_offset+16] = ecu_id_bytes
        
        # Offset, Size, Metadata Size, Firmware Size
        struct.pack_into('<I', header, entry_offset+16, ecu['offset'])
        struct.pack_into('<I', header, entry_offset+20, ecu['size'])
        struct.pack_into('<I', header, entry_offset+24, ecu['metadata_size'])
        struct.pack_into('<I', header, entry_offset+28, ecu['firmware_size'])
        struct.pack_into('<I', header, entry_offset+32, ecu['firmware_version'])
        struct.pack_into('<I', header, entry_offset+36, ecu['crc32'])
        header[entry_offset+40] = ecu['priority']
    
    # Assemble Zone Package
    zone_package = bytearray(header)
    for ecu in ecu_packages:
        zone_package.extend(ecu['data'])
    
    # Calculate Zone CRC32 (excluding header)
    zone_crc32 = crc32_calculate(zone_package[1024:])
    struct.pack_into('<I', zone_package, 32, zone_crc32)
    
    print(f"  Total Size: {len(zone_package)} bytes ({len(zone_package)/1024:.1f} KB)")
    print(f"  ECU Count: {len(ecu_packages)}")
    print(f"  Zone CRC32: 0x{zone_crc32:08X}")
    
    return bytes(zone_package), zone_id, zone_number, len(ecu_packages)

# ==================== Vehicle Package ====================

def create_vehicle_package(output_path, vin, model, model_year):
    """
    Vehicle Package 생성 (최상위)
    
    Structure:
        - Vehicle Package Metadata (12KB)
        - Zone Package #1
        - Zone Package #2
        - ...
    """
    print("\n" + "="*60)
    print("  Vehicle Package Creator")
    print("="*60)
    print(f"VIN: {vin}")
    print(f"Model: {model} ({model_year})")
    print("="*60)
    
    # Create Zone Packages
    zone_packages = []
    current_offset = 12288  # Vehicle metadata size (12KB)
    
    for zone_key, zone_cfg in ECU_CONFIG.items():
        zone_pkg, zone_id, zone_num, ecu_count = create_zone_package(zone_cfg)
        
        zone_packages.append({
            'zone_id': zone_id,
            'zone_number': zone_num,
            'ecu_count': ecu_count,
            'data': zone_pkg,
            'offset': current_offset,
            'size': len(zone_pkg)
        })
        
        current_offset += len(zone_pkg)
    
    # Build Vehicle Package Metadata (12KB)
    metadata = bytearray(12288)
    
    # Magic number
    struct.pack_into('<I', metadata, 0, VEHICLE_PACKAGE_MAGIC)
    
    # Version
    struct.pack_into('<I', metadata, 4, 0x00010000)  # v1.0.0
    
    # Total size
    total_size = current_offset
    struct.pack_into('<I', metadata, 8, total_size)
    
    # VIN
    vin_bytes = vin.encode('ascii').ljust(17, b'\x00')
    metadata[12:29] = vin_bytes
    
    # Model
    model_bytes = model.encode('ascii').ljust(32, b'\x00')
    metadata[29:61] = model_bytes
    
    # Model year
    struct.pack_into('<H', metadata, 61, model_year)
    
    # Region
    metadata[63] = 3  # Korea
    
    # Master SW Version
    master_version = version_to_int('v2.0.0')
    struct.pack_into('<I', metadata, 76, master_version)
    
    # Master SW String
    master_sw_str = "v2.0.0".ljust(32, '\0').encode('ascii')
    metadata[80:112] = master_sw_str
    
    # Zone count
    metadata[128] = len(zone_packages)
    
    # Total ECU count
    total_ecu_count = sum(z['ecu_count'] for z in zone_packages)
    metadata[129] = total_ecu_count
    
    # Zone References (starts at offset 192)
    zone_ref_offset = 192
    for i, zone in enumerate(zone_packages):
        entry_offset = zone_ref_offset + (i * 32)
        
        # Zone ID
        zone_id_bytes = zone['zone_id'].encode('ascii').ljust(16, b'\x00')
        metadata[entry_offset:entry_offset+16] = zone_id_bytes
        
        # Offset, Size
        struct.pack_into('<I', metadata, entry_offset+16, zone['offset'])
        struct.pack_into('<I', metadata, entry_offset+20, zone['size'])
        
        # Zone number
        metadata[entry_offset+24] = zone['zone_number']
        
        # ECU count
        metadata[entry_offset+25] = zone['ecu_count']
    
    # ECU Quick Reference (starts at offset 704)
    # (optional, skipping for now)
    
    # Vehicle CRC32 (will calculate after assembly)
    # struct.pack_into('<I', metadata, 144, vehicle_crc32)
    
    # Assemble Vehicle Package
    vehicle_package = bytearray(metadata)
    for zone in zone_packages:
        vehicle_package.extend(zone['data'])
    
    # Calculate Vehicle CRC32 (excluding metadata)
    vehicle_crc32 = crc32_calculate(vehicle_package[12288:])
    struct.pack_into('<I', vehicle_package, 144, vehicle_crc32)
    
    # Write to file
    output_file = Path(output_path)
    output_file.parent.mkdir(parents=True, exist_ok=True)
    
    with open(output_path, 'wb') as f:
        f.write(vehicle_package)
    
    print("\n" + "="*60)
    print("  ✓ Vehicle Package Created Successfully!")
    print("="*60)
    print(f"Output: {output_path}")
    print(f"Total Size: {len(vehicle_package)} bytes ({len(vehicle_package)/1024/1024:.2f} MB)")
    print(f"Zone Packages: {len(zone_packages)}")
    print(f"Total ECUs: {total_ecu_count}")
    print(f"Vehicle CRC32: 0x{vehicle_crc32:08X}")
    print("="*60)
    
    return output_path

# ==================== Main ====================

def main():
    parser = argparse.ArgumentParser(description='Vehicle Package Simulator for VMG OTA')
    parser.add_argument('--output', '-o', default='vehicle_package_v2.0.0.bin',
                        help='Output file path (default: vehicle_package_v2.0.0.bin)')
    parser.add_argument('--vin', default='KMHXX00XXXX000001',
                        help='Vehicle VIN (default: KMHXX00XXXX000001)')
    parser.add_argument('--model', default='Genesis GV80',
                        help='Vehicle model (default: Genesis GV80)')
    parser.add_argument('--year', type=int, default=2024,
                        help='Model year (default: 2024)')
    
    args = parser.parse_args()
    
    create_vehicle_package(args.output, args.vin, args.model, args.year)

if __name__ == '__main__':
    main()

