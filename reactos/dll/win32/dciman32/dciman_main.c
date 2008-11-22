
#include <windows.h>
#include <ndk/ntndk.h>
#include <stdio.h>
#include <dciman.h>
#include <ddraw.h>
#include <ddrawi.h>
#include <d3dhal.h>
#include <ddrawgdi.h>
#include <pseh/pseh.h>

CRITICAL_SECTION ddcs;

BOOL
WINAPI
DllMain( HINSTANCE hModule, DWORD ul_reason_for_call, LPVOID reserved )
{
    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_DETACH:
            DeleteCriticalSection( &ddcs );
            break;

        case DLL_PROCESS_ATTACH:
            InitializeCriticalSection( &ddcs );
            break;
    }
    return TRUE;
}

/*++
* @name HDC WINAPI DCIOpenProvider(void)
* @implemented
*
* The function DCIOpenProvider are almost same as CreateDC, it is more simple to use,
* you do not need type which monitor or graphic card you want to use, it will use
*  the current one that the program is running on, so it simple create a hdc, and
*  you can use gdi32 api without any problem

* @return
* Returns a hdc that it have create by gdi32.dll CreateDC
*
* @remarks.
* None
*
*--*/
HDC WINAPI
DCIOpenProvider(void)
{
    HDC hDC;
    if (GetSystemMetrics(SM_CMONITORS) > 1)
    {
        /* FIXME we need add more that one mointor support for DCIOpenProvider, need exaime how */
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return NULL;
    }
    else
    {
        /* Note Windows using the word "Display" not with big letter to create the hdc
         * MS gdi32 does not care if the Display spell with big or small letters */

        /* Note it only create a hDC handle and send it back. */
        hDC = CreateDCW(L"Display",NULL, NULL, NULL);
    }

    return hDC;
}

/*++
* @name void WINAPI DCICloseProvider(HDC hdc)
* @implemented
*
* Same as DeleteDC, it call indirecly to DeleteDC, it simple free and destory the hdc.

* @return
* None
*
* @remarks.
* None
*
*--*/
void WINAPI
DCICloseProvider(HDC hDC)
{
    /* Note it only delete the hdc */
    DeleteDC(hDC);
}


typedef struct _DCISURFACE_LCL
{
    BOOL LostSurface;
    DDRAWI_DIRECTDRAW_GBL DirectDrawGlobal;
    DDRAWI_DDRAWSURFACE_LCL SurfaceLocal;
    DDHAL_DDCALLBACKS DDCallbacks;
    DDHAL_DDSURFACECALLBACKS DDSurfaceCallbacks;
} DCISURFACE_LCL, *LPDCISURFACE_LCL;

typedef struct _DCISURFACE_INT
{
    DCISURFACE_LCL DciSurface_lcl;
    DCISURFACEINFO DciSurfaceInfo;
} DCISURFACE_INT, *LPDCISURFACE_INT;


int WINAPI 
DCICreatePrimary(HDC hDC, LPDCISURFACEINFO *pDciSurfaceInfo)
{
    int retvalue = DCI_OK;
    BOOL bNewMode = FALSE;
    LPDCISURFACE_INT pDciSurface_int;
    DDHALINFO HalInfo;
    DDHAL_DDPALETTECALLBACKS DDPaletteCallbacks;

    /* Alloc memory for the internal struct, rember this internal struct are not compatible with windows xp */
    if ( (pDciSurface_int = (LPDCISURFACE_INT) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DCISURFACE_INT) )) == NULL )
    {
        /* Fail alloc memmory */
        retvalue = DCI_ERR_OUTOFMEMORY;
    }
    else
    {
        /* Activate the DirectX support from HAL, if any of these function fail, we was not avail setup infomrations we need to create our frame */
        if ( ( DdCreateDirectDrawObject( &pDciSurface_int->DciSurface_lcl.DirectDrawGlobal,
                                         hDC) == FALSE) ||
             ( DdReenableDirectDrawObject(&pDciSurface_int->DciSurface_lcl.DirectDrawGlobal,
                                      &bNewMode) == FALSE) ||
             ( DdQueryDirectDrawObject( &pDciSurface_int->DciSurface_lcl.DirectDrawGlobal,
                                        &HalInfo,
                                        &pDciSurface_int->DciSurface_lcl.DDCallbacks, 
                                        &pDciSurface_int->DciSurface_lcl.DDSurfaceCallbacks,
                                        &DDPaletteCallbacks,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL,
                                        NULL) == FALSE) )
        {
            /* Fail to get importment Directx informations */
            retvalue = DCI_FAIL_UNSUPPORTED;
        }
        else
        {
            /* We got all informations to start create our primary surface */

            /* Will be use later, it check see if we lost the surface or not */
            pDciSurface_int->DciSurface_lcl.LostSurface = FALSE;

            /* FIXME Add the real code now to create the primary surface after we create the dx hal interface */

            /* Fill in the size of DCISURFACEINFO struct */
            pDciSurface_int->DciSurfaceInfo.dwSize = sizeof(DCISURFACEINFO);

            /* Fill in the surface is visible, for it always are the primary surface and it is visible */
            pDciSurface_int->DciSurfaceInfo.dwDCICaps = DCI_VISIBLE;

            /* for primary screen res lower that 256Color, ms dciman.dll set this value to BI_RGB 
             * old note I found in ddk header comment follow dwMask is type for BI_BITMASK surfaces
             * and for dwCompression it was which format the surface was create in, I also tested with older graphic card like S3
             * that support 16Color or lower for the primary display. and saw this member change betwin 0 (BI_RGB) and 3 (BI_BITFIELDS).
             */
            if (HalInfo.vmiData.ddpfDisplay.dwRGBBitCount >= 8)
            {
                pDciSurface_int->DciSurfaceInfo.dwCompression = BI_BITFIELDS;
            }
            else
            {
                pDciSurface_int->DciSurfaceInfo.dwCompression = BI_RGB;
            }

            /* Fill in the RGB mask */
            pDciSurface_int->DciSurfaceInfo.dwMask[0] = HalInfo.vmiData.ddpfDisplay.dwRBitMask;
            pDciSurface_int->DciSurfaceInfo.dwMask[1] = HalInfo.vmiData.ddpfDisplay.dwGBitMask;
            pDciSurface_int->DciSurfaceInfo.dwMask[2] = HalInfo.vmiData.ddpfDisplay.dwBBitMask;

            /* Fill in the width and height of the primary surface */
            pDciSurface_int->DciSurfaceInfo.dwWidth = HalInfo.vmiData.dwDisplayWidth;
            pDciSurface_int->DciSurfaceInfo.dwHeight = HalInfo.vmiData.dwDisplayHeight;

            /* Fill in the color deep and stride of the primary surface */
            pDciSurface_int->DciSurfaceInfo.dwBitCount = HalInfo.vmiData.ddpfDisplay.dwRGBBitCount;
            pDciSurface_int->DciSurfaceInfo.lStride = HalInfo.vmiData.lDisplayPitch;

            *pDciSurfaceInfo = (LPDCISURFACEINFO) &pDciSurface_int->DciSurfaceInfo;
        }
    }

/* Cleanup code when we fail */
    if (retvalue < DCI_OK)
    {
        if (pDciSurface_int->DciSurface_lcl.DirectDrawGlobal.hDD != 0)
        {
            DdDeleteDirectDrawObject(&pDciSurface_int->DciSurface_lcl.DirectDrawGlobal);
        }

        if (pDciSurface_int != NULL)
        {
            HeapFree(GetProcessHeap(), 0, pDciSurface_int);
        }
    }

    return retvalue;
}



