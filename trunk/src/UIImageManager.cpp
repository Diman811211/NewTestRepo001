//////////////////////////////////////////////////////////////////////////
//
//
//	UIImageManager.cpp : implementation of the CUIImageManager class.
//
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <math.h>

#include "UIVC80Helpers.h"
#include "UIColorManager.h"
#include "UIDrawHelpers.h"
#include "UISystemHelpers.h"
#include "UIMacros.h"
#include "UIPropExchange.h"
#include "UIImageManager.h"
#include "UIImageManagerResource.h"
#include "UIResourceManager.h"

#ifdef _UI_INCLUDE_GRAPHICLIBRARY
#include "GraphicLibrary/UIGraphicBitmapPng.h"
#endif

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define DESTROYICON(hIcon) if (hIcon) { ::DestroyIcon(hIcon); hIcon = 0; }
#define FREE(hHandle) if (hHandle) { free(hHandle); hHandle = 0; }

double CUIImageManager::m_dDisabledAlphaFactor = 1.0;
double CUIImageManager::m_dDisabledBrightnessFactor = 0.5;
BOOL CUIImageManager::m_bAutoResample = FALSE;
CLIPFORMAT CUIImageManager::m_nImageClipFormat = (CLIPFORMAT)RegisterClipboardFormat(_T("Xtreme ToolBar Image"));
CLIPFORMAT CUIImageManager::m_nAlphaClipFormat = (CLIPFORMAT)RegisterClipboardFormat(_T("Alpha Bitmap Image"));

#ifndef NOMIRRORBITMAP
#define NOMIRRORBITMAP      (DWORD)0x80000000 /* Do not Mirror the bitmap in this call */
#endif

#define  IMAGE_PLACEHOLDER 0xDEADBEAF

//---------------------------------------------------------------------------
//    Should be a prime number:
// 37, 53, 79 , 101, 127, 199, 503, 1021, 1511, 2003, 3001, 4001, 5003, 6007, 8009, 12007, 16001, 32003, 48017, 64007
#define UI_IMAGEMAN_HASH_TABLE_SIZE 127

//////////////////////////////////////////////////////////////////////////
// Common

// Based on http://support.microsoft.com/kb/303916/en-us
BOOL CUIImageManager::BitmapsCompatible(LPBITMAP lpbm1, LPBITMAP lpbm2) const
{
	if (lpbm1->bmBitsPixel != lpbm2->bmBitsPixel)
		return FALSE;

	if (lpbm1->bmPlanes != lpbm2->bmPlanes)
		return FALSE;

	if (lpbm1->bmWidth != lpbm2->bmWidth)
		return FALSE;

	if (lpbm1->bmHeight != lpbm2->bmHeight)
		return FALSE;

	return TRUE;
}

// Based on http://www.rsdn.ru/article/files/Functions/mctranspblt.xml
BOOL CUIImageManager::McTransparentBlt(HDC hdcDest, int nXOriginDest, int nYOriginDest,
	int nWidthDest, int nHeightDest, HDC hdcSrc,
	int nXOriginSrc, int nYOriginSrc, int nWidthSrc,
	int nHeightSrc, UINT crTransparent)
{

	BOOL bResult = FALSE;

	if (!hdcDest || !hdcSrc)
		return FALSE;

	HDC hdcMask = NULL;
	HDC hdcTmpSrc = NULL;
	HBITMAP hbmTransMask = NULL;
	HBITMAP oldhbmTransMask = NULL;
	HBITMAP hbmTmpSrc = NULL;
	HBITMAP oldhbmTmpSrc = NULL;


	COLORREF OldBkColor = ::SetBkColor(hdcDest, RGB(255, 255, 255));
	COLORREF OldTextColor = ::SetTextColor(hdcDest, RGB(0, 0, 0));

	if ((hdcMask = ::CreateCompatibleDC(hdcDest)) == NULL)
	{
		goto ClearUp;
	}

	if ((hdcTmpSrc = ::CreateCompatibleDC(hdcSrc))==NULL)
	{
		goto ClearUp;
	}

	if ((hbmTmpSrc = ::CreateCompatibleBitmap(hdcDest,nWidthSrc,nHeightSrc))==NULL)
	{
		goto ClearUp;
	}

	oldhbmTmpSrc = (HBITMAP)::SelectObject(hdcTmpSrc,hbmTmpSrc);

	if (!::BitBlt(hdcTmpSrc, 0, 0, nWidthSrc, nHeightSrc, hdcSrc, nXOriginSrc, nYOriginSrc, SRCCOPY))
	{
		goto ClearUp;
	}

	if ((hbmTransMask = ::CreateBitmap(nWidthSrc, nHeightSrc, 1, 1, NULL)) == NULL)
	{
		goto ClearUp;
	}

	oldhbmTransMask = (HBITMAP)::SelectObject(hdcMask,hbmTransMask);

	::SetBkColor(hdcTmpSrc,crTransparent);
	if (!::BitBlt(hdcMask, 0, 0, nWidthSrc, nHeightSrc, hdcTmpSrc, 0, 0, SRCCOPY))
	{
		goto ClearUp;
	}

	if (crTransparent != RGB(0, 0, 0))
	{
		::SetBkColor(hdcTmpSrc, RGB(0, 0, 0));
		::SetTextColor(hdcTmpSrc, RGB(255, 255, 255));

		if (!::BitBlt(hdcTmpSrc, 0, 0, nWidthSrc, nHeightSrc, hdcMask, 0, 0, SRCAND))
		{
			goto ClearUp;
		}
	}

	if ((nWidthDest == nWidthSrc) && (nHeightDest == nHeightSrc))
	{
		if (!::BitBlt(hdcDest, nXOriginDest, nYOriginDest, nWidthSrc, nHeightSrc, hdcMask, 0, 0, SRCAND))
		{
			goto ClearUp;
		}
		if (!::BitBlt(hdcDest, nXOriginDest, nYOriginDest, nWidthSrc, nHeightSrc, hdcTmpSrc, 0, 0, SRCPAINT))
		{
			goto ClearUp;
		}
	}
	else
	{
		if (!::StretchBlt(hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, hdcMask, 0, 0, nWidthSrc, nHeightSrc,SRCAND))
		{
			goto ClearUp;
		}
		if (!::StretchBlt(hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest, hdcTmpSrc, 0, 0,  nWidthSrc, nHeightSrc,SRCPAINT))
		{
			goto ClearUp;
		}
	}
	bResult = TRUE;

ClearUp:
	if (hdcMask)
	{
		if (oldhbmTransMask)
			::SelectObject(hdcMask,oldhbmTransMask);
		::DeleteDC(hdcMask);
	}

	if (hbmTmpSrc)
	{
		if (oldhbmTmpSrc)
			::SelectObject(hdcTmpSrc,oldhbmTmpSrc);
		::DeleteObject(hbmTmpSrc);
	}

	if (hdcTmpSrc)
		::DeleteDC(hdcTmpSrc);

	if (hbmTransMask)
		::DeleteObject(hbmTransMask);

	::SetBkColor(hdcDest, OldBkColor);
	::SetTextColor(hdcDest,OldTextColor);

	return bResult;
}

BOOL CUIImageManager::IsWindow2000() const
{
	return UISystemVersion()->IsWin2KOrGreater();
}

BOOL CUIImageManager::TransparentBlt(HDC hdcDest, const CRect& rcDest,HDC hdcSrc,
	const CRect& rcSrc, UINT crTransparent) const
{
	if (IsWindow2000() && UIDrawHelpers()->m_pfnTransparentBlt)
	{
		return UIDrawHelpers()->m_pfnTransparentBlt(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
			hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height(), crTransparent);
	}

	return McTransparentBlt(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
		hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height(), crTransparent);
}

// Based on http://support.microsoft.com/kb/303916/en-us
BOOL CUIImageManager::BlendImages(HBITMAP hbmSrc1, BOOL bRTL1, HBITMAP hbmSrc2, BOOL bRTL2, HBITMAP hbmDst) const
{
	BITMAP bmSrc1, bmSrc2, bmDst;
	RGBQUAD *lprgbSrc1, *lprgbSrc2, *lprgbDst;
	DWORD dwWidthBytes;
	int x, y;

	// Get weighting value for second source image.

	// Get information about the surfaces you were passed.
	if (!GetObject(hbmSrc1, sizeof(BITMAP), &bmSrc1)) return FALSE;
	if (!GetObject(hbmSrc2, sizeof(BITMAP), &bmSrc2)) return FALSE;
	if (!GetObject(hbmDst,  sizeof(BITMAP), &bmDst))  return FALSE;

	// Make sure you have data that meets your requirements.
	if (!BitmapsCompatible(&bmSrc1, &bmSrc2)) return FALSE;
	if (!BitmapsCompatible(&bmSrc1, &bmDst))  return FALSE;
	if (bmSrc1.bmBitsPixel != 32) return FALSE;
	if (bmSrc1.bmPlanes != 1) return FALSE;
	if (!bmSrc1.bmBits || !bmSrc2.bmBits || !bmDst.bmBits) return FALSE;

	dwWidthBytes = bmDst.bmWidthBytes;

	// Initialize the surface pointers.
	lprgbSrc1 = (RGBQUAD*)bmSrc1.bmBits;
	lprgbSrc2 = (RGBQUAD*)bmSrc2.bmBits;
	lprgbDst  = (RGBQUAD*)bmDst.bmBits;

	#define F(a)  BYTE(gbSrc1.##a + (255 - gbSrc1.rgbReserved) * gbSrc2.##a / 255)

	for (y = 0; y < bmDst.bmHeight; y++)
	{
		for (x = 0; x < bmDst.bmWidth; x++)
		{
			RGBQUAD& gbSrc1 = bRTL1 ? lprgbSrc1[bmDst.bmWidth - 1 - x] : lprgbSrc1[x];
			RGBQUAD& gbSrc2 = bRTL2 ? lprgbSrc2[bmDst.bmWidth - 1 - x] : lprgbSrc2[x];

			lprgbDst[x].rgbRed   = F(rgbRed);
			lprgbDst[x].rgbGreen = F(rgbGreen);
			lprgbDst[x].rgbBlue  = F(rgbBlue);
		}

		// Move to next scan line.
		lprgbSrc1 = (RGBQUAD *)((LPBYTE)lprgbSrc1 + dwWidthBytes);
		lprgbSrc2 = (RGBQUAD *)((LPBYTE)lprgbSrc2 + dwWidthBytes);
		lprgbDst  = (RGBQUAD *)((LPBYTE)lprgbDst  + dwWidthBytes);
	}

	return TRUE;
}

// Based on http://support.microsoft.com/kb/303916/en-us
HBITMAP AFX_CDECL CUIImageManager::Create32BPPDIBSection(HDC hDC, int iWidth, int iHeight, LPBYTE* lpBits)
{
	BITMAPINFO bmi;
	HBITMAP hbm;
	LPBYTE pBits;

	// Initialize header to 0s.
	ZeroMemory(&bmi, sizeof(bmi));

	// Fill out the fields you care about.
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = iWidth;
	bmi.bmiHeader.biHeight = iHeight;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	// Create the surface.
	hbm = CreateDIBSection(hDC, &bmi, DIB_RGB_COLORS, (LPVOID*)&pBits, NULL, 0);

	if (lpBits)
	{
		*lpBits = pBits;
	}

	return(hbm);
}

// Based on http://support.microsoft.com/kb/303916/en-us
BOOL CUIImageManager::DoAlphaBlend(HDC hdcDest,                 // Handle to destination DC.
	int nXOriginDest,            // X-coord of upper-left corner.
	int nYOriginDest,            // Y-coord of upper-left corner.
	int nWidthDest,              // Destination width.
	int nHeightDest,             // Destination height.
	HDC hdcSrc,                  // Handle to source DC.
	int nXOriginSrc,             // X-coord of upper-left corner.
	int nYOriginSrc,             // Y-coord of upper-left corner.
	int nWidthSrc,               // Source width.
	int nHeightSrc              // Source height.
) const
{
	HDC      hdcSrc1 = NULL;
	HDC      hdcSrc2 = NULL;
	HBITMAP  hbmSrc1 = NULL, hbmOldSrc1 = NULL;
	HBITMAP  hbmSrc2 = NULL, hbmOldSrc2 = NULL;
	HBITMAP  hbmDst  = NULL;
	BOOL     bReturn = FALSE;
	BOOL     bRTLSrc    = UIDrawHelpers()->IsContextRTL(hdcSrc);
	BOOL     bRTLDest   = UIDrawHelpers()->IsContextRTL(hdcDest);

	// Create surfaces for sources and destination images.
	hbmSrc1 = CUIImageManager::Create32BPPDIBSection(hdcDest, nWidthDest,nHeightDest);
	if (!hbmSrc1) goto HANDLEERROR;

	hbmSrc2 = CUIImageManager::Create32BPPDIBSection(hdcDest, nWidthDest,nHeightDest);
	if (!hbmSrc2) goto HANDLEERROR;

	hbmDst  = CUIImageManager::Create32BPPDIBSection(hdcDest, nWidthDest,nHeightDest);
	if (!hbmDst) goto HANDLEERROR;

	// Create HDCs to hold our surfaces.
	hdcSrc1 = CreateCompatibleDC(hdcSrc);
	if (!hdcSrc1) goto HANDLEERROR;

	hdcSrc2 = CreateCompatibleDC(hdcSrc);
	if (!hdcSrc2) goto HANDLEERROR;

	UIDrawHelpers()->SetContextRTL(hdcSrc2, FALSE);
	UIDrawHelpers()->SetContextRTL(hdcSrc1, FALSE);

	// Prepare the surfaces for drawing.
	hbmOldSrc1 = (HBITMAP)SelectObject(hdcSrc1, hbmSrc1);
	hbmOldSrc2 = (HBITMAP)SelectObject(hdcSrc2, hbmSrc2);
	SetStretchBltMode(hdcSrc1, COLORONCOLOR);
	SetStretchBltMode(hdcSrc2, COLORONCOLOR);


	// Capture a copy of the source area.
	if (!StretchBlt(hdcSrc1, 0,0,nWidthDest,nHeightDest,
		hdcSrc,  nXOriginSrc, nYOriginSrc, nWidthSrc, nHeightSrc,
		SRCCOPY))
		goto HANDLEERROR;


	// Capture a copy of the destination area.
	if (!BitBlt(hdcSrc2, 0, 0, nWidthDest, nHeightDest,
		hdcDest, nXOriginDest, nYOriginDest, SRCCOPY))
		goto HANDLEERROR;



	SelectObject(hdcSrc1, hbmOldSrc1); hbmOldSrc1 = NULL;
	SelectObject(hdcSrc2, hbmOldSrc2); hbmOldSrc2 = NULL;

	// Blend the two source areas to create the destination image.
	bReturn = BlendImages(hbmSrc1, bRTLSrc, hbmSrc2, bRTLDest, hbmDst);


	// Display the blended (destination) image to the target HDC.
	if (bReturn)
	{
		hbmOldSrc1 = (HBITMAP)SelectObject(hdcSrc1, hbmDst);

		BitBlt(hdcDest, nXOriginDest, nYOriginDest, nWidthDest, nHeightDest,
			hdcSrc1, 0,0, SRCCOPY | (bRTLDest ? NOMIRRORBITMAP : 0));

		SelectObject(hdcSrc1, hbmOldSrc1);
		hbmOldSrc1 = NULL;
	}

	// Clean up the rest of the objects you created.

HANDLEERROR:

	if (hbmOldSrc1) SelectObject(hdcSrc1, hbmOldSrc1);
	if (hbmOldSrc2) SelectObject(hdcSrc2, hbmOldSrc2);
	if (hdcSrc1) DeleteDC(hdcSrc1);
	if (hdcSrc2) DeleteDC(hdcSrc2);
	if (hbmSrc1) DeleteObject(hbmSrc1);
	if (hbmSrc2) DeleteObject(hbmSrc2);
	if (hbmDst) DeleteObject(hbmDst);

	return bReturn;
}


BOOL CUIImageManager::AlphaBlend(HDC hdcDest, const CRect& rcDest, HDC hdcSrc, const CRect& rcSrc) const
{
	if (UIDrawHelpers()->IsContextRTL(hdcDest) && UISystemVersion()->IsWin9x())
	{
		return DoAlphaBlend(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
			hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height());
	}

	if (UIDrawHelpers()->m_pfnAlphaBlend && (IsWindow2000() || (rcSrc.top == 0 && rcSrc.left == 0 && rcDest.top >= 0)))
	{
		BLENDFUNCTION bf;
		ZeroMemory(&bf, sizeof(BLENDFUNCTION));

		bf.AlphaFormat  = 0x01;
		bf.SourceConstantAlpha = 255;
		bf.BlendOp = AC_SRC_OVER;

		return UIDrawHelpers()->m_pfnAlphaBlend(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
			hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height(), bf);

	}
	return DoAlphaBlend(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
		hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height());
}

