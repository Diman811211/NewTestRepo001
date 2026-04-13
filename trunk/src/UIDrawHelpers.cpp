//////////////////////////////////////////////////////////////////////////
//
//
//	UIDrawHelpers.cpp: implementation of the CUIDrawHelpers class.
//
//
/////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "UISystemHelpers.h"
#include "UIColorManager.h"
#include "UIDrawHelpers.h"

#include "UIResourceManager.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

HHOOK CUIMouseMonitor::m_hHookMouse = 0;
CWnd* CUIMouseMonitor::m_pWndMonitor = 0;

typedef BOOL (WINAPI *PFNSETLAYEREDWINDOWATTRIBUTES) (HWND hwnd, COLORREF crKey, BYTE bAlpha, DWORD dwFlags);

#ifndef LWA_ALPHA
#define LWA_ALPHA               0x00000002
#endif

#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED           0x00080000
#endif


//////////////////////////////////////////////////////////////////////////
// CUIMouseMonitor
//////////////////////////////////////////////////////////////////////////

void CUIMouseMonitor::SetupHook(CWnd* pWndMonitor)
{
	if (pWndMonitor && m_hHookMouse == 0)
	{
		m_hHookMouse = SetWindowsHookEx(WH_MOUSE, MouseProc, 0, GetCurrentThreadId ());
	}
	if (!pWndMonitor && m_hHookMouse)
	{
		UnhookWindowsHookEx(m_hHookMouse);
		m_hHookMouse = 0;
	}
	m_pWndMonitor = pWndMonitor;
}

BOOL CUIMouseMonitor::IsMouseHooked()
{
	return m_pWndMonitor != NULL;
}

LRESULT CALLBACK CUIMouseMonitor::MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode != HC_ACTION || !m_pWndMonitor)
		return CallNextHookEx(m_hHookMouse, nCode, wParam, lParam);

	CUIWindowRect rc(m_pWndMonitor);

	if (!rc.PtInRect(((PMOUSEHOOKSTRUCT)lParam)->pt))
	{
		switch (wParam)
		{
		case WM_LBUTTONDOWN:
		case WM_NCLBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_NCRBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_NCMBUTTONDOWN:
			m_pWndMonitor->GetOwner()->SetFocus();
			return TRUE;
		}
	}

	return CallNextHookEx(m_hHookMouse, nCode, wParam, lParam);
}


//===========================================================================
// CUIClientCursorPos class
//===========================================================================

CUITransparentBitmap::CUITransparentBitmap(HBITMAP hBitmap)
: m_hBitmap(hBitmap)
{
}

// Not foolproof, but works 99% of the time :).  Assumes the top
// left pixel is the transparent color.

COLORREF CUITransparentBitmap::GetTransparentColor() const
{
	CBitmap* pBitmap = CBitmap::FromHandle(m_hBitmap);
	if (pBitmap != NULL)
	{
		CUICompatibleDC dc(NULL, pBitmap);
		return dc.GetPixel(0, 0);
	}
	return (COLORREF)-1;
}


HICON CUITransparentBitmap::ConvertToIcon() const
{
	if (m_hBitmap == NULL)
		return NULL;

	COLORREF crTransparent = GetTransparentColor();

	BITMAP bmp;
	if (!::GetObject(m_hBitmap, sizeof(BITMAP), &bmp))
		return NULL;

	if (bmp.bmHeight == 0 || bmp.bmWidth == 0)
		return NULL;

	CImageList il;
	il.Create(bmp.bmWidth, bmp.bmHeight, ILC_COLOR24 | ILC_MASK, 0, 1);
	il.Add(CBitmap::FromHandle(m_hBitmap), crTransparent);

	ASSERT(il.GetImageCount() == 1);

	return il.ExtractIcon(0);
}

//===========================================================================
// CUIClientCursorPos class
//===========================================================================

CUIClientCursorPos::CUIClientCursorPos(CWnd* pWnd)
{
	GetCursorPos(this);
	pWnd->ScreenToClient(this);
}

//===========================================================================
// CUIEmptySize class
//===========================================================================

CUIEmptySize::CUIEmptySize()
{
	SetSizeEmpty();
}

void CUIEmptySize::SetSizeEmpty()
{
	cx = 0;
	cy = 0;
}

const SIZE& CUIEmptySize::operator=(const SIZE& srcSize)
{
	cx = srcSize.cx;
	cy = srcSize.cy;
	return *this;
}

//===========================================================================
// CUIEmptyRect class
//===========================================================================

CUIEmptyRect::CUIEmptyRect()
{
	SetRectEmpty();
}

//===========================================================================
// CUIWindowRect class
//===========================================================================

CUIWindowRect::CUIWindowRect(HWND hWnd)
{
	if (::IsWindow(hWnd))
		::GetWindowRect(hWnd, this);
	else
		SetRectEmpty();
}

CUIWindowRect::CUIWindowRect(const CWnd* pWnd)
{
	if (::IsWindow(pWnd->GetSafeHwnd()))
		::GetWindowRect(pWnd->GetSafeHwnd(), this);
	else
		SetRectEmpty();
}

//===========================================================================
// CUIClientRect class
//===========================================================================

CUIClientRect::CUIClientRect(HWND hWnd)
{
	if (::IsWindow(hWnd))
		::GetClientRect(hWnd, this);
	else
		SetRectEmpty();
}

CUIClientRect::CUIClientRect(const CWnd* pWnd)
{
	if (::IsWindow(pWnd->GetSafeHwnd()))
		::GetClientRect(pWnd->GetSafeHwnd(), this);
	else
		SetRectEmpty();
}

//===========================================================================
// CUIBufferDC class
//===========================================================================

CUIBufferDC::CUIBufferDC(HDC hDestDC, const CRect& rcPaint)
	: m_hDestDC (hDestDC)
{
	m_rect = rcPaint;
	Attach (::CreateCompatibleDC (m_hDestDC));
	if (!m_hDC)
		return;

	m_bitmap.Attach (::CreateCompatibleBitmap(
		m_hDestDC, m_rect.right, m_rect.bottom));
	m_hOldBitmap = ::SelectObject (m_hDC, m_bitmap);
}

CUIBufferDC::CUIBufferDC(HDC hDestDC, const CRect& rcPaint, const CUIPaintManagerColorGradient& clrBack, const BOOL bHorz /*=FALSE*/)
	: m_hDestDC (hDestDC)
{
	m_rect = rcPaint;
	Attach (::CreateCompatibleDC (m_hDestDC));
	if (!m_hDC)
		return;

	m_bitmap.Attach (::CreateCompatibleBitmap(
		m_hDestDC, m_rect.right, m_rect.bottom));
	m_hOldBitmap = ::SelectObject (m_hDC, m_bitmap);

	if (!clrBack.IsNull())
	{
		UIDrawHelpers()->GradientFill(this, m_rect, clrBack, bHorz);
	}
}

