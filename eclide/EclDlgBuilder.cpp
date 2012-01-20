#include "StdAfx.h"
#include "..\en_us\resource.h"

#include "ecldlgbuilder.h"

#include <utilDateTime.h> //clib
#include <util.h> //clib
#include <EclCC.h> //commms
//  ===========================================================================
CBuilderDlg::CBuilderDlg(IAttribute *attribute, IEclBuilderSlot * owner) : m_attribute(attribute), m_owner(owner), baseClass(owner)
{
	if (m_attribute)
		m_sigConn = m_attribute->on_refresh_connect(boost::ref(*this));

	m_comboQueueClusterCtrl = new CComboQueueCluster();
	m_comboLabelCtrl = new CComboLabel();
	m_advanced = false;
	m_schedule = false;
	m_archive= false;
	m_noCommonPrivateAttributes = false;
}

void CBuilderDlg::GetTitle(CString & title)
{
	if ( m_view.IsDirty() )
		title = _T("*") + m_name;
	else
		title = m_name;
}

bool CBuilderDlg::DoFileOpen(const CString & sPathName) 
{
	ATLASSERT(!sPathName.IsEmpty());
	CString cleanPathName;
	if(sPathName[0] == '\"' && sPathName[sPathName.GetLength() - 1] == '\"')
		cleanPathName = sPathName.Mid(1, sPathName.GetLength() - 2);
	else
		cleanPathName = sPathName;
	SetNamePath(cleanPathName);

	if (!m_view.OpenFile(cleanPathName))
	{
		// show the error message
		CString sMessage;
		sMessage.Format ( _T( "Error: Failed to open: %s\n\n" ), cleanPathName);
		ShowLastError( sMessage, ::GetLastError () );
		
		return false;
	}

	ResetSavePoint(true);

	return true;
}

bool CBuilderDlg::DoSave(bool attrOnly) 
{
	CWaitCursor wait;
	CString ecl;
	m_view.GetText(ecl);
	if (m_attribute && m_attribute->SetText(ecl))
	{
		m_view.SetSavePoint();
		IAttributeVector attrs;
		Dali::CEclExceptionVector errors;
		m_attribute->PreProcess(PREPROCESS_SAVE, NULL, attrs, errors);
		if (attrs.size())
		{
			if (!m_migrator)
				m_migrator = CreateIMigration(::AttachRepository());
			m_migrator->Stop();
			for(IAttributeVector::const_iterator itr = attrs.begin(); itr != attrs.end(); ++itr)
				m_migrator->AddToRep(itr->get()->GetAsHistory(), (boost::_tformat(_T("Preprocessed (%1%) from %2%.")) % PREPROCESS_LABEL[PREPROCESS_SAVE] % m_attribute->GetQualifiedLabel()).str().c_str(), true);
			m_migrator->Start();
			m_migrator->Join();
			SendMessage(CWM_SUBMITDONE, Dali::WUActionCheck, (LPARAM)&errors);
			GetIMainFrame()->Send_RefreshRepositoryWindow(m_attribute->GetModule());
			GetIMainFrame()->SyncTOC(m_attribute->GetQualifiedLabel(), m_attribute->GetType());
		}
		return true;
	}
	else if (!m_path.IsEmpty()) 
		return DoFileSave(m_path);
	if (!attrOnly)
		return DoFileSaveAs();
	return false;
}

bool CBuilderDlg::DoFileSaveAs() 
{
	CFileDialogEx wndFileDialog(FALSE, _T(".ecl"), m_path, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, szEclSaveFilter, m_hWnd);

	if ( IDOK == wndFileDialog.DoModal() ) 
	{
		// save the file
		if (!DoFileSave(wndFileDialog.GetPathName()))
		{
			return FALSE;
		}
		// save the title 
		SetNamePath(wndFileDialog.GetPathName());
		return TRUE;
	}
	return FALSE;
}

bool CBuilderDlg::DoFileSave(const CString & sPathName) 
{
	std::_tstring ext;
	CString newPath = CheckExtension((const TCHAR *)sPathName, ".ecl", ext);
	if (!m_view.SaveFile(newPath))
	{
		// show the error message
		CString sMessage;
		sMessage.Format ( _T( "Error: Failed to save: %s\n\n" ), newPath );
		ShowLastError( sMessage, ::GetLastError () );
		
		return FALSE;
	}

	// Save file name for later
	m_attribute = NULL;				//Attribute detaches on SaveAs (as user could be saving anywhere)
	SetNamePath(newPath);	
	m_view.SetSavePoint();

	return TRUE;
}

