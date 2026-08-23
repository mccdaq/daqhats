#!/usr/bin/env python3
#  -*- coding: utf-8 -*-
"""
Fractional-octave (default 1/3-octave) real-time band filter bank for the
MCC 172 PiSLM.

Each band is a Butterworth band-pass filter applied to the full-rate signal
with its IIR state carried across blocks (so filtering is continuous in real
time). The band-limited output is then **decimated per band** -- taken down
to a sample rate just above twice the band's upper edge -- so the transmitted
data volume is a small fraction of streaming every band at full rate.

Bandwidth reality: high fractional-octave bands are wide in absolute Hz, so
they barely decimate. The full 20 Hz - 20 kHz set at 51.2 kHz still sums to
roughly 5x the raw stream for two channels. Lower ``f_max`` (or use one
channel) to fit a Wi-Fi link; the top few bands dominate the total.

Requires numpy and scipy (only when band output is enabled).
"""
from __future__ import print_function

import math

import numpy as np
from scipy import signal


class BandFilterBank:
    """A bank of decimating fractional-octave band-pass filters.

    Args:
        fs (float): input sample rate (Hz per channel).
        channels (list[int]): channel numbers being scanned (data order).
        f_min, f_max (float): requested band-center frequency range (Hz).
        fraction (int): octave fraction; 3 -> 1/3-octave bands.
        order (int): Butterworth order passed to scipy.signal.butter. Note a
            band-pass design yields a 2*order system, so order=3 (the
            default) gives a 6th-order band-pass (3 biquads). This is a real
            tradeoff between two failure modes, not a free lunch:
              - ringing on impulsive/transient content (worse at higher
                order -- order=6 rings ~2x as long as order=3 on the
                lowest-frequency 1/3-octave band);
              - adjacent-band rejection (worse at lower order -- order=3
                only rejects a pure tone one band away by ~18 dB, vs ~35 dB
                at order=6, so tonal/resonant energy visibly leaks into
                neighboring bands and reads them higher than expected).
            There is no order that minimizes both. Prefer higher order if
            accurate per-band spectral separation matters more than
            minimizing ringing (e.g. ISO 3382 / floor impact analysis).
        margin (float): keep the decimated rate >= 2 * upper_edge * margin.
    """

    #: Reference frequency for band centers (IEC 61260 uses 1 kHz).
    F_REF = 1000.0

    def __init__(self, fs, channels, f_min=20.0, f_max=20000.0,
                 fraction=3, order=3, margin=1.0):
        self.fs = float(fs)
        self.channels = list(channels)
        self.fraction = int(fraction)
        self.order = int(order)
        self.margin = float(margin)

        nyquist = self.fs / 2.0
        k_min = int(round(fraction * math.log2(f_min / self.F_REF)))
        k_max = int(round(fraction * math.log2(f_max / self.F_REF)))

        self.bands = []          # list of per-band dicts (metadata + sos + D)
        edge = 2.0 ** (1.0 / (2.0 * fraction))
        for k in range(k_min, k_max + 1):
            center = self.F_REF * 2.0 ** (float(k) / fraction)
            f_lo = center / edge
            f_hi = center * edge
            if f_lo <= 0.0 or f_lo >= nyquist:
                continue
            # Clamp the top band's upper edge below Nyquist.
            f_hi_eff = min(f_hi, nyquist * 0.999)
            sos = signal.butter(self.order, [f_lo, f_hi_eff],
                                btype='band', fs=self.fs, output='sos')
            decimation = max(1, int(math.floor(
                self.fs / (2.0 * f_hi_eff * self.margin))))
            self.bands.append({
                'index': len(self.bands),
                'center': center,
                'f_lo': f_lo,
                'f_hi': f_hi,
                'decimation': decimation,
                'decimated_rate': self.fs / decimation,
                'sos': sos,
            })

        n_sections = self.bands[0]['sos'].shape[0] if self.bands else 0
        n_bands = len(self.bands)
        n_chan = len(self.channels)
        # Per band, per channel: IIR state and decimation phase.
        self._zi = [[np.zeros((n_sections, 2)) for _ in range(n_chan)]
                    for _ in range(n_bands)]
        self._phase = [[0 for _ in range(n_chan)] for _ in range(n_bands)]

    def metadata(self):
        """Band table for the handshake (no filter coefficients)."""
        return {
            'fraction': self.fraction,
            'order': self.order,
            'input_rate': self.fs,
            'channels': list(self.channels),
            'bands': [{'index': b['index'],
                       'center': round(b['center'], 3),
                       'f_lo': round(b['f_lo'], 3),
                       'f_hi': round(b['f_hi'], 3),
                       'decimation': b['decimation'],
                       'decimated_rate': round(b['decimated_rate'], 4)}
                      for b in self.bands],
        }

    def process(self, interleaved):
        """Filter + decimate one raw block.

        Args:
            interleaved (sequence[float]): the raw scan block, interleaved
                channel-fastest exactly as the MCC 172 returns it.

        Yields:
            (band_index, channel, numpy.ndarray): decimated float64 samples
            produced for each band/channel this block. Empty results are
            skipped, so low-frequency bands emit only occasionally.
        """
        n_chan = len(self.channels)
        data = np.asarray(interleaved, dtype=np.float64).reshape(-1, n_chan)
        for item in self.process_2d(data):
            yield item

    def process_2d(self, data):
        """Filter + decimate a block already shaped (frames, channels).

        This is the zero-copy entry point: callers that already hold a 2-D
        view (e.g. a shared-memory slot) avoid the reshape in
        :meth:`process`. Same yields as :meth:`process`.
        """
        n_chan = len(self.channels)
        block_len = data.shape[0]
        if block_len == 0:
            return

        for band in self.bands:
            b = band['index']
            sos = band['sos']
            decim = band['decimation']
            for ci in range(n_chan):
                x = data[:, ci]
                y, self._zi[b][ci] = signal.sosfilt(
                    sos, x, zi=self._zi[b][ci])

                start = self._phase[b][ci]
                if start < block_len:
                    idx = np.arange(start, block_len, decim)
                    out = y[idx]
                    # Phase of the next output sample in the following block.
                    self._phase[b][ci] = idx[-1] + decim - block_len
                    if out.size:
                        yield b, self.channels[ci], out
                else:
                    self._phase[b][ci] = start - block_len

    def skip(self, n_frames):
        """Account for n_frames of INPUT samples that were never fed to
        process_2d() (e.g. dropped by DSP-pool backpressure -- see
        dsp_pool.py): advance each band/channel's decimation phase exactly
        as process_2d() would have, and reset that band/channel's filter
        state so the next process_2d() call starts clean instead of
        splicing pre-gap and post-gap samples together through a stale
        state (which would show up as a spurious transient/spike, most
        visible in the narrowest/highest-Q bands).

        Yields (band_index, channel, n_skipped) -- the number of decimated
        OUTPUT samples that would have been produced for that band/channel,
        for start_index gap accounting. Only skips with a nonzero count are
        yielded (matching process_2d's "empty results are skipped").
        """
        if n_frames <= 0:
            return
        n_chan = len(self.channels)
        for band in self.bands:
            b = band['index']
            decim = band['decimation']
            for ci in range(n_chan):
                start = self._phase[b][ci]
                if start < n_frames:
                    idx = np.arange(start, n_frames, decim)
                    self._phase[b][ci] = idx[-1] + decim - n_frames
                    n_skipped = idx.size
                else:
                    self._phase[b][ci] = start - n_frames
                    n_skipped = 0
                self._zi[b][ci][:] = 0.0
                if n_skipped:
                    yield b, self.channels[ci], n_skipped
