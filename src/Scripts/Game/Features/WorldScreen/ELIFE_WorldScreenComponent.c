//------------------------------------------------------------------------------------------------
[EntityEditorProps(category: "ELifeRPG/WorldScreen", description: "Live-renders a 2D UI layout onto the entity's mesh via a render-target bound to its $rendertarget material.")]
class ELIFE_WorldScreenComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Client-local. Renders a UI layout onto the mesh screen when the local player is nearby.
//! A static prop is configured through the attributes; subclasses can override the protected hooks instead.
class ELIFE_WorldScreenComponent : ScriptComponent
{
	[Attribute("", UIWidgets.ResourceNamePicker, "Canvas holding the RTTextureWidget and the host its content is created in. Empty disables the screen.", "layout", category: "World Screen")]
	protected ResourceName m_sCanvasLayout;

	[Attribute("", UIWidgets.ResourceNamePicker, "Content created in the canvas host and stretched to fill it.", "layout", category: "World Screen")]
	protected ResourceName m_sContentLayout;

	[Attribute("1024", UIWidgets.EditBox, "Canvas width in px. Must match the RTTextureWidget size in the canvas layout.", category: "World Screen")]
	protected int m_iCanvasWidth;

	[Attribute("1024", UIWidgets.EditBox, "Canvas height in px. Must match the RTTextureWidget size in the canvas layout.", category: "World Screen")]
	protected int m_iCanvasHeight;

	[Attribute("ContentRT", UIWidgets.EditBox, "Name of the RTTextureWidget in the canvas layout.", category: "World Screen")]
	protected string m_sRTWidgetName;

	[Attribute("ContentHost", UIWidgets.EditBox, "Name of the widget in the canvas layout the content is created in.", category: "World Screen")]
	protected string m_sHostWidgetName;

	[Attribute("10", UIWidgets.EditBox, "The screen renders while the local player is within this many metres.", category: "World Screen")]
	protected float m_fActivationRange;

	[Attribute("", UIWidgets.EditBox, "Source material of the mesh's screen slot. Empty means no live/idle swap.", category: "World Screen")]
	protected string m_sScreenSourceMaterial;

	[Attribute("", UIWidgets.ResourceNamePicker, "Screen material while rendering (feeds its $rendertarget slot).", "emat", category: "World Screen")]
	protected ResourceName m_sLiveMaterial;

	[Attribute("", UIWidgets.ResourceNamePicker, "Screen material while not rendering.", "emat", category: "World Screen")]
	protected ResourceName m_sIdleMaterial;

	//! Proximity/LOD recheck interval.
	protected const int RECHECK_INTERVAL_MS = 1500;
	//! Redraw interval for a bound screen.
	protected const int REFRESH_INTERVAL_MS = 100;

	protected Widget m_wRoot;
	protected RTTextureWidget m_RT;
	protected bool m_bBound;

