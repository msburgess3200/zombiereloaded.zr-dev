/**
 * ====================
 *   Zombie:Reloaded
 *   File: restriction.inc
 *   Author: Greyscale
 * ==================== 
 */

/**
 * Array to store restricted weapon names.
 */
new Handle:gRestrictedWeapons = INVALID_HANDLE;

/**
 * Array to store keyvalue data.
 */
new Handle:kvWeaponGroups = INVALID_HANDLE;

/**
 * Array that stores the "HookID" to be later unhooked on player disconnect.
 */
new gCanUseHookID[MAXPLAYERS + 1];

/**
 * Query results returned when (un)restricting a weapon.
 */
enum WpnRestrictQuery
{
    Successful_Weapon,  /** Weapon (un)restrict query was successful. */
    Successful_Group,  /** Group (un)restrict query was successful. */
    Failed_Weapon, /** Weapon (un)restrict was unsuccessful */
    Failed_Group, /** Group (un)restrict was unsuccessful */
    Invalid,  /** Weapon/Group invalid */
}

/**
 * Initialize data and hook commands.
 */
RestrictInit()
{
    // Initialize weapon restrict array
    gRestrictedWeapons = CreateArray(32, 0);
    
    // Hook buy command
    RegConsoleCmd("buy", RestrictBuyHook);
}

/**
 * Clears weapon restrict data
 */ 
RestrictClearData()
{
    // Clear restricted weapons
    RestrictWeaponUnrestrictAll();
    
    // Load weapon group data
    if (kvWeaponGroups != INVALID_HANDLE)
    {
        CloseHandle(kvWeaponGroups);
    }
    
    kvWeaponGroups = CreateKeyValues("weapongroups");
}

/**
 * Loads weapon data from file.
 */
RestrictOnMapStart()
{
    // Restrict default restrictions (set in weapons.txt)
    RestrictDefaultRestrictions();
    
    decl String:path[PLATFORM_MAX_PATH];
    BuildPath(Path_SM, path, sizeof(path), "configs/zr/weapons/weapongroups.txt");
    
    // If file isn't found, stop plugin
    if (!FileToKeyValues(kvWeaponGroups, path))
    {
        if (LogFlagCheck(LOG_CORE_EVENTS, LOG_MODULE_WEAPONS))
        {
            ZR_LogMessageFormatted(-1, "Weapons", "Config Validation", "Missing file weapongroups.txt.", LOG_FORMAT_TYPE_FULL);
        }
        
        return;
    }
    
    RestrictValidateWeaponGroups();
}

/**
 * Restrict default restrictions (set in weapons.txt)
 */
RestrictDefaultRestrictions()
{
    KvRewind(kvWeapons);
    
    if (KvGotoFirstSubKey(kvWeapons))
    {
        decl String:weapon[WEAPONS_MAX_LENGTH];
        decl String:display[WEAPONS_MAX_LENGTH];
        
        do
        {
            KvGetSectionName(kvWeapons, weapon, sizeof(weapon));
            
            // If weapon is defaulted to restricted, then restrict weapon
            decl String:restrict[8];
            KvGetString(kvWeapons, "restrict", restrict, sizeof(restrict), "no");
            
            if (ConfigSettingToBool(restrict))
            {
                new WpnRestrictQuery:output = RestrictRestrict(weapon, display);
                RestrictPrintRestrictOutput(0, output, display, true);
                
                // Function calls above screwed with the keyvalue stack, so we have to set it back
                // to where it was before those calls
                KvRewind(kvWeapons);
                KvJumpToKey(kvWeapons, weapon);
            }
        } while (KvGotoNextKey(kvWeapons));
    }
}

/**
 * Validate weapon group options
 */ 
