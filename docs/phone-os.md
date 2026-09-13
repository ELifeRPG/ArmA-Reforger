# ELIFE phone OS — UI rules

In-game phone for an Arma Reforger life mod. Covers lock, home, the shared app shell
(title, list, detail, compose, loading), a phone-wide offline screen, and Settings.
Apps: Messages, Contacts, Banking, Map, Settings. A sixth app must fit without a
redesign.

Output is Enfusion `.layout` and Script under `src/UI/layouts/Menus/Phone/` and
`src/Scripts/Game/UI/Menu/Phone/`. Tokens live in `ELIFE_PhoneStyle`. Apps subclass
`ELIFE_PhoneAppBase` and are hosted by `ELIFE_PhoneScreenShell`. Express everything
in the widgets those files already use (`OverlayWidget`, `SmartPanelWidget`,
`ImageWidget`, `SizeLayoutWidget`, `TextWidget`, `ScrollLayoutWidget`).

## Extending — a sixth app

Do not start a parallel shell. Hook the existing one:

1. Add an `EPhoneScreenState` and a pair of accents in `ELIFE_PhoneStyle`
   (`Accent*` / `AccentDeep*`, wired through `AccentFor` / `AccentDeepFor`). Same
   lightness as the others (L\* ≈ 72 / 52), only the hue rotates.
2. Register the tile in `ELIFE_PhoneScreenShell.BuildApps()` (home-grid order,
   rows of four). Sprite names from [`docs/icons_wrapperUI.md`](icons_wrapperUI.md)
   or `ICON_SET_CHAT`. `FitIcon` the glyph — wrapper cells are not square.
3. If it has an in-phone page: subclass `ELIFE_PhoneAppBase`, return it from
   `CreateApp()`, list the state in `HasAppPage()`. Map does not — it hands off
   to the fullscreen map, and the screen behind it stays home.
4. Implement `GetTitle`, `GetScreenState`, `CreateRoot`, `OnOpened` / `OnClosing`.
   Claim the trailing slot with `ShowNavAction` if the page has one named action;
   it is reset hidden on every bind.
5. Internal pages use `GetSubState` / `ApplySubState` / `NotifySubStateChanged` so
   the world RT stays on the same page as the menu. Handing the phone to another
   app is `OpenAppPage(state, subState)` — Contacts → Messages already does this.
   Back visibility is automatic: the shell hides it whenever `GetSubState()` is
   `""` (`IsAtRoot()`), so the root page needs no Back handling of its own.
6. Strings go through localization (`ELIFE_Localization.st` + `en_us` / `de_de`).
   No hardcoded UI copy.

## Canvases

One authored screen: **252 × 523**. The in-hand menu wraps it in a symmetric 5px
frame (262 × 533 overall). The world render target is the same 252 × 523 with no
frame. Enfusion cannot scale a widget tree — do not author a second size set.

| When | Do |
|---|---|
| The owner must operate it | It cannot depend on hover. Cosmetic hover (a tint under the cursor) is allowed. |
| Type carries meaning | 9px minimum. Nothing smaller. |
| World RT vs menu | Same screen state, content, and data. Presentational polish on the menu does not need an RT twin. |

## Colour space

Enfusion `Color` channels are **linear light, not sRGB**. A raw sRGB triple renders
too bright and washed out. Design in sRGB; convert with `ELIFE_PhoneStyle.Srgb()` in
script. Layout literals are the already-converted linear values for those tokens.
Never type sRGB into a `Color`.

## Glass — when it is glass

Glass means "this layer floats above that one and is lit by it."

| When | Do |
|---|---|
| The surface sits over real content or the wallpaper, and that content can change | Glass. |
| Nothing is behind it | Solid. Do not add decoration just to give glass something to show. |
| Page content: list cards, settings cards, form cards | Dark glass (`ApplyGlass(bar, true)`). Status and nav pass `chrome: true` for a heavier scrim over scrolling content — same look, not a third one. |
| Controls that float at rest: home cards, lock notifications, PIN keys, compose bar, trailing nav pill | Light glass (`ApplyGlass(bar, false, true)`). |
| Home app tiles, home card badges | Accent glass (`ApplyGlass(bar, false, false, false, true, accentColor)`) — the one place a surface is tinted per-app rather than dark or light. Pass that app's `AccentDeep*`. |
| Back, large titles, app-page grounds | No glass. |
| Message bubbles | Decided: glass, not solid. Inbound is dark glass (`ApplyGlass(bubble, true)`); outbound is accent glass in the sender's own accent (`ApplyGlass(bubble, false, false, false, true, AccentDeepFor(state))`) — no `GlassGlow`, the coloured content is the bubble text itself, not an icon sitting on top of it. |

