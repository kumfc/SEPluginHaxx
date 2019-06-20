#include <stdio.h>
#include <windows.h>
#include <iostream>
#include <cstdint>
#include <map>
#include <thread>
#include "D:/csgo/Detours Pro v3.0.316/include/detours.h"


#include "interface.h"
#include "filesystem.h"
#include "engine/iserverplugin.h"
#include "eiface.h"
#include "baseclient.h"
#include "igameevents.h"
#include "glow_outline_effect.h"
#include "convar.h"
#include "shareddefs.h"
#include "Color.h"
#include "vstdlib/random.h"
#include "engine/IEngineTrace.h"
#include "tier2/tier2.h"
#include "tier3/tier3.h"
#include "game/server/pluginvariant.h"
#include "game/server/iplayerinfo.h"
#include "game/server/ientityinfo.h"
#include "game/server/igameinfo.h"
#include "icliententitylist.h"
#include "client_class.h"
#include "icliententity.h"
#include "usercmd.h"
#include "wellshitted.h"
#include <sstream>

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CGlobalVars *gpGlobals = NULL;

////////////////////////////////////////////////////
// IMPORTANT STUFF//////////////////////////////////
////////////////////////////////////////////////////
// 0x31E0B0 CClientState (engine.dll)
class INetChannel
{
public:
	char pad0000[0x18];
	__int32 m_nOutSequenceNr; //0x0018 
	__int32 m_nInSequenceNr; //0x001C 
	__int32 m_nOutSequenceNrAck; //0x0020 
	__int32 m_nOutReliableState; //0x0024 
	__int32 m_nInReliableState; //0x0028 
	__int32 m_nChokedPackets; //0x002C 

};
class CClientState
{
public:
	char pad0000[0x94];
	INetChannel* m_NetChannel; // 0x0094
	//int m_nSignonState; // 0x00E8
	//int m_nPlayerSlot; // 0x0160
	//char m_szLevelName[40]; // 0x0168
	//char m_szLevelNameShort[40]; // 0x0268
	//int m_nMaxClients; // 0x02F0
	//int lastoutgoingcommand; // 0x4C7C
	//int chokedcommands; // 0x4C80
	//QAngle viewangles; // 0x4CE0

};

class R_BaseEntity
{
public:
	char pad_0x0000[0xA4]; //0x0000
	__int8 lifeState; //0x00A4 
	char pad_0x00A5[0x3]; //0x00A5
	__int32 m_iHealth; //0x00A8 
	float speed; //0x00AC 
	__int32 teamNumber; //0x00B0 
	char pad_0x00B4[0xB64]; //0x00B4

}; //Size=0x0C18
//////////////////////////////
typedef void(__thiscall* o_Connect) (INetChannel *pThis);
CreateInterfaceFn clientCreateInterface = (CreateInterfaceFn)GetProcAddress(GetModuleHandle("client.dll"), "CreateInterface");
IVEngineClient *pEngine;
IClientEntityList *pEntities;
CClientState *pClientState;
IEngineTrace *pEngineTrace;
////////////////////////////////////////////////////

//---------------------------------------------------------------------------------
// Purpose: a sample 3rd party plugin class
//---------------------------------------------------------------------------------
class CEmptyServerPlugin: public IServerPluginCallbacks, public IGameEventListener
{
public:
	CEmptyServerPlugin();
	~CEmptyServerPlugin();

	virtual bool			Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory );
	virtual void			Unload( void );
	virtual void			Pause( void );
	virtual void			UnPause( void );
	virtual const char     *GetPluginDescription( void );      
	virtual void			LevelInit( char const *pMapName );
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
	virtual void			GameFrame( bool simulating );
	virtual void			LevelShutdown( void );
	virtual void			ClientActive( edict_t *pEntity );
	virtual void			ClientDisconnect( edict_t *pEntity );
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername );
	virtual void			SetCommandClient( int index );
	virtual void			ClientSettingsChanged( edict_t *pEdict );
	virtual PLUGIN_RESULT	ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual PLUGIN_RESULT	ClientCommand( edict_t *pEntity, const CCommand &args );
	virtual PLUGIN_RESULT	NetworkIDValidated( const char *pszUserName, const char *pszNetworkID );
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue );
	virtual void			OnEdictAllocated( edict_t *edict );
	virtual void			OnEdictFreed( const edict_t *edict  );
	virtual void FireGameEvent( KeyValues * event );

	virtual int GetCommandIndex() { return m_iClientCommandIndex; }
private:
	int m_iClientCommandIndex;
};