RestrictValidateWeaponGroups()
{
    // If log flag check fails, don't log
    if (!LogFlagCheck(LOG_CORE_EVENTS, LOG_MODULE_WEAPONS))
    {
        return;
    }
    
    KvRewind(kvWeaponGroups);
    
    if (KvGotoFirstSubKey(kvWeaponGroups))
    {
        decl String:weapongroup[WEAPONS_MAX_LENGTH];
        decl String:groupweapon[WEAPONS_MAX_LENGTH];
        
        do
        {
            KvGetSectionName(kvWeaponGroups, weapongroup, sizeof(weapongroup));
            
            if (KvGotoFirstSubKey(kvWeaponGroups))
            {
                do
                {
                    KvGetSectionName(kvWeaponGroups, groupweapon, sizeof(groupweapon));
                    
                    if (!WeaponsIsValidWeapon(groupweapon))
                    {
                        ZR_LogMessageFormatted(-1, "Weapon Restrict", "Config Validation", "Invalid weapon \"%s\" in group \"%s\" configured in weapongroups.txt.", LOG_FORMAT_TYPE_ERROR, groupweapon, weapongroup);
                    }
                } while (KvGotoNextKey(kvWeaponGroups));
                
                KvGoBack(kvWeaponGroups);
            }
            else
            {
                ZR_LogMessageFormatted(-1, "Weapon Restrict", "Config Validation", "No weapons listed in weapon group \"%s\" in weapongroups.txt.", LOG_FORMAT_TYPE_ERROR, weapongroup);
            }
        } while (KvGotoNextKey(kvWeaponGroups));
    }
}

/**
 * Clears restricted weapon array.
 */
RestrictWeaponUnrestrictAll()
{
    ClearArray(gRestrictedWeapons);
}

/**
 * Hook Weapon_CanUse function on a client.
 * 
 * @param client    The client index.
 */
RestrictClientInit(client)
{
    gCanUseHookID[client] = Hacks_Hook(client, HACKS_HTYPE_WEAPON_CANUSE, RestrictCanUse, false);
}

/**
 * Unhook Weapon_CanUse function on a client.
 * 
 * @param client    The client index.
 */
RestrictOnClientDisconnect(client)
{
    Hacks_Unhook(gCanUseHookID[client]);
}

/**
 * Command callback function for the "buy" command
 * Used to block use of this command under certain conditions.
 * 
 * @param client    The client index.
 * @param argc      Argument count.
 */
public Action:RestrictBuyHook(client, argc)
{
    // If plugin is disabled then stop
    new bool:enabled = GetConVarBool(gCvars[CVAR_ENABLE]);
    if (!enabled)
    {
        // Allow command
        return Plugin_Continue;
    }
    
    // If player is a zombie then block command
    if (IsPlayerZombie(client))
    {
        ZR_PrintToChat(client, "Zombie cant use weapon");
        
        // Block command
        return Plugin_Handled;
    }
    
    decl String:weapon[64];
    GetCmdArg(1, weapon, sizeof(weapon));
    
    ReplaceString(weapon, sizeof(weapon), "weapon_", "");
    
    // Check if the weapon is restricted, if so then block command
    if (RestrictIsWeaponRestricted(weapon))
    {
        ZR_PrintToChat(client, "Weapon is restricted", weapon);
        
        // Block command
        return Plugin_Handled;
    }
    
    // Allow command
    return Plugin_Continue;
}

/**
 * Restricts a weapon.
 * 
 * @param weapon    The weapon/group name.
 * @param display   String set to the name set in weapons.txt
 *                  Set to the value of 'weapon' if invalid
 * @return          Successful_Weapon: The call successfully restricted a weapon.
 *                  Successful_Group: The call successfully restricted a weapon group.
 *                  Failed_Weapon: The call failed to restrict a weapon.
 *                  Failed_Group: The call failed to restrict a weapon group.
 *                  Invalid: The call was unsuccessful due to invalid weapon.
 */
