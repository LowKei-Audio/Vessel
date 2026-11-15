# VESSEL

**Pour any sound into a resonant body. It rings back in your key.**

Vessel is a resonant-body effect from LowKei Audio. Whatever you feed it (drums, vocals, pads, a full mix) excites a physical vessel made of glass, crystal, bronze, iron, wood, clay, a tube or a drum skin. The vessel sings back at its own pitches, which you can tune to a note or a whole chord so it always sits in the track. Pour water into it to bend its pitch, crack it, let the water slosh, send echoes back into it, and put it in a room. Two performance buttons, **SEAL** and **SHATTER**, are built for drops and transitions.

- **Formats:** VST3 and CLAP (Windows), plus a Windows standalone app. AU is added automatically if built on a Mac.
- **Hosts:** Serato Studio, FL Studio and Ableton Live load the VST3. FL Studio, Bitwig Studio and Reaper also load the CLAP.
- **Views:** **Easy** (play every preset in seconds) and **Pro** (every stage, the modulation matrix and global settings).
- **Presets:** 51 factory presets in 11 categories, each with its four macros assigned.
- **Version:** 1.0.2

**1.0.2** removes the clicks and static heard in Serato Studio when you switch presets, use Pour Me or A/B, or load a project while audio is playing, and when a loop restarts. A sound change now fades briefly to your dry signal, swaps every setting at once and fades the new sound in, all within about 50 ms. Synced movement eases onto the new position when a loop wraps instead of jumping. It also removes clicks when Liquid, Echo, Echo Ping-Pong, Pour Low Cut or Bass Mono are switched while playing. Once a preset settles it sounds the same as before, and your projects and presets are unchanged.

**1.0.1** improves stability when your DAW saves or loads a project while Vessel is open. Some hosts do this in the background, and in rare cases it could crash the plug-in. Your projects and presets are unchanged.

---

## Install (Windows)

1. Download **Vessel-1.0.2-Windows-Setup.exe** from the repository's **Releases** page.
2. Run it and choose **VST3 and CLAP plug-ins and standalone app** or **Plug-ins only**. The installer places the VST3 in `C:\Program Files\Common Files\VST3` and the CLAP in `C:\Program Files\Common Files\CLAP`, the standard folders every supported DAW scans, and adds Start menu shortcuts for the app and this guide.
3. Rescan in your DAW:

| DAW | Steps |
|---|---|
| **Serato Studio** | Setup (cog icon) > **Plugins**. Enable **Default VST Folder**, then click **Scan**. Turn on **Show FX Panel** for a deck or track, choose **Plugins > All Effects** in the Library panel, and drag Vessel onto an FX slot. |
| **FL Studio** | Options > File settings > **Manage plugins**. Click **Find installed plugins** with **Verify plugins** ticked. Vessel appears under Plugin database > Installed > Effects > New. Versions of FL Studio with CLAP support list both a VST3 and a CLAP entry; either works, so use one per project. |
| **Ableton Live** | Options > Preferences > **Plug-Ins**. Turn **Use VST3 Plug-In System Folders** on, then click **Rescan** (hold **Alt** while clicking for a full rescan). Vessel appears under **Plug-Ins** in the browser. |

**Uninstall:** Windows Settings > Apps > Installed apps > **Vessel** > Uninstall. This removes the plug-ins and the app. Your saved presets in `Documents\Vessel\Presets` are kept.

**Manual install (no installer):** download **Vessel-Windows-x64** from the latest green **Build** run on the **Actions** tab, then copy the whole **`Vessel.vst3`** folder (not just the file inside it) to `C:\Program Files\Common Files\VST3`, and **`Vessel.clap`** to `C:\Program Files\Common Files\CLAP` (create the folder if it does not exist).

**Standalone app:** open **Vessel** from the Start menu. Click **Options > Audio/MIDI Settings** to pick your audio interface and input. With no host, synced effects follow the **Free Tempo** setting on the GLOBAL page.

No Visual C++ Redistributable is needed; the runtime is built in.

---

## How it works

```
Input -> POUR -> [FRACTURE pre] -> BODY -> [FRACTURE post] -> LIQUID -> ECHO -> SPACE -> Output
                                    ^                                     |
                                    +----------- Echo Return -------------+
```

The **Body** is the heart: up to 16 resonant modes, like the partials of a struck glass or bell. The **Material** decides where those modes sit and how they ring. **Tuning** decides the pitch: a free frequency, a note, or a chord. The body follows the level of what you feed it, so turning up the ring time never makes it jump out of the mix.

---

## Easy view: play it in ten seconds

