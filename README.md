# Lucky Circuit

A native Game Boy Advance homebrew casino game. It includes European roulette, blackjack, slots, and dice. The games use play credits only; credits have no cash value.

The compiled ROM is [`lucky-circuit.gba`](lucky-circuit.gba). Open it in a GBA emulator.

## Controls

- D-pad: navigate the game menu; adjust roulette number or dice target.
- A: choose, spin, roll, deal, or hit.
- B: toggle roulette number/color bet or stand in blackjack.
- L/R: decrease/increase wager by 5 credits.
- Select: return to the game menu.

The game starts with 1,000 credits and a 25-credit wager.

## Build

Requires Clang and LLD. Run `make`; the ROM is written to the project root. If LLD is not on `PATH`, set `GBA_LD` to its executable path.