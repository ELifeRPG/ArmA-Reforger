enum EELifeDeployScreenType
{
	BRIEFING = 0,
	FACTION = 1,
	GROUP = 2,
	LOADOUT = 3,
	CHARACTER = 4,
	MAP = 5
};

//------------------------------------------------------------------------------------------------
modded class SCR_RespawnSuperMenu : SCR_SuperMenuBase
{
	//------------------------------------------------------------------------------------------------
	override void UpdateTabs()
	{
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		int selectedTab = m_TabViewComponent.GetShownTab();
		SCR_RespawnBriefingComponent briefingComponent = SCR_RespawnBriefingComponent.GetInstance();
		SCR_Faction playerFaction = SCR_Faction.Cast(m_RespawnSystemComponent.GetPlayerFaction(SCR_PlayerController.GetLocalPlayerId()));
		bool isFactionAssigned = (
			playerFaction != null
			&& playerFaction.IsPlayable()
		);
		bool isLoadoutAssigned = (m_RespawnSystemComponent.GetPlayerLoadout(SCR_PlayerController.GetLocalPlayerId()) != null);
		bool isSpawnPointAssigned = (m_RespawnSystemComponent.GetPlayerSpawnPoint(SCR_PlayerController.GetLocalPlayerId()) != null);
		bool isGroupConfirmed = true;
		if (groupsManager)
			isGroupConfirmed = groupsManager.GetConfirmedByPlayer();
		
		// enable individual submenu tabs based on gamemode settings:
		m_TabViewComponent.SetTabVisible(EELifeDeployScreenType.BRIEFING, briefingComponent && briefingComponent.GetInfo()); 

	 	m_TabViewComponent.EnableTab(EELifeDeployScreenType.FACTION,
	 		(m_RespawnMenuHandler.GetAllowFactionSelection()
	 		&& (m_RespawnMenuHandler.GetAllowFactionChange() || !isFactionAssigned))
	 	);

		// Disable group selection tab
		m_TabViewComponent.EnableTab(EELifeDeployScreenType.GROUP, false);
		m_TabViewComponent.SetTabVisible(EELifeDeployScreenType.GROUP, false);

		// Only add tab if multiple loadouts possible
		m_TabViewComponent.EnableTab(EELifeDeployScreenType.LOADOUT,
			(isFactionAssigned
			&& m_RespawnMenuHandler.GetAllowLoadoutSelection())
		);
		m_TabViewComponent.SetTabVisible(EELifeDeployScreenType.LOADOUT, m_RespawnMenuHandler.GetAllowLoadoutSelection());
		
		bool showCharacter = (
			m_RespawnMenuHandler.GetAllowSpawnPointSelection()
			&& isFactionAssigned
			&& (isLoadoutAssigned || !m_RespawnMenuHandler.GetAllowLoadoutSelection())
		);
		
		m_TabViewComponent.EnableTab(EELifeDeployScreenType.CHARACTER, showCharacter);

		bool showMap = (
			m_RespawnMenuHandler.GetAllowSpawnPointSelection()
			&& isFactionAssigned
			&& (isLoadoutAssigned || !m_RespawnMenuHandler.GetAllowLoadoutSelection())
		);

		m_TabViewComponent.EnableTab(EELifeDeployScreenType.MAP, showMap);

		int nextTab = m_TabViewComponent.GetNextValidItem(false);
		if (m_TabViewComponent.IsTabEnabled(nextTab))
			selectedTab = nextTab;

		// switch to the first enabled tab
		if (showMap)
			selectedTab = EELifeDeployScreenType.MAP;
		if (showCharacter)
			selectedTab = EELifeDeployScreenType.CHARACTER;
		else if (!m_TabViewComponent.IsTabEnabled(selectedTab))
			selectedTab = m_TabViewComponent.GetNextValidItem(false);

		if (selectedTab > -1) // todo(hajekmar): should this be handled in ShowTab() instead?
			m_TabViewComponent.ShowTab(selectedTab, true, false);
	}

	//------------------------------------------------------------------------------------------------
	override void HandleOnFactionAssigned(int playerId, Faction faction)
	{
		if (playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			if (!faction)
				m_TabViewComponent.ShowTab(EELifeDeployScreenType.FACTION);
			UpdateTabs();
		}

		SCR_SelectFactionSubMenu menu = SCR_SelectFactionSubMenu.GetInstance();
		if (menu)
		{
			menu.HandleOnFactionAssigned(playerId, faction);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void HandleOnLoadoutAssigned(int playerId, SCR_BasePlayerLoadout loadout)
	{
		if (playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			if (!loadout)
				m_TabViewComponent.ShowTab(EELifeDeployScreenType.FACTION);
			UpdateTabs();
		}

		SCR_SelectLoadoutSubMenu menu = SCR_SelectLoadoutSubMenu.GetInstance();
		if (menu)
		{
			menu.HandleOnLoadoutAssigned(playerId, loadout);
		}
	}
};
