# ============================================================================
# 履带车视觉跟随系统 — OpenMV L1 感知层
# 功能: YOLO person detection + VL53L1X ToF 激光测距 + VIS/UART → ESP32
# 兼容: OpenMV H7 Plus (fw 4.x) 和 N6 (fw 5.0.0)
#
# 硬件配置:
#   VL53L1X ToF → I2C(2) (SCL=P4, SDA=P5), addr 0x29, 40-4000mm ±1mm
#   UART3 → ESP32 (P4 TX, 115200 baud, VIS 协议)
#   N6 用户: model 须通过 OpenMV IDE → Tools → Edit ROM FS → Add File
#     选择 yolo_lc_192.tflite → Commit (IDE 自动转换为 Neural-ART NPU 格式)
# ============================================================================

import gc
import pyb
import time
import math
import ml
from ml.postprocessing.darknet import YoloLC

# ============================================================================
# Board detection — N6 使用 csi 模块, H7 Plus 使用 sensor 模块
# ============================================================================
IS_N6 = False
try:
    import csi as _cam_mod
    IS_N6 = True
except ImportError:
    import sensor as _cam_mod

# ============================================================================
# Configuration (single source of truth — 所有可调参数集中于此)
# ============================================================================

# ---- Camera (摄像头) ----
CAMERA_FRAMESIZE    = _cam_mod.QVGA      # 320x240
CAMERA_PIXFORMAT    = _cam_mod.RGB565    # YOLO LC 需要彩色输入
CAMERA_CONTRAST     = 3
CAMERA_GAINCEILING  = 16
CAMERA_HMIRROR      = False
CAMERA_VFLIP        = False
CAMERA_STABILIZE_MS = 2000
SENSOR_SKIP_FRAMES  = 2                 # 跳帧 → ~10 FPS effective

# Model 输入 (中心裁剪 QVGA → 192x192)
MODEL_W = 192
MODEL_H = 192

# ---- Detection (YOLO LC) ----
DETECTION_THRESHOLD = 0.4               # confidence threshold (0-1)

# ---- Multi-feature distance estimation (多特征融合距离估计) ----
AREA_VERY_CLOSE = 0.50                  # area_ratio > this → STOP
AREA_CLOSE      = 0.30                  # area_ratio > this → near mode
AREA_FAR        = 0.10                  # area_ratio <= this → far mode

# Near mode 权重 (area_ratio > AREA_CLOSE)
WEIGHT_CLOSE_AREA  = 0.7
WEIGHT_CLOSE_FEETY = 0.3
# Medium mode 权重 (AREA_FAR < area_ratio <= AREA_CLOSE)
WEIGHT_MEDIUM_FEETY = 0.6
WEIGHT_MEDIUM_AREA  = 0.4
# Far mode 权重 (area_ratio <= AREA_FAR)
WEIGHT_FAR_FEETY = 0.8
WEIGHT_FAR_AREA  = 0.2

TOP_Y_THRESHOLD = 10                   # top_y < this + area > CLOSE → 极近 (STOP)

# Legacy feetY fallback 阈值 (192x192 窗口, distScore 不可用时降级)
FEETY_CLOSE = 155                       # person too close → STOP
FEETY_FAR   = 80                        # person too far → FWD

# ---- Behavior (行为参数) ----
TRACK_DEADBAND         = 0.08           # ~15px at 192 width
NO_PERSON_STOP_FRAMES  = 5

# ---- Debug ----
DRAW_DEBUG     = True
PRINT_EVERY_MS = 500
GC_EVERY_FRAMES = 10

# ---- Validate critical thresholds at import time (启动时校验) ----
assert 0.0 <= DETECTION_THRESHOLD <= 1.0
assert AREA_FAR < AREA_CLOSE < AREA_VERY_CLOSE
assert TOP_Y_THRESHOLD > 0
assert 0 <= FEETY_FAR < FEETY_CLOSE <= MODEL_H
assert NO_PERSON_STOP_FRAMES > 0
assert SENSOR_SKIP_FRAMES >= 0
assert CAMERA_STABILIZE_MS > 0

# ============================================================================
# Debug drawing (调试叠加层绘制)
# ============================================================================

