<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# GUI profiles implementation plan

> Use Superpowers task-level tests and review before launching the local simulator.

**Goal** Bring the approved 310 × 240 Home prototype into the native radio UI and make the complete menus explorable in a local simulator.

**Design** See [approved wireframe](../../ux/wireframes/gd88-home-profiles.html). Use shared UI code for both the radio and host. Enable the presentation by board capability and use the right-inset display preset in the interactive simulator. Preserve existing menu behavior and non-profile rendering.

- [x] Add a native interactive driver and a board-matching simulator geometry. Test persistent navigation, error recovery, frame output, and the legacy geometry.
- [x] Add persisted GUI profile, Day/Night, and GPS clock settings. Test saved settings and a clock that appears only while a valid GPS date and time are available.
- [x] Render the approved Home, status indicators, and palettes in the shared graphics code. Test settings navigation, content density, alias rendering, and crowded status scenarios.
- [x] Serve the native frames and controls locally. Test loopback navigation, simulated events, and terminal process cleanup.
- [x] Run the host UI tests, layout checks, and integration tests. Open the simulator locally and verify its starting frame and native input path.

The simulator extends existing fixture capabilities. Bluetooth, D-STAR, user recording, and the encryption artwork can be previewed with mock data when the relevant physical feature is unavailable. Use no connected radios or RF.
