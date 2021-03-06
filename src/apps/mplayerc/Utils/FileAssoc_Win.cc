#include "StdAfx.h"
#include "FileAssoc_Win.h"
#include "../svplib/SVPToolBox.h"
#include "../MediaFormats.h"
#include "../mplayerc.h"
#include "../resource.h"
#include "../PPageFormats.h"
#include "../ChkDefPlayerControlBar.h"
#include "..\Controller\PlayerPreference.h"
#include "..\Controller\SPlayerDefs.h"

//////////////////////////////////////////////////////////////////////////
//
//  !!!! WARNING !!!! WARNING !!!! WARNING !!!! WARNING !!!! WARNING !!!!
//
//  This class is created based on PPageFormats.cpp, and is highly
//  disorganized. Further development may require coding style fix
//  and clean up beforehand.
//
//  This class won't be portable.
//
//////////////////////////////////////////////////////////////////////////

CComPtr<IApplicationAssociationRegistration> g_paar;

static wchar_t g_registered_appname[] = L"����Ӱ��������";
static wchar_t g_oldassoc[] 	        = L"PreviousRegistration";
static wchar_t g_registered_key[]	    = L"Software\\Clients\\Media\\����Ӱ��������\\Capabilities";

bool f_setContextFiles  = true;
bool f_setAssociatedWithIcon  = true;

static struct {LPCSTR verb, cmd; UINT action;} handlers[] =
{
  {"VideoFiles", " %1", IDS_AUTOPLAY_PLAYVIDEO},
  {"MusicFiles", " %1", IDS_AUTOPLAY_PLAYMUSIC},
  {"CDAudio", " %1 /cd ", IDS_AUTOPLAY_PLAYAUDIOCD},
  {"DVDMovie", " %1 /dvd ", IDS_AUTOPLAY_PLAYDVDMOVIE},
  {"SuperVideoCDMovie", " %1 /cd ", IDS_AUTOPLAY_PLAYSVCDMOVIE},
  {"VideoCDMovie", " %1 /cd ", IDS_AUTOPLAY_PLAYVCDMOVIE},
  {"BluRay", " %1 /dvd ", IDS_AUTOPLAY_PLAYBDMOVIE},
  {"DVDAudio", " %1 /dvd ", IDS_AUTOPLAY_PLAYDVDAUDIO},
  {"Camera", " %1 /cap ", IDS_AUTOPLAY_CAPTURECAMERA},
};

bool FileAssoc::IsExtRegistered(const wchar_t* ext)
{
  BOOL is_default = FALSE;
  std::wstring prog_id = L"SPlayer";
  std::wstring file_icon = GetFileIcon(ext);
  prog_id += ext;

  if (g_paar == NULL)
    // Default manager (required at least Vista)
    ::CoCreateInstance(CLSID_ApplicationAssociationRegistration,
      NULL, CLSCTX_INPROC,
      __uuidof(IApplicationAssociationRegistration),
      (void**)&g_paar);

  if (g_paar)
    // The Vista way
    g_paar->QueryAppIsDefault(ext, AT_FILEEXTENSION, AL_EFFECTIVE, g_registered_appname, &is_default);
  else
  {
    // The 2000/XP way
    ATL::CRegKey  key;
    TCHAR		buff[256];
    ULONG		len = sizeof(buff);
    memset(buff, 0, len);

    if(ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, ext))
      return false;

    if(ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len) && !CString(buff).Trim().IsEmpty())
      return false;

    is_default = (buff == prog_id);
  }

  if (!f_setContextFiles)
  {
    ATL::CRegKey  key;
    TCHAR buff[MAX_PATH];
    ULONG len = sizeof(buff);

    std::wstring key_path = prog_id;
    key_path += L"\\shell\\open";
    if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ))
    {
      WTL::CString strCommand;
      strCommand.LoadString(IDS_OPEN_WITH_MPC);
      if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len))
        f_setContextFiles = (strCommand.CompareNoCase(CString(buff)) == 0);
    }
  }

  // Check if association is for this instance of MPC
  if (is_default)
  {
    CRegKey		key;
    TCHAR		buff[MAX_PATH];
    ULONG		len = sizeof(buff);

    is_default = FALSE;
    std::wstring key_path = prog_id;
    key_path += L"\\shell\\open\\command";
    if (ERROR_SUCCESS == key.Open(HKEY_CLASSES_ROOT, key_path.c_str(), KEY_READ))
    {
      TCHAR buff[MAX_PATH];
      ::GetModuleFileName(NULL, buff, MAX_PATH);

      std::wstring path;
      path = L"\"";
      path += buff;
      path += L"\" \"%1\"";

      if (ERROR_SUCCESS == key.QueryStringValue(NULL, buff, &len))
        is_default = (_wcsicmp(path.c_str(), buff) == 0);
    }
  }

  return is_default == TRUE?true:false;
}