BOOL CUIImageManager::AlphaBlend2(HDC hdcDest, const CRect& rcDest, HDC hdcSrc, const CRect& rcSrc, BYTE Transparency) const
{
	if (UIDrawHelpers()->IsContextRTL(hdcDest) && UISystemVersion()->IsWin9x())
	{
		return DoAlphaBlend(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
			hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height());
	}

	if (UIDrawHelpers()->m_pfnAlphaBlend && (IsWindow2000() || (rcSrc.top == 0 && rcSrc.left == 0)))
	{
		BLENDFUNCTION bf;
		ZeroMemory(&bf, sizeof(BLENDFUNCTION));

		bf.AlphaFormat  = 0;
		bf.SourceConstantAlpha = Transparency;
		bf.BlendOp = AC_SRC_OVER;

		return UIDrawHelpers()->m_pfnAlphaBlend(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
			hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height(), bf);

	}
	return DoAlphaBlend(hdcDest, rcDest.left, rcDest.top, rcDest.Width(), rcDest.Height(),
		hdcSrc, rcSrc.left, rcSrc.top, rcSrc.Width(), rcSrc.Height());
}



//////////////////////////////////////////////////////////////////////////
// CUIImageManagerImageList

CUIImageManagerImageList::CUIImageManagerImageList()
{
	m_hImageList = NULL;
	m_bDestroyImageList = FALSE;
	m_nBaseCommand = 0;
	m_pImageManager = NULL;
}

CUIImageManagerImageList::~CUIImageManagerImageList()
{
	if (m_hImageList && m_bDestroyImageList)
	{
		ImageList_Destroy(m_hImageList);
	}

	RemoveAll();
}

void CUIImageManagerImageList::RemoveAll()
{
	UINT nCommand;
	CUIImageManagerIcon* pIcon;

	POSITION pos = m_mapIcons.GetStartPosition();
	while (pos)
	{
		m_mapIcons.GetNextAssoc(pos, nCommand, pIcon);
		pIcon->InternalRelease();
	}

	m_mapIcons.RemoveAll();
}

BOOL CUIImageManagerImageList::Lookup(UINT nCommand)
{
	return ((nCommand >= m_nBaseCommand) &&
		(nCommand < m_nBaseCommand + ImageList_GetImageCount(m_hImageList)));
}

CSize CUIImageManagerImageList::GetIconSize() const
{
	int nWidth = 0, nHeight = 0;
	VERIFY(ImageList_GetIconSize(m_hImageList, &nWidth, &nHeight));

	return CSize(nWidth, nHeight);
}

CUIImageManagerIcon* CUIImageManagerImageList::GetIcon(UINT nCommand)
{
	CUIImageManagerIcon* pImage = NULL;

	if (m_mapIcons.Lookup(nCommand, pImage))
	{
		return pImage;
	}

	CSize sz = GetIconSize();

	pImage = new CUIImageManagerIcon(nCommand, sz.cx, sz.cy, this);
	m_mapIcons.SetAt(nCommand, pImage);

	return pImage;
}

void CUIImageManagerImageList::Draw(CDC* pDC, CPoint pt, CUIImageManagerIcon* pIcon, CSize szIcon, COLORREF clrBK, COLORREF clrFG, UINT uiFlags)
{
	ImageList_DrawEx(m_hImageList, pIcon->GetID() - m_nBaseCommand, pDC->GetSafeHdc(),
		pt.x, pt.y, szIcon.cx, szIcon.cy, clrBK, clrFG, ILD_TRANSPARENT | ILD_NORMAL | uiFlags);
}

//////////////////////////////////////////////////////////////////////////
// CUIImageManagerIcon


struct CUIImageManagerIcon::ICONIMAGE
{
	BITMAPINFOHEADER    icHeader;   // DIB header
	RGBQUAD             icColors[1];// Color table
	BYTE                icXOR[1];   // DIB bits for XOR mask
	BYTE                icAND[1];   // DIB bits for AND mask
};

#pragma pack(push, 2)
// resources are WORD-aligned

struct CUIImageManagerIcon::ICONDIRENTRY
{
	BYTE        bWidth;             // Width, in pixels, of the image
	BYTE        bHeight;            // Height, in pixels, of the image
	BYTE        bColorCount;        // Number of colors in image (0 if >= 8bpp)
	BYTE        bReserved;          // Reserved (must be 0)
	WORD        wPlanes;            // Color Planes
	WORD        wBitCount;          // Bits per pixel
	DWORD       dwBytesInRes;       // How many bytes in this resource ?
	DWORD       dwImageOffset;      // Where in the file is this image ?
};

struct CUIImageManagerIcon::ICONDIRHEADER
{
	WORD            idReserved;     // Reserved (must be 0)
	WORD            idType;         // Resource Type (1 for icons)
	WORD            idCount;        // How many images ?
};

struct CUIImageManagerIcon::GRPICONDIRENTRY
{
	BYTE   bWidth;                  // Width, in pixels, of the image
	BYTE   bHeight;                 // Height, in pixels, of the image
	BYTE   bColorCount;             // Number of colors in image (0 if >= 8bpp)
	BYTE   bReserved;               // Reserved
	WORD   wPlanes;                 // Color Planes
	WORD   wBitCount;               // Bits per pixel
	DWORD  dwBytesInRes;            // how many bytes in this resource ?
	WORD   nID;                     // the ID
};

struct CUIImageManagerIcon::GRPICONDIR
{
	WORD              idReserved;   // Reserved (must be 0)
	WORD              idType;       // Resource type (1 for icons)
	WORD              idCount;      // How many images ?
	GRPICONDIRENTRY   idEntries[1]; // The entries for each image
};

#pragma pack(pop)

BOOL CUIImageManagerIcon::IsAlphaBitmapFile(LPCTSTR pszFileName)
{
	HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	DWORD dwBytesRead;

	BITMAPFILEHEADER fileHeader;

	if (!ReadFile(hFile, &fileHeader, sizeof(BITMAPFILEHEADER), &dwBytesRead, 0) || dwBytesRead != sizeof(BITMAPFILEHEADER))
	{
		CloseHandle(hFile);
		return FALSE;
	}

	BITMAPINFOHEADER infoHeader;

	if (!ReadFile(hFile, &infoHeader, sizeof(BITMAPINFOHEADER), &dwBytesRead, 0) || dwBytesRead != sizeof(BITMAPINFOHEADER))
	{
		CloseHandle(hFile);
		return FALSE;
	}

	BOOL bResult = infoHeader.biBitCount == 32;

	CloseHandle(hFile);

	return bResult;
}

BOOL CUIImageManagerIcon::IsPngBitmapFile(LPCTSTR pszFileName)
{
	HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	DWORD dwBytesRead;

	BYTE fileHeader[4];
	BYTE png_signature[4] = {137, 80, 78, 71};

	if (!ReadFile(hFile, &fileHeader, sizeof(fileHeader), &dwBytesRead, 0) || dwBytesRead != sizeof(fileHeader))
	{
		CloseHandle(hFile);
		return FALSE;
	}

	BOOL bResult = memcmp(&fileHeader, &png_signature, 4) == 0;

	CloseHandle(hFile);

	return bResult;
}


HBITMAP AFX_CDECL CUIImageManagerIcon::LoadBitmapFromFile(LPCTSTR lpszFileName, BOOL* lbAlphaBitmap)
{
#ifdef _UI_INCLUDE_GRAPHICLIBRARY
	BOOL bPngBitmap = CUIImageManagerIcon::IsPngBitmapFile(lpszFileName);

	if (bPngBitmap)
	{
		CUIGraphicBitmapPng bmp;
		if (!bmp.LoadFromFile(lpszFileName))
			return NULL;

		if (lbAlphaBitmap)
		{
			*lbAlphaBitmap = bmp.IsAlpha();
		}
		return (HBITMAP)bmp.Detach();
	}
#endif
	BOOL bAlphaBitmap = CUIImageManagerIcon::IsAlphaBitmapFile(lpszFileName);

	HBITMAP hBmp = (HBITMAP)LoadImage(0, lpszFileName,
		IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE | (bAlphaBitmap ? LR_CREATEDIBSECTION : 0) | LR_LOADFROMFILE);

	if (hBmp)
	{
		if (lbAlphaBitmap)
		{
			*lbAlphaBitmap = bAlphaBitmap;
		}
		return hBmp;
	}

	LPPICTURE pPict = NULL;

	if (OleLoadPicturePath((LPOLESTR)UI_CT2CW(lpszFileName), NULL, 0, 0, IID_IPicture, (LPVOID*)&pPict) == S_OK)
	{
		CDC dc;
		dc.CreateCompatibleDC(NULL);

		long hmWidth;
		long hmHeight;

		pPict->get_Width(&hmWidth);
		pPict->get_Height(&hmHeight);

		int cx = MulDiv(dc.GetDeviceCaps(LOGPIXELSX), (int)hmWidth, 2540);
		int cy = MulDiv(dc.GetDeviceCaps(LOGPIXELSY), (int)hmHeight, 2540);

		HBITMAP hbmpImage = CUIImageManager::Create32BPPDIBSection(NULL, cx, cy);

		HGDIOBJ hOldBitmap = ::SelectObject(dc, hbmpImage);
		dc.FillSolidRect(0, 0, cx, cy, GetSysColor(COLOR_3DFACE));

		pPict->Render(dc, 0, 0, cx, cy, 0, hmHeight-1, hmWidth, -hmHeight, 0);

		::SelectObject(dc, hOldBitmap);

		pPict->Release();

		return hbmpImage;
	}

	return NULL;
}


BOOL AFX_CDECL CUIImageManagerIcon::IsPngBitmapResource(HMODULE hModule, LPCTSTR lpBitmapName)
{
	BYTE png_signature[4] = {137, 80, 78, 71};

	HRSRC hResource = ::FindResource(hModule, lpBitmapName, _T("PNG"));

	if (hResource == NULL)
		return FALSE;

	HGLOBAL hGlobal = LoadResource(hModule, hResource);
	if (hGlobal == NULL)
		return FALSE;

	LPBYTE pBitmapInfoHeader = (LPBYTE)::LockResource(hGlobal);
	BOOL bPngBitmap = memcmp(pBitmapInfoHeader, &png_signature, 4) == 0;

	UnlockResource(hGlobal);
	FreeResource(hGlobal);

	return (bPngBitmap);
}

BOOL AFX_CDECL CUIImageManagerIcon::IsAlphaBitmapResource(HMODULE hModule, LPCTSTR lpBitmapName)
{
	HRSRC hResource = ::FindResource(hModule, lpBitmapName, RT_BITMAP);

	if (hResource == NULL)
		return FALSE;

	HGLOBAL hGlobal = LoadResource(hModule, hResource);
	if (hGlobal == NULL)
		return FALSE;

	LPBITMAPINFOHEADER pBitmapInfoHeader = (LPBITMAPINFOHEADER)::LockResource(hGlobal);
	ASSERT(pBitmapInfoHeader != NULL);
	if (!pBitmapInfoHeader)
		return FALSE;

	BOOL bAlpahBitmap = pBitmapInfoHeader->biBitCount == 32;
	UnlockResource(hGlobal);
	FreeResource(hGlobal);

	return (bAlpahBitmap);
}

HBITMAP AFX_CDECL CUIImageManagerIcon::LoadBitmapFromResource(LPCTSTR lpszResource, BOOL* lbAlphaBitmap)
{
	HMODULE hModule;

#ifdef _UI_INCLUDE_GRAPHICLIBRARY
	hModule = AfxFindResourceHandle(lpszResource, _T("PNG"));

	BOOL bPngBitmap = IsPngBitmapResource(hModule, lpszResource);
	if (bPngBitmap)
	{
		CUIGraphicBitmapPng bmpIcons;

		HRSRC hResource = ::FindResource(hModule, lpszResource, _T("PNG"));

		if (!bmpIcons.LoadFromResource(hModule, hResource))
			return NULL;

		if (lbAlphaBitmap)
		{
			*lbAlphaBitmap = bmpIcons.IsAlpha();
		}

		return (HBITMAP)bmpIcons.Detach();
	}
#endif

	hModule = AfxFindResourceHandle(lpszResource, RT_BITMAP);

	CBitmap bmpIcons;
	BOOL bAlphaBitmap = IsAlphaBitmapResource(hModule, lpszResource);

	if (lbAlphaBitmap)
	{
		*lbAlphaBitmap = bAlphaBitmap;
	}

	if (bAlphaBitmap)
	{
		return LoadAlphaBitmap(hModule, lpszResource);
	}

	return (HBITMAP)LoadImage(hModule, lpszResource, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
}

HBITMAP AFX_CDECL CUIImageManagerIcon::LoadBitmapFromResource(HMODULE hModule, LPCTSTR lpszResource, BOOL* lbAlphaBitmap)
{
#ifdef _UI_INCLUDE_GRAPHICLIBRARY

	BOOL bPngBitmap = IsPngBitmapResource(hModule, lpszResource);
	if (bPngBitmap)
	{
		CUIGraphicBitmapPng bmpIcons;

		HRSRC hResource = ::FindResource(hModule, lpszResource, _T("PNG"));

		if (!bmpIcons.LoadFromResource(hModule, hResource))
			return NULL;

		if (lbAlphaBitmap)
		{
			*lbAlphaBitmap = bmpIcons.IsAlpha();
		}

		return (HBITMAP)bmpIcons.Detach();
	}
#endif

	CBitmap bmpIcons;
	BOOL bAlphaBitmap = IsAlphaBitmapResource(hModule, lpszResource);

	if (lbAlphaBitmap)
	{
		*lbAlphaBitmap = bAlphaBitmap;
	}

	if (bAlphaBitmap)
	{
		return LoadAlphaBitmap(hModule, lpszResource);
	}

	return (HBITMAP)LoadImage(hModule, lpszResource, IMAGE_BITMAP, 0, 0, LR_DEFAULTSIZE);
}

HBITMAP CUIImageManagerIcon::LoadAlphaBitmap(UINT nIDResource)
{
	return LoadAlphaBitmap(AfxFindResourceHandle(MAKEINTRESOURCE(nIDResource), RT_BITMAP),
		MAKEINTRESOURCE(nIDResource));
}

HBITMAP CUIImageManagerIcon::LoadAlphaBitmap(HMODULE hModule, LPCTSTR lpszResource)
{
	HRSRC hResource = ::FindResource(hModule, lpszResource, RT_BITMAP);
	if (hResource == NULL)
		return NULL;

	HGLOBAL hGlobal = LoadResource(hModule, hResource);
	if (hGlobal == NULL)
		return NULL;

	LPBITMAPINFO pResourceInfo = (LPBITMAPINFO)::LockResource(hGlobal);
	ASSERT(pResourceInfo != NULL);
	if (!pResourceInfo)
		return NULL;

	int biSizeImage = pResourceInfo->bmiHeader.biHeight * pResourceInfo->bmiHeader.biWidth * 4;

	HBITMAP hbmResult = NULL;

	if (pResourceInfo->bmiHeader.biBitCount == 32 &&
		SizeofResource(hModule, hResource) >= int(biSizeImage + sizeof(BITMAPINFOHEADER)))
	{
		CDC dcSrc;
		dcSrc.CreateCompatibleDC(NULL);

		PBITMAPINFO pBitmapInfo = (PBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + sizeof(COLORREF) * 3);
		ASSERT(pBitmapInfo != NULL);
		if (!pBitmapInfo)
			return NULL;

		MEMCPY_S(pBitmapInfo, &pResourceInfo->bmiHeader, sizeof(BITMAPINFOHEADER));
		pBitmapInfo->bmiHeader.biSizeImage = biSizeImage;

		BYTE* pDestBits = NULL;
		HBITMAP hBmp = CreateDIBSection(dcSrc, pBitmapInfo, DIB_RGB_COLORS, (void**)&pDestBits, NULL, 0);

		if (hBmp && pDestBits)
		{
			MEMCPY_S(pDestBits, &pResourceInfo->bmiColors, biSizeImage);
			hbmResult = hBmp;
		}
		FREE(pBitmapInfo);
	}

	UnlockResource(hGlobal);
	FreeResource(hGlobal);

	return hbmResult;

}



HBITMAP CUIImageManagerIcon::LoadAlphaIcon(LPCTSTR pszFileName, int nWidth)
{

	HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return NULL;
	}

	ICONDIRHEADER idh;

	DWORD dwBytesRead = 0;
	if (!ReadFile(hFile, &idh, sizeof(ICONDIRHEADER) , &dwBytesRead, NULL) || dwBytesRead != sizeof(ICONDIRHEADER))
	{
		CloseHandle(hFile);
		return FALSE;
	}

	if (idh.idType != 1 || idh.idCount == 0)
	{
		CloseHandle(hFile);
		return FALSE;
	}


	ICONDIRENTRY* pEntries = (ICONDIRENTRY*)malloc(sizeof(ICONDIRENTRY) * idh.idCount);

	if (pEntries == NULL)
	{
		CloseHandle(hFile);
		return FALSE;
	}

	if (!ReadFile(hFile, pEntries, idh.idCount * sizeof(ICONDIRENTRY), &dwBytesRead, NULL)
		|| dwBytesRead != idh.idCount * sizeof(ICONDIRENTRY))
	{
		free(pEntries);
		CloseHandle(hFile);
		return FALSE;
	}

	HBITMAP hBitmap = 0;

	for (int i = 0; i < idh.idCount; i++)
	{
		if (pEntries[i].wBitCount == 32 && (((int)pEntries[i].bWidth == nWidth) || (nWidth == 0)))
		{
			if (!SetFilePointer(hFile, pEntries[i].dwImageOffset,
				NULL, FILE_BEGIN))
				continue;

			ICONIMAGE* pIconImage = (ICONIMAGE*)malloc(pEntries[i].dwBytesInRes);
			if (pIconImage == NULL)
				continue;

			dwBytesRead = 0;
			BOOL bResult = ReadFile(hFile, pIconImage, pEntries[i].dwBytesInRes,
				&dwBytesRead, NULL);

			if (!bResult || (dwBytesRead != pEntries[i].dwBytesInRes) || (pIconImage->icHeader.biBitCount != 32))
			{
				free(pIconImage);
				continue;
			}

			CDC dcSrc;
			dcSrc.CreateCompatibleDC(NULL);

			pIconImage->icHeader.biHeight /= 2;
			pIconImage->icHeader.biSizeImage = pIconImage->icHeader.biHeight * pIconImage->icHeader.biWidth * 4;

			BYTE* pDest = NULL;
			HBITMAP hBmp = CreateDIBSection(dcSrc, (BITMAPINFO*)&pIconImage->icHeader, DIB_RGB_COLORS, (void**)&pDest, NULL, 0);

			if (pDest != NULL && hBmp != NULL)
			{
				MEMCPY_S(pDest, &pIconImage->icColors, pIconImage->icHeader.biSizeImage);
				hBitmap = hBmp;

				free(pIconImage);
				break;
			}

			free(pIconImage);
		}
	}
	free(pEntries);
	CloseHandle(hFile);

	return hBitmap;

}

