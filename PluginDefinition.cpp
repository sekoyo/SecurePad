//this file is part of notepad++
//Copyright (C)2003 Don HO <donho@altern.org>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "menuCmdID.h"

//
// Globals
//
HINSTANCE hInstance = NULL;
unsigned char cryptkey[MAX_CRYPT_KEY];

//
// Fwd decl
//
void EncryptDoc() { CryptDoc(Encrypt); }
void DecryptDoc() { CryptDoc(Decrypt); }
void EncryptSelection() { CryptSelection(Encrypt); }
void DecryptSelection() { CryptSelection(Decrypt); }
LRESULT CALLBACK DlgProcCryptKey(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM lParam);
void StrCrypt(unsigned char *buf1, unsigned long buf1len, unsigned char *buf2, unsigned long buf2len, unsigned char *key, CryptAction action);
std::wstring widen(const std::string& str);
std::string narrow(const std::wstring& str);

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{
    hInstance = (HINSTANCE)hModule;
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{

    //--------------------------------------------//
    //-- STEP 3. CUSTOMIZE YOUR PLUGIN COMMANDS --//
    //--------------------------------------------//
    // with function :
    // setCommand(int index,                      // zero based number to indicate the order of command
    //            TCHAR *commandName,             // the command name that you want to see in plugin menu
    //            PFUNCPLUGINCMD functionPointer, // the symbol of function (function pointer) associated with this command. The body should be defined below. See Step 4.
    //            ShortcutKey *shortcut,          // optional. Define a shortcut to trigger this command
    //            bool check0nInit                // optional. Make this menu item be checked visually
    //            );
    setCommand(0, TEXT("Encrypt Document"), EncryptDoc, NULL, false);
    setCommand(1, TEXT("Decrypt Document"), DecryptDoc, NULL, false);
    setCommand(2, TEXT("Encrypt Selected Text"), EncryptSelection, NULL, false);
    setCommand(3, TEXT("Decrypt Selected Text"), DecryptSelection, NULL, false);
    setCommand(4, TEXT("About SecurePad"), AboutDlg, NULL, false);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}


//
// This function help you to initialize your plugin commands
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//
void CryptDoc(CryptAction action)
{
    int currentEdit = -1;
    HWND hCurrentEditView = NULL;
    ULONG textLength = 0L, textLengthOut = 0L;
    TCHAR *textBuf = NULL, *textBufOut = NULL;

    // Prompt for crypt key
    INT_PTR ret = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DLGKEY), nppData._nppHandle, reinterpret_cast<DLGPROC>(DlgProcCryptKey));

    if(ret == 0)
    {
        return;
    }

    // Get edit view handle
    SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    if(currentEdit == 0) hCurrentEditView = nppData._scintillaMainHandle;
    else hCurrentEditView = nppData._scintillaSecondHandle;
    
    // Get document text length
    textLength = (ULONG)SendMessage(hCurrentEditView, SCI_GETTEXTLENGTH, 0, 0);

    if(textLength == 0)
    {
        MessageBox(nppData._nppHandle, TEXT("The document is empty!"), TEXT("Error"), MB_OK);
        return;
    }

    textLength++;
    textLengthOut = textLength;

    if(action == Encrypt) textLengthOut *= 2;

    // Create input buffer  (round up to nearest 8 multiplier)
    while(textLength % 8 != 0)textLength++;
    textBuf = (TCHAR*)VirtualAlloc(NULL, textLength, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    // Create output buffer (round up to nearest 8 multiplier)
    while(textLengthOut % 8 != 0)textLengthOut++;
    textBufOut = (TCHAR*)VirtualAlloc(NULL, textLengthOut, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    // Get text
    SendMessage(hCurrentEditView, SCI_GETTEXT, textLength, (LPARAM)textBuf);

    // Crypt the text
    if(action == Encrypt)
        StrCrypt((unsigned char*)textBuf, textLength, (unsigned char*)textBufOut, textLengthOut, (unsigned char*)cryptkey, Encrypt);
    else
        StrCrypt((unsigned char*)textBuf, textLength, (unsigned char*)textBufOut, textLengthOut, (unsigned char*)cryptkey, Decrypt);

    // Output to view
    SendMessage(hCurrentEditView, SCI_SETTEXT, 0, (LPARAM)textBufOut);

    // Cleanup
    if(textBuf)
    {
        VirtualFree(textBuf, 0, MEM_RELEASE);
    }
    if (textBufOut)
    {
        VirtualFree(textBufOut, 0, MEM_RELEASE);
    }
}

// SCI haven't updated SCI_GETTEXTRANGE to be unicode yet (unicode is very "new")
struct _tSci_TextRange {
    struct Sci_CharacterRange chrg;
    TCHAR *lpstrText;
};

void CryptSelection(CryptAction action)
{
    int currentEdit = -1;
    HWND hCurrentEditView = NULL;
    ULONG textLength = 0L, textLengthOut = 0L;
    TCHAR *textBuf = NULL, *textBufOut = NULL;

    // Prompt for crypt key
    INT_PTR ret = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DLGKEY), nppData._nppHandle, reinterpret_cast<DLGPROC>(DlgProcCryptKey));

    if(ret == 0)
    {
        return;
    }

    // Get edit view handle
    SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    if(currentEdit == 0) hCurrentEditView = nppData._scintillaMainHandle;
    else hCurrentEditView = nppData._scintillaSecondHandle;

    // Get selected text length
    long selectStart = (long)SendMessage(hCurrentEditView, SCI_GETSELECTIONSTART, 0, 0);
    long selectEnd = (long)SendMessage(hCurrentEditView, SCI_GETSELECTIONEND, 0, 0);

    if(selectEnd == 0 || (selectEnd - selectStart) == 0)
    {
        MessageBox(nppData._nppHandle, TEXT("No text was selected!"), TEXT("Error"), MB_OK);
        return;
    }

    textLength = (selectEnd - selectStart); textLength++;
    textLengthOut = textLength;
    if(action == Encrypt) textLengthOut *= 2;

    // Create input buffer  (round up to nearest 8 multiplier)
    while(textLength % 8 != 0)textLength++;
    textBuf = (TCHAR*)VirtualAlloc(NULL, textLength, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    // Create output buffer (round up to nearest 8 multiplier)
    while(textLengthOut % 8 != 0)textLengthOut++;
    textBufOut = (TCHAR*)VirtualAlloc(NULL, textLengthOut, MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    // Get text range
    struct TextRange tr = {{selectStart, selectEnd}, reinterpret_cast<char*>(textBuf)};
    SendMessage(hCurrentEditView, SCI_GETTEXTRANGE, 0, (LPARAM)&tr);

    // Crypt the text
    if(action == Encrypt)
        StrCrypt((unsigned char*)textBuf, textLength, (unsigned char*)textBufOut, textLengthOut, (unsigned char*)cryptkey, Encrypt);
    else
        StrCrypt((unsigned char*)textBuf, textLength, (unsigned char*)textBufOut, textLengthOut, (unsigned char*)cryptkey, Decrypt);
    
    // Output to view
    SendMessage(hCurrentEditView, SCI_REPLACESEL, 0, (LPARAM)textBufOut);

    // Cleanup
    if (textBuf)
    {
        VirtualFree(textBuf, 0, MEM_RELEASE);
    }
    if (textBufOut)
    {
        VirtualFree(textBufOut, 0, MEM_RELEASE);
    }
}

void AboutDlg()
{
    ::MessageBox(NULL, TEXT("SecurePad can be used to securely encrypt plaintext documents with a key of your choice. Be careful, once encrypted you will only be able to decrypt with the key you used!\r\n\r\nAny questions please visit www.dominictobias.com."), TEXT("SecurePad v2.2"), MB_OK);
}

// ===============================================
//  Dialog for key entry
// ===============================================

LRESULT CALLBACK DlgProcCryptKey(HWND hWndDlg, UINT msg, WPARAM wParam, LPARAM /*lParam*/)
{
    switch(msg)
    {
        case WM_DESTROY:
        case WM_CLOSE:
        {
            EndDialog(hWndDlg, 0);
            return TRUE;
        }

        case WM_INITDIALOG:
        {
            SetFocus(GetDlgItem(hWndDlg, IDC_EDIT1));
            break;
        }

        case WM_COMMAND:
        {
            if(LOWORD(wParam) == IDOK)
            {
                TCHAR box1[MAX_CRYPT_KEY] = { 0 };
                TCHAR box2[MAX_CRYPT_KEY] = { 0 };
                LRESULT keyLen = 0;

                SendMessage(GetDlgItem(hWndDlg, IDC_EDIT1), WM_GETTEXT, sizeof(box1), (LPARAM)box1);
                SendMessage(GetDlgItem(hWndDlg, IDC_EDIT2), WM_GETTEXT, sizeof(box2), (LPARAM)box2);

                if(_tcslen(box1) == 0 || _tcslen(box2) == 0)
                {
                    MessageBox(nppData._nppHandle, TEXT("Both fields must be filled in!"), TEXT("Please try again"), MB_OK);
                    break;
                }

                if(_tcscmp(box1, box2))
                {
                    MessageBox(nppData._nppHandle, TEXT("The keys did not match!"), TEXT("Please try again"), MB_OK);
                    break;
                }

                keyLen = SendMessage(GetDlgItem(hWndDlg, IDC_EDIT1), WM_GETTEXTLENGTH, 0, 0);

                if(keyLen > MAX_CRYPT_KEY)
                {
                    TCHAR buf[128];
                    _stprintf(buf, TEXT("The key was too long! (key=%Id, max=%d)\0"), keyLen, MAX_CRYPT_KEY);
                    MessageBox(nppData._nppHandle, buf, TEXT("Please try again"), MB_OK);
                    break;
                }

                if(keyLen < MIN_CRYPT_KEY)
                {
                    TCHAR buf[128];
                    _stprintf(buf, TEXT("The key was too short! (key=%Id, min=%d)\0"), keyLen, MIN_CRYPT_KEY);
                    MessageBox(nppData._nppHandle, buf, TEXT("Please try again"), MB_OK);
                    break;
                }

                // Convert the key to narrow ANSII format
                memset(&cryptkey, 0, sizeof(cryptkey));
                std::string narrowKey = narrow(box1);
                memcpy(cryptkey, narrowKey.c_str(), narrowKey.size() > MAX_CRYPT_KEY ? MAX_CRYPT_KEY : narrowKey.size());

                EndDialog(hWndDlg, 1);
            }

            if(LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hWndDlg, 0);
                return TRUE;
            }

            break;
        }
    }

    return FALSE;
}

