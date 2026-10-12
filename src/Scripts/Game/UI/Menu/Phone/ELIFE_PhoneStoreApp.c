//------------------------------------------------------------------------------------------------
class ELIFE_PhoneStoreActionClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneStoreApp m_App;
	protected string m_sAppKey;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneStoreApp app, string appKey)
	{
		m_App = app;
		m_sAppKey = appKey;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App)
			m_App.OnActionClicked(m_sAppKey);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! The local catalog, grouped by install state. Get / Remove round-trip through the gadget; the
//! installed-apps push then redraws this page and the home grid together.
class ELIFE_PhoneStoreApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{3F8A61C2D94E07B5}UI/layouts/Menus/Phone/Apps/PhoneStore.layout";
	protected const ResourceName LAYOUT_ROW = "{A2C75E19F0B3D468}UI/layouts/Menus/Phone/Apps/PhoneStoreRow.layout";

	//! Badge ink, same as a home card's.
	protected const float ICON_INK = 36;

	//! Hit-area padding either side of the action word.
	protected const float ACTION_PAD_H = 16;

	protected const string ERROR_LOAD = "#ELIFE-Phone_Store_LoadError";

	protected ScrollLayoutWidget m_wStoreScroll;
	protected Widget m_wStoreGroups;
	protected TextWidget m_wStoreError;

	protected ref array<ref ELIFE_PhoneStoreActionClick> m_aActionClicks;

	//! One request at a time; the key it's for, and which way.
	protected string m_sPendingKey;
	protected bool m_bPendingInstall;

	//! Remove takes a second tap on the same row.
	protected string m_sArmedKey;

	protected string m_sError;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Store";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.STORE;
	}

	//------------------------------------------------------------------------------------------------
	protected override Widget CreateRoot(notnull Widget host)
	{
		return CreateStretched(LAYOUT, host);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnOpened()
	{
		if (!m_wRoot)
			return;

		m_wStoreScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("StoreScroll"));
		m_wStoreGroups = m_wRoot.FindAnyWidget("StoreGroups");
		m_wStoreError = TextWidget.Cast(m_wRoot.FindAnyWidget("StoreError"));
		m_aActionClicks = new array<ref ELIFE_PhoneStoreActionClick>();

		if (m_wStoreError)
			m_wStoreError.SetColor(ELIFE_PhoneStyle.Negative());

		TrackScroll(m_wStoreScroll, TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle")));

		if (!m_Phone)
			return;

		m_Phone.m_OnDataChanged.Insert(OnDataChanged);
		m_Phone.m_OnAppInstallResult.Insert(OnAppInstallResult);

		//! Draw what we already have, then refresh - the fetch lands later via OnDataChanged.
		Fill();
		m_Phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_APPS);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		if (m_Phone)
		{
			m_Phone.m_OnDataChanged.Remove(OnDataChanged);
			m_Phone.m_OnAppInstallResult.Remove(OnAppInstallResult);
		}

		m_wStoreScroll = null;
		m_wStoreGroups = null;
		m_wStoreError = null;
		m_aActionClicks = null;
		m_sPendingKey = "";
		m_sArmedKey = "";
		m_sError = "";
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDataChanged(string key)
	{
		if (key == ELIFE_PhoneGadgetComponent.DATA_APPS)
			Fill();
	}

	//------------------------------------------------------------------------------------------------
	protected void Fill()
	{
		ApplyDataStatus(ELIFE_PhoneGadgetComponent.DATA_APPS);

		if (m_aActionClicks)
			m_aActionClicks.Clear();

		ClearChildren(m_wStoreGroups);

		if (!m_wStoreGroups || !m_Phone)
			return;

		//! Until the real list lands the installed state is only assumed, so Bridge apps wait for it.
		bool known = m_Phone.HasInstalledApps();

		bool loadFailed = !known && m_Phone.GetDataStatus(ELIFE_PhoneGadgetComponent.DATA_APPS) == ELIFE_EPhoneDataStatus.ERROR;
		if (loadFailed)
			ShowError(ERROR_LOAD);
		else if (m_sError == ERROR_LOAD)
			ShowError("");

		array<ELIFE_PhoneAppEntry> installed = {};
		array<ELIFE_PhoneAppEntry> available = {};
		array<ELIFE_PhoneAppEntry> builtIn = {};

		array<ref ELIFE_PhoneAppEntry> catalog = ELIFE_PhoneAppCatalog.GetEntries();
		foreach (ELIFE_PhoneAppEntry app : catalog)
		{
			if (app.IsSystem())
				builtIn.Insert(app);
			else if (!known)
				continue;
			else if (m_Phone.IsAppInstalled(app.m_sAppKey))
				installed.Insert(app);
			else
				available.Insert(app);
		}

		bool isFirst = true;
		if (FillGroup(installed, "#ELIFE-Phone_Store_Installed", isFirst))
			isFirst = false;

		if (FillGroup(available, "#ELIFE-Phone_Store_Available", isFirst))
			isFirst = false;

		FillGroup(builtIn, "#ELIFE-Phone_Store_BuiltIn", isFirst);
	}

	//------------------------------------------------------------------------------------------------
	//! False when there was nothing to put in it.
	protected bool FillGroup(notnull array<ELIFE_PhoneAppEntry> apps, string header, bool isFirst)
	{
		if (apps.IsEmpty())
			return false;

		Widget list = CreateListGroup(m_wStoreGroups, header, isFirst);
		if (!list)
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();

		foreach (ELIFE_PhoneAppEntry app : apps)
		{
			Widget row = CreateListRow(LAYOUT_ROW, list, false);
			if (!row)
				continue;

			ELIFE_PhoneStyle.ApplyGlass(row, true);
			ELIFE_PhoneStyle.ApplyGlass(row.FindAnyWidget("StoreBadge"), false, false, false, true, ELIFE_PhoneStyle.AccentDeepFor(app.m_eState));

			if (workspace)
				ELIFE_PhoneScreenShell.PaintAppIcon(workspace, row.FindAnyWidget("StoreIconSize"), TextWidget.Cast(row.FindAnyWidget("StoreMark")), app, ICON_INK);

			SetTextAndColor(row, "StoreName", app.m_sLabel, ELIFE_PhoneStyle.TextPrimary());

			PaintAction(row, app);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The group header already says installed or not, so the row only carries what a tap would do.
	protected void PaintAction(notnull Widget row, notnull ELIFE_PhoneAppEntry app)
	{
		Widget actionSize = row.FindAnyWidget("StoreActionSize");
		Widget button = row.FindAnyWidget("StoreActionButton");
		TextWidget label = TextWidget.Cast(row.FindAnyWidget("StoreActionLabel"));

		//! Built-in apps have nothing to do; bystander copies can't act.
		bool actionable = !app.IsSystem() && IsOwner();
		if (actionSize)
			actionSize.SetVisible(actionable);

		if (!actionable || !button || !label)
			return;

		bool installed = m_Phone.IsAppInstalled(app.m_sAppKey);
		bool pending = ELIFE_PhoneAppCatalog.SameKey(m_sPendingKey, app.m_sAppKey);

		string text = "#ELIFE-Phone_Store_Get";
		Color color = m_Accent;
		if (pending && m_bPendingInstall)
		{
			text = "#ELIFE-Phone_Store_Installing";
			color = ELIFE_PhoneStyle.TextTertiary();
		}
		else if (pending)
		{
			text = "#ELIFE-Phone_Store_Removing";
			color = ELIFE_PhoneStyle.TextTertiary();
		}
		else if (installed && ELIFE_PhoneAppCatalog.SameKey(m_sArmedKey, app.m_sAppKey))
		{
			text = "#ELIFE-Phone_Store_RemoveConfirm";
			color = ELIFE_PhoneStyle.Negative();
		}
		else if (installed)
		{
			text = "#ELIFE-Phone_Store_Remove";
			color = ELIFE_PhoneStyle.Negative();
		}

		label.SetText(text);
		label.SetColor(color);

		//! Hugs the localized word, like the nav pill.
		SizeLayoutWidget size = SizeLayoutWidget.Cast(actionSize);
		if (size)
		{
			float textWidth, textHeight;
			label.GetTextSize(textWidth, textHeight);
			size.SetWidthOverride(textWidth + ACTION_PAD_H * 2);
		}

		button.SetEnabled(m_sPendingKey == "");

		ELIFE_PhoneStoreActionClick click = new ELIFE_PhoneStoreActionClick();
		click.Bind(this, app.m_sAppKey);
		button.AddHandler(click);
		if (m_aActionClicks)
			m_aActionClicks.Insert(click);
	}

	//------------------------------------------------------------------------------------------------
	//! Get installs at once; Remove arms on the first tap and sends on the second.
	void OnActionClicked(string appKey)
	{
		if (m_sPendingKey != "" || !m_Phone || !IsOwner() || appKey == "")
			return;

		ShowError("");

		bool installed = m_Phone.IsAppInstalled(appKey);
		if (installed && !ELIFE_PhoneAppCatalog.SameKey(m_sArmedKey, appKey))
		{
			m_sArmedKey = appKey;
			Fill();
			return;
		}

		m_sArmedKey = "";
		m_sPendingKey = appKey;
		m_bPendingInstall = !installed;
		Fill();

		m_Phone.SetAppInstalled(appKey, !installed);
	}

	//------------------------------------------------------------------------------------------------
	//! Success needs nothing here - the installed-apps push has already redrawn the rows.
	protected void OnAppInstallResult(string appKey, bool installed, bool succeeded)
	{
		if (!m_wRoot || m_sPendingKey == "")
			return;

		m_sPendingKey = "";

		if (!succeeded)
		{
			if (installed)
				ShowError("#ELIFE-Phone_Store_InstallError");
			else
				ShowError("#ELIFE-Phone_Store_RemoveError");
		}

		Fill();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowError(string text)
	{
		m_sError = text;

		if (!m_wStoreError)
			return;

		m_wStoreError.SetVisible(text != "");
		m_wStoreError.SetText(text);
	}
}
