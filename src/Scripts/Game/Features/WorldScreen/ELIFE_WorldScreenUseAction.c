//------------------------------------------------------------------------------------------------
//! "Use screen": activates the owner's ELIFE_WorldScreenInteractComponent and opens the cursor menu bound to it.
class ELIFE_WorldScreenUseAction : ScriptedUserAction
{
	protected ELIFE_WorldScreenInteractComponent m_Screen;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_Screen = ELIFE_WorldScreenInteractComponent.Cast(pOwnerEntity.FindComponent(ELIFE_WorldScreenInteractComponent));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanUse(pUserEntity))
			return;

		m_Screen.SetActive(true);
		ELIFE_WorldScreenMenu.Open(m_Screen);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		return CanUse(user);
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		return CanUse(user);
	}

	//------------------------------------------------------------------------------------------------
	//! The menu and screen input only exist on the user's machine.
	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool CanUse(IEntity user)
	{
		if (ELIFE_PhoneMenu.GetOpen())
			return false;

		return m_Screen && user && user == SCR_PlayerController.GetLocalControlledEntity() && m_Screen.IsUsableBy(user);
	}
}
