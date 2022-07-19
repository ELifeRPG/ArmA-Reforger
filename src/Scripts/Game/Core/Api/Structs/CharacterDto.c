class CharacterDto : JsonApiStruct
{
    string id;
    string firstName;
    string lastName;
	
	void CharacterDto()
	{
		RegV("id");
		RegV("firstName");
		RegV("lastName");
	}
}
