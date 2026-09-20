//------------------------------------------------------------------------------------------------
//! One app on the home screen; the registry is ELIFE_PhoneScreenShell.BuildApps().
class ELIFE_PhoneAppEntry
{
	EPhoneScreenState m_eState;
	string m_sLabel;

	ResourceName m_sIconSet;
	string m_sIconImage;

	//! Fallback character mark if the sprite can't be loaded.
	string m_sGlyph;

	//! Optical size trim, for marks carrying more or less ink than the rest.
	float m_fIconScale;

	//------------------------------------------------------------------------------------------------
	void ELIFE_PhoneAppEntry(EPhoneScreenState state, string label, ResourceName iconSet, string iconImage, string glyph, float iconScale = 1.0)
	{
		m_eState = state;
		m_sLabel = label;
		m_sIconSet = iconSet;
		m_sIconImage = iconImage;
		m_sGlyph = glyph;
		m_fIconScale = iconScale;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneTileClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneScreenShell m_Shell;
	protected EPhoneScreenState m_eState;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneScreenShell shell, EPhoneScreenState state)
	{
		m_Shell = shell;
		m_eState = state;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Shell)
			m_Shell.RequestApp(m_eState);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Opens the notification's thread.
class ELIFE_PhoneBannerClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneScreenShell m_Shell;
	protected string m_sThreadId;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneScreenShell shell, string threadId)
	{
		m_Shell = shell;
		m_sThreadId = threadId;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Shell)
			m_Shell.OpenBanner(m_sThreadId);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhonePinKeyClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneScreenShell m_Shell;

	//! 0-9 for a digit, -1 for delete.
	protected int m_iDigit;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneScreenShell shell, int digit)
	{
		m_Shell = shell;
		m_iDigit = digit;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_Shell)
			return false;

		if (m_iDigit < 0)
			m_Shell.PinBackspace();
		else
			m_Shell.PinPush(m_iDigit);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Drives PhoneScreen.layout. The held phone's screen and bystanders' copies each run one.
class ELIFE_PhoneScreenShell
{
	protected const ResourceName LAYOUT_TILE = "{7C12D4A9E3B25F83}UI/layouts/Menus/Phone/Widgets/PhoneAppTile.layout";
	protected const ResourceName LAYOUT_GRID_ROW = "{7C25D4A9E3B25F8A}UI/layouts/Menus/Phone/Widgets/PhoneGridRow.layout";
	protected const ResourceName LAYOUT_PIN_KEY = "{7C24D4A9E3B25F89}UI/layouts/Menus/Phone/Widgets/PhonePinKey.layout";
	//! 1x banner card for the peek, same widget names as LAYOUT_NOTIFICATION.
	protected const ResourceName LAYOUT_PEEK_CARD = "{5A3C91E7D2B84F20}UI/layouts/Menus/Phone/Widgets/PhonePeekCard.layout";
	protected const ResourceName LAYOUT_NOTIFICATION = "{7C13D4A9E3B25F84}UI/layouts/Menus/Phone/Widgets/PhoneNotificationCard.layout";
	protected const ResourceName LAYOUT_HOME_CARD = "{7C26D4A9E3B25F8B}UI/layouts/Menus/Phone/Widgets/PhoneHomeCard.layout";

	protected const ResourceName LAYOUT_APP_ICON = "{7C27D4A9E3B25F8C}UI/layouts/Menus/Phone/Widgets/PhoneAppIcon.layout";

	//! Visible ink an app icon should show, on a tile and on a home-card badge.
	protected const float ICON_INK_TILE = 52;
	protected const float ICON_INK_CARD = 36;

	//! Icon sprites have transparent padding, so the box is enlarged until the ink matches.
	protected const float ICON_SPRITE_PADDING = 1.34;

	//! Cap height is ~0.72 of font size, so marks are set larger than the ink they should show.
	protected const float ICON_MARK_CAP = 1.38;

	protected const int GRID_COLUMNS = 4;
	protected const int PIN_LENGTH = 4;
	protected const int PIN_ERROR_HOLD_MS = 900;
	protected const float PIN_KEY_DISABLED_OPACITY = 0.4;

	//! Gives up waiting for the server's verdict and clears the entry.
	protected const int UNLOCK_REPLY_TIMEOUT_MS = 3000;

	//! × sits low and small next to digits at the same size.
	protected const int PIN_DELETE_SIZE = 54;
	protected const int LOCK_NOTIFICATION_LIMIT = 3;

	//! Banners only show over home or an open app; the lock screen has its own list.
	protected const int BANNER_LIMIT = 3;
	protected const int BANNER_DURATION_MS = 5000;

	protected Widget m_wNotificationBanner;
	protected ref array<ref ELIFE_PhoneBannerClick> m_aBannerClicks = {};

	//! One tween per card, so a new arrival doesn't restart its neighbour's fade.
	protected ref array<ref ELIFE_PhoneTween> m_aBannerTweens = {};

	//! Cards fading out, which SettleBannerTweens() must leave alone.
	protected ref array<Widget> m_aExitingBanners = {};

	//! Open state lives on the phone (replicated), so bystanders see the hub too.
	protected Widget m_wNotificationHub;
	protected Widget m_wHubList;
	protected Widget m_wHubEmpty;
	protected Widget m_wHubClearSize;
	protected Widget m_wStatusNotifySize;
	protected TextWidget m_wStatusNotifyCount;
	protected ref array<ref ELIFE_PhoneBannerClick> m_aHubClicks = {};

	//! threadId -> unread count already seen. A banner fires when a count rises past this.
	protected ref map<string, int> m_mSeenUnread = new map<string, int>();

	//! The first payload only seeds the baseline, so opening the phone doesn't replay old unread.
	protected bool m_bUnreadSeeded;

	protected bool m_bBannerOnly;

	//! Compact card size relative to the 2x card, tuned so the peek matches the banner on the held phone.
	//! PhonePeekCard.layout is authored at this scale.
	protected const float COMPACT_SCALE = 0.55;

	//! BackBody's 16px padding on each side plus the chevron's 6px gap to the label.
	protected const float BACK_CHIP_CHROME = 38;

	protected Widget m_wRoot;
	protected Widget m_wLockScreen;
	protected Widget m_wHomeScreen;
	protected Widget m_wAppHost;
	protected Widget m_wNavBarSize;
	protected Widget m_wNavBarGlass;
	protected Widget m_wStatusBarGlass;
	protected Widget m_wNavActionSize;
	protected Widget m_wNavActionButton;
	protected Widget m_wNavActionChip;
	protected Widget m_wNavActionIconSize;
	protected ImageWidget m_wNavActionIcon;
	protected TextWidget m_wNavActionLabel;
	protected ref ELIFE_PhoneChrome m_Chrome = new ELIFE_PhoneChrome();
	protected TextWidget m_wNavTitle;
	protected Widget m_wHomeIndicator;
	protected Widget m_wScreenOff;
	protected Widget m_wOfflineScreen;
	protected Widget m_wWallpaper;
	protected Widget m_wAppWashSize;
	protected ImageWidget m_wAppWash;
	protected Widget m_wHomeGridRows;
	protected Widget m_wHomeCards;
	protected TextWidget m_wHomeDate;
	protected Widget m_wLockNotifications;
	protected Widget m_wPinRows;
	protected TextWidget m_wLockDate;
	protected TextWidget m_wLockNumber;
	protected TextWidget m_wPinPrompt;
	protected TextWidget m_wStatusNumber;
	protected Widget m_wBackChipSize;
	protected Widget m_wButtonBack;
	protected TextWidget m_wBackChevron;
	protected TextWidget m_wBackLabel;

	protected ELIFE_PhoneGadgetComponent m_Phone;
	protected bool m_bInteractive;
	protected ref ELIFE_PhoneAppBase m_App;
	protected EPhoneScreenState m_eState = EPhoneScreenState.OFF;
	protected ref array<ref ELIFE_PhoneTileClick> m_aTileClicks = {};
	protected ref array<ref ELIFE_PhoneTileClick> m_aCardClicks = {};
	protected ref array<ref ELIFE_PhonePinKeyClick> m_aPinClicks = {};
	protected ref array<Widget> m_aPinKeys = {};
	protected ref array<Widget> m_aPinDots = {};
	protected ref ELIFE_PhoneTween m_AppEntrance = new ELIFE_PhoneTween();
	protected string m_sPinEntry;
	protected bool m_bPinError;
	protected bool m_bUnlockPending;
	protected float m_fPinBlockedUntil;

