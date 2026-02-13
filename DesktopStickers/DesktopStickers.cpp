#include <windows.h>
#include <tchar.h>
#include <stdlib.h>
#include <string.h>
#include <gdiplus.h>
#include <vector>

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
	UINT frameDelay;
	WCHAR imagePath[MAX_PATH];
	int width{ 300 };
	int height{ 300 };
};

static TCHAR szWindowClass[] = _T("DesktopStickers");
static TCHAR szTitle[] = _T("Windows Desktop Sticker Application");
// manager window
static TCHAR szManagerClass[] = _T("StickerManager");

HINSTANCE hInst;
// GDI+ (image loader) globals
ULONG_PTR gdiplusToken;

// list of stickers for persitance
std::vector<HWND> g_stickerWindows;
HWND g_hManagerWnd = NULL;
BOOL g_globalClickThrough = FALSE;

// helper to get sticker data from a window
StickerData* GetStickerData(HWND hWnd) { return (StickerData*)GetWindowLongPtr(hWnd, GWLP_USERDATA); }

// helper to set sticker data for a window
void SetStickerData(HWND hWnd, StickerData* pData) { SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pData); }

HWND CreateStickerWindow(HINSTANCE hInstance, const wchar_t* imagePath, int x, int y)
{
	// create sticker data
	StickerData* pSticker = new StickerData();
	pSticker->pImage = new Image(imagePath);
	wcscpy_s(pSticker->imagePath, MAX_PATH, imagePath);
	pSticker->currentFrame = 0;
	pSticker->isDragging = FALSE;

	// get frame # in gif
	UINT dimensionCount = pSticker->pImage->GetFrameDimensionsCount();
	GUID* pDimensionIDs = new GUID[dimensionCount];
	pSticker->pImage->GetFrameDimensionsList(pDimensionIDs, dimensionCount);
	pSticker->frameCount = pSticker->pImage->GetFrameCount(&pDimensionIDs[0]);
	delete[] pDimensionIDs;

	// get frame delay from GIF metadata
	UINT propSize = pSticker->pImage->GetPropertyItemSize(PropertyTagFrameDelay);
	if (propSize > 0)
	{
		PropertyItem* pPropItem = (PropertyItem*)malloc(propSize);
		if (pSticker->pImage->GetPropertyItem(PropertyTagFrameDelay, propSize, pPropItem) == Ok)
		{
			// frame delay is stored in 1/100ths of a second
			// get delay for the first frame (typically the same for all frames)
			LONG* pDelays = (LONG*)pPropItem->value;
			int delayInHundreths = pDelays[0];

			// convert to milliseconds (* 10)
			pSticker->frameDelay = delayInHundreths * 10;
		}
		free(pPropItem);
	}
	else // no frame delay in metadata
	{
		pSticker->frameDelay = 50; // set to 50 ¯\_(ツ)_/¯
	}

	// get image size
	pSticker->width = pSticker->pImage->GetWidth();
	pSticker->height = pSticker->pImage->GetHeight();

	// create window
	HWND hWnd = CreateWindowEx(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		szWindowClass,
		szTitle,
		WS_POPUP,
		x, y,
		pSticker->width, pSticker->height,  // Use actual image size
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (!hWnd) return NULL;

	// attach sticker data to window
	SetStickerData(hWnd, pSticker);

	// make ws_ex_layered opaque 255 = 100%
	SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 255, LWA_COLORKEY | LWA_ALPHA);

	// start gif if file more than 1 frame
	if (pSticker->frameCount > 1) {
		SetTimer(hWnd, 1, pSticker->frameDelay, NULL);
	}

	ShowWindow(hWnd, SW_SHOW);
	UpdateWindow(hWnd);

	// store sticker in sticker list
	g_stickerWindows.push_back(hWnd);

	return hWnd;
}

// save stickers to file
void SaveStickers()
{
	FILE* file;
	errno_t err = _wfopen_s(&file, L"stickers.dat", L"w");
	if (err != 0 || !file)
	{
		MessageBox(NULL, _T("Failed to save stickers!"), _T("Debug"), MB_OK);
		return;
	}

	// Write number of stickers
	fwprintf(file, L"%d\n", (int)g_stickerWindows.size());

	for (HWND hWnd : g_stickerWindows)
	{
		StickerData* pSticker = GetStickerData(hWnd);
		if (!pSticker || !pSticker->pImage) continue;

		// get window position
		RECT rect;
		GetWindowRect(hWnd, &rect);

		// save file path
		fwprintf(file, L"%s|%d|%d|%d|%d\n",
			pSticker->imagePath,
			rect.left,
			rect.top,
			pSticker->width,
			pSticker->height);
	}
	fclose(file);
}