BOOL CUIImageManager::SetIconFromIcoFile(LPCTSTR pszFileName, UINT nIDCommand, CSize szIcon, UIImageState imageState)
{
	HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	CUIImageManagerIcon::ICONDIRHEADER idh;

	DWORD dwBytesRead = 0;
	if (!ReadFile(hFile, &idh, sizeof(CUIImageManagerIcon::ICONDIRHEADER) , &dwBytesRead, NULL) || dwBytesRead != sizeof(CUIImageManagerIcon::ICONDIRHEADER))
	{
		CloseHandle(hFile);
		return FALSE;
	}

	if (idh.idType != 1 || idh.idCount == 0)
	{
		CloseHandle(hFile);
		return FALSE;
	}


	CUIImageManagerIcon::ICONDIRENTRY* pEntries = (CUIImageManagerIcon::ICONDIRENTRY*)malloc(sizeof(CUIImageManagerIcon::ICONDIRENTRY) * idh.idCount);

	if (pEntries == NULL)
	{
		CloseHandle(hFile);
		return FALSE;
	}

	if (!ReadFile(hFile, pEntries, idh.idCount * sizeof(CUIImageManagerIcon::ICONDIRENTRY), &dwBytesRead, NULL)
		|| dwBytesRead != idh.idCount * sizeof(CUIImageManagerIcon::ICONDIRENTRY))
	{
		free(pEntries);
		CloseHandle(hFile);
		return FALSE;
	}

	BOOL bResult = FALSE;

	for (int i = 0; i < idh.idCount; i++)
	{
		CSize szResourceIcon((int)pEntries[i].bWidth, (int)pEntries[i].bHeight);
		CUIImageManagerIconHandle hIcon;

		if ((szIcon == szResourceIcon) || (szIcon == CSize(0, 0)))
		{
			if (!SetFilePointer(hFile, pEntries[i].dwImageOffset,
				NULL, FILE_BEGIN))
				continue;

			CUIImageManagerIcon::ICONIMAGE* pIconImage = (CUIImageManagerIcon::ICONIMAGE*)malloc(pEntries[i].dwBytesInRes);
			if (pIconImage == NULL)
				continue;

			dwBytesRead = 0;
			BOOL bReadFile = ReadFile(hFile, pIconImage, pEntries[i].dwBytesInRes,
				&dwBytesRead, NULL);

			if (!bReadFile || (dwBytesRead != pEntries[i].dwBytesInRes))
			{
				free(pIconImage);
				continue;
			}

			if (pIconImage->icHeader.biBitCount == 32)
			{

				CDC dcSrc;
				dcSrc.CreateCompatibleDC(NULL);

				pIconImage->icHeader.biHeight /= 2;
				pIconImage->icHeader.biSizeImage = pIconImage->icHeader.biHeight * pIconImage->icHeader.biWidth * 4;

				BYTE* pDest = NULL;
				HBITMAP hBitmap = CreateDIBSection(dcSrc, (BITMAPINFO*)&pIconImage->icHeader, DIB_RGB_COLORS, (void**)&pDest, NULL, 0);

				if (pDest != NULL && hBitmap != NULL)
				{
					MEMCPY_S(pDest, &pIconImage->icColors, pIconImage->icHeader.biSizeImage);

					hIcon.Attach(hBitmap, FALSE);
				}
			}
			else
			{
				hIcon.Attach(CreateIconFromResourceEx((PBYTE)pIconImage, pEntries[i].dwBytesInRes, TRUE,
					0x00030000, szResourceIcon.cx, szResourceIcon.cy, LR_DEFAULTCOLOR), FALSE);
			}

			if (!hIcon.IsEmpty())
			{
				SetIcon(hIcon, nIDCommand, szResourceIcon, imageState);
				bResult = TRUE;
			}

			free(pIconImage);
		}
	}
	free(pEntries);
	CloseHandle(hFile);

	return bResult;
}

CUIImageManagerResource* CUIImageManager::GetResource(CSize szIcon)
{
	DWORD dwSize = MAKELONG(szIcon.cx, szIcon.cy);

	CUIImageManagerResource* pResource = NULL;
	if (m_mapResources.Lookup(dwSize, pResource))
		return pResource;

	pResource = new CUIImageManagerResource(this, szIcon);
	m_mapResources.SetAt(dwSize, pResource);

	return pResource;
}

BOOL AFX_CDECL CUIImageManagerIcon::IsAlphaIconHandle(HICON hIcon)
{
	if (!hIcon)
		return FALSE;

	ICONINFO iconInfo;
	if ((CUIImageManager::IsAlphaIconsImageListSupported() || UISystemVersion()->IsWinXPOrGreater())
		&& GetIconInfo(hIcon, &iconInfo))
	{
		LPBYTE pBitsI = NULL;
		BOOL bAlpha = -1;
		HBITMAP hBitmap = CUIImageManagerIcon::PreMultiplyAlphaBitmap(iconInfo.hbmColor, &bAlpha, &pBitsI);

		DeleteObject(iconInfo.hbmColor);
		DeleteObject(iconInfo.hbmMask);

		if (bAlpha == TRUE && pBitsI)
		{
			DeleteObject(hBitmap);
			return TRUE;
		}

		if (hBitmap) DeleteObject(hBitmap);
	}

	return FALSE;
}

BOOL CUIImageManager::IsDrawReverted(CDC* pDC) const
{
	if (m_bDrawReverted == 1)
		return TRUE;
	if (m_bDrawReverted == 2)
		return CUIDrawHelpers::IsContextRTL(pDC);
	return FALSE;
}


//////////////////////////////////////////////////////////////////////////
// CUIImageManagerIconHandle

void CUIImageManagerIconHandle::Init()
{
	m_hIcon = NULL;
	m_hBitmap = NULL;
	m_pBits = NULL;
	m_bClearHandles = FALSE;
	m_pImageManager = NULL;
	m_pRInfo = NULL;
	m_szIcon = CSize(0, 0);
	m_bUseResources = TRUE;
}

CUIImageManagerIconHandle::CUIImageManagerIconHandle()
{
	Init();
}

CUIImageManagerIconHandle::CUIImageManagerIconHandle(HICON hIcon, BOOL bUseResources, BOOL bSharedHandle)
{
	Init();

	m_hIcon = hIcon;
	m_bClearHandles = !bSharedHandle;
	m_szIcon = _GetExtent();
	m_bUseResources = bUseResources;
}

CUIImageManagerIconHandle::CUIImageManagerIconHandle(HBITMAP hBitmap, BOOL bSharedHandle)
{
	Init();

	m_hBitmap = hBitmap;
	m_bClearHandles = !bSharedHandle;
	m_szIcon = _GetExtent();
}

CUIImageManagerIconHandle::CUIImageManagerIconHandle(const CUIImageManagerIconHandle& hHandle)
{
	Init();

	m_hIcon = hHandle.m_hIcon;
	m_hBitmap = hHandle.m_hBitmap;
	m_pBits = hHandle.m_pBits;
	m_pRInfo = hHandle.m_pRInfo;
	m_pImageManager = hHandle.m_pImageManager;
	m_bUseResources = hHandle.m_bUseResources;

	m_bClearHandles = FALSE;
	m_szIcon = hHandle.GetExtent();
}


void CUIImageManagerIconHandle::CopyHandle(HBITMAP hBitmap)
{
	Clear();
	m_hBitmap = CUIImageManagerIcon::CopyAlphaBitmap(hBitmap);

	m_szIcon = _GetExtent();
	m_bClearHandles = TRUE;
}

void CUIImageManagerIconHandle::CopyHandle(const CUIImageManagerIconHandle& hHandle)
{
	Clear();

	if (m_pImageManager && !hHandle.IsEmpty() && m_pImageManager->m_bUseResources && hHandle.m_bUseResources)
	{
		CUIImageManagerResource* pResource = m_pImageManager->GetResource(hHandle.GetExtent());

		if (hHandle.m_pRInfo == NULL)
		{
			m_pRInfo = pResource->Add(hHandle);
		}
		else
		{
			CUIImageManagerIconHandle iconClear;
			iconClear.CopyHandle(hHandle);
			m_pRInfo = pResource->Add(iconClear);
		}
	}
	else
	{
		if (hHandle.m_hIcon) m_hIcon = CopyIcon(hHandle.m_hIcon);
		if (hHandle.m_hBitmap) m_hBitmap = CUIImageManagerIcon::CopyAlphaBitmap(hHandle.m_hBitmap, hHandle.m_pBits ? &m_pBits : NULL);
		if (hHandle.m_pRInfo)
		{
			hHandle.m_pRInfo->pResource->CreateIcon(*this, hHandle.m_pRInfo);
		}
	}
	m_bUseResources = hHandle.m_bUseResources;

	m_bClearHandles = TRUE;
	m_szIcon = hHandle.GetExtent();
}

void CUIImageManagerIconHandle::Attach(HICON hIcon, BOOL bSharedHandle)
{
	Clear();
	m_hIcon = hIcon;
	m_bClearHandles = !bSharedHandle;
	m_szIcon = _GetExtent();
}

void CUIImageManagerIconHandle::Attach(HBITMAP hBitmap, BOOL bSharedHandle)
{
	Clear();

	m_hBitmap = hBitmap;
	m_bClearHandles = !bSharedHandle;
	m_szIcon = _GetExtent();
}

const CUIImageManagerIconHandle& CUIImageManagerIconHandle::operator=(const HICON hIcon)
{
	Clear();
	m_hIcon = hIcon;
	m_bClearHandles = TRUE;
	m_szIcon = _GetExtent();

	return *this;
}
const CUIImageManagerIconHandle& CUIImageManagerIconHandle::operator=(const HBITMAP hBitmap)
{
	Clear();

	m_hBitmap = hBitmap;
	m_bClearHandles = TRUE;
	m_szIcon = _GetExtent();

	return *this;
}

CUIImageManagerIconHandle::~CUIImageManagerIconHandle()
{
	Clear();
}

BOOL CUIImageManagerIconHandle::IsEmpty() const
{
	return (m_hIcon == 0 && m_hBitmap == 0 && m_pRInfo == NULL) || (m_szIcon == CSize(0, 0));
}

BOOL CUIImageManagerIconHandle::IsAlpha() const
{
	return m_hBitmap != 0 || (m_pRInfo && m_pRInfo->bAlpha);
}

HBITMAP CUIImageManagerIconHandle::GetBitmap() const
{
	ASSERT(IsAlpha());

	if (m_hBitmap)
		return m_hBitmap;

	if (m_pRInfo && m_pRInfo->pResource && m_pRInfo->pResource->m_hbmpImage)
		return m_pRInfo->pResource->m_hbmpImage;

	ASSERT(FALSE);
	return NULL;
}

HICON CUIImageManagerIconHandle::GetIcon() const
{
	return m_hIcon;
}

void CUIImageManagerIconHandle::Detach()
{
	m_bClearHandles = FALSE;
	Clear();
}

void CUIImageManagerIconHandle::Clear()
{
	if (m_bClearHandles)
	{
		if (m_hIcon)
		{
			DestroyIcon(m_hIcon);
		}

		if (m_hBitmap)
		{
			DeleteObject(m_hBitmap);
		}

		if (m_pRInfo)
		{
			m_pRInfo->pResource->Clear(m_pRInfo);
			m_pRInfo = NULL;
		}
	}

	m_hIcon = 0;
	m_hBitmap = 0;
	m_pBits = 0;
	m_pRInfo = NULL;
	m_bClearHandles = FALSE;
}

CSize CUIImageManagerIconHandle::GetExtent() const
{
	return m_szIcon;
}

CSize CUIImageManagerIconHandle::_GetExtent() const
{
	if (m_pRInfo != NULL)
		return m_pRInfo->pResource->GetExtent();

	if (m_hIcon)
		return CUIImageManagerIcon::GetExtent(m_hIcon);

	if (m_hBitmap)
	{
		BITMAP bmpinfo;
		if (::GetObject(m_hBitmap, sizeof(bmpinfo), &bmpinfo))
		{
			return CSize((int)bmpinfo.bmWidth, (int)bmpinfo.bmHeight);
		}
	}
	return 0;
}



void CUIImageManagerIconHandle::Draw(CDC* pDC, CPoint pt, CSize szIcon, BOOL bDrawComposited)
{
	if (IsEmpty())
		return;

	if (m_pRInfo)
	{
		m_pRInfo->pResource->Draw(pDC, m_pRInfo, pt, szIcon, bDrawComposited);
	}
	else if (!IsAlpha())
	{
		CUIImageManager* pImageManager = m_pImageManager ? m_pImageManager : UIImageManager();

		if (pImageManager->IsDrawReverted(pDC))
		{
			pt.x += szIcon.cx;
			szIcon.cx = -szIcon.cx;
		}

		if (!bDrawComposited)
		{
			DrawIconEx(pDC->GetSafeHdc(), pt.x, pt.y, GetIcon(), szIcon.cx, szIcon.cy, 0, 0, DI_NORMAL);
		}
		else
		{
			CUIImageManagerIcon::DrawIconComposited(pDC, pt, szIcon, GetIcon());
		}
	}
	else if (PreMultiply())
	{
		if (pt.y < 0 && (szIcon.cy == (int)m_szIcon.cy))
		{
			if (pt.y + m_szIcon.cy > 0)
			{
				CUIImageManagerIcon::DrawAlphaBitmap(pDC, m_hBitmap, CPoint(pt.x, 0),
					CSize(szIcon.cx, m_szIcon.cy + pt.y), CPoint(0, -pt.y), CSize(m_szIcon.cx, m_szIcon.cy + pt.y));
			}
		}
		else
		{
			CUIImageManagerIcon::DrawAlphaBitmap(pDC, m_hBitmap, pt, szIcon, CPoint(0, 0), m_szIcon);
		}
	}
}

BOOL CUIImageManagerIconHandle::CreateIconFromResource(LPCTSTR lpszResourceName, CSize szIcon, BOOL bGroupResource)
{
	if (bGroupResource)
	{
		HINSTANCE hInst = AfxFindResourceHandle(lpszResourceName, RT_GROUP_ICON);
		if (hInst)
			return CreateIconFromResource(hInst, lpszResourceName, szIcon, TRUE);
	}

	HINSTANCE hInst = AfxFindResourceHandle(lpszResourceName, RT_ICON);
	if (hInst)
		return CreateIconFromResource(hInst, lpszResourceName, szIcon, FALSE);

	return FALSE;

}