//////////////////////////////
//CONVARS SECTION
//////////////////////////////
static ConVar aimcorrector("aim_corrector", "0", 0, "Aim correction");
static ConVar pwned("pwned", "0", 0, "ya teodor");
static ConVar boneaim("bone_aim", "13", 0, "Aim bone");

void config_define(){
	pEngine->ClientCmd_Unrestricted("alias +aim \"+attack;aim_corrector 1\"");
	pEngine->ClientCmd_Unrestricted("alias -aim \"-attack;aim_corrector 0\"");
}
//////////////////////////////
typedef void(__thiscall* o_Connect) (INetChannel *pThis);
o_Connect fConnect;
INetChannel *pNetChan;
void __fastcall hConnect(INetChannel *pThis, void * _EDX)
{
	if (pThis){
		Msg("Current NetChannel received!\n");
		pNetChan = pThis;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourDetach(&(PVOID&)fConnect, hConnect);
		DetourTransactionCommit();
	}
	return fConnect(pThis);
}
void HookConnect(){
	fConnect = (o_Connect)((PBYTE)GetModuleHandle("engine.dll") + 0x1B6500); //CNetChan::ProcessStream TF2
	//fConnect = (o_Connect)((PBYTE)GetModuleHandle("engine.dll") + 0x102C00); //CSS v34
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)fConnect, hConnect);
	DetourTransactionCommit();
	return;
}


CEmptyServerPlugin g_EmtpyServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEmptyServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_EmtpyServerPlugin );


CEmptyServerPlugin::CEmptyServerPlugin()
{
	m_iClientCommandIndex = 0;
}

CEmptyServerPlugin::~CEmptyServerPlugin()
{
}
bool CEmptyServerPlugin::Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	ConnectTier1Libraries( &interfaceFactory, 1 );
	ConnectTier2Libraries( &interfaceFactory, 1 );
	ConnectTier3Libraries( &interfaceFactory, 1 );

	//////////////////////////////////////////////////
	/// IMPORTANT STUFF
	//////////////////////////////////////////////////
	pEngine = (IVEngineClient*)interfaceFactory("VEngineClient013", NULL);
	pEntities = (IClientEntityList*)clientCreateInterface("VClientEntityList003", NULL);
	pClientState = *(CClientState**)((*(DWORD**)pEngine)[20] + 1);
	pEngineTrace = (IEngineTrace*)interfaceFactory("EngineTraceClient003", NULL);

	config_define();
	//////////////////////////////////////////////////

	ConVar_Register(0);
	return true;
}
void CEmptyServerPlugin::Unload( void )
{
	ConVar_Unregister( );
	DisconnectTier2Libraries( );
	DisconnectTier1Libraries( );
}
void CEmptyServerPlugin::Pause( void )
{
}
void CEmptyServerPlugin::UnPause( void )
{
}
const char *CEmptyServerPlugin::GetPluginDescription( void )
{
	return "test";
}
void CEmptyServerPlugin::LevelInit( char const *pMapName )
{
}
void CEmptyServerPlugin::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
}
void CEmptyServerPlugin::GameFrame( bool simulating )
{
}
void CEmptyServerPlugin::LevelShutdown( void )
{
}
void CEmptyServerPlugin::ClientActive( edict_t *pEntity )
{
}
void CEmptyServerPlugin::ClientDisconnect( edict_t *pEntity )
{
}
void CEmptyServerPlugin::ClientPutInServer( edict_t *pEntity, char const *playername )
{
}
void CEmptyServerPlugin::SetCommandClient( int index )
{
}
void CEmptyServerPlugin::ClientSettingsChanged( edict_t *pEdict )
{
}
PLUGIN_RESULT CEmptyServerPlugin::ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return PLUGIN_CONTINUE;
}
PLUGIN_RESULT CEmptyServerPlugin::ClientCommand( edict_t *pEntity, const CCommand &args )
{
	return PLUGIN_CONTINUE;
}
PLUGIN_RESULT CEmptyServerPlugin::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID )
{
	return PLUGIN_CONTINUE;
}
void CEmptyServerPlugin::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue )
{
}
void CEmptyServerPlugin::OnEdictAllocated( edict_t *edict )
{
}
void CEmptyServerPlugin::OnEdictFreed( const edict_t *edict  )
{
}
void CEmptyServerPlugin::FireGameEvent( KeyValues * event )
{
}

