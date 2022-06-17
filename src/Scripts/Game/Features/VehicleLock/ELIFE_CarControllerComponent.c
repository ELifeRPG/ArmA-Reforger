modded class SCR_CarControllerComponent : CarControllerComponent
{
	[RplProp()]
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
	
	//------------------------------------------------------------------------------------------------
	void UpdateHasPlayerKeyToVehicle(bool hasPlayerKeyToVehicle)
	{
		Rpc(UpdateHasPlayerKeyToVehicle_Server, hasPlayerKeyToVehicle);
	}
	
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	void UpdateHasPlayerKeyToVehicle_Server(bool hasPlayerKeyToVehicle)
	{
		m_bHasPlayerKeyToVehicle = hasPlayerKeyToVehicle;
		
		Replication.BumpMe();
	}
};
