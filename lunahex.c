

#ifndef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT __declspec(dllimport)
#endif

#include <windows.h>
#include <winioctl.h>
#include <commctrl.h>
#include <stdio.h>
#include <commdlg.h>

#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

#ifndef IOCTL_DISK_GET_LENGTH_INFO
#define IOCTL_DISK_GET_LENGTH_INFO 0x0007405C
typedef struct _GET_LENGTH_INFORMATION {
    LARGE_INTEGER Length;
} GET_LENGTH_INFORMATION, *PGET_LENGTH_INFORMATION;
#endif

// ============ МАКРОСЫ И КОНСТАНТЫ ============
#define IDC_LISTVIEW        1001
#define IDC_BTN_OPEN_DISK   1002
#define IDC_BTN_OPEN_FILE   1003
#define IDC_BTN_EXPORT      1004
#define IDC_BTN_LOAD_BACKUP 1005
#define IDC_BTN_COMPARE     1006
#define IDC_BTN_RESTORE_ALL 1007
#define IDC_BTN_EDIT_MODE   1008
#define IDC_BTN_APPLY       1009
#define IDC_STATUSBAR       1011
#define IDC_TIMER           1013

#ifndef IDD_DISK_DIALOG
#define IDD_DISK_DIALOG     2000
#define IDC_DISK_SPIN       2001
#define IDC_DISK_NUMBER     2002
#endif

#define COLOR_MISMATCH_BG   RGB(255, 200, 200)
#define COLOR_MISMATCH_TEXT RGB(200, 0, 0)
#define COLOR_MATCH_BG      RGB(200, 255, 200)
#define COLOR_MATCH_TEXT    RGB(0, 150, 0)
#define COLOR_EDIT_BG       RGB(255, 255, 200)

#define CACHE_SIZE 512

// ============ СТРУКТУРЫ И ТИПЫ ============
// Структура одного изменения (Delta Map)
typedef struct {
    LARGE_INTEGER offset;
    BYTE new_val;
} EDIT_RECORD;

// Движок документа
typedef struct tagDOCUMENT
{
    HANDLE        hFile;
    BOOL          is_disk;
    LARGE_INTEGER total_bytes;
    WCHAR         source_name[MAX_PATH];

    // Кэш основного файла (1 сектор)
    LARGE_INTEGER cache_offset;
    BYTE          cache[CACHE_SIZE];
    BOOL          cache_valid;

    // Карта изменений
    EDIT_RECORD   *edits;
    DWORD         edit_count;
    DWORD         edit_capacity;
    BOOL          is_modified;

    // Бекап (бинарный)
    HANDLE        hBackup;
    LARGE_INTEGER backup_size;
    WCHAR         backup_path[MAX_PATH];
    BOOL          backup_loaded;
    
    // Кэш бекапа
    LARGE_INTEGER backup_cache_offset;
    BYTE          backup_cache[CACHE_SIZE];
    BOOL          backup_cache_valid;
} DOCUMENT;

typedef enum { MODE_VIEW, MODE_EDIT, MODE_COMPARE } APP_MODE;

typedef struct {
    UINT_PTR timerId;
    int remaining;
} APPLY_TIMER;

typedef struct {
    HWND hEdit;
    HWND hListView;
    int itemIndex;
    int subItem;
} EDITCELLINFO;

// ============ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ============
HINSTANCE       g_hInst = NULL;
HWND            g_hMainWnd = NULL;
DOCUMENT        g_doc = {0};
APP_MODE        g_mode = MODE_VIEW;
APPLY_TIMER     g_applyTimer = {0, 0};
HWND            g_hListView = NULL;
HWND            g_hStatusBar = NULL;
HWND            g_hBtnApply = NULL;
EDITCELLINFO    g_editInfo;
WNDPROC         OldEditProc = NULL;

// ============ ПРОТОТИПЫ ФУНКЦИЙ ============
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CheckAdminRights(void);

void DocInit(DOCUMENT* doc);
void DocClose(DOCUMENT* doc);
BOOL DocOpenDisk(DOCUMENT* doc, DWORD diskNumber);
BOOL DocOpenFile(DOCUMENT* doc, const WCHAR* path);
BYTE DocGetByte(DOCUMENT* doc, LARGE_INTEGER offset, BOOL* is_edited);
void DocSetByte(DOCUMENT* doc, LARGE_INTEGER offset, BYTE val);
BOOL DocWriteAllEdits(DOCUMENT* doc);
BYTE BackupGetByte(DOCUMENT* doc, LARGE_INTEGER offset);

