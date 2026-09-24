# Test Signals

The folder `test_signals/` is **not** in the repository. The samples are licensed
material, and the generated signals can be rebuilt at any time.

```
test_signals/
├── samples/     # copied with ./copy_test_samples.sh
└── generated/   # written by python/generate_test_signals.py (Phase 1)
```

## Copied samples

Recreate them with `./copy_test_samples.sh [SAMPLE_ROOT]` (default `~/Music/samples`).
All files are 44.1 kHz and 24 bit. M and S are the RMS levels of `(L+R)/2` and `(L−R)/2`.

| File | Ch | Length | M (dB) | S (dB) | Character / use | Source (relative to `~/Music/samples`) |
|------|----|--------|--------|--------|-----------------|----------------------------------------|
| speech_dry_answers.wav | 2 | 6.9 s | −21.6 | −81.3 | dry speech, dual mono → pseudo-stereo test | Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Dry/FL_AIV_Vocal_Spoken_Female_Answers_Dry.wav |
| speech_dry_participant.wav | 2 | 9.1 s | −20.6 | −85.5 | dry speech, dual mono | Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Dry/FL_AIV_Vocal_Spoken_Female_Participant_Dry.wav |
| speech_wet_answers.wav | 2 | 9.1 s | −17.3 | −41.0 | same phrase with stereo reverb | Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Wet/FL_AIV_Vocal_Spoken_Female_Answers_Wet.wav |
| speech_wet_participant.wav | 2 | 9.1 s | −16.2 | −40.0 | same phrase with stereo reverb | Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Wet/FL_AIV_Vocal_Spoken_Female_Participant_Wet.wav |
| vocal_phrase_always.wav | 2 | 5.7 s | −15.3 | −33.5 | sung phrase, slightly stereo | UNDRGRND Sounds - Ultimate Drum-Hits Bundle/UNDRGRND Sounds - Ultimate Drum-Hits Bundle/UNDRGRND Sounds - Lo-Fi Soul - Wav/US_LFS_Vocal_Phrases/US_LFS_Vocal_always4_C#m.wav |
| vocal_phrase_holdme.wav | 2 | 5.6 s | −15.6 | −37.0 | sung phrase, slightly stereo | UNDRGRND Sounds - …/US_LFS_Vocal_Phrases/US_LFS_Vocal_holdme1_Bbm.wav |
| drums_loop_01.wav | 2 | 3.7 s | −16.8 | −80.8 | drums, dual mono → transient test | musicradar_sub/musicradar-realworld-drum-samples/Drum loops/FS_Drumloop_01a(130BPM).wav |
| drums_loop_02.wav | 2 | 3.8 s | −22.8 | −80.8 | drums, dual mono | musicradar_sub/musicradar-realworld-drum-samples/Drum loops/FS_Drumloop_02a(125BPM).wav |
| bass_loop.wav | 1 | 4.8 s | – | – | mono bass → bass-mono / low-frequency test | musicradar_sub/musicradar-pop-samples/Loops 100bpm/Bass/PO_DualBass100A-01.wav |
| synth_loop.wav | 2 | 4.8 s | −14.7 | −31.0 | stereo synth | musicradar_sub/musicradar-pop-samples/Loops 100bpm/Synth/PO_Chepster100A-01.wav |
| mix_loop_carry_on.wav | 2 | 9.7 s | −11.5 | −34.9 | full mix loop, moderately wide | musicradar_sub/musicradar-electronic-pop-samples/Full loops/Carry On - 1.wav |
| mix_loop_let_it_be.wav | 2 | 8.6 s | −9.3 | −26.2 | full mix loop, wide | musicradar_sub/musicradar-electronic-pop-samples/Full loops/Let It Be - 1.wav |

Notes:
- The dry speech and the drum loops are dual mono (S is about 60 dB below M). They are
  the test material for the pseudo-stereo algorithms (2.3, 2.4, 2.5).
- The wet speech has the same phrases as the dry speech, so it can be used to compare
  "widening by algorithm" with "width from real reverb".

## Generated signals (Phase 1)

Written by `python/generate_test_signals.py` with a fixed seed. Planned: mono pink noise,
independent pink noise L/R, polarity-inverted noise, log sweep, panned sources, mono
speech with a synthetic stereo reverb, and a small mix built from the samples above.
