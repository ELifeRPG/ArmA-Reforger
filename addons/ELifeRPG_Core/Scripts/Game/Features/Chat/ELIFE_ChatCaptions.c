//------------------------------------------------------------------------------------------------
class ELIFE_ChatCaption
{
	int m_iPlayerId;
	int m_iCreatedAtMs;
	int m_iNextLosCheckMs;
	bool m_bOccluded;
	TextWidget m_wText;
}

//------------------------------------------------------------------------------------------------
class ELIFE_ChatCaptionFilterRequest : ELIFE_ProfanityFilterRequest
{
	ELIFE_ChatCaptions m_Captions;
	int m_iPlayerId;
	bool m_bIsEmote;

	//------------------------------------------------------------------------------------------------
	override protected void OnFiltered(string text)
	{
		if (m_Captions)
			m_Captions.AddFiltered(m_iPlayerId, text, m_bIsEmote);
	}
}

//------------------------------------------------------------------------------------------------
//! Local chat above the sender's head. Chat already delivers Local only within range, so no own networking.
//! Local skips the chat log, where vanilla filters profanity, so captions are filtered here.
class ELIFE_ChatCaptions
{
	protected static const ResourceName TEXT_LAYOUT = "{3132F64DB8B4CB44}UI/layouts/WidgetLibrary/TextWidgets/Text_Body.layout";
	protected static const int LIFETIME_MS = 8000;
	protected static const int FADE_MS = 1000;
	protected static const float HEIGHT_ABOVE_EYES = 0.2;
	protected static const float MAX_DISTANCE = 30;
	protected static const int FONT_SIZE = 18;
	protected static const float OWN_CAPTION_SCREEN_Y = 0.75;
	protected static const int LOS_CHECK_INTERVAL_MS = 200;
	protected static const float LOS_HEIGHT_ABOVE_EYES = 0.05;

	protected static ref ELIFE_ChatCaptions s_Instance;

	protected ref array<ref ELIFE_ChatCaption> m_aCaptions = {};

	//! Full-screen container; a text widget placed directly on the workspace isn't drawn.
	protected Widget m_wRoot;

	//! Widgets don't survive their world; reset when it changes.
	protected BaseWorld m_World;

	protected ref array<IEntity> m_aTraceExclude = {};

	//------------------------------------------------------------------------------------------------
	static void Show(int playerId, string text, bool isEmote)
	{
		if (!s_Instance)
			s_Instance = new ELIFE_ChatCaptions();

		ELIFE_ChatCaptionFilterRequest request = new ELIFE_ChatCaptionFilterRequest();
		request.m_Captions = s_Instance;
		request.m_iPlayerId = playerId;
		request.m_bIsEmote = isEmote;
		ELIFE_ProfanityFilter.Filter(text, request);
	}

	//------------------------------------------------------------------------------------------------
	void AddFiltered(int playerId, string message, bool isEmote)
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return;

		BaseWorld world = GetGame().GetWorld();
		if (world != m_World)
		{
			m_aCaptions.Clear();
			m_wRoot = null;
			m_World = world;
		}

		RemoveCaption(playerId);

		if (!m_wRoot)
		{
			m_wRoot = workspace.CreateWidget(GameLibWidgetType.FrameWidgetTypeID, WidgetFlags.VISIBLE | WidgetFlags.IGNORE_CURSOR, null, 0);
			if (!m_wRoot)
				return;

			FrameSlot.SetAnchorMin(m_wRoot, 0, 0);
			FrameSlot.SetAnchorMax(m_wRoot, 1, 1);
			FrameSlot.SetOffsets(m_wRoot, 0, 0, 0, 0);
		}

		TextWidget text = TextWidget.Cast(workspace.CreateWidgets(TEXT_LAYOUT, m_wRoot));
		if (!text)
			return;

		if (isEmote)
			message = "* " + message;

		text.SetFlags(WidgetFlags.NO_LOCALIZATION);
		text.SetText(message);
		text.SetTextWrapping(false);
		text.SetExactFontSize(FONT_SIZE);
		text.SetShadow(2, 0xFF000000, 0.8, 1, 1);
		if (isEmote)
		{
			text.SetItalic(true);
			text.SetColorInt(ELIFE_ChatEmote.COLOR_ARGB);
		}

		FrameSlot.SetSizeToContent(text, true);
		FrameSlot.SetAlignment(text, 0.5, 1);
		text.SetVisible(false);

		ELIFE_ChatCaption caption = new ELIFE_ChatCaption();
		caption.m_iPlayerId = playerId;
		caption.m_iCreatedAtMs = System.GetTickCount();
		caption.m_wText = text;
		m_aCaptions.Insert(caption);

