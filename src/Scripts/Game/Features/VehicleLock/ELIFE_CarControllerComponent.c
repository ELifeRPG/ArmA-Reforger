modded class SCR_CarControllerComponent : CarControllerComponent
{
	bool m_bHasPlayerKeyToVehicle = false;
	
	//------------------------------------------------------------------------------------------------
	//! Return if engine starter is functional
	override bool IsStarterFunctional()
	{
		if (GetEngineDamageState() == EDamageState.DESTROYED)
			return false;
		
		if (m_bFirstRun)
		{
			m_bFirstRun = false;
			m_pPowerComponent = SCR_PowerComponent.Cast(GetOwner().FindComponent(SCR_PowerComponent));
		}
		
		// TODO: Option to start engine despite lack of power if vehicle is moving
		if (m_pPowerComponent && !m_pPowerComponent.HasPower())
			return false;
		
		return m_bHasPlayerKeyToVehicle;
	}
};
