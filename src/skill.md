---
name: corbel
description: |
  Work with Corbel construction punch lists (.corbel files): list, add, and
  update defects pinned on floor plans, import plans, and export the closeout
  report PDF. Use for any request about a punch list, snag list, defect list,
  or a .corbel project.
---

# Corbel

A Corbel project is one `.corbel` file (SQLite) holding floor plans, the items
pinned on them, and their photos. Change it only through the `corbel` command;
none of its commands need a display.

## Commands

- `corbel new site.corbel --name "Harbor St" --plan plans.pdf` creates a
  project; every PDF page becomes a floor.
- `corbel import site.corbel level2.png` adds more floors.
- `corbel plans site.corbel --json` lists floors with their ids.
- `corbel items site.corbel --json [--status open|progress|done] [--trade T]
  [--assignee A] [--floor ID]` lists items.
- `corbel add site.corbel --title "Cracked tile" --floor 1 --x 0.42 --y 0.61
  --trade Tiling --assignee "Acme" --due 2026-11-30` adds an item. `--x` and
  `--y` are fractions of the plan's width and height (0 to 1); without them
  the pin goes in the middle.
- `corbel set site.corbel 12 --status done` changes item 12 (any option from
  `add` works).
- `corbel export site.corbel report.pdf` writes the closeout report.

Every command takes `--json`, exits 0 on success and 1 on failure, and writes
errors to stderr. `corbel <command> --help` lists its options.

## Working with the person

- If they have the project open in the app, it shows your changes the next
  time it is reopened; tell them to reopen it.
- `corbel open site.corbel` opens the app for them; it needs their desktop, so
  do not wait on it.
- Item numbers are what people refer to ("#12"); use them with `set`.
