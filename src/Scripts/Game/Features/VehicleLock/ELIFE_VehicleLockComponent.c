class ELIFE_VehicleLockComponentClass : ScriptComponentClass
{
	// prefab properties here
}

//------------------------------------------------------------------------------------------------
/*!
	Class generated via ScriptWizard.
*/
class ELIFE_VehicleLockComponent : SCR_BaseLockComponent
{
	[Attribute("")]
	string m_sDebugIdentifier;
	
    [Attribute("{89DCD5C00091FF04}Prefabs/Items/Keys/CarKey.et", UIWidgets.ResourceNamePicker, desc: "Vehicle key prefab")]
	ResourceName m_KeyPrefab;
	
    [Attribute("{5A15B897792C7099}Sounds/Vehicles/Vehicle_LockUnlock.acp", UIWidgets.ResourceNamePicker, desc: "Lock/Unlock sound project (acp)")]
	protected ResourceName m_LockUnlockSoundProject;
	
	
	[RplProp()]
	protected string m_sVehicleIdentifier;
	
	[RplProp(onRplName: "OnIsVehicleLockedUpdated")]
	protected bool m_bIsVehicleLocked = true;
	
	
	protected AudioHandle m_AudioHandle;
	private IEntity m_Vehicle;
	
	//------------------------------------------------------------------------------------------------
	bool IsVehicleLocked()
	{
		return m_bIsVehicleLocked;
	}
	
	//------------------------------------------------------------------------------------------------
	string VehicleIdentifier()
	{
		return m_sVehicleIdentifier;
	}
	
	//------------------------------------------------------------------------------------------------
	void SwitchLockedState(bool locked)
	{
		Print("CLIENT | New Locked state from client: " + locked);
		Rpc(RpcAsk_Authority_SwitchVehicleLockedState, locked);
	}
		
	//------------------------------------------------------------------------------------------------
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_Authority_SwitchVehicleLockedState(bool locked)
	{
		Print("SERVER | Replication.IsServer(): " + Replication.IsServer());
		if (!Replication.IsServer())
			return;
		
		Print("SERVER | m_bIsVehicleLocked == locked: " + (m_bIsVehicleLocked == locked));
		if (m_bIsVehicleLocked == locked)	
			return;
		
		m_bIsVehicleLocked = locked;
		
		Print("SERVER | BUMP: m_bIsVehicleLocked: " + m_bIsVehicleLocked);
		Replication.BumpMe();
	}
	
	//------------------------------------------------------------------------------------------------
	protected void OnIsVehicleLockedUpdated()
	{
		Print("CLIENT | OnIsVehicleLockedUpdated");
		if (!AudioSystem.IsSoundPlayed(m_AudioHandle))
			AudioSystem.TerminateSound(m_AudioHandle);
		
		// Play sound		
		string eventName = "";
		
		if (m_bIsVehicleLocked)
		{
			eventName = "SOUND_VEHICLE_UNLOCK";
		}
		else
		{
			eventName = "SOUND_VEHICLE_LOCK";
		}
		
		ref array<string> signalName = new array<string>;
		ref array<float> signalValue = new array<float>;
		vector mat[4];
		
		mat[3] = m_Vehicle.GetOrigin();
		
		m_AudioHandle = AudioSystem.PlayEvent(m_LockUnlockSoundProject, eventName, mat, signalName, signalValue);
	}
	
	//------------------------------------------------------------------------------------------------
	override bool IsLocked(IEntity user, BaseCompartmentSlot compartmentSlot)
	{
		// Check base lock state
		if (user && compartmentSlot)
			if (super.IsLocked(user, compartmentSlot))
				return true;
		
		return m_bIsVehicleLocked;
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		m_Vehicle = owner;
		
		super.EOnInit(owner);
	}

	//------------------------------------------------------------------------------------------------
	void ELIFE_VehicleLockComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
	{
		if (!Replication.IsServer())
			return;
		
		if(m_sDebugIdentifier != "")
		{
			m_sVehicleIdentifier = m_sDebugIdentifier;
			Replication.BumpMe();
		}
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_VehicleLockComponent()
	{
		// Terminate sound
		AudioSystem.TerminateSound(m_AudioHandle);
	}

}
