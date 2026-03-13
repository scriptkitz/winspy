//
//	DisplayTreeInfo.c
//
//  Copyright (c) 2002 by J Brown 
//  Freeware
//
//	Populate the treeview control with the window hierarchy
//  of the currently selected window's top-level parent.
//

#define STRICT
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <tchar.h>
#include <commctrl.h>

#include "resource.h"
#include "WinSpy.h"

extern int  FormatWindowText(HWND hwnd, TCHAR szTotal[]);
extern void InitGlobalWindowTree(HWND hwndTree);

#define MAX_WINDOW_DEPTH 500

typedef struct
{
    HTREEITEM hRoot;
    HWND      hwnd;
} WinStackType;

static WinStackType WindowStack[MAX_WINDOW_DEPTH];
static int          nWindowZ = 0;
static HTREEITEM    hTreeLast;
static HWND         hwndLast;

static HWND         g_hwndTreeRoot = NULL; 
static BOOL         g_bIsBuilding = FALSE; 
static WNDPROC      g_OldEditProc = NULL;

// 子类化搜索框：截获回车键
static LRESULT CALLBACK SearchEditProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg)
    {
    case WM_GETDLGCODE:
        return DLGC_WANTALLKEYS;

    case WM_KEYDOWN:
        if (wParam == VK_RETURN)
        {
            SendMessage(GetParent(hwnd), WM_COMMAND, MAKEWPARAM(IDC_TREE_SEARCH, 1), (LPARAM)hwnd);
            return 0;
        }
        break;
    }
    return CallWindowProc(g_OldEditProc, hwnd, iMsg, wParam, lParam);
}

static BOOL CALLBACK TreeWindowProc(HWND hwnd, LPARAM lParam)
{
    HWND hwndTree = (HWND)lParam;
    TCHAR szTotal[MAX_WINTEXT_LEN + 100];
    TVINSERTSTRUCT tv;
    int idx;
    HWND hwndParent = GetParent(hwnd);
    UINT uStyle = GetWindowLong(hwnd, GWL_STYLE);

    idx = FormatWindowText(hwnd, szTotal);

    ZeroMemory(&tv, sizeof(tv));
    tv.hParent         = TVI_ROOT;
    tv.hInsertAfter    = TVI_LAST;
    tv.item.mask       = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    tv.item.pszText    = szTotal;
    tv.item.cchTextMax = lstrlen(szTotal);
    tv.item.lParam     = (LPARAM)hwnd;

    if (uStyle & WS_CHILD)
        tv.item.iImage = 3; 
    else if ((uStyle & WS_POPUPWINDOW) == WS_POPUPWINDOW)
        tv.item.iImage = 2; 
    else if (uStyle & WS_POPUP)
        tv.item.iImage = 4; 
    else
        tv.item.iImage = 1; 

    if (idx != -1)
        tv.item.iImage = idx;

    if (fShowDimmed && !IsWindowVisible(hwnd))
        tv.item.iImage += 35; 

    tv.item.iSelectedImage = tv.item.iImage;

    if (nWindowZ > 0 && hwndParent != WindowStack[nWindowZ - 1].hwnd)
    {
        if (GetParent(hwnd) == hwndLast)
        {
            WindowStack[nWindowZ].hRoot = hTreeLast;
            WindowStack[nWindowZ].hwnd  = hwndParent;

            if (nWindowZ < MAX_WINDOW_DEPTH - 1)
                nWindowZ++;

            tv.hParent = hTreeLast;
        }
        else
        {
            int i;
            for (i = 0; i < nWindowZ; i++)
            {
                if (WindowStack[i].hwnd == hwndParent)
                {
                    nWindowZ = i + 1;
                    tv.hParent = WindowStack[i].hRoot;
                    break;
                }
            }
        }
    }
    else if (nWindowZ > 0)
    {
        tv.hParent = WindowStack[nWindowZ - 1].hRoot;
    }

    hTreeLast = TreeView_InsertItem(hwndTree, &tv);
    hwndLast  = hwnd;

    return TRUE;
}

