# OpenMV Unit Tests

PC 端运行 (需 pytest), 无需 OpenMV 硬件

## Running Tests

```bash
cd Test/OpenMV
pip install pytest
pytest test_person_detector.py -v
python test_protocol.py
```

## Test Files

| File | Target | Description |
|------|--------|-------------|
| `test_person_detector.py` | 距离估计 | Multi-feature fusion, area/feetY weighting, boundary cases |
| `test_protocol.py` | UART VIS 协议 | XOR checksum, VIS string formatting |

## Notes

- `test_person_detector.py` 包含 `estimate_distance()` 的纯函数副本 (pure function copy)，无需 OpenMV 硬件依赖
- `test_protocol.py` 独立运行，测试 XOR checksum 计算和 VIS packet 格式化