BOOL BackupExport(DOCUMENT* doc, const WCHAR* path);
BOOL BackupLoad(DOCUMENT* doc, const WCHAR* path);
BOOL BackupRestoreAll(DOCUMENT* doc);

BOOL IsValidHexChar(WCHAR c);
BYTE HexCharToByte(WCHAR c);
void ByteToHexStr(BYTE b, WCHAR* out);

void InitGUI(HWND hwnd);
void UpdateListView(HWND hwnd);
void UpdateStatusBar(void);
void SwitchMode(APP_MODE newMode);
void StartApplyTimer(HWND hwnd);
void StopApplyTimer(void);

HWND CreateCellEditor(HWND hListView, int itemIndex, int subItem);
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ApplyEdit(HWND hEdit);
void CancelEdit(HWND hEdit);
void EditNextCell(int nextIndex);
INT_PTR CALLBACK DiskDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleListViewCustomDraw(NMLVCUSTOMDRAW* plvcd);

// ==========================================================
//                     РЕАЛИЗАЦИЯ
// ==========================================================

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInst = hInstance;
    
    INITCOMMONCONTROLSEX icex = {0};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icex);
    
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"LunaHexEditorClass";
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    if (!RegisterClassEx(&wc)) return 1;
    
    g_hMainWnd = CreateWindowEx(
        0, L"LunaHexEditorClass", L"Luna Hex Editor PRO v2.0 (Lazy Load)",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
        NULL, NULL, hInstance, NULL);
    
    if (!g_hMainWnd) return 1;
    
    DocInit(&g_doc);
    
    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);
    CheckAdminRights();
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
}

void CheckAdminRights(void)
{
    BOOL bIsAdmin = FALSE;
    PSID pAdminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY NtAuthority = {SECURITY_NT_AUTHORITY};
    
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &pAdminGroup))
    {
        CheckTokenMembership(NULL, pAdminGroup, &bIsAdmin);
        FreeSid(pAdminGroup);
    }
    
    if (!bIsAdmin)
    {
        MessageBox(g_hMainWnd, 
            L"Внимание! Для прямого доступа к PhysicalDrive нужны права администратора.",
            L"Предупреждение", MB_ICONWARNING | MB_OK);
    }
}

// --- ЯДРО ДВИЖКА (LAZY LOADING) ---
void DocInit(DOCUMENT* doc)
{
    memset(doc, 0, sizeof(DOCUMENT));
    doc->hFile = INVALID_HANDLE_VALUE;
    doc->hBackup = INVALID_HANDLE_VALUE;
    doc->edit_capacity = 256;
    doc->edits = (EDIT_RECORD*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, doc->edit_capacity * sizeof(EDIT_RECORD));
}

void DocClose(DOCUMENT* doc)
{
    if (doc->hFile != INVALID_HANDLE_VALUE) CloseHandle(doc->hFile);
    if (doc->hBackup != INVALID_HANDLE_VALUE) CloseHandle(doc->hBackup);
    doc->hFile = INVALID_HANDLE_VALUE;
    doc->hBackup = INVALID_HANDLE_VALUE;
    doc->edit_count = 0;
    doc->is_modified = FALSE;
    doc->cache_valid = FALSE;
    doc->backup_cache_valid = FALSE;
    doc->backup_loaded = FALSE;
    doc->total_bytes.QuadPart = 0;
}

BOOL DocOpenDisk(DOCUMENT* doc, DWORD diskNumber)
{
    DocClose(doc);
    WCHAR path[64];
    wsprintfW(path, L"\\\\.\\PhysicalDrive%d", diskNumber);
    
    doc->hFile = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);
    
    if (doc->hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    GET_LENGTH_INFORMATION li;
    DWORD br;
    if (DeviceIoControl(doc->hFile, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &li, sizeof(li), &br, NULL))
        doc->total_bytes = li.Length;
    else
        GetFileSizeEx(doc->hFile, &doc->total_bytes);
        
    doc->is_disk = TRUE;
    wsprintfW(doc->source_name, L"Disk %d", diskNumber);
    return TRUE;
}

