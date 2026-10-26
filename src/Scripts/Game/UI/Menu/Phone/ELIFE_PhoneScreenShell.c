//------------------------------------------------------------------------------------------------
//! Opens an app from a home tile or card, and lets it be held and dragged around the home grid.
class ELIFE_PhoneTileClick : ELIFE_PhoneHoldHandler
{
	protected ELIFE_PhoneScreenShell m_Shell;
	protected EPhoneScreenState m_eState;

	//! The ELIFE_PhoneHomeItem id it moves.
	protected string m_sItemId;

	//! False for widgets that only show something, like the clock.
	protected bool m_bOpensApp;

	//! A page inside the app, e.g. a favourite's conversation.
	protected string m_sSubState;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneScreenShell shell, EPhoneScreenState state, string itemId, bool opensApp = true, string subState = "")
	{
		m_Shell = shell;
		m_eState = state;
		m_sItemId = itemId;
		m_bOpensApp = opensApp;
		m_sSubState = subState;
	}

	//------------------------------------------------------------------------------------------------
	//! In edit mode a press picks the item straight up.
	override bool IsHoldImmediate()
	{
		return m_Shell && m_Shell.IsEditingHome();
	}

	//------------------------------------------------------------------------------------------------
	override bool OnHoldStart(Widget w, float canvasX, float canvasY)
	{
		return m_Shell && m_Shell.BeginHomeDrag(m_sItemId, canvasX, canvasY);
	}

	//------------------------------------------------------------------------------------------------
	override void OnHoldMove(Widget w, float canvasX, float canvasY)
	{
		if (m_Shell)
			m_Shell.UpdateHomeDrag(canvasX, canvasY);
	}

	//------------------------------------------------------------------------------------------------
	override void OnHoldEnd(Widget w, float canvasX, float canvasY)
	{
		if (m_Shell)
			m_Shell.EndHomeDrag(canvasX, canvasY);
	}

	//------------------------------------------------------------------------------------------------
	override void OnHoldCancel(Widget w)
	{
		if (m_Shell)
			m_Shell.CancelHomeDrag();
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (!m_Shell || !m_bOpensApp)
			return false;

		if (m_sSubState != "")
			m_Shell.RequestAppPage(m_eState, m_sSubState);
		else
			m_Shell.RequestApp(m_eState);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
enum EPhoneHomeEditAction
{
	OPEN_SHEET,
	OPEN_WALLPAPER,
	CLOSE_SHEET,
	ADD,
	PICK_WALLPAPER,
	DONE
}

//------------------------------------------------------------------------------------------------
//! The home screen itself: a hold on empty space opens edit mode with nothing picked up.
class ELIFE_PhoneHomeBackdrop : ELIFE_PhoneHoldHandler
{
	protected ELIFE_PhoneScreenShell m_Shell;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneScreenShell shell)
	{
		m_Shell = shell;
	}

	//------------------------------------------------------------------------------------------------
	//! Declines the hold, so nothing is dragged; the release that follows clicks nothing.
	override bool OnHoldStart(Widget w, float canvasX, float canvasY)
	{
		if (m_Shell)
			m_Shell.OnBackdropHold();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A home edit-mode control: Add widget, Wallpaper, Done, an entry in a picker sheet, or its backdrop.
class ELIFE_PhoneHomeEditClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneScreenShell m_Shell;
	protected EPhoneHomeEditAction m_eAction;
	protected string m_sItemId;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneScreenShell shell, EPhoneHomeEditAction action, string itemId = "")
	{
		m_Shell = shell;
		m_eAction = action;
		m_sItemId = itemId;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_Shell)
			m_Shell.OnHomeEditAction(m_eAction, m_sItemId);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! A home item eased out of the way of a drag, drawn as a copy in the drag layer while the original
//! stays hidden. Positions are drag-layer px.
class ELIFE_PhoneHomeShift
{
	ELIFE_PhoneHomeItem m_Item;
	Widget m_wCopy;
	float m_fFromX;
	float m_fFromY;
	float m_fToX;
	float m_fToY;
	int m_iStartMs;

	//! Heading back to its own cell, after which it goes away.
	bool m_bReturning;
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
	protected const ResourceName LAYOUT_HOME_CELL = "{7C29D4A9E3B25F8E}UI/layouts/Menus/Phone/Widgets/PhoneHomeCell.layout";
	protected const ResourceName LAYOUT_CHIP = "{7C2AD4A9E3B25F8F}UI/layouts/Menus/Phone/Widgets/PhoneChipButton.layout";
	protected const ResourceName LAYOUT_WIDGET_SHEET = "{7C2BD4A9E3B25F90}UI/layouts/Menus/Phone/Widgets/PhoneWidgetSheet.layout";
	protected const ResourceName LAYOUT_CLOCK_WIDGET = "{7C2CD4A9E3B25F91}UI/layouts/Menus/Phone/Widgets/PhoneClockWidget.layout";
	protected const ResourceName LAYOUT_WIDGET_SMALL = "{7C2DD4A9E3B25F92}UI/layouts/Menus/Phone/Widgets/PhoneHomeWidgetSmall.layout";
	protected const ResourceName LAYOUT_FAVOURITES_WIDGET = "{7C2ED4A9E3B25F93}UI/layouts/Menus/Phone/Widgets/PhoneFavouritesWidget.layout";
	protected const ResourceName LAYOUT_FAVOURITE_ENTRY = "{7C2FD4A9E3B25F94}UI/layouts/Menus/Phone/Widgets/PhoneFavouriteEntry.layout";
	protected const ResourceName LAYOUT_INFO_WIDGET = "{7C31D4A9E3B25F96}UI/layouts/Menus/Phone/Widgets/PhoneInfoWidget.layout";
	protected const ResourceName LAYOUT_SWATCH = "{7C30D4A9E3B25F95}UI/layouts/Menus/Phone/Widgets/PhoneWallpaperSwatch.layout";
	protected const float SWATCH_CHECK_SIZE = 22;
	protected const int SWATCHES_PER_ROW = 5;

	//! sound-off's transparent padding is ~half the cell; 30 lands the mark with the battery.
	protected const float STATUS_ICON_BOX = 30;

	protected const string ICON_DND = "sound-off";
	protected const string ICON_LOCATION = "marker";
	protected const string ICON_CHECK = "check";
	protected const float INFO_ICON_SIZE = 44;

	//! How many favourites fit the widget's row.
	protected const int FAVOURITES_SHOWN = 4;

	//! Weather and location move on their own, so they are re-read while home is up.
	protected const int LIVE_WIDGET_REFRESH_MS = 3000;

	//! Canvas px a press in edit mode may move and still count as a tap, which resizes a widget.
	protected const float EDIT_TAP_SLOP_PX = 12;

	protected const ResourceName LAYOUT_APP_ICON = "{7C27D4A9E3B25F8C}UI/layouts/Menus/Phone/Widgets/PhoneAppIcon.layout";

	//! Visible ink an app icon should show, on a tile and on a home-card badge.
	protected const float ICON_INK_TILE = 46;
	protected const float ICON_INK_CARD = 36;
	protected const float ICON_INK_WIDGET = 26;

	//! Space around a widget inside its cells; about what a tile leaves around itself.
	protected const float WIDGET_GUTTER = ELIFE_PhoneStyle.SPACE_1;

	//! Icon sprites have transparent padding, so the box is enlarged until the ink matches.
	protected const float ICON_SPRITE_PADDING = 1.34;

	//! Cap height is ~0.72 of font size, so marks are set larger than the ink they should show.
	protected const float ICON_MARK_CAP = 1.38;

	protected const int PIN_LENGTH = ELIFE_PhoneGadgetComponent.PIN_LENGTH;
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

	//! Notification ids already accounted for; a banner fires for any id not in here.
	protected ref map<string, bool> m_mSeenNotifications = new map<string, bool>();

	//! The first payload only seeds the baseline, so opening the phone doesn't replay what's waiting.
	protected bool m_bNotificationsSeeded;

	protected bool m_bBannerOnly;

	//! Compact card size relative to the 2x card, tuned so the peek matches the banner on the held phone.
	//! PhonePeekCard.layout is authored at this scale.
	protected const float COMPACT_SCALE = 0.55;

	//! BackBody's 16px padding on each side plus the chevron's 6px gap to the label.
	protected const float BACK_CHIP_CHROME = 38;
	protected const string BACK_LABEL = "#ELIFE-Phone_Nav_Back";

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
	protected Widget m_wHomeGrid;
	protected Widget m_wHomeDragLayer;
	protected Widget m_wStatusBarSize;
	protected Widget m_wStatusDndSize;
	protected ref array<ref ELIFE_PhoneTileClick> m_aFavouriteClicks = {};
	protected ref ELIFE_PhoneHomeBackdrop m_HomeBackdrop;

	//! Wallpaper id -> its swatch's check mark, while the wallpaper sheet is open.
	protected ref map<string, Widget> m_mSwatchChecks = new map<string, Widget>();

	//! An edit-mode press that hasn't moved yet: let go, it is a tap.
	protected bool m_bEditTap;
	protected float m_fPressX;
	protected float m_fPressY;
	protected Widget m_wHomeEditBar;
	protected Widget m_wHomeRemoveZone;
	protected Widget m_wHomeRemoveWash;
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
	protected ref array<ref ELIFE_PhoneTileClick> m_aHomeClicks = {};

	//! The home grid as last built, so a push that changes nothing doesn't rebuild it.
	protected ref ELIFE_PhoneHomeLayout m_HomeLayout;
	protected string m_sHomeSignature;
	protected ref map<string, Widget> m_mHomeCells = new map<string, Widget>();

	//! Edit mode: a press drags at once, and the header becomes the edit bar - or, while something
	//! removable is held, the remove target.
	protected bool m_bEditingHome;
	protected ref array<ref ELIFE_PhoneHomeEditClick> m_aEditClicks = {};
	protected Widget m_wWidgetSheet;
	protected ref array<ref ELIFE_PhoneHomeEditClick> m_aSheetClicks = {};
	protected bool m_bOverRemoveZone;

	//! An app dropped on the remove target, until the Bridge answers.
	protected string m_sPendingRemoveId;

	//! A held item being dragged: a copy follows the cursor in the drag layer while the original stays
	//! hidden in its cell, a faint second copy marks where it would land, and what it would swap with
	//! eases aside. Positions are drag-layer px; the grid rect is taken at pickup.
	protected ELIFE_PhoneHomeItem m_DragItem;
	protected Widget m_wDragGhost;
	protected Widget m_wDropHint;
	protected ref array<ref ELIFE_PhoneHomeShift> m_aShifts = {};
	protected float m_fLayerX;
	protected float m_fLayerY;
	protected float m_fGridX;
	protected float m_fGridY;
	protected float m_fGridWidth;
	protected float m_fGridHeight;
	protected float m_fDragOffsetX;
	protected float m_fDragOffsetY;

	//! The grid can't rebuild mid-drag, since that drops the held item's handler.
	protected bool m_bGridStale;
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

	//! Settings' Adjust home screen: go home, then enter edit - see RequestHomeEdit().
	protected bool m_bPendingHomeEdit;

	protected ref ScriptInvoker m_OnHomePill = new ScriptInvoker();
	protected ref ScriptInvoker m_OnBack = new ScriptInvoker();

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
			case EPhoneScreenState.STORE: return new ELIFE_PhoneStoreApp();
			case EPhoneScreenState.SETUP: return new ELIFE_PhoneSetupApp();
		}

		return null;
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
		m_wHomeGrid = screenRoot.FindAnyWidget("HomeGrid");
		m_wHomeDragLayer = screenRoot.FindAnyWidget("HomeDragLayer");
		m_wStatusBarSize = screenRoot.FindAnyWidget("StatusBarSize");
		m_wStatusDndSize = screenRoot.FindAnyWidget("StatusDndSize");

		ImageWidget dndIcon = ImageWidget.Cast(screenRoot.FindAnyWidget("StatusDnd"));
		if (dndIcon && dndIcon.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_DND))
		{
			dndIcon.SetColor(ELIFE_PhoneStyle.TextPrimary());
			ELIFE_PhoneStyle.FitIcon(dndIcon, STATUS_ICON_BOX);
		}
		m_wHomeEditBar = screenRoot.FindAnyWidget("HomeEditBar");
		m_wHomeRemoveZone = screenRoot.FindAnyWidget("HomeRemoveZoneSize");
		m_wHomeRemoveWash = screenRoot.FindAnyWidget("HomeRemoveWash");
		ELIFE_PhoneStyle.ApplyGlass(screenRoot.FindAnyWidget("HomeRemoveZone"), true);
		ELIFE_PhoneStyle.SetColorOf(screenRoot, "HomeRemoveLabel", ELIFE_PhoneStyle.Negative());
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

		phone.m_OnHomeLayoutChanged.Insert(OnHomeLayoutChanged);
		if (m_bInteractive)
			phone.m_OnAppInstallResult.Insert(OnAppInstallResult);

		phone.m_OnPersonalizationChanged.Insert(OnPersonalizationChanged);

		if (m_bInteractive && m_wHomeScreen)
		{
			//! Containers ignore the cursor by default, which would let a press on empty space hit nothing.
			m_wHomeScreen.ClearFlags(WidgetFlags.IGNORE_CURSOR);

			m_HomeBackdrop = new ELIFE_PhoneHomeBackdrop();
			m_HomeBackdrop.Bind(this);
			m_wHomeScreen.AddHandler(m_HomeBackdrop);
		}
		ApplyWallpaper();

		GetGame().GetCallqueue().CallLater(RefreshLiveWidgets, LIVE_WIDGET_REFRESH_MS, true);
		RefreshNotifications();

		//! Seed from the cache now so the wake poll can still count as an arrival.
		RaiseArrivedBanners();

		//! Home and lock resolve thread names through the contact book, so fetch contacts here too.
		phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);

		//! Home's balance card.
		phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS);

		//! The grid is drawn from the cached set now and rebuilt if this changes it.
		phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_APPS);

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
			m_Phone.m_OnHomeLayoutChanged.Remove(OnHomeLayoutChanged);
			m_Phone.m_OnAppInstallResult.Remove(OnAppInstallResult);
			m_Phone.m_OnPersonalizationChanged.Remove(OnPersonalizationChanged);
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
		GetGame().GetCallqueue().Remove(FlushPendingHomeEdit);
		GetGame().GetCallqueue().Remove(BuildHomeGrid);
		GetGame().GetCallqueue().Remove(RefreshLiveWidgets);
		DestroyWidgetSheet();

		//! Not stale: nothing is left to rebuild.
		m_bGridStale = false;
		ClearHomeDrag();
		m_bEditingHome = false;
		m_bPendingHomeEdit = false;

		if (m_wHomeScreen && m_HomeBackdrop)
			m_wHomeScreen.RemoveHandler(m_HomeBackdrop);

		m_HomeBackdrop = null;
		m_aHomeClicks.Clear();
		m_aEditClicks.Clear();
		m_aFavouriteClicks.Clear();
		m_mHomeCells.Clear();
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
	//! Widgets first, in default reading order - clock and weather side by side, balance, messages,
	//! favourites; location waits in the sheet - then installed apps in catalog order, built-in ones
	//! always. A widget backed by an app exists only while that app is installed.
	protected void CollectHomeItems(notnull ELIFE_PhoneHomeLayout layout)
	{
		if (!m_Phone)
			return;

		array<int> half = {ELIFE_PhoneHomeLayout.HALF};
		array<int> full = {ELIFE_PhoneHomeLayout.FULL};
		array<int> fullOrHalf = {ELIFE_PhoneHomeLayout.FULL, ELIFE_PhoneHomeLayout.HALF};

		layout.AddWidget(ELIFE_PhoneHomeLayout.CLOCK_ID, EPhoneScreenState.HOME, false, half);
		layout.AddWidget(ELIFE_PhoneHomeLayout.WEATHER_ID, EPhoneScreenState.HOME, false, half);

		if (m_Phone.IsAppAvailable(EPhoneScreenState.BANK))
			layout.AddWidget(CardId(EPhoneScreenState.BANK), EPhoneScreenState.BANK, true, fullOrHalf);

		if (m_Phone.IsAppAvailable(EPhoneScreenState.MESSAGES))
			layout.AddWidget(CardId(EPhoneScreenState.MESSAGES), EPhoneScreenState.MESSAGES, true, fullOrHalf);

		layout.AddWidget(ELIFE_PhoneHomeLayout.FAVOURITES_ID, EPhoneScreenState.CONTACTS, true, full);
		if (m_Phone.IsAppAvailable(EPhoneScreenState.MAP))
			layout.AddWidget(ELIFE_PhoneHomeLayout.LOCATION_ID, EPhoneScreenState.MAP, true, half, true);

		array<ref ELIFE_PhoneAppEntry> catalog = ELIFE_PhoneAppCatalog.GetEntries();
		foreach (ELIFE_PhoneAppEntry app : catalog)
		{
			if (m_Phone.IsAppAvailable(app.m_eState))
				layout.AddTile(app);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected string CardId(EPhoneScreenState state)
	{
		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.Find(state);
		if (!app)
			return "";

		return ELIFE_PhoneHomeLayout.CARD_PREFIX + app.m_sAppKey;
	}

	//------------------------------------------------------------------------------------------------
	//! Lays out tiles and cards on the grid, each in a cell anchored to its fraction of the grid.
	protected void BuildHomeGrid()
	{
		if (!m_wHomeGrid)
			return;

		if (m_DragItem)
		{
			m_bGridStale = true;
			return;
		}

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		//! What a finished drop left floating, kept until now so nothing blinks out for a frame.
		ClearFloating();

		ELIFE_PhoneHomeLayout layout = new ELIFE_PhoneHomeLayout();
		CollectHomeItems(layout);
		layout.Place(GetSavedHomeLayout());

		string signature = layout.Serialize();
		if (signature == m_sHomeSignature && m_wHomeGrid.GetChildren())
			return;

		m_sHomeSignature = signature;
		m_HomeLayout = layout;
		m_aHomeClicks.Clear();
		m_mHomeCells.Clear();
		ClearChildren(m_wHomeGrid);

		float columns = ELIFE_PhoneHomeLayout.COLUMNS;
		float rows = ELIFE_PhoneHomeLayout.ROWS;

		foreach (ELIFE_PhoneHomeItem item : layout.GetItems())
		{
			//! No room left on the grid.
			if (!item.IsPlaced())
				continue;

			Widget cell = workspace.CreateWidgets(LAYOUT_HOME_CELL, m_wHomeGrid);
			if (!cell)
				continue;

			FrameSlot.SetAnchorMin(cell, item.m_iColumn / columns, item.m_iRow / rows);
			FrameSlot.SetAnchorMax(cell, (item.m_iColumn + item.m_iWidth) / columns, (item.m_iRow + item.m_iHeight) / rows);
			FrameSlot.SetOffsets(cell, 0, 0, 0, 0);

			CreateItemContent(workspace, cell, item, m_bInteractive);
			m_mHomeCells.Insert(item.m_sId, cell);
			RestCell(item.m_sId, cell);
		}

		FillHomeCards();
		RefreshEditBar();
	}

	//------------------------------------------------------------------------------------------------
	//! A tile or card, centred in cell. Cards are created empty - see FillHomeCards.
	//! fillCell: a widget takes its cells' full height, inset by the gutter; off the grid (the sheet)
	//! it keeps its own height instead.
	protected Widget CreateItemContent(notnull WorkspaceWidget workspace, notnull Widget cell, notnull ELIFE_PhoneHomeItem item, bool clickable, bool fillCell = true)
	{
		bool clock = item.m_sId == ELIFE_PhoneHomeLayout.CLOCK_ID;
		bool favourites = item.m_sId == ELIFE_PhoneHomeLayout.FAVOURITES_ID;

		//! Weather and location sit straight on the wallpaper like the clock, an icon beside the reading.
		bool info = item.m_sId == ELIFE_PhoneHomeLayout.WEATHER_ID || item.m_sId == ELIFE_PhoneHomeLayout.LOCATION_ID;

		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.Find(item.m_eState);
		if (!app && !clock && !info)
			return null;

		ResourceName layout = LAYOUT_TILE;
		if (clock)
			layout = LAYOUT_CLOCK_WIDGET;
		else if (info)
			layout = LAYOUT_INFO_WIDGET;
		else if (favourites)
			layout = LAYOUT_FAVOURITES_WIDGET;
		else if (item.m_bCard && item.m_iWidth < ELIFE_PhoneHomeLayout.FULL)
			layout = LAYOUT_WIDGET_SMALL;
		else if (item.m_bCard)
			layout = LAYOUT_HOME_CARD;

		Widget content = workspace.CreateWidgets(layout, cell);
		if (!content)
			return null;

		AlignableSlot.SetHorizontalAlign(content, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(content, LayoutVerticalAlign.Center);

		if (item.m_bCard && fillCell)
		{
			AlignableSlot.SetVerticalAlign(content, LayoutVerticalAlign.Stretch);
			AlignableSlot.SetPadding(content, WIDGET_GUTTER, WIDGET_GUTTER, WIDGET_GUTTER, WIDGET_GUTTER);

			SizeLayoutWidget size = SizeLayoutWidget.Cast(content);
			if (size)
				size.EnableHeightOverride(false);
		}

		if (clock)
			FillClock(content);
		else if (info)
			FillCard(content, item);
		else if (favourites)
			ELIFE_PhoneStyle.ApplyGlass(content, false, true);
		else if (item.m_bCard)
			PaintCard(workspace, content, app, item.m_iWidth < ELIFE_PhoneHomeLayout.FULL);
		else
			PaintTile(workspace, content, app);

		if (!clickable)
			return content;

		Widget button = content.FindAnyWidget(ItemButtonName(item));
		if (!button)
			return content;

		ELIFE_PhoneTileClick click = new ELIFE_PhoneTileClick();
		click.Bind(this, item.m_eState, item.m_sId, item.m_bOpensApp);
		button.AddHandler(click);
		m_aHomeClicks.Insert(click);
		return content;
	}

	//------------------------------------------------------------------------------------------------
	protected string ItemButtonName(notnull ELIFE_PhoneHomeItem item)
	{
		if (item.m_sId == ELIFE_PhoneHomeLayout.CLOCK_ID)
			return "ClockButton";

		if (item.m_sId == ELIFE_PhoneHomeLayout.WEATHER_ID || item.m_sId == ELIFE_PhoneHomeLayout.LOCATION_ID)
			return "InfoButton";

		if (item.m_bCard)
			return "CardButton";

		return "TileButton";
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintTile(notnull WorkspaceWidget workspace, notnull Widget tile, notnull ELIFE_PhoneAppEntry app)
	{
		ELIFE_PhoneStyle.ApplyGlass(tile, false, false, false, true, ELIFE_PhoneStyle.AccentDeepFor(app.m_eState));

		PaintAppIcon(workspace, tile.FindAnyWidget("TileIconSize"), TextWidget.Cast(tile.FindAnyWidget("TileMark")), app, ICON_INK_TILE);

		TextWidget label = TextWidget.Cast(tile.FindAnyWidget("TileLabel"));
		if (label)
		{
			label.SetText(app.m_sLabel);
			label.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintCard(notnull WorkspaceWidget workspace, notnull Widget card, notnull ELIFE_PhoneAppEntry app, bool small = false)
	{
		ELIFE_PhoneStyle.ApplyGlass(card, false, true);
		ELIFE_PhoneStyle.ApplyGlass(card.FindAnyWidget("CardBadge"), false, false, false, true, ELIFE_PhoneStyle.AccentDeepFor(app.m_eState));

		float ink = ICON_INK_CARD;
		if (small)
			ink = ICON_INK_WIDGET;

		PaintAppIcon(workspace, card.FindAnyWidget("CardIconSize"), TextWidget.Cast(card.FindAnyWidget("CardMark")), app, ink);
	}

	//------------------------------------------------------------------------------------------------
	//! The layout belongs to the phone, so its holder and bystanders all read the same one.
	protected string GetSavedHomeLayout()
	{
		if (m_Phone)
			return m_Phone.GetHomeLayout();

		return "";
	}

	//------------------------------------------------------------------------------------------------
	protected void SaveHomeLayout()
	{
		if (m_HomeLayout && m_Phone)
			m_Phone.SetHomeLayout(m_HomeLayout.Serialize(GetSavedHomeLayout()));
	}

	//------------------------------------------------------------------------------------------------
	//! Deferred: on the authority the save echoes back synchronously, from inside a home handler.
	protected void OnHomeLayoutChanged()
	{
		GetGame().GetCallqueue().Remove(BuildHomeGrid);
		GetGame().GetCallqueue().CallLater(BuildHomeGrid, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Deferred: callers are often handlers on the very widgets a rebuild deletes.
	protected void QueueHomeRebuild()
	{
		m_sHomeSignature = "";
		GetGame().GetCallqueue().Remove(BuildHomeGrid);
		GetGame().GetCallqueue().CallLater(BuildHomeGrid, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	//! An app on its way out stays dimmed until the apps push removes it.
	protected void RestCell(string itemId, notnull Widget cell)
	{
		if (ELIFE_PhoneAppCatalog.SameKey(itemId, m_sPendingRemoveId))
			cell.SetOpacity(ELIFE_PhoneStyle.PENDING_OPACITY);
		else
			cell.SetOpacity(1);
	}

	//------------------------------------------------------------------------------------------------
	// Home edit mode
	//------------------------------------------------------------------------------------------------

	bool IsEditingHome()
	{
		return m_bEditingHome;
	}

	//------------------------------------------------------------------------------------------------
	protected void EnterHomeEdit()
	{
		if (m_bEditingHome || !m_bInteractive)
			return;

		m_bEditingHome = true;
		ShowStatusBar(false);
		RefreshEditBar();
	}

	//------------------------------------------------------------------------------------------------
	//! Leaves the open app and enters home edit. Deferred: the caller is a click in the app this closes.
	void RequestHomeEdit()
	{
		if (!m_bInteractive)
			return;

		m_bPendingHomeEdit = true;
		GetGame().GetCallqueue().Remove(FlushPendingHomeEdit);
		GetGame().GetCallqueue().CallLater(FlushPendingHomeEdit, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void FlushPendingHomeEdit()
	{
		if (!m_bPendingHomeEdit)
			return;

		m_bPendingHomeEdit = false;

		if (m_eState != EPhoneScreenState.HOME)
			m_OnAppRequested.Invoke(EPhoneScreenState.HOME, "");

		EnterHomeEdit();
	}

	//------------------------------------------------------------------------------------------------
	void OnBackdropHold()
	{
		if (m_eState == EPhoneScreenState.HOME && !m_DragItem)
			EnterHomeEdit();
	}

	//------------------------------------------------------------------------------------------------
	protected void ExitHomeEdit()
	{
		if (!m_bEditingHome)
			return;

		m_bEditingHome = false;
		ClearHomeDrag();
		CloseWidgetSheet();
		ShowStatusBar(true);

		//! The bar is cleared with the rebuild, as this may run inside one of its own handlers.
		if (m_wHomeEditBar)
			m_wHomeEditBar.SetVisible(false);

		QueueHomeRebuild();
	}

	//------------------------------------------------------------------------------------------------
	//! The status bar makes way for the edit bar and the remove target, which use its band.
	protected void ShowStatusBar(bool show)
	{
		if (m_wStatusBarSize)
			m_wStatusBarSize.SetVisible(show);
	}

	//------------------------------------------------------------------------------------------------
	//! Add widget while a removed card has room for it, Wallpaper, then Done on the trailing edge.
	protected void RefreshEditBar()
	{
		if (!m_wHomeEditBar)
			return;

		m_aEditClicks.Clear();
		ClearChildren(m_wHomeEditBar);
		m_wHomeEditBar.SetVisible(m_bEditingHome && !m_DragItem);

		if (!m_bEditingHome)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (HasAddableWidget())
			CreateEditChip(workspace, m_wHomeEditBar, "#ELIFE-Phone_Home_AddWidget", EPhoneHomeEditAction.OPEN_SHEET, "", m_aEditClicks);

		Widget wallpaper = CreateEditChip(workspace, m_wHomeEditBar, "#ELIFE-Phone_Home_Wallpaper", EPhoneHomeEditAction.OPEN_WALLPAPER, "", m_aEditClicks);
		if (wallpaper && m_wHomeEditBar.GetChildren() != wallpaper)
			LayoutSlot.SetPadding(wallpaper, ELIFE_PhoneStyle.SPACE_1, 0, 0, 0);

		Widget spacer = workspace.CreateWidgets(LAYOUT_GRID_ROW, m_wHomeEditBar);
		if (spacer)
			LayoutSlot.SetSizeMode(spacer, LayoutSizeMode.Fill);

		CreateEditChip(workspace, m_wHomeEditBar, "#ELIFE-Phone_Home_Done", EPhoneHomeEditAction.DONE, "", m_aEditClicks);
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasAddableWidget()
	{
		if (!m_HomeLayout)
			return false;

		foreach (ELIFE_PhoneHomeItem item : m_HomeLayout.GetItems())
		{
			if (item.m_bRemoved && m_HomeLayout.HasRoomFor(item))
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected Widget CreateEditChip(notnull WorkspaceWidget workspace, notnull Widget parent, string label, EPhoneHomeEditAction action, string itemId, notnull array<ref ELIFE_PhoneHomeEditClick> clicks)
	{
		Widget chip = workspace.CreateWidgets(LAYOUT_CHIP, parent);
		if (!chip)
			return null;

		AlignableSlot.SetVerticalAlign(chip, LayoutVerticalAlign.Center);
		ELIFE_PhoneStyle.ApplyGlass(chip.FindAnyWidget("Chip"), false, true);
		SetText(chip, "ChipLabel", label);
		BindEditClick(chip.FindAnyWidget("ChipButton"), action, itemId, clicks);
		return chip;
	}

	//------------------------------------------------------------------------------------------------
	protected void BindEditClick(Widget button, EPhoneHomeEditAction action, string itemId, notnull array<ref ELIFE_PhoneHomeEditClick> clicks)
	{
		if (!button)
			return;

		ELIFE_PhoneHomeEditClick click = new ELIFE_PhoneHomeEditClick();
		click.Bind(this, action, itemId);
		button.AddHandler(click);
		clicks.Insert(click);
	}

	//------------------------------------------------------------------------------------------------
	void OnHomeEditAction(EPhoneHomeEditAction action, string itemId)
	{
		if (!m_bEditingHome || !m_HomeLayout)
			return;

		switch (action)
		{
			case EPhoneHomeEditAction.DONE:
				ExitHomeEdit();
				break;

			case EPhoneHomeEditAction.OPEN_SHEET:
				OpenWidgetSheet();
				break;

			case EPhoneHomeEditAction.CLOSE_SHEET:
				CloseWidgetSheet();
				break;

			case EPhoneHomeEditAction.ADD:
				AddHomeCard(itemId);
				break;

			case EPhoneHomeEditAction.OPEN_WALLPAPER:
				OpenWallpaperSheet();
				break;

			case EPhoneHomeEditAction.PICK_WALLPAPER:
				if (m_Phone)
					m_Phone.SetWallpaper(itemId);
				break;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A picker sheet over the home screen; its backdrop closes it. Null if one is already open.
	protected Widget CreateSheet(notnull WorkspaceWidget workspace, string title)
	{
		if (m_wWidgetSheet || !m_wHomeScreen)
			return null;

		Widget sheet = workspace.CreateWidgets(LAYOUT_WIDGET_SHEET, m_wHomeScreen);
		if (!sheet)
			return null;

		AlignableSlot.SetHorizontalAlign(sheet, LayoutHorizontalAlign.Stretch);
		AlignableSlot.SetVerticalAlign(sheet, LayoutVerticalAlign.Stretch);
		ELIFE_PhoneStyle.ApplyGlass(sheet.FindAnyWidget("SheetPanel"), true);
		SetText(sheet, "SheetTitle", title);
		BindEditClick(sheet.FindAnyWidget("SheetDismiss"), EPhoneHomeEditAction.CLOSE_SHEET, "", m_aSheetClicks);

		m_wWidgetSheet = sheet;
		return sheet;
	}

	//------------------------------------------------------------------------------------------------
	//! Every removed widget there is room for, shown live as it would sit on the grid; a tap adds it.
	protected void OpenWidgetSheet()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || !m_HomeLayout)
			return;

		Widget sheet = CreateSheet(workspace, "#ELIFE-Phone_Home_Widgets");
		if (!sheet)
			return;

		Widget list = sheet.FindAnyWidget("SheetList");
		foreach (ELIFE_PhoneHomeItem item : m_HomeLayout.GetItems())
		{
			if (!list || !item.m_bRemoved || !m_HomeLayout.HasRoomFor(item))
				continue;

			//! A row of the grid's four columns, so each widget shows at its own width.
			Widget row = workspace.CreateWidgets(LAYOUT_GRID_ROW, list);
			if (!row)
				continue;

			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);
			if (list.GetChildren() != row)
				LayoutSlot.SetPadding(row, 0, ELIFE_PhoneStyle.SPACE_2, 0, 0);

			Widget preview = CreateItemContent(workspace, row, item, false, false);
			if (!preview)
				continue;

			LayoutSlot.SetSizeMode(preview, LayoutSizeMode.Fill);
			LayoutSlot.SetFillWeight(preview, item.m_iWidth);

			if (item.m_iWidth < ELIFE_PhoneHomeLayout.FULL)
			{
				Widget rest = workspace.CreateWidgets(LAYOUT_GRID_ROW, row);
				if (rest)
				{
					LayoutSlot.SetSizeMode(rest, LayoutSizeMode.Fill);
					LayoutSlot.SetFillWeight(rest, ELIFE_PhoneHomeLayout.FULL - item.m_iWidth);
				}
			}

			FillCard(preview, item);
			BindEditClick(preview.FindAnyWidget(ItemButtonName(item)), EPhoneHomeEditAction.ADD, item.m_sId, m_aSheetClicks);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! A swatch per wallpaper, painted like the screen it sets. Stays open so several can be tried.
	protected void OpenWallpaperSheet()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		Widget sheet = CreateSheet(workspace, "#ELIFE-Phone_Home_Wallpaper");
		if (!sheet)
			return;

		//! A short scrolling window and no scrim, so the screen above shows each pick as it is tried on.
		SetVisibleOf(sheet, "SheetScrim", false);

		Widget list = sheet.FindAnyWidget("SheetScrollList");
		Widget scrollSize = sheet.FindAnyWidget("SheetScrollSize");
		if (!list || !scrollSize)
			return;

		scrollSize.SetVisible(true);
		SetVisibleOf(sheet, "SheetList", false);

		Widget row;
		foreach (int i, ELIFE_PhoneWallpaper wallpaper : ELIFE_PhoneWallpapers.GetEntries())
		{
			if (i % SWATCHES_PER_ROW == 0)
			{
				row = workspace.CreateWidgets(LAYOUT_GRID_ROW, list);
				if (!row)
					return;

				AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Center);
				if (i > 0)
					LayoutSlot.SetPadding(row, 0, ELIFE_PhoneStyle.SPACE_2, 0, 0);
			}

			Widget swatch = workspace.CreateWidgets(LAYOUT_SWATCH, row);
			if (!swatch)
				continue;

			if (row.GetChildren() != swatch)
				LayoutSlot.SetPadding(swatch, ELIFE_PhoneStyle.SPACE_2, 0, 0, 0);

			PaintWallpaper(swatch, "Swatch", wallpaper, wallpaper.m_sSwatchPhoto);

			ImageWidget check = ImageWidget.Cast(swatch.FindAnyWidget("SwatchCheck"));
			if (check && check.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_CHECK))
				ELIFE_PhoneStyle.FitIcon(check, SWATCH_CHECK_SIZE);

			m_mSwatchChecks.Insert(wallpaper.m_sId, swatch.FindAnyWidget("SwatchCheckSize"));
			BindEditClick(swatch.FindAnyWidget("SwatchButton"), EPhoneHomeEditAction.PICK_WALLPAPER, wallpaper.m_sId, m_aSheetClicks);
		}

		RefreshSwatchChecks();
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshSwatchChecks()
	{
		if (!m_Phone)
			return;

		ELIFE_PhoneWallpaper current = ELIFE_PhoneWallpapers.Resolve(m_Phone.GetWallpaper());
		foreach (string wallpaperId, Widget check : m_mSwatchChecks)
		{
			if (check)
				check.SetVisible(current && current.m_sId == wallpaperId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Hidden now, deleted next frame: this runs inside the sheet's own handlers.
	protected void CloseWidgetSheet()
	{
		if (!m_wWidgetSheet)
			return;

		m_wWidgetSheet.SetVisible(false);
		GetGame().GetCallqueue().Remove(DestroyWidgetSheet);
		GetGame().GetCallqueue().CallLater(DestroyWidgetSheet, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void DestroyWidgetSheet()
	{
		if (m_wWidgetSheet)
			m_wWidgetSheet.RemoveFromHierarchy();

		m_wWidgetSheet = null;
		m_aSheetClicks.Clear();
		m_mSwatchChecks.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void AddHomeCard(string itemId)
	{
		CloseWidgetSheet();

		ELIFE_PhoneHomeItem item = m_HomeLayout.Find(itemId);
		if (!item || !m_HomeLayout.Restore(item))
			return;

		SaveHomeLayout();
		QueueHomeRebuild();
	}

	//------------------------------------------------------------------------------------------------
	//! Cards can always come off; apps only if they can be uninstalled.
	protected bool IsRemovable(notnull ELIFE_PhoneHomeItem item)
	{
		if (item.m_bCard)
			return true;

		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.Find(item.m_eState);
		return app && !app.IsSystem();
	}

	//------------------------------------------------------------------------------------------------
	//! A card leaves the grid at once; an app is uninstalled and leaves with the next apps push.
	protected void RemoveHomeItem(notnull ELIFE_PhoneHomeItem item)
	{
		if (item.m_bCard)
		{
			m_HomeLayout.Remove(item);
			SaveHomeLayout();
			QueueHomeRebuild();
			return;
		}

		ELIFE_PhoneAppEntry app = ELIFE_PhoneAppCatalog.Find(item.m_eState);
		if (!app || !m_Phone || m_sPendingRemoveId != "")
			return;

		m_sPendingRemoveId = item.m_sId;
		m_Phone.SetAppInstalled(app.m_sAppKey, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnAppInstallResult(string appKey, bool installed, bool succeeded)
	{
		if (!ELIFE_PhoneAppCatalog.SameKey(appKey, m_sPendingRemoveId))
			return;

		string itemId = m_sPendingRemoveId;
		m_sPendingRemoveId = "";

		Widget cell;
		if (m_mHomeCells.Find(itemId, cell))
			RestCell(itemId, cell);
	}

	//------------------------------------------------------------------------------------------------
	// Home drag
	//------------------------------------------------------------------------------------------------

	//! Lifts the held item into the drag layer, entering edit mode on the way. False leaves the press a
	//! plain tap.
	bool BeginHomeDrag(string itemId, float canvasX, float canvasY)
	{
		if (m_DragItem || !m_HomeLayout || !m_wHomeGrid || !m_wHomeDragLayer || m_eState != EPhoneScreenState.HOME)
			return false;

		ELIFE_PhoneHomeItem item = m_HomeLayout.Find(itemId);
		Widget source;
		if (!item || !item.IsPlaced() || !m_mHomeCells.Find(item.m_sId, source))
			return false;

		//! Already on its way out.
		if (ELIFE_PhoneAppCatalog.SameKey(item.m_sId, m_sPendingRemoveId))
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		//! Screen rects are DPI-scaled px; canvas px and frame slot positions are not.
		float x, y, width, height;
		m_wHomeDragLayer.GetScreenPos(x, y);
		m_fLayerX = workspace.DPIUnscale(x);
		m_fLayerY = workspace.DPIUnscale(y);

		m_wHomeGrid.GetScreenPos(x, y);
		m_wHomeGrid.GetScreenSize(width, height);
		m_fGridX = workspace.DPIUnscale(x) - m_fLayerX;
		m_fGridY = workspace.DPIUnscale(y) - m_fLayerY;
		m_fGridWidth = workspace.DPIUnscale(width);
		m_fGridHeight = workspace.DPIUnscale(height);
		if (m_fGridWidth <= 0 || m_fGridHeight <= 0)
			return false;

		//! Only a press made in edit mode can be a tap; the hold that enters it never is.
		m_bEditTap = m_bEditingHome;
		m_fPressX = canvasX;
		m_fPressY = canvasY;

		EnterHomeEdit();
		CloseWidgetSheet();

		m_DragItem = item;

		m_wDropHint = CreateFloatingItem(workspace, item);
		if (m_wDropHint)
		{
			m_wDropHint.SetOpacity(ELIFE_PhoneStyle.DROP_HINT_OPACITY);
			m_wDropHint.SetVisible(false);
		}

		m_wDragGhost = CreateFloatingItem(workspace, item);
		if (!m_wDragGhost)
		{
			ClearHomeDrag();
			return false;
		}

		//! Above the hint and the items easing aside, which are created after it.
		m_wDragGhost.SetZOrder(ELIFE_PhoneStyle.ZORDER_LIFTED);
		ShowPressed(m_wDragGhost);

		float left, top;
		CellPos(item.m_iColumn, item.m_iRow, left, top);
		m_fDragOffsetX = left - (canvasX - m_fLayerX);
		m_fDragOffsetY = top - (canvasY - m_fLayerY);
		FrameSlot.SetPos(m_wDragGhost, left, top);

		//! Opacity rather than visibility, so its cell keeps the gap.
		source.SetOpacity(0);

		//! The edit bar makes way for the remove target while something removable is held.
		if (m_wHomeEditBar)
			m_wHomeEditBar.SetVisible(false);

		ShowRemoveZone(IsRemovable(item));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowRemoveZone(bool show)
	{
		m_bOverRemoveZone = false;
		PaintRemoveZone();

		if (m_wHomeRemoveZone)
			m_wHomeRemoveZone.SetVisible(show);
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintRemoveZone()
	{
		float alpha = 0;
		if (m_bOverRemoveZone)
			alpha = ELIFE_PhoneStyle.REMOVE_ZONE_WASH_ALPHA;

		if (m_wHomeRemoveWash)
			m_wHomeRemoveWash.SetColor(ELIFE_PhoneStyle.WithAlpha(ELIFE_PhoneStyle.Negative(), alpha));
	}

	//------------------------------------------------------------------------------------------------
	//! The cursor, not the copy, decides: the copy is wider than the target when it is a card.
	protected bool IsOverRemoveZone(float canvasX, float canvasY)
	{
		if (!m_wHomeRemoveZone || !m_wHomeRemoveZone.IsVisible())
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		float x, y, width, height;
		m_wHomeRemoveZone.GetScreenPos(x, y);
		m_wHomeRemoveZone.GetScreenSize(width, height);

		float pointX = workspace.DPIScale(canvasX);
		float pointY = workspace.DPIScale(canvasY);
		return pointX >= x && pointX <= x + width && pointY >= y && pointY <= y + height;
	}

	//------------------------------------------------------------------------------------------------
	//! A copy of item in the drag layer, sized to its cells.
	protected Widget CreateFloatingItem(notnull WorkspaceWidget workspace, notnull ELIFE_PhoneHomeItem item)
	{
		Widget cell = workspace.CreateWidgets(LAYOUT_HOME_CELL, m_wHomeDragLayer);
		if (!cell)
			return null;

		FrameSlot.SetAnchor(cell, 0, 0);
		FrameSlot.SetAlignment(cell, 0, 0);
		FrameSlot.SetSize(cell, item.m_iWidth * CellWidth(), item.m_iHeight * CellHeight());

		Widget content = CreateItemContent(workspace, cell, item, false);
		if (!content)
		{
			cell.RemoveFromHierarchy();
			return null;
		}

		FillCard(content, item);

		//! Never clicked, and a hover wash on it would only flicker under the cursor.
		Widget button = content.FindAnyWidget(ItemButtonName(item));
		if (button)
			button.SetFlags(WidgetFlags.IGNORE_CURSOR);

		return cell;
	}

	//------------------------------------------------------------------------------------------------
	//! Pressed glass: the button's own pressed wash, held on while the item is lifted.
	protected void ShowPressed(notnull Widget floating)
	{
		Widget background = floating.FindAnyWidget("Background");
		if (background)
			background.SetColor(ELIFE_PhoneStyle.PressedWash());
	}

	//------------------------------------------------------------------------------------------------
	void UpdateHomeDrag(float canvasX, float canvasY)
	{
		if (!m_DragItem || !m_wDragGhost)
			return;

		if (m_bEditTap && (Math.AbsFloat(canvasX - m_fPressX) > EDIT_TAP_SLOP_PX || Math.AbsFloat(canvasY - m_fPressY) > EDIT_TAP_SLOP_PX))
			m_bEditTap = false;

		float left = canvasX - m_fLayerX + m_fDragOffsetX;
		float top = canvasY - m_fLayerY + m_fDragOffsetY;
		FrameSlot.SetPos(m_wDragGhost, left, top);

		bool overRemove = IsOverRemoveZone(canvasX, canvasY);
		if (overRemove != m_bOverRemoveZone)
		{
			m_bOverRemoveZone = overRemove;
			PaintRemoveZone();
		}

		int column, row;
		array<ELIFE_PhoneHomeItem> displaced = {};
		bool valid = !overRemove && FindDropCell(left, top, column, row, displaced);

		if (m_wDropHint)
		{
			m_wDropHint.SetVisible(valid);
			if (valid)
			{
				float hintLeft, hintTop;
				CellPos(column, row, hintLeft, hintTop);
				FrameSlot.SetPos(m_wDropHint, hintLeft, hintTop);
			}
		}

		UpdateShifts(displaced, column, row);
		AdvanceShifts();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the item on the cell it is over, swapping whatever is there into the cells it left, or
	//! removes it when dropped on the remove target. Anywhere else it goes back where it was.
	void EndHomeDrag(float canvasX, float canvasY)
	{
		if (!m_DragItem)
			return;

		//! A tap on a widget that comes in more than one size switches it to the next.
		if (m_bEditTap)
		{
			ELIFE_PhoneHomeItem tapped = m_DragItem;
			ClearHomeDrag();

			if (m_HomeLayout.Resize(tapped))
			{
				SaveHomeLayout();
				QueueHomeRebuild();
			}

			return;
		}

		if (IsOverRemoveZone(canvasX, canvasY))
		{
			ELIFE_PhoneHomeItem removed = m_DragItem;
			ClearHomeDrag();

			//! Hidden until the rebuild takes it off, or dimmed while its uninstall is out.
			Widget cell;
			if (m_mHomeCells.Find(removed.m_sId, cell))
				cell.SetOpacity(0);

			RemoveHomeItem(removed);

			if (m_mHomeCells.Find(removed.m_sId, cell) && !removed.m_bCard)
				RestCell(removed.m_sId, cell);

			return;
		}

		int column, row;
		array<ELIFE_PhoneHomeItem> displaced = {};
		bool valid = FindDropCell(canvasX - m_fLayerX + m_fDragOffsetX, canvasY - m_fLayerY + m_fDragOffsetY, column, row, displaced);
		if (!valid || !m_HomeLayout.Move(m_DragItem, column, row))
		{
			ClearHomeDrag();
			return;
		}

		SaveHomeLayout();

		//! Everything rests where it lands until the rebuild, which is deferred as this runs inside the
		//! held item's own handler, which the rebuild deletes.
		float left, top;
		CellPos(column, row, left, top);
		FrameSlot.SetPos(m_wDragGhost, left, top);

		if (m_wDropHint)
			m_wDropHint.SetVisible(false);

		foreach (ELIFE_PhoneHomeShift shift : m_aShifts)
		{
			if (shift.m_wCopy)
				FrameSlot.SetPos(shift.m_wCopy, shift.m_fToX, shift.m_fToY);
		}

		m_DragItem = null;
		m_bGridStale = false;
		ShowRemoveZone(false);
		QueueHomeRebuild();
	}

	//------------------------------------------------------------------------------------------------
	void CancelHomeDrag()
	{
		ClearHomeDrag();
	}

	//------------------------------------------------------------------------------------------------
	//! The cell under the dragged copy's top-left, if its centre is over the grid and it can move there.
	protected bool FindDropCell(float left, float top, out int column, out int row, notnull array<ELIFE_PhoneHomeItem> displaced)
	{
		float cellWidth = CellWidth();
		float cellHeight = CellHeight();

		float centreX = left + m_DragItem.m_iWidth * cellWidth * 0.5;
		float centreY = top + m_DragItem.m_iHeight * cellHeight * 0.5;
		if (centreX < m_fGridX || centreX > m_fGridX + m_fGridWidth || centreY < m_fGridY || centreY > m_fGridY + m_fGridHeight)
			return false;

		column = (int)Math.Round((left - m_fGridX) / cellWidth);
		row = (int)Math.Round((top - m_fGridY) / cellHeight);
		m_HomeLayout.ClampToGrid(m_DragItem, column, row);
		return m_HomeLayout.GetSwap(m_DragItem, column, row, displaced);
	}

	//------------------------------------------------------------------------------------------------
	//! Eases what the drop would swap into the cells the dragged item left, and sends back what no
	//! longer would.
	protected void UpdateShifts(notnull array<ELIFE_PhoneHomeItem> displaced, int column, int row)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		int now = System.GetTickCount();

		foreach (ELIFE_PhoneHomeShift shift : m_aShifts)
		{
			if (shift.m_bReturning || displaced.Find(shift.m_Item) != -1)
				continue;

			float homeX, homeY;
			CellPos(shift.m_Item.m_iColumn, shift.m_Item.m_iRow, homeX, homeY);
			RetargetShift(shift, homeX, homeY, now);
			shift.m_bReturning = true;
		}

		foreach (ELIFE_PhoneHomeItem other : displaced)
		{
			float toX, toY;
			CellPos(other.m_iColumn + m_DragItem.m_iColumn - column, other.m_iRow + m_DragItem.m_iRow - row, toX, toY);

			ELIFE_PhoneHomeShift existing = FindShift(other);
			if (!existing)
				existing = StartShift(workspace, other);

			if (!existing)
				continue;

			if (existing.m_bReturning || existing.m_fToX != toX || existing.m_fToY != toY)
			{
				RetargetShift(existing, toX, toY, now);
				existing.m_bReturning = false;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_PhoneHomeShift FindShift(ELIFE_PhoneHomeItem item)
	{
		foreach (ELIFE_PhoneHomeShift shift : m_aShifts)
		{
			if (shift.m_Item == item)
				return shift;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_PhoneHomeShift StartShift(notnull WorkspaceWidget workspace, notnull ELIFE_PhoneHomeItem item)
	{
		Widget source;
		if (!m_mHomeCells.Find(item.m_sId, source))
			return null;

		Widget copy = CreateFloatingItem(workspace, item);
		if (!copy)
			return null;

		ELIFE_PhoneHomeShift shift = new ELIFE_PhoneHomeShift();
		shift.m_Item = item;
		shift.m_wCopy = copy;
		CellPos(item.m_iColumn, item.m_iRow, shift.m_fFromX, shift.m_fFromY);
		shift.m_fToX = shift.m_fFromX;
		shift.m_fToY = shift.m_fFromY;
		shift.m_iStartMs = System.GetTickCount();
		FrameSlot.SetPos(copy, shift.m_fFromX, shift.m_fFromY);

		source.SetOpacity(0);
		m_aShifts.Insert(shift);
		return shift;
	}

	//------------------------------------------------------------------------------------------------
	//! Restarts the ease from wherever the copy is now, so a change of mind never jumps.
	protected void RetargetShift(notnull ELIFE_PhoneHomeShift shift, float toX, float toY, int now)
	{
		float x, y;
		ShiftPos(shift, now, x, y);
		shift.m_fFromX = x;
		shift.m_fFromY = y;
		shift.m_fToX = toX;
		shift.m_fToY = toY;
		shift.m_iStartMs = now;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShiftPos(notnull ELIFE_PhoneHomeShift shift, int now, out float x, out float y)
	{
		float eased = ELIFE_PhoneStyle.EaseOut((now - shift.m_iStartMs) / (float)ELIFE_PhoneStyle.DURATION_STATE_MS);
		x = shift.m_fFromX + (shift.m_fToX - shift.m_fFromX) * eased;
		y = shift.m_fFromY + (shift.m_fToY - shift.m_fFromY) * eased;
	}

	//------------------------------------------------------------------------------------------------
	//! Moves the copies along; one back home is swapped for its original.
	protected void AdvanceShifts()
	{
		int now = System.GetTickCount();

		for (int i = m_aShifts.Count() - 1; i >= 0; i--)
		{
			ELIFE_PhoneHomeShift shift = m_aShifts[i];
			float x, y;
			ShiftPos(shift, now, x, y);

			if (shift.m_wCopy)
				FrameSlot.SetPos(shift.m_wCopy, x, y);

			if (!shift.m_bReturning || now - shift.m_iStartMs < ELIFE_PhoneStyle.DURATION_STATE_MS)
				continue;

			Widget source;
			if (m_mHomeCells.Find(shift.m_Item.m_sId, source))
				RestCell(shift.m_Item.m_sId, source);

			if (shift.m_wCopy)
				shift.m_wCopy.RemoveFromHierarchy();

			m_aShifts.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected float CellWidth()
	{
		return m_fGridWidth / ELIFE_PhoneHomeLayout.COLUMNS;
	}

	//------------------------------------------------------------------------------------------------
	protected float CellHeight()
	{
		return m_fGridHeight / ELIFE_PhoneHomeLayout.ROWS;
	}

	//------------------------------------------------------------------------------------------------
	//! Drag-layer px of a cell's top-left.
	protected void CellPos(int column, int row, out float left, out float top)
	{
		left = m_fGridX + column * CellWidth();
		top = m_fGridY + row * CellHeight();
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a drag that didn't land: everything snaps back to its own cell.
	protected void ClearHomeDrag()
	{
		ClearFloating();
		m_DragItem = null;
		ShowRemoveZone(false);

		if (m_wHomeEditBar)
			m_wHomeEditBar.SetVisible(m_bEditingHome);

		if (!m_bGridStale)
			return;

		m_bGridStale = false;
		QueueHomeRebuild();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every drag-layer copy and shows every cell again.
	protected void ClearFloating()
	{
		if (m_wDragGhost)
			m_wDragGhost.RemoveFromHierarchy();

		if (m_wDropHint)
			m_wDropHint.RemoveFromHierarchy();

		foreach (ELIFE_PhoneHomeShift shift : m_aShifts)
		{
			if (shift.m_wCopy)
				shift.m_wCopy.RemoveFromHierarchy();
		}

		m_aShifts.Clear();
		m_wDragGhost = null;
		m_wDropHint = null;

		foreach (string itemId, Widget cell : m_mHomeCells)
		{
			if (cell)
				RestCell(itemId, cell);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Loads the app's sprite, or falls back to its character mark centred in the full box and sized by
	//! cap height (a text widget centres its line box, so a sprite-sized box would clip it).
	static void PaintAppIcon(notnull WorkspaceWidget workspace, Widget iconHost, TextWidget mark, ELIFE_PhoneAppEntry app, float ink)
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
		//! During setup the pill only advances the tour.
		ELIFE_PhoneSetupApp setup = ELIFE_PhoneSetupApp.Cast(m_App);
		if (setup)
		{
			setup.OnChromeTapped(false);
			return;
		}

		//! Home closes the widget picker, then leaves edit mode, as it would on any phone.
		if (m_wWidgetSheet)
		{
			CloseWidgetSheet();
			return;
		}

		if (m_bEditingHome)
		{
			ExitHomeEdit();
			return;
		}

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

		if (m_wStatusDndSize)
			m_wStatusDndSize.SetVisible(m_Phone && m_Phone.IsDoNotDisturb());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPersonalizationChanged()
	{
		ApplyWallpaper();
		RefreshSwatchChecks();
		RefreshStatusBar();
		FillHomeCards();
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyWallpaper()
	{
		if (!m_wRoot || !m_Phone)
			return;

		ELIFE_PhoneWallpaper wallpaper = ELIFE_PhoneWallpapers.Resolve(m_Phone.GetWallpaper());
		if (wallpaper)
			PaintWallpaper(m_wRoot, "Wallpaper", wallpaper, wallpaper.m_sPhoto);
	}

	//------------------------------------------------------------------------------------------------
	//! Paints prefix + Base, Glow, PhotoGroup and Photo, so a swatch looks like the screen it sets.
	protected void PaintWallpaper(notnull Widget root, string prefix, notnull ELIFE_PhoneWallpaper wallpaper, ResourceName photoTexture)
	{
		ELIFE_PhoneStyle.SetColorOf(root, prefix + "Base", wallpaper.m_Base);

		bool photo = false;
		ImageWidget image = ImageWidget.Cast(root.FindAnyWidget(prefix + "Photo"));
		if (image && photoTexture != ResourceName.Empty && image.LoadImageTexture(0, photoTexture))
		{
			//! Scale to fill crops by the image's own size, so it must carry the texture's aspect.
			int width, height;
			image.GetImageSize(0, width, height);
			if (width > 0 && height > 0)
				image.SetSize(width, height);

			photo = true;
		}

		Widget photoGroup = root.FindAnyWidget(prefix + "PhotoGroup");
		if (photoGroup)
			photoGroup.SetVisible(photo);

		ImageWidget glow = ImageWidget.Cast(root.FindAnyWidget(prefix + "Glow"));
		if (!glow)
			return;

		glow.SetVisible(!photo);
		glow.SetColor(wallpaper.m_Glow);

		if (ELIFE_PhoneStyle.GRADIENT_FLIPPED)
			glow.SetRotation(180);
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
		if (key == ELIFE_PhoneGadgetComponent.DATA_APPS)
		{
			BuildHomeGrid();

			//! The messages card goes with its app.
			if (m_eState == EPhoneScreenState.HOME)
				RefreshHomeScreen();

			return;
		}

		if (key == ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS)
		{
			if (m_eState == EPhoneScreenState.HOME)
				RefreshHomeScreen();

			return;
		}

		bool notifications = key == ELIFE_PhoneGadgetComponent.DATA_NOTIFICATIONS;
		if (!notifications && key != ELIFE_PhoneGadgetComponent.DATA_MESSAGES && key != ELIFE_PhoneGadgetComponent.DATA_CONTACTS)
			return;

		if (m_eState == EPhoneScreenState.LOCKED)
			RefreshLockScreen();
		else if (m_eState == EPhoneScreenState.HOME)
			RefreshHomeScreen();

		if (key == ELIFE_PhoneGadgetComponent.DATA_CONTACTS)
			return;

		if (notifications)
			RaiseArrivedBanners();

		//! After the banners, so a new arrival also lands in the list; the inbox carries the card text.
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
	//! Banners whichever threads got a notification this screen hasn't accounted for yet.
	void RaiseArrivedBanners()
	{
		if (!m_Phone)
			return;

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_NOTIFICATIONS);
		if (json == "")
			return;

		ELIFE_PhoneNotificationListDto queue = new ELIFE_PhoneNotificationListDto();
		queue.ExpandFromRAW(json);

		//! Rebuilt from the queue each time, so acked ids drop out instead of piling up.
		map<string, bool> seen = new map<string, bool>();
		array<string> arrivedThreads = {};

		foreach (ELIFE_PhoneNotification notification : queue.items)
		{
			if (!notification || !notification.IsMessage())
				continue;

			seen.Set(notification.notificationId, true);

			if (m_bNotificationsSeeded && !m_mSeenNotifications.Contains(notification.notificationId) && !arrivedThreads.Contains(notification.groupKey))
				arrivedThreads.Insert(notification.groupKey);
		}

		m_mSeenNotifications = seen;
		m_bNotificationsSeeded = true;

		if (arrivedThreads.IsEmpty())
			return;

		array<ref ELIFE_MessageThreadUpdate> pending = {};
		CollectNotifications(pending);

		foreach (ELIFE_MessageThreadUpdate threadDto : pending)
		{
			if (arrivedThreads.Contains(threadDto.threadId))
				ShowBanner(threadDto);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Peek: banner what's already waiting, then seed so later polls only raise new ones.
	void RaisePendingBanners()
	{
		array<ref ELIFE_MessageThreadUpdate> pending = {};
		CollectNotifications(pending);

		foreach (ELIFE_MessageThreadUpdate threadDto : pending)
			ShowBanner(threadDto);

		m_bNotificationsSeeded = false;
		RaiseArrivedBanners();
	}

	//------------------------------------------------------------------------------------------------
	//! Awake screens only - lock has its own list, and setup has nowhere for a banner to lead.
	//! Bystander copies show banners too, just not clickable.
	protected bool CanShowBanner()
	{
		return m_eState != EPhoneScreenState.OFF && m_eState != EPhoneScreenState.LOCKED && m_eState != EPhoneScreenState.SETUP;
	}

	//------------------------------------------------------------------------------------------------
	//! The open hub already lists it.
	protected bool CanRaiseBanner()
	{
		return CanShowBanner() && m_Phone && !m_Phone.IsHubOpen() && !m_Phone.IsDoNotDisturb();
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
	protected void ShowBanner(notnull ELIFE_MessageThreadUpdate threadDto)
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
	//! One entry per thread with queued Messages notifications, newest first. unreadCount is how many are
	//! queued and lastMessageAt the newest one's time; title and text come from the inbox, which has been
	//! through the profanity filter.
	protected void CollectNotifications(notnull array<ref ELIFE_MessageThreadUpdate> outThreads)
	{
		if (!m_Phone)
			return;

		string queueJson = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_NOTIFICATIONS);
		string inboxJson = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (queueJson == "" || inboxJson == "")
			return;

		ELIFE_PhoneNotificationListDto queue = new ELIFE_PhoneNotificationListDto();
		queue.ExpandFromRAW(queueJson);

		ELIFE_MessageUpdates inbox = new ELIFE_MessageUpdates();
		inbox.ExpandFromRAW(inboxJson);

		foreach (ELIFE_PhoneNotification notification : queue.items)
		{
			if (!notification || !notification.IsMessage() || IsThreadOnScreen(notification.groupKey))
				continue;

			ELIFE_MessageThreadUpdate entry = FindNotified(outThreads, notification.groupKey);
			if (!entry)
			{
				ELIFE_MessageThreadUpdate threadDto = inbox.FindThread(notification.groupKey);
				if (!threadDto)
					continue;

				entry = threadDto.Copy();
				entry.KeepNewestInbound();
				entry.unreadCount = 0;
				entry.lastMessageAt = "";
				outThreads.Insert(entry);
			}

			entry.unreadCount++;
			if (entry.lastMessageAt < notification.occurredAt)
				entry.lastMessageAt = notification.occurredAt;
		}

		//! Newest first; the queue itself is oldest first.
		int count = outThreads.Count();
		for (int i = 1; i < count; i++)
		{
			ELIFE_MessageThreadUpdate current = outThreads.Get(i);
			int j = i - 1;

			while (j >= 0 && outThreads.Get(j).lastMessageAt < current.lastMessageAt)
			{
				outThreads.Set(j + 1, outThreads.Get(j));
				j--;
			}

			outThreads.Set(j + 1, current);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_MessageThreadUpdate FindNotified(notnull array<ref ELIFE_MessageThreadUpdate> threads, string threadId)
	{
		foreach (ELIFE_MessageThreadUpdate threadDto : threads)
		{
			if (threadDto.threadId == threadId)
				return threadDto;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Newest outstanding notification - the peek's door when the phone is drawn.
	string GetNewestNotificationThreadId()
	{
		array<ref ELIFE_MessageThreadUpdate> pending = {};
		CollectNotifications(pending);

		if (pending.IsEmpty() || !pending[0])
			return "";

		return pending[0].threadId;
	}

	//------------------------------------------------------------------------------------------------
	//! Indicator and hub read one list, so they can't disagree.
	protected void RefreshNotifications()
	{
		array<ref ELIFE_MessageThreadUpdate> pending = {};
		CollectNotifications(pending);

		//! Counts messages, not threads.
		int unread = 0;
		foreach (ELIFE_MessageThreadUpdate threadDto : pending)
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
	protected void FillHub(notnull array<ref ELIFE_MessageThreadUpdate> pending)
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
			ELIFE_MessageThreadUpdate threadDto = pending.Get(i);

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
		ELIFE_PhoneSetupApp setup = ELIFE_PhoneSetupApp.Cast(m_App);
		if (setup)
		{
			setup.OnChromeTapped(true);
			return;
		}

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
	//! Acks the queue without marking anything read; the next message on a thread notifies again.
	protected void OnHubClearClicked()
	{
		if (m_Phone)
			m_Phone.ClearNotifications();
	}

	//------------------------------------------------------------------------------------------------
	//! Paints a card for the lock list, banners and hub. compact scales tokens for the 1x card.
	protected void PaintNotificationCard(notnull Widget card, notnull ELIFE_MessageThreadUpdate threadDto, bool showCount = false, bool compact = false)
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

		if (state != EPhoneScreenState.HOME)
			ExitHomeEdit();

		//! Setup is an AppHost page that isn't in the catalog, so it never lands on the home grid.
		bool setup = state == EPhoneScreenState.SETUP;
		bool appOpen = ELIFE_PhoneAppCatalog.HasPage(state) || setup;
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

		//! Wallpaper is for lock and home; app pages get the flat ground. Setup brings its own backdrop.
		if (m_wWallpaper)
			m_wWallpaper.SetVisible(!appOpen && !off);

		if (m_wAppWashSize)
			m_wAppWashSize.SetVisible(false);

		if (m_wNavBarSize)
			m_wNavBarSize.SetVisible(appOpen);

		//! Locked: no home pill, the PIN pad is the only way in. Setup has no home to go to yet.
		if (m_wHomeIndicator)
			m_wHomeIndicator.SetVisible(!locked && !off && !setup);

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

		//! The visibilities above just undid a tour in progress if setup was already open.
		ELIFE_PhoneSetupApp setupApp = ELIFE_PhoneSetupApp.Cast(m_App);
		if (setupApp)
			setupApp.RefreshTour();

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
		{
			m_wBackLabel.SetText(BACK_LABEL);
			m_wBackLabel.SetColor(accent);
		}

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
	//! For a page where leading means Cancel rather than Back. Empty restores Back.
	void SetBackLabel(string label)
	{
		if (!m_wBackLabel)
			return;

		if (label == "")
			label = BACK_LABEL;

		m_wBackLabel.SetText(label);
		SizeBackChip();
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
	//! Any later ShowState() resets these visibilities.
	void ShowSetupTour(bool showHome, bool showHomePill)
	{
		if (!ELIFE_PhoneSetupApp.Cast(m_App))
			return;

		if (m_wHomeScreen)
			m_wHomeScreen.SetVisible(showHome);

		if (m_wWallpaper)
			m_wWallpaper.SetVisible(showHome);

		if (m_wHomeIndicator)
			m_wHomeIndicator.SetVisible(showHomePill);

		if (showHome)
			RefreshHomeScreen();
	}

	//------------------------------------------------------------------------------------------------
	//! The first app tile on the home grid, in DPI-unscaled screen px. False until the grid is laid out.
	bool GetHomeTileRect(out float x, out float y, out float width, out float height)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		array<ref ELIFE_PhoneAppEntry> catalog = ELIFE_PhoneAppCatalog.GetEntries();
		foreach (ELIFE_PhoneAppEntry app : catalog)
		{
			Widget cell;
			if (!m_mHomeCells.Find(app.m_sAppKey, cell) || !cell)
				continue;

			cell.GetScreenPos(x, y);
			cell.GetScreenSize(width, height);
			if (width <= 0 || height <= 0)
				return false;

			x = workspace.DPIUnscale(x);
			y = workspace.DPIUnscale(y);
			width = workspace.DPIUnscale(width);
			height = workspace.DPIUnscale(height);
			return true;
		}

		return false;
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

	//! Picks up widgets coming and going, and refreshes what they show.
	protected void RefreshHomeScreen()
	{
		BuildHomeGrid();
		FillHomeCards();
	}

	//------------------------------------------------------------------------------------------------
	protected void FillHomeCards(bool liveOnly = false)
	{
		if (!m_HomeLayout)
			return;

		foreach (ELIFE_PhoneHomeItem item : m_HomeLayout.GetItems())
		{
			if (liveOnly && item.m_sId != ELIFE_PhoneHomeLayout.WEATHER_ID && item.m_sId != ELIFE_PhoneHomeLayout.LOCATION_ID)
				continue;

			Widget cell;
			if (item.m_bCard && m_mHomeCells.Find(item.m_sId, cell) && cell.GetChildren())
				FillCard(cell.GetChildren(), item, m_bInteractive);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void RefreshLiveWidgets()
	{
		if (m_eState == EPhoneScreenState.HOME)
			FillHomeCards(true);
	}

	//------------------------------------------------------------------------------------------------
	//! A widget's live content. Tiles have none. Only the grid's own copy is clickable.
	protected void FillCard(notnull Widget card, notnull ELIFE_PhoneHomeItem item, bool clickable = false)
	{
		if (!item.m_bCard)
			return;

		bool small = item.m_iWidth < ELIFE_PhoneHomeLayout.FULL;

		if (item.m_sId == ELIFE_PhoneHomeLayout.CLOCK_ID)
			FillClock(card);
		else if (item.m_sId == ELIFE_PhoneHomeLayout.WEATHER_ID)
			FillWeather(card);
		else if (item.m_sId == ELIFE_PhoneHomeLayout.LOCATION_ID)
			FillLocation(card);
		else if (item.m_sId == ELIFE_PhoneHomeLayout.FAVOURITES_ID)
			FillFavourites(card, clickable);
		else if (item.m_eState == EPhoneScreenState.BANK)
			FillBalanceCard(card);
		else if (item.m_eState == EPhoneScreenState.MESSAGES)
			FillMessagesCard(card, small);
	}

	//------------------------------------------------------------------------------------------------
	//! The current weather in the game's own icon and name, over the time of day as the game names it.
	protected void FillWeather(notnull Widget card)
	{
		string label = "#ELIFE-Phone_Home_Weather";
		string value = "";
		ResourceName icon = ResourceName.Empty;

		ChimeraWorld world = ChimeraWorld.CastFrom(GetGame().GetWorld());
		TimeAndWeatherManagerEntity weather;
		if (world)
			weather = world.GetTimeAndWeatherManager();

		if (weather)
		{
			SCR_UIInfo dayTime;
			weather.GetCurrentDayTimeUIInfo(dayTime);
			if (dayTime && dayTime.GetName() != "")
				label = dayTime.GetName();

			WeatherState state = weather.GetCurrentWeatherState();
			if (state)
			{
				value = state.GetLocalizedName();
				icon = state.GetIconPath();
			}
		}

		FillInfo(card, value, label, ELIFE_PhoneStyle.TextPrimary());

		ImageWidget image = ImageWidget.Cast(card.FindAnyWidget("InfoIcon"));
		if (!image)
			return;

		bool loaded = icon != ResourceName.Empty && image.LoadImageTexture(0, icon);
		SetVisibleOf(card, "InfoIconSize", loaded);
		if (loaded)
		{
			image.SetColor(ELIFE_PhoneStyle.TextPrimary());
			ELIFE_PhoneStyle.FitIcon(image, INFO_ICON_SIZE);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FillInfo(notnull Widget card, string value, string label, Color valueColor)
	{
		SetText(card, "InfoValue", value);
		ELIFE_PhoneStyle.SetColorOf(card, "InfoValue", valueColor);
		SetText(card, "InfoLabel", label);
		ELIFE_PhoneStyle.SetColorOf(card, "InfoLabel", ELIFE_PhoneStyle.TextSecondary());
	}

	//------------------------------------------------------------------------------------------------
	protected void SetVisibleOf(notnull Widget parent, string widgetName, bool visible)
	{
		Widget widget = parent.FindAnyWidget(widgetName);
		if (widget)
			widget.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	//! The map grid the phone is in - the phone's, so a bystander reads the same square.
	protected void FillLocation(notnull Widget card)
	{
		string grid = "";
		if (m_Phone && m_Phone.GetOwner())
			grid = SCR_MapEntity.GetGridLabel(m_Phone.GetOwner().GetOrigin());

		FillInfo(card, grid, "#ELIFE-Phone_Home_Location", ELIFE_PhoneStyle.TextPrimary());

		ImageWidget image = ImageWidget.Cast(card.FindAnyWidget("InfoIcon"));
		if (!image)
			return;

		bool loaded = image.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_LOCATION);
		SetVisibleOf(card, "InfoIconSize", loaded);
		if (loaded)
		{
			image.SetColor(ELIFE_PhoneStyle.AccentMap());
			ELIFE_PhoneStyle.FitIcon(image, INFO_ICON_SIZE);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Up to FAVOURITES_SHOWN starred contacts, each a door to its contact page. Ids the contact book
	//! no longer knows (deleted contacts) are skipped.
	protected void FillFavourites(notnull Widget card, bool clickable)
	{
		Widget list = card.FindAnyWidget("FavouriteList");
		if (!list || !m_Phone)
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		if (clickable)
			m_aFavouriteClicks.Clear();

		ClearChildren(list);

		array<string> favourites = {};
		m_Phone.GetFavourites(favourites);

		int shown = 0;
		foreach (string contactId : favourites)
		{
			if (shown >= FAVOURITES_SHOWN)
				break;

			ELIFE_ContactSummary contact = ELIFE_PhoneContactBook.FindById(m_Phone, contactId);
			if (!contact)
				continue;

			Widget entry = workspace.CreateWidgets(LAYOUT_FAVOURITE_ENTRY, list);
			if (!entry)
				continue;

			LayoutSlot.SetSizeMode(entry, LayoutSizeMode.Fill);
			AlignableSlot.SetVerticalAlign(entry, LayoutVerticalAlign.Center);

			string name = ELIFE_PhoneContactBook.DisplayName(contact);
			ELIFE_PhoneAppBase.PaintAvatarInto(entry, "FavouriteAvatarDisc", "FavouriteAvatarFallback", "FavouriteAvatarGlyph", name, ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.CONTACTS));
			SetText(entry, "FavouriteName", FirstWord(name));
			ELIFE_PhoneStyle.SetColorOf(entry, "FavouriteName", ELIFE_PhoneStyle.TextPrimary());

			Widget button = entry.FindAnyWidget("FavouriteButton");
			if (button && clickable)
			{
				//! Opens the contact rather than a chat, so calling is a tap away too. Also carries the widget's drag.
				ELIFE_PhoneTileClick click = new ELIFE_PhoneTileClick();
				click.Bind(this, EPhoneScreenState.CONTACTS, ELIFE_PhoneHomeLayout.FAVOURITES_ID, true, ELIFE_PhoneContactsApp.SUBSTATE_DETAIL_PREFIX + contact.contactId);
				button.AddHandler(click);
				m_aFavouriteClicks.Insert(click);
			}
			else if (button)
			{
				button.SetFlags(WidgetFlags.IGNORE_CURSOR);
			}

			shown++;
		}

		//! Fewer than a full row keep their column width rather than stretching.
		for (int i = shown; i > 0 && i < FAVOURITES_SHOWN; i++)
		{
			Widget spacer = workspace.CreateWidgets(LAYOUT_GRID_ROW, list);
			if (spacer)
				LayoutSlot.SetSizeMode(spacer, LayoutSizeMode.Fill);
		}

		Widget empty = card.FindAnyWidget("FavouriteEmpty");
		if (empty)
			empty.SetVisible(shown == 0);
	}

	//------------------------------------------------------------------------------------------------
	protected string FirstWord(string name)
	{
		name.TrimInPlace();
		int space = name.IndexOf(" ");
		if (space <= 0)
			return name;

		return name.Substring(0, space);
	}

	//------------------------------------------------------------------------------------------------
	//! The time keeps itself current (ELIFE_PhoneClockUIComponent); the date is set here.
	protected void FillClock(notnull Widget clock)
	{
		SetText(clock, "HomeDate", FormatToday());
	}

	//------------------------------------------------------------------------------------------------
	protected void FillBalanceCard(notnull Widget card)
	{
		if (!m_Phone)
			return;

		//! Before the accounts arrive the card just names its app; with none it says so.
		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_BANK_ACCOUNTS);
		ELIFE_BankAccountListDto accounts = new ELIFE_BankAccountListDto();
		if (json != "")
			accounts.ExpandFromRAW(json);

		if (accounts.items.IsEmpty())
		{
			SetText(card, "CardLabel", "#ELIFE-Phone_App_Bank");
			ELIFE_PhoneStyle.SetColorOf(card, "CardLabel", ELIFE_PhoneStyle.TextPrimary());

			string empty = "";
			if (json != "")
				empty = "#ELIFE-Phone_Home_NoAccount";

			SetText(card, "CardValue", empty);
			ELIFE_PhoneStyle.SetTypeOf(card, "CardValue", ELIFE_PhoneStyle.TEXT_BODY, false, ELIFE_PhoneStyle.TextPrimary());
			return;
		}

		ELIFE_BankAccountSummary account = accounts.items.Get(0);

		SetText(card, "CardLabel", account.number);
		ELIFE_PhoneStyle.SetColorOf(card, "CardLabel", ELIFE_PhoneStyle.TextPrimary());

		SetText(card, "CardValue", ELIFE_PhoneBankingService.FormatMoney(ELIFE_PhoneBankingService.ToCents(account.balance)));
		ELIFE_PhoneStyle.SetTypeOf(card, "CardValue", ELIFE_PhoneStyle.TEXT_TITLE, true, ELIFE_PhoneStyle.AccentBank());
	}

	//------------------------------------------------------------------------------------------------
	protected void FillMessagesCard(notnull Widget card, bool small = false)
	{
		if (!m_Phone)
			return;

		//! Half a row has room for how many, not who.
		if (small)
		{
			int unread = TotalUnread();
			string value = "#ELIFE-Phone_Home_NoUnread";
			if (unread > 0)
				value = WidgetManager.Translate("#ELIFE-Phone_Home_Unread", unread.ToString());

			SetText(card, "CardLabel", "#ELIFE-Phone_App_Messages");
			ELIFE_PhoneStyle.SetColorOf(card, "CardLabel", ELIFE_PhoneStyle.TextPrimary());
			SetText(card, "CardValue", value);
			ELIFE_PhoneStyle.SetTypeOf(card, "CardValue", ELIFE_PhoneStyle.TEXT_BODY, unread > 0, ELIFE_PhoneStyle.TextPrimary());
			return;
		}

		ELIFE_MessageThreadUpdate newest = NewestUnreadThread();
		if (!newest)
		{
			SetText(card, "CardLabel", "#ELIFE-Phone_App_Messages");
			ELIFE_PhoneStyle.SetColorOf(card, "CardLabel", ELIFE_PhoneStyle.TextPrimary());
			SetText(card, "CardValue", "#ELIFE-Phone_Home_NoUnread");
			ELIFE_PhoneStyle.SetTypeOf(card, "CardValue", ELIFE_PhoneStyle.TEXT_BODY, false, ELIFE_PhoneStyle.TextPrimary());
			SetText(card, "CardTrailing", "");
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
	protected int TotalUnread()
	{
		if (!m_Phone)
			return 0;

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json == "")
			return 0;

		ELIFE_MessageUpdates updates = new ELIFE_MessageUpdates();
		updates.ExpandFromRAW(json);

		int total = 0;
		foreach (ELIFE_MessageThreadUpdate threadDto : updates.threads)
		{
			if (threadDto && threadDto.unreadCount > 0)
				total += threadDto.unreadCount;
		}

		return total;
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_MessageThreadUpdate NewestUnreadThread()
	{
		if (!m_Phone)
			return null;

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json == "")
			return null;

		ELIFE_MessageUpdates updates = new ELIFE_MessageUpdates();
		updates.ExpandFromRAW(json);

		ELIFE_MessageThreadUpdate newest;
		foreach (ELIFE_MessageThreadUpdate threadDto : updates.threads)
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

		array<ref ELIFE_MessageThreadUpdate> pending = {};
		CollectNotifications(pending);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		int shown = 0;

		foreach (ELIFE_MessageThreadUpdate threadDto : pending)
		{
			if (shown >= LOCK_NOTIFICATION_LIMIT)
				break;

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
	protected string NewestBody(notnull ELIFE_MessageThreadUpdate threadDto)
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
		if (!m_bInteractive)
			return false;

		if (m_eState == EPhoneScreenState.LOCKED)
			return true;

		return m_App && m_App.AcceptsPinKeys();
	}

	//------------------------------------------------------------------------------------------------
	//! Keyboard entry, same path as the on-screen keys - the lock pad's or the open page's.
	void PinKeyDigit(int digit)
	{
		if (!AcceptsPinKeys())
			return;

		if (m_eState == EPhoneScreenState.LOCKED)
			PinPush(digit);
		else
			m_App.PinKeyDigit(digit);
	}

	//------------------------------------------------------------------------------------------------
	void PinKeyBackspace()
	{
		if (!AcceptsPinKeys())
			return;

		if (m_eState == EPhoneScreenState.LOCKED)
			PinBackspace();
		else
			m_App.PinKeyBackspace();
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
		GetGame().GetCallqueue().Remove(BuildHomeGrid);
		GetGame().GetCallqueue().Remove(DestroyWidgetSheet);
		GetGame().GetCallqueue().Remove(RefreshLiveWidgets);
	}
}