void CBuilderDlg::SetNamePath(const CString & sPathName)
{	
	CString pathName = sPathName;
	TCHAR szTitle [_MAX_FNAME], szExt[_MAX_FNAME];
	_tsplitpath(pathName.GetBuffer(_MAX_PATH), NULL, NULL, szTitle, szExt);
	pathName.ReleaseBuffer();
	lstrcat(szTitle, szExt);			

	m_path = sPathName;
	if (szTitle) 
		m_name = szTitle;
	
	//ATLASSERT(!"TODO:  UIUpdateTitle()");
}

const TCHAR * CBuilderDlg::GetPath() const
{
	return m_path;
}

void CBuilderDlg::SetQueueCluster(const CString & queue, const CString & cluster)
{
	m_comboQueueClusterCtrl->SelectClusterQueue((const TCHAR *)queue, (const TCHAR *)cluster);
	PostMessage(BUM_REFRESHQUEUECLUSTER);
}

const TCHAR * CBuilderDlg::GetQueue()
{
	return m_comboQueueClusterCtrl->m_queue.c_str();
}

const TCHAR * CBuilderDlg::GetCluster()
{
	return m_comboQueueClusterCtrl->m_cluster.c_str();
}

bool CBuilderDlg::CanExecute()
{
	std::_tstring cstr = GetCluster();
	std::_tstring qstr = GetQueue();
	if (!cstr.empty() && /*!qstr.empty() && */m_view.GetLength())
		return true;
	return false;
}

LRESULT CBuilderDlg::OnInitDialog(HWND /*hWnd*/, LPARAM /*lParam*/)
{
	SetMsgHandled(false);
	DlgResize_Init(true, false);

	m_name = "builder.ecl";
	m_comboQueueClusterCtrl->Attach(GetDlgItem(IDC_COMBO_QUEUECLUSTER));
	m_comboQueueClusterCtrl->Load();
	m_comboQueueClusterCtrl->m_value = GetIConfig(QUERYBUILDER_CFG)->Get(GLOBAL_CLUSTER);
	m_comboQueueClusterCtrl->SelectDefault(m_comboQueueClusterCtrl->m_value);
	m_comboQueueClusterCtrl->SetDroppedWidth(240);
	m_comboQueueClusterCtrl->LimitText(0);

	*m_comboLabelCtrl = GetDlgItem(IDC_COMBO_LABEL);
	m_comboLabelCtrl->Load(_T(""));
	m_comboLabelCtrl->m_value = _T("");
	m_comboLabelCtrl->SelectDefault(m_comboLabelCtrl->m_value);

	m_advancedCtrl = GetDlgItem(IDC_BUTTON_ADVANCED);

	m_dateCtrl = GetDlgItem(IDC_DATE);
	m_timeCtrl = GetDlgItem(IDC_TIME);
	m_timeCtrl.ModifyStyle(0, DTS_TIMEFORMAT);

	m_limitResult = GetDlgItem(IDC_SPIN_LIMITRESULT);
	m_limitResult.SetRange(0, 9999);
	m_limitResult.SetPos(GetIConfig(QUERYBUILDER_CFG)->Get(GLOBAL_WORKUNIT_RESULTLIMIT));

	m_maxRuntime = GetDlgItem(IDC_SPIN_MAXRUNTIME);
	m_maxRuntime.SetRange(0, 24*60*7);
	m_maxRuntime.SetPos(0);

	m_goButton.SubclassWindow(GetDlgItem(IDC_BUTTON_GO));
	m_goButton.SetMenu(IDR_POPUP_EEGO);

	m_debugButton = GetDlgItem(IDC_BUTTON_DEBUG);
	m_archiveButton = GetDlgItem(IDC_BUTTON_ARCHIVE);

	DoDataExchange();

	return 0;
}

LRESULT CBuilderDlg::OnInitialize(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	SetMsgHandled(false);
	HideAdvanced();
	return 0;
}

void CBuilderDlg::OnDestroy()
{
	SetMsgHandled(false);
}

void CBuilderDlg::OnSize(UINT nType, CSize size)
{
	SetMsgHandled(false);
}