BOOL DocOpenFile(DOCUMENT* doc, const WCHAR* path)
{
    DocClose(doc);
    doc->hFile = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 
        FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (doc->hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    GetFileSizeEx(doc->hFile, &doc->total_bytes);
    doc->is_disk = FALSE;
    wcscpy_s(doc->source_name, MAX_PATH, path);
    return TRUE;
}

BYTE DocGetByte(DOCUMENT* doc, LARGE_INTEGER offset, BOOL* is_edited)
{
    if (is_edited) *is_edited = FALSE;
    if (doc->hFile == INVALID_HANDLE_VALUE || offset.QuadPart >= doc->total_bytes.QuadPart) return 0;
    
    // 1. Проверяем Карту изменений
    for (DWORD i = 0; i < doc->edit_count; i++) {
        if (doc->edits[i].offset.QuadPart == offset.QuadPart) {
            if (is_edited) *is_edited = TRUE;
            return doc->edits[i].new_val;
        }
    }
    
    // 2. Читаем из кэша / диска
    LARGE_INTEGER aligned;
    aligned.QuadPart = offset.QuadPart & ~(CACHE_SIZE - 1);
    
    if (!doc->cache_valid || doc->cache_offset.QuadPart != aligned.QuadPart) {
        SetFilePointerEx(doc->hFile, aligned, NULL, FILE_BEGIN);
        DWORD r;
        if (!ReadFile(doc->hFile, doc->cache, CACHE_SIZE, &r, NULL)) memset(doc->cache, 0, CACHE_SIZE);
        doc->cache_offset = aligned;
        doc->cache_valid = TRUE;
    }
    
    return doc->cache[offset.QuadPart % CACHE_SIZE];
}

void DocSetByte(DOCUMENT* doc, LARGE_INTEGER offset, BYTE val)
{
    // Ищем, есть ли уже изменение для этого смещения
    for (DWORD i = 0; i < doc->edit_count; i++) {
        if (doc->edits[i].offset.QuadPart == offset.QuadPart) {
            doc->edits[i].new_val = val;
            doc->is_modified = TRUE;
            return;
        }
    }
    
    // Расширяем массив если нужно
    if (doc->edit_count >= doc->edit_capacity) {
        doc->edit_capacity *= 2;
        doc->edits = (EDIT_RECORD*)HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, doc->edits, doc->edit_capacity * sizeof(EDIT_RECORD));
    }
    
    // Добавляем новое изменение
    doc->edits[doc->edit_count].offset = offset;
    doc->edits[doc->edit_count].new_val = val;
    doc->edit_count++;
    doc->is_modified = TRUE;
}

BOOL DocWriteAllEdits(DOCUMENT* doc)
{
    if (doc->hFile == INVALID_HANDLE_VALUE || doc->edit_count == 0) return TRUE;
    
    BYTE sector[CACHE_SIZE];
    for (DWORD i = 0; i < doc->edit_count; i++) {
        LARGE_INTEGER aligned;
        aligned.QuadPart = doc->edits[i].offset.QuadPart & ~(CACHE_SIZE - 1);
        
        // Читаем сектор, модифицируем и пишем обратно (R-M-W цикл)
        SetFilePointerEx(doc->hFile, aligned, NULL, FILE_BEGIN);
        DWORD rw;
        if (ReadFile(doc->hFile, sector, CACHE_SIZE, &rw, NULL)) {
            sector[doc->edits[i].offset.QuadPart % CACHE_SIZE] = doc->edits[i].new_val;
            SetFilePointerEx(doc->hFile, aligned, NULL, FILE_BEGIN);
            WriteFile(doc->hFile, sector, CACHE_SIZE, &rw, NULL);
        }
    }
    
    doc->edit_count = 0;
    doc->is_modified = FALSE;
    doc->cache_valid = FALSE; // Инвалидируем кэш
    return TRUE;
}

// --- ЯДРО БЕКАПОВ (БИНАРНОЕ) ---
BOOL BackupExport(DOCUMENT* doc, const WCHAR* path)
{
    if (doc->hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    HANDLE hOut = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut == INVALID_HANDLE_VALUE) return FALSE;
    
    // Дампим по 4 мегабайта за раз
    const DWORD BBUF_SIZE = 4 * 1024 * 1024;
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, BBUF_SIZE);
    
    LARGE_INTEGER off; off.QuadPart = 0;
    SetFilePointerEx(doc->hFile, off, NULL, FILE_BEGIN);
    
    while (off.QuadPart < doc->total_bytes.QuadPart) {
        DWORD toRead = (DWORD)min(BBUF_SIZE, doc->total_bytes.QuadPart - off.QuadPart);
        DWORD r, w;
        if (!ReadFile(doc->hFile, buf, toRead, &r, NULL) || r == 0) break;
        WriteFile(hOut, buf, r, &w, NULL);
        off.QuadPart += r;
    }
    
    HeapFree(GetProcessHeap(), 0, buf);
    CloseHandle(hOut);
    return TRUE;
}

