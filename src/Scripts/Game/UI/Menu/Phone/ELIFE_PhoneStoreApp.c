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
class ELIFE_PhoneStoreRowClick : ScriptedWidgetEventHandler
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
			m_App.OpenDetail(m_sAppKey);

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
	protected const float DETAIL_ICON_INK = 72;

	//! Sub-state for the detail page: prefix + appKey. Empty is the list.
	protected const string SUBSTATE_DETAIL_PREFIX = "app:";

	//! Hit-area padding either side of the action word; the detail page's filled pill gets more.
	protected const float ACTION_PAD_H = 16;
	protected const float DETAIL_ACTION_PAD_H = 24;

	protected const string ERROR_LOAD = "#ELIFE-Phone_Store_LoadError";

	protected Widget m_wIndexPage;
	protected ScrollLayoutWidget m_wStoreScroll;
	protected Widget m_wStoreGroups;
	protected TextWidget m_wStoreError;
	protected TextWidget m_wIndexTitle;

	protected Widget m_wDetailPage;
	protected ScrollLayoutWidget m_wDetailScroll;
	protected TextWidget m_wDetailTitle;
	protected Widget m_wDetailActionSize;
	protected Widget m_wDetailActionButton;
	protected TextWidget m_wDetailError;
	protected Widget m_wRemoveCard;
	protected Widget m_wRemoveButton;

	protected ref array<ref ELIFE_PhoneStoreActionClick> m_aActionClicks;
	protected ref array<ref ELIFE_PhoneStoreRowClick> m_aRowClicks;

	//! The app whose detail page is open. Empty means the list is showing.
	protected string m_sOpenKey;

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
	//! The detail page is its own nav level - Back returns to the list, not home.
	override bool OnBack()
	{
		if (m_sOpenKey == "")
			return false;

		ShowIndex();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	override string GetSubState()
	{
		if (m_sOpenKey != "")
			return SUBSTATE_DETAIL_PREFIX + m_sOpenKey;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	override void ApplySubState(string subState)
	{
		if (subState.StartsWith(SUBSTATE_DETAIL_PREFIX))
		{
			OpenDetail(subState.Substring(SUBSTATE_DETAIL_PREFIX.Length(), subState.Length() - SUBSTATE_DETAIL_PREFIX.Length()));
			return;
		}

		ShowIndex();
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

		m_wIndexPage = m_wRoot.FindAnyWidget("IndexPage");
		m_wStoreScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("StoreScroll"));
		m_wStoreGroups = m_wRoot.FindAnyWidget("StoreGroups");
		m_wStoreError = TextWidget.Cast(m_wRoot.FindAnyWidget("StoreError"));
		m_wIndexTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle"));
		m_aActionClicks = new array<ref ELIFE_PhoneStoreActionClick>();
		m_aRowClicks = new array<ref ELIFE_PhoneStoreRowClick>();

		if (m_wStoreError)
			m_wStoreError.SetColor(ELIFE_PhoneStyle.Negative());

		BindDetail();
		TrackScroll(m_wStoreScroll, m_wIndexTitle);

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

		m_wIndexPage = null;
		m_wStoreScroll = null;
		m_wStoreGroups = null;
		m_wStoreError = null;
		m_wIndexTitle = null;
		m_wDetailPage = null;
		m_wDetailScroll = null;
		m_wDetailTitle = null;
		m_wDetailActionSize = null;
		m_wDetailActionButton = null;
		m_wDetailError = null;
		m_wRemoveCard = null;
		m_wRemoveButton = null;
		m_aActionClicks = null;
		m_aRowClicks = null;
		m_sOpenKey = "";
		m_sPendingKey = "";
		m_sArmedKey = "";
		m_sError = "";
	}

	//------------------------------------------------------------------------------------------------
	protected void BindDetail()
	{
		m_wDetailPage = m_wRoot.FindAnyWidget("DetailPage");
		m_wDetailScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("DetailScroll"));
		m_wDetailTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailTitle"));
		m_wDetailActionSize = m_wRoot.FindAnyWidget("DetailActionSize");
		m_wDetailActionButton = m_wRoot.FindAnyWidget("DetailActionButton");
		m_wDetailError = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailError"));
		m_wRemoveCard = m_wRoot.FindAnyWidget("RemoveCard");
		m_wRemoveButton = m_wRoot.FindAnyWidget("DetailRemoveButton");

		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("DescriptionCard"), true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("InfoCard"), true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRemoveCard, true);

		SetTextAndColor(m_wRoot, "AboutHeader", "#ELIFE-Phone_Store_About", m_Accent);
		SetTextAndColor(m_wRoot, "InfoHeader", "#ELIFE-Phone_Store_Information", m_Accent);

		//! The only free text on the page; the card grows with it.
		TextWidget description = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailDescription"));
		if (description)
			description.SetTextWrapping(true);

		if (m_wDetailError)
			m_wDetailError.SetColor(ELIFE_PhoneStyle.Negative());

		SCR_ButtonTextComponent actionButton = SCR_ButtonTextComponent.GetButtonText("DetailActionButton", m_wRoot);
		if (actionButton)
			actionButton.m_OnClicked.Insert(OnDetailActionClicked);

		SCR_ButtonTextComponent removeButton = SCR_ButtonTextComponent.GetButtonText("DetailRemoveButton", m_wRoot);
		if (removeButton)
			removeButton.m_OnClicked.Insert(OnDetailRemoveClicked);
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

		if (m_aRowClicks)
			m_aRowClicks.Clear();

		ClearChildren(m_wStoreGroups);

		//! The install push lands here too, so the open detail page follows it.
		if (m_sOpenKey != "")
			FillDetail();

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

			Widget rowButton = row.FindAnyWidget("StoreRowButton");
			if (rowButton && m_aRowClicks)
			{
				ELIFE_PhoneStoreRowClick rowClick = new ELIFE_PhoneStoreRowClick();
				rowClick.Bind(this, app.m_sAppKey);
				rowButton.AddHandler(rowClick);
				m_aRowClicks.Insert(rowClick);
			}

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
	//! Shown on both pages - an install started from one can finish while the other is up.
	protected void ShowError(string text)
	{
		m_sError = text;

		if (m_wStoreError)
		{
			m_wStoreError.SetVisible(text != "");
			m_wStoreError.SetText(text);
		}

		if (m_wDetailError)
		{
			m_wDetailError.SetVisible(text != "");
			m_wDetailError.SetText(text);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OpenDetail(string appKey)
	{
		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.FindByKey(appKey);
		if (!app)
			return;

		m_sOpenKey = app.m_sAppKey;
		m_sArmedKey = "";

		if (m_sError != ERROR_LOAD)
			ShowError("");

		FillDetail();

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wDetailPage)
			m_wDetailPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText(app.m_sLabel);

		TrackScroll(m_wDetailScroll, m_wDetailTitle);
		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowIndex()
	{
		m_sOpenKey = "";
		m_sArmedKey = "";

		if (m_wDetailPage)
			m_wDetailPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText(GetTitle());

		Fill();
		TrackScroll(m_wStoreScroll, m_wIndexTitle);
		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void FillDetail()
	{
		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.FindByKey(m_sOpenKey);
		if (!app || !m_wRoot || !m_Phone)
			return;

		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("DetailBadge"), false, false, false, true, ELIFE_PhoneStyle.AccentDeepFor(app.m_eState));

		//! PaintAppIcon adds a sprite each call, so clear the last one first.
		Widget iconHost = m_wRoot.FindAnyWidget("DetailIconSize");
		ClearChildren(iconHost);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (workspace)
			ELIFE_PhoneScreenShell.PaintAppIcon(workspace, iconHost, TextWidget.Cast(m_wRoot.FindAnyWidget("DetailMark")), app, DETAIL_ICON_INK);

		SetTextAndColor(m_wRoot, "DetailTitle", app.m_sLabel, ELIFE_PhoneStyle.TextPrimary());
		SetTextAndColor(m_wRoot, "DetailTagline", app.m_sTagline, ELIFE_PhoneStyle.TextSecondary());
		SetTextAndColor(m_wRoot, "DetailDescription", app.m_sDescription, ELIFE_PhoneStyle.TextPrimary());

		SetTextAndColor(m_wRoot, "InfoDeveloperValue", app.m_sDeveloper, ELIFE_PhoneStyle.TextSecondary());
		SetTextAndColor(m_wRoot, "InfoCategoryValue", app.m_sCategory, ELIFE_PhoneStyle.TextSecondary());
		SetTextAndColor(m_wRoot, "InfoStatusValue", StatusText(app), ELIFE_PhoneStyle.TextSecondary());

		PaintDetailAction(app);
		PaintDetailRemove(app);
	}

	//------------------------------------------------------------------------------------------------
	protected string StatusText(notnull ELIFE_PhoneAppEntry app)
	{
		if (app.IsSystem())
			return "#ELIFE-Phone_Store_BuiltIn";

		if (!m_Phone || !m_Phone.HasInstalledApps())
			return "…";

		if (m_Phone.IsAppInstalled(app.m_sAppKey))
			return "#ELIFE-Phone_Store_Installed";

		return "#ELIFE-Phone_Store_NotInstalled";
	}

	//------------------------------------------------------------------------------------------------
	//! Get for an app that isn't there, Open for one that is. Hidden where neither applies.
	protected void PaintDetailAction(notnull ELIFE_PhoneAppEntry app)
	{
		bool installed = app.IsSystem() || m_Phone.IsAppInstalled(app.m_sAppKey);
		bool pending = ELIFE_PhoneAppCatalog.SameKey(m_sPendingKey, app.m_sAppKey);

		//! Bystanders can't act; Open on the Store itself would go nowhere; Bridge apps wait for the real list.
		bool visible = IsOwner() && app.m_eState != GetScreenState() && (app.IsSystem() || m_Phone.HasInstalledApps());

		//! A removal in flight keeps showing Open until the push takes the app away.
		if (pending && !m_bPendingInstall)
			visible = false;

		if (m_wDetailActionSize)
			m_wDetailActionSize.SetVisible(visible);

		if (!visible)
			return;

		string text = "#ELIFE-Phone_Store_Get";
		if (pending)
			text = "#ELIFE-Phone_Store_Installing";
		else if (installed)
			text = "#ELIFE-Phone_Store_Open";

		Color fill = ELIFE_PhoneStyle.AccentDeepFor(GetScreenState());
		Color ink = ELIFE_PhoneStyle.TextPrimary();
		if (pending)
		{
			fill = ELIFE_PhoneStyle.WithAlpha(ELIFE_PhoneStyle.Hairline(), 0.9);
			ink = ELIFE_PhoneStyle.TextTertiary();
		}

		TextWidget label = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailActionLabel"));
		if (label)
		{
			label.SetText(text);
			label.SetColor(ink);

			SizeLayoutWidget size = SizeLayoutWidget.Cast(m_wDetailActionSize);
			if (size)
			{
				float textWidth, textHeight;
				label.GetTextSize(textWidth, textHeight);
				size.SetWidthOverride(textWidth + DETAIL_ACTION_PAD_H * 2);
			}
		}

		Widget fillPanel = m_wRoot.FindAnyWidget("DetailActionFill");
		if (fillPanel)
			fillPanel.SetColor(fill);

		if (m_wDetailActionButton)
			m_wDetailActionButton.SetEnabled(m_sPendingKey == "");
	}

	//------------------------------------------------------------------------------------------------
	//! Only for an installed, removable app on the owner's own phone.
	protected void PaintDetailRemove(notnull ELIFE_PhoneAppEntry app)
	{
		bool pending = ELIFE_PhoneAppCatalog.SameKey(m_sPendingKey, app.m_sAppKey) && !m_bPendingInstall;
		bool visible = IsOwner() && !app.IsSystem() && (pending || m_Phone.IsAppInstalled(app.m_sAppKey));

		if (m_wRemoveCard)
			m_wRemoveCard.SetVisible(visible);

		if (!visible)
			return;

		string text = "#ELIFE-Phone_Store_Remove";
		Color color = ELIFE_PhoneStyle.Negative();
		if (pending)
		{
			text = "#ELIFE-Phone_Store_Removing";
			color = ELIFE_PhoneStyle.TextTertiary();
		}
		else if (ELIFE_PhoneAppCatalog.SameKey(m_sArmedKey, app.m_sAppKey))
		{
			text = "#ELIFE-Phone_Store_RemoveConfirm";
		}

		SetTextAndColor(m_wRoot, "DetailRemoveLabel", text, color);

		if (m_wRemoveButton)
			m_wRemoveButton.SetEnabled(m_sPendingKey == "");
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDetailActionClicked()
	{
		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.FindByKey(m_sOpenKey);
		if (!app || !m_Phone || !IsOwner() || m_sPendingKey != "")
			return;

		if (app.IsSystem() || m_Phone.IsAppInstalled(app.m_sAppKey))
		{
			OpenAppPage(app.m_eState, "");
			return;
		}

		OnActionClicked(app.m_sAppKey);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDetailRemoveClicked()
	{
		if (m_sOpenKey != "" && m_Phone && m_Phone.IsAppInstalled(m_sOpenKey))
			OnActionClicked(m_sOpenKey);
	}
}