CUIBufferDC::CUIBufferDC(CPaintDC& paintDC)
{
	m_hDestDC = paintDC.GetSafeHdc();
	m_rect = paintDC.m_ps.rcPaint;

	Attach (::CreateCompatibleDC (m_hDestDC));
	if (!m_hDC)
		return;

	m_bitmap.Attach (::CreateCompatibleBitmap(
		m_hDestDC, max(1, m_rect.right), max(1, m_rect.bottom)));
	m_hOldBitmap = ::SelectObject (m_hDC, m_bitmap);

	CRgn rgn;
	rgn.CreateRectRgnIndirect(&m_rect);

	SelectClipRgn(&rgn);
}


CUIBufferDC::~CUIBufferDC()
{
	if (!m_hDC)
		return;

	if (m_hDestDC)
	{
		::BitBlt (m_hDestDC, m_rect.left, m_rect.top, m_rect.Width(),
			m_rect.Height(), m_hDC, m_rect.left, m_rect.top, SRCCOPY);
	}
	::SelectObject (m_hDC, m_hOldBitmap);
}
void CUIBufferDC::Discard()
{
	m_hDestDC = 0;
}

CDC* CUIBufferDC::GetDestDC()
{
	return CDC::FromHandle(m_hDestDC);
}

void CUIBufferDC::TakeSnapshot()
{
	::BitBlt (m_hDC, m_rect.left, m_rect.top, m_rect.Width(),
		m_rect.Height(), m_hDestDC, m_rect.left, m_rect.top, SRCCOPY);
}

//===========================================================================
// CUIBufferDC class
//===========================================================================

CUIBufferDCEx::CUIBufferDCEx(HDC hDestDC, const CRect rcPaint) : m_hDestDC (hDestDC)
{
	m_rect = rcPaint;
	Attach (::CreateCompatibleDC (m_hDestDC));
	m_bitmap = ::CreateCompatibleBitmap(
		m_hDestDC, m_rect.Width(), m_rect.Height());

	m_hOldBitmap = ::SelectObject (m_hDC, m_bitmap);

	SetViewportOrg(-rcPaint.left, -rcPaint.top);
}

CUIBufferDCEx::~CUIBufferDCEx()
{
	SetViewportOrg(0, 0);

	::BitBlt (m_hDestDC, m_rect.left, m_rect.top, m_rect.Width(),
		m_rect.Height(), m_hDC, 0, 0, SRCCOPY);
	::SelectObject (m_hDC, m_hOldBitmap);
	::DeleteObject(m_bitmap);
}

//===========================================================================
// CUIBitmapDC class
//===========================================================================

CUIBitmapDC::CUIBitmapDC()
	: m_hOldBitmap(NULL)
{
}

CUIBitmapDC::CUIBitmapDC(CDC *pDC, CBitmap *pBitmap)
{
	m_hDC = pDC->GetSafeHdc();
	SetBitmap(HBITMAP(pBitmap->GetSafeHandle()));
}


CUIBitmapDC::CUIBitmapDC(CDC* pDC, HBITMAP hBitmap)
{
	m_hDC = pDC->GetSafeHdc();
	SetBitmap(hBitmap);
}

CUIBitmapDC::~CUIBitmapDC()
{
	SelectOld();
}

void CUIBitmapDC::SetBitmap(HBITMAP hBitmap)
{
	SelectOld();
	m_hOldBitmap = HBITMAP(::SelectObject(m_hDC, hBitmap));
}

void CUIBitmapDC::SelectOld()
{
	if (NULL != m_hOldBitmap)
	{
		::SelectObject(GetSafeHdc(), m_hOldBitmap);
	}
}

//===========================================================================
// CUIFontDC class
//===========================================================================

CUIFontDC::CUIFontDC(CDC* pDC, CFont* pFont)
{
	ASSERT(pDC);

	m_pDC = pDC;
	m_pOldFont = NULL;
	m_clrOldTextColor = COLORREF_NULL;

	if (pFont)
	{
		SetFont(pFont);
	}
}

CUIFontDC::CUIFontDC(CDC* pDC, CFont* pFont, COLORREF clrTextColor)
{
	ASSERT(pDC);
	ASSERT(clrTextColor != COLORREF_NULL);

	m_pDC = pDC;
	m_pOldFont = NULL;
	m_clrOldTextColor = COLORREF_NULL;


	if (pFont)
	{
		SetFont(pFont);
	}

	SetColor(clrTextColor);
}

CUIFontDC::~CUIFontDC()
{
	ReleaseFont();
	ReleaseColor();
}

void CUIFontDC::SetFont(CFont* pFont)
{
	if (m_pDC && pFont)
	{
		CFont* pFontPrev = m_pDC->SelectObject(pFont);

		if (!m_pOldFont && pFontPrev)
		{
			m_pOldFont = pFontPrev;
		}
	}
}

void CUIFontDC::SetColor(COLORREF clrTextColor)
{
	ASSERT(clrTextColor != COLORREF_NULL);
	ASSERT(m_pDC);

	if (m_pDC && clrTextColor != COLORREF_NULL)
	{
		COLORREF clrTextColorPrev= m_pDC->SetTextColor(clrTextColor);

		if (m_clrOldTextColor == COLORREF_NULL)
		{
			m_clrOldTextColor = clrTextColorPrev;
		}
	}
}

void CUIFontDC::SetFontColor(CFont* pFont, COLORREF clrTextColor)
{
	SetFont(pFont);
	SetColor(clrTextColor);
}

void CUIFontDC::ReleaseFont()
{
	ASSERT(m_pDC);
	if (m_pDC && m_pOldFont)
	{
		m_pDC->SelectObject(m_pOldFont);
		m_pOldFont = NULL;
	}
}

void CUIFontDC::ReleaseColor()
{
	ASSERT(m_pDC);
	if (m_pDC && m_clrOldTextColor != COLORREF_NULL)
	{
		m_pDC->SetTextColor(m_clrOldTextColor);
		m_clrOldTextColor = COLORREF_NULL;
	}
}

//===========================================================================
// CUIPenDC class
//===========================================================================

CUIPenDC::CUIPenDC(CDC* pDC, CPen* pPen)
: m_hDC(pDC->GetSafeHdc())
{
	m_hOldPen = (HPEN)::SelectObject(m_hDC, pPen->GetSafeHandle());
}

