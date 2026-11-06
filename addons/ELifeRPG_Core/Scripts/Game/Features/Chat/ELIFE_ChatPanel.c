//------------------------------------------------------------------------------------------------
//! Per-line expiry while closed (vanilla fades the whole history only after the newest message),
//! /me registration, unknown-command feedback, and an own style for announcements.
modded class SCR_ChatPanel
{
	protected const int ELIFE_MESSAGE_LIFETIME_MS = 8000;
	protected const int ELIFE_ANNOUNCEMENT_LIFETIME_MS = 60000;
	protected const ResourceName ELIFE_ANNOUNCEMENT_STYLE = "{5827F4331FD1D36B}Configs/Chat/ELIFE_AnnouncementChannel.conf";

	//! When the next shown line expires; 0 when none is pending.
	protected int m_iElifeNextExpiryMs;
	protected ref SCR_ChatMessageStyle m_ElifeAnnouncementStyle;

	//------------------------------------------------------------------------------------------------
	//! Both set before super, which builds the fade animator and resolves channel styles.
	override void HandlerAttached(Widget w)
	{
		m_fFadeOutThreshold_s = ELIFE_ANNOUNCEMENT_LIFETIME_MS / 1000;

		Resource styleResource = BaseContainerTools.LoadContainer(ELIFE_ANNOUNCEMENT_STYLE);
		if (styleResource && styleResource.IsValid())
			m_ElifeAnnouncementStyle = SCR_ChatMessageStyle.Cast(BaseContainerTools.CreateInstanceFromContainer(styleResource.GetResource().ToBaseContainer()));

		super.HandlerAttached(w);

		if (!SCR_Global.IsEditMode())
			ELIFE_ChatEmote.RegisterCommand();
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla matches exact channel classes and would give ours the system style.
	override protected SCR_ChatMessageStyle GetChannelStyle(BaseChatChannel channel)
	{
		if (m_ElifeAnnouncementStyle && ELIFE_AnnouncementChatChannel.Cast(channel))
			return m_ElifeAnnouncementStyle;

		return super.GetChannelStyle(channel);
	}

	//------------------------------------------------------------------------------------------------
	override void Internal_OnNewMessage(SCR_ChatMessage msg)
	{
		if (msg.m_iElifeReceivedAtMs == 0)
			msg.m_iElifeReceivedAtMs = System.GetTickCount();

		super.Internal_OnNewMessage(msg);
	}

	//------------------------------------------------------------------------------------------------
	override protected void SendMessage()
	{
		SCR_ChatPanelManager manager = SCR_ChatPanelManager.GetInstance();
		if (manager && m_Widgets.m_MessageEditBox)
		{
			string command = GetCommand(m_Widgets.m_MessageEditBox.GetText());
			if (!command.IsEmpty() && !manager.GetAllRegisteredCommands().Contains(command))
			{
				manager.ShowHelpMessage(WidgetManager.Translate("#ELIFE-Chat_UnknownCommand", command));
				return;
			}
		}

		super.SendMessage();
	}

	//------------------------------------------------------------------------------------------------
	override void OnUpdateChat(float timeSlice)
	{
		super.OnUpdateChat(timeSlice);

		if (!m_bOpen && m_iElifeNextExpiryMs > 0 && System.GetTickCount() >= m_iElifeNextExpiryMs)
			UpdateChatMessages();
	}

	//------------------------------------------------------------------------------------------------
	//! The open chat keeps showing the full history.
	override protected void UpdateChatMessages()
	{
		super.UpdateChatMessages();

		m_iElifeNextExpiryMs = 0;
		if (m_bOpen)
			return;

		array<ref SCR_ChatMessage> messages = SCR_ChatPanelManager.GetInstance().GetMessages();
		int messageCount = messages.Count();
		if (messageCount == 0)
			return;

		// Mirrors super's mapping: line 0 shows the newest message.
		int lastMessageId = m_iHistoryIdLowestMessage;
		if (m_bHistoryMode)
			lastMessageId = m_iHistoryId;

		lastMessageId = Math.ClampInt(lastMessageId, 0, messageCount - 1);

		int now = System.GetTickCount();
		int lineCount = m_aMessageLines.Count();
		for (int i = 0; i < lineCount; i++)
		{
			int messageId = lastMessageId - i;
			if (messageId < 0)
				break;

			int expiresAtMs = ELIFE_GetExpiresAtMs(messages[messageId]);
			if (now >= expiresAtMs)
				m_aMessageLines[i].SetVisible(false);
			else if (m_iElifeNextExpiryMs == 0 || expiresAtMs < m_iElifeNextExpiryMs)
				m_iElifeNextExpiryMs = expiresAtMs;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected int ELIFE_GetExpiresAtMs(SCR_ChatMessage msg)
	{
		SCR_ChatMessageGeneral general = SCR_ChatMessageGeneral.Cast(msg);
		if (general && ELIFE_AnnouncementChatChannel.Cast(general.m_Channel))
			return msg.m_iElifeReceivedAtMs + ELIFE_ANNOUNCEMENT_LIFETIME_MS;

		return msg.m_iElifeReceivedAtMs + ELIFE_MESSAGE_LIFETIME_MS;
	}
}
