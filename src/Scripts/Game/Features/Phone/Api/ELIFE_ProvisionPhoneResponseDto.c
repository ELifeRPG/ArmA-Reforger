//------------------------------------------------------------------------------------------------
//! Response shape of POST /phones - distinct from ELIFE_PhoneDto (GET /phones/{id}'s shape).
class ELIFE_ProvisionPhoneResponseDto : JsonApiStruct
{
	string phoneId;
	string number;

	void ELIFE_ProvisionPhoneResponseDto()
	{
		RegV("phoneId");
		RegV("number");
	}
}