| Control | What it does |
|---|---|
| **The vessel** | The animated display in the middle. **Drag up or down to pour water in or out** (Fill): the pitch drops like a filled glass and the highs soften. Scroll for fine steps, double-click to empty it. Rings radiate as the modes sing; the strip at the bottom shows every mode at its pitch. The water sloshes with Liquid, cracks spread with Fracture. |
| **Material** | Eight one-click tiles: **Glass** (wine glass), **Crystal** (singing bowl with slowly beating pairs), **Bronze** (church bell: hum, prime, minor-third tierce, quint, nominal), **Iron** (cast-iron plate, clangy), **Wood** (warm box, short), **Clay** (round, earthy jar), **Tube** (hollow pipe, odd harmonics), **Skin** (drum membrane, short and boomy). |
| **Tuning** | **Free**: any pitch in Hz (the Pitch knob). **Note**: tuned to Root and Octave. **Chord**: the modes are re-tuned to a chord stacked up the octaves (Octaves, Fifths, Major, Minor, Sus2, Sus4, Maj7, Min7, Dom7, Min9, Add9, Dim). Set the Root to your track's key and the vessel rings in key. |
| **Excite** | What rings the body. **Input**: everything. **Hits**: only transients (drums ping it, sustained sounds pass through). **Noise**: noise shaped by your sound, like a bowed rim (great on vocals and pads). |
| **Pitch / Decay / Body Mix / Spread** | Free pitch (Free tuning only), ring time, blend of the dry sound and the body, and how far apart the left and right sides are tuned (width and slow beating). |
| **SEAL** | Seals the vessel: whatever is ringing keeps ringing, nothing new gets in. Echo and Space keep running (turn **Seal Space Too** on in GLOBAL to freeze the room as well). |
| **SHATTER** | Ramps the vessel into pieces: cracks, grit and folding come in, the modes warp and drop, echoes get thrown. Let go and it reassembles quickly. **Shatter Time** and **Depth** are on the GLOBAL page. |
| **HOLD / LATCH** | Under each button. **HOLD**: only while pressed. **LATCH**: click on, click off. Right-click a button for the same choice. Both buttons are parameters, so you can automate them from your DAW for repeatable drops. |
| **Macros** | **Resonance** (body level, ring time, width), **Motion** (slosh, ripple, water speed), **Fracture** (cracks, grit, folding; drive and warp when Fracture is off), **Space** (echoes, room, width). Every preset assigns all four. The line under each macro shows how many targets it moves. |
| **Stages** | One-click power for Pour, Body, Fracture, Liquid, Echo and Space. |
| **Mix / Output** | Overall blend of dry and processed sound; final level. |
| **Dice** (top bar) | **Pour Me**: a random but musical vessel, always tuned, always safe. |

The bar along the bottom explains whatever is under your mouse. With the mouse over empty space, it shows what the current preset does.

---

## Pro view: every stage

Click **PRO** in the top bar. The vessel, SEAL, SHATTER and the macros stay on the right; the left side has four pages.

### VESSEL page

**Pour** conditions what goes into the vessel:

| Control | What it does |
|---|---|
| Drive | Warm saturation before the body, auto-levelled so it adds colour, not volume. |
| Strike | Transient shaper. Positive sharpens attacks so hits ring the body harder; negative softens them. |
| Tilt | Dark to bright, before the body. |
| Low Cut | Removes lows before the body so bass does not boom the low modes (20 Hz = off). |

**Body**:

| Control | What it does |
|---|---|
| Material / Tuning / Root / Octave / Chord / Excite | As in the Easy view. Root, Octave and Chord are active only in the tunings that use them. |
| Pitch | Fundamental in Free tuning (30 Hz to 2 kHz). |
| Fine | Fine tuning in cents, for tracks not tuned to A = 440 Hz. |
| Decay | How long the vessel rings (0.05 to 20 s; each material scales it). |
| Damping | How much faster the high modes die than the low ones. |
| Bright | Balance of low and high modes. |
| Warp | Squeezes (-) or stretches (+) the spacing of the modes: from hollow to clangorous. |
| Spread | Left and right tuned slightly apart: width and slow beating. |
| Modes | How many modes ring (4 to 16): fewer is purer, more is richer. |
| Glide | Pitch glide when the tuning changes (automate the Root for melodic glides). |
| Fill | Water level (the same as dragging the vessel). |
| Body Mix | Blend of the poured sound and the body. |

### FX page

**Fracture**: what happens when the vessel cracks.

| Control | What it does |
|---|---|
| Position | **Post**: cracks the ringing vessel. **Pre**: cracks the sound before it rings the vessel. |
| Crack | Glassy cracks triggered by the sound itself. Above 50%, pieces break away (short dropouts). Silent when the input is silent. |
| Grit | Bit and sample-rate reduction on one knob. |
| Fold | Wavefolding: glassy, then harsh. Quiet signals keep their level. |
| Shard | Ring modulation tuned to an inharmonic partner of the vessel's pitch: metallic shards that stay related to the tuning. |
| Tone / Mix | Dark to bright after the damage; blend of whole and fractured. |

