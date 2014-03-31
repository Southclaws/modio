/*==============================================================================


	Southclaw's Modular Binary IO Library

		Description...


==============================================================================*/


#include <YSI\y_hooks>


#if !defined MODIO_FILE_STRUCTURE_VERSION
	#define MODIO_FILE_STRUCTURE_VERSION (10)
#endif

#if !defined MAX_MODIO_TAGS
	#define MAX_MODIO_TAGS (64)
#endif

#define MAX_MODIO_HEADER_SIZE (MAX_MODIO_TAGS * 2)

#if !defined MAX_MODIO_STACK_SIZE
	#define MAX_MODIO_STACK_SIZE (4096)
#endif

#if !defined MAX_MODIO_SESSION
	#define MAX_MODIO_SESSION (32)
#endif

#if !defined MODIO_DEBUG
	#define MODIO_DEBUG (0)
#endif

#if !defined MAX_FILE_PATH
	#define MAX_FILE_PATH (128)
#endif


// Quick and dirty debug print. Levels range from 0 to 5
#define dp:%0(%1) if(modio_debug>=%0)printf(%1)


static
		// Write session data
		modio_wHeader			[MAX_MODIO_SESSION][MAX_MODIO_HEADER_SIZE],
		modio_wHeaderIndex		[MAX_MODIO_SESSION],
		modio_wSessionTags		[MAX_MODIO_SESSION][MAX_MODIO_TAGS],
		modio_wSessionTagIndex	[MAX_MODIO_SESSION],
		modio_wStack			[MAX_MODIO_SESSION][MAX_MODIO_STACK_SIZE],
		modio_wStackIndex		[MAX_MODIO_SESSION],
		modio_wSessionPath		[MAX_MODIO_SESSION][MAX_FILE_PATH],
		modio_wActive			[MAX_MODIO_SESSION],
		modio_wFinaliseTimer	[MAX_MODIO_SESSION] = {-1, ...},
		modio_wSessions,
		modio_wLastSession,
bool:	modio_wScriptExiting,

		// Read session data
		modio_rStack			[MAX_MODIO_SESSION][MAX_MODIO_STACK_SIZE],
		modio_rStackIndex		[MAX_MODIO_SESSION],
		modio_rSessionPath		[MAX_MODIO_SESSION][MAX_FILE_PATH],
		modio_rActive			[MAX_MODIO_SESSION],
		modio_rFinaliseTimer	[MAX_MODIO_SESSION] = {-1, ...},
		modio_rSessions,
		modio_rLastSession,

		// Misc
		modio_debug = MODIO_DEBUG;


forward modio_finalise_write(session);
forward modio_finalise_read(session);


/*==============================================================================

	Writing

==============================================================================*/


