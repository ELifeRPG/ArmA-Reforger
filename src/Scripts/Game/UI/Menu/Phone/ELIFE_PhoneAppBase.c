//------------------------------------------------------------------------------------------------
//! One phone app. Hosted by either the real phone menu or the 3D-mesh screen copy.
class ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT_STATUS = "{9C6B03E5A187D24F}UI/layouts/Menus/Phone/Apps/PhoneAppStatus.layout";

	//! Spacing between two list rows.
	protected const float ROW_GAP = 5;

	protected ELIFE_PhoneGadgetComponent m_Phone;
	protected Widget m_wRoot;
	protected Widget m_wStatusOverlay;

	//------------------------------------------------------------------------------------------------
	//! initialSubState restores nav state on open, so OnOpened() shouldn't broadcast its own default.
	void Open(notnull ELIFE_PhoneGadgetComponent phone, notnull Widget host, string initialSubState = "")
	{
		m_Phone = phone;
		m_wRoot = CreateRoot(host);
		if (m_wRoot)
		{
			//! Last child of the root, so it draws over the app's own content.
			m_wStatusOverlay = CreateStretched(LAYOUT_STATUS, m_wRoot);

			OnOpened();
			ApplySubState(initialSubState);
		}
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		OnClosing();

		m_wStatusOverlay = null;

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_Phone = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Covers the app while it has nothing to show yet. A failed refresh keeps the old data on screen.
	protected void ApplyDataStatus(string key)
	{
		if (!m_wStatusOverlay || !m_Phone)
			return;

		ELIFE_EPhoneDataStatus status = m_Phone.GetDataStatus(key);
		bool blocking = m_Phone.GetData(key) == "" && (status == ELIFE_EPhoneDataStatus.LOADING || status == ELIFE_EPhoneDataStatus.ERROR);

		m_wStatusOverlay.SetVisible(blocking);
		if (!blocking)
			return;

		if (status == ELIFE_EPhoneDataStatus.ERROR)
		{
			SetText(m_wStatusOverlay, "StatusTitle", "#ELIFE-Phone_Status_Error");
			SetText(m_wStatusOverlay, "StatusHint", "#ELIFE-Phone_Status_ErrorHint");
			return;
		}

		SetText(m_wStatusOverlay, "StatusTitle", "#ELIFE-Phone_Status_Loading");
		SetText(m_wStatusOverlay, "StatusHint", "");
	}

	//------------------------------------------------------------------------------------------------
	//! Return true if Back was consumed (e.g. statement → account list).
	bool OnBack()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Override for apps with their own in-app navigation (e.g. Bank's open statement).
	string GetSubState()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Restore navigation from a GetSubState() value received from another instance of this app.
	void ApplySubState(string subState)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Call after any internal navigation change (see GetSubState()).
	protected void NotifySubStateChanged()
	{
		if (m_Phone)
			m_Phone.SetScreenSubState(GetSubState());
	}

	//------------------------------------------------------------------------------------------------
	string GetTitle()
	{
		return "#ELIFE-Item_Phone_Name";
	}

	//------------------------------------------------------------------------------------------------
	EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.HOME;
	}

	//------------------------------------------------------------------------------------------------
	protected Widget CreateRoot(notnull Widget host)
	{
		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOpened()
	{
	}

	//------------------------------------------------------------------------------------------------
	protected void OnClosing()
	{
	}

	//------------------------------------------------------------------------------------------------
	//! CreateWidgets() gives the new root no fill slot, so stretch it to the parent here.
	protected Widget CreateStretched(ResourceName layout, notnull Widget parent)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget widget = workspace.CreateWidgets(layout, parent);
		if (!widget)
			return null;

		AlignableSlot.SetHorizontalAlign(widget, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(widget, LayoutVerticalAlign.Stretch);

		return widget;
	}

	//------------------------------------------------------------------------------------------------
	//! One row in a vertical list layout. gap adds spacing below the row (skip it on the last one).
	protected Widget CreateListRow(ResourceName layout, notnull Widget list, float gap = 0)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget row = workspace.CreateWidgets(layout, list);
		if (!row)
			return null;

		LayoutSlot.SetSizeMode(row, LayoutSizeMode.Fill);

		if (gap > 0)
			LayoutSlot.SetPadding(row, 0, 0, 0, gap);

		return row;
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearChildren(Widget parent)
	{
		if (!parent)
			return;

		Widget child = parent.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetText(notnull Widget parent, string widgetName, string text)
	{
		TextWidget widget = TextWidget.Cast(parent.FindAnyWidget(widgetName));
		if (widget)
			widget.SetText(text);
	}

	//------------------------------------------------------------------------------------------------
	//! First character of a name - the row layouts use it as an avatar instead of a picture.
	protected string Initial(string name)
	{
		if (name.Length() == 0)
			return "";

		return name.Substring(0, 1);
	}
}
