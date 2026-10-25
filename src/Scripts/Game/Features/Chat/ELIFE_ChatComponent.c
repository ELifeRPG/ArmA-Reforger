//------------------------------------------------------------------------------------------------
//! Local is shown only above the sender's head; the chat log would reveal who is nearby.
modded class SCR_ChatComponent
{
	//------------------------------------------------------------------------------------------------
	override void OnNewMessage(string msg, int channelId, int senderId)
	{
		BaseChatChannel channel;
		ScriptedChatEntity chatEntity = ScriptedChatEntity.Cast(GetGame().GetChat());
		if (chatEntity)
			channel = chatEntity.GetChannel(channelId);

		if (!LocalChatChannel.Cast(channel))
		{
			super.OnNewMessage(msg, channelId, senderId);
			return;
		}

		string action;
		if (ELIFE_ChatEmote.IsEmote(msg, action))
			ELIFE_ChatCaptions.Show(senderId, action, true);
		else
			ELIFE_ChatCaptions.Show(senderId, msg, false);
	}
}