// load stickers
void LoadStickers()
{
	FILE* file;
	_wfopen_s(&file, L"stickers.dat", L"r");
	if (!file) return; // no saved stickers

	int count = 0;
	fwscanf_s(file, L"%d\n", &count);

	for (int i = 0; i < count; i++)
	{
		WCHAR path[MAX_PATH];
		int x, y, width, height;

		// read imagePath
		if (fwscanf_s(file, L"%[^|]|%d|%d|%d|%d\n", path, MAX_PATH, &x, &y, &width, &height) == 5)
		{
			HWND hWnd = CreateStickerWindow(hInst, path, x, y);

			if (hWnd)
			{
				StickerData* pSticker = GetStickerData(hWnd);
				if (pSticker)
				{
					// Restore size
					pSticker->width = width;
					pSticker->height = height;
					SetWindowPos(hWnd, NULL, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);

				}
			}
		}
	}
	fclose(file);
}

LRESULT CALLBACK ManagerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int WINAPI WinMain(
				_In_ HINSTANCE hInstance,
				_In_opt_ HINSTANCE hPrevInstace,
				_In_ LPSTR		lpCmdLine,
				_In_ int		nCmdShow)
{
	hInst = hInstance;
	// initialize GDI+
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	// Sticker window registration
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc; // sticker window proc
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
			_T("Sticker class registration failed!"),
			_T("Error"),
			NULL);
		return 1;
	}

	// manager window registration
	WNDCLASSEX managerClass;
	managerClass.cbSize	= sizeof(WNDCLASSEX);
	managerClass.style			= CS_HREDRAW | CS_VREDRAW;
	managerClass.lpfnWndProc	= ManagerWndProc;  // Manager window proc
	managerClass.cbClsExtra		= 0;
	managerClass.cbWndExtra		= 0;
	managerClass.hInstance		= hInstance;
	managerClass.hIcon			= LoadIcon(hInstance, IDI_APPLICATION);
	managerClass.hCursor		= LoadCursor(NULL, IDC_ARROW);
	managerClass.hbrBackground	= (HBRUSH)(COLOR_WINDOW + 1);
	managerClass.lpszMenuName	= NULL;
	managerClass.lpszClassName	= szManagerClass;
	managerClass.hIconSm		= LoadIcon(hInstance, IDI_APPLICATION);

	if (!RegisterClassEx(&managerClass))
	{
		MessageBox(NULL,
			_T("Manager class registration failed!"),
			_T("Error"),
			NULL);
		return 1;
	}

	// Create the manager window
	HWND hManagerWnd = CreateWindowEx(
		WS_EX_TOPMOST,
		szManagerClass,
		_T("Sticker Manager"),
		WS_OVERLAPPEDWINDOW,  // Normal window with title bar for now
		CW_USEDEFAULT, CW_USEDEFAULT,
		250, 150,
		NULL, NULL, hInstance, NULL
	);

	if (!hManagerWnd)
	{
		MessageBox(NULL, _T("Manager window creation failed!"), _T("Error"), NULL);
		return 1;
	}

	g_hManagerWnd = hManagerWnd;

	ShowWindow(hManagerWnd, nCmdShow);
	UpdateWindow(hManagerWnd);

	// load stickers
	LoadStickers();

	// Enable drag-and-drop
	DragAcceptFiles(hManagerWnd, TRUE);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return (int)msg.wParam;
}

