#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
Cross-device clock alignment for PiSLM.

The MCC 172 and the DT9837A run independent ADC clocks (each spec'd to
±50 ppm), so their sample streams slip relative to each other -- up to
~100 µs per second, which is 36° of phase at 1 kHz after one second. A
shared start trigger aligns the *beginning* of the scans but does nothing
about the slip that accumulates afterwards, and the two devices cannot
share a clock (the MCC 172 shares only with other MCC 172s, the DT9837A
only over its own sync bus).

This module closes that gap in software, in two parts:

``ClockTracker``
    Estimates each device's **true** sample rate by regressing delivered
    frame counts against the Pi's monotonic clock. The Pi's own clock error
    cancels in the *ratio* between two devices measured against it, which is
    exactly what resampling needs -- so the alignment is accurate even
    though the reference is an ordinary system clock.

``Resampler``
    Converts a device's stream to a common output rate at an arbitrary,
    slowly-varying ratio, using a windowed-sinc polyphase bank with a
    fractional phase accumulator (the standard asynchronous sample-rate
    conversion structure). Feeding it the tracked true rate removes the
    drift; feeding it the nominal rate would only put both devices on the
    same nominal grid and leave the drift untouched.

Measured resampling error against the exact ideal signal, 51.2 kHz ->
48 kHz with the defaults (32 taps, 4096 phases, neighbouring-kernel
interpolation): -130 dB at 100 Hz, -100 dB at 1 kHz, -93 dB at 5 kHz,
-88 dB at 10 kHz. That is at or below the MCC 172's own -93 dB THD and far
below any microphone tolerance. Cost is roughly 20% of one Pi 4 core for
6 channels.

