#include <znc/Chan.h>
#include <znc/IRCNetwork.h>
#include <pcrecpp.h>

using std::list;
using std::vector;
using std::set;

class CWatchSource {
public:
	CWatchSource(const CString& sSource, bool bNegated) {
		m_sSource = sSource;
		m_bNegated = bNegated;
	}
	virtual ~CWatchSource() {}

	// Getters
	const CString& GetSource() const { return m_sSource; }
	bool IsNegated() const { return m_bNegated; }
	// !Getters

	// Setters
	// !Setters
private:
protected:
	bool    m_bNegated;
	CString m_sSource;
};

class CWatchEntry {
public:
	CWatchEntry(const CString& sHostMask, const CString& sTarget, const CString& sPattern) {
		m_bDisabled = false;
		m_bDetachedOnly = false;
		m_sPattern = (sPattern.size()) ? sPattern : ".*";

		CNick Nick;
		Nick.Parse(sHostMask);

		m_sHostMask = (Nick.GetNick().size()) ? Nick.GetNick() : ".*?";
		m_sHostMask += "!";
		m_sHostMask += (Nick.GetIdent().size()) ? Nick.GetIdent() : ".*?";
		m_sHostMask += "@";
		m_sHostMask += (Nick.GetHost().size()) ? Nick.GetHost() : ".*?";

		if (sTarget.size()) {
			m_sTarget = sTarget;
		} else {
			m_sTarget = "$";
			m_sTarget += Nick.GetNick();
		}
	}
	virtual ~CWatchEntry() {}

	bool IsMatch(const CNick& Nick, const CString& sText, const CString& sSource, const CIRCNetwork* pNetwork) {
		if (IsDisabled()) {
			return false;
		}

		bool bGoodSource = true;

		if (!sSource.empty() && !m_vsSources.empty()) {
			bGoodSource = false;

			for (unsigned int a = 0; a < m_vsSources.size(); a++) {
				const CWatchSource& WatchSource = m_vsSources[a];

				pcrecpp::RE rSource(WatchSource.GetSource());
				if ((rSource.error().length() == 0) && rSource.PartialMatch(sSource)) {
					if (WatchSource.IsNegated()) {
						return false;
					} else {
						bGoodSource = true;
					}
				}
			}
		}

		if (!bGoodSource)
			return false;

		pcrecpp::RE rHostMask(m_sHostMask);
		if (!((rHostMask.error().length() == 0) && rHostMask.PartialMatch(Nick.GetHostMask())))
			return false;

		pcrecpp::RE rPattern(pNetwork->ExpandString(m_sPattern));
		return (rPattern.error().length() == 0) && rPattern.PartialMatch(sText);
	}

	bool operator ==(const CWatchEntry& WatchEntry) {
		return (GetHostMask().Equals(WatchEntry.GetHostMask())
				&& GetTarget().Equals(WatchEntry.GetTarget())
				&& GetPattern().Equals(WatchEntry.GetPattern())
		);
	}

	// Getters
	const CString& GetHostMask() const { return m_sHostMask; }
	const CString& GetTarget() const { return m_sTarget; }
	const CString& GetPattern() const { return m_sPattern; }
	bool IsDisabled() const { return m_bDisabled; }
	bool IsDetachedOnly() const { return m_bDetachedOnly; }
	const vector<CWatchSource>& GetSources() const { return m_vsSources; }
	CString GetSourcesStr() const {
		CString sRet;

		for (unsigned int a = 0; a < m_vsSources.size(); a++) {
			const CWatchSource& WatchSource = m_vsSources[a];

			if (a) {
				sRet += " ";
			}

			if (WatchSource.IsNegated()) {
				sRet += "!";
			}

			sRet += WatchSource.GetSource();
		}

		return sRet;
	}
	// !Getters

