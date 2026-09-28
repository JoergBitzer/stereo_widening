# Chorus Doubler: name without "Dimension D" (v0.1.30)

"Dimension D" is the product name of Roland's SDD-320 (and its Boss/Roland plugin
re-issues); commercial emulations avoid it (e.g. Audiority's "Spatial D320"). To stay
clear of any trademark question, the algorithm is now just **Chorus Doubler** in
everything the plugin shows or ships: the algorithm selector, the help panel title
and text, the README and the source comments of `ChorusDoubler.h/.cpp`. The factory
preset idea "Guitar Clean - Dimension" became "Guitar Clean - Chorus Wide".

Saved sessions and presets are unaffected: the algorithm parameter stores the index,
not the name. DSP unchanged. pluginval --strictness-level 10: SUCCESS, zero
assertions.

The older phase documents, planing.md and the Python reference still mention
"Dimension D" as a description of the effect family (historical record, not a
product name).
