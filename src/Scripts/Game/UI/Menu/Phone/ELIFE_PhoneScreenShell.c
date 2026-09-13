//------------------------------------------------------------------------------------------------
//! One app on the home screen. The whole registry lives in ELIFE_PhoneScreenShell.BuildApps() -
//! adding a sixth app is one entry there and nothing else in the OS changes.
class ELIFE_PhoneAppEntry
{
	EPhoneScreenState m_eState;
	string m_sLabel;

	ResourceName m_sIconSet;
	string m_sIconImage;

	//! Character mark drawn if the sprite cannot be loaded. Empty for icons already proven in-game.
	string m_sGlyph;

	//! Optical trim on top of the shared icon box, applied to a sprite or a character mark alike.
	//! Marks that carry more or less ink than the rest need to sit at a different size to weigh the
	//! same - this is optical sizing, not a layout fix.
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
//! A banner is a door: it hands the phone to the app the notification came from, on that thread.
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
//! Drives PhoneScreen.layout: status bar, lock screen/PIN pad, home grid, nav bar and app host. Both the in-hand menu (interactive) and the world render target (passive) drive one of these, so the two copies never drift apart.
class ELIFE_PhoneScreenShell
{
	protected const ResourceName LAYOUT_TILE = "{7C12D4A9E3B25F83}UI/layouts/Menus/Phone/Widgets/PhoneAppTile.layout";
	protected const ResourceName LAYOUT_GRID_ROW = "{7C25D4A9E3B25F8A}UI/layouts/Menus/Phone/Widgets/PhoneGridRow.layout";
	protected const ResourceName LAYOUT_PIN_KEY = "{7C24D4A9E3B25F89}UI/layouts/Menus/Phone/Widgets/PhonePinKey.layout";
	protected const ResourceName LAYOUT_NOTIFICATION = "{7C13D4A9E3B25F84}UI/layouts/Menus/Phone/Widgets/PhoneNotificationCard.layout";
	protected const ResourceName LAYOUT_HOME_CARD = "{7C26D4A9E3B25F8B}UI/layouts/Menus/Phone/Widgets/PhoneHomeCard.layout";

	protected const ResourceName LAYOUT_APP_ICON = "{7C27D4A9E3B25F8C}UI/layouts/Menus/Phone/Widgets/PhoneAppIcon.layout";

	//! How much visible ink an app icon should show, at tile size and at home-card badge size. A
	//! fitted sprite and a character mark are both brought to this, but by different routes.
	protected const float ICON_INK_TILE = 26;
	protected const float ICON_INK_CARD = 18;

	//! Reforger's icon sprites carry transparent padding inside their own atlas cells, so a sprite
	//! fitted to N px shows appreciably less than N px of ink - which is why the borrowed icons read
	//! smaller than the Bank mark beside them. This lifts a sprite's box until the ink matches.
	protected const float ICON_SPRITE_PADDING = 1.34;

	//! A capital letterform's cap height is roughly 0.72 of its font size, so a mark has to be set
	//! larger than the ink it should show. Systematic here rather than hidden in a per-app number.
	protected const float ICON_MARK_CAP = 1.38;

	protected const int GRID_COLUMNS = 4;
	protected const int PIN_LENGTH = 4;
	protected const int PIN_ERROR_HOLD_MS = 900;

	//! × is a maths operator: drawn near x-height and centred on the maths axis, so at the digits'
	//! own size it reads visibly smaller and lower than they do. It needs to run larger to match.
	protected const int PIN_DELETE_SIZE = 27;
	protected const int LOCK_NOTIFICATION_LIMIT = 3;

	//! Banners for messages that arrive while the phone is awake. The lock screen has its own list,
	//! so these only ever appear over home or an open app.
	protected const int BANNER_LIMIT = 3;
	protected const int BANNER_DURATION_MS = 5000;

	protected Widget m_wNotificationBanner;
	protected ref array<ref ELIFE_PhoneBannerClick> m_aBannerClicks = {};

	//! One tween per card, not one shared: a second arrival must not restart the fade of the card
	//! already resting beside it.
	protected ref array<ref ELIFE_PhoneTween> m_aBannerTweens = {};

	//! Cards already fading out, so SettleBannerTweens() can leave them alone - hauling one back to
	//! full opacity on its way out is the same blink the fade exists to remove.
	protected ref array<Widget> m_aExitingBanners = {};

