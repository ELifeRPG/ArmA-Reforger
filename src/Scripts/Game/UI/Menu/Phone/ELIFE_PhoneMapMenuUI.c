//------------------------------------------------------------------------------------------------
//! Vanilla fullscreen map, opened from the phone's Map app. Closing it returns to the phone
//! home screen instead of dropping the player back into the game world.
class ELIFE_PhoneMapMenuUI : SCR_MapMenuUI
{
	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		super.OnMenuClose();

		// Defer by one tick so the map menu is fully removed from the MenuManager stack before
		// the phone menu opens on top of it (mirrors the same-frame guard in ELIFE_PhoneMenu.OnMapApp).
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
