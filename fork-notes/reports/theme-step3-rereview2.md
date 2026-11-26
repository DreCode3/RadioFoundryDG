<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# QA re-review 2: theme step 3, ae138467..e94841a2

Scope: the last fixes and the user's three decisions, and the lines these commits touched. I worked in two worktrees
at `e94841a2`, both removed now. Scratch output is in this directory (`r3/`, `sw3/`, `gal3/`, `gal4/`,
`remut3.txt`, `py/mut3.py`).

## Verdict: APPROVED

No defects found. Each decision is built as recorded:

- The nine-level stack holds Find's deepest path.
- The Zone offers row shows the offer's name again, and the stack is unchanged.
- The focused-label copy takes the real critical section on both platforms.

Two latent risks around the name-first default and one test gap are below. None blocks.

## Findings

### 1. Latent risk: a radio installed over another firmware's settings area never gets name first on

`ui_svc_settings_new()` is true only for `SETTINGS_DEFAULTS`.

- A radio whose settings area holds another firmware's data boots as `SETTINGS_FOREIGN` (`settings.c:161`), so
  `ui_app_init` leaves name first off.
- The area is claimed later, at run time, through the debug link (`debuglink.c:811`, `settings_poll` and `attach`).
  The claim sets `stats.load = SETTINGS_DEFAULTS` and saves the live settings: name first off, because
  `ui_app_init` already ran.
- The next boot loads that record (`SETTINGS_LOADED`), so a genuinely new radio stays number first.
- Whether a GD-88 install meets this depends on what the stock firmware leaves in that flash region. I didn't
  establish that.

Fix: treat `SETTINGS_FOREIGN` as new too. The bit is then in RAM, and the claim saves it. Or set the bit in
`attach()` when the claimed area is blank.

### 2. Latent risk (low): the default now writes at boot, so a transient load failure becomes permanent

`settings_init` reports `SETTINGS_DEFAULTS` whenever:

- no page-0 record validates;
- the layout differs; or
- the re-read of the found slot fails (`settings.c`, `read_slot(sec0, sl0, r) == SLOT_VALID && ...`).

On a panels radio, `ui_app_init` then calls `ui_svc_setting_set(UI_SET_LABEL_FIRST, 1)`. That marks the settings
dirty, and they are saved a short delay later. A one-off read error at boot used to leave the real record in force
for the next boot. Now that record is superseded by a record of defaults, with no user action. Before this change,
the defaults were saved only once something changed them.

A layout change (an incompatible upgrade) resets every setting anyway, so turning name first on there loses nothing
more. I didn't check whether other code already saves at boot (for example the radio core's position), which would
make this pre-existing.

Fix, if wanted: keep the bit in RAM (`settings_begin` without marking dirty), or set it only when no slot in the area
holds any record (`have_newest` false).

### 3. Test gap: "a stored 0 still means number first" is tested for `SETTINGS_LOADED` only

Mutation N17 makes `ui_svc_settings_new()` true for every load except `SETTINGS_LOADED`. That would turn name first on
for every existing user after a firmware update that appends settings (`SETTINGS_UPGRADED`). No test catches it:

- `radioCoreTest.c:6568` checks `LOADED` only.
- Nothing checks `UPGRADED`, `FOREIGN` or `RAM`.

The code itself is right (`== SETTINGS_DEFAULTS`). Add an `UPGRADED`-load case.

### 4. Note

uiThemeTestPanels fails at 4e9cc7e6..5ec36c4b and e94841a2 fixes it, as reported. I ran it at e94841a2 only: it
passes.

## What was asked, judged

**1. Stack.**
- `UI_STACK_MAX` is 9.
- The flow "Find's deepest path" walks Settings > 0 > APRS > station > Message > To callsign. With the stack set back
  to 8 (N15), uiFlowTest and uiFlowTestNoColor fail.

**2. Name-first default.**
- `settings_defaults` no longer sets the bit, so every stored record keeps its meaning.
- `ui_app_init` picks the starting theme first, then sets the bit only if the radio is new and that theme declares
  `UI_THEME_OPT_NAME_FIRST`. The theme comes from the stored id, which is 0 on a new radio, so the build's default
  where it fits.
- `UI_SET_LABEL_FIRST` is applied afterwards in the same function (`ui_app.c:2040`), so the order is right.
- Simulator: On on 320x240iR10, Off on 220x176 and 160x128 (`sheet-nf.png`). The simulator treats a reset as new and
  `reboot` as not new.
- Factory reset:
  - It passes `ui_theme_starts_name_first()`, which is the theme picked for id 0. That is the theme the radio restarts
    in, because the reset clears `gui_theme`, whatever theme 1 or 2 was stored.
  - The job ORs the bit into the defaults before saving, then reboots (`ui_svc_storage.c:1524-1546`).
  - The on-screen confirm is the only caller of `ui_svc_factory_reset`. The debug link, CPS and USB have no factory
    reset that calls `settings_defaults`.
