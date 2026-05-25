# ============================================================================
# VL53L1X ToF 激光测距模块 — 功能验证测试
# 平台: OpenMV H7 Plus + 星瞳科技测距扩展板 (VL53L1X)
# I2C: Shield 接口 (I2C2) 或 P7(SCL)/P8(SDA) (I2C1)
# ============================================================================

import time
import pyb
from machine import I2C

# ============================================================================
# Step 1: I2C 总线扫描 — 探测 VL53L1X
# ============================================================================

VL53L1X_ADDR = 0x29
EXPECTED_MODEL_ID = 0xEACC

print("=" * 50)
print("VL53L1X ToF Distance Sensor Test")
print("=" * 50)

# 探测可用 I2C 总线 (先 I2C2=Shield, 再 I2C1=P7/P8)
found_bus = None
for bus_id in [2, 1]:
    try:
        bus = I2C(bus_id)
        devices = bus.scan()
        print("I2C(%d) scan: %s" % (bus_id, [hex(d) for d in devices]))
        if VL53L1X_ADDR in devices:
            found_bus = bus_id
            break
    except Exception as e:
        print("I2C(%d) init failed: %s" % (bus_id, e))

if found_bus is None:
    print("\n[FAIL] VL53L1X not found on any I2C bus!")
    print("Check:")
    print("  1. Distance shield is plugged into the Shield connector")
    print("  2. Pins are clean and fully seated")
    print("  3. Try I2C(1) with P7=SCL, P8=SDA via jumper wires")
    raise SystemExit

print("\n[OK] VL53L1X found on I2C(%d) at addr 0x%02X" % (found_bus, VL53L1X_ADDR))

# ============================================================================
# Step 2: 传感器初始化
# ============================================================================

import vl53l1x

tof = None
try:
    tof = vl53l1x.VL53L1X(I2C(found_bus))
    print("[OK] VL53L1X initialized")
except Exception as e:
    print("[FAIL] VL53L1X init failed: %s" % e)

# ============================================================================
# Step 3: 芯片 ID 验证
# ============================================================================

try:
    chip_id = tof.read_model_id()
    if chip_id == EXPECTED_MODEL_ID:
        print("[OK] Model ID verified: 0x%04X (expected 0x%04X)" % (chip_id, EXPECTED_MODEL_ID))
    else:
        print("[WARN] Unexpected model ID: 0x%04X (expected 0x%04X)" % (chip_id, EXPECTED_MODEL_ID))
except Exception as e:
    print("[FAIL] Model ID read failed: %s" % e)

# ============================================================================
# Step 4: 连续测距验证
# ============================================================================

print("\n" + "=" * 50)
print("Continuous Ranging Test (Ctrl+C to stop)")
print("=" * 50)

SAMPLES = 100
readings = []
errors = 0
led = pyb.LED(1)  # 红色 LED

start_ms = pyb.millis()

for i in range(SAMPLES):
    try:
        d = tof.read()
        readings.append(d)
        status = "OK"
    except Exception as e:
        errors += 1
        d = -1
        status = "ERR: %s" % str(e)[:30]

    elapsed = pyb.millis() - start_ms

    # 状态指示
    if d < 0:
        led.off()
    elif d < 500:
        led.on()         # 很近 → 红灯常亮
    elif d < 1500:
        led.toggle()     # 中等 → 红灯闪烁
    else:
        led.off()        # 远 → 灭

    # 每秒打印一行
    if i % 10 == 0 or d < 0:
        print("[%5dms] #%3d | %4d mm | %s" % (elapsed, i + 1, d, status))

    time.sleep_ms(30)  # ~30Hz, VL53L1X 最大 50Hz

total_ms = pyb.millis() - start_ms
led.off()

# ============================================================================
# Step 5: 统计分析
# ============================================================================

valid = [r for r in readings if 40 <= r <= 4000]
out_of_range_low = sum(1 for r in readings if 0 <= r < 40)
out_of_range_high = sum(1 for r in readings if r > 4000)

print("\n" + "=" * 50)
print("Results (%d samples in %dms, %.1f Hz)" %
      (SAMPLES, total_ms, SAMPLES * 1000.0 / total_ms))
print("=" * 50)

if valid:
    print("  Valid range (40-4000mm): %d (%.0f%%)" %
          (len(valid), len(valid) / SAMPLES * 100))
    print("  Min:    %4d mm" % min(valid))
    print("  Max:    %4d mm" % max(valid))
    print("  Avg:    %4d mm" % (sum(valid) // len(valid)))

if out_of_range_low:
    print("  Too close (<40mm):    %d (%.0f%%)" %
          (out_of_range_low, out_of_range_low / SAMPLES * 100))
if out_of_range_high:
    print("  Too far (>4000mm):    %d (%.0f%%)" %
          (out_of_range_high, out_of_range_high / SAMPLES * 100))
if errors:
    print("  Read errors:          %d (%.0f%%)" %
          (errors, errors / SAMPLES * 100))

# 稳定性: 连续值之间的最大跳变
if len(valid) >= 2:
    jumps = [abs(valid[j] - valid[j-1]) for j in range(1, len(valid))]
    max_jump = max(jumps) if jumps else 0
    avg_jump = sum(jumps) // len(jumps) if jumps else 0
    print("  Max jump:  %4d mm" % max_jump)
    print("  Avg jump:  %4d mm" % avg_jump)

print("\n[%s] Test complete." %
      ("PASS" if valid and errors == 0 else "CHECK"))

# ============================================================================
# Step 6: 手动验证提示
# ============================================================================

print("""
Manual verification:
  1. Place a white card at 300mm → should read ~270-330mm
  2. Place at 1000mm → should read ~950-1050mm
  3. Cover sensor → should report <40mm (too close)
  4. Point at open sky/sky → should report >4000mm (out of range)
""")
