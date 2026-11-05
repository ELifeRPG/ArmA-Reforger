//------------------------------------------------------------------------------------------------
//! Asks the server for the ATM's lock. The cursor menu opens when the grant comes back, in ELIFE_ATMScreenComponent.
class ELIFE_ATMUseAction : ELIFE_WorldScreenUseAction
{
	protected ELIFE_ATMComponent m_ATM;

	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		super.Init(pOwnerEntity, pManagerComponent);

		m_ATM = ELIFE_ATMComponent.Cast(pOwnerEntity.FindComponent(ELIFE_ATMComponent));
	}

	//------------------------------------------------------------------------------------------------
	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (!CanUse(pUserEntity) || !m_ATM || IsUsedByOther())
			return;

		ELIFE_ATMCharacterComponent link = ELIFE_ATMCharacterComponent.Cast(pUserEntity.FindComponent(ELIFE_ATMCharacterComponent));
		if (link)
			link.RequestOpen(pOwnerEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Gone while the result of a withdrawal is up, so the prompt doesn't cover the screen.
	override bool CanBeShownScript(IEntity user)
	{
		return super.CanBeShownScript(user) && m_ATM && !m_ATM.IsShowingPaid();
	}

	//------------------------------------------------------------------------------------------------
	override bool CanBePerformedScript(IEntity user)
	{
		if (!CanUse(user) || !m_ATM || m_ATM.IsShowingPaid())
			return false;

		if (IsUsedByOther())
		{
			SetCannotPerformReason("#ELIFE-ATM_InUse");
			return false;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsUsedByOther()
	{
		int occupant = m_ATM.GetOccupantPlayerId();
		return occupant != 0 && occupant != SCR_PlayerController.GetLocalPlayerId();
	}
}