	//! (state, subState) from a tile or an in-app hand-off. The controller decides what it means.
	protected ref ScriptInvoker m_OnAppRequested = new ScriptInvoker();

	//! Cross-app jump requested from inside an app, held for a frame - see RequestAppPage().
	protected EPhoneScreenState m_ePendingApp;
	protected string m_sPendingSubState;
	protected ref ScriptInvoker m_OnHomePill = new ScriptInvoker();
	protected ref ScriptInvoker m_OnBack = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! Home-grid order, rows of four.
	static array<ref ELIFE_PhoneAppEntry> BuildApps()
	{
		array<ref ELIFE_PhoneAppEntry> apps = {};
		apps.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.MESSAGES, "#ELIFE-Phone_App_Messages", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "comments", ""));
		apps.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.CONTACTS, "#ELIFE-Phone_App_Contacts", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "player", ""));
		apps.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.BANK, "#ELIFE-Phone_App_Bank", ResourceName.Empty, "", ELIFE_PhoneStyle.CURRENCY_SYMBOL));
		apps.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.MAP, "#ELIFE-Phone_App_Map", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "compass", ""));
		apps.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.SETTINGS, "#ELIFE-Phone_App_Settings", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "settings", ""));
		return apps;
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_PhoneAppEntry FindApp(EPhoneScreenState state)
	{
		array<ref ELIFE_PhoneAppEntry> apps = BuildApps();
		foreach (ELIFE_PhoneAppEntry app : apps)
		{
			if (app.m_eState == state)
				return app;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Null for states without an AppHost page.
	static ELIFE_PhoneAppBase CreateApp(EPhoneScreenState state)
	{
		switch (state)
		{
			case EPhoneScreenState.MESSAGES: return new ELIFE_PhoneMessagesApp();
			case EPhoneScreenState.CONTACTS: return new ELIFE_PhoneContactsApp();
			case EPhoneScreenState.BANK: return new ELIFE_PhoneBankingApp();
			case EPhoneScreenState.SETTINGS: return new ELIFE_PhoneSettingsApp();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Map has no AppHost page - it opens the fullscreen map menu.
	static bool HasAppPage(EPhoneScreenState state)
	{
		return state == EPhoneScreenState.MESSAGES
			|| state == EPhoneScreenState.CONTACTS
			|| state == EPhoneScreenState.BANK
			|| state == EPhoneScreenState.SETTINGS;
	}

	//------------------------------------------------------------------------------------------------
	void Init(notnull Widget screenRoot, notnull ELIFE_PhoneGadgetComponent phone, bool interactive)
	{
		m_wRoot = screenRoot;
		m_Phone = phone;
		m_bInteractive = interactive;

		m_wLockScreen = screenRoot.FindAnyWidget("LockScreen");
		m_wHomeScreen = screenRoot.FindAnyWidget("HomeScreen");
		m_wAppHost = screenRoot.FindAnyWidget("AppHost");
		m_wNavBarSize = screenRoot.FindAnyWidget("NavBarSize");
		m_wNavBarGlass = screenRoot.FindAnyWidget("NavBarGlass");
		m_wNavTitle = TextWidget.Cast(screenRoot.FindAnyWidget("NavTitle"));
		m_wNavActionSize = screenRoot.FindAnyWidget("NavActionSize");
		m_wNavActionButton = screenRoot.FindAnyWidget("ButtonNavAction");
		m_wNavActionChip = screenRoot.FindAnyWidget("NavActionChip");
		m_wNavActionIconSize = screenRoot.FindAnyWidget("NavActionIconSize");
		m_wNavActionIcon = ImageWidget.Cast(screenRoot.FindAnyWidget("NavActionIcon"));
		m_wNavActionLabel = TextWidget.Cast(screenRoot.FindAnyWidget("NavActionLabel"));
		m_wStatusBarGlass = screenRoot.FindAnyWidget("StatusBarGlass");
		m_wBackChipSize = screenRoot.FindAnyWidget("BackChipSize");
		m_wButtonBack = screenRoot.FindAnyWidget("ButtonBack");

		m_Chrome.m_wStatusBarGlass = m_wStatusBarGlass;
		m_Chrome.m_wNavBarGlass = m_wNavBarGlass;
		m_Chrome.m_wBackChipSize = m_wBackChipSize;
		m_Chrome.m_wNavTitle = m_wNavTitle;
		m_Chrome.m_wNavActionSize = m_wNavActionSize;
		m_Chrome.m_wNavActionButton = m_wNavActionButton;
		m_Chrome.m_wNavActionChip = m_wNavActionChip;
		m_Chrome.m_wNavActionIconSize = m_wNavActionIconSize;
		m_Chrome.m_wNavActionIcon = m_wNavActionIcon;
		m_Chrome.m_wNavActionLabel = m_wNavActionLabel;
		m_wHomeIndicator = screenRoot.FindAnyWidget("HomeIndicatorSize");
		m_wScreenOff = screenRoot.FindAnyWidget("ScreenOff");
		m_wOfflineScreen = screenRoot.FindAnyWidget("OfflineScreen");
		m_wWallpaper = screenRoot.FindAnyWidget("Wallpaper");
		m_wAppWashSize = screenRoot.FindAnyWidget("AppWashSize");
		m_wAppWash = ImageWidget.Cast(screenRoot.FindAnyWidget("AppWash"));
		m_wHomeGridRows = screenRoot.FindAnyWidget("HomeGridRows");
		m_wHomeCards = screenRoot.FindAnyWidget("HomeCards");
		m_wHomeDate = TextWidget.Cast(screenRoot.FindAnyWidget("HomeDate"));
		m_wLockNotifications = screenRoot.FindAnyWidget("LockNotifications");
		m_wNotificationBanner = screenRoot.FindAnyWidget("NotificationBanner");
		m_wNotificationHub = screenRoot.FindAnyWidget("NotificationHub");
		m_wHubList = screenRoot.FindAnyWidget("HubList");
		m_wHubEmpty = screenRoot.FindAnyWidget("HubEmpty");
		m_wHubClearSize = screenRoot.FindAnyWidget("HubClearSize");
		m_wStatusNotifySize = screenRoot.FindAnyWidget("StatusNotifySize");
		m_wStatusNotifyCount = TextWidget.Cast(screenRoot.FindAnyWidget("StatusNotifyCount"));
		m_wPinRows = screenRoot.FindAnyWidget("PinRows");
		m_wLockDate = TextWidget.Cast(screenRoot.FindAnyWidget("LockDate"));
		m_wLockNumber = TextWidget.Cast(screenRoot.FindAnyWidget("LockNumber"));
		m_wPinPrompt = TextWidget.Cast(screenRoot.FindAnyWidget("PinPrompt"));
		m_wStatusNumber = TextWidget.Cast(screenRoot.FindAnyWidget("StatusNumber"));
		m_wBackChevron = TextWidget.Cast(screenRoot.FindAnyWidget("BackChevron"));
		m_wBackLabel = TextWidget.Cast(screenRoot.FindAnyWidget("BackLabel"));

		ApplyDepth();
		BuildHomeGrid();
		BuildPinPad();
		CachePinDots();

		RefreshStatusBar();

		//! Provisioning usually lands after the first draw, so refresh the number when it does.
		phone.m_OnIdentityChanged.Insert(OnIdentityChanged);

		phone.m_OnConnectivityChanged.Insert(OnConnectivityChanged);
		RefreshOfflineScreen();

		//! Home and lock render from the messages cache, which the poll keeps refreshing.
		phone.m_OnDataChanged.Insert(OnDataChanged);

		phone.m_OnHubOpenChanged.Insert(OnHubOpenChanged);
		ApplyHubGlass();
		RefreshNotifications();

		//! Seed from the cache now so the wake poll can still count as an arrival.
		RaiseArrivedBanners();

		//! Home and lock resolve thread names through the contact book, so fetch contacts here too.
		phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);

		if (!m_bInteractive)
			return;

		phone.m_OnUnlockRejected.Insert(OnUnlockRejected);

		SCR_ButtonTextComponent homePill = SCR_ButtonTextComponent.GetButtonText("ButtonClose", screenRoot);
		if (homePill)
			homePill.m_OnClicked.Insert(OnHomePillClicked);

		SCR_ButtonTextComponent back = SCR_ButtonTextComponent.GetButtonText("ButtonBack", screenRoot);
		if (back)
			back.m_OnClicked.Insert(OnBackClicked);

		SCR_ButtonTextComponent offlineRetry = SCR_ButtonTextComponent.GetButtonText("OfflineRetry", screenRoot);
		if (offlineRetry)
			offlineRetry.m_OnClicked.Insert(OnOfflineRetryClicked);

		//! The whole status bar toggles the hub.
		SCR_ButtonTextComponent statusBar = SCR_ButtonTextComponent.GetButtonText("StatusBarButton", screenRoot);
		if (statusBar)
			statusBar.m_OnClicked.Insert(OnStatusBarClicked);

		SCR_ButtonTextComponent hubClear = SCR_ButtonTextComponent.GetButtonText("HubClearButton", screenRoot);
		if (hubClear)
			hubClear.m_OnClicked.Insert(OnHubClearClicked);
	}

	//------------------------------------------------------------------------------------------------
	void Destroy()
	{
		if (m_Phone)
		{
			m_Phone.m_OnIdentityChanged.Remove(OnIdentityChanged);
			m_Phone.m_OnConnectivityChanged.Remove(OnConnectivityChanged);
			m_Phone.m_OnDataChanged.Remove(OnDataChanged);
			m_Phone.m_OnHubOpenChanged.Remove(OnHubOpenChanged);
			m_Phone.m_OnUnlockRejected.Remove(OnUnlockRejected);
		}

		m_aHubClicks.Clear();

		ClearBanners();

		//! The contact book is process-wide, so drop it with the screen that filled it.
		ELIFE_PhoneContactBook.Clear();

		CloseApp();

		if (m_AppEntrance)
			m_AppEntrance.Stop();

		GetGame().GetCallqueue().Remove(ClearPinError);
		GetGame().GetCallqueue().Remove(OnUnlockTimeout);
		GetGame().GetCallqueue().Remove(TickPinBlock);
		GetGame().GetCallqueue().Remove(FlushPendingApp);

		m_aTileClicks.Clear();
		m_aCardClicks.Clear();
		m_aPinClicks.Clear();
		m_aPinKeys.Clear();
		m_aPinDots.Clear();
		m_Phone = null;
		m_wRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	ScriptInvoker GetOnAppRequested() { return m_OnAppRequested; }
	ScriptInvoker GetOnHomePill() { return m_OnHomePill; }
	ScriptInvoker GetOnBack() { return m_OnBack; }
	ELIFE_PhoneAppBase GetOpenApp() { return m_App; }
	EPhoneScreenState GetState() { return m_eState; }

	//------------------------------------------------------------------------------------------------
	//! Layer order from the ZORDER tokens.
	protected void ApplyDepth()
	{
		Widget ground = m_wRoot.FindAnyWidget("ScreenGround");
		if (ground)
			ground.SetZOrder(ELIFE_PhoneStyle.ZORDER_GROUND);

		if (m_wWallpaper)
			m_wWallpaper.SetZOrder(ELIFE_PhoneStyle.ZORDER_GROUND);

		if (ELIFE_PhoneStyle.GRADIENT_FLIPPED)
		{
			ImageWidget glow = ImageWidget.Cast(m_wRoot.FindAnyWidget("WallpaperGlow"));
			if (glow)
				glow.SetRotation(180);
		}

		Widget stage = m_wRoot.FindAnyWidget("ScreenStage");
		if (stage)
			stage.SetZOrder(ELIFE_PhoneStyle.ZORDER_CONTENT);

		if (m_wNavBarSize)
			m_wNavBarSize.SetZOrder(ELIFE_PhoneStyle.ZORDER_CHROME);

		Widget statusBar = m_wRoot.FindAnyWidget("StatusBarSize");
		if (statusBar)
			statusBar.SetZOrder(ELIFE_PhoneStyle.ZORDER_SYSTEM);

		if (m_wHomeIndicator)
			m_wHomeIndicator.SetZOrder(ELIFE_PhoneStyle.ZORDER_SYSTEM);

		if (m_wScreenOff)
			m_wScreenOff.SetZOrder(ELIFE_PhoneStyle.ZORDER_ALERT);

		//! Same tier as ScreenOff; the two are never visible together.
		if (m_wOfflineScreen)
			m_wOfflineScreen.SetZOrder(ELIFE_PhoneStyle.ZORDER_ALERT);

		if (m_wNotificationBanner)
			m_wNotificationBanner.SetZOrder(ELIFE_PhoneStyle.ZORDER_BANNER);

		if (m_wNotificationHub)
			m_wNotificationHub.SetZOrder(ELIFE_PhoneStyle.ZORDER_SHEET);

		//! Status/nav glass starts hidden - ApplyCollapse reveals it on scroll.
		ELIFE_PhoneStyle.ApplyGlass(m_wStatusBarGlass, true, false, true);
		ELIFE_PhoneStyle.ApplyGlass(m_wNavBarGlass, true, false, true);
		if (m_wStatusBarGlass)
			m_wStatusBarGlass.SetOpacity(0);
		if (m_wNavBarGlass)
			m_wNavBarGlass.SetOpacity(0);

		//! Back is plain text; hide any glass authored on the chip.
		ELIFE_PhoneStyle.SetGlassPresence(m_wBackChipSize, 0);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildHomeGrid()
	{
		if (!m_wHomeGridRows)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_aTileClicks.Clear();

		array<ref ELIFE_PhoneAppEntry> apps = BuildApps();
		int count = apps.Count();
		int rows = (count + GRID_COLUMNS - 1) / GRID_COLUMNS;

		for (int rowIndex = 0; rowIndex < rows; rowIndex++)
		{
			Widget row = workspace.CreateWidgets(LAYOUT_GRID_ROW, m_wHomeGridRows);
			if (!row)
				continue;

			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);

			if (rowIndex > 0)
				LayoutSlot.SetPadding(row, 0, ELIFE_PhoneStyle.SPACE_4, 0, 0);

			for (int column = 0; column < GRID_COLUMNS; column++)
			{
				int appIndex = rowIndex * GRID_COLUMNS + column;

				//! Keeps column rhythm on a part-filled last row.
				if (appIndex >= count)
				{
					Widget spacer = workspace.CreateWidgets(LAYOUT_GRID_ROW, row);
					if (spacer)
						LayoutSlot.SetSizeMode(spacer, LayoutSizeMode.Fill);

					continue;
				}

				CreateTile(workspace, row, apps.Get(appIndex));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void CreateTile(notnull WorkspaceWidget workspace, notnull Widget row, notnull ELIFE_PhoneAppEntry app)
	{
		Widget tile = workspace.CreateWidgets(LAYOUT_TILE, row);
		if (!tile)
			return;

		LayoutSlot.SetSizeMode(tile, LayoutSizeMode.Fill);

		ELIFE_PhoneStyle.ApplyGlass(tile, false, false, false, true, ELIFE_PhoneStyle.AccentDeepFor(app.m_eState));

		PaintAppIcon(workspace, tile.FindAnyWidget("TileIconSize"), TextWidget.Cast(tile.FindAnyWidget("TileMark")), app, ICON_INK_TILE);

		TextWidget label = TextWidget.Cast(tile.FindAnyWidget("TileLabel"));
		if (label)
		{
			label.SetText(app.m_sLabel);
			label.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}

		if (!m_bInteractive)
			return;

		Widget button = tile.FindAnyWidget("TileButton");
		if (!button)
			return;

		ELIFE_PhoneTileClick click = new ELIFE_PhoneTileClick();
		click.Bind(this, app.m_eState);
		button.AddHandler(click);
		m_aTileClicks.Insert(click);
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the app's sprite, or falls back to its character mark centred in the full box and sized by
	//! cap height (a text widget centres its line box, so a sprite-sized box would clip it).
	protected void PaintAppIcon(notnull WorkspaceWidget workspace, Widget iconHost, TextWidget mark, ELIFE_PhoneAppEntry app, float ink)
	{
		if (!app)
			return;

		Color tint = ELIFE_PhoneStyle.TextPrimary();
		bool loaded = false;

		if (iconHost && app.m_sIconSet != ResourceName.Empty && app.m_sIconImage != "")
		{
			Widget icon = workspace.CreateWidgets(LAYOUT_APP_ICON, iconHost);
			if (icon)
			{
				AlignableSlot.SetHorizontalAlign(icon, LayoutHorizontalAlign.Stretch);
				AlignableSlot.SetVerticalAlign(icon, LayoutVerticalAlign.Stretch);

				ImageWidget image = ImageWidget.Cast(icon.FindAnyWidget("IconImage"));
				if (image)
				{
					loaded = image.LoadImageFromSet(0, app.m_sIconSet, app.m_sIconImage);
					image.SetVisible(loaded);

					if (loaded)
					{
						image.SetColor(tint);
						ELIFE_PhoneStyle.FitIcon(image, ink * ICON_SPRITE_PADDING * app.m_fIconScale);
					}
				}
			}
		}

		if (!mark)
			return;

		mark.SetVisible(!loaded);
		if (loaded)
			return;

		mark.SetText(app.m_sGlyph);
		mark.SetColor(tint);

		mark.SetExactFontSize(Math.Round(ink * ICON_MARK_CAP * app.m_fIconScale));
	}

	//------------------------------------------------------------------------------------------------
	//! 1-9, then a blank, 0 and delete. Built on read-only copies too, so a locked phone looks locked.
	protected void BuildPinPad()
	{
		if (!m_wPinRows)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_aPinClicks.Clear();
		m_aPinKeys.Clear();

		for (int rowIndex = 0; rowIndex < 4; rowIndex++)
		{
			Widget row = workspace.CreateWidgets(LAYOUT_GRID_ROW, m_wPinRows);
			if (!row)
				continue;

			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Center);

			if (rowIndex < 3)
				LayoutSlot.SetPadding(row, 0, 0, 0, ELIFE_PhoneStyle.SPACE_2);

			for (int column = 0; column < 3; column++)
			{
				bool trailingGap = column < 2;

				if (rowIndex < 3)
				{
					int digit = rowIndex * 3 + column + 1;
					CreatePinKey(workspace, row, digit.ToString(), digit, trailingGap);
					continue;
				}

				//! Blank placeholder keeps the 0 centred.
				if (column == 0)
					CreatePinKey(workspace, row, "", -2, trailingGap);
				else if (column == 1)
					CreatePinKey(workspace, row, "0", 0, trailingGap);
				else
					CreatePinKey(workspace, row, "×", -1, trailingGap, PIN_DELETE_SIZE);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! digit: 0-9, -1 delete, -2 inert placeholder.
	protected void CreatePinKey(notnull WorkspaceWidget workspace, notnull Widget row, string label, int digit, bool trailingGap, int labelSize = 0)
	{
		Widget key = workspace.CreateWidgets(LAYOUT_PIN_KEY, row);
		if (!key)
			return;

		if (trailingGap)
			LayoutSlot.SetPadding(key, 0, 0, ELIFE_PhoneStyle.SPACE_2, 0);

		if (digit == -2)
		{
			key.SetOpacity(0);
			return;
		}

		ELIFE_PhoneStyle.ApplyGlass(key, false, true);
		m_aPinKeys.Insert(key);

		TextWidget keyLabel = TextWidget.Cast(key.FindAnyWidget("KeyLabel"));
		if (keyLabel)
		{
			keyLabel.SetText(label);
			keyLabel.SetColor(ELIFE_PhoneStyle.TextPrimary());

			if (labelSize > 0)
				keyLabel.SetExactFontSize(labelSize);
		}

		if (!m_bInteractive)
			return;

		Widget button = key.FindAnyWidget("KeyButton");
		if (!button)
			return;

		ELIFE_PhonePinKeyClick click = new ELIFE_PhonePinKeyClick();
		click.Bind(this, digit);
		button.AddHandler(click);
		m_aPinClicks.Insert(click);
	}

	//------------------------------------------------------------------------------------------------
	protected void CachePinDots()
	{
		m_aPinDots.Clear();

		if (!m_wRoot)
			return;

		for (int i = 0; i < PIN_LENGTH; i++)
		{
			Widget dot = m_wRoot.FindAnyWidget("PinDot" + i.ToString());
			if (dot)
				m_aPinDots.Insert(dot);
		}
	}

	//------------------------------------------------------------------------------------------------
	void RequestApp(EPhoneScreenState state)
	{
		m_OnAppRequested.Invoke(state, "");
	}

	//------------------------------------------------------------------------------------------------
	//! Deferred a frame: the caller is a click handler in the app that this closes.
	void RequestAppPage(EPhoneScreenState state, string subState)
	{
		m_ePendingApp = state;
		m_sPendingSubState = subState;

		GetGame().GetCallqueue().Remove(FlushPendingApp);
		GetGame().GetCallqueue().CallLater(FlushPendingApp, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void FlushPendingApp()
	{
		m_OnAppRequested.Invoke(m_ePendingApp, m_sPendingSubState);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHomePillClicked()
	{
		//! Home closes the hub first, leaving the page under it open.
		if (m_Phone && m_Phone.IsHubOpen())
		{
			m_Phone.SetHubOpen(false);
			return;
		}

		m_OnHomePill.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnBackClicked()
	{
		m_OnBack.Invoke();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnIdentityChanged()
	{
		RefreshStatusBar();

		if (m_wLockNumber && m_Phone)
			m_wLockNumber.SetText(m_Phone.GetNumber());
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshStatusBar()
	{
		if (m_wStatusNumber && m_Phone)
			m_wStatusNumber.SetText(m_Phone.GetNumber());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnConnectivityChanged()
	{
		RefreshOfflineScreen();
	}

	//------------------------------------------------------------------------------------------------
	//! Home and lock only; open apps subscribe themselves.
	protected void OnDataChanged(string key)
	{
		if (key != ELIFE_PhoneGadgetComponent.DATA_MESSAGES && key != ELIFE_PhoneGadgetComponent.DATA_CONTACTS)
			return;

		if (m_eState == EPhoneScreenState.LOCKED)
			RefreshLockScreen();
		else if (m_eState == EPhoneScreenState.HOME)
			RefreshHomeScreen();

		if (key != ELIFE_PhoneGadgetComponent.DATA_MESSAGES)
			return;

		RaiseArrivedBanners();

		//! After the banners, so a new arrival also lands in the list.
		RefreshNotifications();
	}

	//------------------------------------------------------------------------------------------------
	// Notification banners
	//------------------------------------------------------------------------------------------------

	//! For the peek: shows only the banner layer, from the 1x card, and lets the cursor pass through.
	void UseBannerOnly()
	{
		m_bBannerOnly = true;

		//! On the phone the banner sits below the status bar; alone it starts at the top.
		if (m_wNotificationBanner)
			AlignableSlot.SetPadding(m_wNotificationBanner, 0, 0, 0, 0);

		HideAllButBanner();
		IgnoreCursorTree(m_wRoot);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-applied after anything that toggles screen layers.
	protected void HideAllButBanner()
	{
		if (!m_bBannerOnly || !m_wRoot)
			return;

		Widget child = m_wRoot.GetChildren();
		while (child)
		{
			if (child != m_wNotificationBanner)
				child.SetVisible(false);

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected static void IgnoreCursorTree(Widget widget)
	{
		if (!widget)
			return;

		widget.SetFlags(WidgetFlags.IGNORE_CURSOR);

		Widget child = widget.GetChildren();
		while (child)
		{
			IgnoreCursorTree(child);
			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Diff against what this screen has already accounted for and banner whatever went up.
	void RaiseArrivedBanners()
	{
		if (!m_Phone)
			return;

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json == "")
			return;

		ELIFE_MessageUpdatesDto updates = new ELIFE_MessageUpdatesDto();
		updates.ExpandFromRAW(json);

		//! Record every thread, so one dropping to zero and rising again reads as new.
		array<ref ELIFE_ThreadDisplayDto> arrived = {};

		foreach (ELIFE_ThreadDisplayDto threadDto : updates.threads)
		{
			if (!threadDto)
				continue;

			int seen = 0;
			m_mSeenUnread.Find(threadDto.threadId, seen);

			if (m_bUnreadSeeded && threadDto.unreadCount > seen)
				arrived.Insert(threadDto);

			m_mSeenUnread.Set(threadDto.threadId, threadDto.unreadCount);
		}

		m_bUnreadSeeded = true;

		foreach (ELIFE_ThreadDisplayDto threadDto : arrived)
			ShowBanner(threadDto);
	}

	//------------------------------------------------------------------------------------------------
	//! Peek: banner what's already waiting, then seed so later polls only raise new ones.
	void RaisePendingBanners()
	{
		array<ref ELIFE_ThreadDisplayDto> pending = {};
		CollectNotifications(pending);

		foreach (ELIFE_ThreadDisplayDto threadDto : pending)
			ShowBanner(threadDto);

		m_bUnreadSeeded = false;
		RaiseArrivedBanners();
	}

	//------------------------------------------------------------------------------------------------
	//! Awake screens only - lock has its own list. Bystander copies show banners too, just not clickable.
	protected bool CanShowBanner()
	{
		return m_eState != EPhoneScreenState.OFF && m_eState != EPhoneScreenState.LOCKED;
	}

	//------------------------------------------------------------------------------------------------
	//! The open hub already lists it.
	protected bool CanRaiseBanner()
	{
		return CanShowBanner() && m_Phone && !m_Phone.IsHubOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! The conversation is open on screen, so it's being read as it arrives.
	protected bool IsThreadOnScreen(string threadId)
	{
		if (threadId == "" || m_eState != EPhoneScreenState.MESSAGES || !m_Phone)
			return false;

		return m_Phone.GetScreenSubState() == threadId;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowBanner(notnull ELIFE_ThreadDisplayDto threadDto)
	{
		if (!m_wNotificationBanner || !CanRaiseBanner())
			return;

		if (IsThreadOnScreen(threadDto.threadId))
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		//! Drop the oldest so the newest arrival always shows.
		while (CountChildren(m_wNotificationBanner) >= BANNER_LIMIT)
		{
			Widget oldest = m_wNotificationBanner.GetChildren();
			if (!oldest)
				break;

			SettleBannerTweens();
			oldest.RemoveFromHierarchy();
		}

		ResourceName bannerLayout = LAYOUT_NOTIFICATION;
		float bannerGap = ELIFE_PhoneStyle.SPACE_1;
		if (m_bBannerOnly)
		{
			bannerLayout = LAYOUT_PEEK_CARD;
			bannerGap = bannerGap * COMPACT_SCALE;
		}

		Widget card = workspace.CreateWidgets(bannerLayout, m_wNotificationBanner);
		if (!card)
			return;

		AlignableSlot.SetHorizontalAlign(card, LayoutHorizontalAlign.Stretch);
		if (m_wNotificationBanner.GetChildren() != card)
			LayoutSlot.SetPadding(card, 0, bannerGap, 0, 0);

		PaintNotificationCard(card, threadDto, false, m_bBannerOnly);

		if (m_bBannerOnly)
			IgnoreCursorTree(card);

		m_wNotificationBanner.SetVisible(true);

		BindBannerClick(card, threadDto.threadId);

		ELIFE_PhoneTween entrance = new ELIFE_PhoneTween();
		entrance.Play(card, 0, 1, ELIFE_PhoneStyle.DURATION_PRESENT_MS);
		m_aBannerTweens.Insert(entrance);

		//! Per-card deadline, so close arrivals don't share one.
		GetGame().GetCallqueue().CallLater(DismissBanner, BANNER_DURATION_MS, false, card);
	}

	//------------------------------------------------------------------------------------------------
	//! Snaps in-flight entrances to rest before a card is torn out from under its tween.
	protected void SettleBannerTweens()
	{
		if (m_aBannerTweens.IsEmpty() || !m_wNotificationBanner)
			return;

		m_aBannerTweens.Clear();

		Widget child = m_wNotificationBanner.GetChildren();
		while (child)
		{
			//! Leave exiting cards as they are; only an interrupted entrance needs rescuing.
			if (m_aExitingBanners.Find(child) == -1)
				child.SetOpacity(1);

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Lock-screen copies of the card aren't clickable, so its button ships hidden.
	protected void BindBannerClick(notnull Widget card, string threadId)
	{
		if (!m_bInteractive)
			return;

		Widget button = card.FindAnyWidget("NotifyButton");
		if (!button)
			return;

		button.SetVisible(true);

		ELIFE_PhoneBannerClick click = new ELIFE_PhoneBannerClick();
		click.Bind(this, threadId);
		button.AddHandler(click);
		m_aBannerClicks.Insert(click);
	}

	//------------------------------------------------------------------------------------------------
	// Notification hub
	//------------------------------------------------------------------------------------------------

	//! Heaviest dark scrim - the hub takes over the page rather than sitting on it.
	protected void ApplyHubGlass()
	{
		if (!m_wNotificationHub)
			return;

		ELIFE_PhoneStyle.ApplyGlass(m_wNotificationHub, true, false, true);
		ELIFE_PhoneStyle.ApplyGlass(m_wNotificationHub.FindAnyWidget("HubClearChip"), false, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Threads with an undismissed notification, newest first (payload order).
	protected void CollectNotifications(notnull array<ref ELIFE_ThreadDisplayDto> outThreads)
	{
		if (!m_Phone)
			return;

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json == "")
			return;

		ELIFE_MessageUpdatesDto updates = new ELIFE_MessageUpdatesDto();
		updates.ExpandFromRAW(json);

		foreach (ELIFE_ThreadDisplayDto threadDto : updates.threads)
		{
			if (!threadDto || threadDto.unreadCount <= 0)
				continue;

			//! Hides threads the reader cleared, until the next message arrives on them.
			if (m_Phone.IsNotificationDismissed(threadDto.threadId, threadDto.unreadCount))
				continue;

			if (IsThreadOnScreen(threadDto.threadId))
				continue;

			outThreads.Insert(threadDto);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Newest outstanding notification - the peek's door when the phone is drawn.
	string GetNewestNotificationThreadId()
	{
		array<ref ELIFE_ThreadDisplayDto> pending = {};
		CollectNotifications(pending);

		if (pending.IsEmpty() || !pending[0])
			return "";

		return pending[0].threadId;
	}

	//------------------------------------------------------------------------------------------------
	//! Indicator and hub read one list, so they can't disagree.
	protected void RefreshNotifications()
	{
		array<ref ELIFE_ThreadDisplayDto> pending = {};
		CollectNotifications(pending);

		//! Counts messages, not threads.
		int unread = 0;
		foreach (ELIFE_ThreadDisplayDto threadDto : pending)
			unread += threadDto.unreadCount;

		RefreshNotificationIndicator(unread);
		FillHub(pending);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshNotificationIndicator(int count)
	{
		//! Hidden when locked or dark - the count alone reveals someone is writing.
		bool show = count > 0 && CanShowBanner();

		if (m_wStatusNotifySize)
			m_wStatusNotifySize.SetVisible(show);

		if (!show || !m_wStatusNotifyCount)
			return;

		m_wStatusNotifyCount.SetText(count.ToString());
		ELIFE_PhoneStyle.SetColorOf(m_wStatusNotifySize, "StatusNotifyFill", ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES));
	}

	//------------------------------------------------------------------------------------------------
	protected void FillHub(notnull array<ref ELIFE_ThreadDisplayDto> pending)
	{
		if (!m_wHubList)
			return;

		m_aHubClicks.Clear();
		ClearChildren(m_wHubList);

		int count = pending.Count();

		if (m_wHubEmpty)
			m_wHubEmpty.SetVisible(count == 0);

		if (m_wHubClearSize)
			m_wHubClearSize.SetVisible(count > 0);

		if (count == 0)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		for (int i = 0; i < count; i++)
		{
			ELIFE_ThreadDisplayDto threadDto = pending.Get(i);

			Widget card = workspace.CreateWidgets(LAYOUT_NOTIFICATION, m_wHubList);
			if (!card)
				continue;

			AlignableSlot.SetHorizontalAlign(card, LayoutHorizontalAlign.Stretch);
			if (i > 0)
				LayoutSlot.SetPadding(card, 0, ELIFE_PhoneStyle.SPACE_1, 0, 0);

			//! Only the hub shows a per-row count.
			PaintNotificationCard(card, threadDto, true);

			if (!m_bInteractive)
				continue;

			Widget button = card.FindAnyWidget("NotifyButton");
			if (!button)
				continue;

			button.SetVisible(true);

			ELIFE_PhoneBannerClick click = new ELIFE_PhoneBannerClick();
			click.Bind(this, threadDto.threadId);
			button.AddHandler(click);
			m_aHubClicks.Insert(click);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void OnStatusBarClicked()
	{
		//! No hub on a locked or dark screen - it would show message bodies.
		if (!m_Phone || !CanShowBanner())
			return;

		m_Phone.SetHubOpen(!m_Phone.IsHubOpen());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHubOpenChanged(bool open)
	{
		if (m_wNotificationHub)
			m_wNotificationHub.SetVisible(open);

		HideAllButBanner();

		if (!open)
			return;

		//! The hub lists what the banners show.
		ClearBanners();
		RefreshNotifications();
	}

	//------------------------------------------------------------------------------------------------
	//! Dismisses without marking read; the next message on a thread notifies again.
	protected void OnHubClearClicked()
	{
		if (!m_Phone)
			return;

		array<ref ELIFE_ThreadDisplayDto> pending = {};
		CollectNotifications(pending);

		foreach (ELIFE_ThreadDisplayDto threadDto : pending)
			m_Phone.DismissNotification(threadDto.threadId, threadDto.unreadCount);

		RefreshNotifications();
	}

	//------------------------------------------------------------------------------------------------
	//! Paints a card for the lock list, banners and hub. compact scales tokens for the 1x card.
	protected void PaintNotificationCard(notnull Widget card, notnull ELIFE_ThreadDto threadDto, bool showCount = false, bool compact = false)
	{
		float unit = 1;
		if (compact)
			unit = COMPACT_SCALE;

		ELIFE_PhoneStyle.ApplyGlass(card, false, true);

		if (compact)
		{
			//! ApplyGlass sizes the specular line from the 2x token.
			SizeLayoutWidget specularSize = SizeLayoutWidget.Cast(card.FindAnyWidget("GlassSpecularSize"));
			if (specularSize)
				specularSize.SetHeightOverride(Scaled(ELIFE_PhoneStyle.GLASS_SPECULAR_HEIGHT, unit));
		}

		string title = ELIFE_PhoneContactBook.TitleFor(m_Phone, threadDto);

		ELIFE_PhoneAppBase.PaintAvatarInto(card, "NotifyAvatarDisc", "NotifyAvatarFallback", "NotifyAvatarGlyph",
			title, ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES));

		//! Both lines stay TextPrimary - TextSecondary is too faint on light glass.
		SetText(card, "NotifyTitle", title);
		SetText(card, "NotifyBody", NewestBody(threadDto));
		SetText(card, "NotifyTime", ELIFE_PhoneAppBase.FormatClock(threadDto.lastMessageAt));

		ELIFE_PhoneStyle.SetTypeOf(card, "NotifyTitle", Scaled(ELIFE_PhoneStyle.TEXT_BODY, unit), true, ELIFE_PhoneStyle.TextPrimary());
		ELIFE_PhoneStyle.SetTypeOf(card, "NotifyBody", Scaled(ELIFE_PhoneStyle.TEXT_CAPTION, unit), false, ELIFE_PhoneStyle.TextPrimary());
		Color timeColor = ELIFE_PhoneStyle.AccentFor(EPhoneScreenState.MESSAGES);
		if (compact)
			timeColor = ELIFE_PhoneStyle.Mix(timeColor, ELIFE_PhoneStyle.TextPrimary(), 0.45);

		ELIFE_PhoneStyle.SetTypeOf(card, "NotifyTime", Scaled(ELIFE_PhoneStyle.TEXT_CAPTION, unit), false, timeColor);

		Widget countSize = card.FindAnyWidget("NotifyCountSize");
		if (countSize)
			countSize.SetVisible(showCount && threadDto.unreadCount > 0);

		if (!showCount)
			return;

		ELIFE_PhoneStyle.SetColorOf(card, "NotifyCountFill", ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES));
		SetText(card, "NotifyCountValue", threadDto.unreadCount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	protected static int Scaled(float size, float unit)
	{
		return Math.Round(size * unit);
	}

	//------------------------------------------------------------------------------------------------
	void OpenBanner(string threadId)
	{
		ClearBanners();

		//! Acting on a banner or hub row closes the hub.
		if (m_Phone)
			m_Phone.SetHubOpen(false);

		//! Messages takes a bare threadId as sub-state.
		RequestAppPage(EPhoneScreenState.MESSAGES, threadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Fade out, then remove.
	protected void DismissBanner(Widget card)
	{
		if (!card)
			return;

		//! Exit uses the shorter state duration.
		ELIFE_PhoneTween exit = new ELIFE_PhoneTween();
		exit.Play(card, 1, 0, ELIFE_PhoneStyle.DURATION_STATE_MS);
		m_aBannerTweens.Insert(exit);
		m_aExitingBanners.Insert(card);

		GetGame().GetCallqueue().CallLater(RemoveBanner, ELIFE_PhoneStyle.DURATION_STATE_MS, false, card);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveBanner(Widget card)
	{
		if (card)
		{
			int exiting = m_aExitingBanners.Find(card);
			if (exiting != -1)
				m_aExitingBanners.Remove(exiting);

			card.RemoveFromHierarchy();
		}

		if (!m_wNotificationBanner || m_wNotificationBanner.GetChildren())
			return;

		m_wNotificationBanner.SetVisible(false);

		//! Stack is empty, so every tween here is finished.
		m_aBannerTweens.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearBanners()
	{
		GetGame().GetCallqueue().Remove(DismissBanner);
		GetGame().GetCallqueue().Remove(RemoveBanner);

		m_aBannerClicks.Clear();
		m_aExitingBanners.Clear();

		//! Dropping them stops their ticks.
		m_aBannerTweens.Clear();

		if (!m_wNotificationBanner)
			return;

		ClearChildren(m_wNotificationBanner);
		m_wNotificationBanner.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	protected int CountChildren(notnull Widget parent)
	{
		int count = 0;

		Widget child = parent.GetChildren();
		while (child)
		{
			count++;
			child = child.GetSibling();
		}

		return count;
	}

	//------------------------------------------------------------------------------------------------
	//! Offline blocks the whole phone, like OFF, but is never shown over ScreenOff.
	protected void RefreshOfflineScreen()
	{
		if (!m_wOfflineScreen || !m_Phone)
			return;

		bool off = m_eState == EPhoneScreenState.OFF;
		m_wOfflineScreen.SetVisible(!off && m_Phone.IsOffline());
		HideAllButBanner();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOfflineRetryClicked()
	{
		if (m_Phone)
			m_Phone.RetryConnectivity();
	}

	//------------------------------------------------------------------------------------------------
	//! Single entry point for what the screen shows.
	void ShowState(EPhoneScreenState state, bool animateEntrance = true)
	{
		RefreshStatusBar();

		bool appOpen = HasAppPage(state);
		bool locked = state == EPhoneScreenState.LOCKED;
		bool off = state == EPhoneScreenState.OFF;

		if (m_wScreenOff)
			m_wScreenOff.SetVisible(off);

		if (m_wLockScreen)
			m_wLockScreen.SetVisible(locked);

		if (m_wHomeScreen)
			m_wHomeScreen.SetVisible(!locked && !appOpen && !off);

		if (m_wAppHost)
			m_wAppHost.SetVisible(appOpen);

		//! Wallpaper is for lock and home; app pages get the flat ground.
		if (m_wWallpaper)
			m_wWallpaper.SetVisible(!appOpen && !off);

		if (m_wAppWashSize)
			m_wAppWashSize.SetVisible(false);

		if (m_wNavBarSize)
			m_wNavBarSize.SetVisible(appOpen);

		//! Locked: no home pill, the PIN pad is the only way in.
		if (m_wHomeIndicator)
			m_wHomeIndicator.SetVisible(!locked && !off);

		if (locked)
		{
			ResetPin();
			RefreshLockScreen();
		}
		else if (!appOpen && !off)
			RefreshHomeScreen();

		if (appOpen)
			OpenApp(state, animateEntrance);
		else
			CloseApp();

		m_eState = state;
		RefreshOfflineScreen();

		//! Banners belong to an awake screen.
		if (!CanShowBanner())
			ClearBanners();

		//! The phone closes the hub on sleep/lock; just re-read the replicated flag.
		if (m_wNotificationHub && m_Phone)
			m_wNotificationHub.SetVisible(m_Phone.IsHubOpen());

		HideAllButBanner();
		RefreshNotifications();
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenApp(EPhoneScreenState state, bool animateEntrance)
	{
		if (m_eState == state && m_App)
			return;

		CloseApp();

		if (!m_wAppHost || !m_Phone)
			return;

		m_App = CreateApp(state);
		if (!m_App)
			return;

		Color accent = ELIFE_PhoneStyle.AccentFor(state);

		if (m_wNavTitle)
		{
			m_wNavTitle.SetText(m_App.GetTitle());
			m_wNavTitle.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}

		if (m_wBackChevron)
			m_wBackChevron.SetColor(accent);

		if (m_wBackLabel)
			m_wBackLabel.SetColor(accent);

		SizeBackChip();

		m_Chrome.m_Accent = accent;
		m_App.BindChrome(m_Chrome);
		m_App.BindShell(this);

		//! Sub-state goes into Open() so the page doesn't flash its default first.
		m_App.Open(m_Phone, m_wAppHost, m_Phone.GetScreenSubState());
		RefreshBackVisibility();

		if (animateEntrance)
			m_AppEntrance.Play(m_wAppHost, 0, 1, ELIFE_PhoneStyle.DURATION_PRESENT_MS);
		else
			m_wAppHost.SetOpacity(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Hugs the Back label's real width so translations neither clip nor leave a gap.
	protected void SizeBackChip()
	{
		if (!m_wBackChipSize || !m_wBackChevron || !m_wBackLabel)
			return;

		float chevronWidth, chevronHeight;
		m_wBackChevron.GetTextSize(chevronWidth, chevronHeight);

		float labelWidth, labelHeight;
		m_wBackLabel.GetTextSize(labelWidth, labelHeight);

		SizeLayoutWidget chipSize = SizeLayoutWidget.Cast(m_wBackChipSize);
		if (chipSize)
			chipSize.SetWidthOverride(chevronWidth + labelWidth + BACK_CHIP_CHROME);
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseApp()
	{
		if (m_App)
		{
			m_App.Close();
			m_App = null;
		}

		if (m_AppEntrance)
			m_AppEntrance.Stop();

		if (m_wNavBarGlass)
			m_wNavBarGlass.SetOpacity(0);

		if (m_wStatusBarGlass)
			m_wStatusBarGlass.SetOpacity(0);

		if (m_wNavTitle)
			m_wNavTitle.SetOpacity(0);
	}

	//------------------------------------------------------------------------------------------------
	void ApplySubState(string subState)
	{
		if (m_App)
			m_App.ApplySubState(subState);
	}

	//------------------------------------------------------------------------------------------------
	//! Back only pops within the app; the landing page shows none. Leaving is the home pill's job.
	void RefreshBackVisibility()
	{
		if (m_wButtonBack)
			m_wButtonBack.SetVisible(m_App && !m_App.IsAtRoot());
	}

	//------------------------------------------------------------------------------------------------
	//! True when the open app handled Back itself.
	bool AppConsumedBack()
	{
		return m_App && m_App.OnBack();
	}

	//------------------------------------------------------------------------------------------------
	// Home screen
	//------------------------------------------------------------------------------------------------

	//! Balance card and newest unread thread above the grid.
	protected void RefreshHomeScreen()
	{
		if (m_wHomeDate)
			m_wHomeDate.SetText(FormatToday());

		if (!m_wHomeCards)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_aCardClicks.Clear();
		ClearChildren(m_wHomeCards);

		BuildBalanceCard(workspace);
		BuildMessagesCard(workspace);
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildBalanceCard(notnull WorkspaceWidget workspace)
	{
		array<ref ELIFE_PhoneBankAccount> accounts = {};
		ELIFE_PhoneBankingService.GetAccounts(accounts);
		if (accounts.IsEmpty())
			return;

		ELIFE_PhoneBankAccount account = accounts.Get(0);

		Widget card = CreateHomeCard(workspace, EPhoneScreenState.BANK);
		if (!card)
			return;

		SetText(card, "CardLabel", account.m_sName);
		ELIFE_PhoneStyle.SetColorOf(card, "CardLabel", ELIFE_PhoneStyle.TextPrimary());

		SetText(card, "CardValue", ELIFE_PhoneBankingService.FormatMoney(account.m_iBalanceCents));
		ELIFE_PhoneStyle.SetTypeOf(card, "CardValue", ELIFE_PhoneStyle.TEXT_TITLE, true, ELIFE_PhoneStyle.AccentBank());
	}

	//------------------------------------------------------------------------------------------------
	protected void BuildMessagesCard(notnull WorkspaceWidget workspace)
	{
		Widget card = CreateHomeCard(workspace, EPhoneScreenState.MESSAGES);
		if (!card)
			return;

		ELIFE_ThreadDto newest = NewestUnreadThread();
		if (!newest)
		{
			SetText(card, "CardLabel", "#ELIFE-Phone_App_Messages");
			ELIFE_PhoneStyle.SetColorOf(card, "CardLabel", ELIFE_PhoneStyle.TextPrimary());
			SetText(card, "CardValue", "#ELIFE-Phone_Home_NoUnread");
			ELIFE_PhoneStyle.SetTypeOf(card, "CardValue", ELIFE_PhoneStyle.TEXT_BODY, false, ELIFE_PhoneStyle.TextPrimary());
			return;
		}

		SetText(card, "CardLabel", ELIFE_PhoneContactBook.TitleFor(m_Phone, newest));
		ELIFE_PhoneStyle.SetColorOf(card, "CardLabel", ELIFE_PhoneStyle.TextPrimary());

		SetText(card, "CardValue", NewestBody(newest));
		ELIFE_PhoneStyle.SetTypeOf(card, "CardValue", ELIFE_PhoneStyle.TEXT_BODY, false, ELIFE_PhoneStyle.TextPrimary());

		SetText(card, "CardTrailing", ELIFE_PhoneAppBase.FormatClock(newest.lastMessageAt));
		ELIFE_PhoneStyle.SetTypeOf(card, "CardTrailing", ELIFE_PhoneStyle.TEXT_CAPTION, false, ELIFE_PhoneStyle.AccentMessages());
	}

	//------------------------------------------------------------------------------------------------
	protected Widget CreateHomeCard(notnull WorkspaceWidget workspace, EPhoneScreenState state)
	{
		Widget card = workspace.CreateWidgets(LAYOUT_HOME_CARD, m_wHomeCards);
		if (!card)
			return null;

		AlignableSlot.SetHorizontalAlign(card, LayoutHorizontalAlign.Stretch);

		if (m_wHomeCards.GetChildren() != card)
			LayoutSlot.SetPadding(card, 0, ELIFE_PhoneStyle.SPACE_2, 0, 0);

		ELIFE_PhoneStyle.ApplyGlass(card, false, true);
		ELIFE_PhoneStyle.ApplyGlass(card.FindAnyWidget("CardBadge"), false, false, false, true, ELIFE_PhoneStyle.AccentDeepFor(state));

		PaintAppIcon(workspace, card.FindAnyWidget("CardIconSize"), TextWidget.Cast(card.FindAnyWidget("CardMark")), FindApp(state), ICON_INK_CARD);

		if (!m_bInteractive)
			return card;

		Widget button = card.FindAnyWidget("CardButton");
		if (!button)
			return card;

		ELIFE_PhoneTileClick click = new ELIFE_PhoneTileClick();
		click.Bind(this, state);
		button.AddHandler(click);
		m_aCardClicks.Insert(click);

		return card;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_ThreadDto NewestUnreadThread()
	{
		if (!m_Phone)
			return null;

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json == "")
			return null;

		ELIFE_MessageUpdatesDto updates = new ELIFE_MessageUpdatesDto();
		updates.ExpandFromRAW(json);

		ELIFE_ThreadDto newest;
		foreach (ELIFE_ThreadDto threadDto : updates.threads)
		{
			if (threadDto.unreadCount <= 0)
				continue;

			if (!newest || threadDto.lastMessageAt > newest.lastMessageAt)
				newest = threadDto;
		}

		return newest;
	}

	//------------------------------------------------------------------------------------------------
	// Lock screen
	//------------------------------------------------------------------------------------------------
	protected void RefreshLockScreen()
	{
		if (m_wLockNumber && m_Phone)
			m_wLockNumber.SetText(m_Phone.GetNumber());

		if (m_wLockDate)
			m_wLockDate.SetText(FormatToday());

		FillLockNotifications();
	}

	//------------------------------------------------------------------------------------------------
	//! Weekday and short date from the world clock.
	protected string FormatToday()
	{
		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		if (!world)
			return "";

		TimeAndWeatherManagerEntity timeManager = world.GetTimeAndWeatherManager();
		if (!timeManager)
			return "";

		int year, month, day;
		timeManager.GetDate(year, month, day);

		string weekday = timeManager.GetWeekDayString();
		string date = SCR_DateTimeHelper.GetDateString(day, month, year, false);

		if (weekday == "")
			return date;

		return weekday + " · " + date;
	}

	//------------------------------------------------------------------------------------------------
	protected void FillLockNotifications()
	{
		if (!m_wLockNotifications)
			return;

		ClearChildren(m_wLockNotifications);

		if (!m_Phone)
			return;

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json == "")
			return;

		ELIFE_MessageUpdatesDto updates = new ELIFE_MessageUpdatesDto();
		updates.ExpandFromRAW(json);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		int shown = 0;

		foreach (ELIFE_ThreadDto threadDto : updates.threads)
		{
			if (shown >= LOCK_NOTIFICATION_LIMIT)
				break;

			if (threadDto.unreadCount <= 0)
				continue;

			Widget card = workspace.CreateWidgets(LAYOUT_NOTIFICATION, m_wLockNotifications);
			if (!card)
				continue;

			AlignableSlot.SetHorizontalAlign(card, LayoutHorizontalAlign.Stretch);
			if (shown > 0)
				LayoutSlot.SetPadding(card, 0, ELIFE_PhoneStyle.SPACE_1, 0, 0);

			PaintNotificationCard(card, threadDto);

			shown++;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string NewestBody(notnull ELIFE_ThreadDto threadDto)
	{
		int count = threadDto.messages.Count();
		if (count == 0)
			return WidgetManager.Translate("#ELIFE-Phone_Lock_NewMessages");

		return threadDto.messages.Get(count - 1).body;
	}

	//------------------------------------------------------------------------------------------------
	void PinPush(int digit)
	{
		if (m_bUnlockPending || IsPinBlocked() || m_sPinEntry.Length() >= PIN_LENGTH)
			return;

		if (m_bPinError)
			ClearPinError();

		m_sPinEntry += digit.ToString();
		RedrawPinDots();

		if (m_sPinEntry.Length() == PIN_LENGTH)
			TryUnlock();
	}

	//------------------------------------------------------------------------------------------------
	bool AcceptsPinKeys()
	{
		return m_bInteractive && m_eState == EPhoneScreenState.LOCKED;
	}

	//------------------------------------------------------------------------------------------------
	//! Keyboard entry, same path as the on-screen keys.
	void PinKeyDigit(int digit)
	{
		if (AcceptsPinKeys())
			PinPush(digit);
	}

	//------------------------------------------------------------------------------------------------
	void PinKeyBackspace()
	{
		if (AcceptsPinKeys())
			PinBackspace();
	}

	//------------------------------------------------------------------------------------------------
	void PinBackspace()
	{
		if (m_bUnlockPending || IsPinBlocked())
			return;

		if (m_bPinError)
		{
			ClearPinError();
			return;
		}

		int length = m_sPinEntry.Length();
		if (length == 0)
			return;

		m_sPinEntry = m_sPinEntry.Substring(0, length - 1);
		RedrawPinDots();
	}

	//------------------------------------------------------------------------------------------------
	//! The server judges the PIN: a rejection comes back through OnUnlockRejected, success as a state change.
	protected void TryUnlock()
	{
		if (!m_Phone)
			return;

		m_bUnlockPending = true;
		m_Phone.Unlock(m_sPinEntry);

		GetGame().GetCallqueue().Remove(OnUnlockTimeout);
		GetGame().GetCallqueue().CallLater(OnUnlockTimeout, UNLOCK_REPLY_TIMEOUT_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnUnlockTimeout()
	{
		ResetPin();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnUnlockRejected(int result, int retryInMs)
	{
		GetGame().GetCallqueue().Remove(OnUnlockTimeout);
		m_bUnlockPending = false;

		if (result != ELIFE_EUnlockResult.BLOCKED)
		{
			ShowPinError();
			return;
		}

		m_fPinBlockedUntil = GetGame().GetWorld().GetWorldTime() + retryInMs;
		ResetPin();

		GetGame().GetCallqueue().Remove(TickPinBlock);
		GetGame().GetCallqueue().CallLater(TickPinBlock, 1000, true);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsPinBlocked()
	{
		return m_fPinBlockedUntil > GetGame().GetWorld().GetWorldTime();
	}

	//------------------------------------------------------------------------------------------------
	//! Counts the cooldown down on the prompt, then hands back to "Enter PIN".
	protected void TickPinBlock()
	{
		if (!IsPinBlocked())
		{
			GetGame().GetCallqueue().Remove(TickPinBlock);
			m_fPinBlockedUntil = 0;
		}

		ShowPinPrompt();
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPinPrompt()
	{
		SetPinPadEnabled(!IsPinBlocked());

		if (!m_wPinPrompt)
			return;

		if (!IsPinBlocked())
		{
			m_wPinPrompt.SetText("#ELIFE-Phone_Lock_Enter");
			m_wPinPrompt.SetColor(ELIFE_PhoneStyle.TextSecondary());
			return;
		}

		float remaining = m_fPinBlockedUntil - GetGame().GetWorld().GetWorldTime();
		m_wPinPrompt.SetTextFormat("#ELIFE-Phone_Lock_Blocked", Math.Ceil(remaining / 1000));
		m_wPinPrompt.SetColor(ELIFE_PhoneStyle.Negative());
	}

	//------------------------------------------------------------------------------------------------
	protected void SetPinPadEnabled(bool enabled)
	{
		float opacity = 1;
		if (!enabled)
			opacity = PIN_KEY_DISABLED_OPACITY;

		foreach (Widget key : m_aPinKeys)
		{
			key.SetOpacity(opacity);

			Widget button = key.FindAnyWidget("KeyButton");
			if (button)
				button.SetEnabled(enabled);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPinError()
	{
		m_bPinError = true;
		RedrawPinDots();

		if (m_wPinPrompt)
		{
			m_wPinPrompt.SetText("#ELIFE-Phone_Lock_Wrong");
			m_wPinPrompt.SetColor(ELIFE_PhoneStyle.Negative());
		}

		GetGame().GetCallqueue().Remove(ClearPinError);
		GetGame().GetCallqueue().CallLater(ClearPinError, PIN_ERROR_HOLD_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearPinError()
	{
		GetGame().GetCallqueue().Remove(ClearPinError);

		m_bPinError = false;
		m_sPinEntry = "";
		RedrawPinDots();
		ShowPinPrompt();
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetPin()
	{
		GetGame().GetCallqueue().Remove(ClearPinError);
		GetGame().GetCallqueue().Remove(OnUnlockTimeout);
		m_bUnlockPending = false;
		m_bPinError = false;
		m_sPinEntry = "";
		RedrawPinDots();
		ShowPinPrompt();
	}

	//------------------------------------------------------------------------------------------------
	protected void RedrawPinDots()
	{
		int entered = m_sPinEntry.Length();

		for (int i = 0; i < m_aPinDots.Count(); i++)
		{
			Color color = ELIFE_PhoneStyle.Hairline();

			if (m_bPinError)
				color = ELIFE_PhoneStyle.Negative();
			else if (i < entered)
				color = ELIFE_PhoneStyle.TextPrimary();

			m_aPinDots.Get(i).SetColor(color);
		}
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
	void ~ELIFE_PhoneScreenShell()
	{
		GetGame().GetCallqueue().Remove(ClearPinError);
		GetGame().GetCallqueue().Remove(OnUnlockTimeout);
		GetGame().GetCallqueue().Remove(TickPinBlock);
		GetGame().GetCallqueue().Remove(FlushPendingApp);
	}
}
