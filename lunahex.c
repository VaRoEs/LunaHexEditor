/*
 * Luna Hex Editor - Special Hex Editor for Windows (32/64 bit)
 * Built with MSYS2 MinGW-w64 GCC:
 * gcc -DUNICODE -D_UNICODE -Wall -o lunahex64.exe 1.c -lcomctl32 -lcomdlg32 -mwindows
 * 
 * ==========================================
 * MANIFEST FILE (lunahex.exe.manifest)
 * ==========================================
 * <?xml version="1.0" encoding="UTF-8" standalone="yes"?>
 * <assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
 * <assemblyIdentity version="1.0.0.0" processorArchitecture="*" name="LunaHexEditor" type="win32" />
 * <description>Luna Hex Editor</description>
 * <dependency>
 *     <dependentAssembly>
 *         <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*" />
 *     </dependentAssembly>
 * </dependency>
 * </assembly>
 * ==========================================
 */

// Определение недостающих типов для TCC/MinGW
#ifndef DECLSPEC_IMPORT
#define DECLSPEC_IMPORT __declspec(dllimport)
#endif

// ============ ИНКЛУДЫ ============
#include <windows.h>
#include <winioctl.h>
#include <commctrl.h>
#include <stdio.h>
#include <commdlg.h>

// Включаем визуальные стили Luna
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")

// === ВЫРЕЗАЕМ ВЕСЬ ВЕСОМЫЙ WINIOCTL.H И ВСТАВЛЯЕМ ТОЛЬКО ТО, ЧТО НАМ НУЖНО ===
#ifndef IOCTL_DISK_GET_LENGTH_INFO
#define IOCTL_DISK_GET_LENGTH_INFO 0x0007405C
typedef struct _GET_LENGTH_INFORMATION {
    LARGE_INTEGER Length;
} GET_LENGTH_INFORMATION, *PGET_LENGTH_INFORMATION;
#endif
// ================================================================

// ============ МАКРОСЫ И КОНСТАНТЫ ============
// GUI IDs
#define IDC_LISTVIEW        1001
#define IDC_BTN_OPEN_DISK   1002
#define IDC_BTN_OPEN_FILE   1003
#define IDC_BTN_EXPORT      1004
#define IDC_BTN_LOAD_BACKUP 1005
#define IDC_BTN_COMPARE     1006
#define IDC_BTN_RESTORE_ALL 1007
#define IDC_BTN_EDIT_MODE   1008
#define IDC_BTN_APPLY       1009
#define IDC_BTN_REFRESH     1010
#define IDC_STATUSBAR       1011
#define IDC_EDIT_HEX        1012
#define IDC_TIMER           1013

// Dummy Resource IDs
#ifndef IDD_DISK_DIALOG
#define IDD_DISK_DIALOG     2000
#define IDC_DISK_SPIN       2001
#define IDC_DISK_NUMBER     2002
#endif

// Цвета для подсветки
#define COLOR_MISMATCH_BG   RGB(255, 200, 200)
#define COLOR_MISMATCH_TEXT RGB(200, 0, 0)
#define COLOR_MATCH_BG      RGB(200, 255, 200)
#define COLOR_MATCH_TEXT    RGB(0, 150, 0)
#define COLOR_EDIT_BG       RGB(255, 255, 200)

// ============ СТРУКТУРЫ И ТИПЫ ============
typedef struct tagBYTECELL
{
    DWORD   index;
    BYTE    value;
    BYTE    backup_value;
    BOOL    is_dirty;
    BOOL    is_mismatch;
} BYTECELL;

typedef struct tagDOCUMENT
{
    BYTECELL    *cells;
    DWORD       total_bytes;
    HANDLE      hFile;
    BOOL        is_disk;
    BOOL        is_modified;
    WCHAR       source_name[MAX_PATH];
    WCHAR       backup_path[MAX_PATH];
    BOOL        backup_loaded;
    DWORD       disk_number;
} DOCUMENT;

typedef enum {
    MODE_VIEW,
    MODE_EDIT,
    MODE_COMPARE
} APP_MODE;

typedef struct {
    UINT_PTR timerId;
    int remaining;
    HWND hwndButton;
} APPLY_TIMER;

typedef struct {
    HWND hEdit;
    HWND hListView;
    int itemIndex;
    int subItem;
    WCHAR originalText[3];
} EDITCELLINFO;

// ============ ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ============
HINSTANCE       g_hInst = NULL;
HWND            g_hMainWnd = NULL;
DOCUMENT        g_doc = {0};
APP_MODE        g_mode = MODE_VIEW;
APPLY_TIMER     g_applyTimer = {0, 0, NULL};
HWND            g_hListView = NULL;
HWND            g_hStatusBar = NULL;
HWND            g_hBtnApply = NULL;
EDITCELLINFO    g_editInfo;
WNDPROC         OldEditProc = NULL;

// ============ ПРОТОТИПЫ ФУНКЦИЙ ============
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void CheckAdminRights(void);

BOOL DiskOpen(DOCUMENT* doc, DWORD diskNumber, BOOL readOnly);
BOOL DiskRead(DOCUMENT* doc, DWORD maxBytes);
BOOL DiskWriteByte(DOCUMENT* doc, DWORD index);
BOOL DiskWriteAllDirty(DOCUMENT* doc);
BOOL DiskWriteAll(DOCUMENT* doc, BYTE value);
void DiskClose(DOCUMENT* doc);

BOOL FileOpen(DOCUMENT* doc, const WCHAR* path, BOOL readOnly);
BOOL FileRead(DOCUMENT* doc, DWORD maxBytes);
BOOL FileWriteByte(DOCUMENT* doc, DWORD index);
BOOL FileWriteAllDirty(DOCUMENT* doc);
void FileClose(DOCUMENT* doc);

BOOL DocRead(DOCUMENT* doc, DWORD maxBytes);
BOOL DocWriteByte(DOCUMENT* doc, DWORD index);
BOOL DocWriteAllDirty(DOCUMENT* doc);
void DocClose(DOCUMENT* doc);
void DocFree(DOCUMENT* doc);

