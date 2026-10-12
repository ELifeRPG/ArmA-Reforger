//------------------------------------------------------------------------------------------------
//! Money formatting for the Banking app and the home balance card. Amounts arrive from the Bridge as floats.
class ELIFE_PhoneBankingService
{
	//------------------------------------------------------------------------------------------------
	static int ToCents(float amount)
	{
		return Math.Round(amount * 100);
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

		//! Unit suffixed, not prefixed, so the sign stays leading and figures keep a common left edge.
		return sign + InsertThousands(major) + "." + minorText + " " + ELIFE_PhoneStyle.CURRENCY_SYMBOL;
	}

	//------------------------------------------------------------------------------------------------
	static string FormatSignedMoney(int cents)
	{
		if (cents > 0)
			return "+" + FormatMoney(cents);

		return FormatMoney(cents);
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
