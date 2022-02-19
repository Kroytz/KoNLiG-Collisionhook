#include "extension.h"

#include "sourcehook.h"
#include "detours.h"

#include "vphysics_interface.h"
#include "ihandleentity.h"

#include "tier1/strtools.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CollisionHook g_CollisionHook;
SMEXT_LINK(&g_CollisionHook);


SH_DECL_HOOK0( IPhysics, CreateEnvironment, SH_NOATTRIB, 0 , IPhysicsEnvironment * );
SH_DECL_HOOK1_void( IPhysicsEnvironment, SetCollisionSolver, SH_NOATTRIB, 0, IPhysicsCollisionSolver * );
SH_DECL_HOOK4( IPhysicsCollisionSolver, ShouldCollide, SH_NOATTRIB, 0, int, IPhysicsObject *, IPhysicsObject *, void *, void * );


IGameConfig *g_pGameConf = NULL;
CDetour *g_pFilterDetour = NULL;

IPhysics *g_pPhysics = NULL;

IForward *g_pCollisionFwd = NULL;
IForward *g_pPassFwd = NULL;


DETOUR_DECL_STATIC2( PassServerEntityFilterFunc, bool, const IHandleEntity *, pTouch, const IHandleEntity *, pPass )
{
	// Perform required validations.
	if (!g_pPassFwd->GetFunctionCount() || (!pTouch || !pPass) || pTouch == pPass)
	{
		return DETOUR_STATIC_CALL( PassServerEntityFilterFunc )( pTouch, pPass );
	}

	CBaseEntity *pEnt1 = const_cast<CBaseEntity *>(UTIL_EntityFromEntityHandle(pTouch));
	CBaseEntity *pEnt2 = const_cast<CBaseEntity *>(UTIL_EntityFromEntityHandle(pPass));

	if (!pEnt1 || !pEnt2)
	{
		return DETOUR_STATIC_CALL(PassServerEntityFilterFunc)(pTouch, pPass);
	}

	cell_t ent1 = gamehelpers->EntityToBCompatRef(pEnt1);
	cell_t ent2 = gamehelpers->EntityToBCompatRef(pEnt2);
	
	cell_t result = 0;
	g_pPassFwd->PushCell(ent1);
	g_pPassFwd->PushCell(ent2);
	g_pPassFwd->PushCellByRef(&result);
	g_pPassFwd->Execute();

	if (result != Result_Ignore)
	{
		return result - 1;
	}

	return DETOUR_STATIC_CALL(PassServerEntityFilterFunc)(pTouch, pPass);
}

bool CollisionHook::SDK_OnLoad(char *error, size_t maxlength, bool late)
{
	char szGamedataError[256] = "";
	if (!gameconfs->LoadGameConfigFile("collisionhook.games", &g_pGameConf, szGamedataError, sizeof(szGamedataError)))
	{
		snprintf(error, maxlength, "Could not read collisionhook gamedata: %s", szGamedataError);
		return false;
	}

	CDetourManager::Init(g_pSM->GetScriptingEngine(), g_pGameConf);

	g_pFilterDetour = DETOUR_CREATE_STATIC(PassServerEntityFilterFunc, "PassServerEntityFilter");
	if (!g_pFilterDetour)
	{
		snprintf(error, maxlength, "Unable to create a detour for PassServerEntityFilter!");
		return false;
	}

	g_pFilterDetour->EnableDetour();

	g_pCollisionFwd = forwards->CreateForward("CH_ShouldCollide", ET_Hook, 3, NULL, Param_Cell, Param_Cell, Param_CellByRef );
	g_pPassFwd = forwards->CreateForward("CH_PassFilter", ET_Hook, 3, NULL, Param_Cell, Param_Cell, Param_CellByRef );
	
	sharesys->RegisterLibrary(myself, "collisionhook");

	return true;
}

