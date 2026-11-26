<!-- SPDX-License-Identifier: MIT -->
# Information architecture

How the UI is organized: the Home screen, the logical input layer, what each key does in each context, quick settings, Menu's eight entries, settings grouped by intent, and the key maps for the four reference radios. Principles referenced as P1 to P10 are in `principles.md`.

Hardware details below (key labels, knob types, display sizes) are the designer's understanding and must be checked against `docs/spec/capabilities.md` and the board descriptors when those exist. The design does not depend on them: a wrong assumption only changes a row in a key map table.

---

## 1. Concepts the user sees

Keep the number of concepts small and name them the same everywhere.

| Concept | Meaning | Not called |
|---|---|---|
| **Channel** | A saved frequency plus mode and settings (FM or DMR) | memory, slot |
| **Zone** | A named list of channels | bank, group |
| **VFO** | Tune any frequency by hand | frequency mode, manual mode |
| **Talk target** | Where PTT goes right now: a talkgroup, a person, all-call or (FM) the channel | TX contact, contact, call target |
| **Talkgroup (TG)** | A DMR group number with an optional name | group contact |
| **Contact** | A person (DMR ID, name, callsign) | private contact, digital contact |
| **Listen list** | The talkgroups a channel listens to (the codeplug's RX group list) | RX group list (that phrase appears only in "Advanced" and in qdmr) |
| **Call log** | Stations heard and calls made, newest first | recent, last heard (a filter inside the call log) |
| **Digital monitor** | Promiscuous monitoring: play every talkgroup on this timeslot ("hear all" in the code) | promiscuous, hear all |

On-the-fly changes are **temporary** (P7). A temporary change shows a dot next to the changed field on Home and lasts until the channel changes or `BACK` is pressed on Home. Making it **permanent** writes it to the channel.

The talk target has three lifetimes:

1. **One-shot**: press PTT while a talkgroup or contact is focused in any list. Transmits to it now. It stays the talk target for the reply window (default 10 s after the last transmission or reception on it), then Home reverts to the channel default.
2. **Temporary**: `SELECT` on a talkgroup or contact ("Use TG 91", "Use Joe"). Stays until the channel changes or `BACK` on Home.
3. **Permanent**: long `SELECT` on it ("Make channel default"). Writes the channel.

Replying to whoever just spoke is automatic during the reply window: if a station calls on TG 91 while the channel default is TG 3100, Home shows "Reply → TG 91 (8 s)" and PTT goes to TG 91 until the countdown ends. The setting "Reply window" (Talk & listen) allows 0 (off) to 30 s.

---

## 2. Logical actions

Screens consume only these. Each board maps physical inputs to them (`boards/<radio>/keymap`). Each action arrives with a gesture: `PRESS` (short, released before the long-press threshold), `LONG` (held past the threshold, default 600 ms, fires once at the threshold with a tick), `REPEAT` (navigation keys only, while held), and for `PTT` and `MONITOR` the raw `DOWN`/`UP`.

| Action | Meaning everywhere | Required? |
|---|---|---|
| `NAV_UP`, `NAV_DOWN` | Move focus; on Home and in value editors, change the value (channel, frequency, level) | yes |
| `NAV_LEFT`, `NAV_RIGHT` | Change the focused value / switch tabs. If absent, screens fall back to `NAV_UP`/`NAV_DOWN` after `SELECT` | optional |
| `SELECT` | Do or open the focused thing. Long: make it permanent (save) | yes |
| `BACK` | Back one level; on Home clear temporary state. Long: Home | yes |
| `QUICK` | Quick settings for the current context. Long: key lock (on Home) | strongly recommended; fallback long `SELECT` |
| `CONTEXT` | Actions for the focused thing; on Home, the last or current caller. Long: the talk target picker (DMR) | strongly recommended; fallback: first item in Menu |
| `DIGIT_0` to `DIGIT_9` | Type (numbers, T9 letters). On Home: start Find. In quick settings, Menu and its numbered lists: pick item by number. Long on Home: speed-dial slot | optional (keypad radios) |
| `PTT` | Transmit to the talk target (or the focused target, one-shot) | yes |
| `MONITOR` | FM: squelch open while held. DMR: digital monitor (hear all) on / off on each press (#19) | recommended |
| `TALK_TARGET` | Open the talk target picker | assignable |
| `EMERGENCY` | Hold 1.5 s to start or cancel emergency (if enabled in codeplug) | optional |
| `CHANNEL_POS(n)` | Absolute channel selector position (1 to 16) | from knob |
| `ENC_STEP(+/-)` | Encoder detent. Forward (+, clockwise): next channel on Home (as `NAV_UP`), value up in editors, focus **down** in lists and quick tiles (the next row, like a scroll wheel). Back (-): the opposite | from encoder |
| `USER_1` to `USER_4` | Programmable keys; each has a short and a long function from the assignable list | optional |

Assignable functions for `USER_n` keys (short and long, chosen in Settings > Buttons or in qdmr): Monitor, Talk target, Quick, Context, Call log, Power cycle, Scan on/off, Hear all on/off, Privacy on/off, Zone picker, Speak status, Backlight, Mode FM/DMR (in the VFO), Copy to VFO, Lock, Reply, Radio check last caller, Silent (mute tones and prompts), A/B and Dual watch ([dual-watch.md](dual-watch.md)), None.

Rules for key maps (P3, P10):

- Every board maps `NAV_UP`, `NAV_DOWN`, `SELECT`, `BACK`, `PTT`. Everything else must be reachable through a fallback on boards that lack it.
- Text keys (`*`, `#`, digits) inside a text field are typing keys: `*` = shift/case/symbols, `#` = space and mode (T9 / multi-tap / digits), long digit = the literal digit. `BACK` deletes a character, long `BACK` still goes Home (after an "unsent text kept as draft" banner).

---

## 3. Home

Home answers four questions at a glance (P2, P5): **Where am I** (zone, channel), **What will PTT do** (talk target, mode, TS/CC or tone, power, privacy), **What is happening** (TX/RX, caller), **Is the radio OK** (battery, signal, GPS, messages, lock).

Regions, top to bottom, the same on every display class (sizes differ):

1. **Status strip**: state pill (TX red / RX green / scan / hotspot / idle), USB network word (IP, with link arrows once the computer has it up), RSSI, battery %, GPS fix, unread messages, lock, privacy. Drawn on every screen; on small displays a page's title takes the strip's place and the battery is left to Home.
2. **Place**: zone name (small) and channel number + name (large). In VFO: the frequency (large) and "VFO".
3. **Talk target**: "PTT →" + TG or contact name and number (large); on an FM channel, the RX frequency and a repeater's offset, without the arrow, until PTT (the VFO: → the TX frequency), with chips: mode, TS, CC (DMR) or tone and bandwidth (FM), power. Temporary fields have a dot; reply window shows a countdown.
4. **Activity**: idle = the last heard station (name, callsign, TG, time ago). RX = the current caller big (name, callsign, ID, TG, talker alias, RSSI bar, distance if a position is known). TX = timer and target.
5. **Hint row**: what the nearest keys do here, for example `OK Menu · * Quick · # Last caller`. Hidden in large-text mode and after the user turns hints off.

Home keys:

| Input | Short | Long |
|---|---|---|
| `NAV_UP` / `NAV_DOWN`, encoder forward / back | Channel +1 / -1 in zone (VFO: frequency step) | repeat |
| `SELECT` | Open Menu (the top-level entries) | Make temporary changes permanent ("Saved to channel") |
| `BACK` | Clear temporary state: end reply window, revert temporary TG and fields, dismiss banner | Already Home: tick |
| `QUICK` | Quick settings | Lock keys |
| `CONTEXT` | Actions for the last/current caller | Talk target picker (DMR) |
| Digits | Find (channels, TGs, contacts, settings; in VFO, frequency first) | Speed-dial slot 0-9 |
| `PTT` | Transmit to talk target | - |
| `MONITOR` | Monitor while held | - |

Large-text mode (accessibility, or chosen by the user) shows only the status strip, the channel name and the talk target, each as large as fits.

### Banners and interruptions

- Group call heard: Activity region shows it; no focus change on any screen. Outside Home, the status strip shows `RX TG 91 · Q1AAA`.
- Text message: tone, banner "Message from Q1AAA: first words…", `CONTEXT` while the banner is visible opens it; the banner goes away after 8 s and the message waits in Messages with an unread count on the status strip.
- Private call to me, call alert, radio check request: banner with tone; Home shows the caller with "PTT to reply". A private call does not take over a screen the user is editing; it shows in the status strip and the reply is one `CONTEXT` away.
- Zone or channel share offered: banner with tone; it has no timer and stays until `CONTEXT` opens the offer, `BACK` hides it (on any screen) or the offer ends. The offer also waits in Messages (Zone offers).
- Emergency received: full-screen, alert tone, repeated until acknowledged with `SELECT` or dismissed with `BACK`.

---

## 4. Menu: a 3x3 grid that matches the keypad

`SELECT` on Home opens Menu, titled Menu. Its entries are numbered as the keypad's 3x3 grid; digits pick directly, and
each entry keeps its digit on every radio and build so muscle memory works.
Entries show their values where they fit beside the name: the unread count on Messages, the zone in use on Zones, a
running scan on Scan.

| # | Entry | What is inside |
|---|---|---|
| 1 | **Contacts** | Search field on top. Talkgroups and people, with New contact first. Item actions: Use, Private call, Message, Radio check, Call alert, Request position, Add to speed dial, Edit |
| 2 | **Messages** | Messages (unread first), New APRS message, and Zone offers while another radio's zone or channel offer waits |
| 3 | **Call log** | Everyone heard and every call made, newest first. Item actions are the same as Contacts, plus "Save as contact" and "Show position" |
| 4 | **Zones** | The VFO and the zones; a zone's channels. A zone's list is also its editor: "Add channel" first, "Zone options" (Rename, Copy as new zone, Share zone to..., Delete zone) last, and Channel actions on a channel (Move up, Move down, Copy here, Edit channel, Share channel to..., Remove from zone; the Share rows with zone sharing) |
| 5 | **Scan** | 1 Scan on/off (the zone, or the band in the VFO), 2 Roaming |
| 6 | **Location** | 1 GPS status, 2 APRS, 3 Satellite (where built), 4 Location settings |
| 7 | **Connections** | 1 Hotspot, 2 USB network, 3 SFR (its status page and settings), 4 Data services (where built), 5 PC and network (its settings) |
| 8 | **Settings** | Grouped by intent (section 6) |
| 9 | (empty) | Planned: a favorite the user picks |

Scan, Location and Connections are numbered lists too, and their entries keep their numbers in every build: an entry a
build doesn't have is left out, and the others keep their digits. Settings > Programming has Channels (every channel by
number, New from current first; a channel's settings, edited in place and saved, ui.md 2.1), zone editing, the VFO,
Diagnostics and Factory reset.

Contacts, Messages and the call log are the "people" side, Zones, Scan, Location and Connections the "radio" side,
Settings the preferences and the programming. Nothing operational lives only in Settings (P1); editing channels is
programming.

---

## 5. Quick settings

`QUICK` opens a 3x3 grid (240x320 and 160x128) or a numbered list (128x64). Positions are fixed; digits 1-9 act directly, matching the keypad layout. Positions that mean the same in FM and DMR are in the same place.

| Key | DMR channel | FM channel |
|---|---|---|
| 1 | Power (each press cycles levels) | Power |
| 2 | Timeslot 1/2 | Squelch level |
| 3 | Color code | Tone (CTCSS/DCS, RX and TX) |
| 4 | To VFO (copy the channel to the VFO and open it) | To VFO |
| 5 | Hear all (promiscuous) on/off | Monitor latch on/off (TX freq in the VFO) |
| 6 | Privacy on/off (TX freq in the VFO) | VOX on/off |
| 7 | Dual watch (On / Off; a zone scan is in Menu > Scan) | Dual watch (On / Off; a zone scan is in Menu > Scan) |
| 8 | Talk target | Bandwidth 12.5/25 kHz |
| 9 | Brightness | Brightness |
| 0 | All settings | All settings |

- Toggle tiles change on one press; cycle tiles (power, timeslot, bandwidth) advance one value per press; value tiles (CC, squelch, tone, brightness) open an inline editor (`NAV_UP`/`NAV_DOWN` or digits, `SELECT` or `QUICK` to close).
- Every change is temporary on a channel (dot on Home). Long `SELECT` on a tile makes it permanent. In VFO, changes apply to the VFO directly.
- Tile 4 depends on where you are. On a channel it is "To VFO": the channel's settings in effect now (temporary changes included) become the VFO's own and the VFO opens ("Copied to VFO"); the channel is unchanged, and a channel the VFO can't hold is refused with a toast. In the VFO, tile 4 is Mode (FM / DMR), kept like the VFO's other settings. A channel has no temporary mode change.
- In the VFO, the tile that is Monitor on FM (5) and Privacy on DMR (6) is TX freq ("TXFreq" on a tile): the VFO's split, its TX frequency against RX ("Off", "+600k", "-5M"). Up / Down walk a line out from Off at once, Up +, Down -, through the band's usual offsets most common first and stopping at the ends (2 m 600 kHz, then 1 MHz; 1.25 m 1.6 MHz; 70 cm 5, 7.6, then 1.6 MHz, from the ARRL, IARU Region 1, RSGB and WIA band plans), skipping a TX outside RX's transmit band; a typed split goes to the first offset its way. While it edits, the hint row says what Up / Down and the digits do, and while digits are typed shows the whole TX frequency so far ("TX 147.02___"; the tile says "TX"). Any other split is typed: digits type a TX frequency, OK sets it (refused outside the transmit bands: "Can't transmit there"), BACK takes a digit back. Monitor stays on its key; privacy is for channels.
- Home shows a split VFO's TX frequency: on FM after PTT→; on DMR in the top row ("VFO  TX 147.120"), or as the offset among the chips on displays without that row. While transmitting, the large frequency is the TX frequency, in the TX color; the DMR call screen has a "TX 147.120 MHz" line under the target.
- Each tile shows its current value. The voice prompt reads "Power, high" etc.
- `BACK` or `QUICK` again closes and returns to where the user was.

DMR channel with no codec installed (any DMR tile, and the VFO's Mode tile switching to DMR): the DMR tiles show "DMR needs the codec (see docs)" instead of failing silently.

---

## 6. Settings by intent

Settings: groups 1 to 9, then Radio info (no digit; `NAV_UP` from the top reaches it). `0` opens Find, which finds every
setting by name. Settings > group > item, never deeper, except the two pages of Tones & alerts. A page's title is the
text of the row that opens it, in sentence case.

| # | Group | Contents |
|---|---|---|
| 1 | **Display** | Theme (where more than one theme fits the display), the theme's appearance rows (the reference theme: Colors, Highlight color, Filled status bar; the panels theme: Display profile, Day/Night, GPS clock), then Brightness, Backlight, Large text, Hint row, Talkgroup name first, Language, Battery saver, Auto power off, Low battery warning |
| 2 | **Tones & alerts** | Volume (radios without a knob), Tone volume, Key tones, Voice prompts, Vibrate; pages: Alert tones (each beep on or off) and Signaling tones |
| 3 | **Audio** | Mic gain, Mic AGC, Mic noise reduction |
| 4 | **Call settings** | Reply window, TX timeout, Show talker alias, DMR monitor at start, Answer radio checks, Answer call alerts, Message format, Accept zone shares |
| 5 | **Receive & VFO** | Default squelch, VFO step, FM broadcast step, Dual watch talk back, Dual watch delay |
| 6 | **Privacy** | Key type, Key |
| 7 | **Security & keypad lock** | Auto lock, PTT when locked, Knob when locked |
| 8 | **Buttons** | Long press time, Key repeat, Up/Down on Home, the programmable keys |
| 9 | **Programming** | Channels, Zones (zone editing), VFO, DMR ID and name, Diagnostics, Backlight tests (where built), Crash log, Frequency trim, RX past bands, Hardware, Factory reset (last; confirmed, P7) |
| - | **Radio info** | Read only: battery, DMR ID, radio name, About (versions), GPS, codeplug |

The settings of a feature with its own Menu entry are there: Menu > Location > Location settings (GPS, position
format, sending and answering positions, the position source), Menu > Connections > SFR (SFR receive, SFR CW ID) and
Menu > Connections > PC and network (Hotspot allowed, TFTP codeplug write, USB network at start, PC answers data, USB
address).

Rows: an on/off row flips on `SELECT` and shows the new state; a row with several values opens a pick list with a
check mark on the value in use; a number opens a value editor, the value between left and right markers in the theme's
accent color (amber only for warnings).

Rule: each setting lives in exactly one group, and Find also reaches it by name and synonyms ("promiscuous" finds
DMR monitor at start; "RX group" finds Listen lists). Find opens a setting where it lives, with the screens on the
way under it, so `BACK` goes up through them.

---

## 7. Key maps for the reference radios

Notation: `short / long`. "-" means none. All maps also follow the text-field rules in section 2.

### 7.1 Baofeng DM-1701 (and TYT MD-UV380 class): keypad, channel encoder, volume knob, 3 side keys, 160x128 color

| Physical | Logical (short / long) |
|---|---|
| Green Menu key | `SELECT` / save |
| Red Back key | `BACK` / Home |
| Up / Down | `NAV_UP` / `NAV_DOWN` (repeat) |
| `*` | `QUICK` / lock |
| `#` | `CONTEXT` / talk target picker |
| 0-9 | digits / speed dial |
| Channel encoder detent | `ENC_STEP` (Home: channel; editors: value; lists: focus, forward = down) |
| SK1 (top side key, above PTT; row 2 D5, [hardware-dm1701.md](../spec/hardware-dm1701.md) 20) | `MONITOR` (while held) |
| SK2 (below PTT) | `AB` (dual watch A / B) / Dual watch on/off |
| SK3 (lowest side key) | Press: power cycle. Hold 1.5 s: `EMERGENCY` where it is built (hold again to cancel), else speak status |
| P1, P2 | Previous zone, Next zone |
| PTT | `PTT` |

Notes: the channel knob is a quadrature encoder with detents, not a 16-position switch ([hardware-dm1701.md](../spec/hardware-dm1701.md) 10.3), so
zones of any size step through it. No `NAV_LEFT`/`NAV_RIGHT`.

### 7.2 Baofeng DM-32UV: keypad, channel encoder, volume knob, side keys, color display

| Physical | Logical |
|---|---|
| Menu / OK | `SELECT` / save |
| Back / Exit | `BACK` / Home |
| Up / Down | `NAV_UP` / `NAV_DOWN` |
| Encoder detent | `ENC_STEP` (Home: channel; editors: value; lists: focus, forward = down) |
| Encoder push (if present) | `SELECT` / save |
| `*` | `QUICK` / lock |
| `#` | `CONTEXT` / talk target picker |
| 0-9 | digits / speed dial |
| SK1 | `MONITOR` |
| SK2 | `AB` / Dual watch |
| Top / alarm key (if present) | `EMERGENCY` or assignable |
| PTT | `PTT` |

The encoder has no hidden modes (survey: Yaesu knob complaints). Forward is "next" everywhere: the next channel on
Home (as Up), a higher value in an editor (as Up), the next row down a list (as Down). Lists follow the knob the way a
scroll wheel does; Home and editors follow the number. (Until 2026-10-10 the knob moved list focus up, which read as backward on the radio.) Display resolution comes from the board descriptor; the layouts for its class apply.

### 7.3 Radioddity GD-77: keypad, no encoder, channel knob absent, 128x64 monochrome

| Physical | Logical |
|---|---|
| Green key | `SELECT` / save |
| Red key | `BACK` / Home |
| Up / Down arrows | `NAV_UP` / `NAV_DOWN` |
| Left / Right arrows (if present on the model) | `NAV_LEFT` / `NAV_RIGHT` |
| `*` | `QUICK` / lock |
| `#` | `CONTEXT` / talk target picker |
| 0-9 | digits / speed dial |
| SK1 | `MONITOR` |
| SK2 | `AB` / Dual watch |
| Top orange key (if present) | assignable (default Power cycle / Speak status) |
| PTT | `PTT` |

128x64 has no room for a 3x3 grid; quick settings are a numbered list with the same numbers. Home uses two text sizes only.

### 7.4 TYT MD-9600 (mobile): front panel keys and encoder, hand mic with keypad, 128x64 monochrome

The front panel has Enter (green), Back (red), Up, Down, P1-P4, the orange key, the power key, the channel encoder and
a volume knob in the audio path; it has no `*`, `#` or digit keys ([hardware-md9600.md](../spec/hardware-md9600.md)
9). The display is the 128x64 class.

| Physical | Logical (short / long) |
|---|---|
| Enter (green) | `SELECT` / save |
| Back (red) | `BACK` / Home |
| Up / Down | `NAV_UP` / `NAV_DOWN` (repeat) |
| Channel encoder detent | `ENC_STEP` |
| P1 | `QUICK` / lock; hints call it "P1" |
| P2 | `CONTEXT` / talk target picker; hints call it "P2" |
| P3 | `MONITOR` (while held); assignable |
| P4 | `TALK_TARGET` / `USER_2`; assignable |
| Orange key | `USER_3`; assignable |
| Power key | power on and off (standby), not in the key map |
| Mic PTT | `PTT` |
| Mic 0-9, `*`, `#` (once the mic keypad is read) | as on handhelds (`*` = `QUICK`, `#` = `CONTEXT`) |
| Mic Up / Down | `NAV_UP` / `NAV_DOWN` |

Until the hand-mic keypad is read the radio has no digits: text entry uses Up / Down ([ui.md](../spec/ui.md) 2.1), and hints that
start with a digit are left out. Mobile-specific: while driving, the "driving" profile (Settings > Display, or
automatic when the radio is a mobile and the mic is the input) hides the text keyboard (P5, NHTSA lockout: no manual
text entry), offers canned messages only, and enlarges Home. Voice prompts are on by default on mobiles.

### 7.5 Few-key radios (example: four front keys and a knob)

| Missing action | Fallback |
|---|---|
| `QUICK` | long `SELECT` (save then moves into `CONTEXT` > "Save to channel") |
| `CONTEXT` | Hub item 0 "Last caller" above Contacts. In Zones and a zone's list, long `SELECT` opens what `CONTEXT` would (the zone's list, Channel actions) |
| Digits | Find uses a letter wheel (encoder or Up/Down) with recent and frequent items first |
| `TALK_TARGET` | Quick tile 8 |
| `MONITOR` | Quick tile 5 latch |


### 7.6 Baofeng DM-1801 Pro: keypad with direction keys, side keys, SOS key, no encoder, 128x64 monochrome

| Physical | Logical (short / long) |
|---|---|
| Menu | `SELECT` / save |
| Back | `BACK` / Home |
| Up / Down | `NAV_UP` / `NAV_DOWN` (repeat) |
| Left / Right | `NAV_LEFT` / `NAV_RIGHT`, long: `USER_1` / `USER_4` |
| VFO/MR | `VFO_TOGGLE` (switch between channel and VFO) / `COPY_VFO` (copy the channel to the VFO) |
| A/B | `ZONE_UP` |
| `*` | `QUICK` / lock |
| `#` | `CONTEXT` / talk target picker |
| 0-9 | digits / speed dial |
| SK1 | `MONITOR` (while held) |
| SK2 | `AB` / `USER_2` |
| SOS (top) | hold: `EMERGENCY` where it is built |
| PTT | `PTT` |

The layouts are the 128x64 ones of section 7.3. Which matrix position is which key isn't known yet
([hardware-dm1801pro.md](../spec/hardware-dm1801pro.md) 9.1).
---

## 8. Screen inventory

| Screen | Reached from | Depth |
|---|---|---|
| Home | power on, long `BACK` | 0 |
| Quick settings | `QUICK` | 1 |
| Talk target picker (Talkgroup / Contact) | `TALK_TARGET`, quick 8, long `CONTEXT` on a DMR channel | 1 |
| Find | digits on Home | 1 |
| Caller actions | `CONTEXT` | 1 |
| Menu | `SELECT` | 1 |
| Contacts / Messages / Call log / Zones / Scan / Location / Connections / Settings | Menu | 2 |
| Item actions (contact, message, channel) | from a list | 3 |
| A zone's list (channels, Add channel, Zone options) | `CONTEXT` in Zones | 3 |
| Channel actions, Add channel picker, Zone options | a zone's list | 4 |
| Copy here (the channel editor), Edit channel | Channel actions (replaces it) | 4 |
| Rename (text entry), Delete zone (confirmation) | Zone options | 5 |
| Message composer | Contacts/Call log/Messages actions | 3 |
| Setting's value editor | a settings row | inline, not a further level |
| Hotspot, USB network, SFR status | Connections | 3, and shown on Home while active |
| Settings group | Settings (Location, SFR, PC and network: their Menu entry) | 3 |
| Pick list of a setting | a settings row | 4 |
| Channels, a channel's settings | Settings > Programming | 4, 5 (programming, not field use) |
