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
	//! The app registry. Order here is the order on the home screen, filling rows of four. Bank and Contacts keep their old character-mark glyph as a load fallback since their icon-set sprites are the newest additions.
	static array<ref ELIFE_PhoneAppEntry> BuildApps()
	{
		array<ref ELIFE_PhoneAppEntry> apps = {};
		apps.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.MESSAGES, "#ELIFE-Phone_App_Messages", ELIFE_PhoneStyle.ICON_SET_WRAPPER, "comments", ""));
		apps.Insert(new ELIFE_PhoneAppEntry(EPhoneScreenState.CONTACTS, "#ELIFE-Phone_App_Contacts", ELIFE_PhoneStyle.ICON_SET_CHAT, "squad", "@", 0.92));
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
	}

	//------------------------------------------------------------------------------------------------
	void Destroy()
	{
		if (m_Phone)
		{
			m_Phone.m_OnIdentityChanged.Remove(OnIdentityChanged);
			m_Phone.m_OnConnectivityChanged.Remove(OnConnectivityChanged);
		}

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

		Widget statusBar = m_wRoot.FindAnyWidget("StatusBarSize");
		if (statusBar)
			statusBar.SetZOrder(ELIFE_PhoneStyle.ZORDER_CHROME);

		if (m_wHomeIndicator)
			m_wHomeIndicator.SetZOrder(ELIFE_PhoneStyle.ZORDER_CHROME);

		if (m_wScreenOff)
			m_wScreenOff.SetZOrder(ELIFE_PhoneStyle.ZORDER_ALERT);

		//! Same tier as ScreenOff, never visible at the same time as it (RefreshOfflineScreen() only
		//! shows this while the screen is actually on) - so which one wins never comes up in practice.
		if (m_wOfflineScreen)
			m_wOfflineScreen.SetZOrder(ELIFE_PhoneStyle.ZORDER_ALERT);

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

		Color accent = ELIFE_PhoneStyle.AccentFor(EPhoneScreenState.MESSAGES);
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

			ELIFE_PhoneStyle.ApplyGlass(card, false, true);
			ELIFE_PhoneStyle.SetColorOf(card, "NotifyMark", accent);

			SetText(card, "NotifyTitle", ELIFE_PhoneContactBook.TitleFor(m_Phone, threadDto));
			SetText(card, "NotifyBody", NewestBody(threadDto));
			SetText(card, "NotifyTime", ELIFE_PhoneAppBase.FormatClock(threadDto.lastMessageAt));

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