BOOL BackupExport(const DOCUMENT* doc, const WCHAR* path);
BOOL BackupLoad(DOCUMENT* doc, const WCHAR* path);
void BackupCompare(DOCUMENT* doc);
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
void OnOpenDisk(HWND hwnd);
void OnOpenFile(HWND hwnd);
void OnExportBackup(HWND hwnd);
void OnLoadBackup(HWND hwnd);
void OnCompare(HWND hwnd);
void OnRestoreAll(HWND hwnd);
void OnEditMode(HWND hwnd);
void OnApply(HWND hwnd);

HWND CreateCellEditor(HWND hListView, int itemIndex, int subItem);
LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ApplyEdit(HWND hEdit);
void CancelEdit(HWND hEdit);
void EditNextCell(int nextIndex);
INT_PTR CALLBACK DiskDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT HandleListViewCustomDraw(NMLVCUSTOMDRAW* plvcd);

void GoToAddress(HWND hwnd, DWORD address);
BOOL FindHexPattern(const BYTE* pattern, DWORD patternSize);
BOOL GetSelectedRange(DWORD* start, DWORD* end);
BOOL CopySelectedToClipboard(HWND hwnd);

// ==========================================================
//                     РЕАЛИЗАЦИЯ
// ==========================================================

// --- ТОЧКА ВХОДА ---
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    g_hInst = hInstance;
    
    // Инициализация Common Controls (только нужные классы для стабильности)
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
    
    if (!RegisterClassEx(&wc))
    {
        MessageBox(NULL, L"Window Registration Failed!", L"Error", MB_ICONERROR);
        return 1;
    }
    
    g_hMainWnd = CreateWindowEx(
        0,
        L"LunaHexEditorClass",
        L"Luna Hex Editor v1.0",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
        NULL, NULL, hInstance, NULL);
    
    if (!g_hMainWnd)
    {
        MessageBox(NULL, L"Window Creation Failed!", L"Error", MB_ICONERROR);
        return 1;
    }
    
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
    
    if (AllocateAndInitializeSid(&NtAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &pAdminGroup))
    {
        CheckTokenMembership(NULL, pAdminGroup, &bIsAdmin);
        FreeSid(pAdminGroup);
    }
    
    if (!bIsAdmin)
    {
        MessageBox(g_hMainWnd, 
            L"Внимание! Для работы с дисками требуются права администратора.\n"
            L"Без прав администратора доступен только режим работы с файлами.",
            L"Luna Hex Editor - Предупреждение",
            MB_ICONWARNING | MB_OK);
    }
}

