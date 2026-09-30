# StereoWidener: ReadMe

## Check, if everything is OK
Since you read this file, you unzipped the downloaded file.

The directory should contain:
1. This ReadMeFirst.txt file (in MarkDown format, you can rename it in ReadMeFirst.md for a better view)
2. ManualStereoWidener.pdf, the manual
3. LICENSE and LICENSE-AGPL-3.0.txt, the licenses
4. a directory called StereoWidener.vst3 (the VST3 plugin)
5. the Standalone application (Windows: StereoWidener.exe, macOS: StereoWidener.app, Linux: StereoWidener)
6. (macOS only) StereoWidener.component (the AU plugin)

## Installation

### Windows
Copy the directory StereoWidener.vst3 to C:\Program Files\Common Files\VST3

### Mac VST3
Copy the directory StereoWidener.vst3 to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/VST3

### Mac AU
Copy StereoWidener.component to
/Users/yourUSERNAME/Library/Audio/Plug-Ins/Components

The macOS version is not signed with an Apple Developer ID yet. If macOS reports that the
plugin is damaged or cannot be opened, remove the quarantine flag in the Terminal, e.g.
xattr -cr ~/Library/Audio/Plug-Ins/VST3/StereoWidener.vst3

### Linux
Copy the directory StereoWidener.vst3 to
/home/yourUSERNAME/.vst3/

Done! Start your DAW and let it rescan the plugins.
The Standalone application runs without a DAW: audio input -> StereoWidener -> audio output.

At the first start, the 20 factory presets are copied to your preset folder (plain XML files):
- Windows: C:\Users\yourUSERNAME\AppData\Roaming\Jade_Hochschule\StereoWidener
- macOS:   /Users/yourUSERNAME/Library/Audio/Presets/Jade_Hochschule/StereoWidener
- Linux:   /home/yourUSERNAME/.config/Jade_Hochschule/StereoWidener

Have fun and read the manual for instructions and further information.

J. Bitzer aka audio-dsp


# Source code and license

## Download source code
You can download the source code at GitHub:

https://github.com/JoergBitzer/stereo_widening

## License
- The source code is open source under the MIT License (see LICENSE),
  (c) Joerg Bitzer, Jade Hochschule.
- The plugin binaries contain third-party code: JUCE (used under the AGPLv3), the VST3 SDK
  by Steinberg (MIT License) and, on macOS, the Audio Unit SDK by Apple (Apache License 2.0).
  Therefore the binaries as a whole are distributed under the GNU Affero General Public
  License v3 (see LICENSE-AGPL-3.0.txt). The complete source code is the repository above
  plus JUCE.
- The manual is licensed under CC-BY 4.0.

VST is a registered trademark of Steinberg Media Technologies GmbH.

The plugin comes without any warranty (see the licenses).