stock modio_push(file[], tag[5 char], length, data[], forcewrite = false)
{
	new session;

	dp:1("[MODIO:PUSH] Pushing to file '%s'.", file);

	// Check if there are any sessions open
	if(modio_wSessions > 0)
	{
		// Check if this file is already in use
		session = modio_getsession_write(file);

		// If not, register a new session
		if(session == -1)
		{
			session = modio_register_write(file);
			dp:1("[MODIO:SESSION:%02d] Registering new write session", session);
		}
		else
		{
			dp:1("[MODIO:SESSION:%02d] Pushing to existing write session", session);
		}
	}
	else
	{
		// If no sessions are open, register a new one
		session = modio_register_write(file);
		dp:1("[MODIO:SESSION:%02d] Registering first write session", session);
	}

	if(modio_wActive[session] == 0)
	{
		dp:1("[MODIO:SESSION:%02d] Initializing modio stack push", session);
		modio_wActive[session] = 1;

		modio_wHeader[session][0] = MODIO_FILE_STRUCTURE_VERSION;
		modio_wHeader[session][1] = 0;
		modio_wHeaderIndex[session] = 3;
	}

	// Ensure the data doesn't violate the stack size
	if(modio_wStackIndex[session] + length >= MAX_MODIO_STACK_SIZE - 1)
	{
		printf("ERROR: Modio stack violation while trying to push %d cells (%d/%d)", length, modio_wStackIndex[session] + length, MAX_MODIO_STACK_SIZE);
		return 0;
	}

	// Check if the tag has already been used
	dp:4("[MODIO:SESSION:%02d] modio_wSessionTagIndex: %d", session, modio_wSessionTagIndex[session]);
	if(modio_wSessionTagIndex[session] > 0)
	{
		for(new i; i < modio_wSessionTagIndex[session]; i++)
		{
			dp:5("[MODIO:SESSION:%02d] Loop (%d/%d) comparing tags: %d == %d", session, i, modio_wSessionTagIndex[session], tag[0], modio_wSessionTags[session][i]);
			if(tag[0] == modio_wSessionTags[session][i])
			{
				printf("ERROR: Attempted to overwrite tag: '%c%c%c%c'", tag{0},tag{1},tag{2},tag{3});
				return -1;
			}
		}
	}

	// Add the tag data to the header
	modio_wSessionTags[session][modio_wSessionTagIndex[session]++] = tag[0];
	modio_wHeader[session][2] = modio_wSessionTagIndex[session];
	modio_wHeader[session][modio_wHeaderIndex[session]++] = tag[0];
	modio_wHeader[session][modio_wHeaderIndex[session]++] = modio_wHeaderIndex[session] + modio_wStackIndex[session];

	// Concatenate the tag, 4 packed characters into 1 32 bit cell
	modio_wStack[session][modio_wStackIndex[session]] = tag[0];
	modio_wStackIndex[session]++;

	// Concatenate the length of the data chunk
	modio_wStack[session][modio_wStackIndex[session]] = length;
	modio_wStackIndex[session]++;

	// Concatenate the data chunk
	new i;
	while(i < length)
	{
		modio_wStack[session][modio_wStackIndex[session]] = data[i];
		modio_wStackIndex[session]++;
		i++;
	}

	dp:1("[MODIO:SESSION:%02d] Pushing: %d cells under '%c%c%c%c' session: %d remaining: %d phys: %d tags: %d", session, length, tag{0},tag{1},tag{2},tag{3}, modio_wStackIndex[session], MAX_MODIO_STACK_SIZE - modio_wStackIndex[session], modio_wHeaderIndex[session] + modio_wStackIndex[session], modio_wSessionTagIndex[session]);

	if(forcewrite || modio_wScriptExiting)
	{
		dp:2("[MODIO:SESSION:%02d] Forcing write (forcewrite: %d modio_wScriptExiting: %d)", session, forcewrite, modio_wScriptExiting);
		modio_finalise_write(session);
		return 1;
	}

	if(modio_wFinaliseTimer[session] == -1)
	{
		dp:2("[MODIO:SESSION:%02d] Setting timer", session);
		modio_wFinaliseTimer[session] = SetTimerEx("modio_finalise_write", 50, 0, "d", session);
	}

	return 1;
}

stock modio_register_write(file[])
{
	modio_wSessionPath[modio_wSessions][0] = EOS;
	strcat(modio_wSessionPath[modio_wSessions], file);

	return modio_wSessions++;
}

stock modio_getsession_write(file[])
{
	// Loop through the files, but start with the last used one as that is most
	// likely to be the required file.
	new
		i,
		session = modio_wLastSession;

	while(i < modio_wSessions)
	{
		if(!strcmp(file, modio_wSessionPath[session]))
			return session;

		session++;

		if(session == modio_wSessions)
			session = 0;

		i++;
	}

	return -1;
}

#if defined FILTERSCRIPT

	hook OnFilterScriptExit()
	{
		dp:1("[MODIO:HOOK] OnFilterScriptExit");

		modio_wScriptExiting = true;

		return 1;
	}