// ===============================================
//  Blowfish cypher
// ===============================================

//Function to convert unsigned char to string of length 2
void Char2Hex(const unsigned char ch, char* szHex)
{
    unsigned char byte[2];
    byte[0] = ch/16;
    byte[1] = ch%16;
    for(int i=0; i<2; i++)
    {
        if(byte[i] >= 0 && byte[i] <= 9)
            szHex[i] = '0' + byte[i];
        else
            szHex[i] = 'A' + byte[i] - 10;
    }
    szHex[2] = 0;
}

//Function to convert string of length 2 to unsigned char
void Hex2Char(const char* szHex, unsigned char& rch)
{
    rch = 0;
    for(int i=0; i<2; i++)
    {
        if(*(szHex + i) >='0' && *(szHex + i) <= '9')
            rch = (rch << 4) + (*(szHex + i) - '0');
        else if(*(szHex + i) >='A' && *(szHex + i) <= 'F')
            rch = (rch << 4) + (*(szHex + i) - 'A' + 10);
        else
            break;
    }
}    

//Function to convert string of unsigned chars to string of chars
void CharStr2HexStr(const unsigned char* pucCharStr, char* pszHexStr, unsigned long lSize)
{
    unsigned long i;
    char szHex[2];
    pszHexStr[0] = 0;
    for(i=0; i<lSize; i++)
    {
        Char2Hex(pucCharStr[i], szHex);
        strcat(pszHexStr, szHex);
    }
}