	//! The hub: every notification still standing, over whatever page is open. Its open/closed state
	//! lives on the phone (replicated), not here, so the world screen shows it too.
	protected Widget m_wNotificationHub;
	protected Widget m_wHubList;
	protected Widget m_wHubEmpty;
	protected Widget m_wHubClearSize;
	protected Widget m_wStatusNotifySize;
	protected TextWidget m_wStatusNotifyCount;
	protected ref array<ref ELIFE_PhoneBannerClick> m_aHubClicks = {};

	//! threadId -> the unread count this screen has already accounted for. A banner is raised when a
	//! thread's count goes *up* against this, which is what makes it "arrived while you were looking"
	//! rather than "is unread".
	protected ref map<string, int> m_mSeenUnread = new map<string, int>();

	//! The first payload only seeds the baseline - without this, opening the phone would replay every
	//! unread thread it already had as a fresh arrival.
	protected bool m_bUnreadSeeded;

	//! Horizontal chrome around the chevron+label inside BackChip: BackBody's own 8px left/right
	//! padding, plus the chevron's 3px trailing gap to the label (see PhoneScreen.layout).
	protected const float BACK_CHIP_CHROME = 19;

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
	protected ref array<Widget> m_aPinDots = {};
	protected ref ELIFE_PhoneTween m_AppEntrance = new ELIFE_PhoneTween();
	protected string m_sPinEntry;
	protected bool m_bPinError;

	//! Fires with the requested EPhoneScreenState and the sub-state to land on when a home tile or an
	//! in-app hand-off is used. The owner decides what that means - the Map tile hands off to the
	//! fullscreen map menu rather than to an AppHost page.
	protected ref ScriptInvoker m_OnAppRequested = new ScriptInvoker();

	//! A cross-app jump asked for by the app that is currently open (Contacts' "Message" opening the
	//! thread with that number). Held rather than dispatched, because honouring it destroys the app
	//! whose click handler is still on the stack - see RequestAppPage().
	protected EPhoneScreenState m_ePendingApp;
	protected string m_sPendingSubState;
	protected ref ScriptInvoker m_OnHomePill = new ScriptInvoker();
	protected ref ScriptInvoker m_OnBack = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! Home-grid order, rows of four. Bank still uses the currency glyph until it has a wrapper sprite.
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
	//! Creates the page for a state. Null for the states that have no AppHost page of their own.
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
	//! Whether this state has a page of its own inside AppHost. Map does not - it hands off to the
	//! fullscreen map menu - so the screen behind it stays on the home grid.
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

		//! Built once - every widget in it lives in the screen, not in any one app.
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

		//! Provisioning finishes after the phone is first drawn, so the number is normally still
		//! empty right here. Re-read it when it actually lands rather than leaving a blank status bar
		//! for the rest of the session.
		phone.m_OnIdentityChanged.Insert(OnIdentityChanged);

		//! Applies whatever offline state already exists the moment this canvas opens, not just on the next change.
		phone.m_OnConnectivityChanged.Insert(OnConnectivityChanged);
		RefreshOfflineScreen();

		//! Home and lock read the messages cache directly, and the server's poll refreshes it while
		//! they're on screen - so they have to re-render off it rather than sampling once on open.
		phone.m_OnDataChanged.Insert(OnDataChanged);

		//! Replicated, so the world screen opens the hub at the same moment the menu does.
		phone.m_OnHubOpenChanged.Insert(OnHubOpenChanged);
		ApplyHubGlass();
		RefreshNotifications();

		//! Seed from the cache now so the wake poll can still count as an arrival.
		RaiseArrivedBanners();

		//! Both screens name a thread through ELIFE_PhoneContactBook, so the contacts they resolve
		//! against have to be asked for here too - neither one opens the app that would.
		phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);

		if (!m_bInteractive)
			return;

		SCR_ButtonTextComponent homePill = SCR_ButtonTextComponent.GetButtonText("ButtonClose", screenRoot);
		if (homePill)
			homePill.m_OnClicked.Insert(OnHomePillClicked);

		SCR_ButtonTextComponent back = SCR_ButtonTextComponent.GetButtonText("ButtonBack", screenRoot);
		if (back)
			back.m_OnClicked.Insert(OnBackClicked);

		SCR_ButtonTextComponent offlineRetry = SCR_ButtonTextComponent.GetButtonText("OfflineRetry", screenRoot);
		if (offlineRetry)
			offlineRetry.m_OnClicked.Insert(OnOfflineRetryClicked);

