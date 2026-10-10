"""Mastering / export checks: BS.1770 reference levels, Ogg serial rewriting (byte-reproducible renders, audio.md 9.3)."""

import zlib

import numpy as np
import pytest

from abyss_audio import SAMPLE_RATE as SR
from abyss_audio import master


def _tone(sec: float, freq: float = 997.0, amp: float = 0.5, channels: int = 2) -> np.ndarray:
    t = np.arange(int(sec * SR)) / SR
    return np.tile(amp * np.sin(2 * np.pi * freq * t), (channels, 1))


def test_lufs_of_a_full_scale_sine():
    # BS.1770: a 997 Hz sine at 0 dBFS in one channel reads -3.01 LUFS; both channels add 3.01 LU.
    assert master.integrated_lufs(_tone(5.0, amp=1.0, channels=1), SR) == pytest.approx(-3.01, abs=0.05)
    assert master.integrated_lufs(_tone(5.0, amp=1.0, channels=2), SR) == pytest.approx(0.0, abs=0.05)


def test_ogg_crc_reference_vector():
    # The Ogg CRC (poly 0x04C11DB7, init 0, unreflected, no final xor) of "123456789" is 0x89A1897F (CRC-32/MPEG-2
    # without the init / final inversion).
    assert master._ogg_crc(b"123456789") == 0x89A1897F


def test_ogg_serial_makes_encodes_byte_identical(tmp_path):
    x = _tone(1.5, amp=0.3)
    a, b = tmp_path / "a.ogg", tmp_path / "b.ogg"
    serial = zlib.crc32(b"SW_TEST_Tone") & 0xFFFFFFFF
    master.write_ogg(str(a), x, SR, serial=serial)
    master.write_ogg(str(b), x, SR, serial=serial)
    assert a.read_bytes() == b.read_bytes()
    raw = a.read_bytes()
    assert int.from_bytes(raw[14:18], "little") == serial
    # libogg verifies every page checksum while decoding: a wrong CRC would drop pages.
    dec, sr = master.read_audio(str(a))
    assert sr == SR and dec.shape == x.shape
    assert float(np.max(np.abs(dec - x))) < 0.05


def test_set_ogg_serial_rejects_garbage():
    with pytest.raises(ValueError):
        master.set_ogg_serial(b"RIFF....WAVE", 1)