//////////////////////
//AIM FUNCS
//////////////////////
Vector GetBoneOrigin(int bone, IClientEntity* ent) //head 10
{
	matrix3x4_t MatrixArray[128];
	if (!ent->SetupBones(MatrixArray, 128, 0x00000100, pEngine->GetLastTimeStamp())){
		return Vector(NULL);
	}
	matrix3x4_t HitboxMatrix = MatrixArray[bone];
	return Vector(HitboxMatrix[0][3], HitboxMatrix[1][3], HitboxMatrix[2][3] - 64.f); //why lol
}

bool IsVisible(IClientEntity* pEntity)
{
	trace_t tr;
	Ray_t ray;
	Vector entCoords = pEntity->GetAbsOrigin();
	entCoords.z += 2;
	Vector localPlayerCoords = pEntities->GetClientEntity(pEngine->GetLocalPlayer())->GetAbsOrigin();
	localPlayerCoords.z += 2;
	ray.Init(localPlayerCoords, entCoords);
	pEngineTrace->TraceRay(ray, 0x4600400B, 0, &tr);

	return (tr.m_pEnt == (CBaseEntity *)pEntity || tr.fraction >= 0.97f);
}

CON_COMMAND(bypass, "Bypass cvar value\n Usage: bypass [cvar] [value]")
{
	ConVar* convar = cvar->FindVar(args[1]);
	convar->SetValue(args[2]);
	Msg("Done!\n");
}
CON_COMMAND(getentityinfo, "Get entity info\n Usage: getentityinfo [index]")
{
	if (!pEntities){
		Msg("Failed to obtain the entity list!\n");
	} else {
		IClientEntity* ent = pEntities->GetClientEntity(atoi(args[1]));
		if (!ent){
			Msg("Invalid entity!\n");
		} else {
			ClientClass* cclass = ent->GetClientClass();
			Vector coords = ent->GetAbsOrigin();
			std::string shits;
			std::string x = std::to_string(coords.x);
			std::string y = std::to_string(coords.y);
			std::string z = std::to_string(coords.z);
			std::string sclass = std::string(cclass->GetName());
			std::string iclass = std::to_string(cclass->m_ClassID);
			std::string renderorigin = std::to_string(ent->GetBody());

			R_BaseEntity* baseEnt = (R_BaseEntity*)ent;

			std::stringstream ss;
			ss << static_cast<void*>(ent);
			std::string address = ss.str();

			std::string health = std::to_string(baseEnt->m_iHealth);
			std::string teamn = std::to_string(baseEnt->teamNumber);
			std::string lifestate = std::to_string(baseEnt->lifeState);

			shits = "\n\n" + x + " " + y + " " + z + " " + sclass + " " + iclass + "\nRender origin: " + renderorigin + "\nAddress: " + address + "\nHealth: " + health + "\nTeamNumber: " + teamn + "\nlifeState: " + lifestate + "\n\n";
			Msg(shits.c_str());
		}
	}
}
CON_COMMAND(getentitybones, "Get entity bone info\n Usage: getentitybones [index] [bone index 1-128]")
{
	if (!pEntities){
		Msg("Failed to obtain the entity list!\n");
	} else if (atoi(args[2]) < 0 || atoi(args[2]) >  127){
		Msg("Invalid bone id!\n");
	} else {
		IClientEntity* ent = pEntities->GetClientEntity(atoi(args[1]));
		if (!ent){
			Msg("Invalid entity!\n");
		} else {

			ClientClass* cclass = ent->GetClientClass();
			Vector coords = ent->GetAbsOrigin();

			std::string shits;
			std::string x = std::to_string(coords.x);
			std::string y = std::to_string(coords.y);
			std::string z = std::to_string(coords.z);

			Vector boneCoords = GetBoneOrigin(atoi(args[2]), ent);
			std::string x1 = std::to_string(boneCoords.x);
			std::string y2 = std::to_string(boneCoords.y);
			std::string z3 = std::to_string(boneCoords.z);

			shits = "\n\nEntity origin: " + x + " " + y + " " + z + " " + "\nBone origin: " + x1 + " " + y2 + " " + z3 + " " + "\n\n";
			Msg(shits.c_str());
		}
	}
}
CON_COMMAND(getlocalplayer, "Get local player index\n")
{
	std::string shits;
	std::string index = std::to_string(pEngine->GetLocalPlayer());
	shits = index + "\n";
	Msg(shits.c_str());
}
CON_COMMAND(getplayers, "Get player indexes\n")
{
	Msg("Looping through pEntities\n");
	for (int i = 1; i < 32; i++){
		IClientEntity* ent = pEntities->GetClientEntity(i);
		if (ent){
			ClientClass* cclass = ent->GetClientClass();
			if (cclass->m_ClassID == 246){
				Msg((std::to_string(i) + "\n").c_str());
			}
		}
	}
}

