# RTTexture: rendering live 2D content onto a 3D mesh

How to project a live-updating UI widget tree (text, images, HUD-style content)
onto a mesh's surface in-world — e.g. a screen, sign, gauge, or display prop.

## Core mechanism

1. A material's texture slot can be set to the literal string `"$rendertarget"`
   instead of a normal texture path. This marks that slot as "fed by a bound
   render target" rather than a static texture.
2. A `RTTextureWidget` inside a UI layout renders its widget-tree content
   (whatever's nested inside it — text, images, etc.) to an off-screen buffer.
3. `RTTextureWidget.SetRenderTarget(IEntity ent)` binds that widget's live
   output as the resource fulfilling `$rendertarget` on the given entity's
   mesh. It resolves against *any* material slot on that entity referencing
   `$rendertarget` — no material index/slot parameter needed.
4. `RTTextureWidget.RemoveRenderTarget(IEntity ent)` must be called when
   tearing down the widget (component destructor / entity deletion), or the
   binding leaks.

This is the same mechanism vanilla uses for weapon scope PIP optics and the
ballistic-table gadget display — the API is real and standard. Following it
exactly, however, is not enough to get a visible result. The requirements
below are not documented anywhere in the API reference and will each
independently produce a black/blank result if skipped.

## Requirements

### 1. The material must be explicitly emissive

Setting the map slot alone renders as black or near-black under normal ambient
light — visually indistinguishable from nothing being bound at all. A minimal
working material:

```
MatPBRBasic {
 Color 1 1 1 1
 Emissive 1 1 1 1
 EmissiveLV 5
 ApplyAlbedoToEmissive 1
 RoughnessScale 0.2
 MetalnessScale 0
 BCRMap "$rendertarget"
}
```

- `Emissive` must not be black, and `ApplyAlbedoToEmissive 1` must be set —
  without both, the bound texture is present but not actually lit/visible.
- The map slot name varies by material/shader lineage — `BCRMap` and
  `AlbedoMap` have both been seen in real use. Check what slot an existing,
  working material on the same mesh already uses for its base texture, and
  match that name.
- A flat, standalone `MatPBRBasic { }` block works fine — inheriting from a
  base `.emat` is not required.
- `MatCommon { }` is not a valid sub-block on a standalone (non-inheriting)
  material — only inheriting materials (`MatPBRBasic : "{GUID}base.emat" { }`)
  support nesting properties inside it. On a flat material, put properties
  directly on the root.

### 2. Binding must be deferred, not done synchronously on entity init

Calling `CreateWidgets()` / `SetRenderTarget()` immediately in
`OnPostInit`/`EOnInit` is too early — the workspace and entity hierarchy
aren't reliably settled yet at that point. Defer by roughly a quarter second:

```c
override void OnPostInit(IEntity owner)
{
	super.OnPostInit(owner);
	if (!GetGame().InPlayMode())
		return;
	GetGame().GetCallqueue().CallLater(InitRT, 250, false);
}

protected void InitRT()
{
	WorkspaceWidget workspace = GetGame().GetWorkspace();
	if (!workspace)
		return;

	Widget root = workspace.CreateWidgets(CONTENT_LAYOUT);
	if (!root)
		return;

	RTTextureWidget rt = RTTextureWidget.Cast(root.FindAnyWidget("SomeRTWidgetName"));
	if (!rt)
	{
		root.RemoveFromHierarchy();
		return;
	}

	IEntity owner = GetOwner();
	if (!owner)
		return;

	rt.SetRenderTarget(owner);
	rt.SetEnabled(true);
	root.Update();
	rt.Update();
}
```

### 3. Force an initial render after binding

Call `.Update()` on both the root widget and the `RTTextureWidget` right after
`SetRenderTarget()`/`SetEnabled(true)` (shown above) — this guarantees the
first frame actually renders instead of relying on an implicit refresh that
may not arrive in time.

### 4. A child that must fill the whole canvas needs `FrameWidgetSlot`, not `OverlayWidgetSlot`/`AlignableSlot`

A direct child of `RTTextureWidgetClass` meant to cover the entire render
target must use `FrameWidgetSlot` with `Anchor 0 0 1 1` and zero offsets:

```
ImageWidgetClass "{GUID}" {
 Name "Background"
 Slot FrameWidgetSlot "{GUID}" {
  Anchor 0 0 1 1
  PositionX 0
  OffsetLeft 0
  PositionY 0
  OffsetTop 0
  SizeX 0
  OffsetRight 0
  SizeY 0
  OffsetBottom 0
 }
 Color 0 0 0 1
}
```

The usual `OverlayWidgetSlot`/`AlignableSlot` stretch idiom does **not**
resolve to the parent's real size here — it collapses to a small, inconsistent
box (visible in both Play mode and the Layout Editor's static preview), no
matter what `AllowWidthOverride`/`Update()` calls you throw at it. A child
that only occupies part of the canvas (e.g. a pivoted label) can still use
`OverlayWidgetSlot` normally — this only applies to a full-canvas child.

To host further content sized relative to itself rather than the canvas, add
`AllowWidthOverride`/`WidthOverride` (and height) alongside the `FrameWidgetSlot`
fill — its own children can then use ordinary slot types freely.

## Layout structure notes

A working layout is straightforward — no special wrapper needed:

```
FrameWidgetClass {
 Name "rootFrame"
 {
  RTTextureWidgetClass "{GUID}" {
   Name "SomeRTWidgetName"
   Slot FrameWidgetSlot "{GUID}" {
    OffsetLeft 0
    OffsetTop 0
    SizeX 512
    OffsetRight -512
    SizeY 256
    OffsetBottom -256
   }
   {
    ImageWidgetClass "{GUID}" {
     Name "Background"
     Slot FrameWidgetSlot "{GUID}" {
      Anchor 0 0 1 1
      PositionX 0
      OffsetLeft 0
      PositionY 0
      OffsetTop 0
      SizeX 0
      OffsetRight 0
      SizeY 0
      OffsetBottom 0
     }
     ...
    }
    TextWidgetClass "{GUID}" {
     Slot OverlayWidgetSlot "{GUID}" { HorizontalAlign 3 VerticalAlign 3 }
     ...
    }
   }
  }
 }
}
```

- The first direct child (`Background` above) fills the whole canvas via
  `FrameWidgetSlot` — see requirement 4. A child that only needs to sit at a
  point or a corner (like `TextWidgetClass` here) can use `OverlayWidgetSlot`
  normally.
- Size the `RTTextureWidget`'s `Slot` to roughly match the target UV
  rectangle's aspect ratio on the mesh — mismatches stretch the content.
- If the mesh's UV area for the target surface is a plain, unrotated
  rectangle, no rotation/bridge trick is needed. Only add one (a second,
  nested `RTTextureWidget` sampled by a rotated/UV-remapped `ImageWidget`) if
  the mesh's UV island is itself rotated or a small sub-rectangle of a shared
  texture space.
- A gray/blank box for the `RTTextureWidget` in the Layout Editor's static
  preview is normal and does not indicate a problem — it only has live
  content once bound at runtime in Play mode.
- If the tree is built via nested `workspace.CreateWidgets(layout,
  parentWidget)` (reusing another layout's content instead of duplicating
  it), the same `FrameWidgetSlot` requirement applies to the inserted root if
  it needs to fill its host — an `AlignableSlot` stretch fixup afterward does
  not substitute for it.

### Material reassignment can silently break an existing binding

Reassigning the mesh's material at runtime (`$remap` or similar) after
`SetRenderTarget()` can tear down that binding, even when reassigning the
same material that's already active. If script re-applies a material
unconditionally on every state change or replication update, a
`$rendertarget` binding can silently stop working later with no error
logged. Guard reassignment behind a check against the currently-applied
material and skip when it hasn't changed.

## Debugging checklist (in order)

When a render target shows black/blank in Play mode, check in this order:

1. **Check the material**: is `Emissive` non-black, and is
   `ApplyAlbedoToEmissive 1` set? Is the map slot name (`BCRMap`/`AlbedoMap`/
   etc.) actually the one this shader/material lineage uses?
2. **Check the material actually parses.** A malformed `.emat` fails silently
   in some tool responses — check Workbench's own log output for a
   `Material load ... (E): Unknown keyword/data ...` line, not just whether a
   tool call "succeeded."
3. **Check the binding timing** — is `SetRenderTarget` deferred past initial
   entity setup, or called too early?
4. **Isolate with a flat color.** Temporarily replace the widget tree's real
   content with a single bright, unmistakable `ImageWidgetClass` fill (e.g.
   pure green). If the color shows up on the mesh, the RTT binding itself
   works and the bug is in the actual content (text/font/nested widgets). If
   it's still black, the bug is in the binding or material, not the content.
5. **Check `FindAnyWidget()` isn't returning null** — log or assert that the
   named `RTTextureWidget` was actually found in the created widget tree
   before calling `SetRenderTarget` on it.
6. **On teardown**, confirm `RemoveRenderTarget()` is actually called —
   leaked bindings can produce inconsistent behavior on respawn/re-entry.
7. **If it worked once and then stopped**, suspect a later material
   reassignment breaking the binding — see "Material reassignment can
   silently break an existing binding" above.
8. **If `GetScreenSize()` reads small and inconsistent** (not `0x0`, but far
   below the declared canvas size) while siblings report sane sizes, that's
   usually the wrong slot type (requirement 4), not a timing issue — check
   the Layout Editor's static preview rather than adding more size logging.
