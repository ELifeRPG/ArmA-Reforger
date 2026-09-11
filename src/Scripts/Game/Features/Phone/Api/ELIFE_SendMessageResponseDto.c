//------------------------------------------------------------------------------------------------
//! Response shape of POST .../apps/messages/send. The sent message itself is picked up from the
//! next messages refresh (see ELIFE_PhoneGadgetComponent.SendMessage()) rather than reconstructed
//! from this response, so there stays exactly one place that assembles a message record.
//!
//! threadId is the one field the UI does read back: a first message to a number creates the thread
//! server-side, and this is how the page that sent it learns which thread it is now looking at.
//!
//! undeliverableRecipients is a subset of the "to" that was asked for - the send itself committed,
//! but those numbers did not receive it (blocked, suspended, or no such subscriber). An empty array
//! is the normal case; a send is never partially failed, only partially delivered.
class ELIFE_SendMessageResponseDto : ELIFE_PhoneJsonDto
{
	string threadId;
	string messageId;
	ref array<string> undeliverableRecipients = {};

	void ELIFE_SendMessageResponseDto()
	{
		RegV("threadId");
		RegV("messageId");
		RegV("undeliverableRecipients");
	}
}
