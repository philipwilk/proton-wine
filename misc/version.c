/*
 * Windows and DOS version functions
 *
 * Copyright 1997 Alexandre Julliard
 * Copyright 1997 Marcus Meissner
 * Copyright 1998 Patrik Stridvall
 * Copyright 1998 Andreas Mohr
 */

#include <string.h>
#include <stdlib.h>
#include "windows.h"
#include "winbase.h"
#include "process.h"
#include "options.h"
#include "debug.h"
#include "ole.h"
#include "winversion.h"

typedef struct
{
    LONG             getVersion16; 
    LONG             getVersion32;
    OSVERSIONINFO32A getVersionEx;
} VERSION_DATA;


/* FIXME: compare values below with original and fix */
static VERSION_DATA VersionData[NB_WINDOWS_VERSIONS] =
{
    /* WIN31 */
    {
	MAKELONG( 0x0a03, 0x0616 ), /* DOS 6.22 */
	MAKELONG( 0x0a03, 0x8000 ),
	{
            sizeof(OSVERSIONINFO32A), 3, 10, 0,
            VER_PLATFORM_WIN32s, "Win32s 1.3" 
	}
    },
    /* WIN95 */
    {
        0x07005F03,
        0xC0000004,
	{
            sizeof(OSVERSIONINFO32A), 4, 0, 0x40003B6,
            VER_PLATFORM_WIN32_WINDOWS, "Win95"
	}
    },
    /* NT351 */
    {
        0x05000A03,
        0x04213303,
        {
            sizeof(OSVERSIONINFO32A), 3, 51, 0x421,
            VER_PLATFORM_WIN32_NT, "Service Pack 2"
	}
    },
    /* NT40 */
    {
        0x05000A03,
        0x05650004,
        {
            sizeof(OSVERSIONINFO32A), 4, 0, 0x565,
            VER_PLATFORM_WIN32_NT, "Service Pack 3"
        }
    }
};

static const char *WinVersionNames[NB_WINDOWS_VERSIONS] =
{
    "win31",
    "win95",
    "nt351",
    "nt40"
};

/* the current version has not been autodetected but forced via cmdline */
static BOOL32 versionForced = FALSE;
static WINDOWS_VERSION defaultWinVersion = WIN31;


/**********************************************************************
 *         VERSION_ParseWinVersion
 */
void VERSION_ParseWinVersion( const char *arg )
{
    int i;
    for (i = 0; i < NB_WINDOWS_VERSIONS; i++)
    {
        if (!strcmp( WinVersionNames[i], arg ))
        {
            defaultWinVersion = (WINDOWS_VERSION)i;
            versionForced = TRUE;
            return;
        }
    }
    MSG("Invalid winver value '%s' specified.\n", arg );
    MSG("Valid versions are:" );
    for (i = 0; i < NB_WINDOWS_VERSIONS; i++)
        MSG(" '%s'%c", WinVersionNames[i],
	    (i == NB_WINDOWS_VERSIONS - 1) ? '\n' : ',' );
}


/**********************************************************************
 *         VERSION_ParseDosVersion
 */
void VERSION_ParseDosVersion( const char *arg )
{
    int hi, lo;
    if (sscanf( arg, "%d.%d", &hi, &lo ) == 2)
    {
        VersionData[WIN31].getVersion16 =
            MAKELONG(LOWORD(VersionData[WIN31].getVersion16),
                     (hi<<8) + lo);
    }
    else
        fprintf( stderr, "-dosver: Wrong version format. Use \"-dosver x.xx\"\n");
}


/**********************************************************************
 *         VERSION_GetVersion
 */
WINDOWS_VERSION VERSION_GetVersion(void)
{
    PIMAGE_NT_HEADERS peheader;	

    if (versionForced) /* user has overridden any sensible checks */
        return defaultWinVersion;
    if (!PROCESS_Current()->exe_modref)
    {
        /* HACK: if we have loaded a PE image into this address space,
         * we are probably using thunks, so Win95 is our best bet
         */
        if (PROCESS_Current()->modref_list) return WIN95;
        return WIN31; /* FIXME: hmm, look at DDB.version ? */
    }
    peheader = PE_HEADER(PROCESS_Current()->exe_modref->module);
    if (peheader->OptionalHeader.MajorSubsystemVersion == 4) {
        /* FIXME: check probably not 100% good, verify with win98 too */
	if (peheader->OptionalHeader.MajorOperatingSystemVersion == 4)
	    return NT40;
        return WIN95;
    }
    if (peheader->OptionalHeader.MajorSubsystemVersion == 3)
    {
        /* Win3.10 */
        if (peheader->OptionalHeader.MinorSubsystemVersion <= 11) return WIN31;
        /* NT 3.51 */
        if (peheader->OptionalHeader.MinorSubsystemVersion == 50) return NT351;
        if (peheader->OptionalHeader.MinorSubsystemVersion == 51) return NT351;
    }
    if (peheader->OptionalHeader.MajorSubsystemVersion)
	ERR(ver,"unknown subsystem version: %04x.%04x, please report.\n",
	    peheader->OptionalHeader.MajorSubsystemVersion,
	    peheader->OptionalHeader.MinorSubsystemVersion );
    return defaultWinVersion;
}


