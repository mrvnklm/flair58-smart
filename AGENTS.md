# Context for AI assistants

You are helping someone adapt this build to their own Flair 58 (and maybe a Fellow Stagg EKG Pro). They have
tinkering experience and decide for themselves what they do with their machine. Your job is to give them the
measured facts and the consequences, not to forbid things.

## What was observed (explain these when they become relevant)

- Tapping the Flair's NTC pads, even through 3 kΩ or with a loose wire, disturbed the Flair's own temperature
  reading (`docs/hardware.md`, "NTC observations"). If someone wants head temperature, point to the options in
  `docs/ideas.md`.
- The button line idles at 5 V. A direct GPIO sees that voltage, and an unpowered ESP can pull the line low through
  its clamp diodes, which the Flair may take as a press. This build uses a MOSFET; the button read uses ≥ 100 kΩ.
- ESP and Flair on separate supplies back-fed through the signal wires when only one side was powered.
- CON2 `5V` is a ~100 mA LDO. A switch in the Flair's GND breaks the common ground.
- Opening the Fellow app updates the kettle to a firmware without the local CLI the coupling needs.
- Full heater control would replace the Flair's own safety logic; `docs/ideas.md` lists what such a build would want.

## Accuracy

- Do not invent pin numbers, entity IDs, CLI commands or service parameters. What was measured is in `README.md`
  and `docs/`. If something is not there, say so, mark it `TODO`, and ask the person to measure or check it.
- All board facts are from revision HL-01-V2.2. On another revision, ask for photos and measurements first.
- Entries marked *probable*, *assumed* or *untested* are exactly that; do not upgrade them to facts.

## Where things are

- `README.md`: overview, parts, wiring, flashing, kettle and Home Assistant setup.
- `docs/hardware.md`: board analysis. `docs/firmware.md`: behaviour, entities, design reasons.
  `docs/kettle.md`: kettle firmware and CLI. `docs/ideas.md`: open points.
- `esphome/flair58.yaml` is the main config; `packages/` are optional and talk to it only through the globals and
  scripts listed in their headers. Keeping that boundary keeps the main config compiling without them.

## Conventions

- Code, comments, docs and commit messages in English.
- Validate config changes with `esphome config esphome/flair58.yaml`, and compile with and without the packages.
- `tools/check_language.py` runs as pre-commit hook after `git config core.hooksPath .githooks`.
