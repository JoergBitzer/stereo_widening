#!/usr/bin/env bash
# Copies the test samples listed in test_signals.md into test_signals/samples/.
# The samples are licensed material and are NOT part of the repository.
# Usage: ./copy_test_samples.sh [SAMPLE_ROOT]   (default: ~/Music/samples)

set -euo pipefail

SRC_ROOT="${1:-$HOME/Music/samples}"
DST_DIR="$(cd "$(dirname "$0")" && pwd)/test_signals/samples"
mkdir -p "$DST_DIR"

# target name | source path relative to SRC_ROOT
FILES=(
  "speech_dry_answers.wav|Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Dry/FL_AIV_Vocal_Spoken_Female_Answers_Dry.wav"
  "speech_dry_participant.wav|Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Dry/FL_AIV_Vocal_Spoken_Female_Participant_Dry.wav"
  "speech_wet_answers.wav|Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Wet/FL_AIV_Vocal_Spoken_Female_Answers_Wet.wav"
  "speech_wet_participant.wav|Function Loops - AI - Speech Vocal Hooks/FL_AIV_Spoken_Vocals_Wet/FL_AIV_Vocal_Spoken_Female_Participant_Wet.wav"
  "vocal_phrase_always.wav|UNDRGRND Sounds - Ultimate Drum-Hits Bundle/UNDRGRND Sounds - Ultimate Drum-Hits Bundle/UNDRGRND Sounds - Lo-Fi Soul - Wav/US_LFS_Vocal_Phrases/US_LFS_Vocal_always4_C#m.wav"
  "vocal_phrase_holdme.wav|UNDRGRND Sounds - Ultimate Drum-Hits Bundle/UNDRGRND Sounds - Ultimate Drum-Hits Bundle/UNDRGRND Sounds - Lo-Fi Soul - Wav/US_LFS_Vocal_Phrases/US_LFS_Vocal_holdme1_Bbm.wav"
  "drums_loop_01.wav|musicradar_sub/musicradar-realworld-drum-samples/Drum loops/FS_Drumloop_01a(130BPM).wav"
  "drums_loop_02.wav|musicradar_sub/musicradar-realworld-drum-samples/Drum loops/FS_Drumloop_02a(125BPM).wav"
  "bass_loop.wav|musicradar_sub/musicradar-pop-samples/Loops 100bpm/Bass/PO_DualBass100A-01.wav"
  "synth_loop.wav|musicradar_sub/musicradar-pop-samples/Loops 100bpm/Synth/PO_Chepster100A-01.wav"
  "mix_loop_carry_on.wav|musicradar_sub/musicradar-electronic-pop-samples/Full loops/Carry On - 1.wav"
  "mix_loop_let_it_be.wav|musicradar_sub/musicradar-electronic-pop-samples/Full loops/Let It Be - 1.wav"
)

missing=0
for entry in "${FILES[@]}"; do
  target="${entry%%|*}"
  source="$SRC_ROOT/${entry#*|}"
  if [[ -f "$source" ]]; then
    cp "$source" "$DST_DIR/$target"
    echo "copied   $target"
  else
    echo "MISSING  $source" >&2
    missing=$((missing + 1))
  fi
done

echo "done: $(( ${#FILES[@]} - missing )) copied, $missing missing -> $DST_DIR"
[[ $missing -eq 0 ]]
