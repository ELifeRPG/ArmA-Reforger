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
    ImageWidgetClass "{GUID}" { ... }
    TextWidgetClass "{GUID}" { ... }
   }
  }
 }
}
```

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