// --- ДИСКОВЫЕ ОПЕРАЦИИ ---
BOOL DiskOpen(DOCUMENT* doc, DWORD diskNumber, BOOL readOnly)
{
    WCHAR devicePath[64];
    wsprintfW(devicePath, L"\\\\.\\PhysicalDrive%d", diskNumber);
    
    DWORD access = GENERIC_READ;
    if (!readOnly) access |= GENERIC_WRITE;
    
    doc->hFile = CreateFileW(devicePath, access, 
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
        OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
    
    if (doc->hFile == INVALID_HANDLE_VALUE)
    {
        doc->hFile = CreateFileW(devicePath, access,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
            OPEN_EXISTING, 0, NULL);
        
        if (doc->hFile == INVALID_HANDLE_VALUE)
            return FALSE;
    }
    
    doc->is_disk = TRUE;
    doc->disk_number = diskNumber;
    doc->total_bytes = 0;
    doc->cells = NULL;
    doc->backup_loaded = FALSE;
    doc->is_modified = FALSE;
    wsprintfW(doc->source_name, L"PhysicalDrive%d", diskNumber);
    
    return TRUE;
}

BOOL DiskRead(DOCUMENT* doc, DWORD maxBytes)
{
    if (!doc->is_disk || doc->hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    GET_LENGTH_INFORMATION lengthInfo;
    DWORD bytesReturned;
    
    if (!DeviceIoControl(doc->hFile, IOCTL_DISK_GET_LENGTH_INFO,
        NULL, 0, &lengthInfo, sizeof(lengthInfo), &bytesReturned, NULL))
    {
        LARGE_INTEGER fileSize;
        if (!GetFileSizeEx(doc->hFile, &fileSize))
            return FALSE;
        lengthInfo.Length.QuadPart = fileSize.QuadPart;
    }
    
    DWORD totalSize = (DWORD)min(lengthInfo.Length.QuadPart, maxBytes);
    if (totalSize == 0) totalSize = maxBytes;
    
    doc->cells = (BYTECELL*)HeapAlloc(GetProcessHeap(), 
        HEAP_ZERO_MEMORY, totalSize * sizeof(BYTECELL));
    
    if (!doc->cells) return FALSE;
    
    const DWORD SECTOR_SIZE = 512;
    BYTE* sectorBuffer = (BYTE*)HeapAlloc(GetProcessHeap(), 
        HEAP_ZERO_MEMORY, SECTOR_SIZE);
    
    if (!sectorBuffer)
    {
        HeapFree(GetProcessHeap(), 0, doc->cells);
        return FALSE;
    }
    
    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    SetFilePointerEx(doc->hFile, offset, NULL, FILE_BEGIN);
    
    for (DWORD i = 0; i < totalSize; i += SECTOR_SIZE)
    {
        DWORD toRead = min(SECTOR_SIZE, totalSize - i);
        DWORD readBytes;
        
        if (ReadFile(doc->hFile, sectorBuffer, SECTOR_SIZE, &readBytes, NULL) && readBytes > 0)
        {
            for (DWORD j = 0; j < min(readBytes, toRead); j++)
            {
                doc->cells[i + j].index = i + j + 1;
                doc->cells[i + j].value = sectorBuffer[j];
                doc->cells[i + j].backup_value = 0;
                doc->cells[i + j].is_dirty = FALSE;
                doc->cells[i + j].is_mismatch = FALSE;
            }
        }
        else
        {
            for (DWORD j = 0; j < toRead; j++)
            {
                doc->cells[i + j].index = i + j + 1;
                doc->cells[i + j].value = 0;
                doc->cells[i + j].backup_value = 0;
                doc->cells[i + j].is_dirty = FALSE;
                doc->cells[i + j].is_mismatch = FALSE;
            }
        }
    }
    
    HeapFree(GetProcessHeap(), 0, sectorBuffer);
    doc->total_bytes = totalSize;
    
    return TRUE;
}

BOOL DiskWriteByte(DOCUMENT* doc, DWORD index)
{
    if (!doc->is_disk || doc->hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    if (index == 0 || index > doc->total_bytes)
        return FALSE;
    
    DWORD offset = index - 1;
    
    DWORD sectorOffset = offset % 512;
    DWORD sectorStart = offset - sectorOffset;
    
    BYTE sectorBuffer[512];
    LARGE_INTEGER liSector;
    liSector.QuadPart = sectorStart;
    
    SetFilePointerEx(doc->hFile, liSector, NULL, FILE_BEGIN);
    DWORD bytesRead;
    
    if (!ReadFile(doc->hFile, sectorBuffer, 512, &bytesRead, NULL))
        return FALSE;
    
    sectorBuffer[sectorOffset] = doc->cells[index - 1].value;
    
    SetFilePointerEx(doc->hFile, liSector, NULL, FILE_BEGIN);
    DWORD bytesWritten;
    
    if (!WriteFile(doc->hFile, sectorBuffer, 512, &bytesWritten, NULL))
        return FALSE;
    
    doc->cells[index - 1].is_dirty = FALSE;
    
    return TRUE;
}

BOOL DiskWriteAllDirty(DOCUMENT* doc)
{
    if (!doc->is_disk) return FALSE;
    
    for (DWORD i = 0; i < doc->total_bytes; i++)
    {
        if (doc->cells[i].is_dirty)
        {
            if (!DiskWriteByte(doc, i + 1))
                return FALSE;
        }
    }
    doc->is_modified = FALSE;
    return TRUE;
}

BOOL DiskWriteAll(DOCUMENT* doc, BYTE value)
{
    if (!doc->is_disk) return FALSE;
    
    LARGE_INTEGER offset;
    offset.QuadPart = 0;
    SetFilePointerEx(doc->hFile, offset, NULL, FILE_BEGIN);
    
    const DWORD SECTOR_SIZE = 512;
    BYTE sectorBuffer[512];
    memset(sectorBuffer, value, SECTOR_SIZE);
    
    for (DWORD i = 0; i < doc->total_bytes; i += SECTOR_SIZE)
    {
        DWORD bytesWritten;
        if (!WriteFile(doc->hFile, sectorBuffer, SECTOR_SIZE, &bytesWritten, NULL))
            return FALSE;
    }
    
    for (DWORD i = 0; i < doc->total_bytes; i++)
    {
        doc->cells[i].value = value;
        doc->cells[i].is_dirty = FALSE;
    }
    
    return TRUE;
}

void DiskClose(DOCUMENT* doc)
{
    if (doc->hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(doc->hFile);
        doc->hFile = INVALID_HANDLE_VALUE;
    }
}

// --- ФАЙЛОВЫЕ ОПЕРАЦИИ ---
BOOL FileOpen(DOCUMENT* doc, const WCHAR* path, BOOL readOnly)
{
    DWORD access = GENERIC_READ;
    if (!readOnly) access |= GENERIC_WRITE;
    
    doc->hFile = CreateFileW(path, access, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (doc->hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    doc->is_disk = FALSE;
    doc->total_bytes = 0;
    doc->cells = NULL;
    doc->backup_loaded = FALSE;
    doc->is_modified = FALSE;
    wcscpy_s(doc->source_name, MAX_PATH, path);
    
    return TRUE;
}

BOOL FileRead(DOCUMENT* doc, DWORD maxBytes)
{
    if (doc->is_disk || doc->hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    LARGE_INTEGER fileSize;
    GetFileSizeEx(doc->hFile, &fileSize);
    
    DWORD totalSize = (DWORD)min(fileSize.QuadPart, maxBytes);
    if (totalSize == 0) return FALSE;
    
    doc->cells = (BYTECELL*)HeapAlloc(GetProcessHeap(),
        HEAP_ZERO_MEMORY, totalSize * sizeof(BYTECELL));
    
    if (!doc->cells) return FALSE;
    
    SetFilePointer(doc->hFile, 0, NULL, FILE_BEGIN);
    
    BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), 0, totalSize);
    if (!buffer)
    {
        HeapFree(GetProcessHeap(), 0, doc->cells);
        return FALSE;
    }
    
    DWORD bytesRead;
    if (!ReadFile(doc->hFile, buffer, totalSize, &bytesRead, NULL))
    {
        HeapFree(GetProcessHeap(), 0, buffer);
        HeapFree(GetProcessHeap(), 0, doc->cells);
        return FALSE;
    }
    
    for (DWORD i = 0; i < bytesRead; i++)
    {
        doc->cells[i].index = i + 1;
        doc->cells[i].value = buffer[i];
        doc->cells[i].backup_value = 0;
        doc->cells[i].is_dirty = FALSE;
        doc->cells[i].is_mismatch = FALSE;
    }
    
    doc->total_bytes = bytesRead;
    HeapFree(GetProcessHeap(), 0, buffer);
    
    return TRUE;
}

BOOL FileWriteByte(DOCUMENT* doc, DWORD index)
{
    if (doc->is_disk || doc->hFile == INVALID_HANDLE_VALUE)
        return FALSE;
    
    if (index == 0 || index > doc->total_bytes)
        return FALSE;
    
    DWORD offset = index - 1;
    SetFilePointer(doc->hFile, offset, NULL, FILE_BEGIN);
    
    BYTE value = doc->cells[index - 1].value;
    DWORD bytesWritten;
    
    if (!WriteFile(doc->hFile, &value, 1, &bytesWritten, NULL))
        return FALSE;
    
    doc->cells[index - 1].is_dirty = FALSE;
    return TRUE;
}

BOOL FileWriteAllDirty(DOCUMENT* doc)
{
    if (doc->is_disk) return FALSE;
    
    for (DWORD i = 0; i < doc->total_bytes; i++)
    {
        if (doc->cells[i].is_dirty)
        {
            if (!FileWriteByte(doc, i + 1))
                return FALSE;
        }
    }
    doc->is_modified = FALSE;
    return TRUE;
}

void FileClose(DOCUMENT* doc)
{
    if (doc->hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(doc->hFile);
        doc->hFile = INVALID_HANDLE_VALUE;
    }
}

// --- УНИВЕРСАЛЬНЫЕ ФУНКЦИИ ---
BOOL DocRead(DOCUMENT* doc, DWORD maxBytes)
{
    if (doc->is_disk)
        return DiskRead(doc, maxBytes);
    else
        return FileRead(doc, maxBytes);
}

BOOL DocWriteByte(DOCUMENT* doc, DWORD index)
{
    if (doc->is_disk)
        return DiskWriteByte(doc, index);
    else
        return FileWriteByte(doc, index);
}

BOOL DocWriteAllDirty(DOCUMENT* doc)
{
    if (doc->is_disk)
        return DiskWriteAllDirty(doc);
    else
        return FileWriteAllDirty(doc);
}

void DocClose(DOCUMENT* doc)
{
    if (doc->is_disk)
        DiskClose(doc);
    else
        FileClose(doc);
    
    if (doc->cells)
    {
        HeapFree(GetProcessHeap(), 0, doc->cells);
        doc->cells = NULL;
    }
    doc->total_bytes = 0;
}

void DocFree(DOCUMENT* doc)
{
    DocClose(doc);
    ZeroMemory(doc, sizeof(DOCUMENT));
}

// --- РАБОТА С БЕКАПОМ ---
BOOL BackupExport(const DOCUMENT* doc, const WCHAR* path)
{
    if (!doc->cells || doc->total_bytes == 0)
        return FALSE;
    
    FILE* f = _wfopen(path, L"w, ccs=UTF-8");
    if (!f) return FALSE;
    
    fwprintf(f, L"# LUNA_HEX_BACKUP\n");
    fwprintf(f, L"# SOURCE: %s\n", doc->source_name);
    fwprintf(f, L"# TOTAL_BYTES: %u\n", doc->total_bytes);
    fwprintf(f, L"# DATE: %S\n", __DATE__);
    fwprintf(f, L"# HUMAN_READABLE_FORMAT\n\n");
    
    for (DWORD i = 0; i < doc->total_bytes; i++)
    {
        fwprintf(f, L"%05u: %02X\n", 
            doc->cells[i].index, 
            doc->cells[i].value);
    }
    
    fclose(f);
    return TRUE;
}

BOOL BackupLoad(DOCUMENT* doc, const WCHAR* path)
{
    if (!doc->cells) return FALSE;
    
    FILE* f = _wfopen(path, L"r, ccs=UTF-8");
    if (!f) return FALSE;
    
    WCHAR line[256];
    DWORD loaded = 0;
    
    while (fgetws(line, 256, f) && loaded < doc->total_bytes)
    {
        if (line[0] == L'#' || line[0] == L'\n' || line[0] == L'\r')
            continue;
        
        DWORD index;
        DWORD value;
        
        if (swscanf(line, L"%u: %x", &index, &value) == 2)
        {
            if (index > 0 && index <= doc->total_bytes)
            {
                doc->cells[index - 1].backup_value = (BYTE)value;
                loaded++;
            }
        }
    }
    
    fclose(f);
    wcscpy_s(doc->backup_path, MAX_PATH, path);
    doc->backup_loaded = (loaded > 0);
    
    return doc->backup_loaded;
}

void BackupCompare(DOCUMENT* doc)
{
    if (!doc->backup_loaded) return;
    
    for (DWORD i = 0; i < doc->total_bytes; i++)
    {
        doc->cells[i].is_mismatch = 
            (doc->cells[i].value != doc->cells[i].backup_value);
    }
}

BOOL BackupRestoreAll(DOCUMENT* doc)
{
    if (!doc->backup_loaded || !doc->cells) return FALSE;
    
    for (DWORD i = 0; i < doc->total_bytes; i++)
    {
        doc->cells[i].value = doc->cells[i].backup_value;
        doc->cells[i].is_dirty = TRUE;
    }
    
    doc->is_modified = TRUE;
    return DocWriteAllDirty(doc);
}

// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---
BOOL IsValidHexChar(WCHAR c)
{
    return ((c >= L'0' && c <= L'9') ||
            (c >= L'A' && c <= L'F') ||
            (c >= L'a' && c <= L'f'));
}

BYTE HexCharToByte(WCHAR c)
{
    if (c >= L'0' && c <= L'9') return c - L'0';
    if (c >= L'A' && c <= L'F') return c - L'A' + 10;
    if (c >= L'a' && c <= L'f') return c - L'a' + 10;
    return 0;
}

void ByteToHexStr(BYTE b, WCHAR* out)
{
    static const WCHAR hex[] = L"0123456789ABCDEF";
    out[0] = hex[b >> 4];
    out[1] = hex[b & 0x0F];
    out[2] = 0;
}

// --- GUI: ФУНКЦИИ ОКНА РЕДАКТИРОВАНИЯ И LISTVIEW ---
HWND CreateCellEditor(HWND hListView, int itemIndex, int subItem)
{
    RECT rcSubItem;
    ListView_GetSubItemRect(hListView, itemIndex, subItem, LVIR_BOUNDS, &rcSubItem);
    
    rcSubItem.left += 2;
    rcSubItem.top += 1;
    rcSubItem.bottom -= 1;
    
    HWND hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_UPPERCASE | ES_CENTER,
        rcSubItem.left, rcSubItem.top,
        rcSubItem.right - rcSubItem.left,
        rcSubItem.bottom - rcSubItem.top,
        hListView, NULL, g_hInst, NULL);
    
    if (hEdit)
    {
        HFONT hFont = (HFONT)SendMessage(hListView, WM_GETFONT, 0, 0);
        SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(hEdit, EM_SETLIMITTEXT, 2, 0);
        
        WCHAR hexStr[3];
        ByteToHexStr(g_doc.cells[itemIndex].value, hexStr);
        SetWindowText(hEdit, hexStr);
        SendMessage(hEdit, EM_SETSEL, 0, -1);
        
        g_editInfo.hEdit = hEdit;
        g_editInfo.hListView = hListView;
        g_editInfo.itemIndex = itemIndex;
        g_editInfo.subItem = subItem;
        wcscpy(g_editInfo.originalText, hexStr);
        
        OldEditProc = (WNDPROC)GetWindowLongPtr(hEdit, GWLP_WNDPROC);
        SetWindowLongPtr(hEdit, GWLP_USERDATA, (LONG_PTR)OldEditProc);
        SetWindowLongPtr(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubclassProc);
    }
    
    return hEdit;
}

LRESULT CALLBACK EditSubclassProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    WNDPROC oldProc = (WNDPROC)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (msg)
    {
    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_RETURN:
            ApplyEdit(hwnd);
            return 0;
            
        case VK_ESCAPE:
            CancelEdit(hwnd);
            return 0;
            
        case VK_TAB:
            ApplyEdit(hwnd);
            EditNextCell(g_editInfo.itemIndex + 1);
            return 0;
        }
        break;
        
    case WM_CHAR:
        if (!IsValidHexChar((WCHAR)wParam) && wParam != VK_BACK)
            return 0;
        break;
        
    case WM_KILLFOCUS:
        ApplyEdit(hwnd);
        return 0;
    }
    
    if (oldProc)
        return CallWindowProc(oldProc, hwnd, msg, wParam, lParam);
    else
        return DefWindowProc(hwnd, msg, wParam, lParam);
}

void ApplyEdit(HWND hEdit)
{
    WCHAR hexStr[3];
    GetWindowText(hEdit, hexStr, 3);
    
    if (wcslen(hexStr) == 1)
    {
        hexStr[1] = hexStr[0];
        hexStr[0] = L'0';
        hexStr[2] = 0;
    }
    
    if (wcslen(hexStr) == 2 && IsValidHexChar(hexStr[0]) && IsValidHexChar(hexStr[1]))
    {
        BYTE newValue = (HexCharToByte(hexStr[0]) << 4) | HexCharToByte(hexStr[1]);
        int iItem = g_editInfo.itemIndex;
        
        if (iItem >= 0 && iItem < (int)g_doc.total_bytes)
        {
            g_doc.cells[iItem].value = newValue;
            g_doc.cells[iItem].is_dirty = TRUE;
            g_doc.is_modified = TRUE;
            
            ListView_RedrawItems(g_editInfo.hListView, iItem, iItem);
            UpdateWindow(g_editInfo.hListView);
        }
    }
    
    DestroyWindow(hEdit);
    g_editInfo.hEdit = NULL;
}

void CancelEdit(HWND hEdit)
{
    DestroyWindow(hEdit);
    g_editInfo.hEdit = NULL;
}

void EditNextCell(int nextIndex)
{
    if (nextIndex >= 0 && nextIndex < (int)g_doc.total_bytes)
    {
        ListView_EnsureVisible(g_editInfo.hListView, nextIndex, FALSE);
        ListView_SetItemState(g_editInfo.hListView, nextIndex, 
            LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        CreateCellEditor(g_editInfo.hListView, nextIndex, g_editInfo.subItem);
    }
}

// --- GUI: ГЛАВНЫЕ ФУНКЦИИ И ЛОГИКА ---
void InitGUI(HWND hwnd)
{
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);
    
    g_hListView = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, L"",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | 
        LVS_SHOWSELALWAYS | LVS_OWNERDATA,
        10, 10, rcClient.right - 20, rcClient.bottom - 80,
        hwnd, (HMENU)IDC_LISTVIEW, g_hInst, NULL);
    
    ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    
    // ЖЕСТКОЕ ЗАНУЛЕНИЕ МУСОРА
    LVCOLUMN lvc = {0}; 
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    
    lvc.iSubItem = 0;
    lvc.pszText = L"№";
    lvc.cx = 80;
    ListView_InsertColumn(g_hListView, 0, &lvc);
    
    lvc.iSubItem = 1;
    lvc.pszText = L"Hex";
    lvc.cx = 100;
    ListView_InsertColumn(g_hListView, 1, &lvc);
    
    lvc.iSubItem = 2;
    lvc.pszText = L"BackUp Hex";
    lvc.cx = 100;
    ListView_InsertColumn(g_hListView, 2, &lvc);
    
    CreateWindowEx(0, L"BUTTON", L"Открыть диск", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, rcClient.bottom - 60, 100, 25, hwnd, (HMENU)IDC_BTN_OPEN_DISK, g_hInst, NULL);
    
    CreateWindowEx(0, L"BUTTON", L"Открыть файл", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        115, rcClient.bottom - 60, 100, 25, hwnd, (HMENU)IDC_BTN_OPEN_FILE, g_hInst, NULL);
    
    CreateWindowEx(0, L"BUTTON", L"Экспорт", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        220, rcClient.bottom - 60, 80, 25, hwnd, (HMENU)IDC_BTN_EXPORT, g_hInst, NULL);
    
    CreateWindowEx(0, L"BUTTON", L"Загрузить бекап", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        305, rcClient.bottom - 60, 110, 25, hwnd, (HMENU)IDC_BTN_LOAD_BACKUP, g_hInst, NULL);
    
    CreateWindowEx(0, L"BUTTON", L"Сравнить", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        420, rcClient.bottom - 60, 80, 25, hwnd, (HMENU)IDC_BTN_COMPARE, g_hInst, NULL);
    
    CreateWindowEx(0, L"BUTTON", L"Исправить всё", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        505, rcClient.bottom - 60, 100, 25, hwnd, (HMENU)IDC_BTN_RESTORE_ALL, g_hInst, NULL);
    
    CreateWindowEx(0, L"BUTTON", L"Режим правки", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        610, rcClient.bottom - 60, 100, 25, hwnd, (HMENU)IDC_BTN_EDIT_MODE, g_hInst, NULL);
    
    g_hBtnApply = CreateWindowEx(0, L"BUTTON", L"Применить", WS_CHILD | BS_PUSHBUTTON,
        715, rcClient.bottom - 60, 120, 25, hwnd, (HMENU)IDC_BTN_APPLY, g_hInst, NULL);
    
    g_hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, L"", WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, hwnd, (HMENU)IDC_STATUSBAR, g_hInst, NULL);
    
    UpdateStatusBar();
}