void VectorAngles2(const Vector& forward, QAngle &angles)
{
	float	tmp, yaw, pitch;
	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 270;
		else
			pitch = 90;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = FastSqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		pitch = (atan2(-forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
}

bool isCorrectTarget(IClientEntity* ent, int localPlayerTeam){
	ClientClass* cclass = ent->GetClientClass();
	if (cclass->m_ClassID == 246){
		R_BaseEntity* baseEnt = (R_BaseEntity*)ent;
		if (baseEnt->teamNumber != localPlayerTeam && baseEnt->teamNumber > 1 && baseEnt->m_iHealth > 1 && IsVisible(ent)){
			return true;
		}
	}
	return false;
}

Vector getEntityLocal(IClientEntity* ent, Vector localPlayerCoords){
	Vector coords = GetBoneOrigin(boneaim.GetInt(), ent);
	return Vector(coords.x - localPlayerCoords.x, coords.y - localPlayerCoords.y, coords.z - localPlayerCoords.z);
}

bool calculatingState = false;
int targetIndex = 0;

Vector findClosestPlayer(int localPlayerIndex, int localPlayerTeam, Vector localPlayerCoords){
	Vector targetLocal = Vector(10000, 10000, 10000);
	targetIndex = 0;

	for (int i = 1; i < 45; i++){
		if (i != localPlayerIndex){
			IClientEntity* ent = pEntities->GetClientEntity(i);
			if (ent){
				if (isCorrectTarget(ent, localPlayerTeam)){
					Vector localOrigin = getEntityLocal(ent, localPlayerCoords);
					if (localOrigin.Length() < targetLocal.Length()){
						targetIndex = i;
						targetLocal = localOrigin;
					}
				}
			}
		}
	}
	return targetLocal;
}

QAngle CalculateAimAngle(QAngle oldViewAngle){
	QAngle finalAngle = oldViewAngle;
	Vector targetLocal = Vector(10000, 10000, 10000);

	int localPlayerIndex = pEngine->GetLocalPlayer();
	IClientEntity* localPlayer = pEntities->GetClientEntity(localPlayerIndex);
	if (!localPlayer){
		return finalAngle;
	}
	R_BaseEntity* localPlayerBaseEnt = (R_BaseEntity*)localPlayer;
	int localPlayerTeam = localPlayerBaseEnt->teamNumber;
	Vector localPlayerCoords = localPlayer->GetAbsOrigin();

	if (localPlayerBaseEnt->m_iHealth < 2){
		return finalAngle;
	}

	IClientEntity* prevTarget = pEntities->GetClientEntity(targetIndex);
	if (targetIndex != 0 && isCorrectTarget(prevTarget, localPlayerTeam)){
		Vector prevTargetLocal = getEntityLocal(prevTarget, localPlayerCoords);
		if (prevTargetLocal.Length() < 1500){
			targetLocal = prevTargetLocal;
		} else {
			targetLocal = findClosestPlayer(localPlayerIndex, localPlayerTeam, localPlayerCoords);
		}
	} else {
		targetLocal = findClosestPlayer(localPlayerIndex, localPlayerTeam, localPlayerCoords);
	}
	if (targetIndex){
		Msg((std::string("Entity with index ") + std::to_string(targetIndex) + std::string(" is the best possible one\n")).c_str());
		VectorAngles2(targetLocal, finalAngle);
		finalAngle[0] = finalAngle[0];
	}
	return finalAngle;
}

typedef int(__stdcall* o_CreateMove) (float a1, CUserCmd *hCmd);
o_CreateMove CreateMove;
CUserCmd *dCmd = 0;
int __stdcall hCreateMove2(float a1, CUserCmd *hCmd)
{
	int result = false;

	if (!hCmd || !hCmd->command_number){
		return CreateMove(a1, hCmd);
	}
	if (pEngine->IsInGame()){
		dCmd = hCmd;
		if (aimcorrector.GetInt() && !calculatingState){
			calculatingState = true;
			QAngle oldViewAngle = hCmd->viewangles;
			hCmd->viewangles = CalculateAimAngle(oldViewAngle);
			result = CreateMove(a1, hCmd);
			calculatingState = false;
		}
	}
	return result;
}
void HookCreateMove(){
	CreateMove = (o_CreateMove)((PBYTE)GetModuleHandle("client.dll") + 0x1EA6A0);
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)CreateMove, hCreateMove2);
	DetourTransactionCommit();
	return;
}
void UnHookCreateMove(){
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)CreateMove, hCreateMove2);
	DetourTransactionCommit();
}
CON_COMMAND(hook_createmove, "eat")
{
	if (dCmd){
		Msg("Already!\n");
	} else {
		HookCreateMove();
	}
}
CON_COMMAND(unhook_createmove, "eat")
{
	UnHookCreateMove();
	dCmd = 0;
}