WpnRestrictQuery:RestrictRestrict(const String:weapon[], String:display[] = "")
{
    if (RestrictIsWeaponGroup(weapon))
    {
        if (RestrictIsGroupRestricted(weapon))
        {
            return Failed_Group;
        }
        
        KvRewind(kvWeaponGroups);
        KvJumpToKey(kvWeaponGroups, weapon);
        
        KvGetSectionName(kvWeaponGroups, display, WEAPONS_MAX_LENGTH);
        
        if (KvGotoFirstSubKey(kvWeaponGroups))
        {
            decl String:groupweapon[WEAPONS_MAX_LENGTH];
            
            do
            {
                KvGetSectionName(kvWeaponGroups, groupweapon, sizeof(groupweapon));
                
                // If weapon is invalid, then skip
                if (!WeaponsIsValidWeapon(groupweapon))
                {
                    continue;
                }
                
                if (!RestrictIsWeaponRestricted(groupweapon))
                {
                    PushArrayString(gRestrictedWeapons, groupweapon);
                }
            } while (KvGotoNextKey(kvWeaponGroups));
        }
        
        return Successful_Group;
    }
    
    if (!WeaponsIsValidWeapon(weapon))
    {
        strcopy(display, WEAPONS_MAX_LENGTH, weapon);
        
        return Invalid;
    }
    
    WeaponGetDisplayName(weapon, display);
    
    if (RestrictIsWeaponRestricted(weapon))
    {
        return Failed_Weapon;
    }
    
    PushArrayString(gRestrictedWeapons, display);
    
    return Successful_Weapon;
}

/**
 * Unrestricts a weapon.
 * 
 * @param weapon    The weapon/group name.
 * @param display   String set to the name set in weapons.txt
 *                  Set to the value of 'weapon' if invalid
 * @return          Successful_Weapon: The call successfully restricted a weapon.
 *                  Successful_Group: The call successfully restricted a weapon group.
 *                  Failed_Weapon: The call failed to restrict a weapon.
 *                  Failed_Group: The call failed to restrict a weapon group. 
 *                  Invalid: The call was unsuccessful due to invalid weapon.
 */
WpnRestrictQuery:RestrictUnrestrict(const String:weapon[], String:display[] = "")
{
    if (RestrictIsWeaponGroup(weapon))
    {
        if (RestrictIsGroupUnrestricted(weapon))
        {
            return Failed_Group;
        }
        
        KvRewind(kvWeaponGroups);
        KvJumpToKey(kvWeaponGroups, weapon);
        
        KvGetSectionName(kvWeaponGroups, display, WEAPONS_MAX_LENGTH);
        
        if (KvGotoFirstSubKey(kvWeaponGroups))
        {
            decl String:groupweapon[WEAPONS_MAX_LENGTH];
            
            do
            {
                KvGetSectionName(kvWeaponGroups, groupweapon, sizeof(groupweapon));
                
                // If weapon is invalid, then skip
                if (!WeaponsIsValidWeapon(groupweapon))
                {
                    continue;
                }
                
                if (RestrictIsWeaponRestricted(groupweapon))
                {
                    new weaponindex = RestrictGetIndex(groupweapon);
                    if (weaponindex > -1)
                    {
                        RemoveFromArray(gRestrictedWeapons, weaponindex);
                    }
                }
            } while (KvGotoNextKey(kvWeaponGroups));
        }
        
        return Successful_Group;
    }
    
    if (!WeaponsIsValidWeapon(weapon))
    {
        strcopy(display, WEAPONS_MAX_LENGTH, weapon);
        
        return Invalid;
    }
    
    WeaponGetDisplayName(weapon, display);
    
    if (!RestrictIsWeaponRestricted(weapon))
    {
        return Failed_Weapon;
    }
    
    new weaponindex = RestrictGetIndex(display);
    if (weaponindex > -1)
    {
        RemoveFromArray(gRestrictedWeapons, weaponindex);
    }
    
    return Successful_Weapon;
}

/**
 * Prints text to server or client based off the output it RestrictRestrict().
 * @param client    The client index.
 * @param output    The output of RestrictRestrict().
 * @param weapon    The weapon client is trying to restrict.
 * @param cmd       True if printing output in reply to a client command.
 */ 
