# Global Logical Switches: Draft specification

**Product ask:** Add radio-wide logical switches so users can define reusable
control behavior once and use it across models, instead of copying and
maintaining the same logical-switch chains in every model. The motivating
example is shared trainer-control logic: latch student control, return control
to the instructor when the instructor moves a stick or uses an override switch,
and trigger the corresponding announcements. Shared switch gestures, such as
double/triple clicks, are another use case. Global logical switches should be
usable by both Global Functions and model settings.

**Related feature request:** [#5125: Global Logical Switches](https://github.com/EdgeTX/edgetx/issues/5125).
This draft specifies the proposed feature; it does not implement or close the
request.

**Background discussions:**
[#7176: Global "Logical Switches" ??](https://github.com/EdgeTX/edgetx/discussions/7176)
and [#3342: Some logical switches for radio setup (for global functions)](https://github.com/EdgeTX/edgetx/discussions/3342).

## Version target

Proposed target: EdgeTX 3.0 and its UI, with matching Companion support.
Supporting 2.12 requires a separate backport, particularly for the radio editors.

## Naming and scope

- Full feature name: Global Logical Switches.
- Display identifiers: GL01 through GL64.
- Provide 64 entries in addition to the existing model L01 through L64.
- Store definitions in radio settings, not model files.
- Global means shared definitions, not state that survives every reset.

## Allowed sources and existing source semantics

GL definitions may reference:

- Other global logical switches, including inverted conditions.
- Existing stick sources, using their current EdgeTX semantics. These bypass
  model Input rates/expo but retain existing trainer-stick substitution,
  throttle reversal, and jitter-filter behavior where applicable.
- Local calibrated pots/sliders, without model-specific center/reverse settings.
- Configured multiposition-pot positions.
- Physical switch positions.
- Configurable function-switch states, using the active model's configured
  toggle/group behavior, as in local logical switches and Special Functions.
- Physical trim-button presses, not model trim values. Using these conditions
  does not itself suppress normal trimming.
- Radio battery voltage, radio activity, and trainer-connected status.
- Constant switches ON (and its inverse, OFF) and ONE. ONE is true only for
  the first mixer cycle after startup, model load, or Reset flight.
- Constant sources MIN (-100%) and MAX (+100%).
- Radio time and radio GPS.
- Radio battery current and power, where the hardware provides them.
- IMU tilt, SpaceMouse, light-sensor, and voice-control-sensor sources, where
  the hardware provides them.

GL definitions must not reference model Inputs, Lua mix-script outputs, local
logical switches, output channels, flight modes, GVars, model trims, heli
cyclic sources, function-switch group sources, model timers, model telemetry
(including the telemetry-streaming switch and sensor switches), or trainer
channel values. Model logic can depend on GL. GL cannot directly reference
these model-defined signals, but permitted built-in sources retain their
existing processing, including any model-dependent settings.

## Functions, timing, and dependencies

- Support the existing local logical-switch function set.
- Match existing thresholds, delay/duration ranges, and timing semantics,
  except for the explicit source, reset, and persistence differences here.
- Evaluate GL01 through GL64 in order.
- References to an earlier entry observe its newly evaluated state.
- Self-references and references to later entries observe their state from the
  previous evaluation.
- Permit feedback under these ordered semantics; do not recursively evaluate
  dependencies or reject cycles.
- Maintain a single global runtime context, independent of flight modes.

## Startup and reset

Clear all GL runtime state:

- At radio startup.
- On model change.
- On Reset flight.
- On emergency restart after an unexpected shutdown.

"Reset flight" means `flightReset()`. Users reach it through the Special or
Global Function Reset "Flight" target (shown as "All" on narrow screens), the
color UI's "Reset session" action, and the monochrome main-view reset menu.
Model load also calls it.

Reset includes latches, timers, delays, change-detection history, and in-progress
gestures. No power-cycle sticky persistence is provided in this version.

This differs from local Sticky switches on emergency restart. After an
unexpected shutdown, firmware restores the model from the RAM backup and calls
`logicalSwitchesInit(true)`, which restores every local Sticky latch from its
saved `lsState`. GL latches are cleared instead. GL definitions remain in the
RAM backup, so GL logic keeps working in emergency mode.

GL latch changes must not mark radio settings for saving. Local Sticky switches
update `lsState` in the model and mark it for saving on every change.

Changing a GL definition while the radio runs, through the radio editor, Lua,
or a settings reload, resets that slot's runtime state. Fresh-transition rules
then apply to that slot. Other slots keep their state.

After initialization/reset, Sticky and Edge require fresh transitions.
An already-held button must not set a latch or produce an edge event merely
because a reset occurred. Ordinary level conditions may become true immediately.
Thereafter, normal function and timing semantics apply.

## Consumers

- Offer GL as a condition wherever local logical switches are supported,
  including model logical switches and Special Functions.
- Additionally offer GL to Global Functions.
- Support inverted conditions, displayed as !GL01, etc.
- Offer GL as numeric sources: false corresponds to -100%, true to +100%.
- Offer GL numeric sources in the radio-wide backlight, volume, and status-LED
  source settings. This requires widening those fields; see
  [Identifier capacity](#identifier-capacity).
- Do not offer GL in sources stored in 8-bit containers: heli swash
  collective/aileron/elevator sources and the throttle source.
- Keep existing local logical switches and model APIs unchanged.

### Function-switch conditions

Function-switch conditions use the same meaning in GL, local logical switches,
and Special Functions: the active model's configured state (`cfsState`), not
the physical button's pressed/released state. A shared GL definition can
therefore behave differently across models with different function-switch
configurations. GL runtime state still resets on model change.

Reuse existing switch labels and selectors in the radio editor and Companion.
Help text and Lua documentation must explain that these conditions follow the
active model's configuration. Separate raw button-press conditions are outside
the scope of this version. Function-switch group numeric sources remain
excluded.

## Monitoring and outputs

GL must have the same runtime visibility as local logical switches:

- Logical-switch monitor screens: the color logical-switch view and the
  monochrome main-view switch display.
- SD-card logs: add a GL state column alongside the existing `LSW` column,
  using the same encoding, and update Companion's log viewer to parse it.
- On/off sound files: play per-slot GL on/off files from a radio-wide location
  in the language's sound directory, not the model folder. Use
  GL-specific file names so they cannot collide with local `L` files.
- Companion simulator: show GL state in the simulator outputs widget and
  extend the simulator library and WASM interface used to copy
  logical-switch state.

## Editing, storage, and model transfer

- Cover color and monochrome radio interfaces.
- Provide a Radio Setup editor and corresponding Companion editing.
- Include definitions in radio-settings backup/restore.
- Models store references to GL slot numbers, not copies of definitions.
- Importing a model must not install or overwrite GL definitions.
- A reference to GL03 uses GL03 as configured on the destination radio.
- Transfer radio settings separately when transferring global definitions.
- Undefined slots follow local logical-switch behavior: false, with inversion
  producing true. No additional missing-definition warning or activation block.

## Lua

Provide equivalent definition and runtime access to local logical switches,
excluding persistence.

| Purpose | API |
|---|---|
| Read a global definition | getGlobalLogicalSwitch(id) |
| Write a global definition | setGlobalLogicalSwitch(id, definition) |
| Read the evaluated Boolean state | getGlobalLogicalSwitchValue(id) |
| Request a global sticky latch state change | setGlobalStickySwitch(id, value) |

- IDs are zero-based: 0 means GL01; 63 means GL64.
- Use corresponding local definition fields where applicable, without offering
  persistence.
- Extend generic switch/source discovery and read APIs to recognize GL.
- Existing model.* APIs and local runtime APIs retain their current scope.
- Lua writes must respect the same source restrictions as the editors.
- `setGlobalStickySwitch()` returns the same buffer-full result as
  `setStickySwitch()`. Requests are applied at the next 100 ms tick.

## Implementation constraints

### Identifier capacity

Proposed representation:

- Append the 64 GL switch IDs after existing switch IDs, before the count
  sentinel. Existing physical, local logical, flight-mode, and status IDs stay
  unchanged.
- Append the 64 GL numeric source IDs after existing telemetry sources, before
  the internal sentinels. Existing signal-source IDs, including telemetry, stay
  unchanged.
- Retain signed 10-bit switch fields, with a range of -512 through 511.
- Use signed 11-bit general source fields, with a range of -1024 through 1023.
  Update mixer/input source fields, logical-switch source operands, and the
  radio-settings `backlightSrc`, `volumeSrc`, and `statusLedSrc` fields.
- Expand source-or-number storage to a signed 11-bit value plus its source flag,
  for 12 bits total. Update its mixer/input weight, offset, and curve-reference
  containers together, preserving existing numeric value ranges and Lua
  source-or-number conversions.
- Update YAML signed-source encoding to use the corresponding field width
  instead of a fixed 10-bit mask. Update generated layouts and size assertions
  with these representation changes.

The maximum identifiers for this layout fit these signed representations. Each
row is an upper bound for its class, counting every optional source, four
function-switch groups, `DEBUG_LATENCY`, and 15 GVars. Color targets with 60
sensors include STM32H7RS boards, which have 15 GVars.

| Class | Highest GL switch ID | Highest GL numeric source ID |
|---|---|---|
| Monochrome, 40 sensors | 307 | 447 |
| Monochrome, 60 sensors | 327 | 507 |
| Color, 60 sensors | 375 | 533 |
| Color, 99 sensors | 414 | 650 |

The maximum switch ID is 414; a signed 10-bit field supports positive values up
to 511. The maximum source ID is 650; a signed 11-bit field supports positive
values up to 1023. Both positive and inverted IDs fit without truncation.

Existing source IDs already exceed the current 10-bit fields. On color STM32H7
targets with 99 sensors, telemetry source IDs reach up to 586. A signed 10-bit
field holds at most 511, and the YAML writer reads values of 512 and above as
inverted sources. Selecting a high-numbered telemetry sensor as an Input source
or logical-switch operand is therefore already truncated. The source
widening fixes this independently of GL. It can land first as a separate
change, and should be tested with existing models that reference high-numbered
sensors.

Relevant code: `radio/src/datastructs_private.h:81-86,126-159,187-198,1190,1199,1246`,
`radio/src/dataconstants.h:487-582`,
`radio/src/storage/yaml/yaml_datastructs_funcs.cpp:474-483`.

Add explicit GL handlers to `getSwitch()`, `getValue()`, naming, YAML codecs,
and availability tables. Extend selector/search bounds to include GL while
retaining each context's source restrictions. Treat GL as its own category,
distinct from telemetry and GVars.

The longest inverted display identifier, `!GL64`, needs six bytes including its
terminator. Allocate sufficient buffer space and display width; the shared
32-byte naming buffer and common 8-byte drawing buffers already have capacity.

Use a separate global Lua sticky-command queue. Each bank retains a 6-bit slot
index and two control bits in its one-byte message. Existing local addressing
remains unchanged.

Relevant code: `radio/src/strhelpers.cpp:36,475-540`,
`radio/src/switches.cpp:694-785,1062-1089`,
`radio/src/lua/api_general.cpp:2664-2680`.

### Evaluation cadence and ordering

The mixer reads ADC values and switch positions before calling `evalMixes()`.
Local logical switches are evaluated on a nonzero 10 ms tick inside
`evalFlightModeMixes()`; their timer/Sticky/Edge processing runs at 100 ms in
`doMixerPeriodicUpdates()`.

Place global evaluation outside the flight-mode loop. Use one global context
and preserve the existing function timing cadence. The global timer tick must
likewise run once, not once per flight mode.

Existing stick-source values are prepared by `evalInputs()`, not by ADC sampling
alone. Integration must explicitly account for their freshness and the ordering
of GL evaluation versus model consumers; placing GL immediately after `getADC()`
does not itself provide refreshed existing stick values. Reuse normal source
processing rather than introducing an independent control path, and verify
sampling/response timing in the implementation.

Global Functions execute after mixing. Verify end-to-end latency from a GL
change to its effect on outputs, including actions that affect mixing, not just
when the GL latch changes.

Relevant code: `radio/src/tasks/mixer_task.cpp:229-265`,
`radio/src/mixer.cpp:787-792,1174-1255,1383-1388`.

### Stick-source access

`getValue()` reads stick values from `calibratedAnalogs`. `evalInputs()` applies
model throttle reversal and, for Trainer "Sticks", trainer substitution to those
values. The ADC filter also permits a model jitter-filter override.

Use existing source access and change-detection logic. No new calibration
algorithm, independent ADC snapshot, or filter is required.

Stick sources bypass Input rates/expo, so movement conditions on them need
thresholds chosen for raw stick values.

Function-switch conditions must use the existing `getSwitch()` path, which
returns `g_model.cfsState()` for function switches. No separate hardware-read
path is needed. Ensure the active model's function-switch state is initialized
and updated before GL evaluation.

Relevant code: `radio/src/mixer.cpp:373-387,597-669`,
`radio/src/hal/adc_driver.cpp:334-352,361-377,441-478`,
`radio/src/switches.cpp:715-723`,
`radio/src/input_mapping.cpp:111-124`,
`radio/src/gui/gui_common.cpp:1433-1450`.

### Fresh-transition initialization

Local Sticky switches intentionally latch at startup when their set condition
is already true. An existing regression test explicitly records that behavior.
Local Edge processing also starts measuring an already-held condition after
reset.

Give GL separate reset/arming initialization while leaving local behavior
unchanged. Initialize transition history from valid source samples and suppress a held
Edge gesture until it has been released. Cover conditions derived through
other GL entries as well as direct physical controls. Clear queued global Lua
sticky commands when resetting so old requests cannot restore a cleared latch.

Cover held-through-reset, release/repress, set/reset conditions sharing a button,
GL-derived conditions, Lua requests around model changes, emergency restart,
and editing a single slot at runtime.

Relevant code: `radio/src/tests/switches.cpp:90-147`,
`radio/src/switches.cpp:1091-1157,1192-1204`,
`radio/src/storage/storage_common.cpp:273-279`.

### Storage and defaults

Missing YAML fields do not clear existing memory. Parsing settings without a
GL bank over populated memory would retain stale definitions unless the bank
is explicitly initialized.

Explicitly initialize the GL bank for every applicable firmware and Companion
load/reload/recovery path, not only at process startup. Update `RadioData`,
`GeneralSettings`, both symbolic codecs, generated per-target YAML tables,
size assertions, and copy/conversion paths together.

The normal firmware startup loader clears radio memory, but direct settings
reload and fallback attempts also need review. Companion's `clear()` zeros its
settings object, while YAML decoding into an existing object cannot assume a
fresh constructor.

RTC recovery uses generated data-copy code and a fixed 4,094-byte compressed
payload holding `ModelData` and `RadioData`. Fields marked `NOBACKUP` are
excluded. GL definitions must not be marked `NOBACKUP`, so emergency mode keeps
GL logic. With the proposed widened source operand, 64 definitions add 640
bytes to the uncompressed backup. The widened source fields also grow existing
model records. Runtime state is not part of the backup: the runtime bank
(256 bytes), reset/arming state, and the global Lua queue only add RAM. Verify
final RAM/flash usage and compressed-backup capacity with target builds and
populated model/GL configurations.

Relevant code: `radio/src/storage/sdcard_common.cpp:200-205`,
`radio/src/storage/sdcard_yaml.cpp:122-166,610-618`,
`companion/src/firmwares/generalsettings.cpp:192-196`,
`radio/src/storage/rtc_backup.cpp:25-75`,
`radio/src/storage/rtc_backup.h:26-29`,
`radio/src/CMakeLists.txt:237-249`,
`radio/src/edgetx.cpp:1672-1676`.

### Version compatibility

Firmware writes a semantic version to YAML, but the generated tables
have no reader/validator for that field. Unknown nodes are skipped. Companion
has a newer-semantic-version warning with an explicit user override, not an
unconditional rejection.

That warning does not appear for 3.0.0 files in Companion 2.12.x. A temporary
exception, marked for removal at the 3.0 release, skips it when the file
version is exactly 3.0.0 and Companion is at least 2.12.0 but below 3.0.0. This
applies to both radio settings and model files. Released 2.12 builds cannot be
changed. If GL ships in 3.0.0, Companion 2.12 silently opens GL-bearing files.
It then drops GL definitions and may turn GL references into NONE when saving.
This is a known limitation. Release notes and user documentation must tell
users to upgrade Companion before editing GL-bearing radio settings or models.

Without GL support, an unknown switch name falls through to an enum lookup whose
unknown result is zero (`SWSRC_NONE`). `getSwitch(SWSRC_NONE)` returns true.
Consumers may interpret NONE differently; unsupported GL conditions must not be
assumed to become false on older firmware.

Retain pre-upgrade backups and use GL-bearing models/settings only with
GL-capable firmware and Companion. Existing old files must remain readable by
the new implementation; safe use of new files on older firmware is not promised.
Any stronger downgrade protection would need an explicit additional mechanism.

Relevant code: `radio/src/storage/yaml/yaml_datastructs_tx15.cpp:385-386`,
`radio/src/storage/yaml/yaml_tree_walker.cpp:43-55,261-284`,
`radio/src/storage/yaml/yaml_datastructs_funcs.cpp:1382-1465`,
`radio/src/switches.cpp:694-700`,
`companion/src/firmwares/edgetx/yaml_generalsettings.cpp:463-485`,
`companion/src/firmwares/edgetx/yaml_modeldata.cpp:1363-1369`,
`companion/src/firmwares/edgetx/yaml_rawswitch.cpp:91-95,199-215`.
