//------------------------------------------------------------------------------------------------
//! Announcements show only their text, without "Name: " in front.
modded class SCR_ChatMessageLineComponent
{
	//------------------------------------------------------------------------------------------------
	override void SetMessage(notnull SCR_ChatMessage msg, notnull SCR_ChatMessageStyle style)
	{
		super.SetMessage(msg, style);

		SCR_ChatMessageGeneral general = SCR_ChatMessageGeneral.Cast(msg);
		if (general && ELIFE_AnnouncementChatChannel.Cast(general.m_Channel))
			m_Widgets.m_wMessageText.SetText(msg.m_sMessage);
	}
}