static HTREEITEM FindTreeItem(HWND hwndTree, HWND hwndTarget, HTREEITEM hItem)
{
    if (hItem == NULL)
        hItem = TreeView_GetRoot(hwndTree);

    while (hItem != NULL)
    {
        TVITEM item;
        item.hItem  = hItem;
        item.mask   = TVIF_PARAM;
        TreeView_GetItem(hwndTree, &item);

        if ((HWND)item.lParam == hwndTarget)
            return hItem;

        HTREEITEM hChild = TreeView_GetChild(hwndTree, hItem);
        if (hChild != NULL)
        {
            HTREEITEM hFound = FindTreeItem(hwndTree, hwndTarget, hChild);
            if (hFound != NULL)
                return hFound;
        }

        hItem = TreeView_GetNextSibling(hwndTree, hItem);
    }

    return NULL;
}

// 辅助函数：不修改原字符串的大小写匹配
static BOOL IsMatch(LPCTSTR szText, LPCTSTR szTerm)
{
    TCHAR szTextLower[256];
    TCHAR szTermLower[128];
    lstrcpyn(szTextLower, szText, 256);
    lstrcpyn(szTermLower, szTerm, 128);
    _tcslwr(szTextLower);
    _tcslwr(szTermLower);
    return _tcsstr(szTextLower, szTermLower) != NULL;
}

static HTREEITEM FindTreeItemByText(HWND hwndTree, LPCTSTR szText, HTREEITEM hStartItem)
{
    HTREEITEM hItem = hStartItem;
    
    if (hItem == NULL)
        hItem = TreeView_GetRoot(hwndTree);
    else
    {
        HTREEITEM hChild = TreeView_GetChild(hwndTree, hItem);
        if (hChild)
        {
            hItem = hChild;
        }
        else
        {
            HTREEITEM hNext = TreeView_GetNextSibling(hwndTree, hItem);
            if (hNext)
            {
                hItem = hNext;
            }
            else
            {
                while (hItem && !TreeView_GetNextSibling(hwndTree, hItem))
                    hItem = TreeView_GetParent(hwndTree, hItem);
                
                if (hItem)
                    hItem = TreeView_GetNextSibling(hwndTree, hItem);
            }
        }
    }

    while (hItem != NULL)
    {
        TCHAR szBuf[256];
        TVITEM item;
        item.hItem = hItem;
        item.mask = TVIF_TEXT;
        item.pszText = szBuf;
        item.cchTextMax = 256;
        TreeView_GetItem(hwndTree, &item);

        if (IsMatch(szBuf, szText))
            return hItem;

        HTREEITEM hChild = TreeView_GetChild(hwndTree, hItem);
        if (hChild)
        {
            hItem = hChild;
            continue;
        }

        HTREEITEM hNext = TreeView_GetNextSibling(hwndTree, hItem);
        if (hNext)
        {
            hItem = hNext;
            continue;
        }

        while (hItem && !TreeView_GetNextSibling(hwndTree, hItem))
            hItem = TreeView_GetParent(hwndTree, hItem);

        if (hItem)
            hItem = TreeView_GetNextSibling(hwndTree, hItem);
    }

    return NULL;
}

void DoSearch(HWND hwnd, BOOL bNext)
{
    HWND hwndTree = GetDlgItem(hwnd, IDC_TREE1);
    TCHAR szText[128];
    GetDlgItemText(hwnd, IDC_TREE_SEARCH, szText, 128);

    if (szText[0] == 0) return;

    HTREEITEM hStart = NULL;
    if (bNext)
        hStart = TreeView_GetSelection(hwndTree);

    HTREEITEM hFound = FindTreeItemByText(hwndTree, szText, hStart);
    
    if (!hFound && hStart)
        hFound = FindTreeItemByText(hwndTree, szText, NULL);

    if (hFound)
    {
        TVITEM item;
        item.hItem = hFound;
        item.mask = TVIF_PARAM;
        TreeView_GetItem(hwndTree, &item);

        g_bIsBuilding = TRUE;
        TreeView_SelectItem(hwndTree, hFound);
        TreeView_EnsureVisible(hwndTree, hFound);
        
        // 关键：显式同步更新全局选中的窗口信息
        if (item.lParam)
            DisplayWindowInfo((HWND)item.lParam);

        g_bIsBuilding = FALSE;
    }
}