Requires numpy.
"""
from __future__ import print_function

from time import monotonic

import numpy as np

#: Sinc kernel length. More taps mainly help near the passband edge.
DEFAULT_TAPS = 32
#: Fractional-phase resolution. This is what limits high-frequency accuracy.
DEFAULT_PHASES = 4096
#: Kernel cutoff as a fraction of the lower Nyquist (leaves a guard band).
DEFAULT_ROLLOFF = 0.90


def _build_bank(n_taps, n_phases, ratio, rolloff):
    """Windowed-sinc polyphase bank.

    ``ratio`` is input/output rate; when downsampling (>1) the kernel is
    widened so it band-limits to the *output* Nyquist, preventing aliasing.
    """
    cutoff = rolloff / max(1.0, ratio)
    half = n_taps // 2
    phase = np.arange(n_phases) / float(n_phases)
    tap = np.arange(n_taps) - half + 1
    # Kernel row p is the sinc sampled at (tap - p/n_phases).
    arg = tap[None, :] - phase[:, None]
    kernel = cutoff * np.sinc(cutoff * arg) * np.blackman(n_taps)[None, :]
    kernel /= kernel.sum(axis=1, keepdims=True)     # unity DC gain
    # Append the phase == n_phases row so neighbouring-kernel interpolation
    # has a right-hand neighbour everywhere. A full-sample phase shift is
    # kernel row 0 shifted one tap along; the tap that falls off the end is
    # the windowed edge of the sinc, which is zero by construction.
    wrap = np.zeros((1, n_taps))
    wrap[0, 1:] = kernel[0, :-1]
    return np.vstack((kernel, wrap)), half


class ClockTracker:
    """Estimates a device's true sample rate against the Pi's clock.

    Feed it the frame count of every block as it arrives. It keeps a
    sliding window of (frames, timestamp) points and fits a straight line;
    the slope is the true rate. Timing jitter on individual blocks averages
    out, so the estimate tightens as the window fills.
    """

    #: Accuracy is set by the time span, not the point count -- the slope
    #: error falls as jitter * sqrt(12 / n) / span. Measured with 1 ms of
    #: block-arrival jitter: 30 s -> 6 ppm, 60 s -> 1.8 ppm, 300 s ->
    #: 0.15 ppm, 600 s -> 0.04 ppm. So the window is kept in SECONDS, and
    #: points are thinned so a long window stays cheap.
    def __init__(self, nominal_rate, window_seconds=600.0,
                 min_interval=0.05, min_points=20, settle_seconds=60.0,
                 max_ppm=500.0):
        self.nominal_rate = float(nominal_rate)
        self.measured_rate = float(nominal_rate)
        self._window_s = float(window_seconds)
        self._min_interval = float(min_interval)
        self._min_points = int(min_points)
        self._settle_s = float(settle_seconds)
        self._max_ppm = float(max_ppm)
        self._frames = []           # cumulative frames at each sample point
        self._times = []            # seconds since the first block
        self._total = 0
        self._t0 = None
        self.points = 0

    def update(self, n_frames):
        """Record a block of ``n_frames`` frames that just arrived."""
        now = monotonic()
        if self._t0 is None:
            self._t0 = now
        self._total += int(n_frames)
        t = now - self._t0
        # Thin the series: one point per min_interval is plenty, and it
        # keeps a 10-minute window to a few thousand points.
        if self._times and (t - self._times[-1]) < self._min_interval:
            return self.measured_rate
        self._frames.append(self._total)
        self._times.append(t)
        # Drop anything older than the window.
        cutoff = t - self._window_s
        while len(self._times) > 2 and self._times[0] < cutoff:
            del self._frames[0]
            del self._times[0]
        self.points = len(self._frames)
        if self.points >= self._min_points:
            self._fit()
        return self.measured_rate

    def _fit(self):
        t = np.asarray(self._times)
        f = np.asarray(self._frames, dtype=np.float64)
        span = t[-1] - t[0]
        if span <= 0:
            return
        # Least-squares slope (frames per second).
        t_m = t.mean()
        denom = np.sum((t - t_m) ** 2)
        if denom <= 0:
            return
        slope = float(np.sum((t - t_m) * (f - f.mean())) / denom)
        # Reject nonsense (a stall, a restart, a wildly wrong nominal rate):
        # a real crystal is within a few hundred ppm of nominal.
        ppm = (slope / self.nominal_rate - 1.0) * 1e6
        if abs(ppm) <= self._max_ppm:
            self.measured_rate = slope

    @property
    def ppm(self):
        """Measured offset from the nominal rate, in ppm."""
        return (self.measured_rate / self.nominal_rate - 1.0) * 1e6

    @property
    def elapsed(self):
        return (self._times[-1] - self._times[0]) if self.points > 1 else 0.0

    def settled(self):
        """True once the estimate is worth applying to a resampler.

        Defaults to 60 s of history, where the slope is good to roughly
        2 ppm -- a ~50x reduction of the 100 ppm worst-case device-to-device
        drift. It keeps tightening after that (0.15 ppm by 300 s), and the
        resampler is retuned periodically, so the correction improves as the
        measurement runs.
        """
        return (self.points >= self._min_points and
                self.elapsed >= self._settle_s)

    def state(self):
        return {'nominal_rate': round(self.nominal_rate, 4),
                'measured_rate': round(self.measured_rate, 4),
                'ppm': round(self.ppm, 2),
                'points': self.points,
                'elapsed': round(self.elapsed, 1),
                'settled': self.settled()}


class Resampler:
    """Arbitrary-ratio, stateful resampler for one device's block stream.

    Call :meth:`process` with successive interleaved blocks; the
    concatenated output is a single continuous stream at ``output_rate``
    (there is no discontinuity at block boundaries -- the tail samples and
    the fractional read position are carried over).
    """

    def __init__(self, input_rate, output_rate, num_channels,
                 n_taps=DEFAULT_TAPS, n_phases=DEFAULT_PHASES,
                 rolloff=DEFAULT_ROLLOFF):
        self.input_rate = float(input_rate)
        self.output_rate = float(output_rate)
        self.num_channels = int(num_channels)
        self.n_taps = int(n_taps)
        self.n_phases = int(n_phases)
        self._rolloff = float(rolloff)
        self._bank_ext, self._half = _build_bank(
            self.n_taps, self.n_phases, self.input_rate / self.output_rate,
            self._rolloff)
        self._tail = np.zeros((0, self.num_channels), dtype=np.float64)
        # Read position within (tail + new block), in input samples.
        self._pos = float(self._half - 1)
        self._taps_idx = np.arange(self.n_taps)
        self.frames_in = 0
        self.frames_out = 0

    @property
    def step(self):
        return self.input_rate / self.output_rate

    def set_input_rate(self, input_rate):
        """Retune to a newly measured true input rate.

        The kernel only depends on the ratio, and a few-ppm change does not
        meaningfully alter it, so the bank is rebuilt only when the ratio
        moves enough to matter. The phase accumulator is untouched, so the
        output stays continuous.
        """
        input_rate = float(input_rate)
        if input_rate <= 0:
            return False
        old = self.input_rate
        self.input_rate = input_rate
        if abs(input_rate / old - 1.0) > 1e-3:      # >1000 ppm: rebuild
            self._bank_ext, self._half = _build_bank(
                self.n_taps, self.n_phases, self.step, self._rolloff)
        return True

    def process(self, interleaved):
        """Resample one interleaved block; returns an interleaved block."""
        data = np.asarray(interleaved, dtype=np.float64)
        if data.size:
            data = data.reshape(-1, self.num_channels)
            self.frames_in += data.shape[0]
        else:
            data = np.zeros((0, self.num_channels), dtype=np.float64)
        buf = (data if self._tail.shape[0] == 0
               else np.concatenate((self._tail, data), axis=0))
        n = buf.shape[0]

        # Highest position whose kernel window still fits inside buf.
        limit = n - 1 - self.n_taps + self._half
        if limit < self._pos:
            self._tail = buf
            return np.empty(0, dtype=np.float64)

        count = int(np.floor((limit - self._pos) / self.step)) + 1
        pos = self._pos + self.step * np.arange(count)
        base = np.floor(pos).astype(np.int64)
        # Interpolate between the two neighbouring phase kernels instead of
        # snapping to one. That removes the phase-quantisation staircase --
        # otherwise the dominant high-frequency error, and a source of
        # 1-LSB differences depending on which side of a phase boundary
        # floating-point rounding lands on.
        exact = (pos - base) * self.n_phases
        phase = exact.astype(np.int64)
        alpha = (exact - phase)[:, None]
        idx = (base - self._half + 1)[:, None] + self._taps_idx[None, :]
        weights = (self._bank_ext[phase] * (1.0 - alpha) +
                   self._bank_ext[phase + 1] * alpha)     # (count, n_taps)

        out = np.empty((count, self.num_channels), dtype=np.float64)
        for c in range(self.num_channels):
            out[:, c] = np.einsum('ij,ij->i', buf[idx, c], weights)

        # Keep only what the next output sample still needs.
        next_pos = self._pos + self.step * count
        keep_from = max(0, int(np.floor(next_pos)) - self._half + 1)
        self._tail = buf[keep_from:]
        self._pos = next_pos - keep_from
        self.frames_out += count
        return out.reshape(-1)

    def state(self):
        return {'input_rate': round(self.input_rate, 4),
                'output_rate': self.output_rate,
                'taps': self.n_taps, 'phases': self.n_phases,
                'frames_in': self.frames_in, 'frames_out': self.frames_out}