Three looks: dark, light, accent. Flags are mutually exclusive — dark wins over
light wins over accent. Do not invent a sheet look, a chrome look, or a fourth
per-app recipe beyond these three.

Hard limits:

- At most **two** glass layers stacked. Three is mud.
- Text on glass needs a scrim or a solid chip.
- **Alpha is the material** (fixed per look). **Opacity is presence** (the whole bar,
  so tint, specular, and hairline stay one pane). Chrome bars earn Opacity from
  scroll (`ApplyCollapse`). Every other glass surface must get `Opacity 1` from
  `ApplyGlass`. Editing the three alphas to fade a bar pulls the material apart.
  `GlassGlow`, where present, has its own alpha precisely because it is a separate
  layer, not part of that one pane — fading it independently is not the same
  mistake.
- One boolean (`GLASS_ENABLED`) swaps every stack for a flat panel at the same
  lightness.

Recipe, not a fourth look: baked blurred wallpaper as refraction (or tint-only if
alignment is impossible), a near-white tint with no hue of its own, a top specular
and a 1px bottom hairline. Tint and specular each come in a dark and a light step;
accent glass borrows the dark tint but the light specular, so both are always named
per step (`GlassTintDark()`, `GlassSpecularLight()`) rather than a default plus an
override. Specular still stretches the pane; `ApplyGlass` sets its height to
`GLASS_SPECULAR_HEIGHT` (2) on every look — dark, light, and accent. A 1px bar
reads as a stroke. Do not invent a min-width or centre it. Any new glass widget
must carry the layer names `ApplyGlass` looks up — `GlassBlur`, `GlassScrim`,
`GlassTint`, `GlassSpecular`, `GlassHairline` — clipped to `rounded_6px`. Accent
glass is the same recipe with one substitution: the scrim mixes toward the app's
own accent colour instead of a fixed neutral (`GlassBaseDark()` for dark,
`GlassBaseLight()` for light). The tint is never recoloured with the app hue.

A widget may additionally carry `GlassGlow`: a layer stacked above the content
(the icon or glyph) instead of below it, painted with the accent colour at its
own alpha, independent of the scrim/tint alphas. Only declare it where the
accent is meant to read as part of the content itself rather than its backdrop —
today that is only the home screen's app tiles and card badges. A widget with no
`GlassGlow` child simply has no glow; accent glass on it behaves exactly like
dark/light, just recoloured on the scrim.

Radii: `STYLE_RADIUS_ELEMENT` / `STYLE_RADIUS_SCREEN` are `rounded_6px` (engine
maximum for the case, cards, chips, badges, keys). `STYLE_RADIUS_DETAIL` is
`rounded_2px` for small inner details a 6px arc would swallow. Reforger ships
nothing wider. Never fake a round corner with overlapping rectangles.

Altitude is tint lightness + hairline strength, not blur. Three altitudes: content,
chrome, overlay.

## Gradients

A gradient is a **ground**, never decoration.

| When | Do |
|---|---|
| Lock or home wallpaper | One dithered ramp from `UI/Textures/Common/`, OS anchor hue, direction from `GRADIENT_FLIPPED` only. |
| App pages, lists, cards, forms, value columns | Flat. No header wash. Identity is accent on type and controls. |

## Accent

Each app owns **two** steps of one hue, declared together: `Accent*` (text, badges,
actions) and `AccentDeep*` (filled surfaces it owns — tile, person marks, outbound
bubbles). Same lightness per step; only hue rotates. Settings is the most
desaturated. Neutrals stay neutral, tinted toward the OS anchor.

| When | Colour |
|---|---|
| Title controls, selected/active, badges, the page's one named action | This app's `Accent*` |
| The one figure the app is *about* (Bank balances; Messages unread) | That app's `Accent*` |
| A filled surface this app owns | `AccentDeep*` |
| Names, previews, kinds, captions | Neutral (`TextPrimary`). Light glass still reads mid-dark — `Ink` is the wrong register, `TextSecondary` sits too close to the pane. |
| A control that **hands the phone to another app** | The **destination** accent, on that control only. Chrome and title stay the host's. Never invent a third hue. |
| Status bar | OS-owned. Never app-tinted. |
| Home, lock, off | `AccentFor` falls through to Settings (the anchor neutral). Do not invent an OS hue. |