BOOL CUIImageManagerIconHandle::CreateIconFromResource(HINSTANCE hInst, LPCTSTR lpszResourceName, CSize szIcon, BOOL bGroupResource)
{
	HRSRC hRsrc = ::FindResource(hInst, lpszResourceName, RT_GROUP_ICON);

	if (hRsrc != NULL && bGroupResource)
	{
		// Load and Lock to get a pointer to a GRPICONDIR
		HGLOBAL hGlobal = LoadResource(hInst, hRsrc);
		if (!hGlobal)
			return FALSE;

		CUIImageManagerIcon::GRPICONDIR* lpGrpIconDir = (CUIImageManagerIcon::GRPICONDIR*)LockResource(hGlobal);
		if (!lpGrpIconDir)
			return FALSE;

		for (int i = (int)lpGrpIconDir->idCount - 1; i >= 0; i--)
		{
			if (lpGrpIconDir->idEntries[i].bWidth == szIcon.cx && lpGrpIconDir->idEntries[i].bHeight == szIcon.cy)
			{
				if (CreateIconFromResource(hInst, MAKEINTRESOURCE(lpGrpIconDir->idEntries[i].nID), szIcon, FALSE))
					return TRUE;
			}
		}

		return FALSE;
	}

	hRsrc = ::FindResource(hInst, lpszResourceName, RT_ICON);

	if (hRsrc == NULL)
		return FALSE;

	HGLOBAL hGlobal = LoadResource(hInst, hRsrc);
	if (!hGlobal)
		return FALSE;
	CUIImageManagerIcon::ICONIMAGE* lpIconImage = (CUIImageManagerIcon::ICONIMAGE*)LockResource(hGlobal);

	if (lpIconImage == NULL)
		return FALSE;

	DWORD dwResourceSize = SizeofResource(hInst, hRsrc);

	if (lpIconImage->icHeader.biBitCount == 32)
	{
		CDC dcSrc;
		dcSrc.CreateCompatibleDC(0);

		BITMAPINFOHEADER* pBitmapInfo = (BITMAPINFOHEADER*)malloc(sizeof(BITMAPINFOHEADER) + sizeof(COLORREF) * 3);
		if (!pBitmapInfo)
			return FALSE;

		*pBitmapInfo = lpIconImage->icHeader;

		pBitmapInfo->biHeight /= 2;
		pBitmapInfo->biSizeImage = pBitmapInfo->biHeight * pBitmapInfo->biWidth * 4;

		BYTE* pDest = NULL;
		HBITMAP hBitmap = CreateDIBSection(dcSrc, (BITMAPINFO*)pBitmapInfo, DIB_RGB_COLORS, (void**)&pDest, NULL, 0);

		if (pDest == NULL || hBitmap == NULL)
		{
			free(pBitmapInfo);
			return FALSE;
		}

		ASSERT(pBitmapInfo->biSizeImage <= dwResourceSize - sizeof(BITMAPINFOHEADER));

		MEMCPY_S(pDest, &lpIconImage->icColors, pBitmapInfo->biSizeImage);

		Attach(hBitmap, FALSE);
		free(pBitmapInfo);
	}
	else
	{
		Attach(CreateIconFromResourceEx((PBYTE)lpIconImage, dwResourceSize, TRUE,
			0x00030000, szIcon.cx, szIcon.cy, LR_DEFAULTCOLOR), FALSE);
	}

	return !IsEmpty();
}


LPBYTE CUIImageManagerIconHandle::PreMultiply()
{
	if (m_pBits != NULL)
		return m_pBits;

	if (m_hBitmap == 0)
		return NULL;

	LPBYTE pBits = NULL;

	HBITMAP hBitmapAlpha =  CUIImageManagerIcon::PreMultiplyAlphaBitmap(m_hBitmap, NULL, &pBits);
	if (!hBitmapAlpha || (pBits == NULL))
		return NULL;

	Clear();

	m_hBitmap = hBitmapAlpha;
	m_pBits = pBits;
	m_bClearHandles = TRUE;
	m_szIcon = _GetExtent();

	return m_pBits;
}

BOOL CUIImageManagerIcon::IsAlpha() const
{
	return m_hIcon.IsAlpha();
}

HBITMAP CUIImageManagerIcon::InvertAlphaBitmap(HBITMAP hBitmap)
{

	CDC dcSrc;
	dcSrc.CreateCompatibleDC(NULL);

	DWORD* pBits = 0;
	PBITMAPINFO pBitmapInfo = 0;

	HBITMAP hBitmapAlpha = 0;

	TRY
	{
		UINT nSize;
		if (!GetBitmapBits(dcSrc, hBitmap, pBitmapInfo, (LPVOID&)pBits, nSize))
			return 0;

		DWORD* pDest = NULL;

		hBitmapAlpha = CreateDIBSection(dcSrc, pBitmapInfo, DIB_RGB_COLORS, (void**)&pDest, NULL, 0);
		if (pDest == NULL || hBitmapAlpha == NULL)
			AfxThrowMemoryException();

		int cy = pBitmapInfo->bmiHeader.biHeight;
		int cx = pBitmapInfo->bmiHeader.biWidth;

		for (int y = 0; y < cy; y++)
		{
			DWORD* pBitsRow = &pBits[y * cx + cx - 1];

			for (int x = 0; x < cx; x++)
			{
				*pDest++ = *pBitsRow--;
			}
		}
	}
	CATCH (CMemoryException, e)
	{
		TRACE(_T("Failed -- Memory exception thrown."));
	}
	END_CATCH

	FREE(pBits);
	FREE(pBitmapInfo);

	return hBitmapAlpha;
}

HBITMAP CUIImageManagerIcon::PreMultiplyAlphaBitmap(HBITMAP hBitmap, BOOL* pbAlpha, LPBYTE* lpBits)
{
	if (lpBits) *lpBits = NULL;

	CDC dcSrc;
	dcSrc.CreateCompatibleDC(NULL);

	PBYTE pBitsSrc = 0;
	PBITMAPINFO pBitmapInfo = 0;

	HBITMAP hBitmapAlpha = 0;

	TRY
	{
		UINT nSize;
		if (!GetBitmapBits(dcSrc, hBitmap, pBitmapInfo, (LPVOID&)pBitsSrc, nSize))
			return 0;

		BYTE* pBitsDest = NULL;

		hBitmapAlpha = CreateDIBSection(dcSrc, pBitmapInfo, DIB_RGB_COLORS, (void**)&pBitsDest, NULL, 0);
		if (pBitsDest == NULL || hBitmapAlpha == NULL)
			AfxThrowMemoryException();

		MEMCPY_S(pBitsDest, pBitsSrc, nSize);

		BOOL bAlpha0Found = FALSE;

		BYTE* pBitsI = pBitsDest;
		UINT nCount = nSize / 4;

		for (UINT i = 0; i < nCount; i++)
		{
			int nAlpha = pBitsI[3];
			pBitsI[0] = BYTE(pBitsI[0] * nAlpha / 255);
			pBitsI[1] = BYTE(pBitsI[1] * nAlpha / 255);
			pBitsI[2] = BYTE(pBitsI[2] * nAlpha / 255);

			if (pbAlpha)
			{
				if (nAlpha == 0)
					bAlpha0Found = TRUE;

				if ((nAlpha != 255 && nAlpha != 0) || (nAlpha == 255 && bAlpha0Found) || (nAlpha == 255 && *pbAlpha == -1))
					*pbAlpha = TRUE;
			}
			pBitsI += 4;
		}

		if (pbAlpha && (*pbAlpha == FALSE))
		{
			MEMCPY_S(pBitsDest, pBitsSrc, nSize);
		}

		if (lpBits)
		{
			*lpBits = pBitsDest;
		}

	}
	CATCH (CMemoryException, e)
	{
		TRACE(_T("Failed -- Memory exception thrown."));
	}
	END_CATCH

	FREE(pBitsSrc);
	FREE(pBitmapInfo);

	return hBitmapAlpha;
}

BOOL CUIImageManagerIcon::GetBitmapBits(CDC& dcSrc, HBITMAP hBitmap, PBITMAPINFO& pBitmapInfo, LPVOID& pBits, UINT& nSize)
{
	if (hBitmap == 0)
		return FALSE;

	BITMAPINFO  bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);


	if (GetDIBits(dcSrc, hBitmap, 0, 0, NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS) == 0)
		return FALSE;

	if (bmi.bmiHeader.biBitCount != 32)
		return FALSE;

	nSize = bmi.bmiHeader.biHeight * bmi.bmiHeader.biWidth * 4;
	pBits = (PBYTE)malloc(nSize);
	if (pBits == NULL)
		return FALSE;

	if ((pBitmapInfo = (PBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + 3 * sizeof(COLORREF))) == NULL)
	{
		FREE(pBits);
		return FALSE;
	}

	MEMCPY_S(pBitmapInfo, &bmi, sizeof(BITMAPINFOHEADER));

	if (GetDIBits(dcSrc, hBitmap, 0, bmi.bmiHeader.biHeight, pBits, pBitmapInfo, DIB_RGB_COLORS) == NULL)
	{
		FREE(pBits);
		FREE(pBitmapInfo);
		return FALSE;
	}
	return TRUE;
}

HBITMAP CUIImageManagerIcon::CopyAlphaBitmap(HBITMAP hBitmap, LPBYTE* lpBits)
{
	CDC dcSrc;
	dcSrc.CreateCompatibleDC(NULL);

	PBYTE pBitsSrc = 0;
	PBITMAPINFO pBitmapInfo = 0;
	UINT nSize;

	if (!GetBitmapBits(dcSrc, hBitmap, pBitmapInfo, (LPVOID&)pBitsSrc, nSize))
		return 0;

	BYTE* pBitsDest = NULL;
	HBITMAP hBmp = CreateDIBSection(dcSrc, pBitmapInfo, DIB_RGB_COLORS, (void**)&pBitsDest, NULL, 0);

	if (pBitsDest != NULL && hBmp != NULL)
	{
		MEMCPY_S(pBitsDest, pBitsSrc, nSize);
	}

	FREE(pBitsSrc);
	FREE(pBitmapInfo);

	if (lpBits)
	{
		*lpBits = pBitsDest;
	}

	return hBmp;

}




//////////////////////////////////////////////////////////////////////////
// CUIImageManagerIcon
CUIImageManagerIcon::CUIImageManagerIcon(UINT nID, int nWidth, int nHeight, CUIImageManagerIconSet* pIconSet)
	: m_nID(nID)
	, m_nWidth(nWidth)
	, m_nHeight(nHeight)
	, m_pIconSet(pIconSet)
	, m_pImageList(NULL)
{
	m_bDrawComposited = FALSE;

	if (m_pIconSet)
	{
		m_hIcon.m_pImageManager = m_pIconSet->m_pImageManager;
		m_hFaded.m_pImageManager = m_pIconSet->m_pImageManager;
		m_hShadow.m_pImageManager = m_pIconSet->m_pImageManager;
		m_hHot.m_pImageManager = m_pIconSet->m_pImageManager;
		m_hChecked.m_pImageManager = m_pIconSet->m_pImageManager;
		m_hPressed.m_pImageManager = m_pIconSet->m_pImageManager;
		m_hDisabled.m_pImageManager = m_pIconSet->m_pImageManager;
		m_hDisabledAuto.m_pImageManager = m_pIconSet->m_pImageManager;
	}
}

CUIImageManagerIcon::CUIImageManagerIcon(UINT nID, int nWidth, int nHeight, CUIImageManagerImageList* pImageList)
	: m_nID(nID)
	, m_nWidth(nWidth)
	, m_nHeight(nHeight)
	, m_pIconSet(NULL)
	, m_pImageList(pImageList)
{

}


CUIImageManagerIcon::~CUIImageManagerIcon()
{
	Clear(TRUE);
}

UINT CUIImageManagerIcon::GetID() const
{
	return m_nID;
}

int CUIImageManagerIcon::GetHeight() const
{
	return m_nHeight;
}

int CUIImageManagerIcon::GetWidth() const
{
	return m_nWidth;
}

void CUIImageManagerIcon::Clear(BOOL bIcon /* = FALSE */)
{
	if (bIcon)
	{
		m_hIcon.Clear();
	}
	m_hFaded.Clear();
	m_hShadow.Clear();
	m_hHot.Clear();
	m_hChecked.Clear();
	m_hPressed.Clear();
	m_hDisabled.Clear();
	m_hDisabledAuto.Clear();
}