CUIPenDC::CUIPenDC(HDC hDC, COLORREF crColor)
: m_hDC (hDC)
{
	VERIFY(m_pen.CreatePen (PS_SOLID, 1, crColor));
	m_hOldPen = (HPEN)::SelectObject (m_hDC, m_pen);
}

CUIPenDC::~CUIPenDC ()
{
	::SelectObject (m_hDC, m_hOldPen);
}

void CUIPenDC::Color(COLORREF crColor)
{
	::SelectObject (m_hDC, m_hOldPen);
	VERIFY(m_pen.DeleteObject());
	VERIFY(m_pen.CreatePen (PS_SOLID, 1, crColor));
	m_hOldPen = (HPEN)::SelectObject (m_hDC, m_pen);
}

COLORREF CUIPenDC::Color()
{
	LOGPEN logPen;
	m_pen.GetLogPen(&logPen);
	return logPen.lopnColor;
}

//===========================================================================
// CUIBrushDC class
//===========================================================================

CUIBrushDC::CUIBrushDC(HDC hDC, COLORREF crColor)
: m_hDC (hDC)
{
	VERIFY(m_brush.CreateSolidBrush (crColor));
	m_hOldBrush = (HBRUSH)::SelectObject (m_hDC, m_brush);
}

CUIBrushDC::~CUIBrushDC()
{
	::SelectObject(m_hDC, m_hOldBrush);
}

void CUIBrushDC::Color(COLORREF crColor)
{
	::SelectObject(m_hDC, m_hOldBrush);
	VERIFY(m_brush.DeleteObject());
	VERIFY(m_brush.CreateSolidBrush(crColor));
	m_hOldBrush = (HBRUSH)::SelectObject (m_hDC, m_brush);
}

//===========================================================================
// CUICompatibleDC class
//===========================================================================

CUICompatibleDC::CUICompatibleDC(CDC* pDC, CBitmap* pBitmap)
{
	CreateCompatibleDC(pDC);
	m_hOldBitmap = (HBITMAP)::SelectObject(GetSafeHdc(), pBitmap->GetSafeHandle());
}

CUICompatibleDC::CUICompatibleDC(CDC* pDC, HBITMAP hBitmap)
{
	CreateCompatibleDC(pDC);
	m_hOldBitmap = (HBITMAP)::SelectObject(GetSafeHdc(), hBitmap);
}

CUICompatibleDC::~CUICompatibleDC()
{
	::SelectObject(GetSafeHdc(), m_hOldBitmap);
	DeleteDC();
}



//===========================================================================
// CUISplitterTracker class
//===========================================================================
CUISplitterTracker::CUISplitterTracker(BOOL bSolid /*= FALSE*/, BOOL bDesktopDC /*= TRUE*/)
{
	m_bSolid = bSolid;
	m_rcBoundRect.SetRectEmpty();
	m_pDC = 0;
	m_bDesktopDC = bDesktopDC;
	m_pWnd = NULL;
	m_pSplitterWnd = NULL;

	m_pfnSetLayeredWindowAttributes = NULL;

	HMODULE hLib = GetModuleHandle(_T("USER32"));
	if (hLib)
	{
		m_pfnSetLayeredWindowAttributes = (PVOID) ::GetProcAddress(hLib, "SetLayeredWindowAttributes");
	}
}


void CUISplitterTracker::OnInvertTracker(CRect rect)
{
	ASSERT(!rect.IsRectEmpty());

	if (!m_bDesktopDC)
	{
		m_pWnd->ScreenToClient(rect);
	}

	if (m_pSplitterWnd)
	{
		m_pSplitterWnd->SetWindowPos(0, rect.left, rect.top, rect.Width(), rect.Height(),
			SWP_SHOWWINDOW | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
		return;
	}

	if (!m_pDC)
		return;

	if (m_bSolid)
	{
		m_pDC->InvertRect(rect);
	}
	else
	{
		CBrush* pDitherBrush = CDC::GetHalftoneBrush();
		CBrush* pBrush = (CBrush*)m_pDC->SelectObject(pDitherBrush);

		m_pDC->PatBlt(rect.left, rect.top, rect.Width(), rect.Height(), PATINVERT);
		m_pDC->SelectObject(pBrush);
	}
}

BOOL CUISplitterTracker::Track(CWnd* pTrackWnd, CRect rcAvail, CRect& rectTracker, CPoint point, BOOL bHoriz)
{
	pTrackWnd->SetCapture();
	m_pDC = 0;
	m_pSplitterWnd = NULL;

	if (m_rcBoundRect.IsRectEmpty() && m_bDesktopDC && UISystemVersion()->IsWinVistaOrGreater() &&
		m_pfnSetLayeredWindowAttributes && !UIColorManager()->IsLowResolution())
	{
		m_pSplitterWnd = new CWnd();
		m_pSplitterWnd->CreateEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
			AfxRegisterWndClass(0, AfxGetApp()->LoadStandardCursor(IDC_ARROW), (HBRUSH)GetStockObject(BLACK_BRUSH)), 0, WS_POPUP, CRect(0, 0, 0, 0), NULL, 0);

		((PFNSETLAYEREDWINDOWATTRIBUTES)m_pfnSetLayeredWindowAttributes)(m_pSplitterWnd->m_hWnd, 0, 100, LWA_ALPHA);
	}
	else
	{
		if (m_bDesktopDC)
			m_pWnd = CWnd::GetDesktopWindow();
		else
			m_pWnd = pTrackWnd;

		if (m_pWnd->LockWindowUpdate())
			m_pDC = m_pWnd->GetDCEx(NULL, DCX_WINDOW | DCX_CACHE | DCX_LOCKWINDOWUPDATE);
		else
			m_pDC = m_pWnd->GetDCEx(NULL, DCX_WINDOW | DCX_CACHE);
		ASSERT(m_pDC != NULL);
	}

	CPoint ptOffset = bHoriz ? CPoint(rectTracker.left - point.x, 0) :
		CPoint(0, rectTracker.top - point.y);

	OnInvertTracker(rectTracker);

	if (!m_rcBoundRect.IsRectEmpty())
		OnInvertTracker(m_rcBoundRect);

	BOOL bAccept = FALSE;
	while (CWnd::GetCapture() == pTrackWnd)
	{
		MSG msg;
		if (!GetMessage(&msg, NULL, 0, 0))
			break;

		if (msg.message == WM_MOUSEMOVE)
		{
			point = CPoint(msg.lParam);
			pTrackWnd->ClientToScreen(&point);
			point += ptOffset;

			point.x = max(min(point.x, rcAvail.right), rcAvail.left);
			point.y = max(min(point.y, rcAvail.bottom), rcAvail.top);

			if (bHoriz)
			{
				if (rectTracker.left != point.x)
				{
					OnInvertTracker(rectTracker);
					rectTracker.OffsetRect(point.x - rectTracker.left, 0);
					OnInvertTracker(rectTracker);
				}

			}
			else
			{
				if (rectTracker.top != point.y)
				{
					OnInvertTracker(rectTracker);
					rectTracker.OffsetRect(0, point.y - rectTracker.top);
					OnInvertTracker(rectTracker);
				}
			}
		}
		else if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) break;
		else if (msg.message == WM_LBUTTONUP)
		{
			bAccept = TRUE;
			break;
		}
		else  ::DispatchMessage(&msg);
	}

	if (!m_rcBoundRect.IsRectEmpty())
		OnInvertTracker(m_rcBoundRect);

	OnInvertTracker(rectTracker);

	if (CWnd::GetCapture() == pTrackWnd) ReleaseCapture();

	if (m_pSplitterWnd)
	{
		m_pSplitterWnd->DestroyWindow();

		delete m_pSplitterWnd;
		m_pSplitterWnd = NULL;
	}
	else
	{
		m_pWnd->UnlockWindowUpdate();
		if (m_pDC != NULL)
		{
			m_pWnd->ReleaseDC(m_pDC);
			m_pDC = NULL;
		}
	}

	return bAccept;
}

