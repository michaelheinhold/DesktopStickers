#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>
#include <gdiplus.h>

using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")

static TCHAR szWindowClass[] = _T("DesktopStickers");
static TCHAR szTitle[] = _T("Windows Desktop Sticker Application");

HINSTANCE hInst;

// GDI+ (image loader) globals
ULONG_PTR gdiplusToken;
Image* pImage = NULL;

// window dragging state
BOOL isDragging = FALSE;
POINT dragOffset;

// click through gloabal var
BOOL isClickThrough = FALSE;

// animation globals
UINT frameCount = 0;
UINT currentFrame = 0;
UINT frameDelay = 30;

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

	// load the image
	pImage = new Image(L"C:\\Users\\micha\\Pictures\\stickers\\miku.gif");

	// Get the number of frames in the GIF
	UINT dimensionCount = pImage->GetFrameDimensionsCount();
	GUID* pDimensionIDs = new GUID[dimensionCount];
	pImage->GetFrameDimensionsList(pDimensionIDs, dimensionCount);
	frameCount = pImage->GetFrameCount(&pDimensionIDs[0]);
	delete[] pDimensionIDs;

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
	if (!hWnd)
	{
		MessageBox(NULL,
			_T("Call to CreateWindowEx failed!"),
			_T("Windows Desktop Guided Tour"),
			NULL);

		return 1;
	}

	// make ws_ex_layered opaque 255 = 100%
	SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);

	// start gif if file more than 1 frame
	if (frameCount > 1) {
		SetTimer(hWnd, 1, frameDelay, NULL);
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

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
	TCHAR greeting[] = _T("Hello, Windows desktop!");

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);

		if (pImage)
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
			graphics.Clear(Color(0, 0, 0, 0));
			graphics.DrawImage(pImage, 0, 0);

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
		isDragging = TRUE;

		RECT rect;
		GetWindowRect(hWnd, &rect); // get window current pos (x,y)

		POINT cursorPos;
		GetCursorPos(&cursorPos); // get cursor pos (x,y)

		// calc offset
		dragOffset.x = cursorPos.x - rect.left;
		dragOffset.y = cursorPos.y - rect.top;

		SetCapture(hWnd);
		break;
	case WM_MOUSEMOVE:
		if (isDragging)
		{
			POINT cursorPos;
			GetCursorPos(&cursorPos);

			// new window pos = curr mouse pos - offset
			SetWindowPos(hWnd, NULL,
				cursorPos.x - dragOffset.x,
				cursorPos.y - dragOffset.y,
				0, 0,
				SWP_NOSIZE | SWP_NOZORDER);
		}
		break;
	case WM_LBUTTONUP:
		if (isDragging)
		{
			isDragging = FALSE;
			ReleaseCapture(); // releases mouse capture
		}
		break;
	case WM_KEYDOWN:
		// Check if Ctrl+Shift+T is pressed (click through command)
		if (wParam == 'T' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000))
		{
			isClickThrough = !isClickThrough;

			if (isClickThrough)
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
		if (pImage && frameCount > 1)
		{
			// move to next frame
			currentFrame = (currentFrame + 1) % frameCount;

			// select the frame
			GUID dimensionID = FrameDimensionTime;
			pImage->SelectActiveFrame(&dimensionID, currentFrame);

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