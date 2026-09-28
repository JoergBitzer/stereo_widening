# Save button turns red on changes (v1.0.3)

The preset bar's Save button is meant to turn red when a parameter of the loaded
preset changes (as the manual says). It never did: `PresetComponent::
setSomethingChanged()` existed, but nothing in StereoWidener called it. The other
plugins wire every slider's `onValueChange` to it via a `somethingChanged` lambda.

## Fix

Instead of wiring every knob, display, button and combo box, the editor listens to
all parameters (`juce::AudioProcessorParameter::Listener`) and reacts to the start of
a **gesture**. Gestures come only from user actions -- knob and display drags, typed
values, double-click resets, buttons, combo boxes (all through JUCE attachments or
`ParameterAttachment`) -- never from loading a preset or from host automation. So a
freshly loaded preset keeps a grey Save button, and automation does not mark it as
changed. The call is forwarded to the message thread (`MessageManager::callAsync`
with a `SafePointer`). Saving or loading a preset resets the button, as before.

## Verification

Throwaway test (Debug, JUCE 9): load a preset, change a parameter without a gesture
(like automation), then with a gesture (like a knob drag):

![Save button: after loading, after automation, after a user change](img/save_button_test.png)

Pixel check of the button: grey (128,128,128), grey, red (255,26,26).
pluginval --strictness-level 10 (Release): 2/2 SUCCESS.