//===========================================================================
// CUIDrawHelpers class
//===========================================================================

CUIDrawHelpers::CUIDrawHelpers()
{
	m_pfnFastGradientFill = 0;
	m_pfnAlphaBlend = 0;
	m_pfnTransparentBlt = 0;

	// Don't use CUIModuleHandle to reduce dependence between common source
	m_hMsImgDll = ::LoadLibrary(_T("msimg32.dll"));

	if (m_hMsImgDll)
	{
		m_pfnFastGradientFill = (PFNGRADIENTFILL)GetProcAddress(m_hMsImgDll, "GradientFill");
		m_pfnAlphaBlend = (PFNALPHABLEND)::GetProcAddress(m_hMsImgDll, "AlphaBlend");
		m_pfnTransparentBlt = (PFNTRANSPARENTBLT)::GetProcAddress(m_hMsImgDll, "TransparentBlt");
	}
}

CUIDrawHelpers* AFX_CDECL UIDrawHelpers()
{
	static CUIDrawHelpers s_instance; // singleton
	return &s_instance;
}

CUIDrawHelpers::~CUIDrawHelpers()
{
	if (m_hMsImgDll != NULL)
	{
		//::FreeLibrary(m_hMsImgDll); Dangerous to call FreeLibrary in destructor of static object.
	}
}

void CUIDrawHelpers::SolidRectangle(CDC *pDC, CRect rc, COLORREF clrRect, COLORREF clrFill)
{
	pDC->Draw3dRect(rc, clrRect, clrRect);
	rc.DeflateRect(1, 1, 1, 1);
	pDC->FillSolidRect(rc, clrFill);
}

BOOL CUIDrawHelpers::GradientFill(HDC hdc, PTRIVERTEX pVertex, ULONG dwNumVertex, PVOID pMesh, ULONG dwNumMesh, ULONG dwMode)
{
	if (m_pfnFastGradientFill)
	{
		return (*m_pfnFastGradientFill)(hdc, pVertex, dwNumVertex, pMesh, dwNumMesh, dwMode);
	}

	return FALSE;
}

void CUIDrawHelpers::GradientFillSlow(CDC* pDC, LPCRECT lpRect, COLORREF crFrom, COLORREF crTo, BOOL bHorz)
{
	int cx = max(1, lpRect->right - lpRect->left);
	int cy = max(1, lpRect->bottom - lpRect->top);

	CRect rc;
	pDC->GetClipBox(&rc);

	if (rc.IsRectEmpty())
		rc = *lpRect;
	else
		rc.IntersectRect(rc, lpRect);

	if (bHorz)
	{
		for (int nX = rc.left; nX < rc.right; nX++)
		{
			pDC->FillSolidRect(nX, rc.top, 1, rc.Height(), BlendColors(
				crFrom, crTo, (double)(1.0 - ((nX - lpRect->left) / (double)cx))));
		}
	}
	else
	{
		for (int nY = rc.top; nY < rc.bottom; nY++)
		{
			pDC->FillSolidRect(rc.left, nY, rc.Width(), 1, BlendColors(
				crFrom, crTo, (double)(1.0 - ((nY - lpRect->top)) / (double)cy)));
		}
	}
}

void CUIDrawHelpers::GradientFillFast(CDC* pDC, LPCRECT lpRect, COLORREF crFrom, COLORREF crTo, BOOL bHorz)
{
	TRIVERTEX vert[2];
	vert[0].x = lpRect->left;
	vert[0].y = lpRect->top;
	vert[0].Red = (COLOR16)(GetRValue(crFrom) << 8);
	vert[0].Green = (COLOR16)(GetGValue(crFrom) << 8);
	vert[0].Blue = (COLOR16)(GetBValue(crFrom) << 8);
	vert[0].Alpha = 0x0000;

	vert[1].x = lpRect->right;
	vert[1].y = lpRect->bottom;
	vert[1].Red = (COLOR16)(GetRValue(crTo) << 8);
	vert[1].Green = (COLOR16)(GetGValue(crTo) << 8);
	vert[1].Blue = (COLOR16)(GetBValue(crTo) << 8);
	vert[1].Alpha = 0x0000;

	GRADIENT_RECT gRect = { 0, 1 };

	GradientFill(*pDC, vert, 2, &gRect, 1, bHorz ? GRADIENT_FILL_RECT_H : GRADIENT_FILL_RECT_V);
}

void CUIDrawHelpers::GradientFill(CDC* pDC, LPCRECT lpRect, COLORREF crFrom, COLORREF crTo, BOOL bHorz)
{
	if (!lpRect)
		return;

	if (::IsRectEmpty(lpRect))
		return;

	if (IsLowResolution(pDC->GetSafeHdc()))
	{
		pDC->FillSolidRect(lpRect, crFrom);
	}
	else if (crFrom == crTo)
	{
		pDC->FillSolidRect(lpRect, crFrom);
	}
	else if ((m_pfnFastGradientFill == NULL) || (IsContextRTL(pDC) && UISystemVersion()->IsWin9x()))
	{
		GradientFillSlow(pDC, lpRect, crFrom, crTo, bHorz);
	}
	else
	{
		GradientFillFast(pDC, lpRect, crFrom, crTo, bHorz);
	}
}