void CollisionHook::SDK_OnUnload()
{
	forwards->ReleaseForward(g_pCollisionFwd);
	forwards->ReleaseForward(g_pPassFwd);

	gameconfs->CloseGameConfigFile(g_pGameConf);

	if (g_pFilterDetour)
	{
		g_pFilterDetour->Destroy();
		g_pFilterDetour = NULL;
	}
}

bool CollisionHook::SDK_OnMetamodLoad( ISmmAPI *ismm, char *error, size_t maxlen, bool late )
{
	GET_V_IFACE_CURRENT(GetPhysicsFactory, g_pPhysics, IPhysics, VPHYSICS_INTERFACE_VERSION);

	SH_ADD_HOOK(IPhysics, CreateEnvironment, g_pPhysics, SH_MEMBER(this, &CollisionHook::CreateEnvironment), false);

	return true;
}

bool CollisionHook::SDK_OnMetamodUnload(char *error, size_t maxlength)
{
	SH_REMOVE_HOOK(IPhysics, CreateEnvironment, g_pPhysics, SH_MEMBER(this, &CollisionHook::CreateEnvironment), false);

	g_pPhysics = NULL;

	return true;
}

IPhysicsEnvironment *CollisionHook::CreateEnvironment()
{
	// in order to hook IPhysicsCollisionSolver::ShouldCollide, we need to know when a solver is installed
	// in order to hook any installed solvers, we need to hook any created physics environments

	IPhysicsEnvironment *pEnvironment = SH_CALL( g_pPhysics, &IPhysics::CreateEnvironment )();

	if (!pEnvironment)
	{
		RETURN_META_VALUE( MRES_SUPERCEDE, pEnvironment ); // just in case
	}
	
	// hook so we know when a solver is installed
	SH_ADD_HOOK(IPhysicsEnvironment, SetCollisionSolver, pEnvironment, SH_MEMBER(this, &CollisionHook::SetCollisionSolver), false);

	RETURN_META_VALUE(MRES_SUPERCEDE, pEnvironment);
}

void CollisionHook::SetCollisionSolver( IPhysicsCollisionSolver *pSolver )
{
	if (!pSolver)
	{
		RETURN_META(MRES_IGNORED); // this shouldn't happen, but knowing valve...
	}

	// the game is installing a solver, hook the func we want
	SH_ADD_HOOK(IPhysicsCollisionSolver, ShouldCollide, pSolver, SH_MEMBER(this, &CollisionHook::VPhysics_ShouldCollide), false);

	RETURN_META(MRES_IGNORED);
}

int CollisionHook::VPhysics_ShouldCollide(IPhysicsObject *pObj1, IPhysicsObject *pObj2, void *pGameData1, void *pGameData2)
{
	// No plugins using 'CH_ShouldCollide' forward, therefore no need to call it.
	// Object hitting itself.
	if (!g_pCollisionFwd->GetFunctionCount() || pObj1 == pObj2)
	{
		RETURN_META_VALUE(MRES_IGNORED, 1);
	}

	CBaseEntity *pEnt1 = reinterpret_cast<CBaseEntity *>(pGameData1);
	CBaseEntity *pEnt2 = reinterpret_cast<CBaseEntity *>(pGameData2);

	// One of the entities is invalid.
	if (!pEnt1 || !pEnt2)
	{
		RETURN_META_VALUE(MRES_IGNORED, 1);
	}

	cell_t ent1 = gamehelpers->EntityToBCompatRef(pEnt1);
	cell_t ent2 = gamehelpers->EntityToBCompatRef(pEnt2);
	
	cell_t result = Result_Ignore;
	g_pCollisionFwd->PushCell(ent1);
	g_pCollisionFwd->PushCell(ent2);
	g_pCollisionFwd->PushCellByRef(&result);
	g_pCollisionFwd->Execute();
	
	// plugin wants to change the result
	if (result != Result_Ignore)
	{
		RETURN_META_VALUE(MRES_SUPERCEDE, result - 1);
	}

	// otherwise, game decides
	RETURN_META_VALUE(MRES_IGNORED, 0);
}