RestrictPrintRestrictOutput(client, WpnRestrictQuery:output, const String:weapon[], bool:reply)
{
    switch(output)
    {   
        case Successful_Weapon:
        {
            ZR_PrintToChat(0, "Restrict weapon", weapon);
            
            if (LogFlagCheck(LOG_GAME_EVENTS, LOG_MODULE_WEAPONS))
            {
                ZR_LogMessageFormatted(client, "Weapon Restrict", "Restrict", "\"%L\" restricted weapon: \"%s\".", LOG_FORMAT_TYPE_FULL, client, weapon);
            }
        }
        case Successful_Group:
        {
            decl String:weaponlist[128];
            RestrictGetGroupWeapons(weapon, weaponlist, sizeof(weaponlist), ", ");
            
            ZR_PrintToChat(0, "Restrict custom weapon group", weapon, weaponlist);
            
            if (LogFlagCheck(LOG_GAME_EVENTS, LOG_MODULE_WEAPONS))
            {
                ZR_LogMessageFormatted(client, "Weapon Restrict", "Restrict", "\"%L\" restricted weapon group: \"%s\".", LOG_FORMAT_TYPE_FULL, client, weapon);
            }
        }
        case Failed_Weapon:
        {
            if (reply)
            {
                ZR_ReplyToCommand(client, "Restrict weapon failed", weapon);
            }
            else
            {
                ZR_PrintToChat(client, "Restrict weapon failed", weapon);
            }
        }
        case Failed_Group:
        {
            decl String:weaponlist[128];
            RestrictGetGroupWeapons(weapon, weaponlist, sizeof(weaponlist), ", ");
            
            if (reply)
            {
                ZR_ReplyToCommand(client, "Restrict custom weapon group failed", weapon, weaponlist);
            }
            else
            {
                ZR_PrintToChat(client, "Restrict custom weapon group failed", weapon, weaponlist);
            }
        }
        case Invalid:
        {
            if (reply)
            {
                ZR_ReplyToCommand(client, "Weapon invalid", weapon);
            }
            else
            {
                ZR_PrintToChat(client, "Weapon invalid", weapon);
            }
        }
    }
}

/**
 * Prints text to server or client based off the output it RestrictUnrestrict().
 * @param client    The client index.
 * @param output    The output of RestrictUnrestrict().
 * @param weapon    The weapon client is trying to unrestrict.
 * @param cmd       True if printing output in reply to a client command.
 */ 
RestrictPrintUnrestrictOutput(client, WpnRestrictQuery:output, const String:weapon[], bool:reply)
{
    switch(output)
    {
        case Successful_Weapon:
        {
            ZR_PrintToChat(0, "Unrestrict weapon", weapon);
            
            if (LogFlagCheck(LOG_GAME_EVENTS, LOG_MODULE_WEAPONS))
            {
                ZR_LogMessageFormatted(client, "Weapon Restrict", "Unrestrict", "\"%L\" unrestricted weapon: \"%s\".", LOG_FORMAT_TYPE_FULL, client, weapon);
            }
        }
        case Successful_Group:
        {
            decl String:weaponlist[128];
            RestrictGetGroupWeapons(weapon, weaponlist, sizeof(weaponlist), ", ");
            
            ZR_PrintToChat(0, "Unrestrict custom weapon group", weapon, weaponlist);
            
            if (LogFlagCheck(LOG_GAME_EVENTS, LOG_MODULE_WEAPONS))
            {
                ZR_LogMessageFormatted(client, "Weapon Restrict", "Unrestrict", "\"%L\" unrestricted weapon group: \"%s\".", LOG_FORMAT_TYPE_FULL, client, weapon);
            }
        }
        case Failed_Weapon:
        {
            if (reply)
            {
                ZR_ReplyToCommand(client, "Unrestrict weapon failed", weapon);
            }
            else
            {
                ZR_PrintToChat(client, "Unrestrict weapon failed", weapon);
            }
        }
        case Failed_Group:
        {
            decl String:weaponlist[128];
            RestrictGetGroupWeapons(weapon, weaponlist, sizeof(weaponlist), ", ");
            
            if (reply)
            {
                ZR_ReplyToCommand(client, "Unrestrict custom weapon group failed", weapon, weaponlist);
            }
            else
            {
                ZR_PrintToChat(client, "Unrestrict custom weapon group failed", weapon, weaponlist);
            }
        }
        case Invalid:
        {
            if (reply)
            {
                ZR_ReplyToCommand(client, "Weapon invalid", weapon);
            }
            else
            {
                ZR_PrintToChat(client, "Weapon invalid", weapon);
            }
        }
    }
}