void CUIDrawHelpers::GradientFill(CDC* pDC, LPCRECT lpRect, COLORREF crFrom, COLORREF crTo, BOOL bHorz, LPCRECT lpRectClip)
{
	CRect rc(lpRect);

	if (lpRectClip == NULL)
	{
		GradientFill(pDC, lpRect, crFrom, crTo, bHorz);
		return;
	}

	COLORREF crFrom1 = crFrom;

	if (bHorz)
	{
		if (rc.top < lpRectClip->top)
		{
			rc.top = lpRectClip->top;
		}

		if (rc.bottom > lpRectClip->bottom)
		{
			rc.bottom = lpRectClip->bottom;
		}

		if ((rc.left > lpRectClip->right) || (rc.right < lpRectClip->left))
			return;

		if (rc.left < lpRectClip->left)
		{
			rc.left = lpRectClip->left;

			crFrom = BlendColors(crFrom, crTo,
				(float)(lpRect->right - lpRectClip->left) / (float)(lpRect->right - lpRect->left));
		}

		if (rc.right > lpRectClip->right)
		{
			rc.right = lpRectClip->right;

			crTo = BlendColors(crFrom1, crTo,
				(float)(lpRect->right - lpRectClip->right) / (float)(lpRect->right - lpRect->left));

		}

		GradientFill(pDC, rc, crFrom, crTo, bHorz);

	}
	else
	{
		if (rc.left < lpRectClip->left)
		{
			rc.left = lpRectClip->left;
		}

		if (rc.right > lpRectClip->right)
		{
			rc.right = lpRectClip->right;
		}

		if ((rc.top > lpRectClip->bottom) || (rc.bottom < lpRectClip->top))
			return;

		if (rc.top < lpRectClip->top)
		{
			rc.top = lpRectClip->top;

			crFrom = BlendColors(crFrom, crTo,
				(float)(lpRect->bottom - lpRectClip->top) / (float)(lpRect->bottom - lpRect->top));
		}

		if (rc.bottom > lpRectClip->bottom)
		{
			rc.bottom = lpRectClip->bottom;

			crTo = BlendColors(crFrom1, crTo,
				(float)(lpRect->bottom - lpRectClip->bottom) / (float)(lpRect->bottom - lpRect->top));

		}

		GradientFill(pDC, rc, crFrom, crTo, bHorz);
	}
}

void CUIDrawHelpers::GradientFill(CDC* pDC, LPCRECT lpRect, const CUIPaintManagerColorGradient& grc, BOOL bHorz, LPCRECT lpRectClip)
{
	// using gradient factor color gradient fill
	if (grc.fGradientFactor != 0.5f)
	{
		COLORREF clrMid = BlendColors(grc.clrLight, grc.clrDark, grc.fGradientFactor);

		if (bHorz)
		{
			CRect rcLeft(lpRect);
			rcLeft.right -= rcLeft.Width()/2;
			GradientFill(pDC, &rcLeft, grc.clrLight, clrMid, bHorz, lpRectClip);

			CRect rcRight(lpRect);
			rcRight.left = rcLeft.right;
			GradientFill(pDC, &rcRight, clrMid, grc.clrDark, bHorz, lpRectClip);
		}
		else
		{
			CRect rcTop(lpRect);
			rcTop.bottom -= rcTop.Height()/2;
			GradientFill(pDC, &rcTop, grc.clrLight, clrMid, bHorz, lpRectClip);

			CRect rcBottom(lpRect);
			rcBottom.top = rcTop.bottom;
			GradientFill(pDC, &rcBottom, clrMid, grc.clrDark, bHorz, lpRectClip);
		}
	}
	// using 2 color gradient fill
	else
	{
		GradientFill(pDC, lpRect, grc.clrLight, grc.clrDark, bHorz, lpRectClip);
	}
}

void CUIDrawHelpers::ExcludeCorners(CDC* pDC, CRect rc, BOOL bTopCornersOnly /*= FALSE*/)
{
	pDC->ExcludeClipRect(rc.left, rc.top, rc.left + 1, rc.top + 1);
	pDC->ExcludeClipRect(rc.right - 1, rc.top, rc.right, rc.top + 1);

	if (bTopCornersOnly == FALSE)
	{
		pDC->ExcludeClipRect(rc.left, rc.bottom - 1, rc.left + 1, rc.bottom);
		pDC->ExcludeClipRect(rc.right - 1, rc.bottom - 1, rc.right, rc.bottom);
	}
}

void CUIDrawHelpers::StripMnemonics(CString& strClear)
{
	for (int i = 0; i < strClear.GetLength(); i++)
	{
		if (strClear[i] == _T('&')) // Converts "&&" to "&" and "&&&&" to "&&"
		{
			strClear.Delete(i);
		}
	}
}

void CUIDrawHelpers::StripMnemonics(LPTSTR lpszClear)
{
	if (lpszClear == NULL || lpszClear == LPSTR_TEXTCALLBACK)
		return;

	LPTSTR lpszResult = lpszClear;

	while (*lpszClear)
	{
		if (*lpszClear == _T('&') && *(lpszClear + 1) != _T('&'))
		{
			lpszClear++;
			continue;
		}

		*lpszResult++ = *lpszClear++;
	}

	*lpszResult = 0;
}

void CUIDrawHelpers::BlurPoints(CDC* pDC, LPPOINT pts, int nCount)
{
	for (int i = 0; i < nCount; i += 2)
	{
		CPoint ptBlur = pts[i];
		CPoint ptDirection(pts[i].x + pts[i + 1].x, pts[i].y + pts[i + 1].y);

		COLORREF clrBlur = pDC->GetPixel(ptDirection);
		COLORREF clrDirection = pDC->GetPixel(ptBlur);

		pDC->SetPixel(ptBlur, RGB(
			(GetRValue(clrBlur) + GetRValue(clrDirection)) / 2,
			(GetGValue(clrBlur) + GetGValue(clrDirection)) / 2,
			(GetBValue(clrBlur) + GetBValue(clrDirection)) / 2));
	}
}

COLORREF CUIDrawHelpers::BlendColors(COLORREF crA, COLORREF crB, double fAmountA)
{
	double fAmountB = (1.0 - fAmountA);
	int btR = (int)(GetRValue(crA) * fAmountA + GetRValue(crB) * fAmountB);
	int btG = (int)(GetGValue(crA) * fAmountA + GetGValue(crB) * fAmountB);
	int btB = (int)(GetBValue(crA) * fAmountA + GetBValue(crB) * fAmountB);

	return RGB(min(255, btR), (BYTE)min(255, btG), (BYTE)min(255, btB));
}