**Liquid**: the water moving inside.

| Control | What it does |
|---|---|
| Sync / Rate | Speed of the water, free in Hz or locked to the host tempo. |
| Slosh | Every mode of the body wobbles in pitch on its own phase. |
| Ripple | A liquid chorus, its sweep shaped like a wave. |
| Turbulence | Smooth waves become random currents (bubbles in the display). |
| Mix | Blend of still and rippling sound. |

**Echo**: tape-flavoured echoes.

| Control | What it does |
|---|---|
| Sync / Time | Echo time, free (10 ms to 2 s) or a note value. |
| Feedback / Tone / Wobble | Repeats, darkening per repeat, tape wow. |
| Return | Sends the echoes **back into the vessel**, so every repeat rings it again. The return is automatically limited so echoes plus return can never run away. |
| Ping-Pong / Mix | Repeats bounce left and right; level of the echoes. |

**Space**: the room around the vessel. Mode (**Hall**, **Plate**, **Reverse** for sucked-in swells, **Gated**), Size, Decay (0.2 to 20 s), Pre-Delay, Damping, Low Cut, **Shimmer** (an octave-up halo rising out of the tail), Movement and Mix.

### MOD page

| Part | What it does |
|---|---|
| **LFO 1 / LFO 2** | Sine, Triangle, Saw Up, Saw Down, Square, S&H and Smooth Random. Free in Hz or synced. Random shapes are seeded per cycle, so a synced pattern repeats identically on every pass. |
| **Envelope** | Follows the input level: Attack, Release and Sensitivity. |
| **Drift** | Random movement, from stepped (0% Smooth) to gliding (100%). |
| **Matrix (16 slots)** | **Source** to **Destination** with a bipolar **Amount**. Sources: LFO 1, LFO 2, Envelope, Drift and the four macros. Destinations are grouped by stage; LFO and Drift rates are destinations too (modulate the modulators). **CLEAR ALL** empties the matrix, macros included. |

**Fastest way to modulate:** right-click any knob and choose **Modulate with** (LFO 1, LFO 2, Envelope, Drift) or **Assign to macro**. A thin outer ring on a knob shows its modulation range; a white dot shows where modulation and Shatter have actually moved it.

### GLOBAL page

| Section | Controls |
|---|---|
| **Levels & Output** | Input, Output, Mix, Width (0 to 200%), Bass Mono (keeps lows mono for club systems), **Delta** (hear only what Vessel adds) and the **Safety Limiter** (-0.3 dBFS ceiling, on by default). |
| **Performance** | Shatter **Ramp** time (20 ms to 8 s), Shatter **Depth**, and **Seal Space Too**. |
| **Tempo & Random** | Free Tempo (used when the host sends none, such as in the standalone app) and Seed (same seed = same cracks, currents and random patterns every time). |

---

## Tips for DJs (Serato Studio)

- **Stay in key:** set Tuning to **Chord**, the **Root** to the track's key and the chord to its mode (Minor for minor keys). Try **Key Wash** and **Minor Choir**.
- **Drop trick:** load **Seal Drop**, set SEAL to **LATCH**, hit it on the last beat before the drop, and release on the one.
- **Build-ups:** **Shatter Riser** breaks the vessel apart over 8 seconds while you hold SHATTER; let go on the drop.
- **Drums in key:** **Snare Ring**, **Tuned Kick** and **Marimba Body** use Hits excitation so only the hits ring.

---

## Building from source

