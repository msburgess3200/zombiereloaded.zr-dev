/*
 * ============================================================================
 *
 *   Zombie:Reloaded
 *
 *   File:          ambientsounds.inc
 *   Type:          Core 
 *   Description:   Plays ambient sounds to clients.
 *
 * ============================================================================
 */

/**
 * Global variable that tells if ambient sound cvar data was loaded successfully.
 */ 
new bool:g_bAmbientSounds;

/**
 * Global variable to store ambient sounds timer handle.
 */
new Handle:tAmbientSounds = INVALID_HANDLE;

/**
 * Array for flagging client to play sound.
 */
new bool:bAmbientSoundsIsPlaying[MAXPLAYERS + 1]; 

/**
 * Load ambient sound data.
 */
AmbientSoundsLoad()
{
    // Validate cvars.
    g_bAmbientSounds = AmbientSoundsValidateConfig();
}

/**
 * Client is joining the server.
 * 
 * @param client    The client index.
 */
AmbientSoundsClientInit(client)
{
    // Reset flag to play sound on client.
    bAmbientSoundsIsPlaying[client] = false;
}

/**
 * Validate ambient sounds cvars.
 */
bool:AmbientSoundsValidateConfig()
{
    // If ambience is disabled, then stop.
    new bool:ambience = GetConVarBool(g_hCvarsList[CVAR_AMBIENTSOUNDS]);
    if (!ambience)
    {
        return false;
    }
    
    // If logging is disabled for ambient sounds, then stop.
    if (!LogCheckFlag(LOG_CORE_EVENTS, LOG_MODULE_AMBIENTSOUNDS))
    {
        return false;
    }
    
    // Get ambient sound file.
    decl String:sound[SOUND_MAX_PATH];
    GetConVarString(g_hCvarsList[CVAR_AMBIENTSOUNDS_FILE], sound, sizeof(sound));
    Format(sound, sizeof(sound), "sound/%s", sound);
    
    // If file doesn't exist, then log error and stop.
    if (!FileExists(sound, true))
    {
        // Log invalid sound file error.
        LogMessageFormatted(-1, "Ambient Sounds", "Config Validation", "Invalid sound file specified in zr_ambientsounds_file.", LOG_FORMAT_TYPE_ERROR);
        return false;
    }
    
    // If volume is muted or invalid, then log error and stop.
    new Float:ambientvolume = GetConVarFloat(g_hCvarsList[CVAR_AMBIENTSOUNDS_VOLUME]);
    if (ambientvolume <= 0.0)
    {
        // Log invalid ambient sound volume error.
        LogMessageFormatted(-1, "Ambient Sounds", "Config Validation", "Ambient sound is either muted or invalid.", LOG_FORMAT_TYPE_ERROR);
        return false;
    }
    
    // If length is invalid, then log error and stop.
    new Float:ambientlength = GetConVarFloat(g_hCvarsList[CVAR_AMBIENTSOUNDS_LENGTH]);
    if (ambientlength <= 0.0)
    {
        // Log invalid ambient sound length error.
        LogMessageFormatted(-1, "Ambient Sounds", "Config Validation", "Ambient sound length is invalid.", LOG_FORMAT_TYPE_ERROR);
        return false;
    }
    
    // Add sound file to downloads table.
    AddFileToDownloadsTable(sound);
    
    return true;
}

/**
 * Map is starting.
 */
AmbientSoundsOnMapStart()
{
    // Reset timer handle.
    tAmbientSounds = INVALID_HANDLE;
}

/**
 * The round is starting.
 */
AmbientSoundsOnRoundStart()
{
    // Restart ambient sound for all clients.
    AmbientSoundsRestart();
}

/**
 * The round is ending.
 */
AmbientSoundsOnRoundEnd()
{
    // x = client index
    for (new x = 1; x <= MaxClients; x++)
    {
        bAmbientSoundsIsPlaying[x] = false;
    }
}

/**
 * Client is spawning into the game.
 * 
 * @param client    The client index.
 */
AmbientSoundsOnClientSpawn(client)
{
    // If ambience is disabled, then stop.
    if (!g_bAmbientSounds)
    {
        return;
    }
    
    // If flag is enabled, then stop.
    if (bAmbientSoundsIsPlaying[client])
    {
        return;
    }
    
    // Get ambient sound file.
    decl String:sound[SOUND_MAX_PATH];
    GetConVarString(g_hCvarsList[CVAR_AMBIENTSOUNDS_FILE], sound, sizeof(sound));
    
    // Get ambient sound volume.
    new Float:ambientvolume = GetConVarFloat(g_hCvarsList[CVAR_AMBIENTSOUNDS_VOLUME]);
    
    // Emit ambient sound.
    SEffectsEmitAmbientSound(sound, ambientvolume, client);
    
    // Flag client that sound is playing.
    bAmbientSoundsIsPlaying[client] = true;
}

/**
 * Restart ambient sound for all clients.
 */
AmbientSoundsRestart()
{
    // If timer is running, kill it.
    if (tAmbientSounds != INVALID_HANDLE)
    {
        KillTimer(tAmbientSounds);
    }
    
    // If ambience is disabled, then stop.
    if (!g_bAmbientSounds)
    {
        return;
    }
    
    // Get ambient sound length.
    new Float:ambientlength = GetConVarFloat(g_hCvarsList[CVAR_AMBIENTSOUNDS_LENGTH]);
    
    // Start ambient sounds timer.
    tAmbientSounds = CreateTimer(ambientlength, AmbientSoundsTimer, _, TIMER_FLAG_NO_MAPCHANGE);
}

/**
 * Timer callback, Replays ambient sound on all clients.
 * 
 * @param timer     The timer handle.  
*/
public Action:AmbientSoundsTimer(Handle:timer)
{
    // If ambience is disabled, then stop.
    if (!g_bAmbientSounds)
    {
        return;
    }
    
    // Get ambient sound file.
    decl String:sound[SOUND_MAX_PATH];
    GetConVarString(g_hCvarsList[CVAR_AMBIENTSOUNDS_FILE], sound, sizeof(sound));
    
    // Get ambient sound volume.
    new Float:ambientvolume = GetConVarFloat(g_hCvarsList[CVAR_AMBIENTSOUNDS_VOLUME]);
    
    // Stop sound before playing again.
    SEffectsStopAmbientSound(sound);
    
    // Emit ambient sound.
    SEffectsEmitAmbientSound(sound, ambientvolume);
    
    // Get ambient sound length.
    new Float:ambientlength = GetConVarFloat(g_hCvarsList[CVAR_AMBIENTSOUNDS_LENGTH]);
    
    // Start new timer with sound length as delay.
    tAmbientSounds = CreateTimer(ambientlength, AmbientSoundsTimer, _, TIMER_FLAG_NO_MAPCHANGE);
}