COLORREF CUIDrawHelpers::DarkenColor(long lScale, COLORREF lColor)
{
	long red   = MulDiv(GetRValue(lColor), (255 - lScale), 255);
	long green = MulDiv(GetGValue(lColor), (255 - lScale), 255);
	long blue  = MulDiv(GetBValue(lColor), (255 - lScale), 255);

	return RGB(red, green, blue);
}

COLORREF CUIDrawHelpers::LightenColor(long lScale, COLORREF lColor)
{
	long R = MulDiv(255 - GetRValue(lColor), lScale, 255) + GetRValue(lColor);
	long G = MulDiv(255 - GetGValue(lColor), lScale, 255) + GetGValue(lColor);
	long B = MulDiv(255 - GetBValue(lColor), lScale, 255) + GetBValue(lColor);

	return RGB(R, G, B);
}

CPoint CUIDrawHelpers::Dlu2Pix(int dluX, int dluY)
{
	CPoint baseXY(::GetDialogBaseUnits());
	CPoint pixXY(0,0);
	pixXY.x = ::MulDiv(dluX, baseXY.x, 4);
	pixXY.y = ::MulDiv(dluY, baseXY.y, 8);
	return pixXY;
}

COLORREF CUIDrawHelpers::RGBtoHSL(COLORREF rgb)
{
	int delta, sum;
	int nH, nS, nL;
	int r = GetRValue(rgb);
	int g = GetGValue(rgb);
	int b = GetBValue(rgb);
	int cmax = ((r) >= (g) ? ((r) >= (b) ? (r) : (b)) : (g) >= (b) ? (g) : (b));
	int cmin = ((r) <= (g) ? ((r) <= (b) ? (r) : (b)) : (g) <= (b) ? (g) : (b));

	nL = (cmax + cmin + 1) / 2;
	if (cmax == cmin)
	{
		nH = 255; // H is really undefined
		nS = 0;
	}
	else
	{
		delta = cmax - cmin;
		sum = cmax + cmin;
		if (nL < 127)
			nS = ((delta + 1) * 256) / sum;
		else
			nS = (delta * 256) / ((2 * 256) - sum);
		if (r == cmax)
			nH = ((g - b) * 256) / delta;
		else if (g == cmax)
			nH = (2 * 256) + ((b - r) * 256) / delta;
		else
			nH = (4 * 256) + ((r - g) * 256) / delta;
		nH /= 6;
		if (nH < 0)
			nH += 256;
	}
	nH = nH * 239 / 255;
	nS = nS * 240 / 255;
	nL = nL * 240 / 255;

	return RGB((BYTE)min(nH, 239), (BYTE)min(nS, 240), (BYTE)min(nL, 240));
}

void CUIDrawHelpers::RGBtoHSL(COLORREF clr, double& h, double& s, double& l)
{
	double r = (double)GetRValue(clr)/255.0;
	double g = (double)GetGValue(clr)/255.0;
	double b = (double)GetBValue(clr)/255.0;

	double maxcolor = __max(r, __max(g, b));
	double mincolor = __min(r, __min(g, b));

	l = (maxcolor + mincolor)/2;

	if (maxcolor == mincolor)
	{
		h = 0;
		s = 0;
	}
	else
	{
		if (l < 0.5)
			s = (maxcolor-mincolor)/(maxcolor + mincolor);
		else
			s = (maxcolor-mincolor)/(2.0-maxcolor-mincolor);

		if (r == maxcolor)
			h = (g-b)/(maxcolor-mincolor);
		else if (g == maxcolor)
			h = 2.0+(b-r)/(maxcolor-mincolor);
		else
			h = 4.0+(r-g)/(maxcolor-mincolor);

		h /= 6.0;
		if (h < 0.0) h += 1;
	}
}

double CUIDrawHelpers::HueToRGB(double temp1, double temp2, double temp3)
{
	if (temp3 < 0)
		temp3 = temp3 + 1.0;
	if (temp3 > 1)
		temp3 = temp3-1.0;

	if (6.0*temp3 < 1)
		return (temp1+(temp2-temp1)*temp3 * 6.0);

	else if (2.0*temp3 < 1)
		return temp2;

	else if (3.0*temp3 < 2.0)
		return (temp1+(temp2-temp1)*((2.0/3.0)-temp3)*6.0);

	return temp1;
}

int CUIDrawHelpers::HueToRGB(int m1, int m2, int h)
{
	if (h < 0)
		h += 255;

	if (h > 255)
		h -= 255;

	if ((6 * h) < 255)
		return ((m1 + ((m2 - m1) / 255 * h * 6)) / 255);

	if ((2 * h) < 255)
		return m2 / 255;

	if ((3 * h) < (2 * 255))
		return ((m1 + (m2 - m1) / 255 * ((255 * 2 / 3) - h) * 6) / 255);

	return (m1 / 255);
}

COLORREF CUIDrawHelpers::HSLtoRGB(COLORREF hsl)
{
	int r, g, b;
	int m1, m2;
	int nH = GetRValue(hsl) * 255 / 239;
	int nS = GetGValue(hsl) * 255 / 240;
	int nL = GetBValue(hsl) * 255 / 240;

	if (nS == 0)
		r = g = b = nL;
	else
	{
		if (nL <= 127)
			m2 = nL * (255 + nS);
		else
			m2 = (nL + nS - ((nL * nS) / 255)) * 255;
		m1 = (2 * 255 * nL) - m2;
		r = HueToRGB(m1, m2, nH + (255 / 3));
		g = HueToRGB(m1, m2, nH);
		b = HueToRGB(m1, m2, nH - (255 / 3));
	}
	return RGB((BYTE)min(r, 255), (BYTE)min(g, 255), (BYTE)min(b, 255));
}

COLORREF CUIDrawHelpers::HSLtoRGB(double h, double s, double l)
{
	double r, g, b;
	double temp1, temp2;

	if (s == 0)
	{
		r = g = b = l;
	}
	else
	{
		if (l < 0.5)
			temp2 = l*(1.0 + s);
		else
			temp2 = l + s-l*s;

		temp1 = 2.0 * l-temp2;

		r = HueToRGB(temp1, temp2, h + 1.0/3.0);
		g = HueToRGB(temp1, temp2, h);
		b = HueToRGB(temp1, temp2, h - 1.0/3.0);
	}

	return RGB((BYTE)(r * 255), (BYTE)(g * 255), (BYTE)(b * 255));
}

