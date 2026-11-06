//------------------------------------------------------------------------------------------------
//! /me is sent through Local with this prefix. Typed "/..." text is always a command and never
//! reaches a channel, so players can't fake it.
class ELIFE_ChatEmote
{
	static const string PREFIX = "/me ";
	static const string COMMAND = "me";
	static const int COLOR_ARGB = 0xFFC2A0FF;

	//------------------------------------------------------------------------------------------------
	//! Registered through the manager's API: modding SCR_ChatPanelManager breaks its config.
	static void RegisterCommand()
	{
		SCR_ChatPanelManager manager = SCR_ChatPanelManager.GetInstance();
		if (!manager)
			return;

		// Remove first so several chat panels don't stack the handler.
		ChatCommandInvoker invoker = manager.GetCommandInvoker(COMMAND);
		invoker.Remove(OnMeCommand);
		invoker.Insert(OnMeCommand);
	}

	//------------------------------------------------------------------------------------------------
	static void OnMeCommand(SCR_ChatPanel panel, string data)
	{
		if (data.IsEmpty())
		{
			SCR_ChatPanelManager manager = SCR_ChatPanelManager.GetInstance();
			if (manager)
				manager.ShowHelpMessage("#ELIFE-Chat_MeUsage");

			return;
		}

		ScriptedChatEntity chatEntity = ScriptedChatEntity.Cast(GetGame().GetChat());
		PlayerController playerController = GetGame().GetPlayerController();
		if (!chatEntity || !playerController)
			return;

		SCR_ChatComponent chatComponent = SCR_ChatComponent.Cast(playerController.FindComponent(SCR_ChatComponent));
		if (!chatComponent)
			return;

		int channelCount = chatEntity.GetChannelsCount();
		for (int i = 0; i < channelCount; i++)
		{
			if (LocalChatChannel.Cast(chatEntity.GetChannel(i)))
			{
				chatComponent.SendMessage(PREFIX + data, i);
				return;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	static bool IsEmote(string text, out string action)
	{
		if (!text.StartsWith(PREFIX))
			return false;

		action = text.Substring(PREFIX.Length(), text.Length() - PREFIX.Length());
		return true;
	}
}
