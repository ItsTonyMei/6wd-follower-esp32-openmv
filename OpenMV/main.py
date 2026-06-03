# ============================================================================
# 履带车视觉跟随系统 — OpenMV L1 感知层
# N6: YOLOv8n NPU 45FPS + VL53L1X ToF (I2C2, P4/P5)
# VIS 输出: P2 UART4 TX @ 4800 baud → ESP32 GPIO4 (SoftwareSerial)
# P4/P5 = VL53L1X ToF (I2C2), P0 = SPI2 only (无 UART)
# LED: 红=错误, 绿=有人+ToF有效, 蓝=扫描中
# ============================================================================

import gc, time, math, os
import ml
from pyb import LED as _LED

# ============================================================================
# Board detection — N6 用 csi, H7 Plus 用 sensor
# ============================================================================
IS_N6 = "yolov8n_192.tflite" in os.listdir("/rom")

if IS_N6:
    import csi as _cam_mod
    from ml.postprocessing.ultralytics import YoloV8
    _POSTPROC = YoloV8
    _MODEL_PATH = "/rom/yolov8n_192.tflite"
    _CAM_W, _CAM_H = 320, 240       # QVGA 完整分辨率（画面清晰）
else:
    import sensor as _cam_mod
    from ml.postprocessing.darknet import YoloLC
    _POSTPROC = YoloLC
    _MODEL_PATH = "/rom/yolo_lc_192.tflite"
    _CAM_W, _CAM_H = 320, 240

# VIS 协议坐标空间 (ESP8266 FollowLogic 期望 192x192)
_VIS_W, _VIS_H = 192, 192
_VIS_CX, _VIS_CY = 96, 96

# ============================================================================
# Configuration
# ============================================================================

CAMERA_FRAMESIZE    = _cam_mod.QVGA      # 320x240
CAMERA_PIXFORMAT    = _cam_mod.RGB565
CAMERA_CONTRAST     = 3
CAMERA_GAINCEILING  = 16
CAMERA_HMIRROR      = False
CAMERA_VFLIP        = False
CAMERA_STABILIZE_MS = 2000

# N6: NPU 实时推理无需跳帧, H7: 跳 2 帧 ~10 FPS
SENSOR_SKIP_FRAMES  = 0 if IS_N6 else 2

DETECTION_THRESHOLD  = 0.4

# ---- 距离估计: ToF 主信号 + 视觉备用 + EMA 平滑 ----
# VL53L1X ToF 有效范围
TOF_MIN_VALID = 40     # mm, < this → 无效 (遮挡/太近)
TOF_MAX_VALID = 4000   # mm, > this → 无效 (超量程, 退回视觉)

# ToF mm → distScore 分段线性映射
# distScore 含义 (与 ESP32 FollowLogic 对齐):
#   >= 0.85 → STOP,  0.65-0.85 → SLOW,  0.30-0.65 → MID,  < 0.30 → FAR
TOF_STOP_MM   = 500    # < this → distScore = 1.0 (STOP)
TOF_NEAR_MM   = 1000   # STOP → SLOW 过渡终点
TOF_MID_MM    = 2000   # SLOW → FAR 过渡终点 (~1.5m 映射到 ~0.55)
TOF_FAR_MM    = 4000   # FAR → 全速追赶

# EMA 平滑: alpha 越大响应越快，越小越平滑 (0.0-1.0)
TOF_SMOOTH_ALPHA = 0.3

# 视觉备用保留原有阈值 (ToF 无效时启用)
AREA_VERY_CLOSE = 0.50
AREA_CLOSE      = 0.30
AREA_FAR        = 0.10
WEIGHT_CLOSE_AREA  = 0.7;  WEIGHT_CLOSE_FEETY = 0.3
WEIGHT_MEDIUM_FEETY = 0.6; WEIGHT_MEDIUM_AREA  = 0.4
WEIGHT_FAR_FEETY = 0.8;    WEIGHT_FAR_AREA  = 0.2
TOP_Y_THRESHOLD = 10
FEETY_CLOSE = 155; FEETY_FAR = 80

TRACK_DEADBAND         = 0.08
NO_PERSON_STOP_FRAMES  = 5
DRAW_DEBUG     = True
PRINT_EVERY_MS = 500
GC_EVERY_FRAMES = 50 if IS_N6 else 10  # N6 RAM 更充裕

# ---- Validate ----
assert 0.0 <= DETECTION_THRESHOLD <= 1.0
assert AREA_FAR < AREA_CLOSE < AREA_VERY_CLOSE
assert NO_PERSON_STOP_FRAMES > 0
assert CAMERA_STABILIZE_MS > 0

# ============================================================================
# Camera setup
# ============================================================================

_cam_obj = None