std::wstring FileAssoc::GetFileIcon(const wchar_t* ext)
{
  TCHAR           buff[MAX_PATH];
  ULONG           len = sizeof(buff);
  memset(buff, 0, len);
  CString FileIcon;
  UINT iconId = 0;

  CString szExtIconfile(ext);
  szExtIconfile.Trim('.');

  CSVPToolBox svpTool;
  szExtIconfile = svpTool.GetPlayerPath(CString(_T("skins\\icons\\"))+szExtIconfile + _T(".ico") );
  if(svpTool.ifFileExist(szExtIconfile)){
    return (LPCTSTR)szExtIconfile;
  }

  CMediaFormats& mf = AfxGetAppSettings().Formats;
  for(size_t i = 0; i < mf.GetCount(); i++)
  {
    if ( mf[i].FindExt(ext) ){

      CString szType = mf[i].GetLabel();

      szExtIconfile = szType;
      szExtIconfile = svpTool.GetPlayerPath(CString(_T("skins\\icons\\"))+szExtIconfile.Trim() + _T(".ico") );
      if(svpTool.ifFileExist(szExtIconfile))
        return (LPCTSTR)szExtIconfile;

      iconId = mf[i].GetIconType();

      break;
    }

  }

  if (iconId > 0)
  {
    ::GetModuleFileName(AfxGetInstanceHandle(), buff, MAX_PATH);
    FileIcon.Format(_T("\"%s\",%d"), buff, iconId + 2 - IDI_AUDIOCD); 

    return (LPCTSTR)FileIcon;
  }
  else
  {
    return _T("");
  }
}

bool FileAssoc::IsAutoPlayRegistered(autoplay_t ap)
{
  ULONG len;
  TCHAR buff[MAX_PATH];
  if(::GetModuleFileName(NULL, buff, MAX_PATH) == 0)
    return(false);

  CString exe = buff;

  int i = (int)ap;
  if(i < 0 || i >= countof(handlers)) return(false);

  CRegKey key;

  if(ERROR_SUCCESS != key.Open(HKEY_LOCAL_MACHINE, 
    CString(CStringA("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\AutoplayHandlers\\EventHandlers\\Play") + handlers[i].verb + "OnArrival"),
    KEY_READ)) return(false);
  len = countof(buff);
  if(ERROR_SUCCESS != key.QueryStringValue(
    CString(_T("Play")) + handlers[i].verb + _T("OnArrival"), 
    buff, &len)) return(false);
  key.Close();

  if(ERROR_SUCCESS != key.Open(HKEY_CLASSES_ROOT, 
    CString(CStringA("SPlayer.Autorun\\Shell\\Play") + handlers[i].verb + "\\Command"),
    KEY_READ)) return(false);
  len = countof(buff);
  if(ERROR_SUCCESS != key.QueryStringValue(NULL, buff, &len))
    return(false);

  if(_tcsnicmp(exe, buff+1, exe.GetLength()))
    return(false);
  key.Close();

  return(true);
}
void FileAssoc::RegisterPlayer(int action_id)
{
  if (AfxGetMyApp()->IsVista() && !IsUserAnAdmin())
  {
    AfxGetMyApp()->GainAdminPrivileges(action_id);
    return;
  }

  CPPageFormats cpf;

  /* m_assoc_video << 3 | m_assoc_audio << 4 | m_ap_video << 5
   | m_ap_audio << 6 | m_ap_dvd << 7 || m_ap_cd << 8;
  */
  cpf.AddAutoPlayToRegistry(cpf.AP_VIDEO, action_id & (1 << 5));
  cpf.AddAutoPlayToRegistry(cpf.AP_MUSIC, action_id & (1 << 6));
  cpf.AddAutoPlayToRegistry(cpf.AP_DVDMOVIE, action_id & (1 << 7));
  cpf.AddAutoPlayToRegistry(cpf.AP_AUDIOCD, action_id & (1 << 8));
  cpf.AddAutoPlayToRegistry(cpf.AP_SVCDMOVIE, action_id & (1 << 7));
  cpf.AddAutoPlayToRegistry(cpf.AP_VCDMOVIE, action_id & (1 << 7));
  cpf.AddAutoPlayToRegistry(cpf.AP_BDMOVIE, action_id & (1 << 7));
  cpf.AddAutoPlayToRegistry(cpf.AP_DVDAUDIO, action_id & (1 << 8));
  cpf.AddAutoPlayToRegistry(cpf.AP_CAPTURECAMERA, true);

  // Register/Unregister Assoc
  if (action_id & (1 << 3)) //video
  {
    //TODO: rewrite and move the code into this class
    ChkDefPlayerControlBar chk_defplayer;
    chk_defplayer.SetKeyboardNativeMediaPlayers();
    chk_defplayer.SetKeyboardNativeMediaPlayers2();
  }
  PlayerPreference::GetInstance()->SetIntVar(INTVAR_CHECKFILEASSOCONSTARTUP ,action_id);



  CMediaFormats& mf = AfxGetAppSettings().Formats;
  for(size_t i = 0; i < mf.GetCount(); i++)
  {
    int fAudioOnly = mf[i].IsAudioOnly() ;
    if (fAudioOnly < 0) //we should never reg this type: rar, image etc
      continue;
    CString szPerceivedType = mf[i].getPerceivedType();

    int j = 0;
    CString str = mf[i].GetExtsWithPeriod();
    for(CString ext = str.Tokenize(_T(" "), j); !ext.IsEmpty(); ext = str.Tokenize(_T(" "), j))
    {
      bool b_regit = (!fAudioOnly && (action_id & (1 << 3)))   // should we reg video?
                     || (fAudioOnly && (action_id & (1 << 4)));// should we reg audio?
      //TODO: rewrite and move code into this class
      CPPageFormats::RegisterExt(ext, b_regit, szPerceivedType);
    }
  }

  SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
}
