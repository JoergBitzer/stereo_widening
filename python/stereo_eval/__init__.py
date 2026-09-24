"""stereo_eval: test signals and objective measures for stereo processing.

Modules
  audio_io   read/write wav files as (N, 2) arrays
  signals    building blocks for test signals (noise, sweep, panning, reverb)
  measures   correlation, levels, loudness, mono-sum colouration
  binaural   loudspeaker playback model (spherical head) and IACC
  report     all measures in one call, text and plot output
"""

from . import audio_io, binaural, measures, report, signals  # noqa: F401