LRESULT CBuilderDlg::OnRefreshQueueCluster(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	std::_tstring queueStr = _T("");
	std::_tstring clusterStr = _T("");
	if (CComPtr<Topology::IEclServer> eclServer = m_comboQueueClusterCtrl->GetSelectedQueue())
		queueStr = eclServer->GetQueue();
	if (CComPtr<Topology::ICluster> cluster = m_comboQueueClusterCtrl->GetSelectedCluster())
		clusterStr = cluster->GetID();
	std::_tstring display = clusterStr;
	if (!queueStr.empty())
		display += _T(" (") + queueStr +  _T(")");
	::SetWindowText(*m_comboQueueClusterCtrl, display.c_str());
	return 0;
}

LRESULT CBuilderDlg::OnBnClickedCheckSchedule(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	DoUpdateScheduleInfo();
	return 0;
}

bool CBuilderDlg::IsScheduled()
{
	return m_schedule;
}

void CBuilderDlg::SetSchedule(const std::_tstring & utc)
{
	if (utc.length() && !boost::algorithm::iequals(utc, DATETIME_ZERO))
	{
		SYSTEMTIME d;
		UTCToSystemTime(utc, d, true);
		m_dateCtrl.SetSystemTime(GDT_VALID, &d);
		m_timeCtrl.SetSystemTime(GDT_VALID, &d);
		m_schedule = true;
		DoDataExchange();
		DoUpdateScheduleInfo();
	}
}

void CBuilderDlg::GetSchedule(std::_tstring & utc)
{
	SYSTEMTIME d, t;
	m_dateCtrl.GetSystemTime(&d);
	m_timeCtrl.GetSystemTime(&t);
	DateTimeToUTC(d, t, utc);
}

bool CBuilderDlg::IsLabeled()
{
	if (m_comboLabelCtrl->GetCurSel() > 0)
		return true;
	return false;
}

void CBuilderDlg::SetLabel(const CString & label)
{
	m_comboLabelCtrl->SelectDefault(label);
	DoDataExchange();
}

void CBuilderDlg::GetLabel(CString & label)
{
	DoDataExchange(true);
	label = m_comboLabelCtrl->m_value;
}

void CBuilderDlg::GetName(CString & name)
{
	name = m_name;
}

void CBuilderDlg::SetResultLimit(int limit)
{
	m_limitResult.SetPos(limit);
}

int CBuilderDlg::GetResultLimit()
{
	return m_limitResult.GetPos();
}

void CBuilderDlg::SetMaxRuntime(int maxRuntime)
{
	m_maxRuntime.SetPos(maxRuntime);
}

int CBuilderDlg::GetMaxRuntime()
{
	return m_maxRuntime.GetPos();
}

void CBuilderDlg::SetDebug(const std::_tstring & debug)
{
	m_debug = debug.c_str();
	DoDataExchange();
}

const TCHAR * CBuilderDlg::GetDebug()
{
	DoDataExchange(true);
	return m_debug;
}

bool CBuilderDlg::IsArchive()
{
	//DoDataExchange(true);
	return m_archive;
}

//void CBuilderDlg::SetArchive(bool archive)
//{
//	m_archive = archive;
//	DoDataExchange();
//}
//
bool CBuilderDlg::IsNoCommonPrivateAttributes()
{
	DoDataExchange(true);
	return m_noCommonPrivateAttributes;
}

void CBuilderDlg::SetNoCommonPrivateAttributes(bool noCommonPrivateAttributes)
{
	m_noCommonPrivateAttributes = noCommonPrivateAttributes;
	DoDataExchange();
}

void CBuilderDlg::SavePersistInfo(CPersistMap & persistInfo)
{
	persistInfo.Set(PERSIST_LABEL, static_cast<const TCHAR *>(m_name));
	persistInfo.Set(PERSIST_FILEPATH, static_cast<const TCHAR *>(m_path));
	persistInfo.Set(PERSIST_MAXRUNTIME, boost::lexical_cast<std::_tstring>(m_maxRuntime.GetPos()).c_str());
	
	baseClass::SavePersistInfo(persistInfo);
}

void CBuilderDlg::RestorePersistInfo(const CPersistMap & persistInfo)
{
	try
	{
		m_maxRuntime.SetPos(boost::lexical_cast<int>(persistInfo.Get(PERSIST_MAXRUNTIME)));
	}
	catch (boost::bad_lexical_cast &)
	{
		m_maxRuntime.SetPos(0);
	}
	m_path = persistInfo.Get(PERSIST_FILEPATH);
	m_view.OpenFile(m_path);
	ResetSavePoint(true);
	m_name = persistInfo.Get(PERSIST_LABEL);
	baseClass::RestorePersistInfo(persistInfo);

	DoDataExchange();
}

