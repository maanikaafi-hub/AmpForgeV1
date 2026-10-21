# Third party notices

AmpForge itself is licensed under the GNU GPL v3 (see `LICENSE`). It is built
from third party code with the following licences. Nothing here requires a paid
SDK or a signed agreement.

## JUCE 8.0.15 (pinned)

* Repository: <https://github.com/juce-framework/JUCE>
* Pinned commit: `91ad83ae34a81e0833b1a2b0866f54846370ae53` (tag `8.0.15`)
* Licence: dual — the **JUCE 8 End User Licence Agreement** (free Starter tier)
  **or** the **AGPLv3**. Which one applies to you depends on how you distribute
  the plug-in; read <https://juce.com/legal/juce-8-licence/> before shipping a
  binary. This repository is distributed under the GPL v3, which is compatible
  with JUCE's AGPLv3 option (and with the Starter tier, which imposes
  additional branding/reporting conditions).
* JUCE is fetched at build time; it is not vendored here.

## VST 3 SDK 3.8.0 (bundled inside JUCE)

* Location in a JUCE checkout: `modules/juce_audio_processors_headless/format_types/VST3_SDK`
* Licence: **MIT** (Steinberg relicensed the VST 3 SDK under MIT with the 3.8.0
  release). No agreement to sign, no fees, no membership.
* The licence text ships with JUCE as `VST3_SDK/LICENSE.txt`.
* "VST" is a trademark of Steinberg Media Technologies GmbH. Using the SDK does
  not grant the right to use the trademark; this project only uses the name to
  describe the plug-in format it supports.

## Embedded third party code inside JUCE

JUCE bundles further libraries that end up linked into the plug-in, among them
zlib, libpng, libjpeg, libflac, HarfBuzz,Sheen Bidi and (on Windows) the ASIO
SDK. Their licences are recorded by JUCE in the corresponding module
directories. All of them are permissive (zlib/BSD/MIT style) or, for the ASIO
SDK, GPLv3.

## Algorithmic credits

* The reverb uses a **Schroeder / Freeverb style** topology (parallel damped
  comb filters followed by all-pass filters). That topology was published by
  Manfred R. Schroeder in 1962 and the well known Freeverb implementation is in
  the public domain; the code here was written from the published structure and
  is not a copy of any existing source file.
* The biquad (RBJ "Audio EQ Cookbook") and the remaining DSP are written from
  standard filter-design formulas.
* The cabinet impulse responses are **generated procedurally** by
  `CabinetSim::generateProfile()` — they are synthetic models, not recordings of
  any real cabinet, and no third party IR files are distributed here.