Colouring every label to warm up a card is how accent stops meaning anything.

## Marks — what the row *is*

Do not give a thing a face so it matches Contacts. Do not put a person in a tile so
they match home.

| Subject | Mark |
|---|---|
| A person (contact, thread, picker, hero) | Circle + initials, this app's `AccentDeep` |
| A door (hands the phone to another app) | Smaller circle + glyph, **destination** accent, label under it |
| An app | Rounded square + glyph |
| A thing (account, transaction, setting) | No portrait. Name and value. Kind is the group. |

A door is not a portrait: glyph, not initials. It is not a commit: do not use it
for Save, Send, or Add. Contacts' Message is one. A colour that is not this
page's hue can only mean where the tap goes.

**How a circle is drawn:** `PaintAvatar` on `ELIFE_PhoneAppBase` — atlas sprite
`circle` (`ICON_PERSON_MARK`) via `ImageWidget.LoadImageFromSet()`, initials on
top. A SmartPanel fill is a rounded square, not a circle. Do not point `Texture`
at `UI/Textures/Common/circleFull.edds` or
`UI/Textures/RadialMenu/RadialMenuMaskInverse.edds` — those are shader masks and
draw as opaque squares. List marks are `AVATAR_SIZE` (28), initials at 12px —
enough padding inside the circle that the glyph doesn't hug the edge.

## Actions — slot, then cost

A circle-as-control is not a circle-as-portrait. One leading edge and one trailing
slot per screen. No floating + over a list.

| Where | Look | When |
|---|---|---|
| Leading (Back, close) | Icon or accent word. No glass pill. | Reversible. |
| Trailing nav slot | A **word** on a light-glass pill. A leading icon is allowed (Add's plus, Save's check). | This page's one named action. Hide it when that page is not showing. |
| In the page (compose send, PIN key) | The page owns it. A write is still a **word** on that surface, with a hover wash on its hit area — not a person-circle, not a second chip beside the bar. | Do not also fill the trailing slot. |
| A door | The door mark above. | Hand-off only. Do not also fill the trailing slot. |

Decide in order:

1. Which slot? Leading = not a pill. Trailing = a word. In-page = that surface.
2. Is this page still showing? If not, hide the trailing pill.
3. Would a wrong tap write state? Then it must be a word. Trailing is a word anyway.
4. Does this icon mean the same thing in many places? Then it may be icon-only
   (Back). If this screen is the only place it would mean this, it is a word.

Back and the trailing label are a **pair of words**: caption size (`TEXT_CAPTION` /
9), regular, not bold. The pill is a chip the same hold as Back, not the full
nav bar; width hugs the label. A leading icon sits at that same size — it marks
the action, it does not replace the word or enlarge the control. Do not glass
Back to match the pill. A bare `+` in that corner, with no word, is the floating
action we already pulled.

`ShowNavAction` is shell infrastructure: one pill, reset hidden on every app bind,
width hugging the localized label (`GetTextSize` + padding). Relabel with
`SetNavActionLabel` (does not re-enable a disabled button). `SetNavActionEnabled`
for in-flight saves. `SetGlassPresence` fades the pill once the nav bar glazes so
the word sits on the bar. A sixth app claims the slot the same way.

Button children use `ButtonWidgetSlot`. The fill is a sibling in a wrapping frame,
not a child of the button.

A `ButtonWidgetClass` is never bare. Without `style blank` it draws the engine's
default button texture — an opaque white quad over whatever it covers, with no
error. Every clickable surface here carries the same three parts: `style blank`,
an `SCR_ButtonTextComponent` holding the hover/pressed washes, and a
`ButtonSurface` child on `ButtonWidgetSlot` with a transparent `Background`.
Copy `CardButton` in `PhoneHomeCard.layout`; do not hand-roll a new one.

## Lists

Any grouped index uses `CreateListGroup` / `CreateListRow` on `ELIFE_PhoneAppBase`.
Do not re-author `PhoneListGroup.layout` per app.

**Host.** An empty `VerticalLayoutWidget` under the scroll body (`ContactGroups`,
`AccountGroups`, `ThreadGroups`). Content inset `Padding 12 0 12 30`. The layout
holds no section widgets; groups are created at fill time.

