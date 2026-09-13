//------------------------------------------------------------------------------------------------
class ELIFE_ContactAddClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneContactsApp m_App;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneContactsApp app)
	{
		m_App = app;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App)
			m_App.OpenForm();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_ContactRowClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneContactsApp m_App;
	protected string m_sContactId;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneContactsApp app, string contactId)
	{
		m_App = app;
		m_sContactId = contactId;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App && m_sContactId != "")
			m_App.OpenDetail(m_sContactId);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_ContactMessageClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneContactsApp m_App;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneContactsApp app)
	{
		m_App = app;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App)
			m_App.MessageOpenContact();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_ContactSaveClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneContactsApp m_App;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneContactsApp app)
	{
		m_App = app;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App)
			m_App.SubmitForm();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! An edit box takes keyboard focus on click, but entering write mode needs an explicit call.
class ELIFE_PhoneFieldFocus : ScriptedWidgetEventHandler
{
	protected Widget m_wRule;
	protected ref Color m_FocusColor;

	//------------------------------------------------------------------------------------------------
	void Bind(Widget rule, Color focusColor)
	{
		m_wRule = rule;
		m_FocusColor = focusColor;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnFocus(Widget w, int x, int y)
	{
		EditBoxWidget field = EditBoxWidget.Cast(w);
		if (field)
			field.ActivateWriteMode();

		if (m_wRule && m_FocusColor)
			m_wRule.SetColor(m_FocusColor);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnFocusLost(Widget w, int x, int y)
	{
		if (m_wRule)
			m_wRule.SetColor(ELIFE_PhoneStyle.WithAlpha(ELIFE_PhoneStyle.Hairline(), 0.9));

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneContactsApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{3F1A6C820D9B4E51}UI/layouts/Menus/Phone/Apps/PhoneContacts.layout";
	protected const ResourceName LAYOUT_CONTACT_ROW = "{5C2E9A17B4D06F38}UI/layouts/Menus/Phone/Apps/PhoneContactRow.layout";

	//! LoadImageFromSet() fails closed (no image, no error) on an unknown sprite name - "checkmark" silently drew nothing before it turned out to be "check".
	protected const string ICON_ADD = "plus";
	protected const string ICON_SAVE = "check";
	protected const string ICON_MESSAGE = "comments";

	//! Sub-state values: "" is the index, SUBSTATE_FORM is the (single) add form, a detail page is
	//! SUBSTATE_DETAIL_PREFIX + contactId.
	protected const string SUBSTATE_FORM = "new";
	protected const string SUBSTATE_DETAIL_PREFIX = "c:";

	//! Same form, prefilled - Messages' own hand-off for an unsaved number. Public/static since
	//! Messages builds this value and Contacts reads it, mirroring SUBSTATE_CONTACT_PREFIX in reverse.
	static const string SUBSTATE_FORM_NUMBER_PREFIX = "new:";

	//! Enforced here, not via EditBoxFilterComponent - the engine refuses to attach that component to
	//! a bare EditBoxWidget. Only stops a runaway string; the backend still judges the number's shape.
	protected const int MAX_NUMBER_LENGTH = 24;
	protected const int MAX_NAME_LENGTH = 40;

	//! EditBox write mode is left-aligned. Hug the box to the string and keep the box centered so
	//! typing still reads as centered text.
	protected const float NAME_FIELD_PAD = 8;
	protected const float NAME_FIELD_MAX = 216;

	protected ScrollLayoutWidget m_wContactScroll;
	protected Widget m_wContactGroups;
	protected Widget m_wEmptyContacts;
	protected TextWidget m_wIndexTitle;

	protected Widget m_wIndexPage;
	protected Widget m_wFormPage;

	protected Widget m_wDetailPage;
	protected ScrollLayoutWidget m_wDetailScroll;
	protected TextWidget m_wDetailTitle;
	protected ImageWidget m_wDetailAvatarDisc;
	protected Widget m_wDetailAvatarFallback;
	protected TextWidget m_wDetailAvatarGlyph;
	protected Widget m_wMessageButton;
	protected ImageWidget m_wMessageDisc;
	protected Widget m_wMessageFallback;
	protected ImageWidget m_wMessageHover;
	protected ImageWidget m_wMessageIcon;
	protected Widget m_wMessageIconSize;
	protected Widget m_wMessageGlyph;
	protected TextWidget m_wMessageLabel;
	protected ScrollLayoutWidget m_wFormScroll;
	protected ImageWidget m_wFormAvatarDisc;
	protected Widget m_wFormAvatarFallback;
	protected TextWidget m_wFormAvatarGlyph;
	protected EditBoxWidget m_wNumberField;
	protected EditBoxWidget m_wNameField;
	protected SizeLayoutWidget m_wNameFieldSize;
	protected TextWidget m_wNameFieldMeasure;
	protected TextWidget m_wFormError;
	protected string m_sFormError;

	protected ref ELIFE_ContactAddClick m_AddClick;
	protected ref ELIFE_ContactSaveClick m_SaveClick;
	protected ref ELIFE_ContactMessageClick m_MessageClick;
	protected ref array<ref ELIFE_ContactRowClick> m_aRowClicks = {};
	protected ref ELIFE_PhoneFieldFocus m_NumberFocus;
	protected ref ELIFE_PhoneFieldFocus m_NameFocus;

	protected bool m_bFormOpen;

	//! Set when the form was opened prefilled (Messages' hand-off for an unsaved number) - carried into
	//! GetSubState() so the world RT reopens the same prefilled form, not a blank one.
	protected string m_sPrefillNumber;

	//! The contact whose detail page is open, held by contactId. Empty means the index is showing.
	protected string m_sOpenContactId;

	//! Kept (not local) so FindContact's returned DTO stays alive when FillDetail reads it later.
	protected ref ELIFE_ContactListDto m_List;

	protected bool m_bSaving;
	protected int m_iSavingShownAt;

	//! The request's outcome, held until the "Saving…" label has had its minimum time on screen.
	protected bool m_bHasPendingResult;
	protected ELIFE_EContactSaveResult m_ePendingResult;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Contacts";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.CONTACTS;
	}

	//------------------------------------------------------------------------------------------------
	//! Form and detail page each count as their own nav level - Back returns to the list, not the app.
	override bool OnBack()
	{
		if (m_bFormOpen)
		{
			CloseForm();
			return true;
		}

		if (m_sOpenContactId != "")
		{
			ShowIndex();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Without a value per page, the world render target kept showing the list while owner was on the form.
	override string GetSubState()
	{
		if (m_bFormOpen)
		{
			if (m_sPrefillNumber != "")
				return SUBSTATE_FORM_NUMBER_PREFIX + m_sPrefillNumber;

			return SUBSTATE_FORM;
		}

		if (m_sOpenContactId != "")
			return SUBSTATE_DETAIL_PREFIX + m_sOpenContactId;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	override void ApplySubState(string subState)
	{
		if (subState == SUBSTATE_FORM)
		{
			OpenForm();
			return;
		}

		if (subState.StartsWith(SUBSTATE_FORM_NUMBER_PREFIX))
		{
			OpenForm(subState.Substring(SUBSTATE_FORM_NUMBER_PREFIX.Length(), subState.Length() - SUBSTATE_FORM_NUMBER_PREFIX.Length()));
			return;
		}

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
		m_wFormPage = m_wRoot.FindAnyWidget("FormPage");
		m_wContactScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("ContactScroll"));
		m_wContactGroups = m_wRoot.FindAnyWidget("ContactGroups");
		m_wEmptyContacts = m_wRoot.FindAnyWidget("EmptyContacts");
		m_wIndexTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle"));

		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("NumberCard"), true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("DetailCard"), true);

		m_wDetailPage = m_wRoot.FindAnyWidget("DetailPage");
		m_wDetailScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("DetailScroll"));
		m_wDetailTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailTitle"));
		m_wDetailAvatarDisc = ImageWidget.Cast(m_wRoot.FindAnyWidget("DetailAvatarDisc"));
		m_wDetailAvatarFallback = m_wRoot.FindAnyWidget("DetailAvatarFallback");
		m_wDetailAvatarGlyph = TextWidget.Cast(m_wRoot.FindAnyWidget("DetailAvatarGlyph"));
		m_wMessageButton = m_wRoot.FindAnyWidget("ButtonMessage");
		m_wMessageDisc = ImageWidget.Cast(m_wRoot.FindAnyWidget("MessageDisc"));
		m_wMessageFallback = m_wRoot.FindAnyWidget("MessageFallback");
		m_wMessageIconSize = m_wRoot.FindAnyWidget("MessageIconSize");
		m_wMessageIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("MessageIcon"));
		m_wMessageGlyph = m_wRoot.FindAnyWidget("MessageGlyph");
		m_wMessageLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("MessageLabel"));

		Widget messageFrame = m_wRoot.FindAnyWidget("MessageFrame");
		if (messageFrame)
			m_wMessageHover = ImageWidget.Cast(messageFrame.FindAnyWidget("Background"));

		m_wFormScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("FormScroll"));
		m_wFormAvatarDisc = ImageWidget.Cast(m_wRoot.FindAnyWidget("FormAvatarDisc"));
		m_wFormAvatarFallback = m_wRoot.FindAnyWidget("FormAvatarFallback");
		m_wFormAvatarGlyph = TextWidget.Cast(m_wRoot.FindAnyWidget("FormAvatarGlyph"));
		m_wNumberField = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("NumberField"));
		m_wNameField = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("NameField"));
		m_wNameFieldSize = SizeLayoutWidget.Cast(m_wRoot.FindAnyWidget("NameFieldSize"));
		m_wNameFieldMeasure = TextWidget.Cast(m_wRoot.FindAnyWidget("NameFieldMeasure"));
		m_wFormError = TextWidget.Cast(m_wRoot.FindAnyWidget("FormError"));

		m_NumberFocus = BindField(m_wNumberField, "NumberFieldRule");
		m_NameFocus = BindField(m_wNameField, "NameFieldRule");

		//! OnChange cannot be overloaded on a ScriptedWidgetEventHandler - reserved engine event.
		//! The field's SCR_EventHandlerComponent already owns it and exposes GetOnChange() per keystroke.
		if (m_wNameField)
		{
			SCR_EventHandlerComponent nameEvents = SCR_EventHandlerComponent.Cast(m_wNameField.FindHandler(SCR_EventHandlerComponent));
			if (nameEvents)
				nameEvents.GetOnChange().Insert(OnFormNameChanged);
		}

		if (m_wNumberField)
		{
			SCR_EventHandlerComponent numberEvents = SCR_EventHandlerComponent.Cast(m_wNumberField.FindHandler(SCR_EventHandlerComponent));
			if (numberEvents)
			{
				numberEvents.GetOnChange().Insert(OnFormNumberChanged);
				numberEvents.GetOnFocusLost().Insert(OnFormNumberLeft);
			}
		}

		TrackScroll(m_wContactScroll, m_wIndexTitle);
		ShowAddAction();

		if (!m_Phone)
			return;

		m_Phone.m_OnDataChanged.Insert(OnDataChanged);
		m_Phone.m_OnContactSaveResult.Insert(OnContactSaveResult);

		//! Draw what we already have, then refresh - the fetch lands later via OnDataChanged.
		Fill();
		m_Phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		if (m_Phone)
		{
			m_Phone.m_OnDataChanged.Remove(OnDataChanged);
			m_Phone.m_OnContactSaveResult.Remove(OnContactSaveResult);
		}

		GetGame().GetCallqueue().Remove(ShowSavingLabel);
		GetGame().GetCallqueue().Remove(FinishSaving);

		m_wContactScroll = null;
		m_wContactGroups = null;
		m_wEmptyContacts = null;
		m_wIndexTitle = null;
		m_wIndexPage = null;
		m_wFormPage = null;
		m_wFormScroll = null;
		m_wFormAvatarDisc = null;
		m_wFormAvatarFallback = null;
		m_wFormAvatarGlyph = null;
		m_wNumberField = null;
		m_wNameField = null;
		m_wNameFieldSize = null;
		m_wNameFieldMeasure = null;
		m_wFormError = null;
		m_sFormError = "";
		m_wDetailPage = null;
		m_wDetailScroll = null;
		m_wDetailTitle = null;
		m_wDetailAvatarDisc = null;
		m_wDetailAvatarFallback = null;
		m_wDetailAvatarGlyph = null;
		m_wMessageButton = null;
		m_wMessageDisc = null;
		m_wMessageFallback = null;
		m_wMessageHover = null;
		m_wMessageIcon = null;
		m_wMessageIconSize = null;
		m_wMessageGlyph = null;
		m_wMessageLabel = null;
		m_AddClick = null;
		m_SaveClick = null;
		m_MessageClick = null;
		m_aRowClicks.Clear();
		m_NumberFocus = null;
		m_NameFocus = null;
		m_List = null;
		m_bFormOpen = false;
		m_sPrefillNumber = "";
		m_sOpenContactId = "";
		m_bSaving = false;
		m_bHasPendingResult = false;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDataChanged(string key)
	{
		if (key == ELIFE_PhoneGadgetComponent.DATA_CONTACTS)
			Fill();
	}

	//------------------------------------------------------------------------------------------------
	protected void Fill()
	{
		ApplyDataStatus(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
		m_aRowClicks.Clear();
		ClearChildren(m_wContactGroups);

		m_List = new ELIFE_ContactListDto();
		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
		if (json != "")
			m_List.ExpandFromRAW(json);

		int count = m_List.items.Count();

		if (m_wEmptyContacts)
			m_wEmptyContacts.SetVisible(count == 0);

		if (!m_wContactGroups || count == 0)
		{
			if (m_sOpenContactId != "")
				ShowIndex();

			return;
		}

		array<ref ELIFE_ContactDto> sorted = ELIFE_PhoneContactBook.SortedByLastName(m_List);

		string currentLetter = "";
		Widget currentList = null;
		bool isFirstGroup = true;

		for (int i = 0; i < sorted.Count(); i++)
		{
			ELIFE_ContactDto contact = sorted.Get(i);
			if (!contact)
				continue;

			string letter = ELIFE_PhoneContactBook.GroupLetter(contact);
			if (letter != currentLetter || !currentList)
			{
				currentLetter = letter;
				currentList = CreateListGroup(m_wContactGroups, letter, isFirstGroup);
				isFirstGroup = false;
			}

			if (!currentList)
				continue;

			//! isLast is always false - these are glass cards, not the hairline-row style it applies to.
			Widget row = CreateListRow(LAYOUT_CONTACT_ROW, currentList, false);
			if (!row)
				continue;

			ELIFE_PhoneStyle.ApplyGlass(row, true);

			string name = ELIFE_PhoneContactBook.DisplayName(contact);
			SetTextAndColor(row, "ContactName", name, ELIFE_PhoneStyle.TextPrimary());
			SetTextAndColor(row, "ContactNumber", contact.number, ELIFE_PhoneStyle.TextSecondary());
			PaintAvatar(row, "ContactAvatarDisc", "ContactAvatarFallback", "ContactAvatarGlyph", name);

			Widget button = row.FindAnyWidget("ContactButton");
			if (!button)
				button = row;

			ELIFE_ContactRowClick click = new ELIFE_ContactRowClick();
			click.Bind(this, ELIFE_PhoneContactBook.KeyFor(contact));
			button.AddHandler(click);
			m_aRowClicks.Insert(click);
		}

		//! A refresh can rename or delete the contact an open detail page is showing, so re-read it.
		if (m_sOpenContactId != "" && !FillDetail())
			ShowIndex();
	}

	//------------------------------------------------------------------------------------------------
	void OpenDetail(string contactId)
	{
		if (contactId == "")
			return;

		m_sOpenContactId = contactId;
		m_bFormOpen = false;

		if (!FillDetail())
		{
			m_sOpenContactId = "";
			return;
		}

		if (m_wFormPage)
			m_wFormPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wDetailPage)
			m_wDetailPage.SetVisible(true);

		TrackScroll(m_wDetailScroll, m_wDetailTitle);

		//! The nav bar's trailing pill slot belongs to Save, so Message gets its own labelled button.
		HideNavAction();
		ShowMessageAction();

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected bool FillDetail()
	{
		ELIFE_ContactDto contact = FindContact(m_sOpenContactId);
		if (!contact)
			return false;

		string name = ELIFE_PhoneContactBook.DisplayName(contact);

		SetTextAndColor(m_wRoot, "DetailTitle", name, ELIFE_PhoneStyle.TextPrimary());
		SetTextAndColor(m_wRoot, "DetailNumberLabel", "#ELIFE-Phone_Contacts_Form_Number", ELIFE_PhoneStyle.TextPrimary());
		SetTextAndColor(m_wRoot, "DetailNumberValue", contact.number, ELIFE_PhoneStyle.TextSecondary());

		PaintHeroAvatar(m_wDetailAvatarDisc, m_wDetailAvatarFallback, m_wDetailAvatarGlyph, name);
		SyncDetailNavTitle();
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! ApplyCollapse() only cross-fades opacity between the nav title and DetailTitle, so a rename
	//! from a refresh needs this to reach the collapsed (scrolled-down) title too.
	protected void SyncDetailNavTitle()
	{
		if (m_wNavTitle && m_wDetailTitle)
			m_wNavTitle.SetText(m_wDetailTitle.GetText());
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFormNameChanged(Widget w)
	{
		string name = "";
		if (m_wNameField)
			name = m_wNameField.GetText();

		PaintFormAvatar(name);
		FitNameField();
	}

	//------------------------------------------------------------------------------------------------
	//! Box is centered and sized to the string so the caret lands right after a centered run of letters, not mid-way through a wide field.
	protected void FitNameField()
	{
		if (!m_wNameField || !m_wNameFieldSize || !m_wNameFieldMeasure)
			return;

		string shown = m_wNameField.GetText();
		if (shown == "")
			shown = m_wNameField.GetPlaceholderText();

		m_wNameFieldMeasure.SetText(shown);

		float textWidth, textHeight;
		m_wNameFieldMeasure.GetTextSize(textWidth, textHeight);

		float width = textWidth + NAME_FIELD_PAD;
		if (width > NAME_FIELD_MAX)
			width = NAME_FIELD_MAX;

		m_wNameFieldSize.SetWidthOverride(width);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFormNumberChanged(Widget w)
	{
		RefreshNumberError(false);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnFormNumberLeft(Widget w)
	{
		RefreshNumberError(true);
	}

	//------------------------------------------------------------------------------------------------
	//! Placeholder already says "8 digits". Don't nag while they are still typing a short number;
	//! show the error when they leave a bad value, or as soon as it is too long to become valid.
	protected void RefreshNumberError(bool leaving)
	{
		string number = "";
		if (m_wNumberField)
			number = m_wNumberField.GetText();

		if (number == "" || ELIFE_PhoneNumber.IsWellFormed(number))
		{
			if (m_sFormError == "#ELIFE-Phone_Contacts_Form_InvalidNumber")
				ShowFormError("");

			return;
		}

		if (leaving || CountDigits(number) > ELIFE_PhoneNumber.DIGIT_COUNT)
			ShowFormError("#ELIFE-Phone_Contacts_Form_InvalidNumber");
	}

	//------------------------------------------------------------------------------------------------
	protected int CountDigits(string raw)
	{
		int digits;
		int length = raw.Length();
		for (int i = 0; i < length; i++)
		{
			if ("0123456789".IndexOf(raw.Substring(i, 1)) != -1)
				digits++;
		}

		return digits;
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintFormAvatar(string displayName)
	{
		PaintHeroAvatar(m_wFormAvatarDisc, m_wFormAvatarFallback, m_wFormAvatarGlyph, displayName);
	}

	//------------------------------------------------------------------------------------------------
	//! Same monogram as the list row, but hero-sized on a real circle instead of the row's rounded square.
	protected void PaintHeroAvatar(ImageWidget disc, Widget fallback, TextWidget glyph, string displayName)
	{
		Color fill = ELIFE_PhoneStyle.AccentDeepFor(GetScreenState());

		bool round = false;
		if (disc)
		{
			round = disc.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_PERSON_MARK);
			disc.SetVisible(round);
			disc.SetColor(fill);
		}

		if (fallback)
		{
			fallback.SetVisible(!round);
			fallback.SetColor(fill);
		}

		if (glyph)
		{
			glyph.SetText(Initials(displayName));
			glyph.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_ContactDto FindContact(string contactId)
	{
		if (contactId == "" || !m_List)
			return null;

		foreach (ELIFE_ContactDto contact : m_List.items)
		{
			if (!contact)
				continue;

			if (contact.contactId == contactId || contact.number == contactId)
				return contact;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Icon-only circle like the Add FAB, but in MESSAGES' accent since it's a door into that app.
	protected void ShowMessageAction()
	{
		if (!m_MessageClick)
		{
			m_MessageClick = new ELIFE_ContactMessageClick();
			m_MessageClick.Bind(this);

			if (m_wMessageButton)
				m_wMessageButton.AddHandler(m_MessageClick);
		}

		Color fill = ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES);

		bool round = false;
		if (m_wMessageDisc)
		{
			round = m_wMessageDisc.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_PERSON_MARK);
			m_wMessageDisc.SetVisible(round);
			m_wMessageDisc.SetColor(fill);
		}

		if (m_wMessageFallback)
		{
			m_wMessageFallback.SetVisible(!round);
			m_wMessageFallback.SetColor(fill);
		}

		if (m_wMessageHover)
			m_wMessageHover.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_PERSON_MARK);

		bool iconLoaded = false;
		if (m_wMessageIcon)
		{
			iconLoaded = m_wMessageIcon.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_MESSAGE);
			if (iconLoaded)
			{
				m_wMessageIcon.SetColor(ELIFE_PhoneStyle.TextPrimary());
				ELIFE_PhoneStyle.FitIcon(m_wMessageIcon, 16);
			}
		}

		//! The wrapper carries the visibility - a hidden wrapper hides a perfectly loaded child.
		if (m_wMessageIconSize)
			m_wMessageIconSize.SetVisible(iconLoaded);

		//! Never an empty circle: if the sprite ever goes missing the glyph takes the ink instead.
		if (m_wMessageGlyph)
			m_wMessageGlyph.SetVisible(!iconLoaded);

		//! Caption is a child of the button, so it takes the destination app's accent along with the disc.
		if (m_wMessageLabel)
			m_wMessageLabel.SetColor(ELIFE_PhoneStyle.AccentFor(EPhoneScreenState.MESSAGES));
	}

	//------------------------------------------------------------------------------------------------
	//! Hands off to Messages by contactId, not number - sub-state is replicated, and a number would
	//! print on a bystander's screen before Messages could redact it.
	void MessageOpenContact()
	{
		if (m_sOpenContactId == "")
			return;

		OpenAppPage(EPhoneScreenState.MESSAGES, ELIFE_PhoneMessagesApp.SUBSTATE_CONTACT_PREFIX + m_sOpenContactId);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowIndex()
	{
		m_bFormOpen = false;
		m_sPrefillNumber = "";
		m_sOpenContactId = "";
		CancelSaving();

		if (m_wFormPage)
			m_wFormPage.SetVisible(false);

		if (m_wDetailPage)
			m_wDetailPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText(GetTitle());

		TrackScroll(m_wContactScroll, m_wIndexTitle);
		ShowAddAction();

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Returns the handler so the app can hold it - a collected handler stops firing, silently.
	protected ELIFE_PhoneFieldFocus BindField(EditBoxWidget field, string ruleName)
	{
		if (!field || !m_wRoot)
			return null;

		ELIFE_PhoneFieldFocus focus = new ELIFE_PhoneFieldFocus();
		focus.Bind(m_wRoot.FindAnyWidget(ruleName), m_Accent);
		field.AddHandler(focus);

		return focus;
	}

	//------------------------------------------------------------------------------------------------
	//! Index's commit-free "+" lives in the shared nav trailing pill, same slot the form's "Save" claims.
	protected void ShowAddAction()
	{
		if (!m_AddClick)
		{
			m_AddClick = new ELIFE_ContactAddClick();
			m_AddClick.Bind(this);
		}

		ShowNavAction("#ELIFE-Phone_Contacts_Index_Add", m_Accent, m_AddClick, ICON_ADD);
	}

	//------------------------------------------------------------------------------------------------
	//! prefillNumber is Messages' hand-off for a number it doesn't recognize - empty for the normal Add.
	void OpenForm(string prefillNumber = "")
	{
		if (m_bFormOpen)
			return;

		m_bFormOpen = true;
		m_sOpenContactId = "";
		m_sPrefillNumber = prefillNumber;

		if (m_wDetailPage)
			m_wDetailPage.SetVisible(false);

		if (m_wNumberField)
			m_wNumberField.SetText(prefillNumber);

		if (m_wNameField)
			m_wNameField.SetText("");

		PaintFormAvatar("");
		FitNameField();
		ShowFormError("");

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wFormPage)
			m_wFormPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText("#ELIFE-Phone_Contacts_Form_Title");

		//! No on-page large title — the hero name is the identity, like detail. Nav shows
		//! "New Contact" once the bar collapses.
		TrackScroll(m_wFormScroll, null);

		if (!m_SaveClick)
		{
			m_SaveClick = new ELIFE_ContactSaveClick();
			m_SaveClick.Bind(this);
		}

		ShowNavAction("#ELIFE-Phone_Contacts_Form_Save", m_Accent, m_SaveClick, ICON_SAVE);

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void CloseForm()
	{
		ShowIndex();
	}

	//------------------------------------------------------------------------------------------------
	void SubmitForm()
	{
		if (m_bSaving || !m_Phone)
			return;

		string number = "";
		if (m_wNumberField)
			number = m_wNumberField.GetText();

		string displayName = "";
		if (m_wNameField)
			displayName = m_wNameField.GetText();

		if (number == "" || displayName == "")
		{
			ShowFormError("#ELIFE-Phone_Contacts_Form_Incomplete");
			return;
		}

		if (number.Length() > MAX_NUMBER_LENGTH)
			number = number.Substring(0, MAX_NUMBER_LENGTH);

		if (displayName.Length() > MAX_NAME_LENGTH)
			displayName = displayName.Substring(0, MAX_NAME_LENGTH);

		//! Backend still rejects a malformed number with 400; this just catches a typo without the round trip.
		if (!ELIFE_PhoneNumber.IsWellFormed(number))
		{
			ShowFormError("#ELIFE-Phone_Contacts_Form_InvalidNumber");
			return;
		}

		ShowFormError("");
		BeginSaving();

		m_Phone.SaveContact(number, displayName);
	}

	//------------------------------------------------------------------------------------------------
	//! Held rather than applied immediately, so a "Saving…" label gets its minimum time on screen.
	protected void OnContactSaveResult(ELIFE_EContactSaveResult result)
	{
		if (!m_wRoot || !m_bSaving)
			return;

		m_bHasPendingResult = true;
		m_ePendingResult = result;

		EndSaving();
	}

	//------------------------------------------------------------------------------------------------
	//! Label only shows after SPINNER_DELAY_MS - most saves resolve before that and never flash it.
	protected void BeginSaving()
	{
		m_bSaving = true;
		m_iSavingShownAt = 0;
		m_bHasPendingResult = false;
		SetNavActionEnabled(false);

		GetGame().GetCallqueue().Remove(ShowSavingLabel);
		GetGame().GetCallqueue().Remove(FinishSaving);
		GetGame().GetCallqueue().CallLater(ShowSavingLabel, ELIFE_PhoneStyle.SPINNER_DELAY_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowSavingLabel()
	{
		if (!m_bSaving)
			return;

		m_iSavingShownAt = System.GetTickCount();
		SetNavActionLabel("#ELIFE-Phone_Contacts_Form_Saving", m_Accent, ICON_SAVE);
	}

	//------------------------------------------------------------------------------------------------
	//! Finishes at once if the label never appeared, otherwise waits out SPINNER_MIN_VISIBLE_MS so it
	//! doesn't flash and vanish.
	protected void EndSaving()
	{
		GetGame().GetCallqueue().Remove(ShowSavingLabel);

		if (!m_bSaving)
			return;

		if (m_iSavingShownAt > 0)
		{
			int shownFor = System.GetTickCount() - m_iSavingShownAt;
			if (shownFor < ELIFE_PhoneStyle.SPINNER_MIN_VISIBLE_MS)
			{
				GetGame().GetCallqueue().Remove(FinishSaving);
				GetGame().GetCallqueue().CallLater(FinishSaving, ELIFE_PhoneStyle.SPINNER_MIN_VISIBLE_MS - shownFor, false);
				return;
			}
		}

		FinishSaving();
	}

	//------------------------------------------------------------------------------------------------
	//! Success closes the form - the new row appearing behind it is confirmation enough.
	protected void FinishSaving()
	{
		GetGame().GetCallqueue().Remove(FinishSaving);

		m_bSaving = false;
		m_iSavingShownAt = 0;
		SetNavActionEnabled(true);

		if (m_bFormOpen)
			SetNavActionLabel("#ELIFE-Phone_Contacts_Form_Save", m_Accent, ICON_SAVE);

		if (!m_bHasPendingResult)
			return;

		m_bHasPendingResult = false;

		if (m_ePendingResult == ELIFE_EContactSaveResult.SAVED)
			CloseForm();
		else
			ShowFormError(FailureText(m_ePendingResult));
	}

	//------------------------------------------------------------------------------------------------
	//! Backing out mid-request: stop tracking the save so its response is ignored when it lands.
	protected void CancelSaving()
	{
		GetGame().GetCallqueue().Remove(ShowSavingLabel);
		GetGame().GetCallqueue().Remove(FinishSaving);

		m_bSaving = false;
		m_iSavingShownAt = 0;
		m_bHasPendingResult = false;
		SetNavActionEnabled(true);
	}

	//------------------------------------------------------------------------------------------------
	protected string FailureText(ELIFE_EContactSaveResult result)
	{
		if (result == ELIFE_EContactSaveResult.INVALID_NUMBER)
			return "#ELIFE-Phone_Contacts_Form_InvalidNumber";

		if (result == ELIFE_EContactSaveResult.DUPLICATE)
			return "#ELIFE-Phone_Contacts_Form_Duplicate";

		return "#ELIFE-Phone_Contacts_Form_SaveError";
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowFormError(string text)
	{
		m_sFormError = text;

		if (!m_wFormError)
			return;

		m_wFormError.SetVisible(text != "");
		m_wFormError.SetText(text);
	}
}
