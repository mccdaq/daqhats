#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
Excitation signal synthesis for reverberation-time measurement (ISO 3382-2).

These signals drive the DT9837A's analog output so the laptop can excite a
room and deconvolve the response into an impulse response (T20/T30, needed
for L'nT and L'n). The generator here is hardware-agnostic: it returns a
normalized signal (peak amplitude = 10**(level_dbfs/20), so 1.0 at 0 dBFS);
the caller multiplies by the output device's full-scale volts.

The sweep and MLS generators here MUST be bit-identical to the laptop
client's implementation, or deconvolution produces noise instead of an
impulse response -- port this file rather than reimplementing the
algorithms independently.
"""
from __future__ import print_function

import numpy as np

#: Fibonacci-LFSR feedback taps (polynomial exponents) for a maximal-length
#: sequence at each supported register order.
#:
#: WARNING: exponent t maps to register index t-1, not order-t. The
#: order-t mapping also produces a full-period sequence (so a period-length
#: check alone will not catch the mistake), but its circular
#: autocorrelation collapses from a peak-to-sidelobe ratio of N to about 1,
#: and deconvolution against it silently returns noise. Verify any change
#: here with mls_autocorr_ratio(): it must equal exactly 2**order - 1.
MLS_TAPS = {
    8: (8, 6, 5, 4),
    10: (10, 7),
    12: (12, 6, 4, 1),
    14: (14, 5, 3, 1),
    15: (15, 14),
    16: (16, 15, 13, 4),
    17: (17, 14),
    18: (18, 11),
}


def _fade(n, rate, ms=20.0):
    """Raised-cosine (Hann) fade-in/out envelope, `ms` at each end."""
    n_fade = min(max(1, int(round(rate * ms / 1000.0))), n // 2)
    env = np.ones(n, dtype=np.float64)
    if n_fade > 0:
        ramp = 0.5 * (1.0 - np.cos(np.pi * np.arange(n_fade) / n_fade))
        env[:n_fade] = ramp
        env[n - n_fade:] = ramp[::-1]
    return env


def _normalize(x, level_dbfs):
    peak = np.max(np.abs(x)) if x.size else 0.0
    if peak > 0:
        x = x / peak
    return x * (10.0 ** (level_dbfs / 20.0))


def generate_sweep(seconds, f_min, f_max, rate, level_dbfs=0.0):
    """Exponential (Farina) sine sweep from f_min to f_max over `seconds`,
    with a 20 ms raised-cosine fade at both ends, normalized to unit peak
    and scaled by level_dbfs."""
    if f_min <= 0 or f_max <= f_min:
        raise ValueError('f_min must be > 0 and f_max > f_min')
    n = int(round(seconds * rate))
    if n < 2:
        raise ValueError('seconds too short for the given rate')
    t = np.arange(n, dtype=np.float64) / rate
    w1 = 2.0 * np.pi * f_min
    w2 = 2.0 * np.pi * f_max
    big_t = seconds
    log_ratio = np.log(w2 / w1)
    k = w1 * big_t / log_ratio
    s = np.sin(k * (np.exp(t / big_t * log_ratio) - 1.0))
    s *= _fade(n, rate)
    return _normalize(s, level_dbfs)


def generate_mls(order, level_dbfs=0.0, repeats=2):
    """Maximal-length sequence via a Fibonacci LFSR (register initialized
    to all ones, output taken from the last cell, {0,1} -> {-1,+1}),
    played back-to-back for `repeats` whole periods with no gaps between
    them (deconvolution uses the last complete period, by which point the
    room response has reached steady state)."""
    if order not in MLS_TAPS:
        raise ValueError('unsupported MLS order: {} (supported: {})'.format(
            order, sorted(MLS_TAPS)))
    taps = MLS_TAPS[order]
    n = (1 << order) - 1
    reg = np.ones(order, dtype=np.uint8)
    out = np.empty(n, dtype=np.uint8)
    for i in range(n):
        out[i] = reg[-1]
        feedback = 0
        for t in taps:
            feedback ^= int(reg[t - 1])
        reg[1:] = reg[:-1]
        reg[0] = feedback
    seq = np.where(out > 0, 1.0, -1.0)
    scale = 10.0 ** (level_dbfs / 20.0)
    return np.tile(seq, max(1, int(repeats))) * scale


def mls_autocorr_ratio(order):
    """Peak-to-sidelobe ratio of one period's circular autocorrelation.
    Must equal exactly 2**order - 1 for a genuine maximal-length sequence
    -- see the warning on MLS_TAPS."""
    seq = generate_mls(order, level_dbfs=0.0, repeats=1)
    n = seq.size
    spec = np.fft.rfft(seq)
    autocorr = np.fft.irfft(spec * np.conj(spec), n)
    peak = autocorr[0]
    sidelobe = np.max(np.abs(autocorr[1:]))
    return peak / sidelobe if sidelobe > 0 else float('inf')


def generate_noise(kind, seconds, rate, f_min, f_max, level_dbfs=0.0,
                   rng=None):
    """Band-limited noise in [f_min, f_max]. 'white' is flat in-band;
    'pink' is shaped exactly 1/sqrt(f) in the frequency domain (an exact
    -3 dB/octave slope, not a filter-cascade approximation)."""
    if kind not in ('white', 'pink'):
        raise ValueError('kind must be "white" or "pink"')
    n = int(round(seconds * rate))
    if n < 2:
        raise ValueError('seconds too short for the given rate')
    rng = rng if rng is not None else np.random.default_rng()
    x = rng.standard_normal(n)
    spec = np.fft.rfft(x)
    freqs = np.fft.rfftfreq(n, 1.0 / rate)
    band = (freqs >= f_min) & (freqs <= f_max)
    spec = np.where(band, spec, 0.0)
    if kind == 'pink':
        with np.errstate(divide='ignore'):
            shape = np.where(freqs > 0, 1.0 / np.sqrt(freqs), 0.0)
        spec = spec * shape
    y = np.fft.irfft(spec, n)
    y *= _fade(n, rate)
    return _normalize(y, level_dbfs)


def generate(signal, rate, seconds=None, f_min=None, f_max=None,
            level_dbfs=0.0, mls_order=None, repeats=1, rng=None):
    """Dispatch to the right generator by name, matching set_output's
    'signal' field. Returns a normalized float64 array."""
    if signal == 'sweep':
        return generate_sweep(seconds, f_min, f_max, rate, level_dbfs)
    if signal == 'mls':
        return generate_mls(mls_order, level_dbfs, repeats)
    if signal in ('white', 'pink'):
        return generate_noise(signal, seconds, rate, f_min, f_max,
                              level_dbfs, rng)
    raise ValueError('unknown signal: {}'.format(signal))