	protected Widget m_wContentRoot;
	protected ResourceName m_sAppliedScreenMaterial;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!GetGame().InPlayMode() || GetCanvasLayout().IsEmpty())
			return;

		GetGame().GetCallqueue().CallLater(RecheckProximity, RECHECK_INTERVAL_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! The canvas the screen is painted on (GetCanvasWidth() x GetCanvasHeight() pixels), for hit-testing. Null while unbound.
	Widget GetScreenCanvasHost()
	{
		if (!m_bBound || !m_wRoot)
			return null;

		return m_RT;
	}

	//------------------------------------------------------------------------------------------------
	//! Root of the content layout while bound, else null.
	Widget GetContentRoot()
	{
		return m_wContentRoot;
	}

	//------------------------------------------------------------------------------------------------
	float GetCanvasWidth()
	{
		return m_iCanvasWidth;
	}

	//------------------------------------------------------------------------------------------------
	float GetCanvasHeight()
	{
		return m_iCanvasHeight;
	}

	//------------------------------------------------------------------------------------------------
	//! Id of the on-screen control at this canvas point standing in for a physical button, empty if none.
	string FindButtonAtCanvas(float canvasX, float canvasY)
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Holds the RTTextureWidget and the host its content is created in.
	protected ResourceName GetCanvasLayout()
	{
		return m_sCanvasLayout;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetRTWidgetName()
	{
		return m_sRTWidgetName;
	}

	//------------------------------------------------------------------------------------------------
	protected string GetHostWidgetName()
	{
		return m_sHostWidgetName;
	}

	//------------------------------------------------------------------------------------------------
	protected float GetActivationRange()
	{
		return m_fActivationRange;
	}

	//------------------------------------------------------------------------------------------------
	//! False keeps the screen unbound however close the player is.
	protected bool IsContentWanted()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Every recheck, before binding is decided. -1 means no local player.
	protected void OnProximityChecked(float distance)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! True drops a bound screen so the same recheck binds it afresh.
	protected bool NeedsRebind()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Fills the canvas host with the content layout. False aborts the bind.
	protected bool BuildContent(Widget host)
	{
		if (m_sContentLayout.IsEmpty())
			return true;

		m_wContentRoot = GetGame().GetWorkspace().CreateWidgets(m_sContentLayout, host);
		if (!m_wContentRoot)
		{
			Print(string.Format("ELIFE_WorldScreen: Failed to insert %1 into %2 - aborting RT screen bind", m_sContentLayout, GetHostWidgetName()), LogLevel.WARNING);
			return false;
		}

		//! CreateWidgets() doesn't give the returned root a fill slot by default.
		AlignableSlot.SetHorizontalAlign(m_wContentRoot, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(m_wContentRoot, LayoutVerticalAlign.Stretch);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! After the content is built, before SetRenderTarget().
	protected void OnBeforeBind()
	{
		ApplyScreenMaterial(m_sLiveMaterial);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnUnbound()
	{
		ApplyScreenMaterial(m_sIdleMaterial);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyScreenMaterial(ResourceName material)
	{
		if (m_sScreenSourceMaterial.IsEmpty() || material.IsEmpty())
			return;

		//! Skip redundant remaps - re-issuing the same material tears down an already-bound $rendertarget.
		if (material == m_sAppliedScreenMaterial)
			return;

		IEntity owner = GetOwner();
		VObject obj = owner.GetVObject();
		if (!obj)
		{
			Print("ELIFE_WorldScreen: ApplyScreenMaterial aborted - GetVObject() returned null", LogLevel.WARNING);
			return;
		}

		owner.SetObject(obj, string.Format("$remap '%1' '%2';", m_sScreenSourceMaterial, material));
		m_sAppliedScreenMaterial = material;
	}

	//------------------------------------------------------------------------------------------------
	protected void RecheckProximity()
	{
		float distance = GetDistanceToLocalPlayer();

		OnProximityChecked(distance);

		bool shouldBeActive = IsContentWanted() && distance >= 0 && distance <= GetActivationRange();

		if (m_bBound && NeedsRebind())
			DestroyRTScreen();

		if (shouldBeActive && !m_bBound)
			CreateRTScreen();
		else if (!shouldBeActive && m_bBound)
			DestroyRTScreen();
	}

	//------------------------------------------------------------------------------------------------
	//! -1 if there's no local player (e.g. server console) - always counts as out of range.
	protected float GetDistanceToLocalPlayer()
	{
		IEntity localEntity = SCR_PlayerController.GetLocalControlledEntity();
		if (!localEntity)
			return -1;

		vector delta = GetOwner().GetOrigin() - localEntity.GetOrigin();
		return delta.Length();
	}

	//------------------------------------------------------------------------------------------------
	//! Builds the render-target canvas with BuildContent() filling its host. Returns the root, or null on failure.
	protected Widget CreateScreenTree(out RTTextureWidget rt)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget root = workspace.CreateWidgets(GetCanvasLayout());
		if (!root)
			return null;

		rt = RTTextureWidget.Cast(root.FindAnyWidget(GetRTWidgetName()));
		Widget screenHost = root.FindAnyWidget(GetHostWidgetName());
		if (!rt || !screenHost)
		{
			Print(string.Format("ELIFE_WorldScreen: %1 is missing %2/%3 - aborting RT screen bind", GetCanvasLayout(), GetRTWidgetName(), GetHostWidgetName()), LogLevel.WARNING);
			root.RemoveFromHierarchy();
			return null;
		}

		if (!BuildContent(screenHost))
		{
			root.RemoveFromHierarchy();
			return null;
		}

		return root;
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateRTScreen()
	{
		m_wRoot = CreateScreenTree(m_RT);
		if (!m_wRoot)
			return;

		IEntity owner = GetOwner();

		OnBeforeBind();

		m_RT.SetRenderTarget(owner);
		m_RT.SetEnabled(true);

		m_wRoot.Update();
		m_RT.Update();

		//! Not in a composited GUI layer, so it's ticked manually.
		GetGame().GetCallqueue().CallLater(TickRT, REFRESH_INTERVAL_MS, true);

		m_bBound = true;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickRT()
	{
		if (!m_wRoot || !m_RT)
			return;

		m_wRoot.Update();
		m_RT.Update();
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyRTScreen()
	{
		if (!m_bBound)
			return;

		GetGame().GetCallqueue().Remove(TickRT);

		if (m_RT)
		{
			m_RT.SetEnabled(false);
			m_RT.RemoveRenderTarget(GetOwner());
		}

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_RT = null;
		m_wContentRoot = null;
		m_bBound = false;

		OnUnbound();
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_WorldScreenComponent()
	{
		GetGame().GetCallqueue().Remove(RecheckProximity);
		GetGame().GetCallqueue().Remove(TickRT);

		IEntity owner = GetOwner();
		if (m_RT && owner)
			m_RT.RemoveRenderTarget(owner);

		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
	}
}
