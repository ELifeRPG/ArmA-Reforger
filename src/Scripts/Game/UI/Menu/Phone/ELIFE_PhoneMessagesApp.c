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
class ELIFE_PhoneMessagesApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{8B7D4F2306E1A95C}UI/layouts/Menus/Phone/Apps/PhoneMessages.layout";
	protected const ResourceName LAYOUT_THREAD_ROW = "{2D9F5081C6A73B4E}UI/layouts/Menus/Phone/Apps/PhoneThreadRow.layout";
	protected const ResourceName LAYOUT_MESSAGE_ROW = "{6E48B3D9F20C517A}UI/layouts/Menus/Phone/Apps/PhoneMessageRow.layout";

	protected Widget m_wIndexPage;
	protected Widget m_wThreadPage;
	protected Widget m_wThreadScroll;
	protected Widget m_wThreadList;
	protected Widget m_wEmptyThreads;
	protected Widget m_wMessageScroll;
	protected Widget m_wMessageList;
	protected Widget m_wEmptyMessages;

	protected ref array<ref ELIFE_PhoneThreadRowClick> m_aRowClicks = {};
	protected ref ELIFE_MessageUpdatesDto m_Updates = new ELIFE_MessageUpdatesDto();
	protected string m_sOpenThreadId;

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
		if (m_sOpenThreadId != "")
		{
			ShowIndex();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override string GetSubState()
	{
		return m_sOpenThreadId;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplySubState(string subState)
	{
		if (subState == "")
			ShowIndex();
		else
			OpenThread(subState);
	}

	//------------------------------------------------------------------------------------------------
	void OpenThread(string threadId)
	{
		m_sOpenThreadId = threadId;
		FillThread();

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wThreadPage)
			m_wThreadPage.SetVisible(true);

		NotifySubStateChanged();
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
		m_wThreadScroll = m_wRoot.FindAnyWidget("ThreadScroll");
		m_wThreadList = m_wRoot.FindAnyWidget("ThreadList");
		m_wEmptyThreads = m_wRoot.FindAnyWidget("EmptyThreads");
		m_wMessageScroll = m_wRoot.FindAnyWidget("MessageScroll");
		m_wMessageList = m_wRoot.FindAnyWidget("MessageList");
		m_wEmptyMessages = m_wRoot.FindAnyWidget("EmptyMessages");

		if (!m_Phone)
			return;

		m_Phone.m_OnDataChanged.Insert(OnDataChanged);

		//! Draw what we already have, then refresh - the fetch lands later via OnDataChanged.
		//! Which page ends up visible is decided by Open()'s ApplySubState() call right after this.
		ReadUpdates();
		FillIndex();
		m_Phone.RequestData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		if (m_Phone)
			m_Phone.m_OnDataChanged.Remove(OnDataChanged);

		m_sOpenThreadId = "";
		m_aRowClicks.Clear();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnDataChanged(string key)
	{
		if (key != ELIFE_PhoneGadgetComponent.DATA_MESSAGES)
			return;

		ReadUpdates();
		FillIndex();

		if (m_sOpenThreadId != "")
			FillThread();
	}

	//------------------------------------------------------------------------------------------------
	protected void ReadUpdates()
	{
		m_Updates = new ELIFE_MessageUpdatesDto();

		string json = m_Phone.GetData(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		if (json != "")
			m_Updates.ExpandFromRAW(json);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowIndex()
	{
		m_sOpenThreadId = "";

		if (m_wThreadPage)
			m_wThreadPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(true);

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void FillIndex()
	{
		ApplyDataStatus(ELIFE_PhoneGadgetComponent.DATA_MESSAGES);
		m_aRowClicks.Clear();
		ClearChildren(m_wThreadList);

		int count = m_Updates.threads.Count();

		if (m_wEmptyThreads)
			m_wEmptyThreads.SetVisible(count == 0);

		if (m_wThreadScroll)
			m_wThreadScroll.SetVisible(count > 0);

		if (!m_wThreadList || count == 0)
			return;

		for (int i = 0; i < count; i++)
		{
			ELIFE_ThreadDto threadDto = m_Updates.threads.Get(i);

			float gap = 0;
			if (i < count - 1)
				gap = ROW_GAP;

			Widget row = CreateListRow(LAYOUT_THREAD_ROW, m_wThreadList, gap);
			if (!row)
				continue;

			string participants = JoinParticipants(threadDto);
			SetText(row, "ThreadParticipants", participants);
			SetText(row, "ThreadAvatarGlyph", Initial(participants));
			SetText(row, "ThreadPreview", FormatTimestamp(threadDto.lastMessageAt));

			Widget unread = row.FindAnyWidget("ThreadUnread");
			if (unread)
				unread.SetVisible(threadDto.unreadCount > 0);

			SetText(row, "ThreadUnreadCount", threadDto.unreadCount.ToString());

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
	protected void FillThread()
	{
		ClearChildren(m_wMessageList);

		ELIFE_ThreadDto threadDto = FindThread(m_sOpenThreadId);

		//! A refresh replaces the thread list wholesale, so the open thread can disappear under us.
		if (!threadDto)
		{
			SetText(m_wRoot, "ThreadTitle", "");
			SetText(m_wRoot, "ThreadSubtitle", "");

			if (m_wEmptyMessages)
				m_wEmptyMessages.SetVisible(true);

			if (m_wMessageScroll)
				m_wMessageScroll.SetVisible(false);

			return;
		}

		SetText(m_wRoot, "ThreadTitle", JoinParticipants(threadDto));
		SetText(m_wRoot, "ThreadSubtitle", FormatTimestamp(threadDto.lastMessageAt));

		int count = threadDto.messages.Count();

		if (m_wEmptyMessages)
			m_wEmptyMessages.SetVisible(count == 0);

		if (m_wMessageScroll)
			m_wMessageScroll.SetVisible(count > 0);

		if (!m_wMessageList || count == 0)
			return;

		Color outbound = new Color(0.133, 0.773, 0.369, 1);
		Color inbound = new Color(0.55, 0.56, 0.62, 1);

		for (int i = 0; i < count; i++)
		{
			ELIFE_MessageDto message = threadDto.messages.Get(i);

			Widget row = CreateListRow(LAYOUT_MESSAGE_ROW, m_wMessageList);
			if (!row)
				continue;

			string sender = message.from;
			if (message.isOutbound)
				sender = WidgetManager.Translate("#ELIFE-Phone_Messages_You");

			TextWidget meta = TextWidget.Cast(row.FindAnyWidget("MessageMeta"));
			if (meta)
			{
				meta.SetText(sender + " · " + FormatTimestamp(message.sentAt));
				if (message.isOutbound)
					meta.SetColor(outbound);
				else
					meta.SetColor(inbound);
			}

			TextWidget body = TextWidget.Cast(row.FindAnyWidget("MessageBody"));
			if (body)
			{
				//! Only place in the phone UI with free text of unbounded length.
				body.SetTextWrapping(true);
				body.SetText(message.body);
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

	//------------------------------------------------------------------------------------------------
	protected string JoinParticipants(notnull ELIFE_ThreadDto threadDto)
	{
		string result = "";

		foreach (string participant : threadDto.participants)
		{
			if (result != "")
				result += ", ";

			result += participant;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! "2026-08-29T14:32:10Z" -> "29.08 14:32". No timezone handling, the API sends UTC.
	protected string FormatTimestamp(string isoTimestamp)
	{
		if (isoTimestamp.Length() < 16)
			return isoTimestamp;

		return isoTimestamp.Substring(8, 2) + "." + isoTimestamp.Substring(5, 2) + " " + isoTimestamp.Substring(11, 5);
	}
}