def draw_overlay(img, frame_w, frame_h, frame_cx, frame_cy,
                 status, target_rect, target_cx, target_cy, target_feet_y,
                 score, fps, detect_count,
                 motion_cmd="", dist_category="", dist_score=0.0, raw_features=None):
    # 画面中心十字线
    img.draw_cross(frame_cx, frame_cy, color=200, size=6)

    if target_rect:
        x, y, w, h = target_rect
        # 根据距离分类着色: STOP=红, CLOSE_SLOW=橙, MEDIUM=蓝, FAR/FULL_SPEED=绿
        if dist_category == 'STOP':
            box_color = (255, 0, 0)
        elif dist_category == 'CLOSE_SLOW':
            box_color = (255, 200, 0)
        elif dist_category == 'MEDIUM':
            box_color = (0, 200, 255)
        else:
            box_color = (0, 255, 0)

        img.draw_rectangle(x, y, w, h, color=box_color, thickness=2)
        img.draw_circle(int(target_cx), int(target_cy), 4, color=box_color, thickness=2)
        img.draw_circle(int(target_cx), int(target_feet_y), 5, color=(255, 0, 0), thickness=2)

        # 距离分条 (Distance score bar)
        bar_x, bar_y = 3, frame_h - 12
        bar_w = int(dist_score * (frame_w - 6))
        img.draw_rectangle(bar_x, bar_y, frame_w - 6, 8, color=80, thickness=1)
        if bar_w > 0:
            img.draw_rectangle(bar_x, bar_y, bar_w, 8, color=box_color, fill=True)

    # 左上角状态信息
    img.draw_string(3, 3, "%.1f fps" % fps, color=(255, 255, 255), scale=1)
    img.draw_string(3, 13, "YOLO-LC person", color=(0, 220, 255), scale=1)
    if dist_category:
        img.draw_string(3, 23, "%s (%.2f)" % (dist_category, dist_score), color=(255, 200, 0), scale=1)
    else:
        img.draw_string(3, 23, "No person", color=(255, 100, 100), scale=1)

    # 右上角 confidence
    img.draw_string(frame_w - 38, 3, "%.2f" % score, color=(0, 255, 0), scale=1)

    # feetY 指示器
    if target_feet_y is not None:
        img.draw_string(3, frame_h - 10, "fy=%d" % target_feet_y, color=(255, 100, 0), scale=1)

    # 原始特征值 (area_ratio, top_y)
    if raw_features is not None and DRAW_DEBUG:
        ar = raw_features.get('area_ratio', 0)
        ty = raw_features.get('top_y', 0)
        img.draw_string(60, frame_h - 10, "ar=%.2f ty=%d" % (ar, ty), color=(100, 100, 100), scale=1)


def format_status_line(status, fps, detect_count, target_rect,
                       target_cx, target_cy, target_feet_y, score,
                       dist_category="", dist_score=0.0):
    if target_rect:
        area = target_rect[2] * target_rect[3]
        return "%s cx=%-3d cy=%-3d area=%-4d fy=%-3d score=%.2f dist=%s(%.2f) fps=%.1f" % (
            status, int(target_cx), int(target_cy), area,
            int(target_feet_y) if target_feet_y else 0, score,
            dist_category or "--", dist_score or 0.0, fps)
    return "%s faces=%d fps=%.1f" % (status, detect_count, fps)

# ============================================================================
# YOLO LC Person Detector
# ============================================================================

_MODEL_PATH = "/rom/yolo_lc_192.tflite"


