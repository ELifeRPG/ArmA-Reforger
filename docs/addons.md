# Addons and publishing

ELifeRPG is published as several Workshop addons instead of one. This keeps every addon well
below the Workshop limits and lets the large, rarely changing art live apart from the code.

## Layout

| Addon | GUID | Contains | Depends on |
| --- | --- | --- | --- |
| `ELifeRPG_Items` | `14E46A965EE53285` | Art for general items: food (apple, plum), phone mesh, materials, screen textures and UI sounds | vanilla |
| `ELifeRPG_Vehicles` | `4B054C78918EAA22` | Car key art, vehicle lock sounds | vanilla |
| `ELifeRPG_Vegetation` | `12BC1DE093734B8A` | Fruit tree materials and polyplanes | vanilla |
| `ELifeRPG_Core` | `66EB6BA548DD28C2` | **All** scripts, configs, UI, localization and prefabs | vanilla + all asset addons |
| `ELifeRPG` | `596AFBF40E2150FD` | Nothing. Meta-pack that pulls in the whole stack | all of the above |
| `ELifeRPG_Dev` | `B23160BEBCAB2D32` | `DevWorld` and test layers. Published unlisted for internal servers | `ELifeRPG` |

Servers list a single mod, `ELifeRPG` (`596AFBF40E2150FD`). It's the GUID the original single
addon had, so existing server configs keep working and get the new addons through dependencies.

**Asset addons** are named after their topic (`ELifeRPG_Items`, `ELifeRPG_Weapons_Glock`, …) and hold
only art. Every addon except `ELifeRPG`, `ELifeRPG_Core`, `ELifeRPG_Dev`, `ELifeRPG_Map_*` and
`ELifeRPG_Patches_*` is an asset addon.

Coming later, following the same rules:

- `ELifeRPG_<Topic>`: one asset addon per new art topic (`ELifeRPG_Clothing`, `ELifeRPG_Buildings`, …).
  Weapons split per family right away: `ELifeRPG_Weapons_Glock`, `ELifeRPG_Weapons_M4`.
- `ELifeRPG_<Topic>_Common` / `ELifeRPG_Common`: shared art, see rule 4.
- `ELifeRPG_Map_<Name>`: terrain and world. Depends on `ELifeRPG_Core`.
- `ELifeRPG_Patches_<Mod>`: compatibility with one third-party mod. This is the only kind of addon
  besides Core that may contain scripts, because it has to depend on that mod. It is not part of the
  meta-pack. Servers add the ones they need.

## Rules

1. **Dependencies only point one way:** asset addons ← `Core` ← `Map_*` / `Patches_*` ← `ELifeRPG` ← `Dev`.
2. **Scripts, configs, UI, localization and prefabs go into `ELifeRPG_Core`.** Prefabs carry script
   components and are referenced by the game mode and by scripts, so they have to live next to the code.
3. **Raw art goes into the asset addon for its topic:** meshes (`.xob`, `.fbx`, `.txo`),
   materials and textures (`.emat`, `.edds`), sounds (`.wav`, `.acp`).
4. **Shared art goes into a Common addon:**
   - Shared within one topic → `ELifeRPG_<Topic>_Common`, e.g. `ELifeRPG_Weapons_Common` for
     attachments, magazines, materials or sounds used by several weapon families. The family addons
     depend on it.
   - Shared across topics → `ELifeRPG_Common`.
   - An asset addon depends only on vanilla and on Common addons, never on a sibling family and never
     on Core. Core depends on every asset addon, including the Common ones.
   - Create a Common addon when the second addon needs the same file, not earlier, and move
     the file there following rule 5.
5. **Keep the internal path when moving a file between addons**, and move its `.meta` with it.
   References are `{GUID}path`, and the GUID lives in the `.meta`. With the same path and the `.meta`
   moved along, every reference still resolves.
