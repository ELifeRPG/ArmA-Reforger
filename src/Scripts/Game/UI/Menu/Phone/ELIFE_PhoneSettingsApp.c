//------------------------------------------------------------------------------------------------
class ELIFE_PhoneSettingsApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{924472620BCF7E97}UI/layouts/Menus/Phone/Apps/PhoneSettings.layout";

	protected TextWidget m_wPhoneIdValue;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Settings";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.SETTINGS;
	}

	//------------------------------------------------------------------------------------------------
	protected override Widget CreateRoot(notnull Widget host)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget root = workspace.CreateWidgets(LAYOUT, host);
		if (root)
		{
			//! CreateWidgets() doesn't give the returned root a fill slot by default.
			AlignableSlot.SetHorizontalAlign(root, LayoutHorizontalAlign.Stretch);
			AlignableSlot.SetVerticalAlign(root, LayoutVerticalAlign.Stretch);
		}

		return root;
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnOpened()
	{
		if (!m_wRoot)
			return;

		m_wPhoneIdValue = TextWidget.Cast(m_wRoot.FindAnyWidget("PhoneIdValue"));
		if (!m_wPhoneIdValue)
			return;

		string phoneId = "";
		if (m_Menu && m_Menu.GetBoundPhone())
			phoneId = m_Menu.GetBoundPhone().GetPhoneId();

		m_wPhoneIdValue.SetText(phoneId);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		m_wPhoneIdValue = null;
	}
}
