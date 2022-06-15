modded class SCR_EngineAction : SCR_VehicleActionBase
{
	protected CarControllerComponent m_pCarController;
	
	protected SCR_CarControllerComponent m_pScrCarController;
	
	protected ELIFE_VehicleLockComponent m_pVehicleLockComponent;
	
	//------------------------------------------------------------------------------------------------
	override void Init(IEntity pOwnerEntity, GenericComponent pManagerComponent)
	{
		m_pCarController = CarControllerComponent.Cast(pOwnerEntity.FindComponent(CarControllerComponent));
		m_pScrCarController = SCR_CarControllerComponent.Cast(pOwnerEntity.FindComponent(SCR_CarControllerComponent));
		m_pVehicleLockComponent = ELIFE_VehicleLockComponent.Cast(pOwnerEntity.FindComponent(ELIFE_VehicleLockComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	override bool CanBeShownScript(IEntity user)
	{
		bool playerHasKeyToCar = true;
		
		if (m_pVehicleLockComponent)
		{
			playerHasKeyToCar = ELIFE_VehicleLockUtils.HasPlayerKeyToCar(user, m_pVehicleLockComponent.m_sVehicleIdentifier);
			m_pScrCarController.m_bHasPlayerKeyToVehicle = playerHasKeyToCar;
		}
		
		return playerHasKeyToCar && CanBePerformedScript(user) && super.CanBeShownScript(user);
	}
};