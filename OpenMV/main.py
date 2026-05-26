# ============================================================================
# 履带车视觉跟随系统 — OpenMV L1 感知层
# N6: YOLOv8n NPU 加速 (120+ FPS) + VL53L1X ToF + VIS/UART → ESP32
# H7: YOLO-LC CPU 推理 (10 FPS) — 自动降级兼容
# ============================================================================

import gc, pyb, time, math, os
import ml

# ============================================================================
# Board detection — N6 用 csi, H7 Plus 用 sensor
# ============================================================================
IS_N6 = "yolov8n_192.tflite" in os.listdir("/rom")

if IS_N6:
    import csi as _cam_mod
    from ml.postprocessing.ultralytics import YoloV8
    _POSTPROC = YoloV8
    _MODEL_PATH = "/rom/yolov8n_192.tflite"
    _CAM_W, _CAM_H = 320, 240       # QVGA
    _FRAME_W, _FRAME_H = 192, 192   # 模型输入
    _FRAME_CX, _FRAME_CY = 96, 96
else:
    import sensor as _cam_mod
    from ml.postprocessing.darknet import YoloLC
    _POSTPROC = YoloLC
    _MODEL_PATH = "/rom/yolo_lc_192.tflite"
    _CAM_W, _CAM_H = 320, 240
    _FRAME_W, _FRAME_H = 192, 192
    _FRAME_CX, _FRAME_CY = 96, 96

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
VIS_INTERVAL_MS      = 100 if IS_N6 else 200  # N6 更快的更新率

# ---- 距离估计 ----
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
        # N6 CSI: window(), H7 sensor: set_windowing()
        if hasattr(_cam_obj, "window"):
            _cam_obj.window((_FRAME_W, _FRAME_H))
        elif hasattr(_cam_obj, "set_windowing"):
            _cam_obj.set_windowing((_FRAME_W, _FRAME_H))
        _cam_obj.snapshot(time=CAMERA_STABILIZE_MS)
    else:
        _cam_mod.reset()
        _cam_mod.set_contrast(CAMERA_CONTRAST)
        _cam_mod.set_gainceiling(CAMERA_GAINCEILING)
        _cam_mod.set_framesize(CAMERA_FRAMESIZE)
        _cam_mod.set_pixformat(CAMERA_PIXFORMAT)
        _cam_mod.set_hmirror(CAMERA_HMIRROR)
        _cam_mod.set_vflip(CAMERA_VFLIP)
        _cam_mod.set_windowing((_FRAME_W, _FRAME_H))
        _cam_mod.skip_frames(time=CAMERA_STABILIZE_MS)

    print("Camera OK: %dx%d → model %dx%d (platform=%s)" %
          (_CAM_W, _CAM_H, _FRAME_W, _FRAME_H, "N6" if IS_N6 else "H7"))

def capture():
    return _cam_obj.snapshot() if IS_N6 else _cam_mod.snapshot()

setup_camera()

# ============================================================================
# VL53L1X ToF init
# ============================================================================

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
# UART bridge to ESP32
# ============================================================================

uart = pyb.UART(3, 115200)
uart.init(115200, bits=8, parity=None, stop=1, timeout=1000)

# ============================================================================
# Distance estimation (same logic for both N6 & H7)
# ============================================================================

def estimate_distance(w, h, feet_y, frame_w, frame_h):
    area_ratio = (w * h) / (frame_w * frame_h)
    top_y = feet_y - h  # approximate

    if area_ratio > AREA_VERY_CLOSE:
        return ("STOP", 1.0, area_ratio)
    if top_y < TOP_Y_THRESHOLD and area_ratio > AREA_CLOSE:
        return ("STOP", 1.0, area_ratio)

    if area_ratio > AREA_CLOSE:
        area_score = min(1.0, area_ratio / AREA_VERY_CLOSE)
        feet_score = min(1.0, feet_y / _FRAME_H)
        return ("CLOSE_SLOW", area_score * WEIGHT_CLOSE_AREA + feet_score * WEIGHT_CLOSE_FEETY, area_ratio)

    if area_ratio > AREA_FAR:
        feet_score = min(1.0, feet_y / _FRAME_H)
        area_score = area_ratio / AREA_CLOSE
        return ("MEDIUM", feet_score * WEIGHT_MEDIUM_FEETY + area_score * WEIGHT_MEDIUM_AREA, area_ratio)

    feet_score = max(0.0, min(1.0, feet_y / FEETY_FAR)) if feet_y < FEETY_FAR else 1.0
    area_score = area_ratio / AREA_FAR
    score = feet_score * WEIGHT_FAR_FEETY + area_score * WEIGHT_FAR_AREA
    if score >= 0.65: return ("CLOSE_SLOW", score, area_ratio)
    elif score >= 0.30: return ("MEDIUM", score, area_ratio)
    else: return ("FULL_SPEED", score, area_ratio)

# ============================================================================
# Debug drawing (simplified for speed)
# ============================================================================

def draw_debug(img, fps, person_rect, score, dist_category, dist_score):
    if not DRAW_DEBUG:
        return
    # Center cross
    img.draw_cross((_FRAME_CX, _FRAME_CY), color=200, size=6)
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

    # Read ToF
    if TOF_ENABLED:
        try:
            tof_distance = tof.read()
        except:
            tof_distance = 0

    # Detection
    results = model.predict([img])
    detections = results[person_idx] if person_idx < len(results) else []

    person_rect = None
    dist_category = ""
    dist_score = 0.0
    target_cx = target_cy = target_w = target_h = target_feet_y = 0

    if detections:
        # Pick lowest (closest) person
        # YOLOv8: ([x,y,w,h], score) — rect 是 list
        detections.sort(key=lambda d: d[0][1] + d[0][3], reverse=True)
        rect, score = detections[0]
        x, y, w, h = rect[0], rect[1], rect[2], rect[3]

        # N6 YOLOv8: coords already in model space (192x192 via windowing)
        target_cx = int(x + w // 2)
        target_cy = int(y + h // 2)
        target_w  = int(w)
        target_h  = int(h)
        target_feet_y = int(y + h)

        dist_category, dist_score, _ = estimate_distance(
            target_w, target_h, target_feet_y, _FRAME_W, _FRAME_H)

        person_rect = (x, y, w, h)
        no_person_count = 0
        total_detections += 1
    else:
        no_person_count += 1

    # Debug overlay
    draw_debug(img, clock.fps(), person_rect, score if detections else 0,
               dist_category, dist_score)

    # VIS output
    if time.ticks_diff(time.ticks_ms(), last_vis_ms) >= VIS_INTERVAL_MS:
        last_vis_ms = now_ms
        if person_rect:
            data_str = "%d,%d,%d,%d,%d,%.2f,PERSON,%.2f,%d" % (
                target_cx, target_cy, target_w, target_h,
                target_feet_y, score, dist_score, tof_distance)
        else:
            data_str = "0,0,0,0,0,0.00,NONE,0.00,0"
        # XOR checksum
        csum = 0
        for ch in data_str:
            csum ^= ord(ch)
        try:
            uart.write("VIS:%s*%d\r\n" % (data_str, csum))
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
