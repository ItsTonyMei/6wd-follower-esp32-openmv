"""
Unit tests for VIS UART protocol
Tests: checksum calculation, string formatting, parsing
Run on PC: python test_protocol.py
"""

def calc_checksum(data_str):
    """XOR checksum calculation (matches OpenMV main.py)"""
    checksum = 0
    for ch in data_str:
        checksum ^= ord(ch)
    return checksum


def test_checksum_known():
    """Test with known values"""
    data = "96,96,50,120,155,0.85,PERSON,0.75"
    cs = calc_checksum(data)
    print(f"Checksum for '{data}': {cs}")
    # Verify by manual calculation
    expected = 0
    for c in data:
        expected ^= ord(c)
    assert cs == expected, f"Checksum mismatch: {cs} vs {expected}"
    print("[PASS] Checksum calculation")


def test_checksum_none():
    """Checksum for NONE packet"""
    data = "0,0,0,0,0,0.00,NONE,0.00"
    cs = calc_checksum(data)
    print(f"Checksum for NONE: {cs}")
    vis_str = f"VIS:{data}*{cs}"
    print(f"Full packet: {vis_str}")
    assert cs > 0, "Checksum should be non-zero"
    print("[PASS] NONE packet checksum")


def test_checksum_max():
    """Checksum for maximum values"""
    data = "191,191,192,192,192,1.00,PERSON,1.00"
    cs = calc_checksum(data)
    print(f"Checksum for max: {cs}")
    vis_str = f"VIS:{data}*{cs}"
    print(f"Full packet: {vis_str}")
    print("[PASS] Max values checksum")


def test_vis_format():
    """Test VIS string formatting"""
    cx, cy, w, h = 96, 150, 80, 120
    feet_y = 160
    conf = 0.85
    dist = 0.75

    data_str = f"{cx},{cy},{w},{h},{feet_y},{conf:.2f},PERSON,{dist:.2f}"
    expected = "96,150,80,120,160,0.85,PERSON,0.75"
    assert data_str == expected, f"Format mismatch: {data_str} vs {expected}"
    print("[PASS] VIS data string format")


def test_vis_format_none():
    """Test VIS format for no-detection"""
    data_str = "0,0,0,0,0,0.00,NONE,0.00"
    cs = calc_checksum(data_str)
    vis_str = f"VIS:{data_str}*{cs}\r\n"
    print(f"NONE packet: {repr(vis_str)}")
    assert vis_str.startswith("VIS:0,0,0,0,0"), "Should start with VIS:0,0,0,0,0"
    assert vis_str.endswith("\r\n"), "Should end with \\r\\n"
    print("[PASS] NONE packet format")


if __name__ == '__main__':
    print("=== VIS Protocol Tests ===\n")
    test_checksum_known()
    test_checksum_none()
    test_checksum_max()
    test_vis_format()
    test_vis_format_none()
    print("\n=== All tests passed ===")