void CBuilderDlg::DoCheckSyntax()
{
	m_view.SyntaxChecking();
	CString ecl;
	m_view.GetText(ecl);
	CString cluster(GetCluster());
	if (CComPtr<IEclCC> eclcc = CreateIEclCC())
	{
		if (!m_path.IsEmpty()) 
			DoFileSave(m_path);
	}
	clib::thread run(__FUNCTION__, boost::bind(&EclCheckSyntax, this, ecl, cluster, CString(), CString(), m_path, m_debug, false, m_noCommonPrivateAttributes));
}

void CBuilderDlg::DoCheckComplexity()
{
	CWaitCursor wait;
	m_view.SyntaxClearAll();
	CString ecl;
	GetSource(ecl);
	StlLinked<Dali::IDali> server = Dali::AttachDali(GetIConfig(QUERYBUILDER_CFG)->Get(GLOBAL_SERVER_WORKUNIT), _T("Dali"));
	std::_tstring complexity, complexitDisplay;
	Dali::CEclExceptionVector errorResults;
	server->CheckComplexity(GetCluster(), GetIConfig(QUERYBUILDER_CFG)->Get(GLOBAL_QUEUE), ecl, _T(""), _T(""), complexity, errorResults);
	if (complexity.length() == 0)
	{
		complexitDisplay = ComplexityErrorMsg;
		SendMessage(CWM_SUBMITDONE, Dali::WUActionCheck, (LPARAM)&errorResults);
	}
	else
		complexitDisplay = _T("Complexity Count = \"") + complexity + _T("\"");
	MessageBox(complexitDisplay.c_str(), _T("Complexity Count"), MB_ICONINFORMATION);
}

void CBuilderDlg::DoCheckDependency()
{
	CWaitCursor wait;
	m_view.SyntaxClearAll();
	CString ecl;
	GetSource(ecl);
	StlLinked<IRepository> rep = ::AttachRepository();
	StlLinked<Dali::IDali> dali = Dali::AttachDali();
	Dali::CEclExceptionVector errorResults;
	DoShowDependantAttributes(rep, dali, static_cast<const TCHAR *>(CString(GetIConfig(QUERYBUILDER_CFG)->Get(GLOBAL_QUEUE))), GetCluster(), static_cast<const TCHAR *>(ecl), _T(""), _T(""), errorResults);
	SendMessage(CWM_SUBMITDONE, Dali::WUActionCheck, (LPARAM)&errorResults);
}

void CBuilderDlg::SetAttribute(IAttribute *attribute)
{
	if (m_attribute)
		m_sigConn.disconnect();

	m_attribute = attribute;
	if (m_attribute)
		m_sigConn = m_attribute->on_refresh_connect(boost::ref(*this));
}

IAttribute *CBuilderDlg::GetAttribute()
{
	return m_attribute;
}

void CBuilderDlg::DoUpdateScheduleInfo()
{
	DoDataExchange(true);
	if (m_schedule)
	{
		m_dateCtrl.EnableWindow(true);
		m_timeCtrl.EnableWindow(true);
		m_goButton.SetWindowText(_T("Schedule"));
	}
	else
	{
		m_dateCtrl.EnableWindow(false);
		m_timeCtrl.EnableWindow(false);
		m_goButton.SetWindowText(_T("Submit"));
	}
}

void CBuilderDlg::ShowHide(UINT nID, bool hide)
{
	CWindow win = GetDlgItem(nID);
	win.ShowWindow(hide ? SW_HIDE : SW_SHOW);
}

void CBuilderDlg::ShowAdvanced()
{
	CRect win;
	GetWindowRect(win);
	ScreenToClient(win);
	CRect lastControl;
	m_maxRuntime.GetWindowRect(lastControl);
	ScreenToClient(lastControl);
	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;// | SWP_NOSENDCHANGING;
	m_view.SetWindowPos(NULL, 0, lastControl.bottom + 2, win.Width(), win.Height() - (lastControl.bottom + 2), flags);
	ShowHide(IDC_CHECK_SCHEDULE, false);
	ShowHide(IDC_CHECK_SYNTAXPLUS, false);
	ShowHide(IDC_DATE, false);
	ShowHide(IDC_TIME, false);
	ShowHide(IDC_STATIC_LABEL, false);
	ShowHide(IDC_COMBO_LABEL, false);
	ShowHide(IDC_STATIC_LIMITRESULT, false);
	ShowHide(IDC_EDIT_LIMITRESULT, false);
	ShowHide(IDC_SPIN_LIMITRESULT, false);
	ShowHide(IDC_STATIC_MAXRUNTIME, false);
	ShowHide(IDC_EDIT_MAXRUNTIME, false);
	ShowHide(IDC_SPIN_MAXRUNTIME, false);
	ShowHide(IDC_STATIC_DEBUG, false);
	ShowHide(IDC_EDIT_DEBUG, false);
	ShowHide(IDC_BUTTON_DEBUG, true);	//  No Debug for OSS
	ShowHide(IDC_BUTTON_ARCHIVE, false);

	DoDataExchange(true);
	if (IsScheduled() || IsLabeled() || m_limitResult.GetPos() != static_cast<int>(GetIConfig(QUERYBUILDER_CFG)->Get(GLOBAL_WORKUNIT_RESULTLIMIT)) || m_debug.GetLength() || m_maxRuntime.GetPos() != 0)
		m_advancedCtrl.SetWindowText(_T("Less *"));
	else
		m_advancedCtrl.SetWindowText(_T("Less"));
}