class PersonDetector:
    def __init__(self, threshold=0.4):
        self.model = ml.Model(_MODEL_PATH, postprocess=YoloLC(threshold=threshold))
        self.threshold = threshold
        self.labels = self.model.labels
        print("YOLO LC model loaded:", self.labels)

        self.person_idx = None
        for i, label in enumerate(self.labels):
            if 'person' in label.lower() or 'people' in label.lower():
                self.person_idx = i
                print("Person class index:", i)
                break
        if self.person_idx is None:
            print("Warning: no 'person' label found, using class 0")
            self.person_idx = 0

    def detect(self, img):
        results = self.model.predict([img])
        if self.person_idx >= len(results):
            return []
        person_detections = results[self.person_idx]
        return person_detections if person_detections else []

    def best_detection(self, img):
        detections = self.detect(img)
        if not detections:
            return None
        # 选底部最低的检测框（最接近的人）
        detections.sort(key=lambda d: d[0][1] + d[0][3], reverse=True)
        return detections[0]

    def person_info(self, img, frame_w, frame_h):
        """返回 (cx, cy, w, h, feet_y, score) 或 None"""
        detection = self.best_detection(img)
        if detection is None:
            return None
        (x, y, w, h), score = detection
        # 从 model 坐标 (192x192) 缩放到 frame 坐标
        scale_x = frame_w / 192.0
        scale_y = frame_h / 192.0
        cx = int((x + w // 2) * scale_x)
        cy = int((y + h // 2) * scale_y)
        w_s = int(w * scale_x)
        h_s = int(h * scale_y)
        feet_y = int((y + h) * scale_y)
        return (cx, cy, w_s, h_s, feet_y, score)

    def estimate_distance(self, cx, cy, w, h, feet_y, frame_w, frame_h):
        """多特征融合距离估计 → (category, dist_score, raw_features)"""
        area_ratio = (w * h) / (frame_w * frame_h)
        top_y = cy - h // 2

        raw_features = {
            'area_ratio': area_ratio, 'feet_y': feet_y, 'top_y': top_y,
            'w': w, 'h': h, 'frame_w': frame_w, 'frame_h': frame_h,
        }

        # Segment 1: 极近 (area_ratio > 0.50) → STOP
        if area_ratio > AREA_VERY_CLOSE:
            return ('STOP', 1.0, raw_features)

        # Segment 2: 头部越过画面上边界 + area > CLOSE → STOP
        if top_y < TOP_Y_THRESHOLD and area_ratio > AREA_CLOSE:
            return ('STOP', 1.0, raw_features)

        # Segment 3: Near mode — 加权融合 area_ratio + feet_y
        if area_ratio > AREA_CLOSE:
            area_score = min(1.0, area_ratio / AREA_VERY_CLOSE)
            feet_y_score = min(1.0, feet_y / MODEL_H)
            distance_score = area_score * WEIGHT_CLOSE_AREA + feet_y_score * WEIGHT_CLOSE_FEETY
            return ('CLOSE_SLOW', distance_score, raw_features)

        # Segment 4: Medium mode
        if area_ratio > AREA_FAR:
            feet_y_score = min(1.0, feet_y / MODEL_H)
            area_score = area_ratio / AREA_CLOSE
            distance_score = feet_y_score * WEIGHT_MEDIUM_FEETY + area_score * WEIGHT_MEDIUM_AREA
            return ('MEDIUM', distance_score, raw_features)

        # Segment 5: Far mode
        feet_y_score = max(0.0, min(1.0, feet_y / FEETY_FAR)) if feet_y < FEETY_FAR else 1.0
        area_score = area_ratio / AREA_FAR
        distance_score = feet_y_score * WEIGHT_FAR_FEETY + area_score * WEIGHT_FAR_AREA

        if distance_score >= 0.65:
            return ('CLOSE_SLOW', distance_score, raw_features)
        elif distance_score >= 0.30:
            return ('MEDIUM', distance_score, raw_features)
        else:
            return ('FULL_SPEED', distance_score, raw_features)

# ============================================================================
# Camera setup
# ============================================================================

_cam_obj = None   # CSI instance on N6, None on H7 Plus


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
        if hasattr(_cam_obj, "window"):
            _cam_obj.window((MODEL_W, MODEL_H))
        elif hasattr(_cam_obj, "set_windowing"):
            _cam_obj.set_windowing((MODEL_W, MODEL_H))
        _cam_obj.snapshot(time=CAMERA_STABILIZE_MS)
    else:
        _cam_mod.reset()
        _cam_mod.set_contrast(CAMERA_CONTRAST)
        _cam_mod.set_gainceiling(CAMERA_GAINCEILING)
        _cam_mod.set_framesize(CAMERA_FRAMESIZE)
        _cam_mod.set_pixformat(CAMERA_PIXFORMAT)
        _cam_mod.set_hmirror(CAMERA_HMIRROR)
        _cam_mod.set_vflip(CAMERA_VFLIP)
        _cam_mod.set_windowing((MODEL_W, MODEL_H))
        _cam_mod.skip_frames(time=CAMERA_STABILIZE_MS)

    print("Camera OK: %dx%d (platform=%s)" % (MODEL_W, MODEL_H, "N6" if IS_N6 else "H7"))
    return MODEL_W, MODEL_H, MODEL_W // 2, MODEL_H // 2


def capture():
    return _cam_obj.snapshot() if IS_N6 else _cam_mod.snapshot()


FRAME_W, FRAME_H, FRAME_CX, FRAME_CY = setup_camera()

# ============================================================================
# UART bridge to ESP32 (P4 = TX, 115200 baud)
# 单向发送 VIS 协议帧到 ESP32 UART1 (GPIO15 RX)
# ============================================================================

uart = pyb.UART(3, 115200)
uart.init(115200, bits=8, parity=None, stop=1, timeout=1000)

# ============================================================================
# Main loop
# ============================================================================

detector = PersonDetector(threshold=DETECTION_THRESHOLD)

clock = time.clock()
last_vis_ms = pyb.millis()
last_print_ms = pyb.millis()
frame_counter = 0
no_person_count = 0
total_detections = 0
frame_skip_counter = 0
uart_write_errors = 0

target_cx = target_cy = target_w = target_h = target_feet_y = 0
target_rect = None

print("=" * 50)
print("YOLO LC Person Detection + ESP32 UART Bridge")
print("Platform: %s | VIS every 200ms on P4 TX" % ("N6" if IS_N6 else "H7"))
print("Model: %s | FPS: ~%d" % (_MODEL_PATH, 30 // (SENSOR_SKIP_FRAMES + 1)))
print("Distance: multi-feature fusion (dist_score 0.0-1.0)")
print("=" * 50)

while True:
    clock.tick()
    now_ms = pyb.millis()

    # 帧跳 (Frame throttling) → ~10 FPS
    frame_skip_counter += 1
    if frame_skip_counter <= SENSOR_SKIP_FRAMES:
        continue
    frame_skip_counter = 0

    # 采集 (Capture)
    img = capture()

    # 检测 (Detection)
    info = detector.person_info(img, FRAME_W, FRAME_H)
    dist_category = ""
    dist_score = 0.0
    raw_features = None

    if info is not None:
        target_cx, target_cy, target_w, target_h, target_feet_y, score = info
        target_rect = (target_cx - target_w // 2, target_cy - target_h // 2, target_w, target_h)
        no_person_count = 0
        total_detections += 1

        # 距离估计
        dist_category, dist_score, raw_features = detector.estimate_distance(
            target_cx, target_cy, target_w, target_h, target_feet_y, FRAME_W, FRAME_H)

        if DRAW_DEBUG:
            draw_overlay(img, FRAME_W, FRAME_H, FRAME_CX, FRAME_CY,
                         "DETECTED", target_rect, target_cx, target_cy, target_feet_y,
                         score, clock.fps(), total_detections,
                         dist_category=dist_category, dist_score=dist_score,
                         raw_features=raw_features)
    else:
        no_person_count += 1
        if DRAW_DEBUG:
            img.draw_string(3, 3, "%.1f fps" % clock.fps(), color=(255, 255, 255), scale=1)
            img.draw_string(3, 13, "YOLO-LC person", color=(0, 220, 255), scale=1)
            img.draw_string(3, 23, "No person", color=(255, 100, 100), scale=1)

    # UART VIS 协议输出 (每 200ms)
    if pyb.elapsed_millis(last_vis_ms) >= 200:
        last_vis_ms = now_ms
        if info is not None:
            data_str = "%d,%d,%d,%d,%d,%.2f,PERSON,%.2f" % (
                target_cx, target_cy, target_w, target_h, target_feet_y, score, dist_score)
        else:
            data_str = "0,0,0,0,0,0.00,NONE,0.00"
        # XOR checksum
        checksum = 0
        for ch in data_str:
            checksum ^= ord(ch)
        try:
            uart.write("VIS:%s*%d\r\n" % (data_str, checksum))
        except Exception as e:
            uart_write_errors += 1
            print("UART error: %s (count=%d)" % (e, uart_write_errors))

    # 终端 debug 输出
    if pyb.elapsed_millis(last_print_ms) >= PRINT_EVERY_MS:
        last_print_ms = now_ms
        if info is not None:
            print(format_status_line("DETECTED", clock.fps(), total_detections,
                                     target_rect, target_cx, target_cy, target_feet_y, score,
                                     dist_category, dist_score))
        else:
            print("[SCANNING] no person (n=%d, frame=%d)" % (no_person_count, frame_counter))
        if uart_write_errors and frame_counter % 50 == 0:
            print("UART errors: %d" % uart_write_errors)

    frame_counter += 1

    # 内存管理 (Memory)
    del img
    if info is not None:
        del info
    if frame_counter % GC_EVERY_FRAMES == 0:
        gc.collect()
