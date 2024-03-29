/*
 * ============================================================================
 *
 *  Zombie:Reloaded
 *
 *  File:          zombiereloaded.inc
 *  Type:          Core 
 *  Description:   General plugin functions and utilities.
 *
 *  Copyright (C) 2009-2010  Greyscale, Richard Helgeby
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * ============================================================================
 */

/**
 * Index of server console.
 */
#define ZR_CONSOLE_INDEX 0

/**
 * Global variable set to true when the first zombie(s) is/are spawned.
 */
new bool:g_bZombieSpawned;

/**
 * Options that a condition must pass to be eligible.
 */
enum EligibleCondition
{
    Condition_False = 0,    /** Condition must be false. */
    Condition_True = 1,     /** Condition must be true. */
    Condition_Either = -1   /** Condition can be either true or false. */
}

/**
 * Function to convert numbers to defined units.
 * 
 * @param number        The number to convert.
 * @param conversion    The conversion factor to multiply by. (See defines above)
 * @return              The converted number.
 */

#define CONVERSION_UNITS_TO_FEET    16.000
#define CONVERSION_FEET_TO_UNITS    0.0625

stock Float:ZRConvertUnitsFloat(Float:number, Float:conversion)
{
    return number / conversion;
}

// Timer utilities

/**
 * Checks if a timer is currently running.
 * 
 * @param timer     The timer handle.
 */
stock bool:ZRIsTimerRunning(Handle:timer)
{
    // Return true if the handle isn't empty.
    return (timer != INVALID_HANDLE);
}

/**
 * Wrapper function for KillTimer.
 * Ends a timer if running, and resets its timer handle variable.
 * 
 * @param timer     The timer handle.
 * @param kill      True to kill the timer and reset the variable, false to only reset the variable.
 *                  Using false is useful when calling from the timer callback, because the timer is already killed.
 * 
 * @return          True if the handle wasn't INVALID_HANDLE, false if the handle wasn't valid.
 */
stock bool:ZREndTimer(&Handle:timer, bool:kill = true)
{
    // If the timer is running, then kill it.
    if (ZRIsTimerRunning(timer))
    {
        // Kill if caller says to.
        if (kill)
        {
            KillTimer(timer);
        }
        
        // Reset variable.
        timer = INVALID_HANDLE;
        
        return true;
    }
    
    // Reset variable.
    timer = INVALID_HANDLE;
    
    return false;
}

/**
 * Create an array populated with eligible clients.
 * 
 * @param arrayEligibleClients  The handle of the array, don't forget to call CloseHandle
 *                              on it when finished!
 * @param team                  Client is only eligible if on a team.
 * @param alive                 Client is only eligible if alive.
 * @param human                 Client is only eligible if human.
 * 
 * @return                      Number of eligible clients.
 */  
stock ZRCreateEligibleClientList(&Handle:arrayEligibleClients, EligibleCondition:team = Condition_Either, EligibleCondition:alive = Condition_Either, EligibleCondition:human = Condition_Either)
{
    // Create array.
    arrayEligibleClients = CreateArray();
    
    // Populate list with eligible clients.
    // x = client index.
    for (new x = 1; x <= MaxClients; x++)
    {
        // If client isn't in-game, then stop.
        if (!IsClientInGame(x))
        {
            continue;
        }
        
        // If the client doesn't pass the conditions set, then skip them.
        
        new bool:bTeam = bool:team;
        if (team != Condition_Either && bTeam != ZRIsClientOnTeam(x))
        {
            continue;
        }
        
        new bool:bAlive = bool:alive;
        if (alive != Condition_Either && bAlive != IsPlayerAlive(x))
        {
            continue;
        }
        
        new bool:bHuman = bool:human;
        if (human != Condition_Either && bHuman != TLib_IsClientHuman(x))
        {
            continue;
        }
        
        // Add eligible client to array.
        PushArrayCell(arrayEligibleClients, x);
    }
    
    return GetArraySize(arrayEligibleClients);
}

/**
 * Check if a client index is a valid player.
 * 
 * @param client    The client index.
 * @param console   True to include console (index 0), false if not.
 * @return          True if client is valid, false otherwise. 
 */    
stock bool:ZRIsClientValid(client, bool:console = false)
{
    // If index is greater than max number of clients, then return false.
    if (client > MaxClients)
    {
        return false;
    }
    
    // If console is true, return if client is >= 0, if not, then return client > 0.
    return console ? (client >= 0) : (client > 0);
}

/**
 * Check if a given index is console.
 * 
 * @param client    The client index.
 * @param console   True to include console (index 0), false if not.
 * @return          True if client is valid, false otherwise. 
 */    
stock bool:ZRIsConsole(index)
{
    // Return true if index is equal to console's index.
    return (index == ZR_CONSOLE_INDEX);
}

/**
 * Count clients on each team.
 * 
 * @param zombies   This is set to the number of clients that are zombies.
 * @param humans    This is set to the number of clients that are humans.
 * @param alive     If true it will only count live players, false will count alive and dead.
 * @return          True if successful (zombie has spawned), false otherwise.
 */
stock bool:ZRCountValidClients(&zombiecount = 0, &humancount = 0, bool:alive = true, bool:ignorezombiespawned = false)
{
    // If zombie hasn't spawned and were not only counting humans, then stop.
    if (!g_bZombieSpawned && !ignorezombiespawned)
    {
        return false;
    }
    
    // x = client index.
    for (new x = 1; x <= MaxClients; x++)
    {
        // If client isn't in-game, then stop.
        if (!IsClientInGame(x))
        {
            continue;
        }
        
        // If client isn't on a team, then stop.
        if (!ZRIsClientOnTeam(x))
        {
            continue;
        }
        
        // If player must be alive, and player is dead, then stop.
        if (alive && !IsPlayerAlive(x))
        {
            continue;
        }
        
        // If player is a zombie, then increment zombie variable.
        if (TLib_IsClientZombie(x))
        {
            zombiecount++;
        }
        // If player is a human, then increment human variable.
        else if (TLib_IsClientHuman(x))
        {
            humancount++;
        }
    }
    
    return true;
}

