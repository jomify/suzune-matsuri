"""Build the game's sound effects into SuzuneMatsuri/SourceArt/Audio/Sfx/SFX_<Name>.wav.

    python tools/audio/build_sfx.py

Two sources:
  * Kenney "RPG Audio" (CC0, kenney.nl) - SourceArt/Audio/kenney_rpg-audio.zip, converted from OGG with
    ffmpeg (MATSURI_FFMPEG, else ffmpeg on PATH, else the copy that ships with GPT-SoVITS on this machine).
  * Synthesised here with numpy for what the pack lacks: a plucked bowstring, the suzu bell, whooshes,
    chimes, a wood-block click and short koto-like phrases (Karplus-Strong plucks on the in-scale).
All files: mono, 44.1 kHz, 16-bit, peak at -1 dBFS. tools/ue/import_audio.py imports them.
"""
from __future__ import annotations

import os
import shutil
import subprocess
import tempfile
import wave
import zipfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
PROJECT = HERE.parents[1] / "SuzuneMatsuri"
ZIP = PROJECT / "SourceArt" / "Audio" / "kenney_rpg-audio.zip"
OUT = PROJECT / "SourceArt" / "Audio" / "Sfx"
RATE = 44100
RNG = np.random.default_rng(7)  # the same sounds on every build

# Kenney RPG Audio picks: our name -> file inside the zip.
KENNEY = {
    "Footstep_1": "Audio/footstep00.ogg",
    "Footstep_2": "Audio/footstep02.ogg",
    "Footstep_3": "Audio/footstep04.ogg",
    "Footstep_4": "Audio/footstep06.ogg",
    "CoinPickup": "Audio/handleCoins.ogg",
    "ZeniThrow": "Audio/handleCoins2.ogg",
    "TalismanPickup": "Audio/bookFlip1.ogg",
    "KunaiThrow": "Audio/drawKnife1.ogg",
    "MeleeSwing": "Audio/cloth2.ogg",
    "MeleeHit": "Audio/knifeSlice.ogg",
    "ArrowThunk": "Audio/chop.ogg",
    "BellBounce": "Audio/metalClick.ogg",
    "CoinBounce": "Audio/metalPot1.ogg",
    "DryFire": "Audio/metalLatch.ogg",
}

IN_SCALE = [293.66, 311.13, 392.00, 440.00, 466.16, 587.33]  # D Eb G A Bb D': the Japanese in-scale

# Peak level in dBFS (default -1). Short percussive sounds at full scale sound far louder than they measure:
# the hit confirm is heard dozens of times a floor and should sit under the fight, not on top of it.
PEAK_DB = {"HitTick": -10.0, "Kill": -6.0, "BowDraw": -8.0, "Graze": -14.0, "BossShot": -6.0, "GrappleFire": -6.0}

DRAW_SECONDS = 0.8  # Sakura's full draw (MatsuriHeroes.cpp: Bow.MaxChargeSeconds)


def find_ffmpeg() -> str:
    for candidate in (os.environ.get("MATSURI_FFMPEG"), shutil.which("ffmpeg")):
        if candidate and Path(candidate).exists():
            return candidate
    raise SystemExit("ffmpeg not found: set MATSURI_FFMPEG")


def write_wav(name: str, samples: np.ndarray) -> None:
    peak = float(np.max(np.abs(samples))) or 1.0
    pcm = np.clip(samples / peak * 10 ** (PEAK_DB.get(name, -1.0) / 20), -1.0, 1.0)
    with wave.open(str(OUT / f"SFX_{name}.wav"), "wb") as out:
        out.setnchannels(1)
        out.setsampwidth(2)
        out.setframerate(RATE)
        out.writeframes((pcm * 32767).astype("<i2").tobytes())
    print(f"[sfx] {name}: {len(samples) / RATE:.2f} s")


def t(seconds: float) -> np.ndarray:
    return np.arange(int(seconds * RATE)) / RATE


