# OpenMV Unit Tests
# Tests: person detection, distance estimation, UART protocol

## Running Tests

### On PC (requires pytest)
```bash
cd Test/OpenMV
pip install pytest
pytest test_person_detector.py -v
```

### On OpenMV Camera
```python
# Open main.py in OpenMV IDE
# Tests are embedded as functions, run manually
test_estimate_distance()
test_person_info()
```

## Test Files

| File | Target | Description |
|------|--------|-------------|
| `test_person_detector.py` | Distance estimation | Multi-feature fusion, area/feetY weighting |
| `test_protocol.py` | UART protocol | VIS string formatting, checksum calculation |
| `test_settings.py` | Configuration | Parameter validation, boundary checks |

## Manual Tests (OpenMV IDE)

Run these in the OpenMV IDE to verify hardware behavior:

```python
# Test distance estimation
from person_detector import PersonDetector
detector = PersonDetector(threshold=0.4)

# Simulate a detection at frame center
cx, cy, w, h = 96, 150, 80, 120
feet_y = 160
category, score, features = detector.estimate_distance(cx, cy, w, h, feet_y, 192, 192)
print("Category:", category, "Score:", score)

# Verify VIS protocol checksum
data_str = "96,150,80,120,160,0.85,PERSON,0.75"
checksum = 0
for ch in data_str:
    checksum ^= ord(ch)
print("VIS:%s*%d" % (data_str, checksum))
```

## Adding New Tests

1. Create `test_*.py` in `Test/OpenMV/`
2. Use `assert` for assertions
3. Follow pytest conventions for PC test discovery