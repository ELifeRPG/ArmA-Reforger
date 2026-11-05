---
icon: lucide/link
---

# Account linking — what's left to build

The REST contract for this is in place; the flow that uses it is not. This page is the handover.

!!! note "Depends on an open PR"

    The fields named below arrive with
    [#12](https://github.com/ELifeRPG/ArmA-Reforger/pull/12), which regenerates the client against
    the Bridge's current spec. Everything else in the chain (Central API, Bridge, Keycloak, portal)
    is already merged and deployed.

## What already works

Everything outside the mod. A player joins, the Bridge creates an account for their Game Identity if
nobody has seen it, and answers with a PIN and a URL that links that identity to a web account. The
Keycloak form prefills from the URL, the portal shows link status and accepts further platforms.

In the mod, the generated client can call all of it — `ELIFE_Api.PlayerConnected()`,
`CharacterSelected()`, `PlayerDisconnected()` — and the structs carry every field.

## What's missing

**Nothing calls any of it.** `ELIFE_GameMode` overrides only `OnGameStart()`; no script references
`PlayerConnected`. There is no session, no character select, and no link prompt.

**Character identity is a placeholder.** `ELIFE_CharacterIdentity.GetCharacterId()` returns a fixed
demo UUID, the same one for every player. The phone feature already depends on it, so the session
flow is what finally makes it real — and anything keyed on `characterId` today is shared between all
players until it does.

## The flow to build

1. **Hook the connect.** Override the player-connected hook on `ELIFE_GameMode` (it extends
   `SCR_BaseGameMode`) and call `PlayerConnected` with the player's Game Identity. Prefer the hook
   that fires after the engine has confirmed the player's identity, not the earliest one — a request
   sent before that has nothing useful to send.
2. **Read the identity.** `GetGame().GetPlayerManager()` is the way in; see
   `ELIFE_CharacterIdentity` for the pattern. The Game Identity is per *game-platform* combination,
   so one player across PC, Xbox and PlayStation has three — never cache it as "the player".
3. **Send `platform` if you can get it.** Optional, descriptive only. The Bridge records it on the
   link for staff and ignores a value it does not recognise, so a wrong guess is harmless and
   omitting it costs nothing.
4. **Store the response per player.** `accountId` and `characters` drive character select;
   `webAccountLinked`, `linkPin` and `linkUrl` drive the link prompt.
5. **Act on `status`** — `active` plays, `blocked` and `not_whitelisted` do not, `unlinked` means
   retry (see below). Treat an unknown value as "let them in and log it"; the Bridge adds values.
6. **Show the link prompt** when `webAccountLinked` is false.
7. **Replace `ELIFE_CharacterIdentity.GetCharacterId()`** with the selected character, and call
   `CharacterSelected` when the player picks one and `PlayerDisconnected` when they leave.

## Three things that will bite you

**`webAccountLinked`, not `status`, decides whether to show the link.** A player who has never
touched the portal reads `active` and plays normally — that is the normal join-first state, not an
error. Branch on `status` and the prompt never appears for anyone who needs it.

**`linkUrl` is a bearer credential** for that game identity for as long as the PIN lives (30
minutes). Anyone who gets the URL can attach that identity to *their* web account. Show it on
demand, offer copy-to-clipboard, and don't leave it on screen or in a shared log.

**`unlinked` does not mean "new player".** Joining creates the account now. It means the Bridge
could not resolve the identity at all and deliberately created nothing rather than risk a second
account for one player. Retry; don't treat it as a first-join state.

## Unknowns worth settling first

**Is a Game Identity a UUID on Xbox and PlayStation?** `gameIdentityId` is `format: uuid` across the
whole chain, proven on PC only. The script API types identity as a plain string. If a console
identity is not a UUID, the type has to change in all five repos — so **join once from a console
before building much on top of this.**

**The platform enum members are unconfirmed.** `GetPlatformKind` is the documented source, with
config-level counterparts `PLATFORM_PC` / `PLATFORM_XBL` / `PLATFORM_PSN`, but the exact script enum
names have not been checked against the engine. Map what you find to `"Pc"`, `"Xbox"`,
`"PlayStation"`, and send nothing rather than a guess — the field is optional for this reason.

## Engine notes

The generated client's conventions — handler signatures, why handlers must be public, callback
lifetime, bare-array responses, the `1`/`0` bool packing — are in
[Backend API client](backend-api.md). Read that before writing the first call; the arity rule in
particular fails at runtime, not compile time.