	// Setters
	void SetHostMask(const CString& s) { m_sHostMask = s; }
	void SetTarget(const CString& s) { m_sTarget = s; }
	void SetPattern(const CString& s) { m_sPattern = s; }
	void SetDisabled(bool b = true) { m_bDisabled = b; }
	void SetDetachedOnly(bool b = true) { m_bDetachedOnly = b; }
	void SetSources(const CString& sSources) {
		VCString vsSources;
		VCString::iterator it;
		sSources.Split(" ", vsSources, false);

		m_vsSources.clear();

		for (it = vsSources.begin(); it != vsSources.end(); ++it) {
			pcrecpp::RE rSource(*it);
			if (rSource.error().size() != 0) {
				continue;
			}
			if (it->at(0) == '!' && it->size() > 1) {
				m_vsSources.push_back(CWatchSource(it->substr(1), true));
			} else {
				m_vsSources.push_back(CWatchSource(*it, false));
			}
		}
	}
	// !Setters
private:
protected:
	CString              m_sHostMask;
	CString              m_sTarget;
	CString              m_sPattern;
	bool                 m_bDisabled;
	bool                 m_bDetachedOnly;
	vector<CWatchSource> m_vsSources;
};

class CWatcherMod : public CModule {
public:
	MODCONSTRUCTOR(CWatcherMod) {
		m_Buffer.SetLineCount(500);
		m_uiLength = 5;
		m_MessageCache.SetTTL(1000 * 3600 * 6);
		Load();
		AddSubPage(std::make_shared<CWebSubPage>("configure", "Configure", 0));
		AddSubPage(std::make_shared<CWebSubPage>("importexport", "Import/Export", 0));
	}

	virtual ~CWatcherMod() {}

	virtual bool OnLoad(const CString& sArgs, CString& sMessage) {
		if (!sArgs.Token(0).Equals("")) {
			m_uiLength = sArgs.Token(0).ToUInt();
		} else {
			m_uiLength = 5;
		}

		if (!sArgs.Token(1).Equals("")) {
			m_MessageCache.SetTTL(1000 * sArgs.Token(1).ToUInt());
		} else {
			m_MessageCache.SetTTL(1000 * 3600 * 6);
		}

		return true;
	}

	virtual void OnRawMode(const CNick& OpNick, CChan& Channel, const CString& sModes, const CString& sArgs) {
		Process(OpNick, "* " + OpNick.GetNick() + " sets mode: " + sModes + " " +
			sArgs + " on " + Channel.GetName(), Channel.GetName());
	}

	virtual void OnClientLogin() {
		MCString msParams;
		msParams["target"] = m_pNetwork->GetCurNick();

		size_t uSize = m_Buffer.Size();
		for (unsigned int uIdx = 0; uIdx < uSize; uIdx++) {
			PutUser(m_Buffer.GetLine(uIdx, *GetClient(), msParams));
		}
		m_Buffer.Clear();
	}

	virtual void OnKick(const CNick& OpNick, const CString& sKickedNick, CChan& Channel, const CString& sMessage) {
		Process(OpNick, "* " + OpNick.GetNick() + " kicked " + sKickedNick + " from " +
			Channel.GetName() + " because [" + sMessage + "]", Channel.GetName());
	}

	virtual void OnQuit(const CNick& Nick, const CString& sMessage, const vector<CChan*>& vChans) {
		Process(Nick, "* Quits: " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") "
			"(" + sMessage + ")", "");
	}