def setup_camera():
    global _cam_obj
    if IS_N6:
        _cam_obj = _cam_mod.CSI()
        _cam_obj.reset()
        _cam_obj.pixformat(CAMERA_PIXFORMAT)
        _cam_obj.framesize(CAMERA_FRAMESIZE)
        _cam_obj.hmirror(CAMERA_HMIRROR)
        _cam_obj.vflip(CAMERA_VFLIP)
        for m in ("contrast", "set_contrast"):
            if hasattr(_cam_obj, m):
                getattr(_cam_obj, m)(CAMERA_CONTRAST)
                break
        for m in ("gainceiling", "set_gainceiling"):
            if hasattr(_cam_obj, m):
                getattr(_cam_obj, m)(CAMERA_GAINCEILING)
                break
        # N6: 不裁剪，用完整 QVGA 分辨率（模型自动居中裁剪到 192x192）
        _cam_obj.snapshot(time=CAMERA_STABILIZE_MS)
    else:
        _cam_mod.reset()
        _cam_mod.set_contrast(CAMERA_CONTRAST)
        _cam_mod.set_gainceiling(CAMERA_GAINCEILING)
        _cam_mod.set_framesize(CAMERA_FRAMESIZE)
        _cam_mod.set_pixformat(CAMERA_PIXFORMAT)
        _cam_mod.set_hmirror(CAMERA_HMIRROR)
        _cam_mod.set_vflip(CAMERA_VFLIP)
        _cam_mod.set_windowing((_VIS_W, _VIS_H))
        _cam_mod.skip_frames(time=CAMERA_STABILIZE_MS)

    print("Camera OK: %dx%d → VIS:%dx%d (platform=%s)" %
          (_CAM_W, _CAM_H, _VIS_W, _VIS_H, "N6" if IS_N6 else "H7"))

def capture():
    return _cam_obj.snapshot() if IS_N6 else _cam_mod.snapshot()

setup_camera()

# ============================================================================
# YOLO Model
# ============================================================================

model = ml.Model(_MODEL_PATH, postprocess=_POSTPROC(threshold=DETECTION_THRESHOLD))
print("Model: %s @ %s NPU=%s" %
      (_MODEL_PATH.split("/")[-1], _POSTPROC.__name__, str(IS_N6)))

# Determine person class index
person_idx = None
for i, label in enumerate(model.labels):
    if "person" in label.lower() or "people" in label.lower():
        person_idx = i
        print("Person class: idx=%d label=%s" % (i, label))
        break
if person_idx is None:
    print("Warning: no person class found, using idx=0")
    person_idx = 0

# ============================================================================
# VIS 输出: P0 硬件 UART(3) @ 4800 baud → ESP8266 D5 (GPIO14)
# VL53L1X → I2C(2) (P4=SCL, P5=SDA) — 独占, 无冲突
# ============================================================================

VIS_BAUD      = 4800
VIS_INTERVAL_MS = 200   # 每 200ms 发送一次 VIS 帧

# P2 = UART4 TX (STM32N657 官方引脚映射, P4/P5 被 ToF I2C2 占用)
from machine import UART, Pin as _Pin
_p2 = _Pin("P2", mode=_Pin.ALT, alt=_Pin.AF8_UART4)
vis_uart = UART(4, VIS_BAUD)

TOF_ENABLED = False
try:
    from machine import I2C
    import vl53l1x
    tof = vl53l1x.VL53L1X(I2C(2))
    _chip = tof.read_model_id()
    TOF_ENABLED = (_chip == 0xEACC)
    print("VL53L1X: OK (I2C2, id=0x%04X)" % _chip if TOF_ENABLED else
          "VL53L1X: WRONG ID 0x%04X" % _chip)
except Exception as e:
    print("VL53L1X: N/A (%s)" % e)

# ============================================================================
# Distance estimation: ToF fusion + visual fallback + EMA smoothing
# ============================================================================

_smooth_score = -1.0   # EMA 平滑后的全局 distScore (-1 = 未初始化)

def tof_to_dist_score(tof_mm):
    """ToF 距离 (mm) → distScore (0.0-1.0), 分段线性映射.
       返回 -1 表示 ToF 无效."""
    if tof_mm < TOF_MIN_VALID or tof_mm > TOF_MAX_VALID:
        return -1

    if tof_mm < TOF_STOP_MM:
        return 1.0
    if tof_mm < TOF_NEAR_MM:
        t = (tof_mm - TOF_STOP_MM) / (TOF_NEAR_MM - TOF_STOP_MM)
        return 1.0 - 0.15 * t           # 1.0 → 0.85
    if tof_mm < TOF_MID_MM:
        t = (tof_mm - TOF_NEAR_MM) / (TOF_MID_MM - TOF_NEAR_MM)
        return 0.85 - 0.55 * t          # 0.85 → 0.30
    if tof_mm < TOF_FAR_MM:
        t = (tof_mm - TOF_MID_MM) / (TOF_FAR_MM - TOF_MID_MM)
        return 0.30 - 0.25 * t          # 0.30 → 0.05
    return 0.05