/**
 * Checks if a weapon is restricted.
 * 
 * @param weapon    The weapon name.
 */
bool:RestrictIsWeaponRestricted(const String:weapon[])
{
    decl String:restrictedweapon[WEAPONS_MAX_LENGTH];
    
    new size = GetArraySize(gRestrictedWeapons);
    for (new x = 0; x < size; x++)
    {
        GetArrayString(gRestrictedWeapons, x, restrictedweapon, sizeof(restrictedweapon));
        
        if (StrEqual(weapon, restrictedweapon, false))
        {
            return true;
        }
    }
    
    return false;
}

/**
 * Checks if a weapon group is completely restricted.
 * 
 * @param weapongroup    The weapon group name.
 */
bool:RestrictIsGroupRestricted(const String:weapongroup[])
{
    KvRewind(kvWeaponGroups);
    if (KvJumpToKey(kvWeaponGroups, weapongroup))
    {
        decl String:groupweapon[WEAPONS_MAX_LENGTH];
        
        if (KvGotoFirstSubKey(kvWeaponGroups))
        {
            do
            {
                KvGetSectionName(kvWeaponGroups, groupweapon, WEAPONS_MAX_LENGTH);
                
                // Return false is a weapon isn't restricted, but only if the weapon is valid (we ignore invalid ones)
                if (WeaponsIsValidWeapon(groupweapon) && !RestrictIsWeaponRestricted(groupweapon))
                {
                    return false;
                }
            } while (KvGotoNextKey(kvWeaponGroups));
            
            return true;
        }
    }
    
    return false;
}

/**
 * Checks if a weapon group is completely unrestricted.
 * 
 * @param weapongroup    The weapon group name.
 */
bool:RestrictIsGroupUnrestricted(const String:weapongroup[])
{
    KvRewind(kvWeaponGroups);
    if (KvJumpToKey(kvWeaponGroups, weapongroup))
    {
        decl String:groupweapon[WEAPONS_MAX_LENGTH];
        
        if (KvGotoFirstSubKey(kvWeaponGroups))
        {
            do
            {
                KvGetSectionName(kvWeaponGroups, groupweapon, WEAPONS_MAX_LENGTH);
                
                // Return false if a weapon is restricted
                if (RestrictIsWeaponRestricted(groupweapon))
                {
                    return false;
                }
            } while (KvGotoNextKey(kvWeaponGroups));
            
            return true;
        }
    }
    
    return false;
}

/**
 * Checks if a weapon group is partially restricted.
 * 
 * @param weapongroup    The weapon group name.
 */
bool:RestrictIsPartialRestricted(const String:weapongroup[])
{
    return (!RestrictIsGroupRestricted(weapongroup) && !RestrictIsGroupUnrestricted(weapongroup));
}

/**
 * Returns the array index of the restricted weapon.
 * 
 * @param weapon    The weapon name.
 */
RestrictGetIndex(const String:weapon[])
{
    decl String:restrictedweapon[WEAPONS_MAX_LENGTH];
    
    new size = GetArraySize(gRestrictedWeapons);
    for (new x = 0; x < size; x++)
    {
        GetArrayString(gRestrictedWeapons, x, restrictedweapon, sizeof(restrictedweapon));
        
        if (StrEqual(weapon, restrictedweapon, false))
        {
            return x;
        }
    }
    
    return -1;
}

/**
 * Checks if the provided name is a custom group.
 * 
 * @param groupname     Name of the group to check.
 * @return              True if it's a group, false if not.
 */
bool:RestrictIsWeaponGroup(const String:groupname[])
{
    // Reset the traversal stack
    KvRewind(kvWeaponGroups);
    
    // Returns true if groupname is listed in the custom groups file
    return KvJumpToKey(kvWeaponGroups, groupname);
}

/**
 * Creates an array of all listed weapon groups in weapongroups.txt.
 * @param arrayWeaponGroups  The handle of the array, don't forget to call CloseHandle
 *                           on it when finished!
 * @return                   The size of the array.
 */