static int CALLBACK UIEnumFontFamExProc(ENUMLOGFONTEX* pelf, NEWTEXTMETRICEX* /*lpntm*/, int /*FontType*/, LPVOID pThis)
{
	LPCTSTR strFontName = (LPCTSTR)pThis;
	CString strFaceName = pelf->elfLogFont.lfFaceName;

	if (strFaceName.CompareNoCase(strFontName) == 0)
		return 0;

	return 1;
}

BOOL CUIDrawHelpers::FontExists(LPCTSTR strFaceName)
{
	// Enumerate all styles and charsets of all fonts:
	LOGFONT lfEnum;
	::ZeroMemory(&lfEnum, sizeof(LOGFONT));

	lfEnum.lfFaceName[ 0 ] = 0;
	lfEnum.lfCharSet = DEFAULT_CHARSET;

	CWindowDC dc(NULL);

	return  ::EnumFontFamiliesEx(dc.m_hDC, &lfEnum, (FONTENUMPROC)
		UIEnumFontFamExProc, (LPARAM)strFaceName, 0) == 0;
}

CString CUIDrawHelpers::GetDefaultFontName()
{
	LOGFONT lfFont;
	ZeroMemory(&lfFont, sizeof(LOGFONT));
	::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &lfFont);

	return CString(lfFont.lfFaceName);
}

CString AFX_CDECL CUIDrawHelpers::GetVerticalFontName(BOOL bUseOfficeFont)
{
	LOGFONT lfFont;
	ZeroMemory(&lfFont, sizeof(LOGFONT));
	::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &lfFont);
	bool bUseSystemFont = lfFont.lfCharSet > SYMBOL_CHARSET;

	if (bUseSystemFont && !UISystemVersion()->IsWin2KOrGreater())
		bUseSystemFont = FALSE;

	if (bUseSystemFont && (_tcsicmp(lfFont.lfFaceName, _T("MS Shell Dlg")) == 0))
		bUseSystemFont = FALSE; // Can draw it vertically in Windows 2000.

	CString strVerticalFaceName = _T("Arial");
	LPCTSTR strOfficeFont = _T("Tahoma");

	if (bUseSystemFont || !FontExists(strVerticalFaceName))
	{
		strVerticalFaceName = lfFont.lfFaceName;
	}
	else if (bUseOfficeFont && !bUseSystemFont && FontExists(strOfficeFont))
	{
		strVerticalFaceName = strOfficeFont;
	}

	return strVerticalFaceName;
}

DWORD CUIDrawHelpers::GetLayout(CDC *pDC)
{
	DWORD dwLayout = 0;

	if (NULL != pDC)
	{
		dwLayout = GetLayout(pDC->GetSafeHdc());
	}

	return dwLayout;
}

DWORD CUIDrawHelpers::GetLayout(HDC hDC)
{
	typedef DWORD (CALLBACK* PFNGDIGETLAYOUTPROC)(HDC);
	static PFNGDIGETLAYOUTPROC s_pfnGetLayout = NULL;

	if (NULL == s_pfnGetLayout)
	{
		HINSTANCE hInstance = ::GetModuleHandle(_T("GDI32.DLL"));
		s_pfnGetLayout = (PFNGDIGETLAYOUTPROC)::GetProcAddress(hInstance, "GetLayout");
		ASSERT(NULL != s_pfnGetLayout); // No entry point for GetLayout
	}

	DWORD dwLayout = 0;

	if (NULL != s_pfnGetLayout)
	{
		dwLayout = s_pfnGetLayout(hDC);
	}

	return dwLayout;
}

BOOL CUIDrawHelpers::IsContextRTL(CDC *pDC)
{
	BOOL bIsContextRTL = FALSE;

	if (NULL != pDC)
	{
		bIsContextRTL = IsContextRTL(pDC->GetSafeHdc());
	}

	return bIsContextRTL;
}

BOOL CUIDrawHelpers::IsContextRTL(HDC hDC)
{
	DWORD dwLayout = GetLayout(hDC);
	return dwLayout;
}

void CUIDrawHelpers::SetContextRTL(CDC* pDC, BOOL bLayoutRTL)
{
	if (pDC) SetContextRTL(pDC->GetSafeHdc(), bLayoutRTL);
}

void CUIDrawHelpers::SetContextRTL(HDC hDC, BOOL bLayoutRTL)
{
	typedef DWORD (CALLBACK* PFNGDISETLAYOUTPROC)(HDC, DWORD);
	static PFNGDISETLAYOUTPROC s_pfn = (PFNGDISETLAYOUTPROC)-1;

	if (s_pfn == (PFNGDISETLAYOUTPROC)-1)
	{
		HINSTANCE hInst = ::GetModuleHandleA("GDI32.DLL");

		s_pfn = hInst ? (PFNGDISETLAYOUTPROC)GetProcAddress(hInst, "SetLayout") : NULL;
	}

	if (s_pfn != NULL)
	{
		(*s_pfn)(hDC, bLayoutRTL);
	}
}


void CUIDrawHelpers::KeyToLayout(CWnd* pWnd, UINT& nChar)
{
	ASSERT(pWnd);
	if (!pWnd || !pWnd->GetSafeHwnd())
		return;

	if (nChar == VK_LEFT && pWnd->GetExStyle() & WS_EX_LAYOUTRTL)
		nChar = VK_RIGHT;
	else if (nChar == VK_RIGHT && pWnd->GetExStyle() & WS_EX_LAYOUTRTL)
		nChar = VK_LEFT;

}

void CUIDrawHelpers::Triangle(CDC* pDC, CPoint pt0, CPoint pt1, CPoint pt2, COLORREF clr)
{
	CUIPenDC pen (*pDC, clr);
	CUIBrushDC brush (*pDC, clr);

	Triangle(pDC, pt0, pt1, pt2);
}

BOOL CUIDrawHelpers::DrawLine(CDC *pDC, int x1, int y1, int x2, int y2, COLORREF crLine)
{
	if (pDC->GetSafeHdc())
	{
		CUIPenDC penDC(*pDC, crLine);
		pDC->MoveTo(x1, y1);
		pDC->LineTo(x2, y2);
		return TRUE;
	}
	return FALSE;
}

DWORD CUIDrawHelpers::GetComCtlVersion()
{
	return UISystemVersion()->GetComCtlVersion();
}

