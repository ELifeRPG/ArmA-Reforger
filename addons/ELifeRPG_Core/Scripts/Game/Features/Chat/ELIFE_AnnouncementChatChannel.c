//------------------------------------------------------------------------------------------------
//! Only admins can send, everyone receives.
class ELIFE_AnnouncementChatChannel : BaseChatChannel
{
	//------------------------------------------------------------------------------------------------
	override bool IsAvailable(BaseChatComponent sender)
	{
		return IsAdminSender(sender);
	}

	//------------------------------------------------------------------------------------------------
	//! Server re-check: IsAvailable only gates the client UI.
	override bool ProcessMessage(BaseChatComponent sender, string message, bool isAuthority)
	{
		if (isAuthority)
			return IsAdminSender(sender);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsAdminSender(BaseChatComponent sender)
	{
		if (!sender)
			return false;

		PlayerController playerController = PlayerController.Cast(sender.GetOwner());
		if (!playerController)
			return false;

		return SCR_Global.IsAdmin(playerController.GetPlayerId());
	}
}
