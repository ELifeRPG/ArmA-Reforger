//------------------------------------------------------------------------------------------------
class ELIFE_PhoneThreadRowClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneMessagesApp m_App;
	protected string m_sThreadId;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneMessagesApp app, string threadId)
	{
		m_App = app;
		m_sThreadId = threadId;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App && m_sThreadId != "")
			m_App.OpenThread(m_sThreadId);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneSendClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneMessagesApp m_App;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneMessagesApp app)
	{
		m_App = app;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App)
			m_App.SubmitCompose();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
//! Enter-to-send is NOT handled here via OnChange - that's a reserved engine event with a fixed
//! signature, and overloading it is a hard compile error. BindCompose() instead subscribes to
//! GetOnChangeFinal() on the field's SCR_EventHandlerComponent, which fires exactly on commit.
class ELIFE_PhoneComposeFocus : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneMessagesApp m_App;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneMessagesApp app)
	{
		m_App = app;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnFocus(Widget w, int x, int y)
	{
		EditBoxWidget field = EditBoxWidget.Cast(w);
		if (field)
			field.ActivateWriteMode();

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneMessagesApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{8B7D4F2306E1A95C}UI/layouts/Menus/Phone/Apps/PhoneMessages.layout";
	protected const ResourceName LAYOUT_THREAD_ROW = "{2D9F5081C6A73B4E}UI/layouts/Menus/Phone/Apps/PhoneThreadRow.layout";
	protected const ResourceName LAYOUT_MESSAGE_ROW = "{6E48B3D9F20C517A}UI/layouts/Menus/Phone/Apps/PhoneMessageRow.layout";

	protected Widget m_wIndexPage;
	protected Widget m_wThreadPage;
	protected ScrollLayoutWidget m_wThreadScroll;
	protected Widget m_wThreadGroups;
	protected Widget m_wEmptyThreads;
	protected ScrollLayoutWidget m_wMessageScroll;
	protected Widget m_wMessageList;
	protected Widget m_wEmptyMessages;
	protected TextWidget m_wIndexTitle;
	protected TextWidget m_wThreadTitle;

	protected ref array<ref ELIFE_PhoneThreadRowClick> m_aRowClicks = {};

	protected Widget m_wComposeBar;
	protected EditBoxWidget m_wComposeField;
	protected Widget m_wSendButton;
	protected ImageWidget m_wSendDisc;
	protected Widget m_wSendFallback;
	protected ImageWidget m_wSendHover;
	protected ImageWidget m_wSendIcon;
	protected Widget m_wSendIconSize;
	protected Widget m_wSendGlyph;

	protected ref ELIFE_PhoneSendClick m_SendClick;
	protected ref ELIFE_PhoneComposeFocus m_ComposeFocus;

	//! In flight, so a second Enter or a second tap cannot fire a second send of the same text.
	protected bool m_bSending;

	//! API answers 400 for an over-long body; this only stops an obviously oversized one client-side.
	protected const int MAX_BODY_LENGTH = 480;

	//! LoadImageFromSet() fails closed on an unknown sprite name.
	protected const string ICON_DISC = "circle";

	//! Chat atlas' paper-plane sprite - the wrapper set has no plane and no good stand-in for "send".
	protected const string ICON_SEND = "whisper";
	protected ref ELIFE_MessageUpdatesDto m_Updates = new ELIFE_MessageUpdatesDto();
	protected string m_sOpenThreadId;

	//! A conversation addressed by number, not thread id - Contacts' "Message" on someone who has
	//! never written. Promotes itself to the real thread once one shows up in an update.
	protected string m_sOpenNumber;

	//! The contact this empty conversation is with, when opened from Contacts. Replicated sub-state
	//! carries this id rather than the number, which must not reach a bystander's screen.
	protected string m_sOpenContactId;

	//! Sub-state prefix for a number hand-off (a thread id never starts with it). Public/static since
	//! Contacts builds this value and Messages reads it.
	static const string SUBSTATE_NUMBER_PREFIX = "n:";

	//! Same channel, for a saved contact - preferred over `n:` whenever an id is known.
	static const string SUBSTATE_CONTACT_PREFIX = "c:";

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Messages";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.MESSAGES;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnBack()
	{
		if (m_sOpenThreadId != "" || m_sOpenNumber != "" || m_sOpenContactId != "")
		{
			ShowIndex();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override string GetSubState()
	{
		if (m_sOpenThreadId != "")
			return m_sOpenThreadId;

		if (m_sOpenContactId != "")
			return SUBSTATE_CONTACT_PREFIX + m_sOpenContactId;

		if (m_sOpenNumber != "")
			return SUBSTATE_NUMBER_PREFIX + m_sOpenNumber;

		return "";
	}

	//------------------------------------------------------------------------------------------------
	override void ApplySubState(string subState)
	{
		if (subState == "")
		{
			ShowIndex();
			return;
		}

		if (subState.StartsWith(SUBSTATE_CONTACT_PREFIX))
		{
			OpenConversationWithContact(subState.Substring(SUBSTATE_CONTACT_PREFIX.Length(), subState.Length() - SUBSTATE_CONTACT_PREFIX.Length()));
			return;
		}

		if (subState.StartsWith(SUBSTATE_NUMBER_PREFIX))
		{
			OpenConversationWith(subState.Substring(SUBSTATE_NUMBER_PREFIX.Length(), subState.Length() - SUBSTATE_NUMBER_PREFIX.Length()));
			return;
		}

		OpenThread(subState);
	}

	//------------------------------------------------------------------------------------------------
	void OpenConversationWith(string number)
	{
		ELIFE_ContactDto contact = ELIFE_PhoneContactBook.FindByNumber(m_Phone, number);
		string contactId = "";
		if (contact)
			contactId = contact.contactId;

		OpenConversation(contactId, number);
	}

	//------------------------------------------------------------------------------------------------
	//! Contacts' hand-off. Keeps the id even if the contact list hasn't landed yet - OnDataChanged
	//! resolves the number once it does, rather than dropping the hand-off.
	void OpenConversationWithContact(string contactId)
	{
		ELIFE_ContactDto contact = ELIFE_PhoneContactBook.FindById(m_Phone, contactId);
		string number = "";
		if (contact)
			number = contact.number;

		OpenConversation(contactId, number);
	}

	//------------------------------------------------------------------------------------------------
	protected void OpenConversation(string contactId, string number)
	{
		m_sOpenContactId = contactId;

		if (number != "")
		{
			ELIFE_ThreadDto existing = FindThreadWith(number);
			if (existing)
			{
				OpenThread(existing.threadId);
				return;
			}
		}

		if (number == "" && contactId == "")
			return;

		m_sOpenThreadId = "";
		m_sOpenNumber = number;
		ShowThreadPage();
		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Only matches one-to-one conversations - nothing creates group threads yet.
	protected ELIFE_ThreadDto FindThreadWith(string number)
	{
		if (number == "")
			return null;

		foreach (ELIFE_ThreadDto threadDto : m_Updates.threads)
		{
			foreach (string participant : threadDto.participants)
			{
				if (participant == number)
					return threadDto;
			}
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	void OpenThread(string threadId)
	{
		m_sOpenThreadId = threadId;
		m_sOpenNumber = "";
		m_sOpenContactId = "";
		ShowThreadPage();

		//! Opening a thread is what reading it means, so the unread count clears here.
		if (m_Phone && threadId != "")
			m_Phone.MarkThreadRead(threadId);

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Shared by both ways in, so an empty number-addressed conversation gets the same chrome as a real thread.
	protected void ShowThreadPage()
	{
		FillThread();

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wThreadPage)
			m_wThreadPage.SetVisible(true);

		TrackScroll(m_wMessageScroll, m_wThreadTitle);
		ScrollToLatest();
	}

	//------------------------------------------------------------------------------------------------
	//! ApplyCollapse() only cross-fades opacity between the nav title and ThreadTitle, so every
	//! FillThread() branch has to call this or the collapsed title goes stale.
	protected void SyncThreadNavTitle()
	{
		if (m_wNavTitle && m_wThreadTitle)
			m_wNavTitle.SetText(m_wThreadTitle.GetText());
	}

	//------------------------------------------------------------------------------------------------
	//! Runs twice, one tick apart: the row just created this frame hasn't been through layout yet, so
	//! an immediate SetSliderPos(0, 1) lands one row short and the second call corrects it.
	protected void ScrollToLatest()
	{
		if (!m_wMessageScroll)
			return;

		GetGame().GetCallqueue().Remove(ApplyScrollToLatest);
		ApplyScrollToLatest();
		GetGame().GetCallqueue().CallLater(ApplyScrollToLatest, ELIFE_PhoneStyle.TICK_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ApplyScrollToLatest()
	{
		if (!m_wMessageScroll)
			return;

		m_wMessageScroll.Update();
		m_wMessageScroll.SetSliderPos(0, 1);
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
		m_wThreadPage = m_wRoot.FindAnyWidget("ThreadPage");
		m_wThreadScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("ThreadScroll"));
		m_wThreadGroups = m_wRoot.FindAnyWidget("ThreadGroups");
		m_wEmptyThreads = m_wRoot.FindAnyWidget("EmptyThreads");
		m_wMessageScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("MessageScroll"));
		m_wMessageList = m_wRoot.FindAnyWidget("MessageList");
		m_wEmptyMessages = m_wRoot.FindAnyWidget("EmptyMessages");
		m_wIndexTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle"));
		m_wThreadTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("ThreadTitle"));

		m_wComposeBar = m_wRoot.FindAnyWidget("ComposeBar");
		m_wComposeField = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("ComposeField"));
		m_wSendButton = m_wRoot.FindAnyWidget("ButtonSend");
		m_wSendDisc = ImageWidget.Cast(m_wRoot.FindAnyWidget("SendDisc"));
		m_wSendFallback = m_wRoot.FindAnyWidget("SendFallback");
		m_wSendIconSize = m_wRoot.FindAnyWidget("SendIconSize");
		m_wSendIcon = ImageWidget.Cast(m_wRoot.FindAnyWidget("SendIcon"));
		m_wSendGlyph = m_wRoot.FindAnyWidget("SendGlyph");

		//! Scoped: every button in this layout owns a "Background" widget for the hover tint.
		Widget sendFrame = m_wRoot.FindAnyWidget("SendFrame");
		if (sendFrame)
			m_wSendHover = ImageWidget.Cast(sendFrame.FindAnyWidget("Background"));

		BindCompose();

		if (!m_Phone)
			return;

		m_Phone.m_OnDataChanged.Insert(OnDataChanged);
		m_Phone.m_OnMessageSendResult.Insert(OnMessageSendResult);

		//! Draw what we already have, then refresh - the fetch lands later via OnDataChanged.
		//! Which page ends up visible is decided by Open()'s ApplySubState() call right after this.
		ReadUpdates();
		FillIndex();
		m_Phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);

		//! Names shown here come from the contacts payload, even though this screen renders no contact.
		m_Phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_CONTACTS);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		if (m_Phone)
		{
			m_Phone.m_OnDataChanged.Remove(OnDataChanged);
			m_Phone.m_OnMessageSendResult.Remove(OnMessageSendResult);
		}

		GetGame().GetCallqueue().Remove(ApplyScrollToLatest);

		m_wComposeBar = null;
		m_wComposeField = null;
		m_wSendButton = null;
		m_wSendDisc = null;
		m_wSendFallback = null;
		m_wSendHover = null;
		m_wSendIcon = null;
		m_wSendIconSize = null;
		m_wSendGlyph = null;
		m_SendClick = null;
		m_ComposeFocus = null;
		m_bSending = false;

		m_sOpenThreadId = "";
		m_sOpenNumber = "";
		m_sOpenContactId = "";
		m_aRowClicks.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDataChanged(string key)
	{
		//! Saving a contact renames every thread with that number, so this needs a re-render too.
		if (key == ELIFE_PhoneGadgetComponent.DATA_CONTACTS)
		{
			ReadUpdates();
			FillIndex();

			//! A `c:` hand-off can land before this payload does - re-resolve the number now.
			if (m_sOpenContactId != "" && m_sOpenThreadId == "")
			{
				ELIFE_ContactDto contact = ELIFE_PhoneContactBook.FindById(m_Phone, m_sOpenContactId);
				if (contact)
					m_sOpenNumber = contact.number;

				if (m_sOpenNumber != "")
				{
					ELIFE_ThreadDto existing = FindThreadWith(m_sOpenNumber);
					if (existing)
					{
						OpenThread(existing.threadId);
						return;
					}
				}
			}

			if (m_sOpenThreadId != "" || m_sOpenNumber != "" || m_sOpenContactId != "")
				FillThread();

			return;
		}

		if (key != ELIFE_PhoneGadgetComponent.DATA_MESSAGES)
			return;

		ReadUpdates();
		FillIndex();

		//! A number-addressed conversation promotes itself the moment its thread actually arrives.
		if (m_sOpenNumber != "")
		{
			ELIFE_ThreadDto arrived = FindThreadWith(m_sOpenNumber);
			if (arrived)
			{
				OpenThread(arrived.threadId);
				return;
			}
		}

		if (m_sOpenThreadId != "" || m_sOpenNumber != "" || m_sOpenContactId != "")
		{
			FillThread();
			ScrollToLatest();
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadUpdates()
	{
		m_Updates = new ELIFE_MessageUpdatesDto();

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json != "")
			m_Updates.ExpandFromRAW(json);

		//! Owner payload still has raw numbers in `from`; a bystander payload is already redacted to a
		//! name and must not go through the contact book again.
		if (m_Phone && m_Phone.IsLocalCharacterOwner())
			ResolveOwnerSenders();
	}

	//------------------------------------------------------------------------------------------------
	protected void ResolveOwnerSenders()
	{
		foreach (ELIFE_ThreadDisplayDto threadDto : m_Updates.threads)
		{
			if (!threadDto)
				continue;

			foreach (ELIFE_MessageDto message : threadDto.messages)
			{
				if (message && !message.isOutbound)
					message.from = ELIFE_PhoneContactBook.NameFor(m_Phone, message.from);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowIndex()
	{
		m_sOpenThreadId = "";
		m_sOpenNumber = "";
		m_sOpenContactId = "";

		if (m_wThreadPage)
			m_wThreadPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText(GetTitle());

		TrackScroll(m_wThreadScroll, m_wIndexTitle);

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	//! Threads arrive ordered by lastMessageAt desc, matching the bucket order, so grouping is a
	//! single pass with no re-sort needed.
	protected void FillIndex()
	{
		ApplyDataStatus(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		m_aRowClicks.Clear();
		ClearChildren(m_wThreadGroups);

		int count = m_Updates.threads.Count();

		if (m_wEmptyThreads)
			m_wEmptyThreads.SetVisible(count == 0);

		if (!m_wThreadGroups || count == 0)
			return;

		string currentBucket = "";
		Widget currentList = null;
		bool isFirstGroup = true;

		for (int i = 0; i < count; i++)
		{
			ELIFE_ThreadDto threadDto = m_Updates.threads.Get(i);

			string bucket = DateBucket(threadDto);
			if (bucket != currentBucket || !currentList)
			{
				currentBucket = bucket;
				currentList = CreateListGroup(m_wThreadGroups, bucket, isFirstGroup);
				isFirstGroup = false;
			}

			if (!currentList)
				continue;

			//! isLast is always false - these are glass cards, not the hairline-row style it applies to.
			Widget row = CreateListRow(LAYOUT_THREAD_ROW, currentList, false);
			if (!row)
				continue;

			ELIFE_PhoneStyle.ApplyGlass(row, true);

			string participants = ELIFE_PhoneContactBook.TitleFor(m_Phone, threadDto);
			SetTextAndColor(row, "ThreadParticipants", participants, ELIFE_PhoneStyle.TextPrimary());
			SetTextAndColor(row, "ThreadPreview", ThreadPreview(threadDto), ELIFE_PhoneStyle.TextSecondary());
			SetTextAndColor(row, "ThreadTime", FormatDayTime(threadDto.lastMessageAt), ELIFE_PhoneStyle.TextTertiary());
			PaintAvatar(row, "ThreadAvatarGlyph", "ThreadAvatarFill", participants);

			Widget unread = row.FindAnyWidget("ThreadUnreadSize");
			if (unread)
				unread.SetVisible(threadDto.unreadCount > 0);

			ELIFE_PhoneStyle.SetColorOf(row, "ThreadUnreadFill", m_Accent);
			SetTextAndColor(row, "ThreadUnreadCount", threadDto.unreadCount.ToString(), ELIFE_PhoneStyle.Ink());

			Widget button = row.FindAnyWidget("ThreadButton");
			if (!button)
				button = row;

			ELIFE_PhoneThreadRowClick click = new ELIFE_PhoneThreadRowClick();
			click.Bind(this, threadDto.threadId);
			button.AddHandler(click);
			m_aRowClicks.Insert(click);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! By real calendar day, not a rolling 24h window - "Yesterday" is the previous calendar date.
	protected string DateBucket(notnull ELIFE_ThreadDto threadDto)
	{
		int days = DaysSince(threadDto.lastMessageAt);

		if (days <= 0)
			return "#ELIFE-Phone_Messages_Bucket_Today";

		if (days == 1)
			return "#ELIFE-Phone_Messages_Bucket_Yesterday";

		if (days <= 7)
			return "#ELIFE-Phone_Messages_Bucket_Last7Days";

		if (days <= 14)
			return "#ELIFE-Phone_Messages_Bucket_Last14Days";

		if (days <= 30)
			return "#ELIFE-Phone_Messages_Bucket_LastMonth";

		return "#ELIFE-Phone_Messages_Bucket_Older";
	}

	//------------------------------------------------------------------------------------------------
	//! Calendar days between now and an ISO timestamp, both UTC - uses System.GetUnixTime() (real
	//! wall-clock), not the in-game day/night cycle, which runs on its own calendar.
	protected int DaysSince(string iso)
	{
		if (iso.Length() < 10)
			return 0;

		int year = iso.Substring(0, 4).ToInt();
		int month = iso.Substring(5, 2).ToInt();
		int day = iso.Substring(8, 2).ToInt();

		int nowDays = System.GetUnixTime() / 86400;
		int msgDays = DaysFromCivil(year, month, day);

		return nowDays - msgDays;
	}

	//------------------------------------------------------------------------------------------------
	//! Days since the Unix epoch, via Howard Hinnant's constant-time civil_from_days algorithm.
	protected int DaysFromCivil(int year, int month, int day)
	{
		int y = year;
		if (month <= 2)
			y -= 1;

		int era = y / 400;
		if (y < 0)
			era = (y - 399) / 400;

		int yoe = y - era * 400;

		int monthAdjusted = month - 3;
		if (month <= 2)
			monthAdjusted = month + 9;

		int doy = (153 * monthAdjusted + 2) / 5 + day - 1;
		int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

		return era * 146097 + doe - 719468;
	}

	//------------------------------------------------------------------------------------------------
	//! Real glass, not a painted strip - the conversation scrolls underneath it.
	protected void BindCompose()
	{
		ELIFE_PhoneStyle.ApplyGlass(m_wComposeBar, false, true);

		if (m_wComposeField && !m_ComposeFocus)
		{
			m_ComposeFocus = new ELIFE_PhoneComposeFocus();
			m_ComposeFocus.Bind(this);
			m_wComposeField.AddHandler(m_ComposeFocus);

			//! Enter sends - GetOnChangeFinal() fires only on commit, not on every keystroke.
			SCR_EventHandlerComponent events = SCR_EventHandlerComponent.Cast(m_wComposeField.FindHandler(SCR_EventHandlerComponent));
			if (events)
				events.GetOnChangeFinal().Insert(OnComposeConfirmed);
		}

		if (m_wSendButton && !m_SendClick)
		{
			m_SendClick = new ELIFE_PhoneSendClick();
			m_SendClick.Bind(this);
			m_wSendButton.AddHandler(m_SendClick);
		}

		Color fill = ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES);

		bool round = false;
		if (m_wSendDisc)
		{
			round = m_wSendDisc.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_DISC);
			m_wSendDisc.SetVisible(round);
			m_wSendDisc.SetColor(fill);
		}

		if (m_wSendFallback)
		{
			m_wSendFallback.SetVisible(!round);
			m_wSendFallback.SetColor(fill);
		}

		if (m_wSendHover)
			m_wSendHover.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, ICON_DISC);

		bool iconLoaded = false;
		if (m_wSendIcon)
		{
			iconLoaded = m_wSendIcon.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_CHAT, ICON_SEND);
			if (iconLoaded)
			{
				m_wSendIcon.SetColor(ELIFE_PhoneStyle.TextPrimary());

				//! A fixed Size stretches the non-square atlas cell and shifts the ink off centre.
				ELIFE_PhoneStyle.FitIcon(m_wSendIcon, 14);
			}
		}

		if (m_wSendIconSize)
			m_wSendIconSize.SetVisible(iconLoaded);

		//! Never an empty circle: the arrow glyph takes the ink if the sprite ever goes missing.
		if (m_wSendGlyph)
			m_wSendGlyph.SetVisible(!iconLoaded);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnComposeConfirmed(Widget w)
	{
		SubmitCompose();
	}

	//------------------------------------------------------------------------------------------------
	//! Reachable two ways, Enter and the button, so this is written to be safe to call twice.
	void SubmitCompose()
	{
		if (m_bSending || !m_Phone || !m_wComposeField)
			return;

		string body = m_wComposeField.GetText();
		body.TrimInPlace();

		//! An empty send is not an error worth saying anything about - there is nothing to send.
		if (body == "")
			return;

		if (body.Length() > MAX_BODY_LENGTH)
			body = body.Substring(0, MAX_BODY_LENGTH);

		string number = ComposeRecipient();
		if (number == "")
			return;

		m_bSending = true;
		SetComposeEnabled(false);

		m_Phone.SendMessage(number, body);
	}

	//------------------------------------------------------------------------------------------------
	//! A number-opened thread already knows; a real thread's recipient is whoever in it isn't this phone.
	protected string ComposeRecipient()
	{
		if (m_sOpenNumber != "")
			return m_sOpenNumber;

		ELIFE_ThreadDto threadDto = FindThread(m_sOpenThreadId);
		if (!threadDto)
			return "";

		string own = m_Phone.GetNumber();

		foreach (string participant : threadDto.participants)
		{
			if (participant != own)
				return participant;
		}

		return "";
	}

	//------------------------------------------------------------------------------------------------
	//! Field clears only once the send is confirmed - a failure hands the typed text straight back.
	protected void OnMessageSendResult(ELIFE_EMessageSendResult result, string threadId)
	{
		if (!m_wRoot || !m_bSending)
			return;

		m_bSending = false;
		SetComposeEnabled(true);

		if (result == ELIFE_EMessageSendResult.FAILED || result == ELIFE_EMessageSendResult.TOO_LONG)
			return;

		if (m_wComposeField)
			m_wComposeField.SetText("");

		//! A first message to a number creates the thread, so the page promotes to it.
		if (threadId != "" && m_sOpenThreadId == "")
			OpenThread(threadId);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetComposeEnabled(bool enabled)
	{
		if (m_wSendButton)
			m_wSendButton.SetEnabled(enabled);

		if (m_wComposeField)
			m_wComposeField.SetEnabled(enabled);
	}

	//------------------------------------------------------------------------------------------------
	protected string ThreadPreview(notnull ELIFE_ThreadDto threadDto)
	{
		int count = threadDto.messages.Count();
		if (count == 0)
			return "";

		return threadDto.messages.Get(count - 1).body;
	}

	//------------------------------------------------------------------------------------------------
	protected void FillThread()
	{
		ClearChildren(m_wMessageList);

		ELIFE_ThreadDto threadDto = FindThread(m_sOpenThreadId);

		//! A conversation opened onto a person has no thread yet - expected, not a missing thread.
		if (!threadDto && (m_sOpenNumber != "" || m_sOpenContactId != ""))
		{
			SetTextAndColor(m_wRoot, "ThreadTitle", ELIFE_PhoneContactBook.TitleForOpen(m_Phone, m_sOpenContactId, m_sOpenNumber), ELIFE_PhoneStyle.TextPrimary());
			SetText(m_wRoot, "ThreadSubtitle", "");
			SyncThreadNavTitle();

			if (m_wEmptyMessages)
				m_wEmptyMessages.SetVisible(true);

			return;
		}

		//! A refresh replaces the thread list wholesale, so the open thread can disappear under us.
		if (!threadDto)
		{
			SetText(m_wRoot, "ThreadTitle", "");
			SetText(m_wRoot, "ThreadSubtitle", "");
			SyncThreadNavTitle();

			if (m_wEmptyMessages)
				m_wEmptyMessages.SetVisible(true);

			return;
		}

		SetTextAndColor(m_wRoot, "ThreadTitle", ELIFE_PhoneContactBook.TitleFor(m_Phone, threadDto), ELIFE_PhoneStyle.TextPrimary());
		SetTextAndColor(m_wRoot, "ThreadSubtitle", FormatDayTime(threadDto.lastMessageAt), ELIFE_PhoneStyle.TextSecondary());
		SyncThreadNavTitle();

		int count = threadDto.messages.Count();

		if (m_wEmptyMessages)
			m_wEmptyMessages.SetVisible(count == 0);

		if (!m_wMessageList || count == 0)
			return;

		string you = WidgetManager.Translate("#ELIFE-Phone_Messages_You");
		string previousMeta = "";

		for (int i = 0; i < count; i++)
		{
			ELIFE_MessageDto message = threadDto.messages.Get(i);

			Widget row = CreateListRow(LAYOUT_MESSAGE_ROW, m_wMessageList);
			if (!row)
				continue;

			//! `from` is already a name (resolved in ReadUpdates() or by MessageDto.Redact()) - don't re-look it up.
			string sender = message.from;
			if (message.isOutbound)
				sender = you;

			string meta = sender + " · " + FormatClock(message.sentAt);

			//! Consecutive messages from the same sender in the same minute share one meta line.
			TextWidget metaWidget = TextWidget.Cast(row.FindAnyWidget("MessageMeta"));
			if (metaWidget)
			{
				if (meta == previousMeta)
				{
					metaWidget.SetVisible(false);
				}
				else
				{
					metaWidget.SetText(meta);
					metaWidget.SetColor(ELIFE_PhoneStyle.TextTertiary());
					LayoutSlot.SetHorizontalAlign(metaWidget, LayoutHorizontalAlign.Left);
					if (message.isOutbound)
						LayoutSlot.SetHorizontalAlign(metaWidget, LayoutHorizontalAlign.Right);
				}
			}

			previousMeta = meta;

			Widget bubbleSize = row.FindAnyWidget("BubbleSize");
			if (bubbleSize)
			{
				LayoutSlot.SetHorizontalAlign(bubbleSize, LayoutHorizontalAlign.Left);
				if (message.isOutbound)
					LayoutSlot.SetHorizontalAlign(bubbleSize, LayoutHorizontalAlign.Right);
			}

			Color bubbleFill = ELIFE_PhoneStyle.SurfaceRaised();
			Color bodyColor = ELIFE_PhoneStyle.TextPrimary();
			if (message.isOutbound)
			{
				bubbleFill = ELIFE_PhoneStyle.AccentDeepFor(EPhoneScreenState.MESSAGES);
				bodyColor = ELIFE_PhoneStyle.TextPrimary();
			}

			ELIFE_PhoneStyle.SetColorOf(row, "BubbleFill", bubbleFill);

			TextWidget body = TextWidget.Cast(row.FindAnyWidget("MessageBody"));
			if (body)
			{
				//! Only place in the phone UI with free text of unbounded length.
				body.SetTextWrapping(true);
				body.SetText(message.body);
				body.SetColor(bodyColor);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_ThreadDto FindThread(string threadId)
	{
		if (threadId == "")
			return null;

		foreach (ELIFE_ThreadDto threadDto : m_Updates.threads)
		{
			if (threadDto.threadId == threadId)
				return threadDto;
		}

		return null;
	}

}