// Manager window procedure
LRESULT CALLBACK ManagerWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		// Simple background
		RECT rect;
		GetClientRect(hWnd, &rect);
		FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

		// Draw some text
		TCHAR text[] = _T("Drop GIFs here");
		DrawText(hdc, text, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DROPFILES:
	{
		HDROP hDrop = (HDROP)wParam;

		// get number of files dropped
		UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

		// process each dropped file
		for (UINT i = 0; i < fileCount; i++)
		{
			// get file path
			WCHAR filePath[MAX_PATH];
			DragQueryFile(hDrop, i, filePath, MAX_PATH);

			int len = wcslen(filePath);
			if (len > 4 && _wcsicmp(&filePath[len - 4], L".gif") == 0)
			{
				// create sticker at default position
				CreateStickerWindow(hInst, filePath, 200 + (i * 50), 200 + (i * 50));
			}
		}
		DragFinish(hDrop);
	}
	break;
	case WM_CLOSE:
		SaveStickers();
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}

	return 0;
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
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);

		if (pSticker && pSticker->pImage)
		{
			// Get window size
			RECT rect;
			GetClientRect(hWnd, &rect);
			int width = rect.right - rect.left;
			int height = rect.bottom - rect.top;

			// Create off-screen buffer
			HDC memDC = CreateCompatibleDC(hdc);
			HBITMAP memBitmap = CreateCompatibleBitmap(hdc, width, height);
			HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

			// Draw with GDI+ on BLACK background (matches colorkey)
			Graphics graphics(memDC);
			graphics.Clear(Color(0, 0, 0));  // Black = transparent
			graphics.SetInterpolationMode(InterpolationModeHighQualityBicubic);
			graphics.SetSmoothingMode(SmoothingModeHighQuality);
			graphics.DrawImage(pSticker->pImage, 0, 0, width, height);

			// Copy to screen
			BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);

			// Cleanup
			SelectObject(memDC, oldBitmap);
			DeleteObject(memBitmap);
			DeleteDC(memDC);
		}

		EndPaint(hWnd, &ps);
	}
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
	case WM_RBUTTONDOWN:
	{
		if (pSticker && !g_globalClickThrough) // sticker exists, and we're not in clickthru mode
		{
			// create context menu
			HMENU hMenu = CreatePopupMenu();
			AppendMenu(hMenu, MF_STRING, 1, _T("Delete Sticker"));

			// get cursor position for menu placement
			POINT pt;
			GetCursorPos(&pt);

			// show menu and get user selection
			int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTBUTTON,
				pt.x, pt.y, 0, hWnd, NULL);
			if (cmd == 1)
			{
				DestroyWindow(hWnd);
			}
			DestroyMenu(hMenu);
		}
	}
	break;
	case WM_KEYDOWN:
		// Check if Ctrl+Shift+T is pressed (click through command)
		if (pSticker && wParam == 'T' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000))
		{
			// Apply to ALL stickers
			for (HWND stickerWnd : g_stickerWindows)
			{
				if (g_globalClickThrough)
				{
					SetWindowLong(stickerWnd, GWL_EXSTYLE,
						GetWindowLong(stickerWnd, GWL_EXSTYLE) | WS_EX_TRANSPARENT);
				}
				else
				{
					SetWindowLong(stickerWnd, GWL_EXSTYLE,
						GetWindowLong(stickerWnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
				}
			}
		}
		break;
	case WM_MOUSEWHEEL:
	{
		if (pSticker && !g_globalClickThrough)
		{
			// get scroll direction
			int delta = GET_WHEEL_DELTA_WPARAM(wParam);
			// adjust size 5% per scroll
			float scale = (delta > 0) ? 1.05f : 0.95f;
			pSticker->width = (int)(pSticker->width * scale);
			pSticker->height = (int)(pSticker->height * scale);

			// Enforce minimum size
			if (pSticker->width < 50) pSticker->width = 50;
			if (pSticker->height < 50) pSticker->height = 50;

			// Enforce maximum size
			if (pSticker->width > 1920) pSticker->width = 1920;
			if (pSticker->height > 1920) pSticker->height = 1920;

			// Get current position
			RECT rect;
			GetWindowRect(hWnd, &rect);

			// Move AND resize in one call with exact dimensions
			MoveWindow(hWnd, rect.left, rect.top, pSticker->width, pSticker->height, TRUE);

			// Force immediate redraw
			RedrawWindow(hWnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE);
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
	case WM_SIZE:
	{
		// Force complete redraw when window size changes
		InvalidateRect(hWnd, NULL, TRUE);
	}
	break;
	case WM_DESTROY:
	{
		//remove sticker from list
		auto it = std::find(g_stickerWindows.begin(), g_stickerWindows.end(), hWnd);
		if (it != g_stickerWindows.end())
		{
			g_stickerWindows.erase(it);
		}

		// Clean up this sticker
		StickerData* pSticker = GetStickerData(hWnd);
		if (pSticker)
		{
			// stop timer
			KillTimer(hWnd, 1);

			if (pSticker->pImage)
			{
				delete pSticker->pImage;
			}
			delete pSticker;
		}
	}
		break;
	case WM_ERASEBKGND:
		return 1;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	}

	return 0;
}