**Fill.** `ClearChildren` the host, then walk the data in order. Open a new group
when the section key changes. `isFirst` is `true` only for the first group —
later groups get `LIST_GROUP_GAP` (6) above them. Append rows to the returned
`GroupList`, not to the host.

```
currentList = CreateListGroup(host, headerText, isFirstGroup);
Widget row = CreateListRow(LAYOUT_ROW, currentList, false);
ELIFE_PhoneStyle.ApplyGlass(row, true);
```

`CreateListRow(..., isLast)` only hides `RowHairline`. Glass cards have none, so
`isLast` is always `false` there. `CreateListRow` stretches horizontally; do not
use `LayoutSizeMode.Fill` or leftover height is divided across rows. The 2px gap
between cards is the row overlay's own bottom padding.

| When | Do |
|---|---|
| Grouped index (Bank kinds, Contacts letters, Messages day buckets) | Dark-glass cards under a left-aligned accent caption. No hairline on a glass card. |
| Authored cards (Settings groups, contact number/detail) | Dark glass, same look, not via `CreateListGroup`. Hairlines, when present, start at the text, not the screen edge. |
| Ledgers (Bank statement) | Hairline rows, no glass. Amounts use `Positive()` / `Negative()` — the only semantic colour pair besides accents. No edge stripe. |
| Contact form | Mirrors detail: circle hero, grounded name field, one number card. No name card. |

Do not mix glass cards and hairline rows on the same list. Section headers are not
pills. Height follows that treatment, not how many strings are on the row: a
glass card stays a glass card when the subtitle goes; a hairline row stays a
hairline row when it has two lines.

**The group header already answered one question. The row does not ask it again.**

| Header is… | Row shows |
|---|---|
| A kind (Personal / Company) | Name + value. Kind belongs on the statement, where there is no group. |
| A specific day (Today / Yesterday) | Time only (`FormatClock`). |
| A span (Last 7 days, Last month, Older) | Day + time (`FormatDayTime`). |
| A letter (A–Z) | Name + number. The letter is not repeated on the row. |

A second line is only legal when it is new information (a number, a preview). Echoing
the header is not information.

**Times live on the thing they stamp.** The thread list stamps last activity on the
card. The chat stamps each bubble. The name under the large title is who you are
talking to. Leave that subtitle empty. Do not invent last-seen.

API timestamps are UTC ISO and shown as-is: `FormatClock` → `14:32`,
`FormatDayTime` → `29.08 14:32`.

## Events, data, names

| When | Do |
|---|---|
| A field changes as the user types | `SCR_EventHandlerComponent.GetOnChange()`. Never override `OnChange` on `ScriptedWidgetEventHandler` — reserved engine event, hard compile error. |
| A field commits (Enter) | `GetOnChangeFinal()` on the same component. |
| The page needs backend data | `m_Phone.RequestData(DATA_*)`, then `OnDataChanged` / `ApplyDataStatus`. Loading skeleton via the shared status overlay; no per-app error card (offline is phone-wide). |
| A name or number is shown | `ELIFE_PhoneContactBook` (`NameFor`, `TitleFor`, `FindById`, …). One resolution everywhere — home cards, lock, Messages, Contacts. |
| A bystander is mirroring the screen | No raw numbers. Contact id and server-resolved `displayName` only. `ELIFE_DataRedactor` already blanks numbers. |
| Scroll must drive the large title and bar glaze | `TrackScroll(scroll, largeTitle)`. Wheel is bound there; keep the handler referenced or it silently dies. Index pages use a widget named `LargeTitle`. |

## Shell

- **Home.** Clock and date at the top; a 4-column app grid at the bottom. Above the
  grid: two cards from real data — what have I got, who wants me — each opens its
  app. Nothing invented. Page dots only if a second page exists. Labels: one line,
  ellipsis, never shrunk. Card type follows what the figure *is*, not one fixed
  size for CardValue — a payload figure and a sentence preview earn different
  weight and size. Same layout, many jobs: size, weight, and colour are set per
  caller in script (`ELIFE_PhoneStyle.SetTypeOf`), not baked into the widget.
  Card labels stay `TextPrimary`, not `Ink` or `TextSecondary`.