BOOL BackupLoad(DOCUMENT* doc, const WCHAR* path)
{
    if (doc->hBackup != INVALID_HANDLE_VALUE) CloseHandle(doc->hBackup);
    
    doc->hBackup = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (doc->hBackup != INVALID_HANDLE_VALUE) {
        GetFileSizeEx(doc->hBackup, &doc->backup_size);
        doc->backup_loaded = TRUE;
        doc->backup_cache_valid = FALSE;
        wcscpy_s(doc->backup_path, MAX_PATH, path);
        return TRUE;
    }
    return FALSE;
}

BYTE BackupGetByte(DOCUMENT* doc, LARGE_INTEGER offset)
{
    if (!doc->backup_loaded || offset.QuadPart >= doc->backup_size.QuadPart) return 0;
    
    LARGE_INTEGER aligned;
    aligned.QuadPart = offset.QuadPart & ~(CACHE_SIZE - 1);
    
    if (!doc->backup_cache_valid || doc->backup_cache_offset.QuadPart != aligned.QuadPart) {
        SetFilePointerEx(doc->hBackup, aligned, NULL, FILE_BEGIN);
        DWORD r;
        if (!ReadFile(doc->hBackup, doc->backup_cache, CACHE_SIZE, &r, NULL)) memset(doc->backup_cache, 0, CACHE_SIZE);
        doc->backup_cache_offset = aligned;
        doc->backup_cache_valid = TRUE;
    }
    return doc->backup_cache[offset.QuadPart % CACHE_SIZE];
}

BOOL BackupRestoreAll(DOCUMENT* doc)
{
    if (!doc->backup_loaded || doc->hFile == INVALID_HANDLE_VALUE) return FALSE;
    
    const DWORD BBUF_SIZE = 4 * 1024 * 1024;
    BYTE* buf = (BYTE*)HeapAlloc(GetProcessHeap(), 0, BBUF_SIZE);
    
    LARGE_INTEGER off; off.QuadPart = 0;
    while (off.QuadPart < doc->backup_size.QuadPart && off.QuadPart < doc->total_bytes.QuadPart) {
        DWORD toRead = (DWORD)min(BBUF_SIZE, doc->backup_size.QuadPart - off.QuadPart);
        DWORD r, w;
        
        SetFilePointerEx(doc->hBackup, off, NULL, FILE_BEGIN);
        if (!ReadFile(doc->hBackup, buf, toRead, &r, NULL) || r == 0) break;
        
        SetFilePointerEx(doc->hFile, off, NULL, FILE_BEGIN);
        WriteFile(doc->hFile, buf, r, &w, NULL);
        
        off.QuadPart += r;
    }
    
    HeapFree(GetProcessHeap(), 0, buf);
    doc->edit_count = 0;
    doc->is_modified = FALSE;
    doc->cache_valid = FALSE;
    return TRUE;
}

// --- ВСПОМОГАТЕЛЬНЫЕ ---
BOOL IsValidHexChar(WCHAR c) {
    return ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'F') || (c >= L'a' && c <= L'f'));
}
BYTE HexCharToByte(WCHAR c) {
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    return 0;
}
void ByteToHexStr(BYTE b, WCHAR* out) {
    static const WCHAR hex[] = L"0123456789ABCDEF";
    out[0] = hex[b >> 4]; out[1] = hex[b & 0x0F]; out[2] = 0;
}