def envelope(n: int, attack: float, decay: float) -> np.ndarray:
    times = np.arange(n) / RATE
    return np.minimum(times / max(attack, 1e-4), 1.0) * np.exp(-times / decay)


def lowpass(samples: np.ndarray, cutoff: float) -> np.ndarray:
    alpha = 1.0 - np.exp(-2.0 * np.pi * cutoff / RATE)
    out = np.empty_like(samples)
    acc = 0.0
    for i, x in enumerate(samples):
        acc += alpha * (x - acc)
        out[i] = acc
    return out


def pluck(freq: float, seconds: float, brightness: float = 0.5, decay: float = 0.996) -> np.ndarray:
    """Karplus-Strong plucked string (koto, bowstring)."""
    period = int(RATE / freq)
    buffer = RNG.uniform(-1, 1, period)
    buffer = lowpass(buffer, 2000 + 6000 * brightness)
    out = np.empty(int(seconds * RATE))
    for i in range(len(out)):
        j = i % period
        out[i] = buffer[j]
        buffer[j] = decay * 0.5 * (buffer[j] + buffer[(j + 1) % period])
    return out * envelope(len(out), 0.002, seconds * 0.45)


def noise_sweep(seconds: float, start: float, end: float) -> np.ndarray:
    """Band-limited noise whose brightness sweeps from start to end Hz (a whoosh)."""
    n = int(seconds * RATE)
    noise = RNG.uniform(-1, 1, n)
    out = np.empty(n)
    acc = 0.0
    for i in range(n):
        cutoff = start + (end - start) * i / n
        alpha = 1.0 - np.exp(-2.0 * np.pi * cutoff / RATE)
        acc += alpha * (noise[i] - acc)
        out[i] = acc
    return out * np.sin(np.pi * np.arange(n) / n) ** 1.5


def resonate(samples: np.ndarray, freq, q: float) -> np.ndarray:
    """Two-pole band-pass resonator; freq may be an array (a sweeping resonance)."""
    freqs = np.broadcast_to(np.asarray(freq, dtype=float), samples.shape)
    out = np.zeros_like(samples)
    y1 = y2 = 0.0
    for i, x in enumerate(samples):
        w = 2 * np.pi * freqs[i] / RATE
        r = np.exp(-w / (2 * q))
        y = (1 - r) * x + 2 * r * np.cos(w) * y1 - r * r * y2
        out[i] = y
        y2, y1 = y1, y
    return out


def bow_draw(seconds: float) -> np.ndarray:
    """A bow being drawn: the shaft hissing across the rest, rising with the tension, over the faint
    stick-slip creak of the string and limbs (a quickening train of tiny clicks through a wooden resonance)."""
    n = int(seconds * RATE)
    times = np.arange(n) / RATE
    tension = np.clip(times / (seconds * 0.85), 0, 1) ** 1.6
    noise = RNG.uniform(-1, 1, n)
    hiss = lowpass(noise, 2600) - lowpass(noise, 650)
    grain = lowpass(RNG.uniform(-1, 1, n), 35)
    fibres = 0.7 + 0.3 * grain / (np.max(np.abs(grain)) or 1.0)
    friction = hiss * fibres * (0.15 + 0.85 * tension)
    clicks = np.zeros(n)
    phase = 0.0
    for i in range(n):
        phase += (14 + 46 * tension[i]) / RATE
        if phase >= 1.0:
            phase -= 1.0
            clicks[i] = RNG.uniform(0.4, 1.0)
    creak = resonate(clicks, 360 + 160 * tension, 9)
    creak *= 0.35 * np.max(np.abs(friction)) / (np.max(np.abs(creak)) or 1.0) * tension
    fade = np.minimum(np.minimum(times / 0.02, 1.0), np.minimum((seconds - times) / 0.05, 1.0))
    return lowpass(friction + creak, 4200) * fade  # soft: a hiss above 8 kHz read as tape noise, not wood