void CBuilderDlg::HideAdvanced()
{
	CRect win;
	GetWindowRect(win);
	ScreenToClient(win);
	CRect date;
	m_dateCtrl.GetWindowRect(date);
	ScreenToClient(date);
	UINT flags = SWP_NOZORDER | SWP_NOACTIVATE;// | SWP_NOSENDCHANGING;
	m_view.SetWindowPos(NULL, 0, date.top, win.Width(), win.Height() - date.top, flags);
	ShowHide(IDC_CHECK_SCHEDULE, true);
	ShowHide(IDC_CHECK_SYNTAXPLUS, true);
	ShowHide(IDC_DATE, true);
	ShowHide(IDC_TIME, true);
	ShowHide(IDC_STATIC_LABEL, true);
	ShowHide(IDC_COMBO_LABEL, true);
	ShowHide(IDC_STATIC_LIMITRESULT, true);
	ShowHide(IDC_EDIT_LIMITRESULT, true);
	ShowHide(IDC_SPIN_LIMITRESULT, true);
	ShowHide(IDC_STATIC_MAXRUNTIME, true);
	ShowHide(IDC_EDIT_MAXRUNTIME, true);
	ShowHide(IDC_SPIN_MAXRUNTIME, true);
	ShowHide(IDC_STATIC_DEBUG, true);
	ShowHide(IDC_EDIT_DEBUG, true);
	ShowHide(IDC_BUTTON_DEBUG, true);
	ShowHide(IDC_BUTTON_ARCHIVE, true);

	DoDataExchange(true);
	if (IsScheduled() || IsLabeled() || m_limitResult.GetPos() != static_cast<int>(GetIConfig(QUERYBUILDER_CFG)->Get(GLOBAL_WORKUNIT_RESULTLIMIT)) || m_debug.GetLength() || m_maxRuntime.GetPos() != 0)
		m_advancedCtrl.SetWindowText(_T("More *"));
	else
		m_advancedCtrl.SetWindowText(_T("More"));
}