/**
 * Check if a client index is on a team.
 * 
 * @param client    The client index.
 * @param team      Team to check if player is on, -1 to check both T and CT. 
 * @return          True if client is on a team, false otherwise. 
 */    
stock bool:ZRIsClientOnTeam(client, team = -1)
{
    // If index is invalid, then stop.
    if (!ZRIsClientValid(client))
    {
        return false;
    }
    
    // Get client team.
    new clientteam = GetClientTeam(client);
    
    if (team == -1)
    {
        return (clientteam == CS_TEAM_T || clientteam == CS_TEAM_CT);
    }
    
    return (clientteam == team);
}

/**
 * Check if there are clients on a team.
 * 
 * @param team      (Optional) Team to check if there are clients on.
 */
stock bool:ZRTeamHasClients(team = -1)
{
    // If team is
    if (team == -1)
    {
        // Return true if both teams have at least 1 client.
        return (GetTeamClientCount(CS_TEAM_T) && GetTeamClientCount(CS_TEAM_CT));
    }
    
    // Return true if given team has at least 1 client.
    return bool:GetTeamClientCount(team);
}

/**
 * Returns whether a player is a admin or not.
 *
 * @param client    The client index.
 * @param flag      Optional. Flag to check. Default is generic admin flag.
 * @return          True if generic admin, false otherwise.
 */
stock bool:ZRIsClientAdmin(client, AdminFlag:flag = Admin_Generic)
{
    // If index is invalid, then stop.
    if (!ZRIsClientValid(client))
    {
        return false;
    }
    
    // If client doesn't have the specified flag, then stop.
    if (!GetAdminFlag(GetUserAdmin(client), flag))
    {
        return false;
    }
    
    // Client is an admin.
    return true;
}

/**
 * Replies to a client with a given message describing a targetting 
 * failure reason. (formatted for ZR)
 *
 * Note: The translation phrases are found in common.phrases.txt.
 *
 * @param client		Client index, or 0 for server.
 * @param reason		COMMAND_TARGET reason.
 */
stock ZRReplyToTargetError(client, reason)
{
	switch (reason)
	{
		case COMMAND_TARGET_NONE:
		{
			TranslationReplyToCommand(client, "No matching client");
		}
		case COMMAND_TARGET_NOT_ALIVE:
		{
			TranslationReplyToCommand(client, "Target must be alive");
		}
		case COMMAND_TARGET_NOT_DEAD:
		{
			TranslationReplyToCommand(client, "Target must be dead");
		}
		case COMMAND_TARGET_NOT_IN_GAME:
		{
			TranslationReplyToCommand(client, "Target is not in game");
		}
		case COMMAND_TARGET_IMMUNE:
		{
			TranslationReplyToCommand(client, "Unable to target");
		}
		case COMMAND_TARGET_EMPTY_FILTER:
		{
			TranslationReplyToCommand(client, "No matching clients");
		}
		case COMMAND_TARGET_NOT_HUMAN:
		{
			TranslationReplyToCommand(client, "Cannot target bot");
		}
		case COMMAND_TARGET_AMBIGUOUS:
		{
			TranslationReplyToCommand(client, "More than one client matched");
		}
	}
}

/**
 * Adds support for printing strings longer than 1 KB to console. Max 4 KB.
 *
 * Note: 1024 characters is max for the console, including newline and null
 *       terminator.
 *
 * @param client        The client index.
 * @param text          Long text to write.
 * @param splitsize     Optional. Sets the split size. 1022 is default.
 *                      Allowed range: 128 to 1022.
 */
stock ZRPrintToConsoleLong(client, const String:text[], splitsize = 1022)
{
    // Validate split size.
    if (splitsize < 128 || splitsize > 1022)
    {
        return;
    }
    
    decl String:partbuffer[splitsize];
    new pos;
    new cellswritten = 1;           // Initialize for the loop.

    while (cellswritten)
    {
        cellswritten = strcopy(partbuffer, splitsize, text[pos]);
        (client > 0) ? PrintToConsole(client, partbuffer) : PrintToServer(partbuffer);
        pos += cellswritten;
    }
}

/**
 * Converts a boolean value into a string.
 * 
 * @param value     The value to convert to string.
 * @param output    The converted string.
 * @param maxlen    The maximum length of the string.
 */
ZRBoolToString(bool:value, String:output[], maxlen)
{
    // If the value is true, then set string to "1".
    if (value)
    {
        strcopy(output, maxlen, "1");
    }
    // If the value is false, then set string to "0".
    else
    {
        strcopy(output, maxlen, "0");
    }
}

/**
 * CloseHandle wrapper that ignore INVALID_HANDLE.
 * The handle is also set to INVALID_HANDLE if closed.
 *
 * @param handle    Handle to close.
 */
ZR_CloseHandle(&Handle:handle)
{
    if (handle != INVALID_HANDLE)
    {
        CloseHandle(handle);
        handle = INVALID_HANDLE;
    }
}

/**
 * Returns whether the handle value is different from INVALID_HANDLE or not.
 *
 * @param handle    Handle to check.
 */
bool:ZR_HasHandle(Handle:handle)
{
    return handle == INVALID_HANDLE ? false : true;
}
