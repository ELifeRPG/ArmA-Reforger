//------------------------------------------------------------------------------------------------
//! Mouse-wheel scrolling for a ScrollLayoutWidget, which the engine doesn't provide.
class ELIFE_PhoneWheelScroll : ScriptedWidgetEventHandler
{
	//! Pixels per wheel notch, roughly two message bubbles.
	protected const float STEP = 128;

	protected ScrollLayoutWidget m_wScroll;

	//------------------------------------------------------------------------------------------------
	void Bind(ScrollLayoutWidget scroll)
	{
		m_wScroll = scroll;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnMouseWheel(Widget w, int x, int y, int wheel)
	{
		if (!m_wScroll)
			return false;

		float posX, posY;
		m_wScroll.GetSliderPosPixels(posX, posY);

		//! Positive wheel scrolls up the page.
		m_wScroll.SetSliderPosPixels(posX, posY - wheel * STEP);

		//! Consumed so the wheel doesn't reach the menu behind.
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! Shell chrome lent to the open app.
class ELIFE_PhoneChrome
{
	Widget m_wStatusBarGlass;
	Widget m_wNavBarGlass;
	Widget m_wBackChipSize;
	TextWidget m_wNavTitle;
	Widget m_wNavActionSize;
	Widget m_wNavActionButton;
	Widget m_wNavActionChip;
	Widget m_wNavActionIconSize;
	ImageWidget m_wNavActionIcon;
	TextWidget m_wNavActionLabel;
	ref Color m_Accent;
}

//------------------------------------------------------------------------------------------------
//! Base for a phone app page, hosted by ELIFE_PhoneScreenShell.
class ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT_STATUS = "{9C6B03E5A187D24F}UI/layouts/Menus/Phone/Apps/PhoneAppStatus.layout";
	protected const ResourceName LAYOUT_SKELETON_ROW = "{7C22D4A9E3B25F87}UI/layouts/Menus/Phone/Apps/PhoneSkeletonRow.layout";

	protected const ResourceName LAYOUT_LIST_GROUP = "{7C28D4A9E3B25F8D}UI/layouts/Menus/Phone/Widgets/PhoneListGroup.layout";

	protected const float LIST_GROUP_GAP = 12;

	//! Scroll distance over which the large title collapses into the nav bar.
	protected const float COLLAPSE_DISTANCE = 44;
	protected const int COLLAPSE_TICK_MS = 33;

	//! Enough skeleton rows to fill the page.
	protected const int SKELETON_ROWS = 5;

	//! Circle sprite for person avatars.
	protected const string ICON_PERSON_MARK = "circle";

	protected ELIFE_PhoneGadgetComponent m_Phone;

	protected ELIFE_PhoneScreenShell m_Shell;

	protected Widget m_wRoot;
	protected Widget m_wStatusOverlay;
	protected Widget m_wSkeletonList;
	protected TextWidget m_wLargeTitle;

	protected Widget m_wStatusBarGlass;
	protected Widget m_wNavBarGlass;
	protected Widget m_wBackChipSize;
	protected TextWidget m_wNavTitle;
	protected ref Color m_Accent;

	//! Nav action pill padding and icon box. The pill hugs its content - see ShowNavAction().
	protected const float NAV_ACTION_PAD_H = 20;
	protected const float NAV_ACTION_ICON_SIZE = 18;
	protected const float NAV_ACTION_ICON_GAP = 6;
	protected const float NAV_ACTION_CHIP_H = 40;

	//! The nav bar's trailing action slot, claimed per app in OnOpened().
	protected Widget m_wNavActionSize;
	protected Widget m_wNavActionButton;
	protected Widget m_wNavActionChip;
	protected Widget m_wNavActionIconSize;
	protected ImageWidget m_wNavActionIcon;
	protected TextWidget m_wNavActionLabel;
	protected ScriptedWidgetEventHandler m_NavActionHandler;

	protected ScrollLayoutWidget m_TrackedScroll;

	//! Kept alive here; an unreferenced handler silently stops firing.
	protected ref array<ref ELIFE_PhoneWheelScroll> m_aWheelHandlers = {};
	protected ref array<ScrollLayoutWidget> m_aWheelBound = {};
	protected float m_fCollapse = -1;
	protected bool m_bSkeletonVisible;
	protected int m_iSkeletonShownAt;

	//------------------------------------------------------------------------------------------------
	//! Called by the shell before Open().
	void BindChrome(notnull ELIFE_PhoneChrome chrome)
	{
		m_wStatusBarGlass = chrome.m_wStatusBarGlass;
		m_wNavBarGlass = chrome.m_wNavBarGlass;
		m_wBackChipSize = chrome.m_wBackChipSize;
		m_wNavTitle = chrome.m_wNavTitle;
		m_wNavActionSize = chrome.m_wNavActionSize;
		m_wNavActionButton = chrome.m_wNavActionButton;
		m_wNavActionChip = chrome.m_wNavActionChip;
		m_wNavActionIconSize = chrome.m_wNavActionIconSize;
		m_wNavActionIcon = chrome.m_wNavActionIcon;
		m_wNavActionLabel = chrome.m_wNavActionLabel;
		m_Accent = chrome.m_Accent;

		//! Hidden until claimed, so the previous app's label doesn't linger for a frame.
		if (m_wNavActionButton)
			m_wNavActionButton.SetVisible(false);

		//! Light glass at rest; ApplyCollapse fades the pill once the nav bar has glass.
		if (m_wNavActionChip)
			ELIFE_PhoneStyle.ApplyGlass(m_wNavActionChip, false, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Claims the nav bar's trailing action. Calling again relabels it without stacking handlers.
	protected void ShowNavAction(string label, Color color, ScriptedWidgetEventHandler clickHandler, string iconSprite = "")
	{
		if (!m_wNavActionButton || !m_wNavActionLabel)
			return;

		if (m_NavActionHandler)
			m_wNavActionButton.RemoveHandler(m_NavActionHandler);

		m_NavActionHandler = clickHandler;
		if (m_NavActionHandler)
			m_wNavActionButton.AddHandler(m_NavActionHandler);

		SetNavActionLabel(label, color, iconSprite);

		if (m_wNavActionChip)
			m_wNavActionChip.SetVisible(true);

		m_wNavActionButton.SetVisible(true);
		m_wNavActionButton.SetEnabled(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Relabels without touching the enabled state, so "Saving…" doesn't re-enable the button.
	protected void SetNavActionLabel(string label, Color color, string iconSprite = "")
	{
		if (!m_wNavActionLabel)
			return;

		m_wNavActionLabel.SetText(label);
		m_wNavActionLabel.SetColor(color);

		bool iconLoaded = false;
		if (m_wNavActionIcon && iconSprite != "")
		{
			iconLoaded = m_wNavActionIcon.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, iconSprite);
			if (iconLoaded)
			{
				m_wNavActionIcon.SetColor(color);
				ELIFE_PhoneStyle.FitIcon(m_wNavActionIcon, NAV_ACTION_ICON_SIZE);
			}
		}

		if (m_wNavActionIcon)
			m_wNavActionIcon.SetVisible(iconLoaded);

		//! Collapses the icon slot when there's no icon.
		if (m_wNavActionIconSize)
		{
			m_wNavActionIconSize.SetVisible(iconLoaded);

			SizeLayoutWidget iconSize = SizeLayoutWidget.Cast(m_wNavActionIconSize);
			if (iconSize)
			{
				if (iconLoaded)
					iconSize.SetWidthOverride(NAV_ACTION_ICON_SIZE);
				else
					iconSize.SetWidthOverride(0);
			}
		}

		if (m_wNavActionSize)
		{
			float textWidth, textHeight;
			m_wNavActionLabel.GetTextSize(textWidth, textHeight);

			float iconWidth = 0;
			if (iconLoaded)
				iconWidth = NAV_ACTION_ICON_SIZE + NAV_ACTION_ICON_GAP;

			SizeLayoutWidget pillSize = SizeLayoutWidget.Cast(m_wNavActionSize);
			if (pillSize)
			{
				pillSize.SetWidthOverride(textWidth + iconWidth + NAV_ACTION_PAD_H * 2);
				pillSize.SetHeightOverride(NAV_ACTION_CHIP_H);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void HideNavAction()
	{
		if (m_wNavActionButton && m_NavActionHandler)
			m_wNavActionButton.RemoveHandler(m_NavActionHandler);

		m_NavActionHandler = null;

		if (m_wNavActionButton)
			m_wNavActionButton.SetVisible(false);

		if (m_wNavActionChip)
			m_wNavActionChip.SetVisible(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Disabled during a save so a second tap can't send a second request.
	protected void SetNavActionEnabled(bool enabled)
	{
		if (m_wNavActionButton)
			m_wNavActionButton.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! initialSubState restores nav state, so OnOpened() shouldn't broadcast its own default.
	void Open(notnull ELIFE_PhoneGadgetComponent phone, notnull Widget host, string initialSubState = "")
	{
		m_Phone = phone;
		m_wRoot = CreateRoot(host);
		if (!m_wRoot)
			return;

		//! Last child, so it draws over the app's content.
		m_wStatusOverlay = CreateStretched(LAYOUT_STATUS, m_wRoot);
		if (m_wStatusOverlay)
			m_wSkeletonList = m_wStatusOverlay.FindAnyWidget("SkeletonList");

		//! Index pages use "LargeTitle"; detail pages pass their own heading to TrackScroll().
		TextWidget indexTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle"));
		if (indexTitle)
		{
			indexTitle.SetText(GetTitle());
			indexTitle.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}

		OnOpened();
		ApplySubState(initialSubState);
	}

	//------------------------------------------------------------------------------------------------
	void Close()
	{
		OnClosing();

		StopTrackingScroll();

		m_aWheelHandlers.Clear();
		m_aWheelBound.Clear();

		HideNavAction();
		GetGame().GetCallqueue().Remove(ShowSkeletons);
		GetGame().GetCallqueue().Remove(HideStatus);

		m_wStatusOverlay = null;
		m_wSkeletonList = null;
		m_wLargeTitle = null;
		m_wStatusBarGlass = null;
		m_wNavBarGlass = null;
		m_wBackChipSize = null;
		m_wNavTitle = null;
		m_wNavActionButton = null;
		m_wNavActionChip = null;
		m_wNavActionLabel = null;

		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}

		m_Phone = null;
	}

	//------------------------------------------------------------------------------------------------
	// Nav bar transition
	//------------------------------------------------------------------------------------------------

	//! Points the nav bar at the scroll view and title of the current page.
	protected void TrackScroll(ScrollLayoutWidget scroll, TextWidget largeTitle)
	{
		BindWheel(scroll);

		m_TrackedScroll = scroll;
		m_wLargeTitle = largeTitle;
		m_fCollapse = -1;

		GetGame().GetCallqueue().Remove(TickCollapse);

		if (!scroll)
		{
			ApplyCollapse(0);
			return;
		}

		//! Applied now so a restored page doesn't flash an uncollapsed bar.
		TickCollapse();
		GetGame().GetCallqueue().CallLater(TickCollapse, COLLAPSE_TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Binds the wheel once per scroll widget; pages get re-tracked every time they show.
	protected void BindWheel(ScrollLayoutWidget scroll)
	{
		if (!scroll || m_aWheelBound.Contains(scroll))
			return;

		ELIFE_PhoneWheelScroll wheel = new ELIFE_PhoneWheelScroll();
		wheel.Bind(scroll);
		scroll.AddHandler(wheel);

		m_aWheelHandlers.Insert(wheel);
		m_aWheelBound.Insert(scroll);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopTrackingScroll()
	{
		GetGame().GetCallqueue().Remove(TickCollapse);
		m_TrackedScroll = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void TickCollapse()
	{
		if (!m_TrackedScroll)
		{
			GetGame().GetCallqueue().Remove(TickCollapse);
			return;
		}

		float x, y;
		m_TrackedScroll.GetSliderPosPixels(x, y);

		ApplyCollapse(Math.Clamp(y / COLLAPSE_DISTANCE, 0, 1));
	}

	//------------------------------------------------------------------------------------------------
	//! 0 = at rest (large title, clear bars), 1 = collapsed (inline title, glass bars).
	protected void ApplyCollapse(float progress)
	{
		if (m_fCollapse == progress)
			return;

		m_fCollapse = progress;

		if (m_wStatusBarGlass)
			m_wStatusBarGlass.SetOpacity(progress);

		if (m_wNavBarGlass)
			m_wNavBarGlass.SetOpacity(progress);

		//! Once the bar has glass, the action becomes a plain label on it.
		ELIFE_PhoneStyle.SetGlassPresence(m_wNavActionChip, 1 - progress);

		if (m_wNavTitle)
			m_wNavTitle.SetOpacity(progress);

		if (m_wLargeTitle)
			m_wLargeTitle.SetOpacity(1 - progress);
	}

	//------------------------------------------------------------------------------------------------
	// Loading / error states
	//------------------------------------------------------------------------------------------------

	//! Only while there's nothing to show; a failed refresh keeps existing data.
	protected void ApplyDataStatus(string key)
	{
		if (!m_wStatusOverlay || !m_Phone)
			return;

		ELIFE_EPhoneDataStatus status = m_Phone.GetDataStatus(key);
		bool empty = m_Phone.GetData(key) == "";

		if (!empty || status != ELIFE_EPhoneDataStatus.LOADING)
		{
			HideStatus();
			return;
		}

		if (m_bSkeletonVisible)
			return;

		GetGame().GetCallqueue().Remove(ShowSkeletons);
		GetGame().GetCallqueue().CallLater(ShowSkeletons, ELIFE_PhoneStyle.SPINNER_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowSkeletons()
	{
		if (!m_wStatusOverlay || !m_wSkeletonList)
			return;

		ClearChildren(m_wSkeletonList);

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		for (int i = 0; i < SKELETON_ROWS; i++)
		{
			Widget row = workspace.CreateWidgets(LAYOUT_SKELETON_ROW, m_wSkeletonList);
			if (!row)
				continue;

			AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);

			//! Rows fade toward the ground down the page.
			row.SetOpacity(1 - i * 0.15);
		}

		m_bSkeletonVisible = true;
		m_iSkeletonShownAt = System.GetTickCount();
		m_wStatusOverlay.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Skeletons stay at least SPINNER_MIN_VISIBLE_MS so they don't flash.
	protected void HideStatus()
	{
		GetGame().GetCallqueue().Remove(ShowSkeletons);
		GetGame().GetCallqueue().Remove(HideStatus);

		if (!m_wStatusOverlay)
			return;

		if (m_bSkeletonVisible)
		{
			int shownFor = System.GetTickCount() - m_iSkeletonShownAt;
			if (shownFor < ELIFE_PhoneStyle.SPINNER_MIN_VISIBLE_MS)
			{
				GetGame().GetCallqueue().CallLater(HideStatus, ELIFE_PhoneStyle.SPINNER_MIN_VISIBLE_MS - shownFor, false);
				return;
			}
		}

		m_bSkeletonVisible = false;
		m_wStatusOverlay.SetVisible(false);
		ClearChildren(m_wSkeletonList);
	}

	//------------------------------------------------------------------------------------------------
	// Navigation contract
	//------------------------------------------------------------------------------------------------

	//! Return true if Back was consumed.
	bool OnBack()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True on the landing page, where the shell hides Back.
	bool IsAtRoot()
	{
		return GetSubState() == "";
	}

	//------------------------------------------------------------------------------------------------
	//! Override for apps with in-app navigation.
	string GetSubState()
	{
		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Restores navigation from a GetSubState() value.
	void ApplySubState(string subState)
	{
	}

	//------------------------------------------------------------------------------------------------
	void BindShell(ELIFE_PhoneScreenShell shell)
	{
		m_Shell = shell;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the phone to another app. subState goes with the request since the replicated value lags.
	protected void OpenAppPage(EPhoneScreenState state, string subState)
	{
		if (m_Shell)
			m_Shell.RequestAppPage(state, subState);
	}

	//------------------------------------------------------------------------------------------------
	//! Call after any internal navigation change.
	protected void NotifySubStateChanged()
	{
		if (m_Phone)
			m_Phone.SetScreenSubState(GetSubState());

		if (m_Shell)
			m_Shell.RefreshBackVisibility();
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
	// Shared building blocks
	//------------------------------------------------------------------------------------------------

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
	//! A list row with an inset hairline separator; isLast drops it.
	protected Widget CreateListRow(ResourceName layout, notnull Widget list, bool isLast = false)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget row = workspace.CreateWidgets(layout, list);
		if (!row)
			return null;

		//! Stretch, not Fill - Fill would split leftover height between rows.
		AlignableSlot.SetHorizontalAlign(row, LayoutHorizontalAlign.Stretch);

		Widget hairline = row.FindAnyWidget("RowHairline");
		if (hairline)
		{
			hairline.SetVisible(!isLast);
			hairline.SetColor(ELIFE_PhoneStyle.WithAlpha(ELIFE_PhoneStyle.Hairline(), 0.9));
		}

		return row;
	}

	//------------------------------------------------------------------------------------------------
	//! Accent-filled circle with the name's initials.
	protected void PaintAvatar(notnull Widget row, string discName, string fallbackName, string glyphName, string name)
	{
		PaintAvatarInto(row, discName, fallbackName, glyphName, name, ELIFE_PhoneStyle.AccentDeepFor(GetScreenState()));
	}

	//------------------------------------------------------------------------------------------------
	//! Same mark with an explicit fill, for callers outside an app page.
	static void PaintAvatarInto(notnull Widget row, string discName, string fallbackName, string glyphName, string name, Color fill)
	{
		ImageWidget disc = ImageWidget.Cast(row.FindAnyWidget(discName));
		bool round = false;
		if (disc)
		{
			round = disc.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_PERSON_MARK);
			disc.SetVisible(round);
			disc.SetColor(fill);
		}

		Widget fallback = row.FindAnyWidget(fallbackName);
		if (fallback)
		{
			fallback.SetVisible(!round);
			fallback.SetColor(fill);
		}

		TextWidget glyph = TextWidget.Cast(row.FindAnyWidget(glyphName));
		if (!glyph)
			return;

		glyph.SetText(Initials(name));
		glyph.SetColor(ELIFE_PhoneStyle.TextPrimary());
	}

	//------------------------------------------------------------------------------------------------
	//! Creates a section group from PhoneListGroup.layout and returns its "GroupList" container, or null.
	protected Widget CreateListGroup(notnull Widget parent, string headerText, bool isFirst = false)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget group = workspace.CreateWidgets(LAYOUT_LIST_GROUP, parent);
		if (!group)
			return null;

		AlignableSlot.SetHorizontalAlign(group, LayoutHorizontalAlign.Stretch);
		if (!isFirst)
			LayoutSlot.SetPadding(group, 0, LIST_GROUP_GAP, 0, 0);

		TextWidget label = TextWidget.Cast(group.FindAnyWidget("GroupHeaderLabel"));
		if (label)
		{
			label.SetText(headerText);
			label.SetColor(m_Accent);
		}

		return group.FindAnyWidget("GroupList");
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
	protected void SetTextAndColor(notnull Widget parent, string widgetName, string text, Color color)
	{
		TextWidget widget = TextWidget.Cast(parent.FindAnyWidget(widgetName));
		if (!widget)
			return;

		widget.SetText(text);
		widget.SetColor(color);
	}

	//------------------------------------------------------------------------------------------------
	//! Initials for an avatar: first and last ("Jane Doe" -> "JD"), or one character for a single word.
	static string Initials(string name)
	{
		if (name.Length() == 0)
			return "";

		array<string> words = {};
		name.Split(" ", words, true);

		if (words.IsEmpty())
			return "";

		string first = words.Get(0);
		if (first.Length() == 0)
			return "";

		string monogram = first.Substring(0, 1);

		if (words.Count() < 2)
			return monogram;

		string last = words.Get(words.Count() - 1);
		if (last.Length() > 0)
			monogram += last.Substring(0, 1);

		return monogram;
	}

	//------------------------------------------------------------------------------------------------
	//! "2026-08-29T14:32:10Z" -> "14:32". Shown in UTC as sent.
	static string FormatClock(string isoTimestamp)
	{
		if (isoTimestamp.Length() < 16)
			return isoTimestamp;

		return isoTimestamp.Substring(11, 5);
	}

	//------------------------------------------------------------------------------------------------
	//! "2026-08-29T14:32:10Z" -> "29.08 14:32", for rows where the day matters.
	static string FormatDayTime(string isoTimestamp)
	{
		if (isoTimestamp.Length() < 16)
			return isoTimestamp;

		return isoTimestamp.Substring(8, 2) + "." + isoTimestamp.Substring(5, 2) + " " + isoTimestamp.Substring(11, 5);
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneAppBase()
	{
		GetGame().GetCallqueue().Remove(TickCollapse);
		GetGame().GetCallqueue().Remove(ShowSkeletons);
		GetGame().GetCallqueue().Remove(HideStatus);
	}
}
