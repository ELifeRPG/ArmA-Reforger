//------------------------------------------------------------------------------------------------
class ELIFE_PhoneBankRowClick : ScriptedWidgetEventHandler
{
	protected ELIFE_PhoneBankingApp m_App;
	protected string m_sAccountId;

	//------------------------------------------------------------------------------------------------
	void Bind(ELIFE_PhoneBankingApp app, string accountId)
	{
		m_App = app;
		m_sAccountId = accountId;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnClick(Widget w, int x, int y, int button)
	{
		if (m_App && m_sAccountId != "")
			m_App.OpenStatement(m_sAccountId);

		return false;
	}
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneBankingApp : ELIFE_PhoneAppBase
{
	protected const ResourceName LAYOUT = "{8E4C1A27B9D05F63}UI/layouts/Menus/Phone/Apps/PhoneBanking.layout";
	protected const ResourceName LAYOUT_ACCOUNT_ROW = "{91D3B8E04A7C2F16}UI/layouts/Menus/Phone/Apps/PhoneBankAccountRow.layout";
	protected const ResourceName LAYOUT_TX_ROW = "{6B2F9C41D0E85A37}UI/layouts/Menus/Phone/Apps/PhoneBankTransactionRow.layout";

	protected Widget m_wIndexPage;
	protected Widget m_wStatementPage;
	protected ScrollLayoutWidget m_wAccountScroll;
	protected Widget m_wAccountGroups;
	protected Widget m_wEmptyAccounts;
	protected ScrollLayoutWidget m_wPostedScroll;
	protected Widget m_wPostedList;
	protected Widget m_wEmptyPosted;
	protected Widget m_wPostedLabel;
	protected TextWidget m_wIndexTitle;
	protected TextWidget m_wStatementName;
	protected TextWidget m_wStatementKind;
	protected TextWidget m_wStatementBalance;

	protected ref array<ref ELIFE_PhoneBankAccount> m_aAccounts;
	protected ref array<ref ELIFE_PhoneBankRowClick> m_aRowClicks;
	protected string m_sOpenAccountId;

	//------------------------------------------------------------------------------------------------
	override string GetTitle()
	{
		return "#ELIFE-Phone_App_Bank";
	}

	//------------------------------------------------------------------------------------------------
	override EPhoneScreenState GetScreenState()
	{
		return EPhoneScreenState.BANK;
	}

	//------------------------------------------------------------------------------------------------
	override bool OnBack()
	{
		if (m_sOpenAccountId != "")
		{
			ShowIndex();
			return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	override string GetSubState()
	{
		return m_sOpenAccountId;
	}

	//------------------------------------------------------------------------------------------------
	override void ApplySubState(string subState)
	{
		if (subState == "")
			ShowIndex();
		else
			OpenStatement(subState);
	}

	//------------------------------------------------------------------------------------------------
	void OpenStatement(string accountId)
	{
		ELIFE_PhoneBankAccount account = FindAccount(accountId);
		if (!account)
			return;

		m_sOpenAccountId = accountId;

		if (m_wStatementName)
		{
			m_wStatementName.SetText(account.m_sName);
			m_wStatementName.SetColor(ELIFE_PhoneStyle.TextPrimary());
		}

		if (m_wStatementKind)
		{
			m_wStatementKind.SetText(KindLabel(account));
			m_wStatementKind.SetColor(ELIFE_PhoneStyle.TextSecondary());
		}

		//! The balance is the one number on this page that carries the app's accent.
		if (m_wStatementBalance)
		{
			m_wStatementBalance.SetText(ELIFE_PhoneBankingService.FormatMoney(account.m_iBalanceCents));
			m_wStatementBalance.SetColor(m_Accent);
		}

		FillPosted(account);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(false);

		if (m_wStatementPage)
			m_wStatementPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText(account.m_sName);

		TrackScroll(m_wPostedScroll, m_wStatementName);

		NotifySubStateChanged();
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

		m_wIndexPage = m_wRoot.FindAnyWidget("IndexPage");
		m_wStatementPage = m_wRoot.FindAnyWidget("StatementPage");
		m_wAccountScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("AccountScroll"));
		m_wAccountGroups = m_wRoot.FindAnyWidget("AccountGroups");
		m_wEmptyAccounts = m_wRoot.FindAnyWidget("EmptyAccounts");
		m_wPostedScroll = ScrollLayoutWidget.Cast(m_wRoot.FindAnyWidget("PostedScroll"));
		m_wPostedList = m_wRoot.FindAnyWidget("PostedList");
		m_wEmptyPosted = m_wRoot.FindAnyWidget("EmptyPosted");
		m_wPostedLabel = m_wRoot.FindAnyWidget("PostedLabel");
		m_wIndexTitle = TextWidget.Cast(m_wRoot.FindAnyWidget("LargeTitle"));
		m_wStatementName = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementName"));
		m_wStatementKind = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementKind"));
		m_wStatementBalance = TextWidget.Cast(m_wRoot.FindAnyWidget("StatementBalance"));

		m_aAccounts = new array<ref ELIFE_PhoneBankAccount>();
		m_aRowClicks = new array<ref ELIFE_PhoneBankRowClick>();
		ELIFE_PhoneBankingService.GetAccounts(m_aAccounts);
		FillIndex();

		//! Actual visible page is set by Open()'s ApplySubState() call right after this, not here.
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnClosing()
	{
		m_sOpenAccountId = "";
		m_aAccounts = null;
		m_aRowClicks = null;
	}

	//------------------------------------------------------------------------------------------------
	protected void ShowIndex()
	{
		m_sOpenAccountId = "";

		if (m_wStatementPage)
			m_wStatementPage.SetVisible(false);

		if (m_wIndexPage)
			m_wIndexPage.SetVisible(true);

		if (m_wNavTitle)
			m_wNavTitle.SetText(GetTitle());

		TrackScroll(m_wAccountScroll, m_wIndexTitle);

		NotifySubStateChanged();
	}

	//------------------------------------------------------------------------------------------------
	protected void FillIndex()
	{
		if (m_aRowClicks)
			m_aRowClicks.Clear();

		ClearChildren(m_wAccountGroups);

		int count = 0;
		if (m_aAccounts)
			count = m_aAccounts.Count();

		if (m_wEmptyAccounts)
			m_wEmptyAccounts.SetVisible(count == 0);

		if (!m_wAccountGroups || count == 0)
			return;

		//! Personal and company accounts get separate groups, same CreateListGroup() path as Contacts/Messages.
		array<ref ELIFE_PhoneBankAccount> personal = {};
		array<ref ELIFE_PhoneBankAccount> company = {};

		for (int i = 0; i < count; i++)
		{
			ELIFE_PhoneBankAccount account = m_aAccounts.Get(i);
			if (!account)
				continue;

			if (account.m_eOwnerKind == ELIFE_EPhoneBankOwnerKind.COMPANY)
				company.Insert(account);
			else
				personal.Insert(account);
		}

		bool isFirst = true;

		if (!personal.IsEmpty())
		{
			FillAccountGroup(personal, "#ELIFE-Phone_Bank_Personal", isFirst);
			isFirst = false;
		}

		if (!company.IsEmpty())
			FillAccountGroup(company, "#ELIFE-Phone_Bank_Company", isFirst);
	}

	//------------------------------------------------------------------------------------------------
	protected void FillAccountGroup(notnull array<ref ELIFE_PhoneBankAccount> accounts, string header, bool isFirst)
	{
		Widget list = CreateListGroup(m_wAccountGroups, header, isFirst);
		if (!list)
			return;

		int count = accounts.Count();
		for (int i = 0; i < count; i++)
		{
			ELIFE_PhoneBankAccount account = accounts.Get(i);
			if (!account)
				continue;

			Widget row = CreateListRow(LAYOUT_ACCOUNT_ROW, list, false);
			if (!row)
				continue;

			ELIFE_PhoneStyle.ApplyGlass(row, true);

			//! Kind lives on the group header (Personal/Company) now, so it's not repeated per row.
			SetTextAndColor(row, "AccountName", account.m_sName, ELIFE_PhoneStyle.TextPrimary());
			SetTextAndColor(row, "AccountBalance", ELIFE_PhoneBankingService.FormatMoney(account.m_iBalanceCents), m_Accent);

			Widget buttonWidget = row.FindAnyWidget("AccountButton");
			if (!buttonWidget)
				buttonWidget = row;

			ELIFE_PhoneBankRowClick click = new ELIFE_PhoneBankRowClick();
			click.Bind(this, account.m_sId);
			buttonWidget.AddHandler(click);
			if (m_aRowClicks)
				m_aRowClicks.Insert(click);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected void FillPosted(notnull ELIFE_PhoneBankAccount account)
	{
		ClearChildren(m_wPostedList);

		int count = 0;
		if (account.m_aTransactions)
			count = account.m_aTransactions.Count();

		if (m_wEmptyPosted)
			m_wEmptyPosted.SetVisible(count == 0);

		if (m_wPostedLabel)
			m_wPostedLabel.SetVisible(count > 0);

		if (!m_wPostedList || count == 0)
			return;

		for (int i = 0; i < count; i++)
		{
			ELIFE_PhoneBankTransaction tx = account.m_aTransactions.Get(i);
			if (!tx)
				continue;

			Widget row = CreateListRow(LAYOUT_TX_ROW, m_wPostedList, i == count - 1);
			if (!row)
				continue;

			SetTextAndColor(row, "Memo", tx.m_sMemo, ELIFE_PhoneStyle.TextPrimary());
			SetTextAndColor(row, "PostedAt", tx.m_sPostedAt, ELIFE_PhoneStyle.TextSecondary());

			//! Direction is shown by the figure's colour only, no edge stripe down the row.
			Color amountColor = ELIFE_PhoneStyle.Positive();
			if (tx.m_iAmountCents < 0)
				amountColor = ELIFE_PhoneStyle.Negative();

			SetTextAndColor(row, "Amount", ELIFE_PhoneBankingService.FormatSignedMoney(tx.m_iAmountCents), amountColor);
		}
	}

	//------------------------------------------------------------------------------------------------
	protected ELIFE_PhoneBankAccount FindAccount(string accountId)
	{
		if (!m_aAccounts || accountId == "")
			return null;

		int i;
		int count = m_aAccounts.Count();
		for (i = 0; i < count; i++)
		{
			ELIFE_PhoneBankAccount account = m_aAccounts.Get(i);
			if (account && account.m_sId == accountId)
				return account;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	protected string KindLabel(notnull ELIFE_PhoneBankAccount account)
	{
		if (account.m_eOwnerKind == ELIFE_EPhoneBankOwnerKind.COMPANY)
		{
			string company = WidgetManager.Translate("#ELIFE-Phone_Bank_Company");

			//! Skip the owner suffix when it just repeats the account name already shown as the title.
			if (account.m_sOwnerName != "" && account.m_sOwnerName != account.m_sName)
				return company + " · " + account.m_sOwnerName;

			return company;
		}

		return WidgetManager.Translate("#ELIFE-Phone_Bank_Personal");
	}
}