void UpdateListView(HWND hwnd)
{
    if (!g_hListView || !g_doc.cells) return;
    ListView_SetItemCountEx(g_hListView, g_doc.total_bytes, LVSICF_NOSCROLL);
    ListView_RedrawItems(g_hListView, 0, g_doc.total_bytes - 1);
    UpdateWindow(g_hListView);
}

void UpdateStatusBar(void)
{
    if (!g_hStatusBar) return;
    
    WCHAR statusText[512];
    
    if (!g_doc.cells || g_doc.total_bytes == 0)
    {
        wcscpy(statusText, L"Готов. Откройте диск или файл.");
    }
    else
    {
        wsprintfW(statusText, L"%s | Всего байт: %u | Режим: %s | Бекап: %s",
            g_doc.source_name,
            g_doc.total_bytes,
            g_mode == MODE_VIEW ? L"Просмотр" :
            g_mode == MODE_EDIT ? L"Правка" : L"Сравнение",
            g_doc.backup_loaded ? L"Загружен" : L"Не загружен");
    }
    
    SetWindowText(g_hStatusBar, statusText);
}

void SwitchMode(APP_MODE newMode)
{
    g_mode = newMode;
    
    switch (newMode)
    {
    case MODE_VIEW:
        EnableWindow(g_hBtnApply, FALSE);
        ShowWindow(g_hBtnApply, SW_HIDE);
        StopApplyTimer();
        if (g_doc.cells)
        {
            for (DWORD i = 0; i < g_doc.total_bytes; i++)
                g_doc.cells[i].is_dirty = FALSE;
        }
        break;
        
    case MODE_EDIT:
        EnableWindow(g_hBtnApply, TRUE);
        ShowWindow(g_hBtnApply, SW_SHOW);
        StartApplyTimer(g_hBtnApply);
        break;
        
    case MODE_COMPARE:
        EnableWindow(g_hBtnApply, FALSE);
        ShowWindow(g_hBtnApply, SW_HIDE);
        StopApplyTimer();
        break;
    }
    
    UpdateListView(GetParent(g_hListView));
    UpdateStatusBar();
}