def thud(body_hz: float, body_s: float, snap_hz: float, snap_s: float, seconds: float) -> np.ndarray:
    """A soft hit: a low body that drops in pitch, with a muffled snap and a little paper on the front."""
    times = t(seconds)
    n = len(times)
    pitch = body_hz * (1 - 0.3 * np.minimum(times / body_s, 1))
    body = np.sin(2 * np.pi * np.cumsum(pitch) / RATE) * envelope(n, 0.001, body_s)
    snap = lowpass(lowpass(RNG.uniform(-1, 1, n), snap_hz), snap_hz) * envelope(n, 0.0005, snap_s) * 9
    top = RNG.uniform(-1, 1, n)
    paper = (lowpass(top, 5200) - lowpass(top, 2800)) * envelope(n, 0.0003, 0.004) * 0.25
    return body + snap + paper


def bell(partials, seconds: float, decay: float) -> np.ndarray:
    times = t(seconds)
    tone = sum(amp * np.sin(2 * np.pi * freq * times + RNG.uniform(0, 6)) * np.exp(-times / (decay * fall))
               for freq, amp, fall in partials)
    return tone * envelope(len(times), 0.001, seconds)


def mix(*parts) -> np.ndarray:
    """parts: (start_seconds, samples)"""
    length = max(int(start * RATE) + len(s) for start, s in parts)
    out = np.zeros(length)
    for start, samples in parts:
        i = int(start * RATE)
        out[i:i + len(samples)] += samples
    return out


