//------------------------------------------------------------------------------------------------
//! Response shape of POST .../apps/contacts/entries - only the new contact's id. The saved contact
//! itself is picked up from the next contacts refresh (see ELIFE_PhoneGadgetComponent.SaveContact()),
//! not reconstructed from this response, so there is only one place that assembles a contact record.
class ELIFE_SaveContactResponseDto : ELIFE_PhoneJsonDto
{
	string contactId;

	void ELIFE_SaveContactResponseDto()
	{
		RegV("contactId");
	}
}