		//! The whole status bar is the hub's handle - there is no room at this canvas for a control
		//! beside the indicator, and the bar carries nothing else that wants a tap.
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
		}

		m_aHubClicks.Clear();

		//! Drops the pending per-card dismiss timers and entrance tweens with the screen that scheduled them.
		ClearBanners();

		//! The name lookup is process-wide, so it is dropped with the screen that populated it rather
		//! than left holding one phone's contacts for whatever opens next.
		ELIFE_PhoneContactBook.Clear();

		CloseApp();

		if (m_AppEntrance)
			m_AppEntrance.Stop();

		GetGame().GetCallqueue().Remove(ClearPinError);
		GetGame().GetCallqueue().Remove(FlushPendingApp);

		m_aTileClicks.Clear();
		m_aCardClicks.Clear();
		m_aPinClicks.Clear();
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
	//! Layer order, once, from the tokens - no arbitrary ZOrder numbers sprinkled through layouts.
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

		//! The phone's own frame, not the page's - it stays on top of a sheet so the hub can't hide the
		//! clock or the only way out of itself.
		Widget statusBar = m_wRoot.FindAnyWidget("StatusBarSize");
		if (statusBar)
			statusBar.SetZOrder(ELIFE_PhoneStyle.ZORDER_SYSTEM);

		if (m_wHomeIndicator)
			m_wHomeIndicator.SetZOrder(ELIFE_PhoneStyle.ZORDER_SYSTEM);

		if (m_wScreenOff)
			m_wScreenOff.SetZOrder(ELIFE_PhoneStyle.ZORDER_ALERT);

		//! Same tier as ScreenOff, never visible at the same time as it (RefreshOfflineScreen() only
		//! shows this while the screen is actually on) - so which one wins never comes up in practice.
		if (m_wOfflineScreen)
			m_wOfflineScreen.SetZOrder(ELIFE_PhoneStyle.ZORDER_ALERT);

		if (m_wNotificationBanner)
			m_wNotificationBanner.SetZOrder(ELIFE_PhoneStyle.ZORDER_BANNER);

		//! The hub covers the page and the page's nav bar, but not the phone's own frame - so it claims
		//! the sheet tier, under the OS chrome, the banner that can still arrive over it, and alerts.
		if (m_wNotificationHub)
			m_wNotificationHub.SetZOrder(ELIFE_PhoneStyle.ZORDER_SHEET);

		//! Status/nav are dark glass, but ApplyGlass's Opacity=1 is overridden back to 0 here since ApplyCollapse owns their reveal - on home/lock nothing scrolls under them anyway.
		ELIFE_PhoneStyle.ApplyGlass(m_wStatusBarGlass, true, false, true);
		ELIFE_PhoneStyle.ApplyGlass(m_wNavBarGlass, true, false, true);
		if (m_wStatusBarGlass)
			m_wStatusBarGlass.SetOpacity(0);
		if (m_wNavBarGlass)
			m_wNavBarGlass.SetOpacity(0);

		//! Back is a word, not a pill. Hide any glass layers authored on the chip so they never sit
		//! under the label.
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

				//! An unused cell is an empty row widget, so a part-filled last row still keeps the
				//! same column rhythm as a full one.
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
	//! Puts an app's icon into a tile or a card badge: the imageset sprite when the app has one, otherwise its character mark. The two need different boxes because a text widget centres its line box (ascender to descender), not the visible letterform - squeezing a mark into a sprite-sized box would clip it, so the mark centres in the full tile/badge instead and is sized by cap height to match. Stays TextPrimary() regardless of app - the accent identity comes from the glass glow behind it, not the glyph itself.
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

		//! Sized off the same ink target as a sprite, so the two carry equal weight side by side.
		mark.SetExactFontSize(Math.Round(ink * ICON_MARK_CAP * app.m_fIconScale));
	}

	//------------------------------------------------------------------------------------------------
	//! 1-9, then a blank, 0 and delete. Built in both canvases so a locked phone reads as locked from across a room even though the passive copy isn't clickable.
	protected void BuildPinPad()
	{
		if (!m_wPinRows)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		m_aPinClicks.Clear();

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

				//! Last row: an unlabelled placeholder keeps the 0 optically centred, then 0 and delete.
				if (column == 0)
					CreatePinKey(workspace, row, "", -2, trailingGap);
				else if (column == 1)
					CreatePinKey(workspace, row, "0", 0, trailingGap);
				else
					CreatePinKey(workspace, row, "×", -1, trailingGap, PIN_DELETE_SIZE);   //! Not ‹ - that is nav Back
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! digit: 0-9 a real key, -1 delete, -2 the inert placeholder that holds the last row's grid.
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
	//! Hand the screen to another app, landing directly on a page inside it. Deferred by one frame because the caller is a click handler owned by the app about to be closed - closing it now would drop the handler still executing.
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
		//! The hub is a layer over the page, so home dismisses that layer first - exactly how it leaves
		//! an app before it leaves the phone. Whatever was open underneath stays open.
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
	//! Everything on screen that shows a piece of the provisioned identity, re-read together.
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
	//! Only the two screens that read a data key straight out of the phone. An open app isn't touched
	//! here - it subscribes to the same invoker itself and owns its own page.
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

		//! After the banners, so an arrival that raises one also lands in the list behind it.
		RefreshNotifications();
	}

	//------------------------------------------------------------------------------------------------
	// Notification banners
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

		//! Every thread, not just unread ones - a thread dropping back to zero has to be recorded, or
		//! the next message on it wouldn't read as a rise.
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
	//! Awake only - a locked phone already lists these on its own screen, and an off one has nothing to draw on. Not gated on m_bInteractive: a bystander's world RT shows the banner too, just not the click.
	protected bool CanShowBanner()
	{
		return m_eState != EPhoneScreenState.OFF && m_eState != EPhoneScreenState.LOCKED;
	}

	//------------------------------------------------------------------------------------------------
	//! A banner over the open hub would duplicate a row already on screen behind it.
	protected bool CanRaiseBanner()
	{
		return CanShowBanner() && m_Phone && !m_Phone.IsHubOpen();
	}

	//------------------------------------------------------------------------------------------------
	//! True when this conversation is the one open on screen - it must neither banner nor queue in the hub, since it's already being read as it arrives. Shared by both surfaces so they can't disagree.
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

		//! This conversation is already open - the message is landing in the thread on screen, so a
		//! banner would announce something the player is watching arrive.
		if (IsThreadOnScreen(threadDto.threadId))
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		//! Oldest goes rather than refusing the newest - the most recent arrival is the one worth
		//! interrupting for.
		while (CountChildren(m_wNotificationBanner) >= BANNER_LIMIT)
		{
			Widget oldest = m_wNotificationBanner.GetChildren();
			if (!oldest)
				break;

			SettleBannerTweens();
			oldest.RemoveFromHierarchy();
		}

		Widget card = workspace.CreateWidgets(LAYOUT_NOTIFICATION, m_wNotificationBanner);
		if (!card)
			return;

		AlignableSlot.SetHorizontalAlign(card, LayoutHorizontalAlign.Stretch);
		if (m_wNotificationBanner.GetChildren() != card)
			LayoutSlot.SetPadding(card, 0, ELIFE_PhoneStyle.SPACE_1, 0, 0);

		PaintNotificationCard(card, threadDto);

		m_wNotificationBanner.SetVisible(true);

		BindBannerClick(card, threadDto.threadId);

		//! A banner is a present, so it gets the present duration and an ease-out - the list rows it is
		//! made of do not, because "no fade-in on every row" is about lists, not about arrivals.
		ELIFE_PhoneTween entrance = new ELIFE_PhoneTween();
		entrance.Play(card, 0, 1, ELIFE_PhoneStyle.DURATION_PRESENT_MS);
		m_aBannerTweens.Insert(entrance);

		//! Per-card, so two arrivals a second apart don't share one deadline.
		GetGame().GetCallqueue().CallLater(DismissBanner, BANNER_DURATION_MS, false, card);
	}

	//------------------------------------------------------------------------------------------------
	//! Ends every in-flight entrance and snaps its card to rest. Called before a card is torn out from
	//! under a tween that is still ticking - the same discipline CloseApp() uses on the app entrance.
	//! A fade old enough to be evicted has effectively finished anyway, so nothing visible is lost.
	protected void SettleBannerTweens()
	{
		if (m_aBannerTweens.IsEmpty() || !m_wNotificationBanner)
			return;

		m_aBannerTweens.Clear();

		Widget child = m_wNotificationBanner.GetChildren();
		while (child)
		{
			//! A card on its way out keeps whatever the fade had reached; only an interrupted entrance
			//! needs rescuing, since it would otherwise sit half-transparent until its own dismissal.
			if (m_aExitingBanners.Find(child) == -1)
				child.SetOpacity(1);

			child = child.GetSibling();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The card ships its button hidden, since the lock screen's copies of it are not controls.
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

	//! Overlay altitude: the heaviest scrim of the three dark-glass strengths, because unlike a status
	//! bar this is meant to take the screen away from the page rather than sit over it.
	protected void ApplyHubGlass()
	{
		if (!m_wNotificationHub)
			return;

		ELIFE_PhoneStyle.ApplyGlass(m_wNotificationHub, true, false, true);
		ELIFE_PhoneStyle.ApplyGlass(m_wNotificationHub.FindAnyWidget("HubClearChip"), false, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Every thread still carrying an undismissed notification, newest first - the payload is already
	//! in that order, so new arrivals land on top the same way they do in the banner stack.
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

			//! Reading the thread is what retires a notification - this only hides one the reader
			//! actively cleared, and only until the next message arrives on it.
			if (m_Phone.IsNotificationDismissed(threadDto.threadId, threadDto.unreadCount))
				continue;

			if (IsThreadOnScreen(threadDto.threadId))
				continue;

			outThreads.Insert(threadDto);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Newest outstanding notification - the peek's door when the player takes the phone out.
	string GetNewestNotificationThreadId()
	{
		array<ref ELIFE_ThreadDisplayDto> pending = {};
		CollectNotifications(pending);

		if (pending.IsEmpty() || !pending[0])
			return "";

		return pending[0].threadId;
	}

	//------------------------------------------------------------------------------------------------
	//! Indicator and hub list are one refresh - they read the same list and must never disagree about
	//! how much is waiting.
	protected void RefreshNotifications()
	{
		array<ref ELIFE_ThreadDisplayDto> pending = {};
		CollectNotifications(pending);

		//! Messages, not conversations: three from one person is three things waiting, and a "1" over
		//! three unread messages reads as a bug. The hub still collapses them into one row per thread,
		//! which is what its own per-row count is for.
		int unread = 0;
		foreach (ELIFE_ThreadDisplayDto threadDto : pending)
			unread += threadDto.unreadCount;

		RefreshNotificationIndicator(unread);
		FillHub(pending);
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshNotificationIndicator(int count)
	{
		//! Hidden on a locked or dark screen along with the hub itself - a count is still information
		//! about who is contacting you.
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

		//! Nothing to clear is not a disabled button, it is no button.
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

			//! Only the hub carries the count. A banner is a single arrival announcing itself, and the
			//! lock list is a standing list where the number would just repeat the row beside it.
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
		//! Locked or dark, the bar is not a control - the hub would be showing message bodies to
		//! whoever picked the phone up.
		if (!m_Phone || !CanShowBanner())
			return;

		m_Phone.SetHubOpen(!m_Phone.IsHubOpen());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnHubOpenChanged(bool open)
	{
		if (m_wNotificationHub)
			m_wNotificationHub.SetVisible(open);

		if (!open)
			return;

		//! A banner and the hub say the same thing; with the hub open the banner is just noise on top
		//! of the list it duplicates.
		ClearBanners();
		RefreshNotifications();
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the hub without touching whether anything is read - that is the whole point of it being
	//! a separate gesture. Each thread is watermarked at its current count, so the next message on any
	//! of them notifies again.
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
	//! The one place a notification card is painted, for both surfaces that draw one - the lock list
	//! and the awake banner. They are the same card and must not drift apart.
	protected void PaintNotificationCard(notnull Widget card, notnull ELIFE_ThreadDto threadDto, bool showCount = false)
	{
		//! Light glass - it floats at rest over a page or the wallpaper.
		ELIFE_PhoneStyle.ApplyGlass(card, false, true);

		string title = ELIFE_PhoneContactBook.TitleFor(m_Phone, threadDto);

		//! A notification is about a *person*, so it gets a person mark - circle and initials in the
		//! owning app's AccentDeep, the same mark the thread list and contact rows use. The accent dot
		//! this replaced said nothing about who was writing.
		ELIFE_PhoneAppBase.PaintAvatarInto(card, "NotifyAvatarDisc", "NotifyAvatarFallback", "NotifyAvatarGlyph",
			title, ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES));

		//! Both lines are TextPrimary, not a Primary/Secondary pair - TextSecondary sits too close to light glass to stay readable. Hierarchy comes from size and weight; the timestamp takes the accent.
		SetText(card, "NotifyTitle", title);
		SetText(card, "NotifyBody", NewestBody(threadDto));
		SetText(card, "NotifyTime", ELIFE_PhoneAppBase.FormatClock(threadDto.lastMessageAt));

		ELIFE_PhoneStyle.SetTypeOf(card, "NotifyTitle", ELIFE_PhoneStyle.TEXT_BODY, true, ELIFE_PhoneStyle.TextPrimary());
		ELIFE_PhoneStyle.SetTypeOf(card, "NotifyBody", ELIFE_PhoneStyle.TEXT_CAPTION, false, ELIFE_PhoneStyle.TextPrimary());
		ELIFE_PhoneStyle.SetTypeOf(card, "NotifyTime", ELIFE_PhoneStyle.TEXT_CAPTION, false, ELIFE_PhoneStyle.AccentFor(EPhoneScreenState.MESSAGES));

		//! How many messages this one row stands for - the body only ever shows the newest.
		Widget countSize = card.FindAnyWidget("NotifyCountSize");
		if (countSize)
			countSize.SetVisible(showCount && threadDto.unreadCount > 0);

		if (!showCount)
			return;

		ELIFE_PhoneStyle.SetColorOf(card, "NotifyCountFill", ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES));
		SetText(card, "NotifyCountValue", threadDto.unreadCount.ToString());
	}

	//------------------------------------------------------------------------------------------------
	void OpenBanner(string threadId)
	{
		ClearBanners();

		//! Shared by the banner and the hub rows - acting on either hands the phone over, so the hub
		//! has no reason to still be covering the page it lands on.
		if (m_Phone)
			m_Phone.SetHubOpen(false);

		//! Messages addresses a conversation by its bare threadId - see ELIFE_PhoneMessagesApp's
		//! sub-state prefixes, which mark the other two cases rather than this one.
		RequestAppPage(EPhoneScreenState.MESSAGES, threadId);
	}

	//------------------------------------------------------------------------------------------------
	//! Leaves in two steps: fade, then tear out once it's run - an instant removal after an eased entrance would read as a glitch.
	protected void DismissBanner(Widget card)
	{
		if (!card)
			return;

		//! An exit takes the state duration, not the present one: coming in announces something and
		//! earns the longer curve, going away is housekeeping and should not hold the eye.
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

		//! The stack is empty, so every tween in here has finished with a card that no longer exists -
		//! this is what stops the array growing for the life of the screen.
		m_aBannerTweens.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearBanners()
	{
		//! Both halves of the exit - the deadline that starts the fade, and the tear-out that follows it.
		GetGame().GetCallqueue().Remove(DismissBanner);
		GetGame().GetCallqueue().Remove(RemoveBanner);

		m_aBannerClicks.Clear();
		m_aExitingBanners.Clear();

		//! Dropping them stops each tween's own tick - they would otherwise keep writing opacity onto
		//! cards that are about to be removed.
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
	//! No per-app offline error card - a lost connection blocks the whole phone (home, lock, every app), same as OFF, rather than leaving controls reachable behind a broken page. Never shown over ScreenOff itself.
	protected void RefreshOfflineScreen()
	{
		if (!m_wOfflineScreen || !m_Phone)
			return;

		bool off = m_eState == EPhoneScreenState.OFF;
		m_wOfflineScreen.SetVisible(!off && m_Phone.IsOffline());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnOfflineRetryClicked()
	{
		if (m_Phone)
			m_Phone.RetryConnectivity();
	}

	//------------------------------------------------------------------------------------------------
	//! Single entry point for what the screen shows. Both canvases route through here.
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

		//! The wallpaper belongs to the lock and home screens. An app page gets the plain ground back,
		//! because a list is easier to read over one flat tone - and the page itself is then what the
		//! status bar's glass sits over.
		if (m_wWallpaper)
			m_wWallpaper.SetVisible(!appOpen && !off);

		if (m_wAppWashSize)
			m_wAppWashSize.SetVisible(false);

		if (m_wNavBarSize)
			m_wNavBarSize.SetVisible(appOpen);

		//! A locked phone has no home affordance - the pad is the only way in.
		if (m_wHomeIndicator)
			m_wHomeIndicator.SetVisible(!locked && !off);

		if (locked)
			RefreshLockScreen();
		else if (!appOpen && !off)
			RefreshHomeScreen();

		if (appOpen)
			OpenApp(state, animateEntrance);
		else
			CloseApp();

		m_eState = state;
		RefreshOfflineScreen();

		//! A banner belongs to the awake screen it interrupted - the arrival is still waiting in
		//! Messages (and on the lock screen's own list) once the phone comes back.
		if (!CanShowBanner())
			ClearBanners();

		//! The phone closes the hub itself when it sleeps or locks (the flag is replicated), so this
		//! only has to re-read it - plus the indicator, which hides on those same screens.
		if (m_wNotificationHub && m_Phone)
			m_wNotificationHub.SetVisible(m_Phone.IsHubOpen());

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

		//! Back is accent-coloured text, sized to hug the chevron+label (see SizeBackChip()).
		if (m_wBackChevron)
			m_wBackChevron.SetColor(accent);

		if (m_wBackLabel)
			m_wBackLabel.SetColor(accent);

		SizeBackChip();

		m_Chrome.m_Accent = accent;
		m_App.BindChrome(m_Chrome);
		m_App.BindShell(this);

		//! The sub-state is passed into Open() so a page built on the passive copy lands directly in
		//! the right place instead of briefly showing a wrong default.
		m_App.Open(m_Phone, m_wAppHost, m_Phone.GetScreenSubState());
		RefreshBackVisibility();

		//! One entrance per screen: the page rises and fades in as a whole, no per-row stagger.
		if (animateEntrance)
			m_AppEntrance.Play(m_wAppHost, 0, 1, ELIFE_PhoneStyle.DURATION_PRESENT_MS);
		else
			m_wAppHost.SetOpacity(1);
	}

	//------------------------------------------------------------------------------------------------
	//! Sizes BackChipSize to hug the chevron+label's real combined width - a fixed guess would clip a longer translation of "Back" or leave empty space beside a short one.
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
	//! Back only pops a level of the open app's own stack - never leaves it, so an app's landing page
	//! (IsAtRoot()) draws no Back at all. Leaving is the home pill's job alone.
	void RefreshBackVisibility()
	{
		if (m_wButtonBack)
			m_wButtonBack.SetVisible(m_App && !m_App.IsAtRoot());
	}

	//------------------------------------------------------------------------------------------------
	//! True when the open app consumed Back itself (statement -> account list, thread -> inbox).
	bool AppConsumedBack()
	{
		return m_App && m_App.OnBack();
	}

	//------------------------------------------------------------------------------------------------
	// Home screen
	//------------------------------------------------------------------------------------------------

	//! Above the grid, shows the first real account balance and the newest unread thread - both cards open their own app.
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
		ResetPin();

		if (m_wLockNumber && m_Phone)
			m_wLockNumber.SetText(m_Phone.GetNumber());

		if (m_wLockDate)
			m_wLockDate.SetText(FormatToday());

		FillLockNotifications();
	}

	//------------------------------------------------------------------------------------------------
	//! Weekday plus short date, both from the world's own clock and localised by the engine.
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
	//! Real unread threads out of the phone's own message cache - never a mocked-up notification.
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
		if (m_sPinEntry.Length() >= PIN_LENGTH)
			return;

		if (m_bPinError)
			ClearPinError();

		m_sPinEntry += digit.ToString();
		RedrawPinDots();

		if (m_sPinEntry.Length() == PIN_LENGTH)
			TryUnlock();
	}

	//------------------------------------------------------------------------------------------------
	void PinBackspace()
	{
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
	protected void TryUnlock()
	{
		if (!m_Phone)
			return;

		if (m_sPinEntry == m_Phone.GetPin())
		{
			ResetPin();
			m_Phone.SetScreenState(EPhoneScreenState.HOME);
			return;
		}

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

		if (m_wPinPrompt)
		{
			m_wPinPrompt.SetText("#ELIFE-Phone_Lock_Enter");
			m_wPinPrompt.SetColor(ELIFE_PhoneStyle.TextSecondary());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetPin()
	{
		GetGame().GetCallqueue().Remove(ClearPinError);
		m_bPinError = false;
		m_sPinEntry = "";
		RedrawPinDots();

		if (m_wPinPrompt)
		{
			m_wPinPrompt.SetText("#ELIFE-Phone_Lock_Enter");
			m_wPinPrompt.SetColor(ELIFE_PhoneStyle.TextSecondary());
		}
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
		GetGame().GetCallqueue().Remove(FlushPendingApp);
	}
}