/**********************************************************************
 *         VERSION_GetVersionName
 */
char *VERSION_GetVersionName()
{
  WINDOWS_VERSION ver = VERSION_GetVersion();
  switch(ver)
    {
    case WIN31:
      return "Windows 3.1";
    case WIN95:  
      return "Windows 95";
    case NT351:
      return "Windows NT 3.51";
    case NT40:
      return "Windows NT 4.0";
    default:
      FIXME(ver,"Windows version %d not named",ver);
      return "Windows <Unknown>";
    }
}

/***********************************************************************
 *         GetVersion16   (KERNEL.3)
 */
LONG WINAPI GetVersion16(void)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    return VersionData[ver].getVersion16;
}


/***********************************************************************
 *         GetVersion32   (KERNEL32.427)
 */
LONG WINAPI GetVersion32(void)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    return VersionData[ver].getVersion32;
}


/***********************************************************************
 *         GetVersionEx16   (KERNEL.149)
 */
BOOL16 WINAPI GetVersionEx16(OSVERSIONINFO16 *v)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    if (v->dwOSVersionInfoSize != sizeof(OSVERSIONINFO16))
    {
        WARN(ver,"wrong OSVERSIONINFO size from app");
        return FALSE;
    }
    v->dwMajorVersion = VersionData[ver].getVersionEx.dwMajorVersion;
    v->dwMinorVersion = VersionData[ver].getVersionEx.dwMinorVersion;
    v->dwBuildNumber  = VersionData[ver].getVersionEx.dwBuildNumber;
    v->dwPlatformId   = VersionData[ver].getVersionEx.dwPlatformId;
    strcpy( v->szCSDVersion, VersionData[ver].getVersionEx.szCSDVersion );
    return TRUE;
}


/***********************************************************************
 *         GetVersionEx32A   (KERNEL32.428)
 */
BOOL32 WINAPI GetVersionEx32A(OSVERSIONINFO32A *v)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();
    if (v->dwOSVersionInfoSize != sizeof(OSVERSIONINFO32A))
    {
        WARN(ver,"wrong OSVERSIONINFO size from app");
        return FALSE;
    }
    v->dwMajorVersion = VersionData[ver].getVersionEx.dwMajorVersion;
    v->dwMinorVersion = VersionData[ver].getVersionEx.dwMinorVersion;
    v->dwBuildNumber  = VersionData[ver].getVersionEx.dwBuildNumber;
    v->dwPlatformId   = VersionData[ver].getVersionEx.dwPlatformId;
    strcpy( v->szCSDVersion, VersionData[ver].getVersionEx.szCSDVersion );
    return TRUE;
}


/***********************************************************************
 *         GetVersionEx32W   (KERNEL32.429)
 */
BOOL32 WINAPI GetVersionEx32W(OSVERSIONINFO32W *v)
{
    WINDOWS_VERSION ver = VERSION_GetVersion();

    if (v->dwOSVersionInfoSize!=sizeof(OSVERSIONINFO32W))
    {
        WARN(ver,"wrong OSVERSIONINFO size from app");
        return FALSE;
    }
    v->dwMajorVersion = VersionData[ver].getVersionEx.dwMajorVersion;
    v->dwMinorVersion = VersionData[ver].getVersionEx.dwMinorVersion;
    v->dwBuildNumber  = VersionData[ver].getVersionEx.dwBuildNumber;
    v->dwPlatformId   = VersionData[ver].getVersionEx.dwPlatformId;
    lstrcpyAtoW( v->szCSDVersion, VersionData[ver].getVersionEx.szCSDVersion );
    return TRUE;
}


/***********************************************************************
 *	    GetWinFlags   (KERNEL.132)
 */
