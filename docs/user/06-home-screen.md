<!-- SPDX-License-Identifier: MIT -->

# The Home Screen

Home answers four questions at a glance: where you are, what **PTT** will do, what is happening, and whether the radio
is OK.

<img src="../ux/screens/160x120i5/home_dmr.png" width="200" alt="Home on a DMR channel">
<img src="../ux/screens/160x120i5/home_fm.png" width="200" alt="Home on an FM channel">

## Home in the Panels look

On a radio with the Panels look ([Settings > Display > Theme](17-settings.md#display-key-1)), Home is made of rounded
panels on a darker background: the status bar at the top, the mode bar with the channel's mode in large letters, the
channel panel, and the bottom bar with the labels of the two soft keys.

<img src="../ux/profile-samples/home-commercial.png" width="310" alt="Commercial Home receiving a DMR call">
<img src="../ux/profile-samples/home-amateur.png" width="310" alt="Amateur Home receiving the same DMR call">

The channel panel shows the zone and, as the main line, the channel's name from the codeplug. In the VFO the main line
is the frequency. **Settings > Display > Display profile** chooses what else it shows:

| Profile | What you see |
|---|---|
| Commercial (default) | The zone and a larger channel name. The talk target appears below it only when **PTT** would go somewhere other than the channel's own talkgroup: a target chosen for now, the reply window's, or a one-shot call's. In the VFO, where the main line is a frequency, the talk target always shows |
| Amateur | The zone, the channel name, the talk target (its talkgroup or ID and its name, the name first with **Talkgroup name first**) and a details line with the receive frequency and, on DMR, the timeslot and color code. A dot at the start of the details line marks a temporary change |

With **Large text** on, both profiles show the zone, the channel name and the talk target in larger type, and leave
out the details.

During a DMR call, two lines at the bottom of the channel panel show who is calling (the talker alias when the radio
has it and **Settings > Call settings > Show talker alias** is on, else the contact's name or the ID) and the
talkgroup. While you transmit, they show where **PTT** talks to and, if a transmit timeout is set, the time remaining.
The same place shows what needs attention when the radio is idle, such as **No TX frequency**. The text above these
lines stays where it is.

The status bar at the top is the same on every screen, except that Home's leaves out the mode, which the mode bar
shows. When the status bar has more to show than fits, the items at the end of this list give way first, and none is
drawn over another: battery, TX or NO TX, signal, mode, lock, encryption key, scan, monitor, unread message, the GPS
clock, GPS, IP, power, receive-only, VOX, Dual Watch, roaming. The words (the mode, TX, IP, RX, VOX) move to a smaller
size before anything gives way. On Home, a GPS clock that the status bar has no room for moves to the mode bar.

The bottom bar has two labels, one over each soft key: **OK** and **Back** where they do something (on Home, **Menu**
and the function of the right soft key). While the keys are locked, Home's bar says how to unlock them instead. With
**Settings > Display > Hint row** on, the line above the bar shows what the bar can't: the other keys (**#**, **\***,
the number keys, **PTT**, a key you hold) and what **OK** and **Back** do when it isn't their usual job, such as
"OK save  Back cancel" while you change a value or "Back stop scan". With Hint row off, the bar stays and only that
line goes.

On a small screen (under 220 x 200 pixels), Home has no mode bar and no hint line. The status bar shows the mode, as
on every other screen, and the bottom bar keeps its two labels. The two lines at the bottom of the channel panel appear
only while they have something to show, and then take the place of the talk target and details lines, so the channel
name never moves.

The marks in the status bar:

| Place | Indicator | Meaning |
|---|---|---|
| Status bar, left | Signal bars | Reception is active. The bars clear when reception ends. In Dual Watch they follow the audible receiver |
| Status bar, left | Mode text | The channel mode, for example FM or DMR |
| Status bar, left | Ring with a dot | Roaming is active. The dot moves round the ring while the radio searches or surveys |
| Status bar, left | TX, NO TX | The radio is transmitting, or a transmission was refused |
| Status bar, left | Speaker with one wave | FM monitor or DMR digital monitor is on |
| Status bar, left | A and B, each in a box | Dual Watch is active. The filled box is the channel **PTT** uses |
| Status bar, left | VOX | Voice-operated transmit is armed |
| Status bar, right | Zigzag arrow | Scan is on. A small gap travels along the line and ends at the arrowhead |
| Status bar, right | RX | The channel is receive-only |
| Status bar, right | Key | DMR encryption is configured. An AES channel adds the **AES** label |
| Status bar, right | Dimmed key with a red slash | The radio hears encrypted traffic that does not match its configuration or that it cannot decrypt. A clear channel leaves the key slot blank while idle |
| Status bar, right | Envelope | An unread message is waiting |
| Status bar, right | IP | USB is a network adapter, brighter once the computer has connected to it |
| Status bar, right | GPS pin | The GPS receiver has a fix. A dimmed pin: GPS is on and searching |
| Status bar, right | Lock | The keys are locked |
| Status bar, right | L, M, H | Low, medium, or high transmit power |
| Status bar, right | Battery | Filled segments show charge. The icon changes color at the low battery threshold |

With **Settings > Display > GPS clock** on, the status bar shows the date and UTC time on every screen, where it has
room (on Home, in the mode bar when the status bar is full). The clock appears once a valid fix supplies date and
time, and goes when the fix is lost. Small screens have no room for it.
**Day/Night** switches every screen between dark text on a light background for daylight and light text on a dark
background for dark surroundings. See [Display settings](17-settings.md#display-key-1) for the defaults.

Dual Watch has its own two-channel view, and trunked channels keep their own operating display, under the same status
bar and bottom bar. The rest of this page describes Home in the Original look.

## Regions, top to bottom

| Region | Shows |
|---|---|
| Status line | Left: the state (IDLE, RX, TX, or NO TX when a transmission is refused; RX whenever the squelch is open, monitor included), the mode (DMR or FM), SCAN or MON when scan or monitor is on, and IP while USB is a network adapter, with an up and a down arrow after it once the computer has connected to it. Right: "L" when the keys are locked, "M" and a number for unread messages, a satellite dish on a radio with GPS (hollow while GPS is off, solid while it searches, with its feed arm and waves once it has a fix), the signal bars, the battery icon and the battery percentage. On a color screen the line is filled with the highlight color, its text black (Settings > Display > Filled status bar) |
| Place | The zone name and the channel's position in the zone ("3/16"). In the VFO: "VFO" |
| Channel | The channel number and name, as large as they fit. In the VFO: the frequency |
| Talk target | DMR: an arrow and where **PTT** goes, a talkgroup or person, as "TG 91 World"; a name too long to fit is cut ("TG 201 Main O…") before the name is dropped. FM channel: the frequency the channel listens on, then a repeater's offset when the row has room ("147.060 MHz +600k"), with no arrow. On FM, while you hold **PTT**, and in the VFO (where the large frequency is the receive frequency), an arrow and the transmit frequency. Trunked P25 channel: what the radio is doing ([P25 trunked channels](07-channels-zones-vfo.md#p25-trunked-channels)), and "RX only", in the warning color, while it waits on a system it can't transmit on. During the reply window: a return arrow in a filled box, and a countdown. To read the talkgroup's name first ("World TG 91"), turn on **Settings > Display > Talkgroup name first**, or give a side key **TG name / number** (Settings > Buttons) |
| Chips | DMR: timeslot, color code, "Rpt" on a repeater channel, power (the signal bars show when the repeater is heard). FM: tone, bandwidth, power, squelch level |
| Activity | Idle: the last station heard and how long ago, or "No calls heard yet". A setting the radio needs before it can transmit comes first, in the warning color ("Set: Board revision", or "Restart to apply"), with the hint row saying where to set it. Receiving or transmitting: see below |
| Hint row | What the nearest keys do now |

A field changed for now (a temporary change) is drawn in a highlight color on Home, and the hint row says
"Back undo  Hold OK save".

<img src="../ux/screens/160x120i5/home_temporary.png" width="200" alt="Home with a temporary power change">

The battery percentage turns to the warning color at the low battery warning level (15 % by default) and below.

## Hint row messages

The hint row changes with the situation. When the right soft key can be assigned on Home, the left label is
**Menu** and the right label shows its short-press function from **Settings > Buttons**. **Nothing** leaves the
right label blank. While entering a frequency or when the keys are locked, the row shows the controls for that state.
In the Panels look, the bottom bar shows only the labels of the two soft keys, and the hints below for the other keys
(**#**, **\***, the number keys) are on the line above it.

The most common hints:

| Hint | Meaning |
|---|---|
| OK Menu  \* Quick  # Joe | The normal hint. The name after # is the last caller. **#** opens actions for them |
| Back undo  Hold OK save | The channel has temporary changes |
| # caller  Back end reply | The reply window is running. **PTT** answers the last call |
| Back: channel's talkgroup | A talk target chosen for now is in use ("Back default target" where the row is narrow) |
| Digital monitor  Back off | Digital monitor is on |
| # skip  Back stop scan | Scan is running |
| No codec: voice off | DMR channel without the codec merged |
| 0-9 freq  \* Quick  Up/Dn | In the VFO |
| Keys locked: hold \* to unlock | The keys are locked ("Locked: hold \* to unlock" where the row is narrow) |

## Receiving

During a DMR call, Home shows the caller:

- the name, from your contacts or else from the DMR ID database in the codeplug;
- the DMR ID and the timeslot;
- the talker alias as it arrives, unless **Settings > Call settings > Show talker alias** is off;
- the talkgroup, or "Private to you";
- the signal strength (in DMR, the peak of the last 120 ms, so the empty timeslot doesn't show as a drop).

<img src="../ux/screens/160x120i5/rx_group.png" width="200" alt="Receiving a group call">
<img src="../ux/screens/160x120i5/rx_private.png" width="200" alt="Receiving a private call">

A call that the radio hears but doesn't play shows why. The reasons are "Other TG: not played" (a talkgroup you don't
listen to), "Encrypted: not played" and "No codec: voice off". Hold **Monitor** to hear a call on another talkgroup.

On FM, the activity region shows "Receiving" while the squelch is open, or "Squelch open" (monitor).

## Transmitting

While you hold **PTT**, the screen shows TALKING, the target, the timeslot and color code (DMR), the time on air and
the transmit timeout. DMR shows the time as "0:07 of 3:00", FM as "2:53 left of 3:00".

On DMR the top line says what kind of call it is: "TG 3100" for a group call, "Private 9991234" for a private call.
A private call shows the contact's name, and its ID when there is room. A name too long for the line keeps the name
and drops the ID.

<img src="../ux/screens/160x120i5/tx.png" width="200" alt="Transmitting a group call">
<img src="../ux/screens/160x120i5/tx_private.png" width="200" alt="Transmitting a private call">

On DMR the screen may first show WAITING (the radio waits for a free slot) or WAKING RPT (it wakes up the repeater).
While it wakes the repeater, the red LED blinks once for each wake-up signal it sends, and lights steady once you are
on the air.

The radio also transmits without PTT on DMR: registration (ARS), text messages, positions, call alerts and their
answers, and packet data from a computer on USB or serial. While it does, the status line shows TX, the red LED
lights and Home says SENDING. Data the radio receives shows RX on the status line.

When the radio refuses to transmit, the status line shows NO TX and the LEDs flash red, green, red, green, red, green
once with the deny tone. On a busy channel there is no tone, and the call you are hearing keeps playing. The screen says why, and the hint row says what to do:

| Message | Cause | What to do |
|---|---|---|
| This channel is RX only | The channel is set receive only | Pick another channel |
| Outside ... | The frequency is outside the radio's transmit bands | Pick another channel, or tune into a band in the VFO |
| TX time-out reached | You talked longer than the transmit timeout | Release **PTT** and press it again |
| No TX frequency set | The channel has no transmit frequency | Fix the channel in the codeplug |
| No codec: voice off | DMR voice without the codec | "Add codec with rfoundry" ([Installing](02-installing.md#no-codec-no-codec-voice-off)) |
| Channel busy | Someone else is talking on the slot | Wait for the call to end |
| No repeater answered | The repeater didn't wake up | "Check CC and range" |
| Hotspot mode: PTT off | The radio is a hotspot | Press **OK** to leave hotspot mode |
| No DMR ID in codeplug | The codeplug has no DMR ID | Set your ID in the codeplug |
| DMR modem not running | The DMR chip didn't start | Restart the radio. See [Troubleshooting](20-troubleshooting.md) |
| Set: Board revision (or another hardware option) | The radio can't detect it and won't transmit until it knows | Settings > Programming > Hardware ([Radios](23-radios.md) says what to pick) |
| Restart to apply | A hardware option was changed and applies from the next start | Switch the radio off and on |
| Radio refused to send | The radio's transmitter refused this transmission | "Release PTT, try again". If it repeats, restart the radio |
| Modem refused to send | The DMR chip refused the call, or took it and sent nothing for 1.5 s | "Release PTT, try again". If it repeats, restart the radio |

The low and critical battery levels also limit transmitting. See [Battery and power](16-battery-and-power.md).

## Large text

**Settings > Display > Large text** shows only the status line, the channel name and the talk target, each as large as
it fits. The radio keeps this setting across power off. It keeps **Settings > Display > Hint row** too.

<img src="../ux/screens/160x120i5/home_large_text.png" width="200" alt="Home in large text">