- **Lock.** Time and date on sharp wallpaper. At most one glass layer. Notifications
  are glass cards from the bottom, each carrying its sender's person mark. PIN pad is real,
  overlay altitude. Must read on the world RT at a glance.
- **Notifications.** Three surfaces share one card component and must never drift
  apart. A notification is about a **person** (a mark, not an app icon or status
  dot), both text lines are `TextPrimary` (light glass reads mid-dark, so
  `TextSecondary` fails there per the accent table above), and the timestamp takes
  the accent.

  | Surface | Is | Rule |
  |---|---|---|
  | Lock list | Standing state | Every unread thread, rebuilt each lock render. |
  | Banner | An event | Awake-screen arrivals only, capped stack, self-dismisses after `BANNER_DURATION_MS`. Fades **both ways** — in on the present duration, out on the shorter state one, since arriving announces something and leaving is housekeeping. A **door**: taps hand the phone to the source app on that item. |
  | Hub | The backlog | Everything still standing, opened on demand as a dark-glass sheet over the current page — it takes the screen away rather than floating over it, unlike the light-glass cards it lists. |

  A rising unread count is an arrival; a merely non-zero one is not — opening the
  phone must never replay old unread as fresh news, and a thread already open on
  screen must never banner or queue itself, since it is being read as it lands.

  The **status-bar indicator** counts messages (three from one person is three
  things waiting); the **hub row** counts per-thread, since its body only shows the
  newest. Never confuse the two. Both surfaces are hidden entirely on lock and off —
  a count is still information about who is contacting you, and the hub is bodies
  one tap from a locked phone. **Clear** is a watermark per thread, not a read flag
  and never a backend call — the next message pushes past it and the notification
  returns, so clearing can never mean "ignore forever." Hub open state is
  replicated so the world screen matches the menu.
- **Settings.** Grouped rows, caption headers, right-aligned values, chevrons for
  pushes, toggles for booleans. Device ID, number, and PIN are real fields — values,
  not body copy.
- **Navigation.** Large title collapses to the inline title on scroll. Status and
  nav glaze together, only once content passes underneath. At rest they are
  transparent. Home and lock never glaze — nothing scrolls under them. A permanently
  glazed status bar is a window chrome.
