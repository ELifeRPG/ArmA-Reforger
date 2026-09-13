//------------------------------------------------------------------------------------------------
//! Mouse-wheel scrolling for a ScrollLayoutWidget - the engine doesn't do this on its own, bound once per scroll widget by TrackScroll().
class ELIFE_PhoneWheelScroll : ScriptedWidgetEventHandler
{
	//! Pixels per wheel notch, roughly two message bubbles.
	protected const float STEP = 64;

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

		//! wheel is positive away from the user, i.e. "go up the page", so subtract.
		m_wScroll.SetSliderPosPixels(posX, posY - wheel * STEP);

		//! Consumed, or the wheel keeps travelling up to the menu behind the phone.
		return true;
	}
}

//------------------------------------------------------------------------------------------------
//! The shell chrome an app is lent while it is open, bundled instead of passed as a growing arg list.
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
//! One phone app, hosted by either the in-hand phone menu or the world screen's render target through ELIFE_PhoneScreenShell.
class ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT_STATUS = "{9C6B03E5A187D24F}UI/layouts/Menus/Phone/Apps/PhoneAppStatus.layout";
	protected const ResourceName LAYOUT_SKELETON_ROW = "{7C22D4A9E3B25F87}UI/layouts/Menus/Phone/Apps/PhoneSkeletonRow.layout";

	protected const ResourceName LAYOUT_LIST_GROUP = "{7C28D4A9E3B25F8D}UI/layouts/Menus/Phone/Widgets/PhoneListGroup.layout";

	protected const float LIST_GROUP_GAP = 6;

	//! Scroll distance for the large title to hand over to the inline title and the nav bar to earn its glass.
	protected const float COLLAPSE_DISTANCE = 22;
	protected const int COLLAPSE_TICK_MS = 33;

	//! Enough skeleton rows to fill the visible page without implying a count we don't know yet.
	protected const int SKELETON_ROWS = 5;

	//! Circle sprite for a person's avatar mark (Contacts rows, Messages threads/picker) - never accounts.
	protected const string ICON_PERSON_MARK = "circle";

	protected ELIFE_PhoneGadgetComponent m_Phone;

	//! The screen this app is hosted by, so an app can hand the phone over to another one.
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

	//! Horizontal padding either side of the pill's content, and the icon's own box + the gap it
	//! keeps from the label. The pill is never a fixed width - see ShowNavAction().
	protected const float NAV_ACTION_PAD_H = 10;
	protected const float NAV_ACTION_ICON_SIZE = 9;
	protected const float NAV_ACTION_ICON_GAP = 3;
	protected const float NAV_ACTION_CHIP_H = 20;

	//! The nav bar's one trailing action slot (Contacts' "Add", a form's "Save"), claimed per-app in OnOpened().
	protected Widget m_wNavActionSize;
	protected Widget m_wNavActionButton;
	protected Widget m_wNavActionChip;
	protected Widget m_wNavActionIconSize;
	protected ImageWidget m_wNavActionIcon;
	protected TextWidget m_wNavActionLabel;
	protected ScriptedWidgetEventHandler m_NavActionHandler;

	protected ScrollLayoutWidget m_TrackedScroll;

	//! Wheel handlers kept alive here - an uncollected-but-unreferenced handler silently stops firing.
	protected ref array<ref ELIFE_PhoneWheelScroll> m_aWheelHandlers = {};
	protected ref array<ScrollLayoutWidget> m_aWheelBound = {};
	protected float m_fCollapse = -1;
	protected bool m_bSkeletonVisible;
	protected int m_iSkeletonShownAt;

	//------------------------------------------------------------------------------------------------
	//! Called by the shell before Open(). The accent is the app's own, from ELIFE_PhoneStyle.
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

		//! Hidden until an app's OnOpened() claims it - otherwise the previous app's label/handler
		//! would still be sitting on it for the first frame of the next one.
		if (m_wNavActionButton)
			m_wNavActionButton.SetVisible(false);

		//! Light glass at rest - the committing control. ApplyCollapse fades the pill away once the
		//! nav bar has earned its own glass, so the action sits inline on the glaze.
		if (m_wNavActionChip)
			ELIFE_PhoneStyle.ApplyGlass(m_wNavActionChip, false, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Claims the nav bar's trailing action slot; safe to call again to relabel it, replacing rather than stacking the click handler.
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
	//! Relabels the action without touching its enabled state, so "Save" -> "Saving…" doesn't re-enable a disabled button; pill width hugs the localized text.
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

		//! The wrapper's own visibility gates the icon, and its width collapses to zero when absent so the pill doesn't reserve a gap.
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
	//! Disabled during a save round trip so a second tap cannot fire a second request.
	protected void SetNavActionEnabled(bool enabled)
	{
		if (m_wNavActionButton)
			m_wNavActionButton.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! initialSubState restores nav state on open, so OnOpened() shouldn't broadcast its own default.
	void Open(notnull ELIFE_PhoneGadgetComponent phone, notnull Widget host, string initialSubState = "")
	{
		m_Phone = phone;
		m_wRoot = CreateRoot(host);
		if (!m_wRoot)
			return;

		//! Last child of the root, so it draws over the app's own content.
		m_wStatusOverlay = CreateStretched(LAYOUT_STATUS, m_wRoot);
		if (m_wStatusOverlay)
			m_wSkeletonList = m_wStatusOverlay.FindAnyWidget("SkeletonList");

		//! Index pages name their large title "LargeTitle"; detail pages hand their own heading to TrackScroll() instead.
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

	//! Points the shell's nav bar at the scroll view/title that owns the current page; called again on push with the detail page's pair.
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

		//! Applied straight away so a restored page doesn't flash an uncollapsed bar for a frame.
		TickCollapse();
		GetGame().GetCallqueue().CallLater(TickCollapse, COLLAPSE_TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives a scroll widget its wheel, once. Pages are tracked again every time they are shown, so
	//! without the guard a thread reopened five times would scroll five notches per notch.
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
	//! 0 = at rest (transparent bars, large title showing), 1 = collapsed (glass bars, inline title).
	protected void ApplyCollapse(float progress)
	{
		if (m_fCollapse == progress)
			return;

		m_fCollapse = progress;

		if (m_wStatusBarGlass)
			m_wStatusBarGlass.SetOpacity(progress);

		if (m_wNavBarGlass)
			m_wNavBarGlass.SetOpacity(progress);

		//! The trailing action is a light-glass pill only while the bar is transparent. Once the
		//! bar has earned its glass the action sits on it as a label - fading the pill, not the word.
		ELIFE_PhoneStyle.SetGlassPresence(m_wNavActionChip, 1 - progress);

		if (m_wNavTitle)
			m_wNavTitle.SetOpacity(progress);

		if (m_wLargeTitle)
			m_wLargeTitle.SetOpacity(1 - progress);
	}

	//------------------------------------------------------------------------------------------------
	// Loading / error states
	//------------------------------------------------------------------------------------------------

	//! Covers the app only while it has nothing at all to show; a failed refresh keeps existing data on screen rather than blanking the page.
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

			//! Rows fade back toward the ground down the page, so the block reads as "more below"
			//! rather than as five real rows that failed to fill in.
			row.SetOpacity(1 - i * 0.15);
		}

		m_bSkeletonVisible = true;
		m_iSkeletonShownAt = System.GetTickCount();
		m_wStatusOverlay.SetVisible(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Once skeletons are up they stay up for SPINNER_MIN_VISIBLE_MS. A loading state that appears and
	//! vanishes inside a couple of frames reads as a glitch, not as progress.
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

	//! Return true if Back was consumed (e.g. statement -> account list).
	bool OnBack()
	{
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! True on the app's landing page, where the shell hides Back - GetSubState() is already "" exactly there, so apps get this for free.
	bool IsAtRoot()
	{
		return GetSubState() == "";
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
	void BindShell(ELIFE_PhoneScreenShell shell)
	{
		m_Shell = shell;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands the phone to another app on a target sub-state, passed with the request rather than written first since the replicated value only lands after a server round trip.
	protected void OpenAppPage(EPhoneScreenState state, string subState)
	{
		if (m_Shell)
			m_Shell.RequestAppPage(state, subState);
	}

	//------------------------------------------------------------------------------------------------
	//! Call after any internal navigation change (see GetSubState()).
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
	//! One row in a vertical list, separated by an inset hairline instead of a per-row card; isLast drops the trailing hairline.
	protected Widget CreateListRow(ResourceName layout, notnull Widget list, bool isLast = false)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return null;

		Widget row = workspace.CreateWidgets(layout, list);
		if (!row)
			return null;

		//! Stretch, not LayoutSizeMode.Fill - Fill would divide the list's leftover height between rows.
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
	//! A list row's identity mark: an accent-filled circle with the name's initials; fallbackName only takes the ink if the sprite fails to load.
	protected void PaintAvatar(notnull Widget row, string discName, string fallbackName, string glyphName, string name)
	{
		Color fill = ELIFE_PhoneStyle.AccentDeepFor(GetScreenState());

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
	//! Creates one section group (header + row container) from PhoneListGroup.layout; returns the empty "GroupList" widget to append rows into, or null.
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
	//! Monogram for an avatar badge - first + last initial ("Jane Doe" -> "JD"), or one letter/digit for a single word.
	protected string Initials(string name)
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
	//! "2026-08-29T14:32:10Z" -> "14:32". The API sends UTC and the phone shows it as-is.
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