DWORD WINAPI GetWinFlags(void)
{
  static const long cpuflags[5] =
    { WF_CPU086, WF_CPU186, WF_CPU286, WF_CPU386, WF_CPU486 };
  SYSTEM_INFO si;
  OSVERSIONINFO32A ovi;
  DWORD result;

  GetSystemInfo(&si);

  /* There doesn't seem to be any Pentium flag.  */
  result = cpuflags[MIN (si.wProcessorLevel, 4)];

  switch(Options.mode)
  {
  case MODE_STANDARD:
      result |= WF_STANDARD | WF_PMODE | WF_80x87;
      break;

  case MODE_ENHANCED:
      result |= WF_ENHANCED | WF_PMODE | WF_80x87 | WF_PAGING;
      break;

  default:
      ERR(ver, "Unknown mode set? This shouldn't happen. Check GetWinFlags()!\n");
      break;
  }
  if (si.wProcessorLevel >= 4) result |= WF_HASCPUID;
  ovi.dwOSVersionInfoSize = sizeof(ovi);
  GetVersionEx32A(&ovi);
  if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT)
      result |= WF_WIN32WOW; /* undocumented WF_WINNT */
  return result;
}


/***********************************************************************
 *	    GetWinDebugInfo   (KERNEL.355)
 */
BOOL16 WINAPI GetWinDebugInfo(WINDEBUGINFO *lpwdi, UINT16 flags)
{
    FIXME(ver, "(%8lx,%d): stub returning 0\n",
	  (unsigned long)lpwdi, flags);
    /* 0 means not in debugging mode/version */
    /* Can this type of debugging be used in wine ? */
    /* Constants: WDI_OPTIONS WDI_FILTER WDI_ALLOCBREAK */
    return 0;
}


/***********************************************************************
 *	    SetWinDebugInfo   (KERNEL.356)
 */
BOOL16 WINAPI SetWinDebugInfo(WINDEBUGINFO *lpwdi)
{
    FIXME(ver, "(%8lx): stub returning 0\n", (unsigned long)lpwdi);
    /* 0 means not in debugging mode/version */
    /* Can this type of debugging be used in wine ? */
    /* Constants: WDI_OPTIONS WDI_FILTER WDI_ALLOCBREAK */
    return 0;
}


/***********************************************************************
 *           DebugFillBuffer                    (KERNEL.329)
 *
 * TODO:
 * Should fill lpBuffer only if DBO_BUFFERFILL has been set by SetWinDebugInfo()
 */
void WINAPI DebugFillBuffer(LPSTR lpBuffer, WORD wBytes)
{
	memset(lpBuffer, DBGFILL_BUFFER, wBytes);
}

/***********************************************************************
 *           DiagQuery                          (KERNEL.339)
 *
 * returns TRUE if Win called with "/b" (bootlog.txt)
 */
BOOL16 WINAPI DiagQuery()
{
	/* perhaps implement a Wine "/b" command line flag sometime ? */
	return FALSE;
}

/***********************************************************************
 *           DiagOutput                         (KERNEL.340)
 *
 * writes a debug string into <windir>\bootlog.txt
 */
void WINAPI DiagOutput(LPCSTR str)
{
        /* FIXME */
	DPRINTF("DIAGOUTPUT:%s\n", debugstr_a(str));
}

/***********************************************************************
 *           OaBuildVersion           [OLEAUT32.170]
 */
UINT32 WINAPI OaBuildVersion()
{
    WINDOWS_VERSION ver = VERSION_GetVersion();

    FIXME(ver, "Please report to a.mohr@mailto.de if you get version error messages !\n");
    switch(VersionData[ver].getVersion32)
    {
        case 0x80000a03: /* Win 3.1 */
		return 0x140fd1; /* from Win32s 1.1e */
        case 0xc0000004: /* Win 95 */
		return 0x1e10a9; /* some older version: 0x0a0bd3 */
        case 0x04213303: /* NT 3.51 */
		FIXME(ver, "NT 3.51 version value unknown !\n");
		return 0x1e10a9; /* value borrowed from Win95 */
        case 0x05650004: /* NT 4.0 */
		return 0x141016;
	default:
		return 0x0;
    }
}
/***********************************************************************
 *        VERSION_OsIsUnicode	[internal]
 *
 * NOTES
 *   some functions getting sometimes LPSTR sometimes LPWSTR...
 *
 */
BOOL32 VERSION_OsIsUnicode(void)
{
    switch(VERSION_GetVersion())
    {
    case NT351:
    case NT40:
        return TRUE;
    default:
        return FALSE;
    }
}