BOOL CUIDrawHelpers::IsLowResolution(HDC hDC/* = 0*/)
{
	return UIColorManager()->IsLowResolution(hDC);
}
/*
CRect CUIDrawHelpers::GetWorkArea(LPCRECT rect)
{
	return UIMultiMonitor()->GetWorkArea(rect);
}

CRect CUIDrawHelpers::GetWorkArea(const CWnd* pWnd)
{
	return UIMultiMonitor()->GetWorkArea(pWnd);
}

CRect CUIDrawHelpers::GetScreenArea(const CWnd* pWnd)
{
	return UIMultiMonitor()->GetScreenArea(pWnd);
}

CRect CUIDrawHelpers::GetWorkArea(const POINT& point)
{
	return UIMultiMonitor()->GetWorkArea(point);
}

CRect CUIDrawHelpers::GetWorkArea()
{
	return UIMultiMonitor()->GetWorkArea();
}
*/ // iss__
BOOL CUIDrawHelpers::TakeSnapShot(CWnd* pWnd, CBitmap& bmpSnapshot)
{
	if (!::IsWindow(pWnd->GetSafeHwnd()))
		return FALSE;

	CWnd *pWndParent = pWnd->GetParent();
	if (::IsWindow(pWndParent->GetSafeHwnd()))
	{
		if (bmpSnapshot.GetSafeHandle() != NULL)
			bmpSnapshot.DeleteObject();

		//convert our coordinates to our parent coordinates.
		CUIWindowRect rc(pWnd);
		pWndParent->ScreenToClient(&rc);

		//copy what's on the parents background at this point
		CDC *pDC = pWndParent->GetDC();

		CDC memDC;
		memDC.CreateCompatibleDC(pDC);
		bmpSnapshot.CreateCompatibleBitmap(pDC, rc.Width(), rc.Height());

		CUIBitmapDC bitmapDC(&memDC, &bmpSnapshot);
		memDC.BitBlt(0, 0, rc.Width(), rc.Height(), pDC, rc.left, rc.top, SRCCOPY);

		pWndParent->ReleaseDC(pDC);

		return TRUE;
	}

	return FALSE;
}

BOOL CUIDrawHelpers::DrawTransparentBack(CDC* pDC, CWnd* pWnd, CBitmap& bmpSnapshot)
{
	if (!::IsWindow(pWnd->GetSafeHwnd()))
		return FALSE;

	if (::GetWindowLong(pWnd->GetSafeHwnd(), GWL_EXSTYLE) & WS_EX_TRANSPARENT)
	{
		// Get background.
		if (!TakeSnapShot(pWnd, bmpSnapshot))
			return FALSE;

		CUIClientRect rc(pWnd);

		CDC memDC;
		memDC.CreateCompatibleDC(pDC);

		CUIBitmapDC bitmapDC(&memDC, &bmpSnapshot);
		pDC->BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);

		return TRUE;
	}

	return FALSE;
}

HWND AFXAPI AfxGetParentOwner(HWND hWnd);

BOOL CUIDrawHelpers::IsTopParentActive(HWND hWnd)
{
	HWND hwndForeground = ::GetForegroundWindow();

	HWND hWndT;
	while ((hWndT = AfxGetParentOwner(hWnd)) != NULL)
	{
		hWnd = hWndT;
	}

	HWND hwndActivePopup = ::GetLastActivePopup(hWnd);

	if (hwndForeground == hwndActivePopup)
		return TRUE;

	return FALSE;
}
void CUIDrawHelpers::ScreenToWindow(CWnd* pWnd, LPPOINT lpPoint)
{
	RECT rc;
	::GetWindowRect(pWnd->GetSafeHwnd(), &rc);

	lpPoint->y -= rc.top;

	if (GetWindowLong(pWnd->GetSafeHwnd(), GWL_EXSTYLE) & WS_EX_LAYOUTRTL )
	{
		lpPoint->x = rc.right - lpPoint->x;
	}
	else
	{
		lpPoint->x -= rc.left;
	}
}

BOOL AFX_CDECL CUIDrawHelpers::RegisterWndClass(HINSTANCE hInstance, LPCTSTR lpszClassName, UINT style, HICON hIcon, HBRUSH hbrBackground)
{
	WNDCLASS wndcls;
	ZeroMemory(&wndcls, sizeof(wndcls));

	wndcls.style = style;
	wndcls.lpfnWndProc = ::DefWindowProc;
	wndcls.hInstance = hInstance ? hInstance : UIGetInstanceHandle();
	wndcls.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
	wndcls.lpszClassName = lpszClassName;
	wndcls.hIcon = hIcon;
	wndcls.hbrBackground = hbrBackground;

	return AfxRegisterClass(&wndcls);
}

void CUIDrawHelpers::GetWindowCaption(HWND hWnd, CString& strWindowText)
{
#ifdef _UNICODE
	int nLen = (int)::DefWindowProc(hWnd, WM_GETTEXTLENGTH, 0, 0);
	::DefWindowProc(hWnd, WM_GETTEXT, nLen + 1, (WPARAM)(LPCTSTR)strWindowText.GetBuffer(nLen));
	strWindowText.ReleaseBuffer();
#else
	int nLen = ::GetWindowTextLength(hWnd);
	::GetWindowText(hWnd, strWindowText.GetBufferSetLength(nLen), nLen + 1);
	strWindowText.ReleaseBuffer();
#endif
}

BOOL AFX_CDECL UITrackMouseEvent(HWND hWndTrack, DWORD dwFlags /*= TME_LEAVE*/, DWORD dwHoverTime /*= HOVER_DEFAULT*/)
{
	ASSERT(::IsWindow(hWndTrack));

	TRACKMOUSEEVENT tme;
	tme.cbSize = sizeof(TRACKMOUSEEVENT);
	tme.dwFlags = dwFlags;
	tme.dwHoverTime = dwHoverTime;
	tme.hwndTrack = hWndTrack;

	return ::_TrackMouseEvent(&tme);
}

BOOL AFX_CDECL UIGetPrinterDeviceDefaults(HGLOBAL& ref_hDevMode, HGLOBAL& ref_hDevNames)
{
	CWinApp* pApp = AfxGetApp();
	if (pApp)
	{
		PRINTDLG _pd;
		::ZeroMemory(&_pd, sizeof(_pd));
		if (pApp->GetPrinterDeviceDefaults(&_pd))
		{
			ref_hDevMode = _pd.hDevMode;
			ref_hDevNames = _pd.hDevNames;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL CUIDrawHelpers::GetIconLogFont(LOGFONT* plf)
{
	VERIFY(::SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(LOGFONT), plf, 0));
	plf->lfCharSet = UIResourceManager()->GetFontCharset();

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
// CUIDpi
//////////////////////////////////////////////////////////////////////////

int CUIDpi::m_iDefaultDpi = 96; // static

CUIDpi* UIDpiHelper()
{
	static CUIDpi s_instance; // singleton
	return &s_instance;
}

//////////////////////////////////////////////////////////////////////////