	virtual void OnJoin(const CNick& Nick, CChan& Channel) {
		Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") joins " +
			Channel.GetName(), Channel.GetName());
	}

	virtual void OnPart(const CNick& Nick, CChan& Channel, const CString& sMessage) {
		Process(Nick, "* " + Nick.GetNick() + " (" + Nick.GetIdent() + "@" + Nick.GetHost() + ") parts " +
			Channel.GetName() + "(" + sMessage + ")", Channel.GetName());
	}

	virtual void OnNick(const CNick& OldNick, const CString& sNewNick, const vector<CChan*>& vChans) {
		Process(OldNick, "* " + OldNick.GetNick() + " is now known as " + sNewNick, "");
	}

	virtual EModRet OnCTCPReply(CNick& Nick, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " reply [" + sMessage + "]", "priv");
		return CONTINUE;
	}

	virtual EModRet OnPrivCTCP(CNick& Nick, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "]", "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanCTCP(CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "* CTCP: " + Nick.GetNick() + " [" + sMessage + "] to "
			"[" + Channel.GetName() + "]", Channel.GetName());
		return CONTINUE;
	}

	virtual EModRet OnPrivNotice(CNick& Nick, CString& sMessage) {
		Process(Nick, "-" + Nick.GetNick() + "- " + sMessage, "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanNotice(CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "-" + Nick.GetNick() + ":" + Channel.GetName() + "- " + sMessage, Channel.GetName());
		return CONTINUE;
	}

	virtual EModRet OnPrivMsg(CNick& Nick, CString& sMessage) {
		Process(Nick, "<" + Nick.GetNick() + "> " + sMessage, "priv");
		return CONTINUE;
	}

	virtual EModRet OnChanMsg(CNick& Nick, CChan& Channel, CString& sMessage) {
		Process(Nick, "<" + Nick.GetNick() + ":" + Channel.GetName() + "> " + sMessage, Channel.GetName());
		return CONTINUE;
	}

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmdName = sCommand.Token(0);
		if (sCmdName.Equals("ADD") || sCmdName.Equals("WATCH")) {
			Watch(sCommand.Token(1), sCommand.Token(2), sCommand.Token(3, true));
		} else if (sCmdName.Equals("HELP")) {
			Help();
		} else if (sCmdName.Equals("LIST")) {
			List();
		} else if (sCmdName.Equals("DUMP")) {
			Dump();
		} else if (sCmdName.Equals("ENABLE")) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDisabled(~0, false);
			} else {
				SetDisabled(sTok.ToUInt(), false);
			}
		} else if (sCmdName.Equals("DISABLE")) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDisabled(~0, true);
			} else {
				SetDisabled(sTok.ToUInt(), true);
			}
		} else if (sCmdName.Equals("ENABLEDETACHEDONLY")) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDetachedOnly(~0, true);
			} else {
				SetDetachedOnly(sTok.ToUInt(), true);
			}
		} else if (sCmdName.Equals("DISABLEDETACHEDONLY")) {
			CString sTok = sCommand.Token(1);

			if (sTok == "*") {
				SetDetachedOnly(~0, false);
			} else {
				SetDetachedOnly(sTok.ToUInt(), false);
			}
		} else if (sCmdName.Equals("SETSOURCES")) {
			SetSources(sCommand.Token(1).ToUInt(), sCommand.Token(2, true));
		} else if (sCmdName.Equals("CLEAR")) {
			m_lsWatchers.clear();
			PutModule("All entries cleared.");
			Save();
		} else if (sCmdName.Equals("BUFFER")) {
			CString sCount = sCommand.Token(1);

			if (sCount.size()) {
				m_Buffer.SetLineCount(sCount.ToUInt());
			}

			PutModule("Buffer count is set to [" + CString(m_Buffer.GetLineCount()) + "]");
		} else if (sCmdName.Equals("DEL")) {
			Remove(sCommand.Token(1).ToUInt());
		} else {
			PutModule("Unknown command: [" + sCmdName + "]");
		}
	}

