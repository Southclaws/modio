/*==============================================================================


	Southclaw's Modular Binary IO Library

		Description...

	Implement this: https://github.com/Y-Less/YSI-Includes/blob/YSI.tl/YSI_Storage/y_amx.inc#L1079
	or #define TO_4_CHAR(%0,%1,%2,%3) (((%0) & 0xFF << 24) | ((%1) & 0xFF << 16) | ((%2) & 0xFF << 8) | ((%3) & 0xFF))

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


forward modio_finalise_write(session, close_session);
forward modio_finalise_read(session);


/*==============================================================================

	Writing

==============================================================================*/


stock modio_push(file[], tag[5 char], length, data[], forcewrite = false, forceclose = true)
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

	// Add the tag data to the tag list
	modio_wSessionTags[session][modio_wSessionTagIndex[session]++] = tag[0];

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
		modio_finalise_write(session, forceclose);
		return 1;
	}

	if(modio_wFinaliseTimer[session] == -1)
	{
		dp:2("[MODIO:SESSION:%02d] Setting timer", session);
		modio_wFinaliseTimer[session] = SetTimerEx("modio_finalise_write", 50, 0, "dd", session, forceclose);
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

public modio_finalise_write(session, close_session)
{
	if(!(0 <= session < MAX_MODIO_SESSION))
	{
		printf("ERROR: Tried to finalise invalid session (%d)", session);
		return;
	}

	// First tag physpos not offset from the start of the body.
	modio_wHeader[session][3] = modio_wSessionTags[session][0];
	modio_wHeader[session][4] = 0;
	modio_wHeaderIndex[session] = 5;

	dp:5("[MODIO:SESSION:%02d] Header tag 0: %d", session, modio_wHeader[session][4]);

	// i = tag index, j = tag physpos, k = data physpos
	// This loop starts on tag 1 at physpos 5 since the first tag is set above.
	for(new i = 1, j = 5; i < modio_wSessionTagIndex[session]; i++, j += 2)
	{
		// This cell contains the tag
		modio_wHeader[session][j] = modio_wSessionTags[session][i];

		// The next cell contains the tag's data's offset from the top of the
		// body of the file (the first tag is always at position 0)
		// I've never indented code like this but it makes it easier to comment!
		modio_wHeader[session][j + 1] =
			// This position is offset from the position of the previous tag
			// We start by getting that position from the previous cell
			modio_wHeader[session][j - 1] +
			// Then the size of that block needs to be added
			// The size of a block is stored right after the tag in the body.
			modio_wStack[session][
				// We use the previous cell again to get the position of the
				// block in the file
				modio_wHeader[session][j - 1] +
				// Then add 1 because the size of the block is in the next cell
				1
			] +
			// Then add 2 since the size of the block is only the size of the
			// data IN the block, the extra 2 cells contain the tag and the
			// block size.
			2;

		// Advance the header index by 2 (one for the tag and one for the pos)
		modio_wHeaderIndex[session] += 2;

		dp:5("[MODIO:SESSION:%02d] Header tag %d: %d offset: %d", session, i, modio_wHeader[session][j], modio_wStack[session][ modio_wHeader[session][j - 3] + 1 ]);
	}

	dp:1("[MODIO:SESSION:%02d] storing: header of size: %d, stack of size %d, tags: %d", session, modio_wHeaderIndex[session], modio_wStackIndex[session], modio_wSessionTagIndex[session]);

	// Assign sizes; done afterwards so wHeaderIndex is the correct value.
	modio_wHeader[session][1] = modio_wHeaderIndex[session] + modio_wStackIndex[session];
	modio_wHeader[session][2] = modio_wSessionTagIndex[session];

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

	if(modio_debug >= 4)
	{
		for(new i; i < modio_wHeaderIndex[session]; i++)
			printf("[MODIO:SESSION:%02d] HEAD: [%02d]: %d", session, i, modio_wHeader[session][i]);

		for(new i; i < modio_wStackIndex[session]; i++)
			printf("[MODIO:SESSION:%02d] BODY: [%02d] [%02d]: %d", session, i+modio_wHeaderIndex[session], i, modio_wStack[session][i]);
	}

	if(close_session)
		modio_close_session_write(session);
}

stock modio_close_session_write(session)
{
	dp:1("[MODIO:SESSION:%02d] Closing session", session);

	if(!(0 <= session < MAX_MODIO_SESSION))
	{
		print("ERROR: Tried to close invalid session.");
		return 0;
	}

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

	Reading

==============================================================================*/


stock modio_read(file[], tag[5 char], data[], forceclose = false)
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
			dp:1("[MODIO:SESSION:%02d] Using existing read session", session);
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

		if(modio_debug >= 4)
		{
			printf("[MODIO:SESSION:%02d] READ [00]: %d", session, tmp[0]);
			printf("[MODIO:SESSION:%02d] READ [01]: %d", session, tmp[1]);

			for(new i; i < modio_rStackIndex[session]; i++)
				printf("[MODIO:SESSION:%02d] READ [%02d]: %d", session, i, modio_rStack[session][i]);
		}

		fclose(f);

		dp:1("[MODIO:SESSION:%02d] Extracting '%s': %d tags, size: %d", session, file, modio_rStack[session][0], modio_rStackIndex[session]);
	}

	// Search for the tag in the header
	new physpos = -1;
	for(new i; i < modio_rStack[session][0]; i++)
	{
		dp:5("[MODIO:SESSION:%02d] Loop (%d/%d) comparing tags: %d == %d", session, i, modio_rStack[session][0], tag[0], modio_rStack[session][(i * 2) + 1]);

		if(modio_rStack[session][(i * 2) + 1] == tag[0])
		{
			physpos = modio_rStack[session][(i * 2) + 2] + (modio_rStack[session][0] * 2) + 1;
			dp:5("[MODIO:SESSION:%02d] Found tag, headersize: %d, physpos: %d", session, (modio_rStack[session][0] * 2) + 1, physpos);
			break;
		}
	}

	dp:1("[MODIO:SESSION:%02d] Reading '%c%c%c%c': %d cells from physpos %d", session, tag{0}, tag{1}, tag{2}, tag{3}, modio_rStack[session][physpos + 1], physpos);

	if(physpos == -1)
	{
		printf("ERROR: Tag '%c%c%c%c' not found in buffer.", tag{0}, tag{1}, tag{2}, tag{3});
		return 0;
	}

	// Extract the data from the physical position
	// Could use memcpy here but it needs a speed test
	for(new i; i < modio_rStack[session][physpos + 1]; i++)
		data[i] = modio_rStack[session][physpos + 2 + i];

	if(forceclose)
	{
		dp:2("[MODIO:SESSION:%02d] Force-closing read session.", session);
		modio_finalise_read(session);
		return modio_rStack[session][physpos + 1];
	}

	// Close the session after any other reads
	if(modio_rFinaliseTimer[session] == -1)
		modio_rFinaliseTimer[session] = SetTimerEx("modio_finalise_read", 50, 0, "d", session);

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

#undef dp