void SetTreeInfo(HWND hwnd)
{
    HWND hwndTree = GetDlgItem(WinSpyTab[TREE_TAB].hwnd, IDC_TREE1);
    HWND hwndRoot;
    TVINSERTSTRUCT tv;
    TCHAR szTotal[MAX_WINTEXT_LEN + 100];
    int idx;

    if (hwnd == 0) return;

    hwndRoot = GetAncestor(hwnd, GA_ROOT);
    if (!hwndRoot) hwndRoot = hwnd;

    if (hwndRoot == g_hwndTreeRoot && TreeView_GetCount(hwndTree) > 0)
    {
        HTREEITEM hItem = FindTreeItem(hwndTree, hwnd, NULL);
        if (hItem)
        {
            g_bIsBuilding = TRUE; 
            TreeView_SelectItem(hwndTree, hItem);
            TreeView_EnsureVisible(hwndTree, hItem);
            g_bIsBuilding = FALSE;
        }
        return;
    }

    g_bIsBuilding = TRUE;
    g_hwndTreeRoot = hwndRoot;

    SendMessage(hwndTree, WM_SETREDRAW, FALSE, 0);
    TreeView_DeleteAllItems(hwndTree);

    nWindowZ = 0;
    hwndLast = NULL;
    hTreeLast = NULL;

    idx = FormatWindowText(hwndRoot, szTotal);
    ZeroMemory(&tv, sizeof(tv));
    tv.hParent         = TVI_ROOT;
    tv.hInsertAfter    = TVI_LAST;
    tv.item.mask       = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_STATE;
    tv.item.pszText    = szTotal;
    tv.item.state      = TVIS_EXPANDED;
    tv.item.stateMask  = TVIS_EXPANDED;
    tv.item.lParam     = (LPARAM)hwndRoot;
    tv.item.iImage     = 1; 
    if (idx != -1) tv.item.iImage = idx;
    tv.item.iSelectedImage = tv.item.iImage;

    hTreeLast = TreeView_InsertItem(hwndTree, &tv);
    hwndLast  = hwndRoot;

    WindowStack[0].hRoot = hTreeLast;
    WindowStack[0].hwnd  = hwndRoot;
    nWindowZ = 1;

    EnumChildWindows(hwndRoot, TreeWindowProc, (LPARAM)hwndTree);

    SendMessage(hwndTree, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(hwndTree, NULL, TRUE);

    HTREEITEM hItem = FindTreeItem(hwndTree, hwnd, NULL);
    if (hItem)
    {
        TreeView_SelectItem(hwndTree, hItem);
        TreeView_EnsureVisible(hwndTree, hItem);
    }

    g_bIsBuilding = FALSE;
}

LRESULT CALLBACK TreeDlgProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
    switch (iMsg)
    {
    case WM_INITDIALOG:
        {
            HWND hwndTree = GetDlgItem(hwnd, IDC_TREE1);
            HWND hwndMainTree = GetDlgItem(GetParent(hwnd), IDC_TREE1);
            HIMAGELIST hImg = TreeView_GetImageList(hwndMainTree, TVSIL_NORMAL);
            if (hImg)
            {
                TreeView_SetImageList(hwndTree, hImg, TVSIL_NORMAL);
            }

            HWND hwndEdit = GetDlgItem(hwnd, IDC_TREE_SEARCH);
            g_OldEditProc = (WNDPROC)SetWindowLongPtr(hwndEdit, GWLP_WNDPROC, (LONG_PTR)SearchEditProc);
        }
        return TRUE;

    case WM_SIZE:
        {
            int cx = LOWORD(lParam);
            int cy = HIWORD(lParam);
            HWND hwndEdit = GetDlgItem(hwnd, IDC_TREE_SEARCH);
            HWND hwndTree = GetDlgItem(hwnd, IDC_TREE1);

            MoveWindow(hwndEdit, 35, 7, cx - 42, 14, TRUE);
            MoveWindow(hwndTree, 0, 25, cx, cy - 25, TRUE);
        }
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_TREE_SEARCH)
        {
            if (HIWORD(wParam) == EN_CHANGE)
            {
                DoSearch(hwnd, FALSE);
            }
            else if (HIWORD(wParam) == 1) 
            {
                DoSearch(hwnd, TRUE);
            }
            return TRUE;
        }
        break;

    case WM_NOTIFY:
        {
            NMHDR *hdr = (NMHDR *)lParam;
            if (hdr->idFrom == IDC_TREE1)
            {
                if (hdr->code == TVN_SELCHANGED)
                {
                    if (!g_bIsBuilding)
                    {
                        LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                        if (pnmtv->itemNew.lParam)
                            DisplayWindowInfo((HWND)pnmtv->itemNew.lParam);
                    }
                }
                else if (hdr->code == NM_RCLICK)
                {
                    TVHITTESTINFO hti;
                    HTREEITEM hItem;
                    POINT pt;
                    GetCursorPos(&pt);
                    hti.pt = pt;
                    ScreenToClient(hdr->hwndFrom, &hti.pt);
                    hItem = TreeView_HitTest(hdr->hwndFrom, &hti);
                    if (hItem && (hti.flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT)))
                    {
                        TVITEM tvi;
                        tvi.mask = TVIF_HANDLE | TVIF_PARAM;
                        tvi.hItem = hItem;
                        TreeView_GetItem(hdr->hwndFrom, &tvi);

                        HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_MENU3));
                        HMENU hPopup = GetSubMenu(hMenu, 0);

                        InsertMenu(hPopup, 0, MF_BYPOSITION | MF_STRING, IDC_FLASH, _T("&Highlight Window\tDouble-Click"));
                        InsertMenu(hPopup, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);

                        WinSpy_SetupPopupMenu(hPopup, (HWND)tvi.lParam);
                        UINT uCmd = TrackPopupMenu(hPopup, TPM_RIGHTBUTTON | TPM_RETURNCMD, pt.x, pt.y, 0, GetParent(hwnd), 0);
                        
                        if (uCmd == IDC_FLASH)
                        {
                            FlashWindowBorder((HWND)tvi.lParam, TRUE);
                        }
                        else
                        {
                            WinSpy_PopupCommandHandler(GetParent(hwnd), uCmd, (HWND)tvi.lParam);
                        }

                        DestroyMenu(hMenu);
                    }
                }
                else if (hdr->code == NM_DBLCLK)
                {
                    TVHITTESTINFO hti;
                    HTREEITEM hItem;
                    POINT pt;
                    GetCursorPos(&pt);
                    hti.pt = pt;
                    ScreenToClient(hdr->hwndFrom, &hti.pt);
                    hItem = TreeView_HitTest(hdr->hwndFrom, &hti);
                    if (hItem && (hti.flags & (TVHT_ONITEM | TVHT_ONITEMRIGHT | TVHT_ONITEMICON)))
                    {
                        TVITEM tvi;
                        tvi.mask = TVIF_PARAM;
                        tvi.hItem = hItem;
                        TreeView_GetItem(hdr->hwndFrom, &tvi);
                        if (tvi.lParam)
                            FlashWindowBorder((HWND)tvi.lParam, TRUE);
                    }
                }
            }
        }
        return TRUE;
    }

    return FALSE;
}