private:
	void Process(const CNick& Nick, const CString& sMessage, const CString& sSource) {
		set<CString> sHandledTargets;

		list<CString> lMessages;
		list<CString> *pMessages = m_MessageCache.GetItem(sSource);
		if (pMessages) {
			lMessages = *pMessages;
		}

		lMessages.push_back(sMessage);

		while (lMessages.size() > m_uiLength) {
			lMessages.pop_front();
		}

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
			CWatchEntry& WatchEntry = *it;

			if (m_pNetwork->IsUserAttached() && WatchEntry.IsDetachedOnly()) {
				continue;
			}

			if (WatchEntry.IsMatch(Nick, sMessage, sSource, m_pNetwork) &&
				sHandledTargets.count(WatchEntry.GetTarget()) < 1) {

				if (m_pNetwork->IsUserAttached()) {
					for (list<CString>::iterator itMsg = lMessages.begin(); itMsg != lMessages.end(); ++itMsg) {
						m_pNetwork->PutUser(":" + WatchEntry.GetTarget() + "!watch@znc.in PRIVMSG " + m_pNetwork->GetCurNick() + " :" + *itMsg);
					}
					if (lMessages.size() > 1) {
						m_pNetwork->PutUser(":" + WatchEntry.GetTarget() + "!watch@znc.in PRIVMSG " + m_pNetwork->GetCurNick() + " :---");
					}
				} else {
					for (list<CString>::iterator itMsg = lMessages.begin(); itMsg != lMessages.end(); ++itMsg) {
						m_Buffer.AddLine(":" + _NAMEDFMT(WatchEntry.GetTarget()) + "!watch@znc.in PRIVMSG {target} :{text}", *itMsg);
					}
					if (lMessages.size() > 1) {
						m_Buffer.AddLine(":" + _NAMEDFMT(WatchEntry.GetTarget()) + "!watch@znc.in PRIVMSG {target} :{text}", "---");
					}
				}
				sHandledTargets.insert(WatchEntry.GetTarget());
			}
		}

		if (sHandledTargets.empty()) {
			m_MessageCache.AddItem(sSource, lMessages);
		} else {
			m_MessageCache.RemItem(sSource);
		}
	}

	void SetDisabled(unsigned int uIdx, bool bDisabled) {
		if (uIdx == (unsigned int) ~0) {
			for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
				(*it).SetDisabled(bDisabled);
			}

			PutModule(((bDisabled) ? "Disabled all entries." : "Enabled all entries."));
			Save();
			return;
		}

		uIdx--; // "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++)
			++it;

		(*it).SetDisabled(bDisabled);
		PutModule("Id " + CString(uIdx +1) + ((bDisabled) ? " Disabled" : " Enabled"));
		Save();
	}

	void SetDetachedOnly(unsigned int uIdx, bool bDetachedOnly) {
		if (uIdx == (unsigned int) ~0) {
			for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
				(*it).SetDetachedOnly(bDetachedOnly);
			}

			PutModule(CString("Set DetachedOnly for all entries to: ") + ((bDetachedOnly) ? "Yes" : "No"));
			Save();
			return;
		}

		uIdx--; // "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++)
			++it;

		(*it).SetDetachedOnly(bDetachedOnly);
		PutModule("Id " + CString(uIdx +1) + " set to: " + ((bDetachedOnly) ? "Yes" : "No"));
		Save();
	}

	void List() {
		CTable Table;
		Table.AddColumn("Id");
		Table.AddColumn("HostMask");
		Table.AddColumn("Target");
		Table.AddColumn("Pattern");
		Table.AddColumn("Sources");
		Table.AddColumn("Off");
		Table.AddColumn("DetachedOnly");

		unsigned int uIdx = 1;

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it, uIdx++) {
			CWatchEntry& WatchEntry = *it;

			Table.AddRow();
			Table.SetCell("Id", CString(uIdx));
			Table.SetCell("HostMask", WatchEntry.GetHostMask());
			Table.SetCell("Target", WatchEntry.GetTarget());
			Table.SetCell("Pattern", WatchEntry.GetPattern());
			Table.SetCell("Sources", WatchEntry.GetSourcesStr());
			Table.SetCell("Off", (WatchEntry.IsDisabled()) ? "Off" : "");
			Table.SetCell("DetachedOnly", (WatchEntry.IsDetachedOnly()) ? "Yes" : "No");
		}

		if (Table.size()) {
			PutModule(Table);
		} else {
			PutModule("You have no entries.");
		}
	}

	void Dump() {
		if (m_lsWatchers.empty()) {
			PutModule("You have no entries.");
			return;
		}

		PutModule("---------------");
		PutModule("/msg " + GetModNick() + " CLEAR");

		unsigned int uIdx = 1;

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it, uIdx++) {
			CWatchEntry& WatchEntry = *it;

			PutModule("/msg " + GetModNick() + " ADD " + WatchEntry.GetHostMask() + " " +
				WatchEntry.GetTarget() + " " + WatchEntry.GetPattern());

			if (WatchEntry.GetSourcesStr().size()) {
				PutModule("/msg " + GetModNick() + " SETSOURCES " + CString(uIdx) + " " +
					WatchEntry.GetSourcesStr());
			}

			if (WatchEntry.IsDisabled()) {
				PutModule("/msg " + GetModNick() + " DISABLE " + CString(uIdx));
			}

			if (WatchEntry.IsDetachedOnly()) {
				PutModule("/msg " + GetModNick() + " ENABLEDETACHEDONLY " + CString(uIdx));
			}
		}

		PutModule("---------------");
	}

	void SetSources(unsigned int uIdx, const CString& sSources) {
		uIdx--; // "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++)
			++it;

		(*it).SetSources(sSources);
		PutModule("Sources set for Id " + CString(uIdx +1) + ".");
		Save();
	}

	void Remove(unsigned int uIdx) {
		uIdx--; // "convert" index to zero based
		if (uIdx >= m_lsWatchers.size()) {
			PutModule("Invalid Id");
			return;
		}

		list<CWatchEntry>::iterator it = m_lsWatchers.begin();
		for (unsigned int a = 0; a < uIdx; a++)
			++it;

		m_lsWatchers.erase(it);
		PutModule("Id " + CString(uIdx +1) + " Removed.");
		Save();
	}

	void Help() {
		CTable Table;

		Table.AddColumn("Command");
		Table.AddColumn("Description");

		Table.AddRow();
		Table.SetCell("Command", "Add <HostMask> [Target] [Pattern]");
		Table.SetCell("Description", "Used to add an entry to watch for.");

		Table.AddRow();
		Table.SetCell("Command", "List");
		Table.SetCell("Description", "List all entries being watched.");

		Table.AddRow();
		Table.SetCell("Command", "Dump");
		Table.SetCell("Description", "Dump a list of all current entries to be used later.");

		Table.AddRow();
		Table.SetCell("Command", "Del <Id>");
		Table.SetCell("Description", "Deletes Id from the list of watched entries.");

		Table.AddRow();
		Table.SetCell("Command", "Clear");
		Table.SetCell("Description", "Delete all entries.");

		Table.AddRow();
		Table.SetCell("Command", "Enable <Id | *>");
		Table.SetCell("Description", "Enable a disabled entry.");

		Table.AddRow();
		Table.SetCell("Command", "Disable <Id | *>");
		Table.SetCell("Description", "Disable (but don't delete) an entry.");

		Table.AddRow();
		Table.SetCell("Command", "EnableDetachedOnly <Id | *>");
		Table.SetCell("Description", "Enable detached only for a disabled entry.");

		Table.AddRow();
		Table.SetCell("Command", "DisableDetachedOnly <Id | *>");
		Table.SetCell("Description", "Disable (but don't delete) detached only for an entry.");

		Table.AddRow();
		Table.SetCell("Command", "Buffer [Count]");
		Table.SetCell("Description", "Show/Set the amount of buffered lines while detached.");

		Table.AddRow();
		Table.SetCell("Command", "SetSources <Id> [#chan priv #foo* !#bar]");
		Table.SetCell("Description", "Set the source channels that you care about.");

		Table.AddRow();
		Table.SetCell("Command", "Help");
		Table.SetCell("Description", "This help.");

		PutModule(Table);
	}

	void Watch(const CString& sHostMask, const CString& sTarget, const CString& sPattern, bool bNotice = false) {
		CString sMessage;

		pcrecpp::RE rHostMask(sHostMask);
		pcrecpp::RE rPattern(sPattern);
		if (sHostMask.size() && rHostMask.error().length() == 0 && rPattern.error().length() == 0) {
			CWatchEntry WatchEntry(sHostMask, sTarget, sPattern);

			bool bExists = false;
			for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
				if (*it == WatchEntry) {
					sMessage = "Entry for [" + WatchEntry.GetHostMask() + "] already exists.";
					bExists = true;
					break;
				}
			}

			if (!bExists) {
				sMessage = "Adding entry: [" + WatchEntry.GetHostMask() + "] watching for "
					"[" + WatchEntry.GetPattern() + "] -> [" + WatchEntry.GetTarget() + "]";
				m_lsWatchers.push_back(WatchEntry);
			}
		} else {
			if (!sHostMask.size()) {
				sMessage = "Watch: Not enough arguments.  Try Help";
			} else {
				if (rHostMask.error().size() != 0) {
					sMessage += CString("hostname: " + rHostMask.error());
				}
				if (rPattern.error().size() != 0) {
					sMessage += (sMessage.size() > 0 ? " | " : "") + CString("pattern: " + rPattern.error());
				}
			}
		}

		if (bNotice) {
			PutModNotice(sMessage);
		} else {
			PutModule(sMessage);
		}
		Save();
	}

	void Save() {
		ClearNV(false);
		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it) {
			CWatchEntry& WatchEntry = *it;
			CString sSave;

			sSave  = WatchEntry.GetHostMask() + "\n";
			sSave += WatchEntry.GetTarget() + "\n";
			sSave += WatchEntry.GetPattern() + "\n";
			sSave += (WatchEntry.IsDisabled() ? "disabled\n" : "enabled\n");
			sSave += (WatchEntry.IsDetachedOnly() ? "yes\n" : "no\n");
			sSave += WatchEntry.GetSourcesStr();
			// Without this, loading fails if GetSourcesStr()
			// returns an empty string
			sSave += " ";

			SetNV(sSave, "", false);
		}

		SaveRegistry();
	}

	void Load() {
		// Just to make sure we dont mess up badly
		m_lsWatchers.clear();

		bool bWarn = false;

		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			VCString vList;
			it->first.Split("\n", vList);

			if (vList.size() != 6) {
				bWarn = true;
				continue;
			}

			CWatchEntry WatchEntry(vList[0], vList[1], vList[2]);
			if (vList[3].Equals("disabled"))
				WatchEntry.SetDisabled(true);
			else
				WatchEntry.SetDisabled(false);
			if (vList[4].Equals("YES"))
				WatchEntry.SetDetachedOnly(true);
			else
				WatchEntry.SetDetachedOnly(false);
			WatchEntry.SetSources(vList[5]);
			m_lsWatchers.push_back(WatchEntry);
		}

		if (bWarn)
			PutModule("WARNING: malformed entry found while loading");
	}

	virtual CString GetWebMenuTitle() { return "watch2"; }

	virtual bool OnWebRequest(CWebSock& WebSock, const CString& sPageName, CTemplate& Tmpl) {
		if (sPageName == "index") {
			return true;
		} else if (sPageName == "configure") {
			return ConfigPage(WebSock, Tmpl);
		} else if (sPageName == "importexport") {
			return ImportExportPage(WebSock, Tmpl);
		}

		return false;
	}

	bool ConfigPage(CWebSock& WebSock, CTemplate& Tmpl) {
		Tmpl["Action"] = "configure";

		Tmpl["Title"] = "Configure";

		if (WebSock.IsPost()) {
			unsigned int uIdx = 1, uId = 1;
			CString sIdx(uIdx), sId(uId);
			m_lsWatchers.clear();

			for (; WebSock.HasParam("has_" + sId); uId++, sIdx = CString(uIdx), sId = CString(uId)) {
				if (WebSock.GetParam("has_" + sId) == "0")
					continue;

				OnModCommand("ADD " + WebSock.GetParam("hostmask_" + sId) + " " + WebSock.GetParam("target_" + sId) + " " + WebSock.GetParam("pattern_" + sId));

				if (!WebSock.GetParam("sources_" + sId).empty())
					OnModCommand("SETSOURCES " + sIdx + " " + WebSock.GetParam("sources_" + sId));

				if (WebSock.HasParam("off_" + sId))
					OnModCommand("DISABLE " + sIdx);

				if (WebSock.HasParam("detachedonly_" + sId))
					OnModCommand("ENABLEDETACHEDONLY " + sIdx);

				uIdx++;
			}

			Save();
		}

		unsigned int uIdx = 1;

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it, uIdx++) {
			CWatchEntry& WatchEntry = *it;

			CTemplate& Row = Tmpl.AddRow("ConfigureLoop");
			Row["Id"] = CString(uIdx);
			Row["HostMask"] = WatchEntry.GetHostMask();
			Row["Target"] = WatchEntry.GetTarget();
			Row["Pattern"] = WatchEntry.GetPattern();
			Row["Sources"] = WatchEntry.GetSourcesStr();
			Row["Off"] = CString(WatchEntry.IsDisabled());
			Row["DetachedOnly"] = CString(WatchEntry.IsDetachedOnly());
		}

		return true;
	}

	bool ImportExportPage(CWebSock& WebSock, CTemplate& Tmpl) {
		Tmpl["Action"] = "importexport";

		Tmpl["Title"] = "Import/Export";

		if (WebSock.IsPost()) {
			VCString vsPerf;
			WebSock.GetRawParam("importexport").Split("\n", vsPerf);
			m_lsWatchers.clear();

			for (VCString::const_iterator it = vsPerf.begin(); it != vsPerf.end(); ++it) {
				if (!it->Trim_n().empty()) {
					OnModCommand(it->Trim_n());
				}
			}

			Save();
		}

		unsigned int uIdx = 1;
		CString sIdx(uIdx);

		for (list<CWatchEntry>::iterator it = m_lsWatchers.begin(); it != m_lsWatchers.end(); ++it, uIdx++, sIdx = CString(uIdx)) {
			CWatchEntry& WatchEntry = *it;

			Tmpl.AddRow("ImportExportLoop")["ImportExport"] = "ADD " + WatchEntry.GetHostMask() + " " + WatchEntry.GetTarget() + " " + WatchEntry.GetPattern();

			if (WatchEntry.GetSourcesStr().size()) {
				Tmpl.AddRow("ImportExportLoop")["ImportExport"] = "SETSOURCES " + sIdx + " " + WatchEntry.GetSourcesStr();
			}

			if (WatchEntry.IsDisabled()) {
				Tmpl.AddRow("ImportExportLoop")["ImportExport"] = "DISABLE " + sIdx;
			}

			if (WatchEntry.IsDetachedOnly()) {
				Tmpl.AddRow("ImportExportLoop")["ImportExport"] = "ENABLEDETACHEDONLY " + sIdx;
			}
		}

		return true;
	}

	list<CWatchEntry>  m_lsWatchers;
	CBuffer            m_Buffer;
	unsigned int                        m_uiLength;
	TCacheMap<CString, list<CString> >  m_MessageCache;
};

template<> void TModInfo<CWatcherMod>(CModInfo& Info) {
	Info.SetWikiPage("watch");
}

NETWORKMODULEDEFS(CWatcherMod, "Copy activity from a specific user into a separate window")