void CUIImageManagerIcon::Refresh()
{
	m_hFaded.Clear();
	m_hShadow.Clear();
	m_hDisabledAuto.Clear();
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetIcon()
{
	return m_hIcon;
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetIcon(UIImageState imageState)
{
	return imageState == uiImageDisabled ? GetDisabledIcon() : imageState == uiImageHot? GetHotIcon() :
		imageState == uiImageChecked? GetCheckedIcon() : imageState == uiImagePressed? GetPressedIcon() : GetIcon();
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetFadedIcon()
{
	if (m_hFaded.IsEmpty())
	{
		CreateFadedIcon();
	}
	return m_hFaded.IsEmpty() ? m_hIcon : m_hFaded;
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetShadowIcon()
{
	if (m_hShadow.IsEmpty())
	{
		CreateShadowIcon();
	}
	return m_hShadow;
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetHotIcon()
{
	return !m_hHot.IsEmpty() ? m_hHot : m_hIcon;
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetCheckedIcon()
{
	return !m_hChecked.IsEmpty() ? m_hChecked : GetHotIcon();
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetPressedIcon()
{
	return !m_hPressed.IsEmpty() ? m_hPressed : GetHotIcon();
}

BOOL CUIImageManagerIcon::HasDisabledIcon() const
{
	if (!m_hDisabled.IsEmpty())
		return TRUE;

	if (!m_hDisabledAuto.IsEmpty())
		return TRUE;

	return FALSE;
}

CUIImageManagerIconHandle& CUIImageManagerIcon::GetDisabledIcon(BOOL bCreateIfNotExists /*= TRUE*/)
{
	if (!m_hDisabled.IsEmpty())
		return m_hDisabled;

	if (!m_hDisabledAuto.IsEmpty())
		return m_hDisabledAuto;

	if (bCreateIfNotExists)
	{
		CreateDisabledIcon();
	}

	return m_hDisabledAuto;
}

BOOL CUIImageManagerIcon::SetIcon(const CUIImageManagerIconHandle& iconHandle, UIImageState imageState)
{
	switch (imageState)
	{
		case uiImageNormal:
			SetNormalIcon(iconHandle);
			break;
		case uiImageHot:
			SetHotIcon(iconHandle);
			break;
		case uiImageChecked:
			SetCheckedIcon(iconHandle);
			break;
		case uiImageDisabled:
			SetDisabledIcon(iconHandle);
			break;
		case uiImagePressed:
			SetPressedIcon(iconHandle);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}


BOOL CUIImageManagerIcon::SetIcon(const CUIImageManagerIconHandle& hIcon)
{
	ASSERT(!hIcon.IsEmpty());
	Clear(TRUE);
	m_hIcon.CopyHandle(hIcon);
	CSize sz = GetExtent();

	m_nHeight = sz.cy;
	m_nWidth = sz.cx;

	if (m_pIconSet)
		m_pIconSet->RefreshAll();

	return TRUE;
}

BOOL CUIImageManagerIcon::SetIcon(UINT nIDResourceIcon, int nWidth, int nHeight)
{
	Clear(TRUE);

	ASSERT(nWidth != 0 && nHeight != 0);

	if (!m_hIcon.CreateIconFromResource(MAKEINTRESOURCE(nIDResourceIcon), CSize(nWidth, nHeight)))
		return FALSE;

	m_nHeight = nHeight;
	m_nWidth = nWidth;

	return TRUE;
}

void CUIImageManagerIcon::SetNormalIcon(const CUIImageManagerIconHandle& hIcon)
{
	ASSERT(!hIcon.IsEmpty());

	Refresh();
	m_hIcon.CopyHandle(hIcon);


#ifdef _DEBUG
	CSize sz = GetExtent();

	ASSERT((int)m_nHeight == sz.cy);
	ASSERT((int)m_nWidth == sz.cx);

#endif
}

void CUIImageManagerIcon::SetHotIcon(const CUIImageManagerIconHandle& hIcon)
{
	Refresh();
	m_hHot.CopyHandle(hIcon);
}

void CUIImageManagerIcon::SetCheckedIcon(const CUIImageManagerIconHandle& hIcon)
{
	Refresh();
	m_hChecked.CopyHandle(hIcon);
}

void CUIImageManagerIcon::SetPressedIcon(const CUIImageManagerIconHandle& hIcon)
{
	Refresh();
	m_hPressed.CopyHandle(hIcon);
}

void CUIImageManagerIcon::SetDisabledIcon(const CUIImageManagerIconHandle& hIcon)
{
	Refresh();
	m_hDisabled.CopyHandle(hIcon);
}


COLORREF CUIImageManagerIcon::LightenColor(COLORREF clr, double factor)
{
	return RGB(
		factor * 255 + (1.0 - factor) * GetRValue(clr),
		factor * 255 + (1.0 - factor) * GetGValue(clr),
		factor * 255 + (1.0 - factor) * GetBValue(clr)) ;
}

void CUIImageManagerIcon::CreateFadedIcon()
{
	ASSERT(m_hFaded.IsEmpty());

	if (!m_hIcon.m_bUseResources)
		return;

	CUIImageManagerIconHandle hFaded;
	hFaded.CopyHandle(m_hIcon);

	if (!hFaded.IsAlpha())
	{
		ICONINFO info;
		if (GetIconInfo(hFaded.GetIcon(), &info))
		{
			if (!CUIDrawHelpers::IsLowResolution())
			{
				CUICompatibleDC dc(NULL, CBitmap::FromHandle(info.hbmColor));
				CUICompatibleDC dcMask(NULL, CBitmap::FromHandle(info.hbmMask));

				BITMAP bmp;
				::GetObject(info.hbmColor, sizeof(BITMAP), &bmp);

				for (int i = 0; i < bmp.bmWidth; i++)
				for (int j = 0; j < bmp.bmHeight; j++)
				{
					COLORREF clr = dc.GetPixel(i, j);
					COLORREF clrMask = dcMask.GetPixel(i, j);
					if (clrMask == 0)
						dc.SetPixel(i, j, LightenColor(clr, .3));
				}
			}

			hFaded.Attach(CreateIconIndirect(&info), FALSE);

			::DeleteObject(info.hbmColor);
			::DeleteObject(info.hbmMask);
		}
	}
	else if (hFaded.PreMultiply())
	{
		PBYTE pBits = hFaded.PreMultiply();
		CSize szIcon = hFaded.GetExtent();

		UINT nCount = szIcon.cx * szIcon.cy;

		for (UINT i = 0; i < nCount; i++)
		{
			int nAlpha = pBits[3];
			if (nAlpha != 0)
			{
				pBits[0] = BYTE(.3 * nAlpha + (1.0 - .3) * pBits[0]);
				pBits[1] = BYTE(.3 * nAlpha + (1.0 - .3) * pBits[1]);
				pBits[2] = BYTE(.3 * nAlpha + (1.0 - .3) * pBits[2]);
			}
			pBits += 4;
		}
	}

	m_hFaded.CopyHandle(hFaded);
}

void CUIImageManagerIcon::CreateShadowIcon()
{
	ASSERT(m_hShadow.IsEmpty());

	if (!m_hIcon.m_bUseResources)
		return;

	COLORREF clrBackground = GetXtremeColor(XPCOLOR_HIGHLIGHT);
	COLORREF clrShadow = RGB(GetRValue(clrBackground) * .75, GetGValue(clrBackground) * .75, GetBValue(clrBackground) * .75);

	CUIImageManagerIconHandle hShadow;
	hShadow.CopyHandle(GetHotIcon());

	if (!hShadow.IsAlpha())
	{
		ICONINFO info;
		if (GetIconInfo(hShadow.GetIcon(), &info))
		{
			{
				CUICompatibleDC dc(NULL, CBitmap::FromHandle(info.hbmColor));
				CUICompatibleDC dcMask(NULL, CBitmap::FromHandle(info.hbmMask));

				BITMAP bmp;
				::GetObject(info.hbmColor, sizeof(BITMAP), &bmp);

				for (int i = 0; i < bmp.bmWidth; i++)
				for (int j = 0; j < bmp.bmHeight; j++)
				{
					COLORREF clrMask = dcMask.GetPixel(i, j);
					if (clrMask == 0)
						dc.SetPixel(i, j, clrShadow);
				}
			}

			hShadow.Attach(CreateIconIndirect(&info), FALSE);

			::DeleteObject(info.hbmMask);
			::DeleteObject(info.hbmColor);
		}
	}
	else if (hShadow.PreMultiply())
	{
		PBYTE pBits = hShadow.PreMultiply();
		CSize szIcon = hShadow.GetExtent();

		UINT nCount = szIcon.cx * szIcon.cy;

		for (UINT i = 0; i < nCount; i++)
		{
			int nAlpha = pBits[3];
			pBits[0] = BYTE(GetRValue(clrShadow) * nAlpha / 255);
			pBits[1] = BYTE(GetGValue(clrShadow) * nAlpha / 255);
			pBits[2] = BYTE(GetBValue(clrShadow) * nAlpha / 255);
			pBits += 4;
		}
	}

	m_hShadow.CopyHandle(hShadow);
}

void CUIImageManagerIcon::CreateDisabledIcon(COLORREF clrDisabledLight /*= (COLORREF)-1*/, COLORREF clrDisabledDark /*= (COLORREF)-1*/)
{
	if (!m_hDisabledAuto.IsEmpty())
		return;

	BOOL bOfficeDisabledIcons = clrDisabledLight != (COLORREF)-1 && clrDisabledDark != (COLORREF)-1;

	CUIImageManagerIconHandle hDisabled;
	hDisabled.CopyHandle(m_hIcon);

	if (!hDisabled.IsAlpha())
	{
		ICONINFO info;
		if (GetIconInfo(hDisabled.GetIcon(), &info))
		{
			{
				CUICompatibleDC dc(NULL, CBitmap::FromHandle(info.hbmColor));
				CUICompatibleDC dcMask(NULL, CBitmap::FromHandle(info.hbmMask));

				BITMAP bmp;
				::GetObject(info.hbmColor, sizeof(BITMAP), &bmp);

				for (int i = 0; i < bmp.bmWidth; i++)
				for (int j = 0; j < bmp.bmHeight; j++)
				{
					COLORREF clrMask = dcMask.GetPixel(i, j);
					COLORREF clr = dc.GetPixel(i, j);

					if (clrMask == 0)
					{
						if (bOfficeDisabledIcons)
						{
							double dGray = (GetRValue(clr) * 0.299 + GetGValue(clr) * 0.587 + GetBValue(clr) * 0.114)/255;
							double dLight = 1.0 - dGray;

							clr = RGB(dLight * GetRValue(clrDisabledDark) + dGray * GetRValue(clrDisabledLight),
								dLight * GetGValue(clrDisabledDark) + dGray * GetGValue(clrDisabledLight),
								dLight * GetBValue(clrDisabledDark) + dGray * GetBValue(clrDisabledLight));

							dc.SetPixel(i, j, clr);
						}
						else
						{
							double dGray = GetRValue(clr) * 0.299 + GetGValue(clr) * 0.587 + GetBValue(clr) * 0.114;
							int nGray = (BYTE)(pow(dGray / 255.0, CUIImageManager::m_dDisabledBrightnessFactor) * 255.0);

							dc.SetPixel(i, j, RGB(nGray, nGray, nGray));
						}
					}
				}
			}

			hDisabled.Attach(CreateIconIndirect(&info), FALSE);
			::DeleteObject(info.hbmMask);
			::DeleteObject(info.hbmColor);

		}
	}
	else if (hDisabled.PreMultiply())
	{
		PBYTE pBits = hDisabled.PreMultiply();
		CSize szIcon = hDisabled.GetExtent();

		UINT nCount = szIcon.cx * szIcon.cy;

		for (UINT i = 0; i < nCount; i++)
		{
			int nAlpha = pBits[3];
			if (nAlpha != 0)
			{
				int R = pBits[0] * 255 / nAlpha;
				int G = pBits[1] * 255 / nAlpha;
				int B = pBits[2] * 255 / nAlpha;

				if (bOfficeDisabledIcons)
				{
					double dGray = (R * 0.114 + G * 0.587 + B * 0.299)/255.0;
					double dLight = 1.0 - dGray;

					R = BYTE(dLight * GetBValue(clrDisabledDark) + dGray * GetBValue(clrDisabledLight));
					G = BYTE(dLight * GetGValue(clrDisabledDark) + dGray * GetGValue(clrDisabledLight));
					B = BYTE(dLight * GetRValue(clrDisabledDark) + dGray * GetRValue(clrDisabledLight));
				}
				else
				{
					double dGray = R * 0.114 + G * 0.587 + B * 0.299;
					R = G = B = (BYTE)(pow(dGray / 255.0, CUIImageManager::m_dDisabledBrightnessFactor) * 255.0);
					nAlpha = BYTE(nAlpha / CUIImageManager::m_dDisabledAlphaFactor);
				}

				pBits[0] = BYTE(R * nAlpha / 255);
				pBits[1] = BYTE(G * nAlpha / 255);
				pBits[2] = BYTE(B * nAlpha / 255);
			}

			pBits += 4;
		}
	}

	m_hDisabledAuto.CopyHandle(hDisabled);
}

CSize CUIImageManagerIcon::GetExtent() const
{
	return m_hIcon.GetExtent();
}

CSize CUIImageManagerIcon::GetExtent(HICON hIcon)
{
	ASSERT(hIcon);

	CSize extent(0);
	if (hIcon)
	{
		ICONINFO iconinfo;
		if (::GetIconInfo(hIcon, &iconinfo))
		{
			BITMAP bmpinfo;
			if (::GetObject(iconinfo.hbmMask, sizeof(bmpinfo), &bmpinfo))
			{
				extent.cx = (int)bmpinfo.bmWidth;
				extent.cy = (int)bmpinfo.bmHeight;
				if (!iconinfo.hbmColor)
				{
					// b/w icons have double size for XOR and AND masks
					extent.cy /= 2;
				}
			}
			// cleanup GDI
			if (iconinfo.hbmMask) ::DeleteObject(iconinfo.hbmMask);
			if (iconinfo.hbmColor) ::DeleteObject(iconinfo.hbmColor);
		}
	}
	return extent;
}


HICON CUIImageManagerIcon::ScaleToFit(HICON hIcon, CSize szExtent, int nWidth)
{
	if (nWidth == 0 || szExtent == CSize(0))
	{
		// invalid arg
		return NULL;
	}
	if (nWidth == szExtent.cx)
		return ::CopyIcon(hIcon);

	CSize szDesiredExtent(nWidth, MulDiv(szExtent.cy, nWidth, szExtent.cx));

	// scale the icon
	CImageList images;
	VERIFY(images.Create(szDesiredExtent.cx, szDesiredExtent.cy, ILC_COLOR32 | ILC_MASK, 1, 1));
	images.Add(hIcon);
	return images.ExtractIcon(0);
}

BOOL CUIImageManagerIcon::GetDIBBitmap(HBITMAP hBitmap, PBYTE& pBits, UINT& nBitsSize, PBITMAPINFO& pBitmapInfo, UINT& nBitmapInfoSize)
{
	pBits = 0;
	pBitmapInfo = 0;

	BITMAPINFO  bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	CDC dcSrc;
	dcSrc.CreateCompatibleDC(NULL);

	TRY
	{
		if (GetDIBits(dcSrc, hBitmap, 0, 0, NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS) == 0)
			AfxThrowMemoryException();

		nBitsSize = bmi.bmiHeader.biSizeImage != 0? bmi.bmiHeader.biSizeImage :
			bmi.bmiHeader.biHeight * bmi.bmiHeader.biWidth * 4;

		pBits = (PBYTE)malloc(nBitsSize);
		if (pBits == NULL)
			AfxThrowMemoryException();

		int nColorTableSize = bmi.bmiHeader.biBitCount == 4 ? 16 : bmi.bmiHeader.biBitCount == 8 ? 256 : 3;

		nBitmapInfoSize = sizeof(BITMAPINFOHEADER) + nColorTableSize * sizeof(COLORREF);
		if ((pBitmapInfo = (PBITMAPINFO)malloc(nBitmapInfoSize)) == NULL)
			AfxThrowMemoryException();

		MEMCPY_S(pBitmapInfo, &bmi, sizeof(BITMAPINFOHEADER));

		if (GetDIBits(dcSrc, hBitmap, 0, bmi.bmiHeader.biHeight, pBits, pBitmapInfo, DIB_RGB_COLORS) == NULL)
			AfxThrowMemoryException();
	}
	CATCH (CMemoryException, e)
	{
		TRACE(_T("Failed -- Memory exception thrown."));
		return FALSE;
	}
	END_CATCH

	return TRUE;
}

HBITMAP CUIImageManagerIcon::ReadDIBBitmap(CArchive& ar, LPBYTE* lpBits)
{
	PBITMAPINFO pBitmapInfo = 0;
	PBYTE pBitsSrc = 0;

	DWORD dwCount = (DWORD) ar.ReadCount();
	DWORD dwBitsCount = 0;

	if (dwCount > 0)
	{
		pBitmapInfo = (PBITMAPINFO)malloc(dwCount);
		ar.Read(pBitmapInfo, dwCount);

		dwBitsCount = (DWORD) ar.ReadCount();
		pBitsSrc = (PBYTE)malloc(dwBitsCount);
		ar.Read(pBitsSrc, dwBitsCount);
	}
	else
	{
		return NULL;
	}

	if (!pBitmapInfo || dwBitsCount == 0)
	{
		FREE(pBitsSrc);
		FREE(pBitmapInfo);
		return NULL;
	}

	CDC dcSrc;
	dcSrc.CreateCompatibleDC(NULL);

	if (pBitmapInfo->bmiHeader.biSizeImage == 0)
	{
		pBitmapInfo->bmiHeader.biSizeImage = dwBitsCount;
	}

	BYTE* pBitsDest = NULL;
	HBITMAP hBmp = CreateDIBSection(dcSrc, pBitmapInfo, DIB_RGB_COLORS, (void**)&pBitsDest, NULL, 0);

	if (!pBitsDest || !hBmp)
	{
		FREE(pBitsSrc);
		FREE(pBitmapInfo);
		return NULL;
	}

	MEMCPY_S(pBitsDest, pBitsSrc, min(dwBitsCount, pBitmapInfo->bmiHeader.biSizeImage));

	FREE(pBitsSrc);
	FREE(pBitmapInfo);

	if (lpBits)
	{
		*lpBits = pBitsDest;
	}

	return hBmp;
}

void CUIImageManagerIcon::WriteDIBBitmap(CArchive& ar, HBITMAP hBitmap)
{
	PBYTE pBits = 0;
	PBITMAPINFO pBitmapInfo = 0;
	UINT nBitsSize;
	UINT nBitmapInfoSize;

	if (GetDIBBitmap(hBitmap, pBits, nBitsSize, pBitmapInfo, nBitmapInfoSize))
	{
		ar.WriteCount(nBitmapInfoSize);
		ar.Write(pBitmapInfo, nBitmapInfoSize);
		ar.WriteCount(nBitsSize);
		ar.Write(pBits, nBitsSize);
	}
	else
	{
		ar.WriteCount(0);
	}
	FREE(pBits);
	FREE(pBitmapInfo);
}

void CUIImageManagerIcon::SerializeIcon(CUIImageManagerIconHandle& hIcon, CArchive& ar, long nSchema)
{
	if (ar.IsStoring())
	{
		CUIImageManagerIconHandle hHandle;
		hHandle.CopyHandle(hIcon);
		hHandle.PreMultiply();

		ICONINFO info;

		if (!hHandle.IsEmpty())
		{
			if (hHandle.IsAlpha())
			{
				WriteDIBBitmap(ar, hHandle.GetBitmap());
				ar.WriteCount(0);
			}
			else if (GetIconInfo(hHandle.GetIcon(), &info))
			{
				WriteDIBBitmap(ar, info.hbmColor);
				WriteDIBBitmap(ar, info.hbmMask);

				::DeleteObject(info.hbmColor);
				::DeleteObject(info.hbmMask);

			}
			else
			{
				ar.WriteCount(0);
				ar.WriteCount(0);
			}
		}
		else
		{
			ar.WriteCount(0);
			ar.WriteCount(0);
		}
	}
	else
	{
		CUIImageManagerIconHandle hHandle;

		LPBYTE lpBits = NULL;
		HBITMAP hbmColor = ReadDIBBitmap(ar, &lpBits);
		HBITMAP hbmMask = ReadDIBBitmap(ar, NULL);

		if (hbmColor)
		{
			if (hbmMask == 0)
			{
				hHandle.Attach(hbmColor, FALSE);

				if (nSchema > _UI_SCHEMA_112)
				{
					hHandle.m_pBits = lpBits;
				}
				hbmColor = 0;
			}
			else
			{
				BITMAP bmp;
				::GetObject(hbmColor, sizeof(BITMAP), &bmp);

				CImageList il;
				il.Create(bmp.bmWidth, bmp.bmHeight, ILC_COLOR24 | ILC_MASK, 0, 1);
				il.Add(CBitmap::FromHandle(hbmColor), hbmMask == 0 ? (CBitmap*)NULL : CBitmap::FromHandle(hbmMask));
				hHandle.Attach(il.ExtractIcon(0), FALSE);
			}

			if (hbmColor) ::DeleteObject(hbmColor);
			if (hbmMask) ::DeleteObject(hbmMask);
		}

		hIcon.CopyHandle(hHandle);
	}

}

void CUIImageManagerIcon::Serialize(CArchive& ar)
{
	long nSchema = _UI_SCHEMA_CURRENT;

	if (ar.IsStoring())
	{
		ar << m_nHeight;
		ar << nSchema;

	}
	else
	{
		ar >> m_nHeight;
		ar >> nSchema;
	}
	SerializeIcon(m_hIcon, ar, nSchema);
	SerializeIcon(m_hDisabled, ar, nSchema);
	SerializeIcon(m_hHot, ar, nSchema);

	if (nSchema > _UI_SCHEMA_875)
	{
		SerializeIcon(m_hChecked, ar, nSchema);
	}
	if (nSchema > _UI_SCHEMA_98)
	{
		SerializeIcon(m_hPressed, ar, nSchema);
	}
}

void CUIImageManagerIcon::Copy(CUIImageManagerIcon* pIcon)
{
	ASSERT(m_nID == pIcon->GetID());
	m_nWidth = pIcon->m_nWidth;
	m_nHeight = pIcon->m_nHeight;

	m_hIcon.CopyHandle(pIcon->m_hIcon);
	m_hDisabled.CopyHandle(pIcon->m_hDisabled);
	m_hHot.CopyHandle(pIcon->m_hHot);
	m_hChecked.CopyHandle(pIcon->m_hChecked);
	m_hPressed.CopyHandle(pIcon->m_hPressed);

	m_hFaded.Clear();
	m_hShadow.Clear();
	m_hDisabledAuto.Clear();
}

void CUIImageManagerIcon::DrawAlphaBitmap(CDC* pDC, HBITMAP hBitmap, CPoint ptDest, CSize szDest, CPoint ptSrc, CSize szSrc)
{
	if (szSrc == CSize(0))
		szSrc = szDest;

	CUICompatibleDC dc(NULL, CBitmap::FromHandle(hBitmap));

	UIImageManager()->AlphaBlend(pDC->GetSafeHdc(), CRect(ptDest, szDest),
		dc, CRect(ptSrc, szSrc));

}
CUIImageManager* CUIImageManagerIcon::GetImageManager() const
{
	if (m_pIconSet)
	{
		ASSERT(m_pIconSet->m_pImageManager);
		return m_pIconSet->m_pImageManager;
	}

	return UIImageManager();
}

void CUIImageManagerIcon::DrawMono(CDC* pDC, CPoint pt, CUIImageManagerIconHandle& hIcon, CSize szIcon, COLORREF clrBrush)
{
	if (hIcon.IsAlpha())
		return;

	if (szIcon.cx == 0)
		szIcon.cx = m_nWidth;

	if (szIcon.cy == 0)
		szIcon.cy = m_nHeight * szIcon.cx / m_nWidth;

	CBitmap bmp;
	bmp.CreateCompatibleBitmap(pDC, szIcon.cx, szIcon.cy);
	{
		CUICompatibleDC dc(pDC, &bmp);
		dc.FillSolidRect(0, 0, szIcon.cx, szIcon.cy, 0xFFFFFF);

		Draw(&dc, CPoint(0, 0), hIcon, szIcon);
	}
	pDC->DrawState(pt, szIcon, bmp, (UINT)DSS_MONO, CBrush(clrBrush));
}

void CUIImageManagerIcon::Draw(CDC* pDC, CPoint pt, CSize szIcon)
{
	Draw(pDC, pt, GetIcon(), szIcon);
}

void CUIImageManagerIcon::Draw(CDC* pDC, CPoint pt)
{
	Draw(pDC, pt, GetIcon());
}

void CUIImageManagerIcon::DrawIconComposited(CDC* pDC, CPoint pt, CSize szIcon, HICON hIcon)
{
	CRect rcDest;
	if (szIcon.cx < 0)
	{
		rcDest.SetRect(pt.x + szIcon.cx, pt.y, pt.x, pt.y + szIcon.cy);
	}
	else
	{
		rcDest.SetRect(pt.x, pt.y, pt.x + szIcon.cx, pt.y + szIcon.cy);
	}

	CSize sz(rcDest.Width(), rcDest.Height());
	CBitmap bmp;
	LPDWORD lpBits;
	bmp.Attach(CUIImageManager::Create32BPPDIBSection(pDC->GetSafeHdc(), sz.cx, sz.cy, (LPBYTE*)&lpBits));

	CUICompatibleDC dc(pDC, &bmp);

	CRect rcDestOrig(0, 0, sz.cx, sz.cy);

	dc.FillSolidRect(rcDestOrig, 0xFF00FF);
	DrawIconEx(dc, szIcon.cx > 0 ? 0 : -szIcon.cx, 0, hIcon, szIcon.cx, szIcon.cy, 0, 0, DI_NORMAL);

	for (int i = 0; i < sz.cx * sz.cy; i++)
	{
		if (lpBits[0] == 0xFF00FF)
		{
			lpBits[0] = 0;
		}
		else
		{
			lpBits[0] |= 0xFF000000;
		}
		lpBits++;
	}

	UIImageManager()->AlphaBlend(*pDC, rcDest, dc, rcDestOrig);
}

void CUIImageManagerIcon::Draw(CDC* pDC, CPoint pt, CUIImageManagerIconHandle& hIcon, CSize szIcon /*= 0*/, COLORREF clrBK, COLORREF clrFG, UINT uiFlags)
{
	if (szIcon.cx == 0)
		szIcon.cx = m_nWidth;

	if (szIcon.cy == 0)
		szIcon.cy = m_nHeight * szIcon.cx / m_nWidth;

	if (m_pImageList)
	{
		m_pImageList->Draw(pDC, pt, this, szIcon,clrBK, clrFG, uiFlags);
		return;
	}

	hIcon.Draw(pDC, pt, szIcon, m_bDrawComposited);
}



//////////////////////////////////////////////////////////////////////////
// CUIImageManagerIconSet

CUIImageManagerIconSet::CUIImageManagerIconSet(UINT nID, CUIImageManager* pImageManager)
	: m_nID(nID)
{
	ASSERT(pImageManager);
	m_pImageManager = pImageManager;
}

CUIImageManagerIconSet::~CUIImageManagerIconSet()
{
	RemoveAll();
}

void CUIImageManagerIconSet::RemoveAll()
{
	UINT nWidth;
	CUIImageManagerIcon* pIcon;

	POSITION pos = m_mapIcons.GetStartPosition();
	while (pos)
	{
		m_mapIcons.GetNextAssoc(pos, nWidth, pIcon);
		pIcon->Clear(TRUE);
		pIcon->InternalRelease();
	}
	m_mapIcons.RemoveAll();
}

CUIImageManagerIcon* CUIImageManagerIconSet::CreateIcon(UINT nWidth)
{
	CUIImageManagerIcon* pIcon = 0;
	if (m_mapIcons.Lookup(nWidth, pIcon))
	{
		ASSERT(pIcon->m_nID == m_nID);
		ASSERT(pIcon->m_nWidth == nWidth);
	}
	else
	{
		pIcon = new CUIImageManagerIcon(m_nID, nWidth, 15, this);
		m_mapIcons.SetAt(nWidth, pIcon);
	}

	ASSERT(pIcon);

	return pIcon;
}

void CUIImageManagerIconSet::SetIcon(const CUIImageManagerIconHandle& hIcon, UINT nWidth)
{
	CUIImageManagerIcon* pIcon = CreateIcon(nWidth);
	pIcon->SetIcon(hIcon);
}

void CUIImageManagerIconSet::SetDisabledIcon(const CUIImageManagerIconHandle& hIcon, UINT nWidth)
{
	CUIImageManagerIcon* pIcon = CreateIcon(nWidth);
	pIcon->SetDisabledIcon(hIcon);
}

void CUIImageManagerIconSet::SetHotIcon(const CUIImageManagerIconHandle& hIcon, UINT nWidth)
{
	CUIImageManagerIcon* pIcon = CreateIcon(nWidth);
	pIcon->SetHotIcon(hIcon);
}

void CUIImageManagerIconSet::SetCheckedIcon(const CUIImageManagerIconHandle& hIcon, UINT nWidth)
{
	CUIImageManagerIcon* pIcon = CreateIcon(nWidth);
	pIcon->SetCheckedIcon(hIcon);
}

void CUIImageManagerIconSet::SetPressedIcon(const CUIImageManagerIconHandle& hIcon, UINT nWidth)
{
	CUIImageManagerIcon* pIcon = CreateIcon(nWidth);
	pIcon->SetPressedIcon(hIcon);
}

CUIImageManagerIcon* CUIImageManagerIconSet::GetIcon(UINT nWidth, BOOL bScaled)
{
	ASSERT(m_mapIcons.GetCount() > 0);

	CUIImageManagerIcon* pIcon = NULL;
	BOOL bSpecial = (nWidth == ICON_BIG) || (nWidth == ICON_SMALL);

	if (!bSpecial)
	{
		if (m_mapIcons.Lookup(nWidth, pIcon))
		{
			return pIcon;
		}
		if (!bScaled)
			return NULL;
	}

	UINT nDelta = 0;

	POSITION pos = m_mapIcons.GetStartPosition();
	while (pos)
	{
		UINT nWidthScale;
		CUIImageManagerIcon* pIconScale;

		m_mapIcons.GetNextAssoc(pos, nWidthScale, pIconScale);

		if (bSpecial)
		{
			if (((nDelta == 0) || ((nWidth == ICON_SMALL && nWidthScale < nDelta)
				|| (nWidth == ICON_BIG && nWidthScale > nDelta))))
			{
				pIcon = pIconScale;
				nDelta = nWidthScale;
			}
		}
		else if (nDelta == 0 || (UINT)abs(int(nWidthScale - nWidth)) < nDelta)
		{
			pIcon = pIconScale;
			nDelta = (UINT)abs(int(nWidthScale - nWidth));
		}
	}

	ASSERT(pIcon != 0);
	if (!pIcon)
		return NULL;

	return pIcon;
}


void CUIImageManagerIconSet::RefreshAll()
{
	POSITION pos = m_mapIcons.GetStartPosition();
	UINT nWidth;
	CUIImageManagerIcon* pIcon;

	while (pos)
	{
		m_mapIcons.GetNextAssoc(pos, nWidth, pIcon);

		pIcon->Refresh();
	}
}



//////////////////////////////////////////////////////////////////////////
// CUIImageManager

CUIImageManager::CUIImageManager()
{
	m_clrMask = (COLORREF)-1;

	m_nCustomID = 0x10AAA;

	m_mapImages.InitHashTable(UI_IMAGEMAN_HASH_TABLE_SIZE, FALSE);

	m_bDrawReverted = 2;

	m_bUseResources = TRUE;
}

CUIImageManager* AFX_CDECL UIImageManager()
{
	static CUIImageManager s_managerInstance;
	return &s_managerInstance;
}

CUIImageManager::~CUIImageManager()
{
	RemoveAll();
}

BOOL CUIImageManager::LoadToolbar(UINT nIDResourceToolBar, UINT*& pItems, int& nCount, CSize& szIcon) const
{
	LPCTSTR lpszResourceName = MAKEINTRESOURCE(nIDResourceToolBar);

	struct CToolBarData
	{
		WORD wVersion;
		WORD wWidth;
		WORD wHeight;
		WORD wItemCount;

		WORD* items()
		{
			return (WORD*)(this + 1);
		}
	};

	ASSERT(lpszResourceName != NULL);
	if (!lpszResourceName)
		return FALSE;

	// determine location of the bitmap in resource fork
	HINSTANCE hInst = AfxFindResourceHandle(lpszResourceName, RT_TOOLBAR);
	HRSRC hRsrc = ::FindResource(hInst, lpszResourceName, RT_TOOLBAR);
	if (hRsrc == NULL)
		return FALSE;

	HGLOBAL hGlobal = LoadResource(hInst, hRsrc);
	if (hGlobal == NULL)
		return FALSE;

	CToolBarData* pData = (CToolBarData*)LockResource(hGlobal);
	if (pData == NULL)
		return FALSE;

	ASSERT(pData->wVersion == 1);

	pItems = new UINT[pData->wItemCount];

	nCount = 0;
	for (int i = 0; i < pData->wItemCount; i++)
		if (pData->items()[i]) pItems[nCount++] = pData->items()[i];

	szIcon = CSize(pData->wWidth, pData->wHeight);

	UnlockResource(hGlobal);
	FreeResource(hGlobal);
	return TRUE;
}

BOOL CUIImageManager::SetIcons(UINT nIDResourceToolBar, CBitmap& bmpIcons, UIImageState imageState)
{
	UINT* pItems = NULL;
	CSize szIcon(0);
	int nCount = 0;
	BOOL bResult = LoadToolbar(nIDResourceToolBar, pItems, nCount, szIcon);

	if (bResult && !SetIcons(bmpIcons, pItems, nCount, szIcon, imageState))
		bResult = FALSE;

	if (pItems)
	{
		delete[] pItems;
	}

	return bResult;
}
BOOL CUIImageManager::SetIcons(UINT nIDResourceToolBar, UINT nIDResourceBitmap, UIImageState imageState)
{
	UINT* pItems = NULL;
	CSize szIcon(0);
	int nCount = 0;
	BOOL bResult = LoadToolbar(nIDResourceToolBar, pItems, nCount, szIcon);

	if (bResult && !SetIcons(nIDResourceBitmap, pItems, nCount, CSize(0, 0), imageState))
		bResult = FALSE;

	if (pItems)
	{
		delete[] pItems;
	}

	return bResult;

}


BOOL CUIImageManager::SetIcons(UINT nIDResourceToolBar, UIImageState imageState)
{
	return SetIcons(nIDResourceToolBar, nIDResourceToolBar, imageState);
}

BOOL CUIImageManager::SetIcons(UINT nIDResourceToolBar, CImageList& imlIcons, UIImageState imageState)
{
	UINT* pItems = NULL;
	CSize szIcon(0);
	int nCount = 0;
	BOOL bResult = LoadToolbar(nIDResourceToolBar, pItems, nCount, szIcon);

	if (bResult && !SetIcons(imlIcons, pItems, nCount, szIcon, imageState))
		bResult = FALSE;

	if (pItems)
	{
		delete[] pItems;
	}

	return bResult;

}

BOOL CUIImageManager::SetIcon(const CUIImageManagerIconHandle& hIcon, UINT nIDCommand, CSize szIcon, UIImageState imageState)
{
	if (hIcon.IsEmpty())
		return FALSE;

	if (szIcon == CSize(0))
	{
		szIcon = hIcon.GetExtent();
	}
	if (szIcon == CSize(0))
		return FALSE;

	CUIImageManagerIconSet* pIconSet;
	if (m_mapImages.Lookup(nIDCommand, pIconSet))
	{
		if (imageState == uiImageNormal) pIconSet->SetIcon(hIcon, szIcon.cx);
		if (imageState == uiImageDisabled) pIconSet->SetDisabledIcon(hIcon, szIcon.cx);
		if (imageState == uiImageHot) pIconSet->SetHotIcon(hIcon, szIcon.cx);
		if (imageState == uiImageChecked) pIconSet->SetCheckedIcon(hIcon, szIcon.cx);
		if (imageState == uiImagePressed) pIconSet->SetPressedIcon(hIcon, szIcon.cx);
	}
	else if (imageState == uiImageNormal)
	{
		pIconSet = new CUIImageManagerIconSet(nIDCommand, this);
		pIconSet->SetIcon(hIcon, szIcon.cx);
		m_mapImages.SetAt(nIDCommand, pIconSet);
	}
	else return FALSE;

	return TRUE;
}

BOOL CUIImageManager::SetIcon(UINT nIDResourceIcon, UINT nIDCommand, CSize szIcon, UIImageState imageState)
{
	return SetIcon(MAKEINTRESOURCE(nIDResourceIcon), nIDCommand, szIcon, imageState);
}

BOOL CUIImageManager::SetIconFromResource(HINSTANCE hInst, LPCTSTR lpszResourceName, UINT nIDCommand, CSize szIcon, UIImageState imageState)
{
	// determine location of the bitmap in resource fork
	HRSRC hRsrc = ::FindResource(hInst, lpszResourceName, RT_GROUP_ICON);
	if (hRsrc != NULL)
	{
		// Load and Lock to get a pointer to a GRPICONDIR
		HGLOBAL hGlobal = LoadResource(hInst, hRsrc);
		if (!hGlobal)
			return FALSE;

		CUIImageManagerIcon::GRPICONDIR* lpGrpIconDir = (CUIImageManagerIcon::GRPICONDIR*)
			LockResource(hGlobal);
		if (!lpGrpIconDir)
			return FALSE;

		for (WORD i = 0; i < lpGrpIconDir->idCount; i++)
		{
			CUIImageManagerIconHandle hIcon;
			CSize szResourceIcon(lpGrpIconDir->idEntries[i].bWidth, lpGrpIconDir->idEntries[i].bHeight);

			if ((szIcon == szResourceIcon) || (szIcon == CSize(0, 0)))
			{
				if (!hIcon.CreateIconFromResource(hInst, MAKEINTRESOURCE(lpGrpIconDir->idEntries[i].nID),
					szResourceIcon, FALSE))
				{
					return FALSE;
				}

				if (!SetIcon(hIcon, nIDCommand, szResourceIcon, imageState))
				{
					return FALSE;
				}
			}
		}
	}
	else
	{
		CUIImageManagerIconHandle hIcon;

		if (!hIcon.CreateIconFromResource(hInst, lpszResourceName, szIcon, FALSE))
		{
			return FALSE;
		}

		return SetIcon(hIcon, nIDCommand, szIcon, imageState);
	}
	return TRUE;
}


BOOL CUIImageManager::SetIcon(LPCTSTR lpszResourceName, UINT nIDCommand, CSize szIcon, UIImageState imageState)
{
	// determine location of the bitmap in resource fork
	HINSTANCE hInst = AfxFindResourceHandle(lpszResourceName, RT_GROUP_ICON);
	HRSRC hRsrc = ::FindResource(hInst, lpszResourceName, RT_GROUP_ICON);

	if (hRsrc != NULL)
	{
		SetIconFromResource(hInst, lpszResourceName, nIDCommand, szIcon, imageState);
	}
	else
	{
		CUIImageManagerIconHandle hIcon;

		if (!hIcon.CreateIconFromResource(lpszResourceName, szIcon, FALSE))
		{
			return FALSE;
		}

		return SetIcon(hIcon, nIDCommand, szIcon, imageState);
	}
	return TRUE;
}


BOOL CUIImageManager::SetIcons(CImageList& imlIcons, UINT* pCommands, int nCount, CSize szIcon, UIImageState imageState)
{
	if (!imlIcons.GetSafeHandle())
		return FALSE;

	if ((nCount == 0) && (pCommands == 0))
	{
		nCount = imlIcons.GetImageCount();
	}

	if (szIcon == CSize(0) && nCount > 0)
	{
		IMAGEINFO imageInfo;
		imlIcons.GetImageInfo(0, &imageInfo);
		szIcon = CRect(imageInfo.rcImage).Size();
	}

	BOOL bIncludeAll = imlIcons.GetImageCount() == nCount;
	int j = 0;

	for (int i = 0; i < nCount; i++)
	{
		if ((pCommands == NULL) || bIncludeAll || (pCommands[i] != 0 && pCommands[i] != IMAGE_PLACEHOLDER))
		{
			HICON hIcon = imlIcons.ExtractIcon(j);
			if (hIcon)
			{
				SetIcon(hIcon, pCommands ? pCommands[i]: i, szIcon, imageState);
				DestroyIcon(hIcon);
			}
			else
			{
				return FALSE;
			}
			j++;
		}
	}
	return TRUE;
}


BOOL CUIImageManager::SplitBitmap(HBITMAP hbmSource, int nCount, HBITMAP* pDest) const
{
	ASSERT(pDest != NULL);
	ASSERT(sizeof(int) == 4);

	if (!pDest)
		return FALSE;

	BITMAPINFO  bmi;
	memset(&bmi, 0, sizeof(bmi));
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	CDC dcSrc;
	dcSrc.CreateCompatibleDC(NULL);

	PINT pBits = 0;
	PBITMAPINFO pBitmapInfo = 0;

	BOOL bResult = TRUE;
	int i;

	TRY
	{
		if (GetDIBits(dcSrc, hbmSource, 0, 0, NULL, (BITMAPINFO*)&bmi, DIB_RGB_COLORS) == 0)
			AfxThrowMemoryException();

		UINT nSize = bmi.bmiHeader.biHeight * bmi.bmiHeader.biWidth * 4;
		pBits = (PINT)malloc(nSize);
		if (pBits == NULL)
			AfxThrowMemoryException();

		if ((pBitmapInfo = (PBITMAPINFO)malloc(sizeof(BITMAPINFOHEADER) + 3 * sizeof(COLORREF))) == NULL)
			AfxThrowMemoryException();

		MEMCPY_S(pBitmapInfo, &bmi, sizeof(BITMAPINFOHEADER));

		if (GetDIBits(dcSrc, hbmSource, 0, bmi.bmiHeader.biHeight, pBits, pBitmapInfo, DIB_RGB_COLORS) == NULL)
			AfxThrowMemoryException();

		ASSERT(pBitmapInfo->bmiHeader.biWidth % nCount == 0);

		pBitmapInfo->bmiHeader.biWidth /= nCount;
		pBitmapInfo->bmiHeader.biSizeImage /= nCount;

		PINT* pDestBits = new PINT[nCount];
		for (i = 0; i < nCount; i++)
		{
			pDest[i] = CreateDIBSection(dcSrc, pBitmapInfo, DIB_RGB_COLORS, (void**)&pDestBits[i], NULL, 0);
			if (!pDest[i])
			{
				for (int j = 0; j < i; j++)
					DeleteObject(pDest[j]);

				delete[] pDestBits;

				AfxThrowMemoryException();
			}
		}

		PINT pBitsIter = pBits;

		for (int y = 0; y < pBitmapInfo->bmiHeader.biHeight; y++)
		{
			for (i = 0; i < nCount; i++)
			{
				for (int x = 0; x < pBitmapInfo->bmiHeader.biWidth; x++)
				{
					*pDestBits[i]++ = *pBitsIter++;
				}
			}
		}
		delete[] pDestBits;

	}
	CATCH (CMemoryException, e)
	{
		TRACE(_T("Failed -- Memory exception thrown."));
		bResult = FALSE;
	}
	END_CATCH

	FREE(pBits);
	FREE(pBitmapInfo);

	return bResult;

}

COLORREF CUIImageManager::GetBitmapMaskColor(CBitmap& bmpIcons, CPoint pt)
{
	CUICompatibleDC dc(NULL, &bmpIcons);
	return dc.GetPixel(pt);
}

COLORREF CUIImageManager::GetBitmapMaskColor(UINT nIDBitmap, CPoint pt)
{
	CBitmap bmp;
	VERIFY(bmp.LoadBitmap(nIDBitmap));
	return GetBitmapMaskColor(bmp, pt);
}


HBITMAP CUIImageManager::ResampleAlphaLayer(UINT nIDBitmap, COLORREF clrMask)
{
	HBITMAP hbmAlpha = CUIImageManagerIcon::LoadAlphaBitmap(nIDBitmap);
	if (!hbmAlpha)
		return 0;

	CBitmap bmp;
	bmp.Attach(hbmAlpha);

	return CUIImageManager::ResampleAlphaLayer(bmp, clrMask);
}

HBITMAP CUIImageManager::ResampleAlphaLayer(HBITMAP bmpAlpha, COLORREF clrMask)
{
	CDC dcSrc;
	dcSrc.CreateCompatibleDC(NULL);

	PBYTE pBits = 0;
	PBITMAPINFO pBitmapInfo = 0;
	UINT nSize;

	if (!CUIImageManagerIcon::GetBitmapBits(dcSrc, bmpAlpha, pBitmapInfo, (LPVOID&)pBits, nSize))
		return 0;

	PBYTE pTarget = NULL;
	PBYTE pSource = pBits;
	pBitmapInfo->bmiHeader.biBitCount = 24;
	pBitmapInfo->bmiHeader.biSizeImage = 0;
	pBitmapInfo->bmiHeader.biCompression = BI_RGB;

	HBITMAP hBitmapResult = CreateDIBSection(dcSrc, pBitmapInfo, DIB_RGB_COLORS, (void**)&pTarget, NULL, 0);

	if (pTarget && pSource && hBitmapResult)
	{
		BYTE byMaskRed = GetRValue(clrMask);
		BYTE byMaskGreen = GetGValue(clrMask);
		BYTE byMaskBlue = GetBValue(clrMask);

		for (int i = 0; i < pBitmapInfo->bmiHeader.biHeight; i ++)
		{
			for (int j = 0; j < pBitmapInfo->bmiHeader.biWidth; j ++)
			{
				int iAlpha = pSource[3];

				if (iAlpha < 120) // Transparent
				{
					pTarget[0] = byMaskRed;
					pTarget[1] = byMaskGreen;
					pTarget[2] = byMaskBlue;
				}
				else if (iAlpha == 255) // Opaque
				{
					pTarget[0] = pSource[0];
					pTarget[1] = pSource[1];
					pTarget[2] = pSource[2];
				}
				else
				{
					pTarget[0] = (BYTE)((pSource[0] * (255 - iAlpha) + pSource[0] * iAlpha) >> 8);
					pTarget[1] = (BYTE)((pSource[1] * (255 - iAlpha) + pSource[1] * iAlpha) >> 8);
					pTarget[2] = (BYTE)((pSource[2] * (255 - iAlpha) + pSource[2] * iAlpha) >> 8);
				}

				pTarget += 3;
				pSource += 4;
			}

			if (pBitmapInfo->bmiHeader.biWidth & 1)
				pTarget++;
		}
	}

	FREE(pBits);
	FREE(pBitmapInfo);

	return hBitmapResult;
}

BOOL CUIImageManager::SetIcons(CBitmap& bmpIcons, UINT* pCommands, int nCount, CSize szIcon, UIImageState imageState, BOOL bAlpha)
{
	if (szIcon == CSize(0) && nCount > 0)
	{
		BITMAP bmpInfo;
		bmpIcons.GetBitmap(&bmpInfo);
		szIcon = CSize(bmpInfo.bmWidth / nCount, bmpInfo.bmHeight);
	}

	if (nCount == 0 && szIcon.cx > 0)
	{
		BITMAP bmpInfo;
		bmpIcons.GetBitmap(&bmpInfo);
		nCount = bmpInfo.bmWidth / szIcon.cx;
	}

	if (szIcon.cx == 0)
		return FALSE;

	if (bAlpha && !IsAlphaIconsSupported() && m_bAutoResample)
	{
		HBITMAP hBitmap = ResampleAlphaLayer(bmpIcons, m_clrMask == COLORREF(-1) ? RGB(0, 0xFF, 0) : m_clrMask);
		if (!hBitmap)
			return FALSE;

		CBitmap bmpResample;
		bmpResample.Attach(hBitmap);
		return SetIcons(bmpResample, pCommands, nCount, szIcon, imageState, FALSE);
	}

	if (bAlpha)
	{
		BITMAP bmpInfo;
		bmpIcons.GetBitmap(&bmpInfo);
		if (bmpInfo.bmWidth % szIcon.cx != 0)
			return FALSE;

		int nIconCount = bmpInfo.bmWidth / szIcon.cx;

		HBITMAP* pDestBitmap = new HBITMAP[nIconCount];

		if (!SplitBitmap(bmpIcons, nIconCount, pDestBitmap))
		{
			delete[] pDestBitmap;
			return FALSE;
		}
		BOOL bIgnoreNulls = (nIconCount == nCount);

		int j = 0;

		for (int i = 0; i < nCount; i++)
		{
			if ((!pCommands || bIgnoreNulls || (pCommands[i] != 0 && pCommands[i] != IMAGE_PLACEHOLDER)) && j < nIconCount)
			{

				SetIcon(pDestBitmap[j], pCommands ? pCommands[i]: i, szIcon, imageState);
				j++;
			}
		}

		for (j = 0; j < nIconCount; j++)
		{
			DeleteObject(pDestBitmap[j]);
		}

		delete[] pDestBitmap;


		return TRUE;

	}
	else
	{
		CImageList imgList;


		if (!imgList.Create(szIcon.cx, szIcon.cy, ILC_COLOR24 | ILC_MASK, 0, 1))
			return FALSE;

		COLORREF clrMask = m_clrMask != (COLORREF)-1 ? m_clrMask : GetBitmapMaskColor(bmpIcons);
		imgList.Add(&bmpIcons, clrMask);

		return SetIcons(imgList, pCommands, nCount, szIcon, imageState);
	}

}


BOOL CUIImageManager::SetIcons(UINT nIDResourceBitmap, UINT* pCommands, int nCount, CSize szIcon, UIImageState imageState)
{
	return SetIcons(MAKEINTRESOURCE(nIDResourceBitmap), pCommands, nCount, szIcon, imageState);
}

BOOL CUIImageManager::SetIcons(LPCTSTR lpszResourceBitmap, UINT* pCommands, int nCount, CSize szIcon, UIImageState imageState)
{
	BOOL bAlphaBitmap = FALSE;
	CBitmap bmpIcons;

	HBITMAP hBmp = CUIImageManagerIcon::LoadBitmapFromResource(lpszResourceBitmap, &bAlphaBitmap);

	if (!hBmp)
		return FALSE;

	bmpIcons.Attach(hBmp);

	return SetIcons(bmpIcons, pCommands, nCount, szIcon, imageState, bAlphaBitmap);
}


BOOL CUIImageManager::SetImageList(HIMAGELIST hImageList, int nBaseCommand, BOOL bDestoryImageList)
{
	if (!hImageList)
		return FALSE;

	CUIImageManagerImageList* pImageList = new CUIImageManagerImageList;
	pImageList->m_bDestroyImageList = bDestoryImageList;
	pImageList->m_hImageList = hImageList;
	pImageList->m_nBaseCommand = nBaseCommand;
	pImageList->m_pImageManager = this;

	m_arrImageList.Add(pImageList);
	return TRUE;
}

BOOL CUIImageManager::SetImageList(UINT nBitmap, int cx, int nBaseCommand, COLORREF clrMask)
{
	BOOL bAlpha = FALSE;
	HBITMAP hBitmap = CUIImageManagerIcon::LoadBitmapFromResource(MAKEINTRESOURCE(nBitmap), &bAlpha);
	if (!hBitmap)
		return FALSE;

	BITMAP bmpInfo;
	if (!::GetObject(hBitmap, sizeof(BITMAP), &bmpInfo))
		return FALSE;

	if (cx == 0) cx = bmpInfo.bmWidth;

	if (((bmpInfo.bmWidth % cx) != 0))
		return FALSE;

	HIMAGELIST hImageList = ImageList_Create(cx, bmpInfo.bmHeight, (bAlpha ? ILC_COLOR32 : ILC_COLOR24) |
		(clrMask == COLORREF_NULL ? 0 : ILC_MASK), 0, 1);

	if (!hImageList)
		return FALSE;

	if (bAlpha || (clrMask == COLORREF_NULL))
	{
		ImageList_Add(hImageList, hBitmap, NULL);
	}
	else
	{
		ImageList_AddMasked(hImageList, hBitmap, clrMask);
	}

	DeleteObject(hBitmap);

	return SetImageList(hImageList, nBaseCommand, TRUE);
}




BOOL CUIImageManager::IsPngBitmapResource(UINT nIDResourceBitmap)
{
	return CUIImageManagerIcon::IsPngBitmapResource(AfxFindResourceHandle(MAKEINTRESOURCE(nIDResourceBitmap), _T("PNG")),
		MAKEINTRESOURCE(nIDResourceBitmap));
}



BOOL CUIImageManager::IsAlphaBitmapResource(UINT nIDResourceBitmap)
{
	return CUIImageManagerIcon::IsAlphaBitmapResource(AfxFindResourceHandle(MAKEINTRESOURCE(nIDResourceBitmap), RT_BITMAP),
		MAKEINTRESOURCE(nIDResourceBitmap));
}

UINT CUIImageManager::AddCustomIcon(const CUIImageManagerIconHandle& hIcon)
{
	CSize szIcon = hIcon.GetExtent();
	m_nCustomID++;

	CUIImageManagerIconSet* pIconSet = new CUIImageManagerIconSet(m_nCustomID, this);
	pIconSet->SetIcon(hIcon, szIcon.cx);
	m_mapImages.SetAt(m_nCustomID, pIconSet);

	return m_nCustomID;
}

void CUIImageManager::CopyImage(UINT nCommand)
{
	CMemFile memFile;
	CArchive ar (&memFile, CArchive::store);

	Serialize(nCommand, ar);
	ar.Flush();

	DWORD dwCount = (DWORD)memFile.GetPosition();
	BYTE* pControls = memFile.Detach();

	HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwCount);
	if (hGlobal != NULL)
	{
		LPVOID lpData = GlobalLock(hGlobal);

		MEMCPY_S(lpData, pControls, dwCount);

		GlobalUnlock(hGlobal);
		SetClipboardData(m_nImageClipFormat, hGlobal);
	}

	ar.Close();
	memFile.Close();
	free(pControls);
}



UINT CUIImageManager::PasteCustomImage(COleDataObject& data)
{
#ifdef __AFXOLE_H__
	HGLOBAL hGlobal = data.GetGlobalData (m_nImageClipFormat);

	if (hGlobal == NULL) return 0;

	try
	{
		CSharedFile globFile;
		globFile.SetHandle(hGlobal, FALSE);
		CArchive ar (&globFile, CArchive::load);

		m_nCustomID++;
		Serialize(m_nCustomID, ar);

		ar.Close();

	}
	catch (COleException* pEx)
	{
		pEx->Delete ();
		return 0;
	}
	catch (CArchiveException* pEx)
	{
		pEx->Delete ();
		return 0;
	}
	return m_nCustomID;
#else
	return 0;
#endif
}

BOOL CUIImageManager::IsPrimaryImageExists(UINT nCommand, int nWidth) const
{
	CUIImageManagerIconSet* pIconSet = GetIconSet(nCommand);

	if (pIconSet)
	{
		CUIImageManagerIcon* pIcon = pIconSet->GetIcon(nWidth, FALSE);
		if (pIcon)
			return TRUE;
	}

	CUIImageManagerImageList* pImageList = GetImageList(nCommand);
	if (pImageList)
	{
		return nWidth == 0 || pImageList->GetIconSize().cx == nWidth;
	}

	return FALSE;
}

CUIImageManagerIcon* CUIImageManager::GetImage(UINT nCommand, int nWidth) const
{
	CUIImageManagerIconSet* pIconSet = GetIconSet(nCommand);

	if (pIconSet)
	{
		return pIconSet->GetIcon(nWidth);
	}

	CUIImageManagerImageList* pImageList = GetImageList(nCommand);
	if (pImageList)
	{
		return pImageList->GetIcon(nCommand);
	}

	return NULL;
}

BOOL CUIImageManager::Lookup(UINT nCommand) const
{
	return (GetIconSet(nCommand) != NULL) || (GetImageList(nCommand) != NULL);
}

CUIImageManagerIconSet* CUIImageManager::GetIconSet(UINT nCommand) const
{
	CUIImageManagerIconSet* pIconSet;
	if (m_mapImages.Lookup(nCommand, pIconSet))
	{
		return pIconSet;

	}
	return NULL;
}

CUIImageManagerImageList* CUIImageManager::GetImageList(UINT nCommand) const
{
	for (int i = 0; i < (int)m_arrImageList.GetSize(); i++)
	{
		CUIImageManagerImageList* pImageList = m_arrImageList[i];

		if (pImageList->Lookup(nCommand))
			return pImageList;
	}
	return NULL;
}

void CUIImageManager::RemoveIcon(UINT nIDCommand)
{
	CUIImageManagerIconSet* pIconSet;
	if (m_mapImages.Lookup(nIDCommand, pIconSet))
	{
		m_mapImages.RemoveKey(nIDCommand);
		pIconSet->RemoveAll();
		pIconSet->InternalRelease();
	}
}

void CUIImageManager::RemoveAll()
{
	POSITION pos = m_mapImages.GetStartPosition();
	UINT nID;
	CUIImageManagerIconSet* pIconSet;
	while (pos)
	{
		m_mapImages.GetNextAssoc(pos, nID, pIconSet);
		pIconSet->RemoveAll();
		pIconSet->InternalRelease();
	}
	m_mapImages.RemoveAll();

	pos = m_mapResources.GetStartPosition();
	DWORD dwSize;
	CUIImageManagerResource* pResource;
	while (pos)
	{
		m_mapResources.GetNextAssoc(pos, dwSize, pResource);
		delete pResource;
	}
	m_mapResources.RemoveAll();


	for (int i = 0; i < (int)m_arrImageList.GetSize(); i++)
	{
		delete m_arrImageList[i];
	}
	m_arrImageList.RemoveAll();

}

void CUIImageManager::RefreshAll()
{
	POSITION pos = m_mapImages.GetStartPosition();
	UINT nID;
	CUIImageManagerIconSet* pIconSet;

	while (pos)
	{
		m_mapImages.GetNextAssoc(pos, nID, pIconSet);
		pIconSet->RefreshAll();
	}

	for (int i = 0; i < (int)m_arrImageList.GetSize(); i++)
	{
		m_arrImageList[i]->RemoveAll();
	}
}

COLORREF CUIImageManager::SetMaskColor(COLORREF clrMask)
{
	COLORREF clrReturn = m_clrMask;
	m_clrMask = clrMask;
	return clrReturn;
}
COLORREF CUIImageManager::GetMaskColor() const
{
	return m_clrMask;
}

BOOL CUIImageManager::IsAlphaIconsSupported() const
{
	return TRUE;

}

BOOL CUIImageManager::IsAlphaIconsImageListSupported()
{
	DWORD dwComCtlVersion = UISystemVersion()->GetComCtlVersion();
	return HIWORD(dwComCtlVersion) > 5;
}

void CUIImageManager::DoPropExchange(UINT nCommand, CUIPropExchange* pPX)
{
#ifndef _UI_EXCLUDE_PROPEXCHANGE
	if (nCommand > m_nCustomID)
		m_nCustomID = nCommand;

	const LPCTSTR lpszCheck = _T("CUIImageManagerIcons");
	CString strCheck = lpszCheck;

	if (pPX->IsStoring())
	{
		PX_String(pPX, _T("CheckSum"), strCheck, lpszCheck);

#ifndef _UI_ACTIVEX
		ASSERT(nCommand > 0);
//this assertion kill VB6
#endif

		CUIImageManagerIconSet* pIconSet = GetIconSet(nCommand);

#ifndef _UI_ACTIVEX
		ASSERT(pIconSet);
//this assertion kill VB6
#endif

		if (!pIconSet)
		{
			CUIPropExchangeEnumeratorPtr pEnumerator(pPX->GetEnumerator(_T("Icon")));
			pEnumerator->GetPosition(0);
			return;
		}

		UINT nWidth;
		CUIImageManagerIcon* pIcon;
		DWORD dwCount =(DWORD)pIconSet->m_mapIcons.GetCount();

		CUIPropExchangeEnumeratorPtr pEnumerator(pPX->GetEnumerator(_T("Icon")));
		POSITION posEnum = pEnumerator->GetPosition(dwCount);

		POSITION pos = pIconSet->m_mapIcons.GetStartPosition();
		while (pos)
		{
			pIconSet->m_mapIcons.GetNextAssoc(pos, nWidth, pIcon);

			CUIPropExchangeSection secIcon(pEnumerator->GetNext(posEnum));

			PX_Long(&secIcon, _T("Width"), (long&)nWidth);
			PX_Serialize(&secIcon, _T("Data"), pIcon);
		}
	}
	else
	{
		PX_String(pPX, _T("CheckSum"), strCheck, lpszCheck);

		if (strCheck != lpszCheck)
			AfxThrowArchiveException(CArchiveException::badClass);


		CUIImageManagerIconSet* pIconSet;
		if (m_mapImages.Lookup(nCommand, pIconSet))
		{
			pIconSet->RemoveAll();
		}
		else
		{
			pIconSet = new CUIImageManagerIconSet(nCommand, this);
			m_mapImages.SetAt(nCommand, pIconSet);
		}

		CUIPropExchangeEnumeratorPtr pEnumerator(pPX->GetEnumerator(_T("Icon")));
		POSITION posEnum = pEnumerator->GetPosition();

		while (posEnum)
		{
			CUIPropExchangeSection secIcon(pEnumerator->GetNext(posEnum));

			UINT nWidth = 16;
			PX_Long(&secIcon, _T("Width"), (long&)nWidth);

			CUIImageManagerIcon* pIcon = new CUIImageManagerIcon(nCommand, nWidth, 15, pIconSet);
			PX_Serialize(&secIcon, _T("Data"), pIcon);

			pIconSet->m_mapIcons.SetAt(nWidth, pIcon);
		}
	}
#endif
}

void CUIImageManager::Serialize(UINT nCommand, CArchive& ar)
{
#ifndef _UI_EXCLUDE_PROPEXCHANGE
	CUIPropExchangeArchive px(ar);
	DoPropExchange(nCommand, &px);
#endif
}



void CUIImageManager::DoPropExchange(CUIPropExchange* pPX)
{
#ifndef _UI_EXCLUDE_PROPEXCHANGE
	CMap<UINT, UINT, CUIImageManagerIconSet*, CUIImageManagerIconSet*>* pIcons = GetImages();

	CUIPropExchangeEnumeratorPtr pEnumerator(pPX->GetEnumerator(_T("IconSet")));
	POSITION posEnum = pEnumerator->GetPosition((DWORD)pIcons->GetCount());

	if (pPX->IsStoring())
	{
		UINT nIDCommand;
		CUIImageManagerIconSet* pIconSet;

		POSITION pos = pIcons->GetStartPosition();

		while (pos)
		{
			pIcons->GetNextAssoc(pos, nIDCommand, pIconSet);
			CUIPropExchangeSection secIconSet(pEnumerator->GetNext(posEnum));

			PX_Long(&secIconSet, _T("Id"), (long&)nIDCommand);
			DoPropExchange(nIDCommand, &secIconSet);
		}
	}
	else
	{
		RemoveAll();

		while (posEnum)
		{
			CUIPropExchangeSection secIconSet(pEnumerator->GetNext(posEnum));

			UINT nIDCommand = 0;
			PX_Long(&secIconSet, _T("Id"), (long&)nIDCommand);

			DoPropExchange(nIDCommand, &secIconSet);
		}
	}
#endif
}
void CUIImageManager::Serialize(CArchive& ar)
{
#ifndef _UI_EXCLUDE_PROPEXCHANGE
	CUIPropExchangeArchive px(ar);
	DoPropExchange(&px);
#endif
}


void CUIImageManager::AddIcons(CUIImageManager* pImageManager)
{
	if (pImageManager == 0)
		return;

	POSITION pos = pImageManager->m_mapImages.GetStartPosition();
	UINT nID;
	CUIImageManagerIconSet* pIconSet;

	while (pos)
	{
		pImageManager->m_mapImages.GetNextAssoc(pos, nID, pIconSet);

		AddIcons(pIconSet);
	}
}

void CUIImageManager::AddIcons(CUIImageManagerIconSet* pIconSetAdd)
{
	int nId = pIconSetAdd->GetID();

	CUIImageManagerIconSet* pIconSet = NULL;

	if (!m_mapImages.Lookup(nId, pIconSet))
	{
		pIconSet = new CUIImageManagerIconSet(nId, this);

		m_mapImages.SetAt(nId, pIconSet);
	}

	POSITION pos = pIconSetAdd->m_mapIcons.GetStartPosition();
	UINT nWidth;
	CUIImageManagerIcon* pIconAdd;

	while (pos)
	{
		pIconSetAdd->m_mapIcons.GetNextAssoc(pos, nWidth, pIconAdd);

		CUIImageManagerIcon* pIcon = NULL;
		if (!pIconSet->m_mapIcons.Lookup(nWidth, pIcon))
		{
			pIcon  = new CUIImageManagerIcon(nId, nWidth, pIconAdd->GetHeight());
			pIconSet->m_mapIcons.SetAt(nWidth, pIcon);
		}

		pIcon->Copy(pIconAdd);
	}
}

BOOL CUIImageManager::DoDisableBitmap(HBITMAP hBmp, COLORREF clrDisabledLight, COLORREF clrDisabledDark,
		int nBlackAndWhiteContrast)
{
	ASSERT(nBlackAndWhiteContrast == -1 ||
		(nBlackAndWhiteContrast >= 0 && nBlackAndWhiteContrast <= 255));

	BOOL bOfficeDisabledIcons = clrDisabledLight != (COLORREF)-1 && clrDisabledDark != (COLORREF)-1;

	BITMAP bmBitmap;

	// Get information about the surfaces you were passed.
	if (!GetObject(hBmp,  sizeof(BITMAP), &bmBitmap))  return FALSE;

	// Make sure you have data that meets your requirements.
	if (bmBitmap.bmBitsPixel != 32)
		return FALSE;

	if (bmBitmap.bmPlanes != 1)
		return FALSE;

	if (!bmBitmap.bmBits)
		return FALSE;

	DWORD dwWidthBytes = bmBitmap.bmWidthBytes;

	// Initialize the surface pointers.
	RGBQUAD *lprgbBitmap = (RGBQUAD*)bmBitmap.bmBits;

	//************************************************
	int x, y;
	for (y = 0; y < bmBitmap.bmHeight; y++)
	{
		for (x = 0; x < bmBitmap.bmWidth; x++)
		{
			if (bOfficeDisabledIcons)
			{
				double dGray = (lprgbBitmap[x].rgbBlue * 0.114 + lprgbBitmap[x].rgbGreen * 0.587 + lprgbBitmap[x].rgbRed * 0.299)/255.0;
				double dLight = 1.0 - dGray;

				lprgbBitmap[x].rgbBlue  = BYTE(dLight * GetBValue(clrDisabledDark) + dGray * GetBValue(clrDisabledLight));
				lprgbBitmap[x].rgbGreen = BYTE(dLight * GetGValue(clrDisabledDark) + dGray * GetGValue(clrDisabledLight));
				lprgbBitmap[x].rgbRed   = BYTE(dLight * GetRValue(clrDisabledDark) + dGray * GetRValue(clrDisabledLight));
			}
			else if (nBlackAndWhiteContrast != -1)
			{
				int nGray = (lprgbBitmap[x].rgbBlue * 114 + lprgbBitmap[x].rgbGreen * 587 + lprgbBitmap[x].rgbRed * 299) / 1000;
				nGray = nGray + (255 - nGray) * nBlackAndWhiteContrast / 255;
				nGray = min(nGray, 255);

				lprgbBitmap[x].rgbBlue = BYTE(nGray);
				lprgbBitmap[x].rgbGreen = BYTE(nGray);
				lprgbBitmap[x].rgbRed = BYTE(nGray);

				lprgbBitmap[x].rgbReserved = BYTE(lprgbBitmap[x].rgbReserved / CUIImageManager::m_dDisabledAlphaFactor);
			}
			else
			{
				double dGray = (lprgbBitmap[x].rgbBlue * 0.114 + lprgbBitmap[x].rgbGreen * 0.587 + lprgbBitmap[x].rgbRed * 0.299)/255.0;

				lprgbBitmap[x].rgbBlue = (BYTE)(pow(dGray, CUIImageManager::m_dDisabledBrightnessFactor) * 255.0);
				lprgbBitmap[x].rgbGreen = lprgbBitmap[x].rgbBlue;
				lprgbBitmap[x].rgbRed = lprgbBitmap[x].rgbBlue;

				lprgbBitmap[x].rgbReserved = BYTE(lprgbBitmap[x].rgbReserved / CUIImageManager::m_dDisabledAlphaFactor);
			}
		}

		// Move to next scan line.
		lprgbBitmap = (RGBQUAD *)((LPBYTE)lprgbBitmap + dwWidthBytes);
	}

	return TRUE;
}

BOOL CUIImageManager::DisableBitmap(HDC hDC, const CRect& rcRect, COLORREF clrDisabledLight, COLORREF clrDisabledDark)
{
	HDC      hdcDst  = NULL;
	HBITMAP  hbmDst  = NULL;
	HBITMAP  hbmpOld = NULL;

	BOOL    bReturn = FALSE;
	// Create surfaces for new image.
	hbmDst  = CUIImageManager::Create32BPPDIBSection(hDC, rcRect.Width(), rcRect.Height());
	if (!hbmDst) goto HANDLEERROR;

	// Create HDCs to hold our surfaces.
	hdcDst  = CreateCompatibleDC(hDC);
	if (!hdcDst) goto HANDLEERROR;

	// Prepare the surfaces for drawing.
	hbmpOld = (HBITMAP)SelectObject(hdcDst,  hbmDst);

	// Capture a copy of the source area.
	if (!BitBlt(hdcDst, 0, 0, rcRect.Width(), rcRect.Height(),
		hDC, rcRect.left, rcRect.top, SRCCOPY) )
		goto HANDLEERROR;

	// Modify sourcepixels to create the destination image.
	bReturn = DoDisableBitmap(hbmDst, clrDisabledLight, clrDisabledDark);

	// Display the disabled image to the target HDC.
	if (bReturn)
	{
		BitBlt(hDC, rcRect.left, rcRect.top, rcRect.Width(), rcRect.Height(),
			hdcDst, 0,0, SRCCOPY);
	}

HANDLEERROR:

	// Clean up the rest of the objects you created.
	if (hbmpOld) SelectObject(hdcDst, hbmpOld);
	if (hdcDst) DeleteDC(hdcDst);
	if (hbmDst) DeleteObject(hbmDst);

	return bReturn;
}

BOOL CUIImageManager::BlackWhiteBitmap(HDC hDC, const CRect& rcRect, int nBlackAndWhiteContrast)
{
	HDC      hdcDst  = NULL;
	HBITMAP  hbmDst  = NULL;
	HBITMAP  hbmpOld = NULL;

	BOOL    bReturn = FALSE;
	// Create surfaces for new image.
	hbmDst  = CUIImageManager::Create32BPPDIBSection(hDC, rcRect.Width(), rcRect.Height());
	if (!hbmDst) goto HANDLEERROR;

	// Create HDCs to hold our surfaces.
	hdcDst  = CreateCompatibleDC(hDC);
	if (!hdcDst) goto HANDLEERROR;

	// Prepare the surfaces for drawing.
	hbmpOld = (HBITMAP)SelectObject(hdcDst,  hbmDst);

	// Capture a copy of the source area.
	if (!BitBlt(hdcDst, 0, 0, rcRect.Width(), rcRect.Height(),
		hDC, rcRect.left, rcRect.top, SRCCOPY) )
		goto HANDLEERROR;

	// Modify sourcepixels to create the destination image.
	bReturn = DoDisableBitmap(hbmDst, (COLORREF)-1, (COLORREF)-1, nBlackAndWhiteContrast);

	// Display the disabled image to the target HDC.
	if (bReturn)
	{
		BitBlt(hDC, rcRect.left, rcRect.top, rcRect.Width(), rcRect.Height(),
			hdcDst, 0,0, SRCCOPY);
	}

HANDLEERROR:

	// Clean up the rest of the objects you created.
	if (hbmpOld) SelectObject(hdcDst, hbmpOld);
	if (hdcDst) DeleteDC(hdcDst);
	if (hbmDst) DeleteObject(hbmDst);

	return bReturn;
}


