modded class SCR_EngineAction : SCR_VehicleActionBase
{
	protected CarControllerComponent m_pCarController;
	
	protected ELIFE_VehicleLockComponent m_pVehicleLockComponent;
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_pCarController = CarControllerComponent.Cast(pOwnerEntity.FindComponent(CarControllerComponent));
		m_pVehicleLockComponent = ELIFE_VehicleLockComponent.Cast(pOwnerEntity.FindComponent(ELIFE_VehicleLockComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		bool playerHasKeyToCar = false;
		
		if (m_pVehicleLockComponent)
		{
			playerHasKeyToCar = ELIFE_VehicleLockUtils.HasPlayerKeyToCar(user, m_pVehicleLockComponent.VehicleIdentifier(), m_pVehicleLockComponent.m_KeyPrefab);
		}
		
		return playerHasKeyToCar && CanBePerformedScript(user) && super.CanBeShownScript(user);
	}
};