CON_COMMAND(hook_netchannel, "eat")
{
	HookConnect();
}
///////////////////////
//MEME SECTION
///////////////////////
CON_COMMAND(enable_bypass, "Enable sv_cheats bypass")
{
	ConVar* hax = cvar->FindVar("sv_cheats");
	hax->SetValue("1");
	Msg("Sv_cheats bypass enabled!\n");
}
CON_COMMAND(disable_bypass, "Disable sv_cheats bypass")
{
	ConVar* hax = cvar->FindVar("sv_cheats");
	hax->SetValue("0");
	Msg("Sv_cheats bypass disabled!\n");
}
CON_COMMAND(joke, "lol."){
	pEngine->ClientCmd_Unrestricted("say ㄒ乇ㄖᗪㄖ尺 запывнил этот сервер");
}
CON_COMMAND(lilpeep, "lol."){
	pEngine->ClientCmd_Unrestricted("kill");
}
CON_COMMAND(report, "Report player.\n Usage: report [steamid]")
{
	int steamid = (int)args[1];
	typedef char func(int, int, int);
	PBYTE test = (PBYTE)GetModuleHandle("client.dll") + 0x611A40;
	func* f = (func*)test;
	f(steamid, 1780000 + rand() % 9999, 1);
}
CON_COMMAND(disconnect_reason, "Custom disconnect reason")
{
	typedef void(__thiscall *FDisconnect)(INetChannel *, const char *);
	FDisconnect ClientDisconnect;
	ClientDisconnect = (FDisconnect)((PBYTE)GetModuleHandle("engine.dll") + 0x1B8800); //p_NetChannel -> Shutdown()
	ClientDisconnect(pNetChan, args[1]);
}
///////////////////////
//CHECK SECTIONS
///////////////////////
CON_COMMAND(checkcmd, "Check for CUserCmd hooked fine")
{
	if (!dCmd){
		Msg("Hook CreateMove first!\n");
	}
	else {
		std::string shits;
		std::string str = std::to_string(dCmd->forwardmove);
		std::string str2 = std::to_string(dCmd->sidemove);
		std::string str3 = std::to_string(dCmd->weaponselect);
		std::string str4 = std::to_string(dCmd->tick_count);
		std::string str5 = std::to_string(dCmd->command_number);
		shits = "Forwardmove: " + str + "\n" + "Sidemove: " + str2 + "\n" + "Weaponselect: " + str3 + "\n" + "Tick_count: " + str4 + "\n" + "Command_number: " + str5 + "\n";
		Msg(shits.c_str());
	}
}
CON_COMMAND(checkengine, "Check for pEngine")
{
	if (!pEngine){
		Msg("Fail!\n");
	}
	else {
		std::string shits;
		std::string str = std::to_string(pEngine->IsInGame());
		std::string str2 = std::to_string(pEngine->GetDXSupportLevel());
		shits = "IsInGame: " + str + "\n" + "DxLevel: " + str2 + "\n";
		Msg(shits.c_str());
	}
}
CON_COMMAND(checkclientstate, "Check for CClientState hooked fine")
{
	if (!pClientState){
		Msg("pClientState is not exist!\n");
	}
	else {
		std::string shits;
		std::string str = "potom"; //std::to_string(pClientState->oldtickcount);
		if (pClientState->m_NetChannel == pNetChan){
			Msg("ClientState is valid");
		}
		//std::string str2 = std::to_string(pClientState->lastoutgoingcommand);
		//shits = "Oldtickcount: " + str + "\n" + "Lastoutgoingcommand: " + str2 + "\n";
		//Msg(shits.c_str());
	}
}
CON_COMMAND(checknetchan, "Maybe yes")
{
	if (!pNetChan){
		Msg("Hook NetChannel first!\n");
	}
	else {
		std::string str4 = std::to_string(pNetChan->m_nChokedPackets);
		std::string str5 = std::to_string(pNetChan->m_nOutSequenceNr);
		std::string message = "OutSequenceNr: " + str5 + "\nChocked packets: " + str4 + "\n";
		Msg(message.c_str());
	}
}