- Load paths: finding 1 covers `FOREIGN`, finding 2 covers `DEFAULTS` after a read failure. `UPGRADED`, `LOADED` and
  `RAM` keep the stored bit (finding 3 is the missing test).

**3. Offer name.**
- `ui_svc_zone_offer_name` reads the current offer through `offer_record(0, ...)`. It returns false when none waits,
  and the row then has no value. After expiry the row is gone (`r3/o3.png`).
- The names are NUL-terminated (`ZS_NAME_MAX + 1`). For a channel offer it gives the first channel's name.
- The value buffer arithmetic fits: marker, name, two markers and NUL.
- Every task's stack in `make stack-check` is identical to the previous re-review. GD-88 UI is 1664 deepest / 204
  free; DM-1701 UI is 1640 / 88. All tasks were compared.

**4. The four mutations missed last time:**
- M15 is now caught by uiFlowTest.
- N4 is caught by uiPrototypeTest.
- N6 is caught by both.
- N7 is caught by uiFlowTest.

**5. `ui.focus_label` and the critical section.**
- C-SKY:
  - `ui_crit_enter` is `taskENTER_CRITICAL` (`vPortEnterCritical`). It disables interrupts and counts nesting in
    `ulCriticalNesting`; exit at zero re-enables them and takes a pending yield (`platform/hrc7000/freertos/port.c:81`).
- STM32:
  - It is the FreeRTOS Cortex-M4 port (`uxCriticalNesting`; it masks up to the syscall priority).
- Time held:
  - Each section holds one `str_copy` of at most 35 bytes.
  - The UI side does its `strncmp` outside the section and writes only when the label changed.
- Callers:
  - The UI task (list drawing) and the debug link's view (`link_section`, task context).
  - `debuglink/dlog.c` already uses `taskENTER_CRITICAL` the same way. No interrupt caller.
- Weak stub versus real function:
  - In the GD-88 ELF, `ui_crit_enter` calls `vPortEnterCritical`.
  - In the DM-1701 LTO ELF, `focus_drawn` and `link_section` call `vPortEnterCritical` and `vPortExitCritical`
    directly (objdump).
  - So the strong definition in `ui.c` wins over the weak no-op in `ui_svc_stub.c`, which is also linked into
    firmware.
- Boards without a display build clean:
  - rt10 and gd88b (C-SKY), 0 warnings.

**6. Gallery.**
- Regenerated with the workflow's `UI_GALLERY_COLORS` (red, yellow, light_blue): all 1263 per-screen PNGs and all 7
  sheets are identical to the committed ones.
- The commit changes only messages_zone_offer (7 classes), the six FM Home frames and the sheets.

**7.** See finding 4.

## Checks run

| Check | Result |
|---|---|
| uiFlowTest, uiFlowTestNoColor | passed (macOS) |
| radioCoreTest | 7228 checks, passed (macOS) |
| settingsTestPanels | passed (macOS) |
| uiPrototypeTest | passed (macOS) |
| uiThemeTest, uiThemeTestPanels, settingsTest | passed (Linux image) |
| GD-88 build + stack-check | 0 warnings. Flash 790992 B, RAM 205872 B (+304 B flash and +8 B RAM against ca0b8bc0) |
| DM-1701 build + stack-check | 0 warnings. Flash 709236 B, RAM 127568 B |
| DM-1701 with ZONE_SHARE=n | 0 warnings |
| rt10, gd88b | 0 warnings |
| Every task's stack, against ca0b8bc0 | unchanged on both boards |
| Gallery regenerated and compared | identical |
| `i18n.py --check`, `check_doc_links` | pass |
| Simulator renders | offer row on 320x240iR10, 160x120i5 and after expiry; name-first default per display class |
| objdump of both ELFs for the critical section | real critical section |

## Mutations (uiFlowTest x2, uiPrototypeTest, radioCoreTest, settingsTestPanels)

| Mutation | Result |
|---|---|
| M15 Radio info DMR ID | caught (uiFlowTest) |
| N4 no reapply after a theme switch | caught (uiPrototypeTest) |
| N6 Radio info first in Find | caught |
| N7 "Use" without its space | caught (uiFlowTest) |
| N11 never treated as new | caught (uiPrototypeTest) |
| N12 name first on every start | caught (uiPrototypeTest) |
| N13 factory reset ignores name_first | caught (radioCoreTest) |
| N14 offer row without name | caught (uiFlowTest) |
| N15 `UI_STACK_MAX` 8 | caught (uiFlowTest) |
| N16 start theme read from the theme in use | caught (uiPrototypeTest) |
| N17 new for every load but LOADED | **missed** (finding 3) |

10 of 11 caught; all four earlier misses are now caught.

## Not run

- The full host suite and `tools/ci.sh`.
- HIL.
- The on-screen factory reset end to end in the simulator (my script didn't reach it). The reset logic was read, and
  radioCoreTest covers it (N13).
- The commits 4e9cc7e6..5ec36c4b individually.
- Whether a GD-88 install meets the foreign-area path in finding 1.
