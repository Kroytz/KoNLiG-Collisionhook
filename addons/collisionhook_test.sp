#include <sourcemod>
#include <collisionhook>

#pragma semicolon 1
#pragma newdecls required

public Plugin myinfo = 
{
	name = "CollisionHook - Tester", 
	author = "KoNLiG", 
	description = "Testing the functionality of CollisionHook API.", 
	version = "1.0.0", 
	url = "https://github.com/KoNLiG/Collisionhook"
};

public Action CH_ShouldCollide(int ent1, int ent2, bool &result)
{
	PrintToConsoleAll("CH_ShouldCollide: ent1: %d, ent2: %d", ent1, ent2);
	
	return Plugin_Continue;
}

public Action CH_PassFilter(int ent1, int ent2, bool &result)
{
	PrintToConsoleAll("CH_PassFilter: ent1: %d, ent2: %d", ent1, ent2);
	
	return Plugin_Continue;
}