6. Vanilla full-file overrides (`Configs/System/chimeraMenus.conf`, `chimeraInputCommon.conf`) may
   exist in only one addon: `ELifeRPG_Core`.
7. Source files (`.blend`, `.obj`, …) stay in the repo-level `assets/` folder and are never shipped.

To check rules 1–5 (unresolved or wrong-direction references, duplicate GUIDs, logic in an asset
addon, asset addons depending on anything but Common addons):

```sh
python3 tools/check_addons.py addons
```

## Size budget

- The Workshop accepts up to **7 GB per addon** (raised from 5 GB in 1.2.1.189). Keep each addon
  under ~3 GB. Split an asset topic by family before it gets there.
- No per-file limit is documented, but big uploads tend to time out, so terrain gets its own addon.
- Consoles have a total mod storage budget (PS5 25 GB, Xbox 50 GB) and download at about 3 MB/s.
  Keep the required stack small.

## Releasing

All ELifeRPG addons share **one version**, taken from `VERSION` at the repository root. Every
release republishes every addon with that version. The Workshop downloads patches between versions,
so players fetch next to nothing for addons whose content didn't change.

1. Bump `VERSION` (`MAJOR` = breaking: removed/renamed prefabs or assets, backend protocol;
   `MINOR` = features and content; `PATCH` = fixes) and merge it to `main`.
2. Tag the merge commit with the change notes as the tag message, and push the tag:

   ```sh
   git tag -a v1.2.0 -m "Phone: new banking app"
   git push origin v1.2.0
   ```

3. The **Publish to Workshop** workflow (`.github/workflows/publish.yml`) runs
   `tools/check_addons.py`, checks that the tag matches `VERSION`, and then runs `tools/publish.ps1`
   on the publishing runner. The script packs and publishes every addon in dependency order (derived
   from the `.gproj` dependencies: asset addons → `ELifeRPG_Core` → `ELifeRPG` → `ELifeRPG_Dev`) and
   stops at the first error. Workbench logs are attached to the run as the `workbench-logs` artifact.

To check that everything packs without publishing anything, start the workflow by hand (*Run workflow*,
`pack_only` checked). As a manual fallback, run `pwsh tools/publish.ps1` on the runner machine.

A **new addon** has to be published once through the Workbench GUI (*Publish Project*), because
title, summary and description can't be set from the command line. After that, the workflow updates it.

Servers either leave mod versions unpinned (always the latest) or pin every ELifeRPG addon to the
same release number. The Workshop keeps only the last 50 versions of an addon.

## Working in Workbench

Register every folder under `addons/` as an addon directory, then open the addon you work in:
`ELifeRPG_Dev` to play the test world, or `ELifeRPG_Core` for scripts and prefabs. When you create
new art, save it inside the matching asset addon so its `.meta` is created there.

## Publishing runner (one-time setup)

Publishing goes through the Workbench command line. That only runs on Windows, and Workbench takes
its Bohemia account session from the logged-in Steam client. There is no public Workshop API.
The workflow therefore runs on a self-hosted Windows machine:

- Steam logged in with the account that owns the Workshop items, plus Arma Reforger Tools installed.
  Log into Workbench once and do the first GUI publish of each addon there.
- A [GitHub Actions runner](https://docs.github.com/en/actions/hosting-your-own-runners) with the
  labels `self-hosted, windows, reforger`, **running as that Windows user** (interactively or as a
  service under that account, not as LocalSystem). Otherwise it can't see the Steam session.
- PowerShell 7 (`pwsh`) and Git on `PATH`.
- Two environment variables for the runner (e.g. in the runner's `.env` file):
  - `REFORGER_WORKBENCH`: full path to `ArmaReforgerWorkbenchSteamDiag.exe`
  - `REFORGER_ADDONS`: the vanilla `addons` folder of the Tools/game install
- At least 3 GB of free RAM. Don't run the game while publishing (Workbench hangs if it does).
- Optional: in the repository settings, add required reviewers to the `workshop` environment so every
  publish needs an approval.