//Function to convert string of chars to string of unsigned chars
void HexStr2CharStr(const char* pszHexStr, unsigned char* pucCharStr, unsigned long lSize)
{
    unsigned long i;
    unsigned char ch;
    for(i=0; i<lSize; i++)
    {
        Hex2Char(pszHexStr+2*i, ch);
        pucCharStr[i] = ch;
    }
}

void StrCrypt(unsigned char *buf1, unsigned long buf1len, unsigned char *buf2, unsigned long buf2len, unsigned char *key, CryptAction action)
{
    CBlowFish *BlowFish = new CBlowFish(key, strlen((char*)key));

    if(action == Encrypt)
    {
        BlowFish->Encrypt(buf1, buf1, buf1len, CBlowFish::ECB);
        CharStr2HexStr(buf1, (char*)buf2, buf1len);
    }

    else if(action == Decrypt)
    {
        HexStr2CharStr((char*)buf1, buf2, buf2len/2);
        BlowFish->Decrypt(buf2, buf2, buf2len, CBlowFish::ECB);
    }

    delete BlowFish;
};

// ===============================================
//  Helper funcs
// ===============================================

std::wstring widen(const std::string& str)
{
	std::wostringstream wstm;
	const std::ctype<wchar_t>& ctfacet =
		std::use_facet< std::ctype<wchar_t> >(wstm.getloc());
    for( size_t i=0 ; i<str.size() ; ++i ) 
              wstm << ctfacet.widen( str[i] ) ;
    return wstm.str() ;
}

std::string narrow(const std::wstring& str)
{
	std::ostringstream stm;
	const std::ctype<wchar_t>& ctfacet =
		std::use_facet< std::ctype<wchar_t> >(stm.getloc());
    for( size_t i=0 ; i<str.size() ; ++i ) 
                  stm << ctfacet.narrow( str[i], 0 ) ;
    return stm.str() ;
}