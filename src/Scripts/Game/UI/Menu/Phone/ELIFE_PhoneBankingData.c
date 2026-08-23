//------------------------------------------------------------------------------------------------
enum ELIFE_EPhoneBankOwnerKind
{
	PERSONAL,
	COMPANY
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneBankTransaction
{
	string m_sId;
	string m_sPostedAt;
	string m_sMemo;
	int m_iAmountCents;
}

//------------------------------------------------------------------------------------------------
class ELIFE_PhoneBankAccount
{
	string m_sId;
	string m_sName;
	ELIFE_EPhoneBankOwnerKind m_eOwnerKind;
	string m_sOwnerName;
	int m_iBalanceCents;
	ref array<ref ELIFE_PhoneBankTransaction> m_aTransactions;

	//------------------------------------------------------------------------------------------------
	void ELIFE_PhoneBankAccount()
	{
		m_aTransactions = new array<ref ELIFE_PhoneBankTransaction>();
	}
}

//------------------------------------------------------------------------------------------------
//! Placeholder ledger. Swap GetAccounts() for the HTTP API later.
class ELIFE_PhoneBankingService
{
	//------------------------------------------------------------------------------------------------
	static void GetAccounts(notnull array<ref ELIFE_PhoneBankAccount> outAccounts)
	{
		outAccounts.Clear();

		ELIFE_PhoneBankAccount everyday = new ELIFE_PhoneBankAccount();
		everyday.m_sId = "everyday";
		everyday.m_sName = "Everyday";
		everyday.m_eOwnerKind = ELIFE_EPhoneBankOwnerKind.PERSONAL;
		everyday.m_sOwnerName = "";
		everyday.m_iBalanceCents = 184520;
		everyday.m_aTransactions.Insert(Tx("e1", "18.08.89", "Morton grocer", -1860));
		everyday.m_aTransactions.Insert(Tx("e2", "16.08.89", "Wages", 125000));
		everyday.m_aTransactions.Insert(Tx("e3", "14.08.89", "St. Philippe fuel", -4200));
		everyday.m_aTransactions.Insert(Tx("e4", "12.08.89", "Cafe Le Pey", -750));
		outAccounts.Insert(everyday);

		ELIFE_PhoneBankAccount stashed = new ELIFE_PhoneBankAccount();
		stashed.m_sId = "stashed";
		stashed.m_sName = "Stashed";
		stashed.m_eOwnerKind = ELIFE_EPhoneBankOwnerKind.PERSONAL;
		stashed.m_sOwnerName = "";
		stashed.m_iBalanceCents = 620000;
		stashed.m_aTransactions.Insert(Tx("s1", "01.08.89", "From Everyday", 50000));
		stashed.m_aTransactions.Insert(Tx("s2", "01.07.89", "From Everyday", 50000));
		outAccounts.Insert(stashed);

		ELIFE_PhoneBankAccount garage = new ELIFE_PhoneBankAccount();
		garage.m_sId = "montignac-motors";
		garage.m_sName = "Montignac Motors";
		garage.m_eOwnerKind = ELIFE_EPhoneBankOwnerKind.COMPANY;
		garage.m_sOwnerName = "Montignac Motors";
		garage.m_iBalanceCents = 2410800;
		garage.m_aTransactions.Insert(Tx("m1", "17.08.89", "Parts — Levie", -89400));
		garage.m_aTransactions.Insert(Tx("m2", "15.08.89", "Job ticket 441", 320000));
		garage.m_aTransactions.Insert(Tx("m3", "11.08.89", "Workshop rent", -45000));
		outAccounts.Insert(garage);
	}

	//------------------------------------------------------------------------------------------------
	static string FormatMoney(int cents)
	{
		bool negative = cents < 0;
		if (negative)
			cents = -cents;

		int major = cents / 100;
		int minor = cents % 100;
		string minorText = minor.ToString();
		if (minor < 10)
			minorText = "0" + minorText;

		string sign = "";
		if (negative)
			sign = "-";

		return sign + InsertThousands(major) + "." + minorText;
	}

	//------------------------------------------------------------------------------------------------
	static string FormatSignedMoney(int cents)
	{
		if (cents > 0)
			return "+" + FormatMoney(cents);

		return FormatMoney(cents);
	}

	//------------------------------------------------------------------------------------------------
	protected static ELIFE_PhoneBankTransaction Tx(string id, string postedAt, string memo, int amountCents)
	{
		ELIFE_PhoneBankTransaction tx = new ELIFE_PhoneBankTransaction();
		tx.m_sId = id;
		tx.m_sPostedAt = postedAt;
		tx.m_sMemo = memo;
		tx.m_iAmountCents = amountCents;
		return tx;
	}

	//------------------------------------------------------------------------------------------------
	protected static string InsertThousands(int major)
	{
		if (major < 1000)
			return major.ToString();

		string result = "";
		while (major >= 1000)
		{
			int group = major % 1000;
			major = major / 1000;

			string groupText = group.ToString();
			if (group < 10)
				groupText = "00" + groupText;
			else if (group < 100)
				groupText = "0" + groupText;

			if (result == "")
				result = groupText;
			else
				result = groupText + " " + result;
		}

		return major.ToString() + " " + result;
	}
}