void CBuilderDlg::OnFileSaveAs(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
{
	DoFileSaveAs();
}

void CBuilderDlg::OnEclGo(UINT /*uNotifyCode*/, int nID, HWND /*hWnd*/)
{
	switch(nID)
	{
	case ID_ECL_GO:
	case ID_GO_SUBMIT:
		m_owner->OnButtonGo(Dali::WUActionRun);
		break;
	case ID_GO_COMPILE:
		m_owner->OnButtonGo(Dali::WUActionCompile);
		break;
	case ID_GO_DEBUG:
		m_owner->OnButtonDebug();
		break;
	default:
		ATLASSERT(false);
		break;
	}
}

void CBuilderDlg::OnEclSyncToc(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
{
	if (m_attribute)
		GetIMainFrame()->SyncTOC(m_attribute->GetQualifiedLabel(), m_attribute->GetType());
}

void CBuilderDlg::OnLocateFileInExplorer(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
{
	boost::filesystem::wpath file = m_path;
	boost::filesystem::wpath folder = file.parent_path();
	::ShellExecute(m_hWnd, _T("open"), _T(""), _T(""), folder.native_directory_string().c_str(), SW_SHOWNORMAL);
}

void CBuilderDlg::OnEclGoto(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
{
	CString message;
	GetWordAtCurPos(message);
	if (message[0])
		GetIMainFrame()->OpenAttribute(message, CreateIAttributeECLType());
}

void CBuilderDlg::OnEclGotoSyncToc(UINT /*uNotifyCode*/, int /*nID*/, HWND /*hWnd*/)
{
	CString message;
	GetWordAtCurPos(message);
	if (message[0])
	{
		if (m_attribute)
			GetIMainFrame()->SyncTOC(message, CreateIAttributeECLType(), m_attribute->GetModuleQualifiedLabel());
		else
			GetIMainFrame()->SyncTOC(message, CreateIAttributeECLType());
	}
}

LRESULT CBuilderDlg::OnBnClickedButtonGo(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& bHandled)
{
	RECT rcWindow = { 0 };
	m_goButton.GetWindowRect(&rcWindow);

	CPoint cursorPos;
	GetCursorPos(&cursorPos);
	if (cursorPos.x >= rcWindow.right - 22)
	{
		//  Let the split menu handle it.
		bHandled = false;
		return 0;
	}

	m_owner->OnButtonGo(Dali::WUActionRun);
	return 0;
}

LRESULT CBuilderDlg::OnBnClickedButtonDebug(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	m_owner->OnButtonDebug();
	return 0;
}

LRESULT CBuilderDlg::OnBnClickedButtonAdvanced(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_advanced)
	{
		HideAdvanced();
		m_advanced = false;
	}
	else
	{
		ShowAdvanced();
		m_advanced = true;
	}
	return 0;
}

LRESULT CBuilderDlg::OnBnClickedButtonArchive(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	CWaitCursor wait;
	if (CComPtr<IEclCC> eclcc = CreateIEclCC())
	{
		CString ecl;
		m_view.GetText(ecl);
		std::_tstring archive;
		Dali::CEclExceptionVector errors;
		if (eclcc->GetArchive(_T(""), (const TCHAR *)ecl, archive, errors))
		{
			CFileDialogEx wndFileDialog(FALSE, _T(".xml"), NULL, OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, _T("Archive File (*.xml)\0*.xml"), m_hWnd);
			if ( IDOK == wndFileDialog.DoModal() ) 
			{
				CUnicodeFile archiveFile;
				archiveFile.Create(wndFileDialog.GetPathName());
				archiveFile.Write(archive);
			}
		}
		else
			MessageBox(_T("Could not create archive, please syntax check and try again."), _T("ECL IDE"));
		
		return 0;
	}

	//Quick hack as "archive" used to be a checkbox...
	m_archive = true;
	m_owner->OnButtonGo(Dali::WUActionRun);
	m_archive = false;
	return 0;
}

LRESULT CBuilderDlg::OnCbnSelendokComboCluster(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	QueueClusterPair * queueCluster = (QueueClusterPair *)m_comboQueueClusterCtrl->GetItemDataPtr(m_comboQueueClusterCtrl->GetCurSel());
	m_comboQueueClusterCtrl->m_queue = queueCluster->first ? queueCluster->first.get()->GetQueue() : _T("");
	m_comboQueueClusterCtrl->m_cluster = queueCluster->second.get()->GetID();
	GetIConfig(QUERYBUILDER_CFG)->Set(GLOBAL_QUEUE, m_comboQueueClusterCtrl->m_queue);
	GetIConfig(QUERYBUILDER_CFG)->Set(GLOBAL_CLUSTER, m_comboQueueClusterCtrl->m_cluster);
	PostMessage(BUM_REFRESHQUEUECLUSTER);
	return 0;
}

	//  IAttribute Notifications  ---
void CBuilderDlg::operator()(IAttribute * attr, bool eclChanged, IAttribute * newAttrAsOldOneMoved, bool deleted)
{
	if (eclChanged)
	{
		CString ecl;
		m_view.GetText(ecl);
		if (ecl.Compare(attr->GetText(false)) != 0)
		{
			CString message = m_name + _T("\r\n\r\n") + _T("This file has been modified outside of the source editor.\r\nDo you want to reload it and lose the changes made in the source editor?");
			if (MessageBox(message, CString(MAKEINTRESOURCE(IDR_MAINFRAME)), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES)	//Keep in sync with ChildBduilderFrame.cpp
				m_view.SetText(attr->GetText(false));
		}
	}
	//TODO handle renamed and deleted.
	ATLASSERT(!newAttrAsOldOneMoved && !deleted);
}

	//  IMigrationCallback  ---
BOOL CBuilderDlg::Invalidate(BOOL bErase)
{
	return TRUE;
}
void CBuilderDlg::LogMsg(const std::_tstring & msg)
{
}
void CBuilderDlg::PostStatus(const TCHAR* pStr)
{
}
void CBuilderDlg::PostProgress(int progress)
{
}//  ===========================================================================