RestrictCreateGroupArray(&Handle:arrayWeaponGroups, maxlen = WEAPONS_MAX_LENGTH)
{
    arrayWeaponGroups = CreateArray(maxlen);
    new count = 0;
    
    KvRewind(kvWeaponGroups);
    if (KvGotoFirstSubKey(kvWeaponGroups))
    {
        decl String:weapongroup[maxlen];
        
        do
        {
            KvGetSectionName(kvWeaponGroups, weapongroup, maxlen);
            
            PushArrayString(arrayWeaponGroups, weapongroup);
            count++;
        } while (KvGotoNextKey(kvWeaponGroups));
    }
    
    return count;
}

/**
 * Creates an array of all weapons listed in a custom weapon group.
 * @param arrayWeaponGroups  The handle of the array, don't forget to call CloseHandle
 *                           on it when finished!
 * @return                   The size of the array.
 */
RestrictCreateGroupWeaponsArray(&Handle:arrayGroupWeapons, const String:weapongroup[], maxlen = WEAPONS_MAX_LENGTH)
{
    arrayGroupWeapons = CreateArray(maxlen);
    new count = 0;
    
    KvRewind(kvWeaponGroups);
    if (KvJumpToKey(kvWeaponGroups, weapongroup))
    {
        decl String:groupweapon[maxlen];
        
        if (KvGotoFirstSubKey(kvWeaponGroups))
        {
            do
            {
                KvGetSectionName(kvWeaponGroups, groupweapon, maxlen);
                
                // If the weapon is invalid, then stop
                if (!WeaponsIsValidWeapon(groupweapon))
                {
                    continue;
                }
                
                PushArrayString(arrayGroupWeapons, groupweapon);
                count++;
            } while (KvGotoNextKey(kvWeaponGroups));
        }
    }
    
    return count;
}

/**
 * Returns a string of all weapons in a custom weapon group separated
 * by the provided character. 
 * 
 * @param groupname     Name of the group to get weapon list from.
 * @param weaponlist    Variable to store weapon list string in.
 * @param maxlen        Maximum length of the weapon list, the rest is truncated.
 * @param separator     Separator character between weapon names. 
 */
RestrictGetGroupWeapons(const String:groupname[], String:weaponlist[], maxlen, const String:separator[])
{
    KvRewind(kvWeaponGroups);
    KvJumpToKey(kvWeaponGroups, groupname);
    
    if (KvGotoFirstSubKey(kvWeaponGroups))
    {
        decl String:groupweapon[WEAPONS_MAX_LENGTH];
        
        strcopy(weaponlist, maxlen, "");
        
        do
        {
            KvGetSectionName(kvWeaponGroups, groupweapon, sizeof(groupweapon));
            
            // If weapon is invalid, then skip
            if (!WeaponsIsValidWeapon(groupweapon))
            {
                continue;
            }
            
            if (!weaponlist[0])
            {
                strcopy(weaponlist, maxlen, groupweapon);
            }
            else
            {
                Format(weaponlist, maxlen, "%s%s%s", weaponlist, separator, groupweapon);
            }
        } while (KvGotoNextKey(kvWeaponGroups));
    }
}

/**
 * Hook callback, called when a player is trying to pick up a weapon.
 * @param client    The client index.
 * @param weapon    The weapon index.
 * @return          0 to block weapon pickup, Hacks_Continue to allow.
 */
public RestrictCanUse(client, weapon, dummy1, dummy2, dummy3, dummy4)
{
    // If plugin is disabled then stop
    new bool:enabled = GetConVarBool(gCvars[CVAR_ENABLE]);
    if (!enabled)
    {
        return Hacks_Continue;
    }
    
    new String:weaponname[WEAPONS_MAX_LENGTH];
    GetEdictClassname(weapon, weaponname, sizeof(weaponname));
    
    // Strip "weapon_" from entity name
    ReplaceString(weaponname, sizeof(weaponname), "weapon_", "");
    
    // If the weapon is restricted then prevent pickup
    if (RestrictIsWeaponRestricted(weaponname))
    {
        return 0;
    }
    
    // If the player is a zombie and the weapon isn't a knife then prevent pickup
    if (IsPlayerZombie(client) && !StrEqual(weaponname, "knife"))
    {
        return 0;
    }
    
    return Hacks_Continue;
}