//------------------------------------------------------------------------------------------------
class ELIFE_SelectCharacterSubMenu : SCR_RespawnSubMenuBase
{
	protected static ELIFE_SelectCharacterSubMenu s_Instance;
	protected SCR_SpinBoxComponent m_FactionSelectionSpinBox;
	protected HorizontalLayoutWidget m_wAvailableFactions;
	protected Widget m_wPlayerList;
	protected Widget m_wOptionalMessage;
	protected Widget m_wSelectFaction;
	protected Widget m_wWarningMsg;

	protected ref map<ELIFE_CharacterMenuTile, Faction> m_mAvailableCharacters = new ref map<ELIFE_CharacterMenuTile, Faction>();

	[Attribute("PermanentFaction")]
	protected string m_sOptionalMessage;
	
	[Attribute("Tiles")]
	protected string m_sTileContainer;
	
	protected bool s_bCharacterSelectionAvailable;

	//------------------------------------------------------------------------------------------------
	override void GetWidgets()
	{
		super.GetWidgets();
		Widget tileSelection = GetRootWidget().FindAnyWidget("TileSelection");
		if (tileSelection)
			m_TileSelection = SCR_DeployMenuTileSelection.Cast(tileSelection.FindHandler(SCR_DeployMenuTileSelection));

		m_wOptionalMessage = GetRootWidget().FindAnyWidget(m_sOptionalMessage);

		m_wWarningMsg = GetRootWidget().FindAnyWidget("NoFactionWarning");
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen(SCR_SuperMenuBase parentMenu)
	{
		if (!GetGame().GetFactionManager())
		{
			Print("No faction manager present in the current world!", LogLevel.ERROR);
			return;
		}

		super.OnMenuOpen(parentMenu);

		m_sConfirmButtonText = m_sButtonTextSelectFaction;

		SCR_RespawnMenuHandlerComponent rsMenuHandler = GetRespawnMenuHandler();
		m_bIsLastAvailableTab = !rsMenuHandler.GetAllowLoadoutSelection() &&
								!rsMenuHandler.GetAllowSpawnPointSelection();

		CreateConfirmButton();
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuShow(SCR_SuperMenuBase parentMenu)
	{
		if (!GetGame().GetFactionManager())
		{
			Print("No faction manager present in the current world!", LogLevel.ERROR);
			return;
		}

		super.OnMenuShow(parentMenu);
		
		InitCharacterList();
		PlayerManager pm = GetGame().GetPlayerManager();
		SCR_RespawnComponent rc = SCR_RespawnComponent.Cast(pm.GetPlayerRespawnComponent(m_iPlayerId));
		rc.GetFactionLockInvoker().Insert(LockCharacterTiles);
		
		m_wWarningMsg.SetVisible(!s_bPlayableFactionsAvailable);
		GetRootWidget().FindAnyWidget("GalleryOverlay").SetVisible(s_bCharacterSelectionAvailable);
		Widget title = GetRootWidget().FindAnyWidget("TitleWidget");
		if (title)
			title.SetVisible(s_bCharacterSelectionAvailable);
	}

	//------------------------------------------------------------------------------------------------
	protected Faction GetSelectedCharacter()
	{
		if (m_TileSelection)
		{
			ELIFE_CharacterMenuTile tile = ELIFE_CharacterMenuTile.Cast(m_TileSelection.GetFocusedTile());
			m_SelectedFaction = m_mAvailableCharacters.Get(tile);
		}

		return m_SelectedFaction;
	}

	//------------------------------------------------------------------------------------------------
	protected void InitCharacterList()
	{
		Widget gallery = GetRootWidget().FindAnyWidget(m_sTileContainer);
		SCR_GalleryComponent gallery_component = SCR_GalleryComponent.Cast( gallery.GetHandler(0));
		gallery_component.ClearAll();	
		array<Faction> factions = {};
		int count = m_FactionManager.GetFactionsList(factions);
		s_bCharacterSelectionAvailable = false;
		for (int i = 0; i < count; ++i)
		{
			SCR_Faction faction = SCR_Faction.Cast(factions[i]);
			
			faction.GetOnFactionPlayableChanged().Insert(UpdateCharacterTile);
			
			if (!faction.IsPlayable())
				continue;

			s_bCharacterSelectionAvailable = true;
			ELIFE_CharacterMenuTile tile = ELIFE_CharacterMenuTile.InitializeTile(m_TileSelection, faction);
			m_mAvailableCharacters.Set(tile, faction);
			tile.m_OnClicked.Insert(HandleOnConfirm);
		}

		m_TileSelection.Init();
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCharacterTile(Faction faction, bool available)
	{
		SCR_DeployMenuTile factionTile = m_mAvailableCharacters.GetKeyByValue(faction);
		if (factionTile)
			factionTile.ShowTile(available);
		s_bCharacterSelectionAvailable = false;
		foreach (SCR_DeployMenuTile tile, Faction f : m_mAvailableCharacters)
		{
			if (tile && tile.IsEnabled())
			{
				s_bCharacterSelectionAvailable = true;
				break;
			}
		}

		SetQuickDeployAvailable();
	}

	//------------------------------------------------------------------------------------------------
	protected override bool ConfirmSelection()
	{
		if (GetSelectedCharacter())
		{
			SetDeployAvailable();
			return RequestFaction(GetSelectedCharacter());
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	protected void LockCharacterTiles(bool locked)
	{
		m_bButtonsUnlocked = !locked;
		m_TileSelection.SetTilesEnabled(!locked);
		SCR_RespawnSuperMenu.Cast(m_ParentMenu).SetLoadingVisible(locked);
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_SelectCharacterSubMenu GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	void ELIFE_SelectCharacterSubMenu()
	{
		s_Instance = this;
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_SelectCharacterSubMenu()
	{
		s_Instance = null;
	}
};