void StartApplyTimer(HWND hwnd)
{
    g_applyTimer.remaining = 20;
    g_applyTimer.hwndButton = g_hBtnApply;
    g_applyTimer.timerId = SetTimer(hwnd, IDC_TIMER, 1000, NULL);
    
    WCHAR btnText[32];
    wsprintfW(btnText, L"Применить (%d)", g_applyTimer.remaining);
    SetWindowText(g_hBtnApply, btnText);
}

void StopApplyTimer(void)
{
    if (g_applyTimer.timerId)
    {
        KillTimer(GetParent(g_hBtnApply), g_applyTimer.timerId);
        g_applyTimer.timerId = 0;
        g_applyTimer.remaining = 0;
    }
    SetWindowText(g_hBtnApply, L"Применить");
}

void OnOpenDisk(HWND hwnd)
{
    WCHAR input[16] = L"0";
    
    if (DialogBoxParam(g_hInst, MAKEINTRESOURCE(IDD_DISK_DIALOG), 
        hwnd, DiskDialogProc, (LPARAM)input) == IDOK)
    {
        DWORD diskNum = _wtoi(input);
        
        DocClose(&g_doc);
        
        if (DiskOpen(&g_doc, diskNum, FALSE))
        {
            if (DiskRead(&g_doc, 1024 * 1024))
            {
                UpdateListView(hwnd);
                SwitchMode(MODE_VIEW);
            }
            else
            {
                MessageBox(hwnd, L"Ошибка чтения диска!", L"Ошибка", MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(hwnd, L"Не удалось открыть диск. Проверьте права администратора.", L"Ошибка", MB_ICONERROR);
        }
    }
}

void OnOpenFile(HWND hwnd)
{
    OPENFILENAMEW ofn = {0};
    WCHAR fileName[MAX_PATH] = L"";
    
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Все файлы (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameW(&ofn))
    {
        DocClose(&g_doc);
        
        if (FileOpen(&g_doc, fileName, FALSE))
        {
            if (FileRead(&g_doc, 10 * 1024 * 1024))
            {
                UpdateListView(hwnd);
                SwitchMode(MODE_VIEW);
            }
            else
            {
                MessageBox(hwnd, L"Ошибка чтения файла!", L"Ошибка", MB_ICONERROR);
            }
        }
        else
        {
            MessageBox(hwnd, L"Не удалось открыть файл!", L"Ошибка", MB_ICONERROR);
        }
    }
}

void OnExportBackup(HWND hwnd)
{
    if (!g_doc.cells || g_doc.total_bytes == 0)
    {
        MessageBox(hwnd, L"Нет данных для экспорта!", L"Ошибка", MB_ICONERROR);
        return;
    }
    
    OPENFILENAMEW ofn = {0};
    WCHAR fileName[MAX_PATH] = L"backup.txt";
    
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Текстовые файлы (*.txt)\0*.txt\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"txt";
    
    if (GetSaveFileNameW(&ofn))
    {
        if (BackupExport(&g_doc, fileName))
            MessageBox(hwnd, L"Бекап успешно экспортирован!", L"Готово", MB_ICONINFORMATION);
        else
            MessageBox(hwnd, L"Ошибка экспорта бекапа!", L"Ошибка", MB_ICONERROR);
    }
}

void OnLoadBackup(HWND hwnd)
{
    if (!g_doc.cells || g_doc.total_bytes == 0)
    {
        MessageBox(hwnd, L"Сначала откройте диск или файл!", L"Ошибка", MB_ICONERROR);
        return;
    }
    
    OPENFILENAMEW ofn = {0};
    WCHAR fileName[MAX_PATH] = L"";
    
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = L"Текстовые файлы (*.txt)\0*.txt\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
    
    if (GetOpenFileNameW(&ofn))
    {
        if (BackupLoad(&g_doc, fileName))
        {
            UpdateListView(hwnd);
            MessageBox(hwnd, L"Бекап успешно загружен!", L"Готово", MB_ICONINFORMATION);
        }
        else
        {
            MessageBox(hwnd, L"Ошибка загрузки бекапа!", L"Ошибка", MB_ICONERROR);
        }
    }
}

void OnCompare(HWND hwnd)
{
    if (!g_doc.backup_loaded)
    {
        MessageBox(hwnd, L"Сначала загрузите бекап!", L"Ошибка", MB_ICONERROR);
        return;
    }
    
    BackupCompare(&g_doc);
    SwitchMode(MODE_COMPARE);
}

void OnRestoreAll(HWND hwnd)
{
    if (!g_doc.backup_loaded)
    {
        MessageBox(hwnd, L"Сначала загрузите бекап!", L"Ошибка", MB_ICONERROR);
        return;
    }
    
    if (MessageBox(hwnd, 
        L"ВНИМАНИЕ! Все данные будут заменены на данные из бекапа!\n"
        L"Это действие нельзя отменить. Продолжить?",
        L"Подтверждение",
        MB_YESNO | MB_ICONWARNING) == IDYES)
    {
        if (BackupRestoreAll(&g_doc))
        {
            MessageBox(hwnd, L"Все данные восстановлены из бекапа!", L"Готово", MB_ICONINFORMATION);
            UpdateListView(hwnd);
            SwitchMode(MODE_VIEW);
        }
        else
        {
            MessageBox(hwnd, L"Ошибка восстановления!", L"Ошибка", MB_ICONERROR);
        }
    }
}

void OnEditMode(HWND hwnd)
{
    if (!g_doc.cells || g_doc.total_bytes == 0)
    {
        MessageBox(hwnd, L"Нет данных для редактирования!", L"Ошибка", MB_ICONERROR);
        return;
    }
    
    SwitchMode(MODE_EDIT);
}

void OnApply(HWND hwnd)
{
    if (g_mode != MODE_EDIT) return;
    
    if (MessageBox(hwnd,
        L"Применить все изменения?\n"
        L"Изменения будут записаны на диск/в файл.",
        L"Подтверждение",
        MB_YESNO | MB_ICONQUESTION) == IDYES)
    {
        if (DocWriteAllDirty(&g_doc))
        {
            MessageBox(hwnd, L"Изменения успешно применены!", L"Готово", MB_ICONINFORMATION);
            SwitchMode(MODE_VIEW);
        }
        else
        {
            MessageBox(hwnd, L"Ошибка записи изменений!", L"Ошибка", MB_ICONERROR);
        }
    }
}

// --- GUI: ДОПОЛНЕНИЯ ---
void GoToAddress(HWND hwnd, DWORD address)
{
    if (!g_doc.cells || address == 0 || address > g_doc.total_bytes)
        return;
    
    int itemIndex = address - 1;
    ListView_EnsureVisible(g_hListView, itemIndex, FALSE);
    ListView_SetItemState(g_hListView, itemIndex, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
}

BOOL FindHexPattern(const BYTE* pattern, DWORD patternSize)
{
    if (!g_doc.cells || patternSize == 0 || patternSize > g_doc.total_bytes)
        return FALSE;
    
    for (DWORD i = 0; i <= g_doc.total_bytes - patternSize; i++)
    {
        BOOL found = TRUE;
        for (DWORD j = 0; j < patternSize; j++)
        {
            if (g_doc.cells[i + j].value != pattern[j])
            {
                found = FALSE;
                break;
            }
        }
        if (found)
        {
            GoToAddress(g_hListView, i + 1);
            return TRUE;
        }
    }
    return FALSE;
}

BOOL GetSelectedRange(DWORD* start, DWORD* end)
{
    int iStart = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
    if (iStart < 0) return FALSE;
    
    *start = iStart + 1;
    int iEnd = iStart;
    int i = iStart;
    
    while ((i = ListView_GetNextItem(g_hListView, i, LVNI_SELECTED)) >= 0)
        iEnd = i;
    
    *end = iEnd + 1;
    return TRUE;
}

BOOL CopySelectedToClipboard(HWND hwnd)
{
    DWORD start, end;
    if (!GetSelectedRange(&start, &end)) return FALSE;
    
    DWORD count = end - start + 1;
    DWORD bufferSize = count * 3 + 256;
    
    WCHAR* buffer = (WCHAR*)HeapAlloc(GetProcessHeap(), 0, bufferSize * sizeof(WCHAR));
    if (!buffer) return FALSE;
    
    int offset = 0;
    offset += wsprintfW(buffer + offset, L"Hex data from %s [%u-%u]:\n", g_doc.source_name, start, end);
    
    for (DWORD i = start - 1; i < end; i++)
    {
        WCHAR hexStr[4];
        ByteToHexStr(g_doc.cells[i].value, hexStr);
        offset += wsprintfW(buffer + offset, L"%s ", hexStr);
        if ((i - start + 2) % 16 == 0) offset += wsprintfW(buffer + offset, L"\n");
    }
    buffer[offset] = 0;
    
    if (OpenClipboard(hwnd))
    {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (offset + 1) * sizeof(WCHAR));
        if (hMem)
        {
            WCHAR* pMem = (WCHAR*)GlobalLock(hMem);
            if (pMem)
            {
                wcscpy(pMem, buffer);
                GlobalUnlock(hMem);
                SetClipboardData(CF_UNICODETEXT, hMem);
            }
        }
        CloseClipboard();
    }
    HeapFree(GetProcessHeap(), 0, buffer);
    return TRUE;
}

// --- GUI: ИСПРАВЛЕННЫЙ CUSTOM DRAW (БЕЗ ВЫЛЕТОВ!) ---
LRESULT HandleListViewCustomDraw(NMLVCUSTOMDRAW* plvcd)
{
    switch (plvcd->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        // ВОТ ОНО - ИСПРАВЛЕНИЕ: Мы просто возвращаем значение (никаких SetWindowLongPtr)
        return CDRF_NOTIFYITEMDRAW;
        
    case CDDS_ITEMPREPAINT:
        {
            DWORD itemIndex = plvcd->nmcd.dwItemSpec;
            
            if (itemIndex < g_doc.total_bytes)
            {
                BYTECELL* cell = &g_doc.cells[itemIndex];
                
                if (g_mode == MODE_COMPARE)
                {
                    if (cell->is_mismatch)
                    {
                        plvcd->clrTextBk = COLOR_MISMATCH_BG;
                        plvcd->clrText = COLOR_MISMATCH_TEXT;
                    }
                    else
                    {
                        plvcd->clrTextBk = COLOR_MATCH_BG;
                        plvcd->clrText = COLOR_MATCH_TEXT;
                    }
                }
                else if (g_mode == MODE_EDIT && cell->is_dirty)
                {
                    plvcd->clrTextBk = COLOR_EDIT_BG;
                    plvcd->clrText = RGB(0, 0, 0);
                }
                else
                {
                    plvcd->clrTextBk = RGB(255, 255, 255);
                    plvcd->clrText = RGB(0, 0, 0);
                }
            }
            return CDRF_NEWFONT;
        }
    }
    return CDRF_DODEFAULT;
}

// --- GUI: ДИАЛОГ ДИСКА ---
INT_PTR CALLBACK DiskDialogProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static WCHAR* diskNumber;
    switch (msg)
    {
    case WM_INITDIALOG:
        diskNumber = (WCHAR*)lParam;
        HWND hSpin = GetDlgItem(hwndDlg, IDC_DISK_SPIN);
        SendMessage(hSpin, UDM_SETRANGE, 0, MAKELPARAM(99, 0));
        SendMessage(hSpin, UDM_SETPOS, 0, 0);
        SetDlgItemTextW(hwndDlg, IDC_DISK_NUMBER, L"0");
        SetFocus(GetDlgItem(hwndDlg, IDC_DISK_NUMBER));
        return FALSE;
        
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDOK:
            GetDlgItemTextW(hwndDlg, IDC_DISK_NUMBER, diskNumber, 16);
            EndDialog(hwndDlg, IDOK);
            return TRUE;
            
        case IDCANCEL:
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

// --- GUI: ОБРАБОТЧИК ГЛАВНОГО ОКНА ---
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        InitGUI(hwnd);
        break;
        
    case WM_SIZE:
        {
            RECT rcClient;
            GetClientRect(hwnd, &rcClient);
            if (g_hListView)
            {
                SetWindowPos(g_hListView, NULL, 10, 10,
                    rcClient.right - 20, rcClient.bottom - 80,
                    SWP_NOZORDER);
            }
            if (g_hStatusBar)
            {
                SendMessage(g_hStatusBar, WM_SIZE, 0, 0);
            }
        }
        break;
        
    case WM_NOTIFY:
        {
            NMHDR* pnmh = (NMHDR*)lParam;
            
            if (pnmh->idFrom == IDC_LISTVIEW)
            {
                if (pnmh->code == NM_CUSTOMDRAW)
                {
                    return HandleListViewCustomDraw((NMLVCUSTOMDRAW*)lParam);
                }

                switch (pnmh->code)
                {
                case LVN_GETDISPINFO:
                    {
                        LV_DISPINFO* pdi = (LV_DISPINFO*)lParam;
                        if (pdi->item.iItem >= 0 && pdi->item.iItem < (int)g_doc.total_bytes)
                        {
                            BYTECELL* cell = &g_doc.cells[pdi->item.iItem];
                            if (pdi->item.mask & LVIF_TEXT)
                            {
                                switch (pdi->item.iSubItem)
                                {
                                case 0:
                                    wsprintfW(pdi->item.pszText, L"%u", cell->index);
                                    break;
                                case 1:
                                    ByteToHexStr(cell->value, pdi->item.pszText);
                                    break;
                                case 2:
                                    if (g_doc.backup_loaded)
                                        ByteToHexStr(cell->backup_value, pdi->item.pszText);
                                    else
                                        wcscpy(pdi->item.pszText, L"--");
                                    break;
                                }
                            }
                            if (g_mode == MODE_COMPARE && pdi->item.mask & LVIF_STATE)
                            {
                                if (cell->is_mismatch)
                                    pdi->item.state = INDEXTOSTATEIMAGEMASK(1);
                                else
                                    pdi->item.state = INDEXTOSTATEIMAGEMASK(2);
                            }
                        }
                    }
                    break;
                    
                case NM_DBLCLK:
                    {
                        if (g_mode == MODE_EDIT)
                        {
                            int iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                            if (iItem >= 0)
                            {
                                CreateCellEditor(g_hListView, iItem, 1);
                            }
                        }
                    }
                    break;
                }
            }
        }
        break;
        
    case WM_COMMAND:
        {
            WORD id = LOWORD(wParam);
            WORD code = HIWORD(wParam);
            
            switch (id)
            {
            case IDC_BTN_OPEN_DISK: OnOpenDisk(hwnd); break;
            case IDC_BTN_OPEN_FILE: OnOpenFile(hwnd); break;
            case IDC_BTN_EXPORT: OnExportBackup(hwnd); break;
            case IDC_BTN_LOAD_BACKUP: OnLoadBackup(hwnd); break;
            case IDC_BTN_COMPARE: OnCompare(hwnd); break;
            case IDC_BTN_RESTORE_ALL: OnRestoreAll(hwnd); break;
            case IDC_BTN_EDIT_MODE: OnEditMode(hwnd); break;
            case IDC_BTN_APPLY: OnApply(hwnd); break;
            case IDC_EDIT_HEX:
                if (code == EN_KILLFOCUS)
                {
                    HWND hEdit = (HWND)lParam;
                    WCHAR hexStr[3];
                    GetWindowText(hEdit, hexStr, 3);
                    
                    if (wcslen(hexStr) == 2 && IsValidHexChar(hexStr[0]) && IsValidHexChar(hexStr[1]))
                    {
                        int iItem = ListView_GetNextItem(g_hListView, -1, LVNI_SELECTED);
                        if (iItem >= 0)
                        {
                            BYTE newValue = (HexCharToByte(hexStr[0]) << 4) | HexCharToByte(hexStr[1]);
                            g_doc.cells[iItem].value = newValue;
                            g_doc.cells[iItem].is_dirty = TRUE;
                            g_doc.is_modified = TRUE;
                            ListView_RedrawItems(g_hListView, iItem, iItem);
                        }
                    }
                    DestroyWindow(hEdit);
                }
                break;
            }
        }
        break;
        
    case WM_TIMER:
        if (wParam == IDC_TIMER)
        {
            if (g_applyTimer.remaining > 0)
            {
                g_applyTimer.remaining--;
                WCHAR btnText[32];
                wsprintfW(btnText, L"Применить (%d)", g_applyTimer.remaining);
                SetWindowText(g_hBtnApply, btnText);
                
                if (g_applyTimer.remaining == 0)
                {
                    StopApplyTimer();
                    SwitchMode(MODE_VIEW);
                    MessageBox(hwnd, L"Время редактирования истекло. Изменения отменены.", L"Таймаут", MB_ICONINFORMATION);
                }
            }
        }
        break;
        
    case WM_DESTROY:
        DocClose(&g_doc);
        PostQuitMessage(0);
        break;
        
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    
    return 0;
}