		// Remove first instead of a flag: the call queue is cleared when a game ends.
		GetGame().GetCallqueue().Remove(Update);
		GetGame().GetCallqueue().CallLater(Update, 0, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void RemoveCaption(int playerId)
	{
		for (int i = m_aCaptions.Count() - 1; i >= 0; i--)
		{
			if (m_aCaptions[i].m_iPlayerId != playerId)
				continue;

			if (m_aCaptions[i].m_wText)
				m_aCaptions[i].m_wText.RemoveFromHierarchy();

			m_aCaptions.Remove(i);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void Update()
	{
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		BaseWorld world = GetGame().GetWorld();
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!workspace || !world || !playerManager)
		{
			for (int i = m_aCaptions.Count() - 1; i >= 0; i--)
			{
				if (m_aCaptions[i].m_wText)
					m_aCaptions[i].m_wText.RemoveFromHierarchy();
			}

			m_aCaptions.Clear();
		}

		vector cameraMat[4];
		if (world)
			world.GetCurrentCamera(cameraMat);

		IEntity localEntity = SCR_PlayerController.GetLocalControlledEntity();
		int now = System.GetTickCount();
		for (int i = m_aCaptions.Count() - 1; i >= 0; i--)
		{
			ELIFE_ChatCaption caption = m_aCaptions[i];
			int age = now - caption.m_iCreatedAtMs;
			ChimeraCharacter character = ChimeraCharacter.Cast(playerManager.GetPlayerControlledEntity(caption.m_iPlayerId));

			if (age > LIFETIME_MS || !character || !caption.m_wText)
			{
				if (caption.m_wText)
					caption.m_wText.RemoveFromHierarchy();

				m_aCaptions.Remove(i);
				continue;
			}

			float x, y;
			bool visible;
			if (character == localEntity && !IsInThirdPerson(character))
			{
				// Own head is out of view in first person, so pin the caption on screen.
				x = workspace.DPIUnscale(workspace.GetWidth()) * 0.5;
				y = workspace.DPIUnscale(workspace.GetHeight()) * OWN_CAPTION_SCREEN_Y;
				visible = true;
			}
			else
			{
				vector worldPos = character.EyePosition() + vector.Up * HEIGHT_ABOVE_EYES;
				vector screenPos = workspace.ProjWorldToScreen(worldPos, world);
				x = screenPos[0];
				y = screenPos[1];
				visible = screenPos[2] > 0 && vector.Distance(cameraMat[3], worldPos) <= MAX_DISTANCE;
				if (visible)
					visible = !IsOccluded(caption, character, localEntity, cameraMat[3], world, now);
			}

			caption.m_wText.SetVisible(visible);
			if (!visible)
				continue;

			FrameSlot.SetPos(caption.m_wText, x, y);

			float remainingMs = LIFETIME_MS - age;
			caption.m_wText.SetOpacity(Math.Clamp(remainingMs / FADE_MS, 0, 1));
		}

		if (!m_aCaptions.IsEmpty())
			return;

		GetGame().GetCallqueue().Remove(Update);
		if (m_wRoot)
		{
			m_wRoot.RemoveFromHierarchy();
			m_wRoot = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsInThirdPerson(ChimeraCharacter character)
	{
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		return controller && controller.IsInThirdPersonView();
	}

	//------------------------------------------------------------------------------------------------
	//! Same two-pass trace as vanilla nametags (SCR_NameTagRulesetBase.TraceLOS), throttled per caption.
	protected bool IsOccluded(ELIFE_ChatCaption caption, ChimeraCharacter character, IEntity localEntity, vector cameraPos, BaseWorld world, int now)
	{
		if (now < caption.m_iNextLosCheckMs)
			return caption.m_bOccluded;

		caption.m_iNextLosCheckMs = now + LOS_CHECK_INTERVAL_MS;

		m_aTraceExclude.Clear();
		m_aTraceExclude.Insert(character);
		IEntity vehicle = CompartmentAccessComponent.GetVehicleIn(character);
		if (vehicle)
			m_aTraceExclude.Insert(vehicle);

		if (localEntity)
		{
			m_aTraceExclude.Insert(localEntity);
			IEntity localVehicle = CompartmentAccessComponent.GetVehicleIn(localEntity);
			if (localVehicle)
				m_aTraceExclude.Insert(localVehicle);
		}

		TraceParam param = new TraceParam();
		param.Start = cameraPos;
		param.End = character.EyePosition() + vector.Up * LOS_HEIGHT_ABOVE_EYES;
		param.Flags = TraceFlags.ANY_CONTACT | TraceFlags.WORLD | TraceFlags.ENTS;
		param.ExcludeArray = m_aTraceExclude;
		param.LayerMask = TRACE_LAYER_CAMERA;

		caption.m_bOccluded = world.TraceMove(param, null) < 1;
		if (caption.m_bOccluded)
		{
			param.LayerMask = EPhysicsLayerDefs.Projectile;
			caption.m_bOccluded = world.TraceMove(param, null) < 1;
		}

		return caption.m_bOccluded;
	}
}
