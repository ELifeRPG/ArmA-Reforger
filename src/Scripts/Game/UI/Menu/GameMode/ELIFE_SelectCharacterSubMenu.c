//------------------------------------------------------------------------------------------------
class ELIFE_SelectCharacterSubMenu : SCR_RespawnSubMenuBase
{
	protected static ELIFE_SelectCharacterSubMenu s_Instance;
	protected SCR_SpinBoxComponent m_FactionSelectionSpinBox;
	protected HorizontalLayoutWidget m_wAvailableFactions;
	protected Widget m_wPlayerList;
	protected Widget m_wOptionalMessage;
	protected Widget m_wSelectFaction;
	protected Widget m_wLoadingCharacters;
	protected Widget m_wNoCharacterWarning;
	
	private ItemPreviewWidget m_renderPreview;
	private IEntity m_CurrentPreviewEntity;
	
	protected CharacterDto m_SelectedCharacter;

	protected ref map<ELIFE_CharacterMenuTile, CharacterDto> m_mAvailableCharacters = new ref map<ELIFE_CharacterMenuTile, CharacterDto>();

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

		m_wLoadingCharacters = GetRootWidget().FindAnyWidget("LoadingCharactersWarning");
		m_wNoCharacterWarning = GetRootWidget().FindAnyWidget("NoCharacterWarning");
		
		m_renderPreview = ItemPreviewWidget.Cast(GetRootWidget().FindAnyWidget("CharacterPreview"));
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
		
		ItemPreviewManagerEntity manager = GetGame().GetItemPreviewManager();
		if (!manager)
			return;
		
		ResourceName resourceName = "{693CF669B0D929B9}Prefabs/Vehicles/Wheeled/M151A2/TestVehicle.et";
		Resource res = Resource.Load(resourceName);

		if (m_CurrentPreviewEntity)
			delete m_CurrentPreviewEntity;

		m_CurrentPreviewEntity = GetGame().SpawnEntityPrefabLocal(res);

		if (m_CurrentPreviewEntity && m_renderPreview)
		{
			manager.SetPreviewItem(m_renderPreview, m_CurrentPreviewEntity);
			m_CurrentPreviewEntity.ClearFlags(EntityFlags.ACTIVE | EntityFlags.TRACEABLE, true);
			m_CurrentPreviewEntity.SetFlags(EntityFlags.NO_LINK, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuShow(SCR_SuperMenuBase parentMenu)
	{
		super.OnMenuShow(parentMenu);
		
		m_wLoadingCharacters.SetVisible(true);
		m_wNoCharacterWarning.SetVisible(false);
		GetRootWidget().FindAnyWidget("GalleryOverlay").SetVisible(false);
		
		Widget gallery = GetRootWidget().FindAnyWidget(m_sTileContainer);
		SCR_GalleryComponent gallery_component = SCR_GalleryComponent.Cast( gallery.GetHandler(0));
		gallery_component.ClearAll();
		
		TriggerInitCharacterList();
	}

	//------------------------------------------------------------------------------------------------
	protected CharacterDto GetSelectedCharacter()
	{
		if (m_TileSelection)
		{
			ELIFE_CharacterMenuTile tile = ELIFE_CharacterMenuTile.Cast(m_TileSelection.GetFocusedTile());
			
			m_SelectedCharacter = tile.m_Character;
		}

		return m_SelectedCharacter;
	}

	//------------------------------------------------------------------------------------------------
	protected void TriggerInitCharacterList()
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		
		if (playerController.m_bPlayerCharactersFound)
		{
			InitCharacterList(playerController.m_aPlayerCharacters);
		}
		else
		{
			GetGame().GetCallqueue().CallLater(TriggerInitCharacterList, 500);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	protected void InitCharacterList(array<ref CharacterDto>> characterList)
	{
		m_mAvailableCharacters.Clear();
		for (int i = 0; i < characterList.Count(); ++i)
		{
			s_bCharacterSelectionAvailable = true;
			CharacterDto character = characterList[i];

			ELIFE_CharacterMenuTile tile = ELIFE_CharacterMenuTile.InitializeTile(m_TileSelection, character);
			m_mAvailableCharacters.Set(tile, character);
			tile.m_OnClicked.Insert(HandleOnConfirm);
		}

		m_TileSelection.Init();
		
		m_wLoadingCharacters.SetVisible(false);
		m_wNoCharacterWarning.SetVisible(!s_bCharacterSelectionAvailable);
		GetRootWidget().FindAnyWidget("GalleryOverlay").SetVisible(s_bCharacterSelectionAvailable);
		Widget title = GetRootWidget().FindAnyWidget("TitleWidget");
		if (title)
			title.SetVisible(s_bCharacterSelectionAvailable);
	}

	//------------------------------------------------------------------------------------------------
	protected void UpdateCharacterTile(CharacterDto character, bool available)
	{
		SCR_DeployMenuTile characterTile = m_mAvailableCharacters.GetKeyByValue(character);
		if (characterTile)
			characterTile.ShowTile(available);
		s_bCharacterSelectionAvailable = false;
		foreach (SCR_DeployMenuTile tile, CharacterDto c : m_mAvailableCharacters)
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
		CharacterDto character = GetSelectedCharacter();
		if (character)
		{
			SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			
			playerController.m_sSelectedCharacterId = character.id;
			
			SetDeployAvailable();
			return true;
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