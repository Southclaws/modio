# What is modio?

"modio" (modular IO) is a file reader/writer library for SA:MP that takes advantage of fblockread and fblockwrite.

How many numbers are on a clock?

![http://i.imgur.com/8NH3Vsg.png](http://i.imgur.com/8NH3Vsg.png)

A plaintext format such as INI or JSON would answer: 15
A binary file format such as modio would answer 12

Hopefully that analogy hooked your interest in this library!

This library is designed for modular gamemodes. Gamemodes that have a lot of different parts that must save to a single file. I wrote it with y_hooks/ALS in mind. For example: calling the modio write function to save player data in 10 OnPlayerDisconnect hooks will all save to the file using a single write operation, pretty cool huh?

# Example of Use

*module #1*
```cpp
static
    mod1_Data[MAX_PLAYERS][10]; // 10 cells are stored with each player


hook OnPlayerConnect(playerid)
{
    new
        name[MAX_PLAYER_NAME],
        filename[35];

    GetPlayerName(playerid, name, MAX_PLAYER_NAME);
    format(filename, 35, "users/%s.dat", name);

    modio_read(filename, _T<M,O,D,1>, 10, mod1_Data[playerid]);

    return 1;
}

hook OnPlayerDisconnect(playerid, reason)
{
    new
        name[MAX_PLAYER_NAME],
        filename[35];

    GetPlayerName(playerid, name, MAX_PLAYER_NAME);
    format(filename, 35, "users/%s.dat", name);

    modio_push(filename, _T<M,O,D,1>, 10, mod1_Data[playerid]);

    return 1;
}
```

*module #2*
```cpp
static
    mod2_Data[MAX_PLAYERS][64]; // 64 cells are stored with each player


hook OnPlayerConnect(playerid)
{
    new
        name[MAX_PLAYER_NAME],
        filename[35];

    GetPlayerName(playerid, name, MAX_PLAYER_NAME);
    format(filename, 35, "users/%s.dat", name);

    modio_read(filename, _T<M,O,D,2>, 64, mod1_Data[playerid]);

    return 1;
}

hook OnPlayerDisconnect(playerid, reason)
{
    new
        name[MAX_PLAYER_NAME],
        filename[35];

    GetPlayerName(playerid, name, MAX_PLAYER_NAME);
    format(filename, 35, "users/%s.dat", name);

    modio_push(filename, _T<M,O,D,2>, 64, mod1_Data[playerid]);

    return 1;
}
```

Here we can see two different scripts hooking OnPlayerConnect and OnPlayerDisconnect each pushing some data related to the player that is unique and specific to that script. The file is only opened and written to once because modio is clever, it knows when the last push is called.

The process is exactly the same for reading, one simple function call in each hook.


# How it works

Each piece of data is tagged with a 4 character (32 bit) cell so sections of data from different scripts can be named (that's what the _T&lt;M,O,D,1&gt; and _T&lt;M,O,D,2&gt; things were. The weird syntax packs the characters into a single cell.)

The data is stored in a partially non-order-dependent structure since the hook order is usually indeterminate anyway so the tags are used to search for data.

This is what a modio file looks like:
(Actual files are binary, this just represents the structure.)
```
cell            bytes

HEADER
filever         4
numbyte         4
numtags         4
taglist         numtags * 8
[
    tagname     4
    physpos     4
]

BODY
tagbody         numtags * (n tagsize)
[
    tagname     4
    tagsize     4
    tagdata     tagsize * 4
]

```
I'll go through each section and explain it:

### Header

- filever (1 cell)
  A single number which identifies which version of modio is required to read the file.

- numbyte (1 cell)
  The size of the header and body in cells (4 bytes per cell).

- numtags (1 cell)
  The number of tags/data sections in the file.

- taglist (numtags * 2 cells)
  A list of tags in the file; each item in this list has two elements:
  - tagname (1 cell)
    The 4 character tag name
  - physpos (1 cell)
    The physical position of the actual data block (offset from first body cell. First tag always has a physpos of 0)


### Body

- data block
  - tagname (1 cell)
    The 4 character tag name
  - tagsize (1 cell)
    The amount of cells stored in the block (not including these first two cells)
  - tagdata (tagsize cells)
    The actual data


# Development

This is still in very early development and can be improved upon a lot.
There is a demo gamemode available in my repository that examples this and some SIF features in a modular structure.

- modgm: [https://github.com/Southclaw/modgm-demo ![http://i.imgur.com/l7gBY92.png](http://i.imgur.com/l7gBY92.png)](https://github.com/Southclaw/modgm-demo)


# Known Issues

- modio_push in OnGameModeExit on gamemode restarts/exits
  Since the write function uses a very short timer to perform a single write after all of the modio_push calls are done, calling it on OnGameModeExit means the timer is never called since the script stops completely after this callback. The current fix for this is to just write the entire file each time modio_push is called.


- modio_push in OnPlayerDisconnect on gamemode restarts/exits
  Saving data on OnPlayerDisconnect is also problematic when restarting or exiting the gamemode. The fix for this is manual unfortunately: the user must write a loop in OnGameModeExit that calls modio_finalise_write for each user file being written to.
