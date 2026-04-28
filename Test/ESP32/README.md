# Test Suite for ESP32 Firmware
# Can be compiled and run on PC (Linux/Mac/Windows with GCC/Clang)

## Build & Run

### Linux / Mac
```bash
cd Test/ESP32
g++ -o VisionBridge_test VisionBridge_test.cpp -I../../ESP32_Solo
./VisionBridge_test
```

### Windows (MSYS2 / MinGW)
```bash
cd Test/ESP32
g++ -o VisionBridge_test.exe VisionBridge_test.cpp -I../../ESP32_Solo
VisionBridge_test.exe
```

## Test Files

| File | Target | Description |
|------|--------|-------------|
| `VisionBridge_test.cpp` | Protocol | VIS protocol parsing, checksum verification |
| `FollowLogic_test.cpp` | Logic | Following decisions, distScore/feetY fallback |
| `MotorDriver_test.cpp` | Motor | PWM output, dead zone, direction control |

## Notes

- Tests use mocked Arduino functions (no hardware required)
- `VisionBridge_test.cpp` is self-contained (inline parser implementation)
- `FollowLogic_test.cpp` and `MotorDriver_test.cpp` need include path to ESP32_Solo source

## Adding New Tests

1. Create `*_test.cpp` in `Test/ESP32/`
2. Use `TEST_ASSERT(condition, "description")` macro for assertions
3. Update this README with new test file descriptions