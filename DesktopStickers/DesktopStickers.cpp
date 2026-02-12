#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>
#include <gdiplus.h>

using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")

// Sticker struct
struct StickerData
{
	Image* pImage;
	UINT frameCount;
	UINT currentFrame;
	BOOL isDragging;
	POINT dragOffset;
	BOOL isClickThrough;
	UINT frameDelay{ 30 };
};

static TCHAR szWindowClass[] = _T("DesktopStickers");
static TCHAR szTitle[] = _T("Windows Desktop Sticker Application");

HINSTANCE hInst;

// GDI+ (image loader) globals
ULONG_PTR gdiplusToken;

// helper to get sticker data from a window
StickerData* GetStickerData(HWND hWnd) { return (StickerData*)GetWindowLongPtr(hWnd, GWLP_USERDATA); }

// helper to set sticker data for a window
void SetStickerData(HWND hWnd, StickerData* pData) { SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pData); }

HWND CreateStickerWindow(HINSTANCE hInstance, const wchar_t* imagePath, int x, int y)
{
	// create sticker data
	StickerData* pSticker = new StickerData();
	pSticker->pImage = new Image(imagePath);
	pSticker->currentFrame = 0;
	pSticker->isDragging = FALSE;
	pSticker->isClickThrough = FALSE;

	// get frame # in gif
	UINT dimensionCount = pSticker->pImage->GetFrameDimensionsCount();
	GUID* pDimensionIDs = new GUID[dimensionCount];
	pSticker->pImage->GetFrameDimensionsList(pDimensionIDs, dimensionCount);
	pSticker->frameCount = pSticker->pImage->GetFrameCount(&pDimensionIDs[0]);
	delete[] pDimensionIDs;

	// create window
	HWND hWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		szWindowClass,
		szTitle,
		WS_POPUP,
		100, 100,
		500, 300,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hWnd) return NULL;

	// attach sticker data to window
	SetStickerData(hWnd, pSticker);

	// make ws_ex_layered opaque 255 = 100%
	SetLayeredWindowAttributes(hWnd, RGB(255, 0, 255), 255, LWA_COLORKEY | LWA_ALPHA);

	// start gif if file more than 1 frame
	if (pSticker->frameCount > 1) {
		SetTimer(hWnd, 1, pSticker->frameDelay, NULL);
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	return hWnd;
}

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(
				_In_ HINSTANCE hInstance,
				_In_opt_ HINSTANCE hPrevInstace,
				_In_ LPSTR		lpCmdLine,
				_In_ int		nCmdShow)
{
	// initialize GDI+
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(wcex.hInstance, IDI_APPLICATION);
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= NULL;
	wcex.lpszMenuName	= NULL;
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL,
			_T("Call to RegisterClassEX failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);
		return 1;
	}

	CreateStickerWindow(hInstance, L"C:\\Users\\micha\\Pictures\\stickers\\miku.gif", 100, 100);

	CreateStickerWindow(hInstance, L"C:\\Users\\micha\\Pictures\\stickers\\darling.gif", 300, 200);


	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(
	_In_ HWND	hWnd,
	_In_ UINT	message,
	_In_ WPARAM	wParam,
	_In_ LPARAM	lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;
	
	// get this windows sticker data
	StickerData* pSticker = GetStickerData(hWnd);

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		if (pSticker && pSticker->pImage)
		{
			// get window size
			RECT rect;
			GetClientRect(hWnd, &rect);
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;

			// create the off screen buffer
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
			HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

			// draw to buffer
			Graphics graphics(memDC);
			graphics.Clear(Color(255, 0, 255));
			graphics.DrawImage(pSticker->pImage, 0, 0);

			// copy buffer to screen
			BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

			// clean up
			SelectObject(memDC, oldBitmap);
			DeleteObject(memBitmap);
			DeleteDC(memDC);
		}

		EndPaint(hWnd, &ps);
		break;
	case WM_LBUTTONDOWN: // dragging window
		if (pSticker)
		{
			pSticker->isDragging = TRUE;

			RECT rect;
			GetWindowRect(hWnd, &rect); // get window current pos (x,y)

			POINT cursorPos;
			GetCursorPos(&cursorPos); // get cursor pos (x,y)

			// calc offset
			pSticker->dragOffset.x = cursorPos.x - rect.left;
			pSticker->dragOffset.y = cursorPos.y - rect.top;

			SetCapture(hWnd);
		}
		break;
	case WM_MOUSEMOVE:
		if (pSticker && pSticker->isDragging)
		{
			POINT cursorPos;
			GetCursorPos(&cursorPos);

			// new window pos = curr mouse pos - offset
			SetWindowPos(hWnd, NULL,
				cursorPos.x - pSticker->dragOffset.x,
				cursorPos.y - pSticker->dragOffset.y,
				0, 0,
				SWP_NOSIZE | SWP_NOZORDER);
		}
		break;
	case WM_LBUTTONUP:
		if (pSticker && pSticker->isDragging)
		{
			pSticker->isDragging = FALSE;
			ReleaseCapture(); // releases mouse capture
		}
		break;
	case WM_KEYDOWN:
		// Check if Ctrl+Shift+T is pressed (click through command)
		if (pSticker && wParam == 'T' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000))
		{
			pSticker->isClickThrough = !pSticker->isClickThrough;

			if (pSticker->isClickThrough)
			{
				// enable click-through
				SetWindowLong(hWnd, GWL_EXSTYLE,
					GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
			}
			else
			{
				// disable click-through
				SetWindowLong(hWnd, GWL_EXSTYLE,
					GetWindowLong(hWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
			}
		}
		break;
	case WM_TIMER:
		if (pSticker && pSticker->pImage && pSticker->frameCount > 1)
		{
			// move to next frame
			pSticker->currentFrame = (pSticker->currentFrame + 1) % pSticker->frameCount;

			// select the frame
			GUID dimensionID = FrameDimensionTime;
			pSticker->pImage->SelectActiveFrame(&dimensionID, pSticker->currentFrame);

			// force redraw
			InvalidateRect(hWnd, NULL, FALSE);
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_ERASEBKGND:
		return 1;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}