/*
	public OnFilterScriptExit()
	{
		dp:1("[MODIO:HOOK] OnFilterScriptExit");

		#if defined modio_OnFilterScriptExit
			return modio_OnFilterScriptExit();
		#else
			return 1;
		#endif
	}
	#if defined _ALS_OnFilterScriptExit
		#undef OnFilterScriptExit
	#else
		#define _ALS_OnFilterScriptExit
	#endif
	 
	#define OnFilterScriptExit modio_OnFilterScriptExit
	#if defined modio_OnFilterScriptExit
		forward modio_OnFilterScriptExit();
	#endif
*/
#else

	hook OnGameModeExit()
	{
		dp:1("[MODIO:HOOK] OnGameModeExit");

		modio_wScriptExiting = true;

		return 1;
	}
/*
	public OnGameModeExit()
	{
		dp:1("[MODIO:HOOK] OnGameModeExit");

		#if defined modio_OnGameModeExit
			return modio_OnGameModeExit();
		#else
			return 1;
		#endif
	}
	#if defined _ALS_OnGameModeExit
		#undef OnGameModeExit
	#else
		#define _ALS_OnGameModeExit
	#endif
	 
	#define OnGameModeExit modio_OnGameModeExit
	#if defined modio_OnGameModeExit
		forward modio_OnGameModeExit();
	#endif
*/
#endif

public modio_finalise_write(session)
{
	if(!(0 <= session < modio_wSessions))
		printf("ERROR: Tried to finalise invalid session (%d)", session);

	dp:1("[MODIO:SESSION:%02d] storing: header of size: %d, stack of size %d, tags: %d", session, modio_wHeaderIndex[session], modio_wStackIndex[session], modio_wSessionTagIndex[session]);

	/*
		Update header to reflect new physical tag positions
		At the time of tag data being added to the header, the function is
		completely unaware how many times it will be called afterwards.
		This results in the physical position becoming offset by the amount of
		new tags added after the tag.
	*/
	modio_wHeader[session][1] = modio_wHeaderIndex[session] + modio_wStackIndex[session];

	for(new i = 0, j = 4; i < modio_wSessionTagIndex[session]; i++, j += 2)
	{
		// Add the remainder of the header size on to the physical position
		modio_wHeader[session][j] += (((modio_wSessionTagIndex[session] - 1) - i) * 2) - 2;
	}

	// Write the 'stack' array to the file and free it.
	new File:file = fopen(modio_wSessionPath[session], io_write);

	if(!file)
	{
		printf("ERROR: Opening of '%s' failed.", modio_wSessionPath[session]);
		return;
	}

	// Write the header
	fblockwrite(file, modio_wHeader[session], modio_wHeaderIndex[session]);

	// Write the body
	fblockwrite(file, modio_wStack[session], modio_wStackIndex[session]);

	fclose(file);

	if(modio_debug == 3)
	{
		print("\nHEAD");
		for(new i; i < modio_wHeaderIndex[session]; i++)
			printf("[%02d]: %d", i, modio_wHeader[session][i]);

		print("\nBODY");
		for(new i; i < modio_wStackIndex[session]; i++)
			printf("[%02d]: %d", i+modio_wHeaderIndex[session], modio_wStack[session][i]);
	}

	modio_close_session_write(session);
}

stock modio_close_session_write(session)
{
	if(!(0 <= session < modio_wSessions))
		return 0;

	KillTimer(modio_wFinaliseTimer[session]);

	modio_wHeaderIndex[session] = 0;
	modio_wSessionTagIndex[session] = 0;
	modio_wStackIndex[session] = 0;
	modio_wSessionPath[session][0] = EOS;
	modio_wActive[session] = 0;
	modio_wFinaliseTimer[session] = -1;

	modio_wSessions--;
	modio_wLastSession = 0;

	return 1;
}


/*==============================================================================

	Utility

==============================================================================*/


