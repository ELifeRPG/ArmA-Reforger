//------------------------------------------------------------------------------------------------
//! Vanilla fullscreen map opened from the phone. Closing it returns to the phone.
class ELIFE_PhoneMapMenuUI : SCR_MapMenuUI
{
	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		super.OnMenuClose();

		// Deferred a tick so the map menu is off the stack before the phone menu reopens.
		GetGame().GetCallqueue().CallLater(ReopenPhoneMenuDeferred, 0, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void ReopenPhoneMenuDeferred()
	{
		IEntity character = SCR_PlayerController.GetLocalControlledEntity();
		if (!character)
			return;

		ELIFE_PhoneGadgetComponent phone = ELIFE_PhoneToggle.FindOwnedPhone(character);
		if (phone)
		{
			phone.OpenPhoneMenu();
			phone.SetScreenState(EPhoneScreenState.HOME);
		}
	}
};