// --- GUI: ФУНКЦИИ ОКНА РЕДАКТИРОВАНИЯ ---
HWND CreateCellEditor(HWND hListView, int itemIndex, int subItem)
{
    RECT rcSubItem;
    ListView_GetSubItemRect(hListView, itemIndex, subItem, LVIR_BOUNDS, &rcSubItem);
    rcSubItem.left += 2; rcSubItem.top += 1; rcSubItem.bottom -= 1;
    
    HWND hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_UPPERCASE | ES_CENTER,
        rcSubItem.left, rcSubItem.top, rcSubItem.right - rcSubItem.left, rcSubItem.bottom - rcSubItem.top,
        hListView, NULL, g_hInst, NULL);
    
    if (hEdit) {
        HFONT hFont = (HFONT)SendMessage(hListView, WM_GETFONT, 0, 0);
        SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hEdit, EM_SETLIMITTEXT, 2, 0);
        
        LARGE_INTEGER off; off.QuadPart = itemIndex;
        BYTE val = DocGetByte(&g_doc, off, NULL);
        
        WCHAR hexStr[3];
        ByteToHexStr(val, hexStr);
        SetWindowText(hEdit, hexStr);
        SendMessage(hEdit, EM_SETSEL, 0, -1);
        
        g_editInfo.hEdit = hEdit;
        g_editInfo.hListView = hListView;
        g_editInfo.itemIndex = itemIndex;
        g_editInfo.subItem = subItem;
        
        OldEditProc = (WNDPROC)GetWindowLongPtr(hEdit, GWLP_WNDPROC);
        SetWindowLongPtr(hEdit, GWLP_USERDATA, (LONG_PTR)OldEditProc);
        SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    }
    return hEdit;
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_RETURN) { ApplyEdit(hwnd); return 0; }
        if (wParam == VK_ESCAPE) { CancelEdit(hwnd); return 0; }
        if (wParam == VK_TAB) { ApplyEdit(hwnd); EditNextCell(g_editInfo.itemIndex + 1); return 0; }
        break;
    case WM_CHAR:
        if (!IsValidHexChar((WCHAR)wParam) && wParam != VK_BACK) return 0;
        break;
    case WM_KILLFOCUS:
        ApplyEdit(hwnd); return 0;
    }
    return oldProc ? CallWindowProc(oldProc, hwnd, msg, wParam, lParam) : DefWindowProc(hwnd, msg, wParam, lParam);
}

void ApplyEdit(HWND hEdit)
{
    WCHAR hexStr[3];
    GetWindowText(hEdit, hexStr, 3);
    if (wcslen(hexStr) == 1) { hexStr[1] = hexStr[0]; hexStr[0] = L'0'; hexStr[2] = 0; }
    
    if (wcslen(hexStr) == 2 && IsValidHexChar(hexStr[0]) && IsValidHexChar(hexStr[1])) {
        BYTE newValue = (HexCharToByte(hexStr[0]) << 4) | HexCharToByte(hexStr[1]);
        LARGE_INTEGER off; off.QuadPart = g_editInfo.itemIndex;
        
        DocSetByte(&g_doc, off, newValue);
        ListView_RedrawItems(g_editInfo.hListView, g_editInfo.itemIndex, g_editInfo.itemIndex);
        UpdateWindow(g_editInfo.hListView);
    }
    DestroyWindow(hEdit);
    g_editInfo.hEdit = NULL;
}