stock modio_read(file[], tag[5 char], data[])
{
	if(!fexist(file))
		return 0;

	new session;

	dp:1("[MODIO:READ] Reading file '%s'.", file);

	// Check if there are any sessions open
	if(modio_rSessions > 0)
	{
		// Check if this file is already in use
		session = modio_getsession_read(file);

		// If not, register a new session
		if(session == -1)
		{
			session = modio_register_read(file);
			dp:1("[MODIO:SESSION:%02d] Registering new read session", session);
		}
		else
		{
			dp:1("[MODIO:SESSION:%02d] Pushing to existing read session", session);
		}
	}
	else
	{
		// If no sessions are open, register a new one
		session = modio_register_read(file);
		dp:1("[MODIO:SESSION:%02d] Registering first read session", session);
	}

	if(modio_rActive[session] == 0)
	{
		dp:1("[MODIO:SESSION:%02d] Initializing modio stack pull", session);
		modio_rActive[session] = 1;

		new
			File:f,
			tmp[2];

		f = fopen(file, io_read);
		fblockread(f, tmp, 2);

		if(tmp[0] != MODIO_FILE_STRUCTURE_VERSION)
		{
			printf("ERROR: File structure version (%d) does not match modio version (%d)", tmp[0], MODIO_FILE_STRUCTURE_VERSION);
			return 0;
		}

		modio_rStackIndex[session] = fblockread(f, modio_rStack[session], tmp[1]) + 2;

		if(modio_debug == 3)
		{
			printf("[00]: %d", tmp[0]);
			printf("[01]: %d", tmp[1]);

			print("\nHEAD");
			for(new i; i < modio_rStackIndex[session]; i++)
				printf("[%02d]: %d", i, modio_rStack[session][i]);
		}

		fclose(f);

		dp:1("[MODIO:SESSION:%02d] Extracting '%s': %d tags, size: %d", session, file, modio_rStack[session][0], modio_rStackIndex[session]);
	}

	// Search for the tag in the header
	new physpos = -1;
	for(new i; i < modio_rStack[session][0]; i++)
	{
		if(modio_rStack[session][(i * 2) + 1] == tag[0])
		{
			physpos = modio_rStack[session][(i * 2) + 2];
			break;
		}
	}

	dp:1("[MODIO:SESSION:%02d] Reading '%c%c%c%c': %d cells from physpos %d", session, tag{0}, tag{1}, tag{2}, tag{3}, modio_rStack[session][physpos + 1], physpos);

	if(physpos == -1)
	{
		printf("ERROR: Tag '%c%c%c%c' not found in buffer.", tag{0}, tag{1}, tag{2}, tag{3});
		return -1;
	}

	// Extract the data from the physical position
	for(new i; i < modio_rStack[session][physpos + 1]; i++)
		data[i] = modio_rStack[session][physpos + 2 + i];

	// Close the session after any other reads
	if(modio_wFinaliseTimer[session] == -1)
		modio_wFinaliseTimer[session] = SetTimerEx("modio_finalise_read", 50, 0, "d", session);

	// Return the amount of cells read
	return modio_rStack[session][physpos + 1];
}

public modio_finalise_read(session)
{
	KillTimer(modio_rFinaliseTimer[session]);

	modio_rStackIndex[session] = 0;
	modio_rSessionPath[session][0] = EOS;
	modio_rActive[session] = 0;
	modio_rFinaliseTimer[session] = -1;

	modio_rLastSession = 0;
	modio_rSessions--;
}

stock modio_register_read(file[])
{
	modio_rSessionPath[modio_rSessions][0] = EOS;
	strcat(modio_rSessionPath[modio_rSessions], file);

	return modio_rSessions++;
}

stock modio_getsession_read(file[])
{
	// Loop through the files, but start with the last used one as that is most
	// likely to be the required file.
	new
		i,
		session = modio_rLastSession;

	while(i < modio_rSessions)
	{
		if(!strcmp(file, modio_rSessionPath[session]))
			return session;

		session++;

		if(session == modio_rSessions)
			session = 0;

		i++;
	}

	return -1;
}


/*==============================================================================

	Utility

==============================================================================*/

/*
stock modio_reverse_cmp(str1[], str2[])
{
	new
		len1 = strlen(str1),
		len2 = strlen(str2);

	if(len1 != len2)
		return 0;

	for(new i = len1 - 1; i >= 0; i--)
	{
		if(str1[i] != str2[i])
			return 0;
	}

	return 1;
}
*/
#undef dp
