# Compatibility matrix

The owner's games (the ISO folder of the test Memory Stick, 2026-10-17) and how States behaves in
each. Part of the v0.1.0 gate ([roadmap M6](roadmap.md#m6--hardening-v010)); the protocol per
column is in the [testing guide](testing.md). A cell is **VERIFIED** only with a log or the
owner's report; empty means untested.

Columns: **Slot** = M2 save to a slot in gameplay; **Load** = M5 load from the card in the same
session; **Reboot** = T3 load after Quit Game and a restart; **10×** = ten loads of one slot in a
row; **Busy** = save during sound / an in-game menu / a loading screen; **Quit** = Quit Game
afterwards returns to the XMB.

| Game (file name) | Format | Slot | Load | Reboot | 10× | Busy | Quit | Notes |
|---|---|---|---|---|---|---|---|---|
| Assassin's Creed: Bloodlines (USA) | ISO | | | | | | | |
| Castlevania: The Dracula X Chronicles (USA) | ISO | | | | | | | RAM save/load VERIFIED (E2, speed runs 1-7, 2026-10-16); T1 passed here |
| Crisis Core: Final Fantasy VII (Europe) | ISO | | | | | | | |
| Dante's Inferno (USA) v1.01 | ISO | | | | | | | |
| Daxter (USA, PSN) | ISO | | | | | | | |
| Eiyuu Densetsu: Ao no Kiseki (English v15) | ISO | | | | | | | fan translation |
| God of War: Chains of Olympus (USA) | ISO | | | | | | | |
| God of War: Ghost of Sparta (USA) | ISO | | | | | | | |
| The Godfather: Mob Wars (USA) | ISO | | | | | | | |
| Gran Turismo (USA) | ISO | | | | | | | |
| Kingdom Hearts: Birth by Sleep (USA) | ISO | | | | | | | |
| Metal Gear Solid: Peace Walker | CSO | | | | | | | compressed ISO (Inferno decompresses) |
| Monster Hunter Freedom Unite (USA) | ISO | | | | | | | |
| Need for Speed: Most Wanted 5-1-0 (USA) | ISO | | | | | | | |
| Persona 2: Innocent Sin (USA) | ISO | | | | | | | |
| Prince of Persia: Revelations (USA) | ISO | | | | | | | |
| Shin Megami Tensei: Persona (USA) | ISO | | | | | | | |
| Silent Hill: Shattered Memories (USA, PSN) | ISO | | | | | | | |
| Tekken 6 (USA) | ISO | | | | | | | |
| The 3rd Birthday (EUR, undub) | ISO | | | | | | | |
| The Legend of Heroes: Trails in the Sky (USA) | ISO | | | | | | | |
| The Sims 2 (USA) | ISO | | | | | | | |

What to write in a cell: `ok`, `fail` (with a note: black screen, back to the XMB, wrong picture,
no sound, the game hangs later), or `refused` with the message shown (a guard or "another setup").
The v0.1.0 gate needs five of these games at 100 cycles each (testing guide section 6, T7).
