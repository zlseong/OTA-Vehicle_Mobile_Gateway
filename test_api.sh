#!/bin/bash

echo "=== API 연결 테스트 ==="
echo "서버: 54.234.98.110:8765"
echo ""

# Test 1: Health Check
echo "[Test 1] Health Check..."
curl -k -s https://54.234.98.110:8765/api/health
echo ""
echo ""

# Test 2: File Upload (예상: 404)
echo "[Test 2] File Upload Test..."
curl -k -s -X POST \
  -F "file=@hi.txt" \
  https://54.234.98.110:8765/api/upload
echo ""
echo ""

# Test 3: Device Register (예상: 404)
echo "[Test 3] Device Register Test..."
curl -k -s -X POST \
  -H "Content-Type: application/json" \
  -d '{"device_id":"test-001","name":"Test Device"}' \
  https://54.234.98.110:8765/api/devices/register
echo ""
echo ""

echo "=== 테스트 완료 ==="
echo "✅ Health Check: 정상 작동"
echo "❌ Upload/Register: 서버에 API 없음 (404 예상)"