Requirements: CMake 3.22+, plus Visual Studio 2022 or newer (Windows), Xcode (macOS), or GCC/Clang (Linux). JUCE 9.0.3, clap-juce-extensions and Catch2 are downloaded automatically.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Vessel_VST3 Vessel_CLAP Vessel_Standalone
```

The plugins are written to `build/src/Vessel_artefacts/Release/VST3/` and `.../Release/CLAP/`, and the app to `.../Release/Standalone/`. Add `-DVESSEL_BUILD_CLAP=OFF` to skip CLAP. Builds never copy into system folders on their own.

To build the installer locally, install [Inno Setup 6](https://jrsoftware.org/isinfo.php), then from the repository root:

```
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" /DAppVersion=1.0.2 /DBuildDir=..\build installer\Vessel.iss
```

The installer is written to `installer\Output\`. To publish a release, either push a tag such as `v1.0.2` (it must match the version in `CMakeLists.txt`), or push the commit to the `release` branch: CI builds and verifies it, then creates the `v<version>` tag and the GitHub Release itself (it refuses a version that is already released).

### Verification

The **Build** workflow runs on pushes to `dev` and `release`, on version tags and on demand (not on `main`, to save Actions minutes: `main` only receives commits that already passed). A run only passes if all of the following pass:

| Check | What it proves |
|---|---|
| DSP tests (`vessel_tests`) | Note and Chord tuning land on the right frequencies; white noise comes out peaked at the tuned modes; Fill lowers the vessel by the right amount; Seal holds the ring while an open vessel dies away; every material and tuning stays finite and bounded at extreme settings; Pour drive keeps its level; Fracture, Echo and Liquid stay bounded; all stages off is bit-exact; switching stages never clicks; the Echo Return loop at maximum always decays; the limiter holds its ceiling; CPU with every stage on (about 3% of one core measured). Static regression tests (`StaticTests.cpp`): one per cause of the 1.0.1 clicks, each failing on 1.0.1. |
| State checks (`vessel_check state`) | Save/restore of parameters and the matrix (byte-identical on reload); Shatter and macros reaching the engines; Seal holding the ring in the real processor; undo; 25 random vessels all safe; preset data integrity. |
| Static check (`vessel_check static`) | No clicks or static, measured on the real plug-in over a held tone: every factory preset, six Pour Me patches, A/B and a project load switched while playing (never sharper than either sound on its own); every preset through a loop wrap and a restart over sound; SEAL and SHATTER pressed and released on every preset; a sound change complete within 64 ms (50.7 ms measured). Vessel 1.0.1 fails it (up to 148x on preset switches, 300x on a loop). |
| Preset check (`vessel_check presets`) | Every factory preset as saved, with Shatter engaged and with all four macros at 100%: finite, under the ceiling, within a sane level of dry, with decaying tails. |
| pluginval, strictness 10 | State save/restore, automation, thread safety, parameter fuzzing and bus layouts. |
| Steinberg VST3 validator | Official VST3 conformance. |
| clap-validator | Official CLAP conformance. |

Each check posts its headline result as a note on the run page. The run also uploads Windows-rendered screenshots of every page and the pluginval logs.

### Project layout

```
src/dsp/            Signal processing, plain C++ (no framework), fully unit-tested
                    Materials, Body (modal resonators), Pour, Fracture, Liquid,
                    Echo, Space, the Chain, mod sources and the output stage
src/presets/        51 factory presets, macro kit, preset manager and randomizer
src/ui/             Look and feel, controls, the animated vessel, Easy / Pro views
src/Parameters*     Every parameter defined once in a table
src/Plugin*.cpp     JUCE processor (control blocks, matrix, Shatter) and editor
tests/              Catch2 engine, chain and static (click) regression tests
tools/Check.cpp     Headless state checks, preset audio check, static (click) check
                    and UI snapshots
tools/make_docs.py  Regenerates README.txt and docs/README.pdf
installer/          Inno Setup script for the Windows installer (built by CI)
resources/fonts/    Embedded Barlow Semi Condensed (SIL OFL 1.1)
```

---

## Licensing notes (before selling)

- **JUCE:** built with JUCE 9 under its free *Starter* licence, which covers commercial sales up to **US $20,000** of revenue in any 12-month period. Above that, a paid JUCE licence is required. Check the current terms at juce.com before launch.
- **CLAP and clap-juce-extensions:** MIT licence.
- **VST3 SDK:** MIT licence. VST is a registered trademark of Steinberg Media Technologies GmbH. Keep "VST" out of the product and company name.
- **Font:** Barlow Semi Condensed, SIL Open Font License 1.1 (`resources/fonts/OFL-Barlow.txt`). Embedding in a commercial plugin is permitted.
- **Plugin identity:** company "LowKei Audio", manufacturer code `Lwka`, plugin code `Vssl`, CLAP ID `com.lowkeiaudio.vessel`. These are stored in every DAW project that uses the plugin, so never change them after release.
- **Code signing:** the installer is not code-signed yet (planned once a certificate is in place).

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Vessel does not appear in the DAW | Make sure the whole `Vessel.vst3` *folder* is in `C:\Program Files\Common Files\VST3` (and `Vessel.clap` in `C:\Program Files\Common Files\CLAP`), then rescan as described above. |
| Windows SmartScreen warns about the installer | The installer is not yet code-signed. Click **More info**, then **Run anyway**. |
| The vessel sounds out of key | Set Tuning to **Note** or **Chord** and match the **Root** to the track's key. Use **Fine** for tracks not tuned to A = 440 Hz. Check that **Fill** is at 0% (water lowers the pitch). |
| Hardly any ring on a sound | Turn up **Body Mix** or **Decay**, or change **Excite**: Noise rings the body on almost anything; Hits only on attacks. |
| SEAL holds silence | Seal holds what is already ringing. Engage it while the vessel is singing, not after it has died away. |
| Synced effects don't follow the tempo in the standalone app | The app has no song tempo; set **Free Tempo** on the GLOBAL page. |
| Interface too big or too small | Drag the bottom-right corner (70% to 200%). The size is saved with your project. |