def vision_dist_score(w, h, feet_y, frame_w, frame_h):
    """纯视觉距离估计 (ToF 无效时备用).
       返回 (category_str, score, area_ratio)."""
    area_ratio = (w * h) / (frame_w * frame_h)
    top_y = feet_y - h

    if area_ratio > AREA_VERY_CLOSE:
        return ("STOP", 1.0, area_ratio)
    if top_y < TOP_Y_THRESHOLD and area_ratio > AREA_CLOSE:
        return ("STOP", 1.0, area_ratio)

    if area_ratio > AREA_CLOSE:
        area_score = min(1.0, area_ratio / AREA_VERY_CLOSE)
        feet_score = min(1.0, feet_y / frame_h)
        return ("CLOSE_SLOW",
                area_score * WEIGHT_CLOSE_AREA + feet_score * WEIGHT_CLOSE_FEETY,
                area_ratio)

    if area_ratio > AREA_FAR:
        feet_score = min(1.0, feet_y / frame_h)
        area_score = area_ratio / AREA_CLOSE
        return ("MEDIUM",
                feet_score * WEIGHT_MEDIUM_FEETY + area_score * WEIGHT_MEDIUM_AREA,
                area_ratio)

    feet_score = max(0.0, min(1.0, feet_y / FEETY_FAR)) if feet_y < FEETY_FAR else 1.0
    area_score = area_ratio / AREA_FAR
    score = feet_score * WEIGHT_FAR_FEETY + area_score * WEIGHT_FAR_AREA
    if score >= 0.65:
        return ("CLOSE_SLOW", score, area_ratio)
    elif score >= 0.30:
        return ("MEDIUM", score, area_ratio)
    else:
        return ("FULL_SPEED", score, area_ratio)

def fused_distance(tof_mm, w, h, feet_y, frame_w, frame_h):
    """融合距离估计: ToF 主信号 + EMA 平滑 + 视觉备用.
       返回 (category_str, dist_score)."""
    global _smooth_score

    raw = tof_to_dist_score(tof_mm)

    if raw >= 0:  # ToF 有效
        if _smooth_score < 0:
            _smooth_score = raw
        else:
            _smooth_score = TOF_SMOOTH_ALPHA * raw + (1 - TOF_SMOOTH_ALPHA) * _smooth_score
        score = _smooth_score
    else:         # ToF 无效, 退回视觉
        _, score, _ = vision_dist_score(w, h, feet_y, frame_w, frame_h)
        _smooth_score = -1  # 重置平滑, 下次 ToF 有效时重新初始化

    # distScore → category
    if score >= 0.85:
        cat = "STOP"
    elif score >= 0.65:
        cat = "CLOSE_SLOW"
    elif score >= 0.30:
        cat = "MEDIUM"
    else:
        cat = "FULL_SPEED"
    return (cat, score)

# ============================================================================
# Debug drawing (simplified for speed)
# ============================================================================

