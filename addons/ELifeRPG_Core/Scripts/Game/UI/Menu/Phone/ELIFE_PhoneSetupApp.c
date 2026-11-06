//------------------------------------------------------------------------------------------------
//! First-launch setup: greeting, owner card, a tour of the real home screen, then a PIN or none.
class ELIFE_PhoneSetupApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{AD7C5828DC00EF55}UI/layouts/Menus/Phone/Apps/PhoneSetup.layout";

	//! Steps double as sub-states, so bystanders follow along. The welcome page is the root.
	protected const string STEP_WELCOME = "";
	protected const string STEP_OWNER = "owner";
	protected const string STEP_TOUR_HOLD = "basics";
	protected const string STEP_TOUR_STATUS = "basics-status";
	protected const string STEP_TOUR_HOME = "basics-home";
	protected const string STEP_PIN = "pin";
	protected const string STEP_CONFIRM = "confirm";
	protected const string STEP_NO_PIN = "pin-off";

	//! Dots across the top: welcome, owner, tour, PIN.
	protected const int STEP_COUNT = 4;
	protected const int TOUR_TIPS = 3;

	protected const string PHOTO_WELCOME = "HqcEveron";
	protected const string PHOTO_OWNER = "Briars";
	protected const string PHOTO_PIN = "Montfort";
	protected const int PHOTO_FADE_MS = 600;

	//! Within the backdrop only: the incoming photo above the outgoing one, both under scrim and fade.
	protected const int PHOTO_Z_BACK = 0;
	protected const int PHOTO_Z_FRONT = 1;
	protected const int PHOTO_Z_SCRIM = 2;

	protected const string GREETINGS = "Hello,Hallo,Ahoj,Bonjour,Hola,Ciao,Hej,Olá";
	protected const int GREETING_HOLD_MS = 2200;

	//! Tour card offsets that clear the status bar above and the home pill below.
	protected const float TOUR_EDGE_TOP = 72;
	protected const float TOUR_EDGE_BOTTOM = 56;
	protected const float TOUR_INSET = 32;
	protected const float TOUR_ARROW_SIZE = 40;

	protected const float TOUCH_FINGER_SIZE = 44;
	protected const float TOUCH_RIPPLE_SIZE = 132;
	protected const float TOUCH_CYCLE_MS = 1400;

	//! Sits the hint on the tile's icon rather than the middle of icon and label.
	protected const float TOUCH_TILE_HEIGHT_RATIO = 0.4;

	protected const string ICON_ARROW_UP = "interaction-arrow-up";
	protected const string ICON_ARROW_DOWN = "interaction-arrow-down";

	protected string m_sStep;
	protected bool m_bStarted;
	protected bool m_bEntered;

	protected Widget m_wPage;
	protected Widget m_wBackdrop;
	protected TextWidget m_wHero;
	protected TextWidget m_wTitle;
	protected TextWidget m_wBody;
	protected Widget m_wOwnerCard;
	protected Widget m_wPinHost;
	protected ref ELIFE_PhonePinFlow m_PinFlow;
	protected Widget m_wContinue;
	protected TextWidget m_wContinueLabel;
	protected ref array<Widget> m_aStepDots = {};
	protected ref ELIFE_PhoneTween m_Entrance = new ELIFE_PhoneTween();

	//! Two photo layers, crossfaded: the next photo loads into the back one and fades in on top.
	protected ref array<Widget> m_aPhotoGroups = {};
	protected ref array<ImageWidget> m_aPhotos = {};
	protected int m_iFrontPhoto;
	protected ResourceName m_sShownPhoto;
	protected ref ELIFE_PhoneTween m_PhotoTween = new ELIFE_PhoneTween();

	protected ref array<string> m_aGreetings = {};
	protected int m_iGreeting;
	protected ref ELIFE_PhoneTween m_GreetingTween = new ELIFE_PhoneTween();

	protected Widget m_wTourDim;
	protected Widget m_wTourGroup;
	protected Widget m_wTourArrowUp;
	protected Widget m_wTourArrowDown;
	protected TextWidget m_wTourCount;
	protected TextWidget m_wTourTitle;
	protected TextWidget m_wTourLine;
	protected TextWidget m_wTourNextLabel;

	protected Widget m_wTouchLayer;
	protected Widget m_wTouchRipple;
	protected Widget m_wTouchFinger;
	protected bool m_bTouchPlaced;
	protected float m_fTouchX;
	protected float m_fTouchY;
	protected float m_fTouchPhase;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_Setup_Title";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.SETUP;
	}

	//------------------------------------------------------------------------------------------------
	override string GetSubState()
	{
		return m_sStep;
	}

	//------------------------------------------------------------------------------------------------
	//! The owner's own steps echo back from the server late, so only the first one (a reopen) and
	//! bystanders follow it. The chosen PIN isn't kept across a reopen, so confirm restarts at choose.
	override void ApplySubState(string subState)
	{
		if (m_bStarted && IsOwner())
			return;

		m_bStarted = true;

		if (IsOwner() && subState == STEP_CONFIRM)
			subState = STEP_PIN;

		ShowStep(subState);
	}

	//------------------------------------------------------------------------------------------------
	override bool OnBack()
	{
		if (IsPinStep(m_sStep) && m_PinFlow && m_PinFlow.Back())
			return true;

		string previous = PreviousStep(m_sStep);
		if (previous == m_sStep)
			return false;

		GoToStep(previous);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The status bar or home pill was tapped while the tour points at it: trying it is the next step.
	void OnChromeTapped(bool statusBar)
	{
		if (!IsOwner())
			return;

		if ((statusBar && m_sStep == STEP_TOUR_STATUS) || (!statusBar && m_sStep == STEP_TOUR_HOME))
			OnContinue();
	}

	//------------------------------------------------------------------------------------------------
	void RefreshTour()
	{
		if (m_Shell)
			m_Shell.ShowSetupTour(TourTip(m_sStep) != -1, m_sStep == STEP_TOUR_HOME);
	}

	//------------------------------------------------------------------------------------------------
	protected override Widget CreateRoot(notnull Widget host)
	{
		return CreateStretched(LAYOUT, host);
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnOpened()
	{
		if (!m_wRoot)
			return;

		m_wPage = m_wRoot.FindAnyWidget("Page");
		m_wBackdrop = m_wRoot.FindAnyWidget("Backdrop");
		m_wHero = TextWidget.Cast(m_wRoot.FindAnyWidget("SetupHero"));
		m_wTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("SetupTitle"));
		m_wBody = TextWidget.Cast(m_wRoot.FindAnyWidget("SetupBody"));
		m_wOwnerCard = m_wRoot.FindAnyWidget("OwnerCard");
		m_wPinHost = m_wRoot.FindAnyWidget("PinHost");
		m_wContinue = m_wRoot.FindAnyWidget("ContinueSize");
		m_wContinueLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("ContinueLabel"));
		m_wTourDim = m_wRoot.FindAnyWidget("TourDim");
		m_wTourGroup = m_wRoot.FindAnyWidget("TourGroup");
		m_wTourArrowUp = m_wRoot.FindAnyWidget("TourArrowUpSize");
		m_wTourArrowDown = m_wRoot.FindAnyWidget("TourArrowDownSize");
		m_wTourCount = TextWidget.Cast(m_wRoot.FindAnyWidget("TourCount"));
		m_wTourTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("TourTitle"));
		m_wTourLine = TextWidget.Cast(m_wRoot.FindAnyWidget("TourLine"));
		m_wTourNextLabel = TextWidget.Cast(m_wRoot.FindAnyWidget("TourNextLabel"));
		m_wTouchLayer = m_wRoot.FindAnyWidget("TourTouchLayer");
		m_wTouchRipple = m_wRoot.FindAnyWidget("TourRipple");
		m_wTouchFinger = m_wRoot.FindAnyWidget("TourFinger");

		SetWrapping(m_wTitle);
		SetWrapping(m_wBody);
		SetWrapping(m_wTourTitle);
		SetWrapping(m_wTourLine);

		ELIFE_PhoneStyle.ApplyGlass(m_wOwnerCard, true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("ContinueChip"), false, true);
		ELIFE_PhoneStyle.ApplyGlass(m_wRoot.FindAnyWidget("TourCard"), false, true);

		BindPhotos();
		LoadSprite(m_wTouchRipple, ICON_PERSON_MARK);
		LoadSprite(m_wTouchFinger, ICON_PERSON_MARK);
		LoadArrow("TourArrowUp", ICON_ARROW_UP);
		LoadArrow("TourArrowDown", ICON_ARROW_DOWN);

		//! The home screen under the tour stays visible but must not take presses.
		if (m_wTourDim)
			m_wTourDim.ClearFlags(WidgetFlags.IGNORE_CURSOR);

		GREETINGS.Split(",", m_aGreetings, true);

		CacheDots(m_aStepDots, "StepDot", STEP_COUNT);

		if (m_wPinHost)
		{
			m_PinFlow = new ELIFE_PhonePinFlow();
			m_PinFlow.Create(m_wPinHost, IsOwner(), "#ELIFE-Phone_Setup_NoPin", "#ELIFE-Phone_Setup_NoPin_Title", "#ELIFE-Phone_Setup_NoPin_Line", "#ELIFE-Phone_Setup_NoPin", "#ELIFE-Phone_Setup_SetPin");
			m_PinFlow.m_OnStageChanged.Insert(OnPinStageChanged);
			m_PinFlow.m_OnSubmit.Insert(OnPinSubmit);
		}

		//! Nothing scrolls here, so the bars stay clear.
		TrackScroll(null, null);

		if (IsOwner())
		{
			SCR_ButtonTextComponent continueButton = SCR_ButtonTextComponent.GetButtonText("ButtonContinue", m_wRoot);
			if (continueButton)
				continueButton.m_OnClicked.Insert(OnContinue);

			SCR_ButtonTextComponent tourNextButton = SCR_ButtonTextComponent.GetButtonText("ButtonTourNext", m_wRoot);
			if (tourNextButton)
				tourNextButton.m_OnClicked.Insert(OnContinue);
		}

		if (m_Phone)
		{
			m_Phone.m_OnIdentityChanged.Insert(FillOwner);
			m_Phone.m_OnPinChangeResult.Insert(OnSetupResult);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		StopGreeting();
		StopTouchHint();
		m_Entrance.Stop();
		m_PhotoTween.Stop();

		if (m_Phone)
		{
			m_Phone.m_OnIdentityChanged.Remove(FillOwner);
			m_Phone.m_OnPinChangeResult.Remove(OnSetupResult);
		}

		if (m_PinFlow)
			m_PinFlow.Destroy();

		m_PinFlow = null;
		m_aStepDots.Clear();
		m_aPhotoGroups.Clear();
		m_aPhotos.Clear();
		m_wPage = null;
		m_wBackdrop = null;
		m_wHero = null;
		m_wTitle = null;
		m_wBody = null;
		m_wOwnerCard = null;
		m_wPinHost = null;
		m_wContinue = null;
		m_wContinueLabel = null;
		m_wTourDim = null;
		m_wTourGroup = null;
		m_wTourArrowUp = null;
		m_wTourArrowDown = null;
		m_wTourCount = null;
		m_wTourTitle = null;
		m_wTourLine = null;
		m_wTourNextLabel = null;
		m_wTouchLayer = null;
		m_wTouchRipple = null;
		m_wTouchFinger = null;
	}

	//------------------------------------------------------------------------------------------------
	// Steps
	//------------------------------------------------------------------------------------------------

	protected void GoToStep(string step)
	{
		ShowStep(step);
		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void OnContinue()
	{
		if (m_sStep == STEP_WELCOME)
			GoToStep(STEP_OWNER);
		else if (m_sStep == STEP_OWNER)
			GoToStep(STEP_TOUR_HOLD);
		else if (m_sStep == STEP_TOUR_HOLD)
			GoToStep(STEP_TOUR_STATUS);
		else if (m_sStep == STEP_TOUR_STATUS)
			GoToStep(STEP_TOUR_HOME);
		else if (m_sStep == STEP_TOUR_HOME)
			GoToStep(STEP_PIN);
	}

	//------------------------------------------------------------------------------------------------
	//! The welcome page has nothing before it, so it answers itself.
	protected string PreviousStep(string step)
	{
		if (step == STEP_OWNER)
			return STEP_WELCOME;

		if (step == STEP_TOUR_HOLD)
			return STEP_OWNER;

		if (step == STEP_TOUR_STATUS)
			return STEP_TOUR_HOLD;

		if (step == STEP_TOUR_HOME)
			return STEP_TOUR_STATUS;

		if (IsPinStep(step))
			return STEP_TOUR_HOME;

		return step;
	}

	//------------------------------------------------------------------------------------------------
	//! -1 off the tour.
	protected int TourTip(string step)
	{
		if (step == STEP_TOUR_HOLD)
			return 0;

		if (step == STEP_TOUR_STATUS)
			return 1;

		if (step == STEP_TOUR_HOME)
			return 2;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsKnownStep(string step)
	{
		return step == STEP_OWNER || TourTip(step) != -1 || IsPinStep(step);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowStep(string step)
	{
		if (!IsKnownStep(step))
			step = STEP_WELCOME;

		bool changed = step != m_sStep || !m_bEntered;
		bool wasPinStep = m_bEntered && IsPinStep(m_sStep);
		m_bEntered = true;
		m_sStep = step;

		int tip = TourTip(step);
		bool tour = tip != -1;
		bool pinStep = IsPinStep(step);

		//! The tour is the real home screen under a dim; every other step is a photo of the island.
		RefreshTour();

		SetVisible(m_wBackdrop, !tour);
		SetVisible(m_wTourDim, tour);
		SetVisible(m_wPage, !tour);
		SetVisible(m_wTourGroup, tour);

		if (tour)
			ShowTip(tip);
		else
			StopTouchHint();

		if (step == STEP_WELCOME)
		{
			ShowPhoto(PHOTO_WELCOME);
			StartGreeting();
			SetCopy("#ELIFE-Phone_Setup_Welcome_Title", "#ELIFE-Phone_Setup_Welcome_Body");
		}
		else
		{
			StopGreeting();

			if (step == STEP_OWNER)
			{
				ShowPhoto(PHOTO_OWNER);
				SetCopy("#ELIFE-Phone_Setup_Owner_Title", "#ELIFE-Phone_Setup_Owner_Body");
			}
			else if (step == STEP_CONFIRM)
			{
				ShowPhoto(PHOTO_PIN);
				SetCopy("#ELIFE-Phone_Pin_Confirm_Title", "#ELIFE-Phone_Pin_Confirm_Body");
			}
			else if (pinStep)
			{
				ShowPhoto(PHOTO_PIN);
				SetCopy("#ELIFE-Phone_Setup_Pin_Title", "#ELIFE-Phone_Setup_Pin_Body");
			}
		}

		SetVisible(m_wOwnerCard, step == STEP_OWNER);
		SetVisible(m_wPinHost, pinStep);
		SetVisible(m_wContinue, !pinStep);

		if (m_wContinueLabel)
		{
			if (step == STEP_WELCOME)
				m_wContinueLabel.SetText("#ELIFE-Phone_Setup_GetStarted");
			else
				m_wContinueLabel.SetText("#ELIFE-Phone_Setup_Continue");
		}

		if (step == STEP_OWNER)
			FillOwner();

		//! Coming in from another step always starts clean; between PIN stages the flow moved itself.
		if (pinStep && m_PinFlow && (!wasPinStep || m_PinFlow.GetStage() != StageFor(step)))
			m_PinFlow.ShowStage(StageFor(step));

		PaintStepDots();

		//! The PIN stages share a page, so only the first gets an entrance.
		if (changed && !(pinStep && wasPinStep))
		{
			if (tour)
				m_Entrance.Play(m_wTourGroup, 0, 1, ELIFE_PhoneStyle.DURATION_PRESENT_MS);
			else
				m_Entrance.Play(m_wPage, 0, 1, ELIFE_PhoneStyle.DURATION_PRESENT_MS);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void SetCopy(string title, string body)
	{
		if (m_wTitle)
			m_wTitle.SetText(title);

		if (m_wBody)
			m_wBody.SetText(body);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetVisible(Widget widget, bool visible)
	{
		if (widget)
			widget.SetVisible(visible);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetWrapping(TextWidget text)
	{
		if (text)
			text.SetTextWrapping(true);
	}

	//------------------------------------------------------------------------------------------------
	protected int StepIndex()
	{
		if (m_sStep == STEP_OWNER)
			return 1;

		if (TourTip(m_sStep) != -1)
			return 2;

		if (m_sStep == STEP_PIN || m_sStep == STEP_CONFIRM)
			return 3;

		return 0;
	}

	//------------------------------------------------------------------------------------------------
	protected void PaintStepDots()
	{
		int current = StepIndex();

		for (int i = 0; i < m_aStepDots.Count(); i++)
		{
			Color color = ELIFE_PhoneStyle.Hairline();
			if (i == current)
				color = ELIFE_PhoneStyle.TextPrimary();
			else if (i < current)
				color = ELIFE_PhoneStyle.TextSecondary();

			m_aStepDots.Get(i).SetColor(color);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Provisioning can land while this page is up, so the card fills in when it does.
	protected void FillOwner()
	{
		if (!m_wRoot || !m_Phone)
			return;

		string name = m_Phone.GetOwnerName();
		string number = m_Phone.GetNumber();
		if (number == "")
			number = WidgetManager.Translate("#ELIFE-Phone_Setup_Registering");

		SetText(m_wRoot, "OwnerNameValue", name);
		SetText(m_wRoot, "OwnerNumberValue", number);
		PaintAvatarInto(m_wRoot, "OwnerAvatarDisc", "OwnerAvatarFallback", "OwnerAvatarGlyph", name, ELIFE_PhoneStyle.AccentDeepFor(GetScreenState()));

		//! No name until identity lands; the number still reads on its own.
		SetVisible(m_wRoot.FindAnyWidget("OwnerNameRowSize"), name != "");
		SetVisible(m_wRoot.FindAnyWidget("OwnerRowHairlineSize"), name != "");
	}

	//------------------------------------------------------------------------------------------------
	// Backdrop and greeting
	//------------------------------------------------------------------------------------------------

	protected void BindPhotos()
	{
		m_aPhotoGroups.Clear();
		m_aPhotos.Clear();

		m_aPhotoGroups.Insert(m_wRoot.FindAnyWidget("PhotoAGroup"));
		m_aPhotoGroups.Insert(m_wRoot.FindAnyWidget("PhotoBGroup"));
		m_aPhotos.Insert(ImageWidget.Cast(m_wRoot.FindAnyWidget("PhotoA")));
		m_aPhotos.Insert(ImageWidget.Cast(m_wRoot.FindAnyWidget("PhotoB")));

		Widget scrim = m_wRoot.FindAnyWidget("PhotoScrim");
		if (scrim)
			scrim.SetZOrder(PHOTO_Z_SCRIM);

		//! Dark at the bottom, where the copy sits, clearing towards the top.
		Widget fadeSize = m_wRoot.FindAnyWidget("PhotoFadeSize");
		if (fadeSize)
			fadeSize.SetZOrder(PHOTO_Z_SCRIM);

		ImageWidget fade = ImageWidget.Cast(m_wRoot.FindAnyWidget("PhotoFade"));
		if (fade)
			fade.SetRotation(180);
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowPhoto(string wallpaperId)
	{
		ELIFE_PhoneWallpaper wallpaper = ELIFE_PhoneWallpapers.Find(wallpaperId);
		if (!wallpaper || wallpaper.m_sPhoto == m_sShownPhoto || m_aPhotos.Count() < 2)
			return;

		int back = 1 - m_iFrontPhoto;
		Widget group = m_aPhotoGroups.Get(back);
		ImageWidget image = m_aPhotos.Get(back);
		if (!group || !image || !image.LoadImageTexture(0, wallpaper.m_sPhoto))
			return;

		//! Scale to fill crops by the image's own size, so it must carry the texture's aspect.
		int width, height;
		image.GetImageSize(0, width, height);
		if (width > 0 && height > 0)
			image.SetSize(width, height);

		Widget front = m_aPhotoGroups.Get(m_iFrontPhoto);
		if (front)
			front.SetZOrder(PHOTO_Z_BACK);

		group.SetZOrder(PHOTO_Z_FRONT);
		group.SetVisible(true);
		m_PhotoTween.Play(group, 0, 1, PHOTO_FADE_MS);

		m_iFrontPhoto = back;
		m_sShownPhoto = wallpaper.m_sPhoto;
	}

	//------------------------------------------------------------------------------------------------
	protected void StartGreeting()
	{
		if (!m_wHero || m_aGreetings.IsEmpty())
			return;

		if (m_wHero.IsVisible())
			return;

		m_iGreeting = 0;
		m_wHero.SetText(m_aGreetings.Get(0));
		m_wHero.SetVisible(true);

		GetGame().GetCallqueue().Remove(FadeOutGreeting);
		GetGame().GetCallqueue().CallLater(FadeOutGreeting, GREETING_HOLD_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopGreeting()
	{
		GetGame().GetCallqueue().Remove(FadeOutGreeting);
		GetGame().GetCallqueue().Remove(NextGreeting);
		m_GreetingTween.Stop();

		if (!m_wHero)
			return;

		m_wHero.SetVisible(false);
		m_wHero.SetOpacity(1);
	}

	//------------------------------------------------------------------------------------------------
	protected void FadeOutGreeting()
	{
		if (!m_wHero)
			return;

		m_GreetingTween.Play(m_wHero, 1, 0, ELIFE_PhoneStyle.DURATION_STATE_MS);
		GetGame().GetCallqueue().CallLater(NextGreeting, ELIFE_PhoneStyle.DURATION_STATE_MS, false);
	}

	//------------------------------------------------------------------------------------------------
	protected void NextGreeting()
	{
		if (!m_wHero || m_aGreetings.IsEmpty())
			return;

		m_iGreeting = (m_iGreeting + 1) % m_aGreetings.Count();
		m_wHero.SetText(m_aGreetings.Get(m_iGreeting));
		m_GreetingTween.Play(m_wHero, 0, 1, ELIFE_PhoneStyle.DURATION_PRESENT_MS);
	}

	//------------------------------------------------------------------------------------------------
	// Tour
	//------------------------------------------------------------------------------------------------

	//! 0 holds a real tile, 1 points up at the status bar, 2 down at the home pill.
	protected void ShowTip(int tip)
	{
		if (m_wTourCount)
			m_wTourCount.SetTextFormat("#ELIFE-Phone_Setup_TourCount", tip + 1, TOUR_TIPS);

		if (m_wTourTitle)
			m_wTourTitle.SetText("#ELIFE-Phone_Setup_Tour" + tip.ToString() + "_Title");

		if (m_wTourLine)
			m_wTourLine.SetText("#ELIFE-Phone_Setup_Tour" + tip.ToString() + "_Line");

		if (m_wTourNextLabel)
		{
			if (tip == TOUR_TIPS - 1)
				m_wTourNextLabel.SetText("#ELIFE-Phone_Setup_GotIt");
			else
				m_wTourNextLabel.SetText("#ELIFE-Phone_Setup_Next");
		}

		//! Arrows only when there's something real to point at, and only if their sprite loaded.
		SetVisible(m_wTourArrowUp, tip == 1 && HasArrow("TourArrowUp"));
		SetVisible(m_wTourArrowDown, tip == 2 && HasArrow("TourArrowDown"));

		//! Tips 0 and 1 sit at the top, clear of the apps at the bottom of a fresh home screen.
		if (m_wTourGroup)
		{
			if (tip == 2)
			{
				AlignableSlot.SetVerticalAlign(m_wTourGroup, LayoutVerticalAlign.Bottom);
				AlignableSlot.SetPadding(m_wTourGroup, TOUR_INSET, 0, TOUR_INSET, TOUR_EDGE_BOTTOM);
			}
			else
			{
				AlignableSlot.SetVerticalAlign(m_wTourGroup, LayoutVerticalAlign.Top);
				AlignableSlot.SetPadding(m_wTourGroup, TOUR_INSET, TOUR_EDGE_TOP, TOUR_INSET, 0);
			}
		}

		if (tip == 0)
			StartTouchHint();
		else
			StopTouchHint();
	}

	//------------------------------------------------------------------------------------------------
	protected bool HasArrow(string imageName)
	{
		ImageWidget arrow = ImageWidget.Cast(m_wRoot.FindAnyWidget(imageName));
		return arrow && arrow.IsVisible();
	}

	//------------------------------------------------------------------------------------------------
	//! A wrong sprite name loads nothing without an error, so the arrow hides itself rather than leave a gap.
	protected void LoadArrow(string imageName, string sprite)
	{
		ImageWidget arrow = ImageWidget.Cast(m_wRoot.FindAnyWidget(imageName));
		if (!arrow)
			return;

		bool loaded = arrow.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, sprite);
		arrow.SetVisible(loaded);
		if (!loaded)
			return;

		arrow.SetColor(ELIFE_PhoneStyle.TextPrimary());
		ELIFE_PhoneStyle.FitIcon(arrow, TOUR_ARROW_SIZE);
	}

	//------------------------------------------------------------------------------------------------
	protected void LoadSprite(Widget widget, string sprite)
	{
		ImageWidget image = ImageWidget.Cast(widget);
		if (image)
			image.LoadImageFromSet(0, ELIFE_PhoneStyle.ICON_SET_WRAPPER, sprite);
	}

	//------------------------------------------------------------------------------------------------
	protected void StartTouchHint()
	{
		if (!m_wTouchLayer)
			return;

		m_bTouchPlaced = false;
		m_fTouchPhase = 0;
		m_wTouchLayer.SetVisible(false);

		GetGame().GetCallqueue().Remove(TickTouchHint);
		GetGame().GetCallqueue().CallLater(TickTouchHint, ELIFE_PhoneStyle.TICK_MS, true);
	}

	//------------------------------------------------------------------------------------------------
	protected void StopTouchHint()
	{
		GetGame().GetCallqueue().Remove(TickTouchHint);
		SetVisible(m_wTouchLayer, false);
	}

	//------------------------------------------------------------------------------------------------
	//! Waits for the home grid to lay out before placing the hint.
	protected void TickTouchHint()
	{
		if (!m_bTouchPlaced && !PlaceTouchHint())
			return;

		m_fTouchPhase += ELIFE_PhoneStyle.TICK_MS / TOUCH_CYCLE_MS;
		if (m_fTouchPhase >= 1)
			m_fTouchPhase -= 1;

		float eased = ELIFE_PhoneStyle.EaseOut(m_fTouchPhase);
		float size = TOUCH_FINGER_SIZE + (TOUCH_RIPPLE_SIZE - TOUCH_FINGER_SIZE) * eased;

		FrameSlot.SetPos(m_wTouchRipple, m_fTouchX - size / 2, m_fTouchY - size / 2);
		FrameSlot.SetSize(m_wTouchRipple, size, size);
		m_wTouchRipple.SetOpacity(1 - eased);
	}

	//------------------------------------------------------------------------------------------------
	//! Over the first app tile on the real home screen, in this page's own px.
	protected bool PlaceTouchHint()
	{
		if (!m_Shell || !m_wTouchLayer || !m_wTouchRipple || !m_wTouchFinger)
			return false;

		float x, y, width, height;
		if (!m_Shell.GetHomeTileRect(x, y, width, height))
			return false;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
			return false;

		//! The touch layer is hidden until placed, so measure from the root it stretches over.
		float rootX, rootY;
		m_wRoot.GetScreenPos(rootX, rootY);

		m_fTouchX = x - workspace.DPIUnscale(rootX) + width / 2;
		m_fTouchY = y - workspace.DPIUnscale(rootY) + height * TOUCH_TILE_HEIGHT_RATIO;

		FrameSlot.SetPos(m_wTouchFinger, m_fTouchX - TOUCH_FINGER_SIZE / 2, m_fTouchY - TOUCH_FINGER_SIZE / 2);
		FrameSlot.SetSize(m_wTouchFinger, TOUCH_FINGER_SIZE, TOUCH_FINGER_SIZE);

		m_wTouchLayer.SetVisible(true);
		m_bTouchPlaced = true;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	// PIN
	//------------------------------------------------------------------------------------------------

	protected ELIFE_EPinFlowStage StageFor(string step)
	{
		if (step == STEP_CONFIRM)
			return ELIFE_EPinFlowStage.CONFIRM;

		if (step == STEP_NO_PIN)
			return ELIFE_EPinFlowStage.NO_PIN;

		return ELIFE_EPinFlowStage.CHOOSE;
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsPinStep(string step)
	{
		return step == STEP_PIN || step == STEP_CONFIRM || step == STEP_NO_PIN;
	}

	//------------------------------------------------------------------------------------------------
	protected void OnPinStageChanged(ELIFE_EPinFlowStage stage)
	{
		if (stage == ELIFE_EPinFlowStage.CONFIRM)
			GoToStep(STEP_CONFIRM);
		else if (stage == ELIFE_EPinFlowStage.NO_PIN)
			GoToStep(STEP_NO_PIN);
		else
			GoToStep(STEP_PIN);
	}

	//------------------------------------------------------------------------------------------------
	//! Success needs nothing from here: the server moves the phone home, which closes this page.
	protected void OnPinSubmit(string pin)
	{
		if (m_Phone)
			m_Phone.CompleteSetup(pin);
	}

	//------------------------------------------------------------------------------------------------
	protected void OnSetupResult(ELIFE_EPinChangeResult result)
	{
		if (m_PinFlow)
			m_PinFlow.OnResult(result);
	}

	//------------------------------------------------------------------------------------------------
	override bool AcceptsPinKeys()
	{
		return IsOwner() && IsPinStep(m_sStep) && m_PinFlow && m_PinFlow.AcceptsKeys();
	}

	//------------------------------------------------------------------------------------------------
	override void PinKeyDigit(int digit)
	{
		if (AcceptsPinKeys())
			m_PinFlow.PinPush(digit);
	}

	//------------------------------------------------------------------------------------------------
	override void PinKeyBackspace()
	{
		if (AcceptsPinKeys())
			m_PinFlow.PinBackspace();
	}

	//------------------------------------------------------------------------------------------------
	protected void CacheDots(notnull array<Widget> dots, string prefix, int count)
	{
		dots.Clear();

		for (int i = 0; i < count; i++)
		{
			Widget dot = m_wRoot.FindAnyWidget(prefix + i.ToString());
			if (dot)
				dots.Insert(dot);
		}
	}

	//------------------------------------------------------------------------------------------------
	void ~ELIFE_PhoneSetupApp()
	{
		GetGame().GetCallqueue().Remove(FadeOutGreeting);
		GetGame().GetCallqueue().Remove(NextGreeting);
		GetGame().GetCallqueue().Remove(TickTouchHint);
	}
}