void CancelEdit(HWND hEdit) { DestroyWindow(hEdit); g_editInfo.hEdit = NULL; }
void EditNextCell(int nextIndex) {
    // В UI лимит 2 ГБ
    int limit = (g_doc.total_bytes.QuadPart > 0x7FFFFFFF) ? 0x7FFFFFFF : (int)g_doc.total_bytes.QuadPart;
    if (nextIndex >= 0 && nextIndex < limit) {
        ListView_EnsureVisible(g_editInfo.hListView, nextIndex, FALSE);
        ListView_SetItemState(g_editInfo.hListView, nextIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        CreateCellEditor(g_editInfo.hListView, nextIndex, g_editInfo.subItem);
    }
}

// --- GUI: ГЛАВНАЯ ЛОГИКА ---
void InitGUI(HWND hwnd)
{
    g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA,
        0, 0, 0, 0, hwnd, (HMENU)IDC_LISTVIEW, g_hInst, NULL);
    
    ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    
    LVCOLUMN lvc = {0}; lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.iSubItem = 0; lvc.pszText = L"Address"; lvc.cx = 100; ListView_InsertColumn(g_hListView, 0, &lvc);
    lvc.iSubItem = 1; lvc.pszText = L"Hex"; lvc.cx = 80; ListView_InsertColumn(g_hListView, 1, &lvc);
    lvc.iSubItem = 2; lvc.pszText = L"Backup"; lvc.cx = 80; ListView_InsertColumn(g_hListView, 2, &lvc);
    
    CreateWindowEx(0, L"BUTTON", L"Открыть диск", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 30, hwnd, (HMENU)IDC_BTN_OPEN_DISK, g_hInst, NULL);
    CreateWindowEx(0, L"BUTTON", L"Открыть файл", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 30, hwnd, (HMENU)IDC_BTN_OPEN_FILE, g_hInst, NULL);
    CreateWindowEx(0, L"BUTTON", L"Экспорт", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 80, 30, hwnd, (HMENU)IDC_BTN_EXPORT, g_hInst, NULL);
    CreateWindowEx(0, L"BUTTON", L"Бекап", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 80, 30, hwnd, (HMENU)IDC_BTN_LOAD_BACKUP, g_hInst, NULL);
    CreateWindowEx(0, L"BUTTON", L"Сравнить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 80, 30, hwnd, (HMENU)IDC_BTN_COMPARE, g_hInst, NULL);
    CreateWindowEx(0, L"BUTTON", L"Восстановить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 30, hwnd, (HMENU)IDC_BTN_RESTORE_ALL, g_hInst, NULL);
    CreateWindowEx(0, L"BUTTON", L"Режим правки", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 0, 0, 100, 30, hwnd, (HMENU)IDC_BTN_EDIT_MODE, g_hInst, NULL);
    g_hBtnApply = CreateWindowEx(0, L"BUTTON", L"Применить", WS_CHILD | BS_PUSHBUTTON, 0, 0, 120, 30, hwnd, (HMENU)IDC_BTN_APPLY, g_hInst, NULL);
    
    g_hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, g_hInst, NULL);
}

void UpdateListView(HWND hwnd)
{
    if (!g_hListView || g_doc.hFile == INVALID_HANDLE_VALUE) return;
    
    // Стандартный ListView поддерживает только INT_MAX (2 ГБ). 
    // Для больших дисков в проф. утилитах делают 16 байт на строку.
    int limit = (g_doc.total_bytes.QuadPart > 0x7FFFFFFF) ? 0x7FFFFFFF : (int)g_doc.total_bytes.QuadPart;
    ListView_SetItemCountEx(g_hListView, limit, LVSICF_NOSCROLL);
    ListView_RedrawItems(g_hListView, 0, limit - 1);
    UpdateWindow(g_hListView);
}

void UpdateStatusBar(void)
{
    if (!g_hStatusBar) return;
    WCHAR txt[512];
    if (g_doc.hFile == INVALID_HANDLE_VALUE) wcscpy(txt, L"Готов. Откройте диск или файл.");
    else {
        // Форматируем размер (мб терабайты)
        double sizeMB = (double)g_doc.total_bytes.QuadPart / (1024*1024);
        wsprintfW(txt, L"%s | Размер: %.2f МБ | Режим: %s | Изменений: %u",
            g_doc.source_name, sizeMB,
            g_mode == MODE_VIEW ? L"Просмотр" : (g_mode == MODE_EDIT ? L"Правка" : L"Сравнение"),
            g_doc.edit_count);
    }
    SetWindowText(g_hStatusBar, txt);
}

void SwitchMode(APP_MODE newMode)
{
    g_mode = newMode;
    switch (newMode) {
    case MODE_VIEW:
    case MODE_COMPARE:
        EnableWindow(g_hBtnApply, FALSE); ShowWindow(g_hBtnApply, SW_HIDE);
        StopApplyTimer();
        g_doc.edit_count = 0; // Сбрасываем изменения при выходе
        break;
    case MODE_EDIT:
        EnableWindow(g_hBtnApply, TRUE); ShowWindow(g_hBtnApply, SW_SHOW);
        StartApplyTimer(g_hBtnApply);
        break;
    }
    UpdateListView(GetParent(g_hListView));
    UpdateStatusBar();
}

void StartApplyTimer(HWND hwnd) {
    g_applyTimer.remaining = 60;
    g_applyTimer.timerId = SetTimer(hwnd, IDC_TIMER, 1000, NULL);
    WCHAR txt[32]; wsprintfW(txt, L"Применить (%d)", g_applyTimer.remaining);
    SetWindowText(g_hBtnApply, txt);
}

void StopApplyTimer(void) {
    if (g_applyTimer.timerId) KillTimer(GetParent(g_hBtnApply), g_applyTimer.timerId);
    g_applyTimer.timerId = 0; g_applyTimer.remaining = 0;
    SetWindowText(g_hBtnApply, L"Применить");
}

void OnOpenDisk(HWND hwnd)
{
    WCHAR input[16] = L"0";
    if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DISK_DIALOG), hwnd, DiskDialogProc, (LPARAM)input) == IDOK) {
        if (DocOpenDisk(&g_doc, _wtoi(input))) { UpdateListView(hwnd); SwitchMode(MODE_VIEW); }
        else MessageBox(hwnd, L"Ошибка открытия!", L"Ошибка", MB_ICONERROR);
    }
}

void OnOpenFile(HWND hwnd)
{
    OPENFILENAMEW ofn = {0}; WCHAR file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFilter = L"Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        if (DocOpenFile(&g_doc, file)) { UpdateListView(hwnd); SwitchMode(MODE_VIEW); }
        else MessageBox(hwnd, L"Ошибка открытия!", L"Ошибка", MB_ICONERROR);
    }
}