- **Back vs Home.** Back pops one level of an app's own stack; it never leaves the
  app. Leaving the app is the home pill's job alone. An app's root page — the
  first screen you land on from the home grid — carries no Back: there is nowhere
  shallower inside that app to go, so nothing is drawn in the leading slot. Back
  only appears once a page has pushed past root (a thread inside Messages, a
  contact's detail). Do not add a root-page Back that just re-triggers home — that
  duplicates the home pill under a different name and teaches two gestures for
  one action.
- **Offline.** Phone-wide (`OfflineScreen`), same tier as `ScreenOff`. HTTP 0 means
  the Bridge never answered; any real HTTP code is that route's problem. Apps still
  show a loading skeleton for in-flight latency. One Retry: provision if there is no
  identity yet, otherwise re-ask the last failed data.
- **Case.** Menu canvas draws a thin 5px frame, `rounded_6px`, same specular recipe
  as the bars. No chin, earpiece, antenna, brand, or side buttons. Home is an
  on-screen pill, not a physical button and not a full-width band. On the world RT
  the gadget model is the frame — screen contents only.

## Type, space, motion

- Spacing: `SPACE_1`…`SPACE_6` (4pt). Leading inset `INSET_LEADING` (12). Safe
  areas are hard margins.
- Type: `TEXT_DISPLAY` 46 / `TEXT_HERO` 26 / `TEXT_TITLE_LARGE` 20 / `TEXT_TITLE`
  15 / `TEXT_BODY` 11 / `TEXT_SUBHEAD` 10 / `TEXT_CAPTION` and `TEXT_FLOOR` 9.
  Home clock is its own size (`TEXT_CLOCK` 34). Lock time stays `TEXT_DISPLAY`.
  Status-bar time stays `TEXT_CAPTION`. `ELIFE_PhoneClockUIComponent` only writes
  the digits — it must not stamp a size. Three weights at most. Bold for clock,
  titles, balances; regular for body and captions. Figures that must column use
  fixed-width containers and right alignment. `RobotoCondensed` is the current
  face (`FONT_REGULAR` / `FONT_BOLD`) — do not add a second display font without
  a decision.
- Depth: six named tiers in `ELIFE_PhoneStyle`, set in
  `ELIFE_PhoneScreenShell.ApplyDepth()`. Named constants only — never a literal
  ZOrder, and never a new tier without a reason it can't sit in an existing one.

  | Tier | Value | What sits here |
  |---|---|---|
  | `ZORDER_GROUND` | 0 | `ScreenGround`, `Wallpaper` |
  | `ZORDER_CONTENT` | 10 | `ScreenStage` — lock, home, and every app page |
  | `ZORDER_CHROME` | 20 | Nav bar — chrome the open **page** owns |
  | `ZORDER_SHEET` | 30 | `NotificationHub` — and any future modal over one page (picker, confirm) |
  | `ZORDER_SYSTEM` | 40 | Status bar, home pill — chrome the **phone** owns |
  | `ZORDER_BANNER` | 50 | `NotificationBanner` — arrivals, over everything but an alert |
  | `ZORDER_ALERT` | 60 | `ScreenOff`, `OfflineScreen` — phone-wide takeovers only |

  Three rules decide the order. **Page chrome and OS chrome are different tiers**: a
  sheet covers the nav bar, because Back and the named action operate on the page the
  sheet is hiding, but it never covers the status bar or the home pill — that would be
  a layer with no clock and no way out. **A banner outranks all of it**, because an
  arrival interrupts whatever you are doing. **Alerts outrank the banner**, because a
  dark screen or a dead Bridge is a statement about the whole phone, and drawing news
  over either would misreport what the phone is doing.

  Anything that covers the entire screen and blocks interaction is an alert; anything
  that covers one page is a sheet. The home pill unwinds them in order — a sheet
  first, then the app — so it always means "one layer back", never "jump to home".
- Motion: ease-out, no overshoot. ~250–350ms present, 150–200ms state, instant
  focus. One entrance per screen. Pressed glass lightens one step; it does not
  scale. **Anything given an entrance owes an exit.** A surface that eases in and
  then disappears on a frame reads as a glitch, and is worse than never animating
  it at all. Exits take the shorter state duration: arriving is an announcement,
  leaving is housekeeping.
- States: default / hovered / pressed / focused / disabled, plus loading and empty.
  Spinners: delay 150ms, stay at least 300ms. Skeleton rows when height is known.

## Tokens and sprites

Every colour, size, duration, and ZOrder is a token in `ELIFE_PhoneStyle` first. No
one-off `Color` in a layout.

Every wrapper sprite name is checked against [`docs/icons_wrapperUI.md`](icons_wrapperUI.md)
before use. Load through `ELIFE_PhoneStyle.ICON_SET_WRAPPER` or `ICON_SET_CHAT`.
`LoadImageFromSet()` fails closed: wrong name, no image, no error. Names are
constants on the class, not literals at the call site. After load, `FitIcon` so a
non-square cell does not shift the ink.

## Content

**Currency is the tolar** — Everon's, modelled by Bohemia on the map's fuel pumps.
No symbol ships in the game data; the mark is `Ŧ` (U+0166), fallback `TOL`. Suffixed
to every figure; also the Bank tile mark.

Copy is for this world and matches the DTOs (`ELIFE_ContactDto`, `ELIFE_ThreadDto`,
`ELIFE_MessageDto`). No lorem. Field hints state the rule ("8 digits"), never a
specimen number.

## Do not

- Purple→blue or cyan→magenta gradients, gradient-filled text, glow halos, aurora
  blobs, orbs.
- Inter, Roboto, Open Sans, Poppins, or Lato as a **display** face.
- Pure `#000` or `#fff`.
- Card-in-card, 3-equal-column feature grids, coloured left-edge stripes, uppercase
  eyebrows (unless the content is genuinely ordinal), italic emphasis in headings,
  emoji as icons.
- Fake browser/window chrome, a notch, or a speaker slit on either canvas.
- Animate everything: no hover scale, no overshoot, no cursor-followers, no
  fade-in on every row.
- Success toasts for something already on screen. No confirm for a reversible
  action (optimistic + undo). No invented metrics, "Jane Doe", or brand-name
  placeholders.
- Arbitrary ZOrder or magic offsets that only work at one canvas size.

## Before you hand it back

Score 1–5: philosophy, hierarchy, execution, specificity, restraint, variety.
Anything under 3 is revised first. Then four lines: the depth model; where glass
is used and why something real sits behind each instance; how five accents stay
one family; which anti-pattern you were most tempted by.
