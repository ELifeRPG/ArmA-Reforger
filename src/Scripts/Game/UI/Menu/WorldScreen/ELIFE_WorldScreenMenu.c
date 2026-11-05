//------------------------------------------------------------------------------------------------
//! Draws nothing: holds MenuContext (cursor, no gameplay input) while a world screen is in use, closes on
//! back, and gives the bound ELIFE_WorldScreenInteractComponent a root widget for its click handler.
class ELIFE_WorldScreenMenu : ChimeraMenuBase
{
	protected ELIFE_WorldScreenInteractComponent m_BoundScreen;
	protected bool m_bIsClosing;

	//! Button ids the keyboard presses. Whatever the bound screen doesn't handle on its current page, it ignores.
	protected const string KEY_DIGIT_PREFIX = "key_";
	protected const string KEY_CLEAR = "fn_clear";
	protected const string KEY_CONFIRM = "fn_confirm";

	//! Previous-frame key state, for press edges.
	protected ref array<bool> m_aDigitHeld = {false, false, false, false, false, false, false, false, false, false};
	protected bool m_bClearHeld;
	protected bool m_bConfirmHeld;
	protected ref array<int> m_aDigitKeys = {KeyCode.KC_0, KeyCode.KC_1, KeyCode.KC_2, KeyCode.KC_3, KeyCode.KC_4, KeyCode.KC_5, KeyCode.KC_6, KeyCode.KC_7, KeyCode.KC_8, KeyCode.KC_9};
	protected ref array<int> m_aNumpadKeys = {KeyCode.KC_NUMPAD0, KeyCode.KC_NUMPAD1, KeyCode.KC_NUMPAD2, KeyCode.KC_NUMPAD3, KeyCode.KC_NUMPAD4, KeyCode.KC_NUMPAD5, KeyCode.KC_NUMPAD6, KeyCode.KC_NUMPAD7, KeyCode.KC_NUMPAD8, KeyCode.KC_NUMPAD9};
	protected bool m_bKeysActive;

	//------------------------------------------------------------------------------------------------
	//! Opens the menu (or reuses the open one) bound to screen, which should already be active.
	static ELIFE_WorldScreenMenu Open(notnull ELIFE_WorldScreenInteractComponent screen)
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return null;

		ELIFE_WorldScreenMenu menu = GetOpen();
		if (!menu)
			menu = ELIFE_WorldScreenMenu.Cast(menuManager.OpenMenu(ChimeraMenuPreset.ELIFE_WorldScreenMenu));

		if (menu)
			menu.Bind(screen);

		return menu;
	}

	//------------------------------------------------------------------------------------------------
	static ELIFE_WorldScreenMenu GetOpen()
	{
		MenuManager menuManager = GetGame().GetMenuManager();
		if (!menuManager)
			return null;

		return ELIFE_WorldScreenMenu.Cast(menuManager.FindMenuByPreset(ChimeraMenuPreset.ELIFE_WorldScreenMenu));
	}

	//------------------------------------------------------------------------------------------------
	ELIFE_WorldScreenInteractComponent GetBoundScreen()
	{
		return m_BoundScreen;
	}

	//------------------------------------------------------------------------------------------------
	protected void Bind(ELIFE_WorldScreenInteractComponent screen)
	{
		if (m_BoundScreen && m_BoundScreen != screen)
			m_BoundScreen.SetActive(false);

		m_BoundScreen = screen;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuOpen()
	{
		super.OnMenuOpen();

		InputManager inputManager = GetGame().GetInputManager();
		if (!inputManager)
			return;

		inputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, OnCloseAction);
		inputManager.AddActionListener("MenuOpen", EActionTrigger.DOWN, OnCloseAction);
#ifdef WORKBENCH
		//! Not MenuBackWB: that is Backspace, which is the clear key here.
		inputManager.AddActionListener("MenuOpenWB", EActionTrigger.DOWN, OnCloseAction);
#endif
	}

	//------------------------------------------------------------------------------------------------
	//! Closes once the screen is gone (prop deleted) or no longer usable (out of range, dead).
	override void OnMenuUpdate(float tDelta)
	{
		super.OnMenuUpdate(tDelta);

		if (m_bIsClosing)
			return;

		if (!m_BoundScreen || !m_BoundScreen.IsUsableBy(SCR_PlayerController.GetLocalControlledEntity()))
		{
			CloseMenu();
			return;
		}

		PollKeys();
	}

	//------------------------------------------------------------------------------------------------
	//! Keyboard input for the bound screen, polled like the phone's PIN keys instead of using input actions.
	protected void PollKeys()
	{
		//! Keys belong to whatever is on top, such as a dialog over this menu.
		if (!IsFocused())
		{
			m_bKeysActive = false;
			return;
		}

		//! First frame only records key state, so a key held from before presses nothing.
		bool typing = m_bKeysActive;
		m_bKeysActive = true;

		for (int digit = 0; digit < 10; digit++)
		{
			bool down = Debug.KeyState(m_aDigitKeys[digit]) != 0 || Debug.KeyState(m_aNumpadKeys[digit]) != 0;
			if (typing && down && !m_aDigitHeld[digit])
				m_BoundScreen.PressButton(KEY_DIGIT_PREFIX + digit.ToString());

			m_aDigitHeld[digit] = down;
		}

		bool clearDown = Debug.KeyState(KeyCode.KC_BACK) != 0;
		if (typing && clearDown && !m_bClearHeld)
			m_BoundScreen.PressButton(KEY_CLEAR);

		m_bClearHeld = clearDown;

		bool confirmDown = Debug.KeyState(KeyCode.KC_RETURN) != 0 || Debug.KeyState(KeyCode.KC_NUMPADENTER) != 0;
		if (typing && confirmDown && !m_bConfirmHeld)
			m_BoundScreen.PressButton(KEY_CONFIRM);

		m_bConfirmHeld = confirmDown;
	}

	//------------------------------------------------------------------------------------------------
	//! Read by the bound screen every frame instead of being pushed to it, so nothing can stay held.
	bool IsInspectKeyHeld()
	{
		return IsFocused() && Debug.KeyState(KeyCode.KC_R) != 0;
	}

	//------------------------------------------------------------------------------------------------
	override void OnMenuClose()
	{
		m_bIsClosing = true;

		InputManager inputManager = GetGame().GetInputManager();
		if (inputManager)
		{
			inputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, OnCloseAction);
			inputManager.RemoveActionListener("MenuOpen", EActionTrigger.DOWN, OnCloseAction);
#ifdef WORKBENCH
			inputManager.RemoveActionListener("MenuOpenWB", EActionTrigger.DOWN, OnCloseAction);
#endif
		}

		if (m_BoundScreen)
			m_BoundScreen.SetActive(false);

		super.OnMenuClose();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnCloseAction()
	{
		CloseMenu();
	}

	//------------------------------------------------------------------------------------------------
	//! Close() is deferred, so this guards against queueing it every frame.
	void CloseMenu()
	{
		if (m_bIsClosing)
			return;

		m_bIsClosing = true;
		Close();
	}
}