def draw_debug(img, fps, person_rect, score, dist_category, dist_score):
    if not DRAW_DEBUG:
        return
    w, h = img.width(), img.height()
    # Center cross
    img.draw_cross((w // 2, h // 2), color=200, size=6)
    # FPS
    img.draw_string((3, 3), "%d fps" % int(fps), color=(255,255,255), scale=1)
    # Detection
    if person_rect:
        x, y, w, h = person_rect
        if dist_category == "STOP": c = (255,0,0)
        elif dist_category == "CLOSE_SLOW": c = (255,200,0)
        elif dist_category == "MEDIUM": c = (0,200,255)
        else: c = (0,255,0)
        img.draw_rectangle((x, y, w, h), color=c, thickness=2)
        img.draw_string((3, 13), "%s(%.2f)" % (dist_category, dist_score),
                        color=(255,200,0), scale=1)
    else:
        img.draw_string((3, 13), "No person", color=(255,100,100), scale=1)

# ============================================================================
# LED 状态指示 (参考 2dof-gimbal 项目)
# N6 RGB LED: _LED(1)=红, _LED(2)=绿, _LED(3)=蓝
# ============================================================================

_last_led_state = -1   # 防重复写入
_led_counter    = 0

def update_led(has_person, tof_valid, error=False):
    global _last_led_state, _led_counter
    _led_counter += 1

    if error:
        new_state = 1       # 红 = 错误
    elif has_person:
        if tof_valid:
            new_state = 2   # 绿 = 有人 + ToF 有效
        else:
            new_state = 4   # 绿闪烁 = 有人 + ToF 无效
    else:
        new_state = 3       # 蓝 = 扫描中 (无目标)

    if new_state != _last_led_state:
        _LED(1).off(); _LED(2).off(); _LED(3).off()
        if new_state == 1:
            _LED(1).on()
        elif new_state == 2:
            _LED(2).on()
        elif new_state == 3:
            _LED(3).on()
        elif new_state == 4:
            if _led_counter % 2:
                _LED(2).on()
        _last_led_state = new_state
    elif new_state == 4:    # ToF 丢失时绿闪
        if _led_counter % 8 == 0:
            _LED(2).toggle()

# ============================================================================
# Main loop
# ============================================================================

print("=" * 50)
print("Platform: %s | Model: %s | NPU: %s" %
      ("N6" if IS_N6 else "H7", _MODEL_PATH.split("/")[-1], str(IS_N6)))
print("VIS: every %dms | FPS target: ~%d" %
      (VIS_INTERVAL_MS, 30 // (SENSOR_SKIP_FRAMES + 1)))
print("=" * 50)

clock = time.clock()
last_vis_ms  = time.ticks_ms()
last_print_ms = time.ticks_ms()
frame_counter = 0
no_person_count = 0
total_detections = 0
uart_errors = 0
tof_distance = 0

while True:
    clock.tick()
    now_ms = time.ticks_ms()

    # Frame throttle (N6=0 skip, H7=2 skip)
    if frame_counter % (SENSOR_SKIP_FRAMES + 1) != 0:
        frame_counter += 1
        continue
    frame_counter += 1

    # Capture
    img = capture()
    img_w, img_h = img.width(), img.height()

    # Read ToF (20Hz — VL53L1X max 50Hz, 避免高频读取导致 ENODEV)
    if TOF_ENABLED and frame_counter % 2 == 0:  # N6~48fps → ~24Hz ToF
        try:
            d = tof.read()
            if d > 0:
                tof_distance = d
        except:
            pass  # 保持上次有效读数

    # Detection
    results = model.predict([img])
    detections = results[person_idx] if person_idx < len(results) else []

    person_rect = None
    dist_category = ""
    dist_score = 0.0
    target_cx = target_cy = target_w = target_h = target_feet_y = 0

    if detections:
        detections.sort(key=lambda d: d[0][1] + d[0][3], reverse=True)
        rect, score = detections[0]
        x, y, w, h = rect[0], rect[1], rect[2], rect[3]

        target_cx = int(x + w // 2)
        target_cy = int(y + h // 2)
        target_w  = int(w)
        target_h  = int(h)
        target_feet_y = int(y + h)

        dist_category, dist_score = fused_distance(
            tof_distance, target_w, target_h, target_feet_y, img_w, img_h)

        person_rect = (x, y, w, h)
        no_person_count = 0
        total_detections += 1
    else:
        no_person_count += 1

    # Debug overlay
    draw_debug(img, clock.fps(), person_rect, score if detections else 0,
               dist_category, dist_score)

    # LED 状态更新 (每 2 帧一次, 节省 I2C)
    tof_ok = TOF_ENABLED and TOF_MIN_VALID <= tof_distance <= TOF_MAX_VALID
    if frame_counter % 2 == 0:
        update_led(person_rect is not None, tof_ok)

    # VIS output (scale coords to 192x192 for ESP32 FollowLogic compatibility)
    if time.ticks_diff(time.ticks_ms(), last_vis_ms) >= VIS_INTERVAL_MS:
        last_vis_ms = now_ms
        if person_rect:
            sx = _VIS_W / img_w
            sy = _VIS_H / img_h
            data_str = "%d,%d,%d,%d,%d,%.2f,PERSON,%.2f,%d" % (
                int(target_cx * sx), int(target_cy * sy),
                int(target_w * sx), int(target_h * sy),
                int(target_feet_y * sy), score, dist_score, tof_distance)
        else:
            data_str = "0,0,0,0,0,0.00,NONE,0.00,0"
        # XOR checksum
        csum = 0
        for ch in data_str:
            csum ^= ord(ch)
        try:
            vis_uart.write("VIS:%s*%d\r\n" % (data_str, csum))
        except Exception as e:
            uart_errors += 1

    # Serial debug
    if time.ticks_diff(time.ticks_ms(), last_print_ms) >= PRINT_EVERY_MS:
        last_print_ms = now_ms
        fps = clock.fps()
        if person_rect:
            print("%dfps PERSON cx=%-3d cy=%-3d %dx%d fy=%-3d conf=%.2f dist=%s(%.2f) tof=%dmm" %
                  (int(fps), target_cx, target_cy, target_w, target_h,
                   target_feet_y, score, dist_category, dist_score, tof_distance))
        else:
            print("%dfps SCANNING (n=%d)" % (int(fps), no_person_count))

    # GC
    del img
    if person_rect:
        del detections
    if frame_counter % GC_EVERY_FRAMES == 0:
        gc.collect()