void OnExportBackup(HWND hwnd)
{
    if (g_doc.hFile == INVALID_HANDLE_VALUE) return;
    OPENFILENAMEW ofn = {0}; WCHAR file[MAX_PATH] = L"backup.bin";
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFilter = L"Bin (*.bin)\0*.bin\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_OVERWRITEPROMPT;
    if (GetSaveFileNameW(&ofn)) {
        if (BackupExport(&g_doc, file)) MessageBox(hwnd, L"Экспорт завершен!", L"Готово", MB_OK);
        else MessageBox(hwnd, L"Ошибка экспорта!", L"Ошибка", MB_ICONERROR);
    }
}

void OnLoadBackup(HWND hwnd)
{
    if (g_doc.hFile == INVALID_HANDLE_VALUE) return;
    OPENFILENAMEW ofn = {0}; WCHAR file[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn); ofn.hwndOwner = hwnd; ofn.lpstrFilter = L"Bin (*.bin)\0*.bin\0";
    ofn.lpstrFile = file; ofn.nMaxFile = MAX_PATH; ofn.Flags = OFN_FILEMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        if (BackupLoad(&g_doc, file)) { UpdateListView(hwnd); UpdateStatusBar(); }
    }
}

void OnCompare(HWND hwnd) { if (g_doc.backup_loaded) SwitchMode(MODE_COMPARE); }
void OnRestoreAll(HWND hwnd) {
    if (!g_doc.backup_loaded) return;
    if (MessageBox(hwnd, L"Внимание! Это полностью заменит диск/файл бекапом. Продолжить?", L"Опасно", MB_YESNO) == IDYES) {
        if (BackupRestoreAll(&g_doc)) { SwitchMode(MODE_VIEW); MessageBox(hwnd, L"Восстановлено!", L"Ок", MB_OK); }
    }
}
void OnEditMode(HWND hwnd) { if (g_doc.hFile != INVALID_HANDLE_VALUE) SwitchMode(MODE_EDIT); }
void OnApply(HWND hwnd) {
    if (g_mode != MODE_EDIT) return;
    if (MessageBox(hwnd, L"Применить изменения к физическому файлу/диску?", L"Запись", MB_YESNO) == IDYES) {
        if (DocWriteAllEdits(&g_doc)) { SwitchMode(MODE_VIEW); MessageBox(hwnd, L"Записано!", L"Ок", MB_OK); }
    }
}

// --- ИСПРАВЛЕННЫЙ CUSTOM DRAW (Цвета и Отрисовка) ---
LRESULT HandleListViewCustomDraw(NMLVCUSTOMDRAW* plvcd)
{
    switch (plvcd->nmcd.dwDrawStage) {
    case CDDS_PREPAINT: return CDRF_NOTIFYITEMDRAW;
    case CDDS_ITEMPREPAINT:
        {
            LARGE_INTEGER off; off.QuadPart = plvcd->nmcd.dwItemSpec;
            BOOL is_dirty = FALSE;
            BYTE val = DocGetByte(&g_doc, off, &is_dirty);
            
            if (g_mode == MODE_COMPARE && g_doc.backup_loaded && off.QuadPart < g_doc.backup_size.QuadPart) {
                if (val != BackupGetByte(&g_doc, off)) {
                    plvcd->clrTextBk = COLOR_MISMATCH_BG; plvcd->clrText = COLOR_MISMATCH_TEXT;
                } else {
                    plvcd->clrTextBk = COLOR_MATCH_BG; plvcd->clrText = COLOR_MATCH_TEXT;
                }
            }
            else if (g_mode == MODE_EDIT && is_dirty) {
                plvcd->clrTextBk = COLOR_EDIT_BG; plvcd->clrText = RGB(0,0,0);
            }
            else {
                plvcd->clrTextBk = RGB(255,255,255); plvcd->clrText = RGB(0,0,0);
            }
            return CDRF_NEWFONT;
        }
    }
    return CDRF_DODEFAULT;
}