def synthesise() -> dict[str, np.ndarray]:
    suzu = [(2210, 1.0, 1.0), (3480, 0.6, 0.7), (5230, 0.35, 0.45), (7120, 0.2, 0.3)]
    sounds = {
        # A taut string let go: a low pluck with a hard thump of the bow limbs.
        "BowRelease": mix((0, pluck(98, 0.5, 0.9, 0.990) * 0.9), (0, lowpass(RNG.uniform(-1, 1, 1800), 300) * envelope(1800, 0.001, 0.02) * 3)),
        "ArrowWhoosh": noise_sweep(0.32, 4500, 900) * 0.8,
        # The suzu: a bright cluster of inharmonic partials, struck twice as it shakes.
        "BellShot": mix((0, bell(suzu, 0.9, 0.35)), (0.07, bell(suzu, 0.8, 0.3) * 0.55)),
        "GemPickup": mix(*[(i * 0.07, pluck(IN_SCALE[(2 + i) % 6] * 2, 0.6, 0.8)) for i in range(4)]),
        # Hit confirm: a muffled "thup", heard many times a floor, so soft and short (playtest: the old 2.3 kHz
        # beep was loud and odd). The kill is a heavier one; the yokai's own dissolve (YokaiDeath) carries the rest.
        # The body sits at 230 / 150 Hz, not lower: laptop speakers drop everything under ~200 Hz.
        "HitTick": thud(230, 0.022, 2400, 0.012, 0.08),
        "Kill": thud(150, 0.05, 1800, 0.025, 0.2),
        # Playtest: Kenney's creak (a door) did not read as a bow. Exactly as long as a full draw.
        "BowDraw": bow_draw(DRAW_SECONDS),
        # The boss fight. A graze is heard dozens of times a second at worst (the game throttles it): a tiny tick.
        "Graze": lowpass(RNG.uniform(-1, 1, int(0.025 * RATE)), 7000) * envelope(int(0.025 * RATE), 0.0005, 0.006)
                 - lowpass(RNG.uniform(-1, 1, int(0.025 * RATE)), 3500) * envelope(int(0.025 * RATE), 0.0005, 0.006),
        # She casts: foxfire rushing out over a low bell.
        "BossShot": mix((0, noise_sweep(0.45, 500, 3200) * 0.7), (0.05, bell([(740, 1.0, 1.0), (1110, 0.5, 0.7), (1660, 0.25, 0.5)], 0.8, 0.3) * 0.5)),
        # A new health bar: a deep temple bell and a rising rush.
        "BossPhase": mix((0, bell([(110, 1.0, 1.0), (165, 0.6, 0.8), (277, 0.4, 0.6), (415, 0.2, 0.4)], 2.2, 0.9)),
                         (0.1, noise_sweep(1.2, 200, 4000) * 0.35)),
        "BossTeleport": noise_sweep(0.32, 5000, 700) * 0.9,
        # The grapple's rope paying out: a click and a rising zip.
        "GrappleFire": mix((0, RNG.uniform(-1, 1, 200) * envelope(200, 0.0001, 0.002)), (0.01, noise_sweep(0.28, 1500, 6000) * 0.8)),
        # Her defeat: the in-scale falling, then a bell left ringing.
        "BossDefeat": mix(*[(i * 0.16, pluck(f * 2, 1.4, 0.7)) for i, f in enumerate(reversed([IN_SCALE[0], IN_SCALE[2], IN_SCALE[3], IN_SCALE[4], IN_SCALE[5]]))],
                          (0.9, bell([(1318, 1.0, 1.0), (1976, 0.5, 0.7), (2637, 0.3, 0.5)], 2.0, 0.8) * 0.6)),
        "YokaiDeath": noise_sweep(0.55, 2500, 250) * 0.9,
        "PlayerHurt": mix((0, np.sin(2 * np.pi * 70 * t(0.35)) * envelope(int(0.35 * RATE), 0.002, 0.09)),
                          (0, lowpass(RNG.uniform(-1, 1, int(0.25 * RATE)), 1200) * envelope(int(0.25 * RATE), 0.001, 0.05) * 1.5)),
        # Wood block: a short damped tone with a click on top.
        "UiClick": mix((0, np.sin(2 * np.pi * 880 * t(0.08)) * envelope(int(0.08 * RATE), 0.0005, 0.018)),
                       (0, RNG.uniform(-1, 1, 120) * envelope(120, 0.0001, 0.001) * 0.4)),
        "UiConfirm": mix((0, pluck(IN_SCALE[3] * 2, 0.7, 0.7)), (0.09, pluck(IN_SCALE[5] * 2, 0.9, 0.7))),
        "Loading": mix((0, noise_sweep(0.6, 300, 3200) * 0.6), (0.45, bell([(2637, 1.0, 1.0), (3951, 0.4, 0.6)], 0.7, 0.25) * 0.5)),
        "FloorClear": mix(*[(i * 0.13, pluck(f * 2, 1.2, 0.7)) for i, f in enumerate([IN_SCALE[0], IN_SCALE[2], IN_SCALE[3], IN_SCALE[5], IN_SCALE[3] * 2])]),
    }
    return sounds


def convert_kenney(ffmpeg: str) -> None:
    with zipfile.ZipFile(ZIP) as archive, tempfile.TemporaryDirectory() as temp:
        for name, inner in KENNEY.items():
            source = Path(temp) / Path(inner).name
            source.write_bytes(archive.read(inner))
            target = OUT / f"SFX_{name}.wav"
            subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-i", str(source), "-ac", "1", "-ar", str(RATE),
                            "-af", "loudnorm=I=-16:TP=-1.5:LRA=11", "-sample_fmt", "s16", str(target)], check=True)
            print(f"[sfx] {name} <- kenney {inner}")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    convert_kenney(find_ffmpeg())
    for name, samples in synthesise().items():
        write_wav(name, samples)
    (OUT.parent / "CREDITS.md").write_text(
        "# Audio credits\n\n"
        "- Footsteps, coins, paper, knife, cloth, chop and metal sounds: **RPG Audio** by Kenney "
        "(www.kenney.nl), Creative Commons CC0. `kenney_rpg-audio.zip`.\n"
        "- Bow draw and release, bell, whooshes, hit thuds, chimes, clicks and the koto phrases: synthesised by `tools/audio/build_sfx.py`.\n",
        encoding="utf-8")


if __name__ == "__main__":
    main()