INT_PTR CALLBACK DiskDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static WCHAR* dNum;
    if (msg == WM_INITDIALOG) { dNum = (WCHAR*)lParam; SetDlgItemText(hwndDlg, IDC_DISK_NUMBER, L"0"); return FALSE; }
    if (msg == WM_COMMAND && LOWORD(wParam) == IDOK) { GetDlgItemText(hwndDlg, IDC_DISK_NUMBER, dNum, 16); EndDialog(hwndDlg, IDOK); return TRUE; }
    if (msg == WM_COMMAND && LOWORD(wParam) == IDCANCEL) { EndDialog(hwndDlg, IDCANCEL); return TRUE; }
    return FALSE;
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: InitGUI(hwnd); break;
    case WM_SIZE:
        {
            RECT rc; GetClientRect(hwnd, &rc);
            int sbH = 22; // Высота статусбара
            int btnH = 30; // Высота кнопки
            int pad = 10;
            int listH = rc.bottom - sbH - btnH - (pad * 3);
            
            if (g_hListView) SetWindowPos(g_hListView, NULL, pad, pad, rc.right - (pad*2), listH, SWP_NOZORDER);
            
            // Динамическая расстановка кнопок (привязаны к низу)
            int btnY = pad + listH + pad;
            int x = pad; int w = 90;
            
            HWND btns[] = { GetDlgItem(hwnd, IDC_BTN_OPEN_DISK), GetDlgItem(hwnd, IDC_BTN_OPEN_FILE), GetDlgItem(hwnd, IDC_BTN_EXPORT), 
                            GetDlgItem(hwnd, IDC_BTN_LOAD_BACKUP), GetDlgItem(hwnd, IDC_BTN_COMPARE), GetDlgItem(hwnd, IDC_BTN_RESTORE_ALL),
                            GetDlgItem(hwnd, IDC_BTN_EDIT_MODE), GetDlgItem(hwnd, IDC_BTN_APPLY) };
            
            for (int i=0; i<8; i++) {
                if (btns[i]) { SetWindowPos(btns[i], NULL, x, btnY, w, btnH, SWP_NOZORDER); x += w + 5; }
            }
            if (g_hStatusBar) SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
        }
        break;
        
    case WM_NOTIFY:
        {
            NMHDR* pnmh = (NMHDR*)lParam;
            if (pnmh->idFrom == IDC_LISTVIEW) {
                if (pnmh->code == NM_CUSTOMDRAW) return HandleListViewCustomDraw((NMLVCUSTOMDRAW*)lParam);
                if (pnmh->code == LVN_GETDISPINFO) {
                    LV_DISPINFO* pdi = (LV_DISPINFO*)lParam;
                    LARGE_INTEGER off; off.QuadPart = pdi->item.iItem;
                    if (off.QuadPart < g_doc.total_bytes.QuadPart) {
                        BOOL dirty = FALSE;
                        BYTE val = DocGetByte(&g_doc, off, &dirty);
                        if (pdi->item.mask & LVIF_TEXT) {
                            if (pdi->item.iSubItem == 0) wsprintfW(pdi->item.pszText, L"0x%08X", (DWORD)off.QuadPart);
                            else if (pdi->item.iSubItem == 1) ByteToHexStr(val, pdi->item.pszText);
                            else if (pdi->item.iSubItem == 2) {
                                if (g_doc.backup_loaded && off.QuadPart < g_doc.backup_size.QuadPart) ByteToHexStr(BackupGetByte(&g_doc, off), pdi->item.pszText);
                                else wcscpy(pdi->item.pszText, L"--");
                            }
                        }
                    }
                }
                if (pnmh->code == NM_DBLCLK && g_mode == MODE_EDIT) {
                    int iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                    if (iItem >= 0) CreateCellEditor(g_hListView, iItem, 1);
                }
            }
        }
        break;
        
    case WM_COMMAND:
        {
            switch (LOWORD(wParam)) {
            case IDC_BTN_OPEN_DISK: OnOpenDisk(hwnd); break;
            case IDC_BTN_OPEN_FILE: OnOpenFile(hwnd); break;
            case IDC_BTN_EXPORT: OnExportBackup(hwnd); break;
            case IDC_BTN_LOAD_BACKUP: OnLoadBackup(hwnd); break;
            case IDC_BTN_COMPARE: OnCompare(hwnd); break;
            case IDC_BTN_RESTORE_ALL: OnRestoreAll(hwnd); break;
            case IDC_BTN_EDIT_MODE: OnEditMode(hwnd); break;
            case IDC_BTN_APPLY: OnApply(hwnd); break;
            }
        }
        break;
        
    case WM_TIMER:
        if (wParam == IDC_TIMER && g_applyTimer.remaining > 0) {
            g_applyTimer.remaining--;
            WCHAR txt[32]; wsprintfW(txt, L"Применить (%d)", g_applyTimer.remaining);
            SetWindowText(g_hBtnApply, txt);
            if (g_applyTimer.remaining == 0) { StopApplyTimer(); SwitchMode(MODE_VIEW); MessageBox(hwnd, L"Таймаут!", L"Отмена", MB_OK); }
        }
        break;
        
    case WM_DESTROY: DocClose(&g_doc); PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
