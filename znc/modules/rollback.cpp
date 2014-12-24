#include <znc/Chan.h>
#include <znc/User.h>
#include <znc/IRCNetwork.h>
#include <fstream>
#include <dirent.h>
#include <errno.h>
#include <pcrecpp.h>

using std::list;
using std::vector;
using std::set;

class CRollbackMod : public CModule {
public:
	MODCONSTRUCTOR(CRollbackMod) {
		m_uiLineCount = 30;
		m_sPathToLog = "../log";
		m_sFilename = "*.log";
		Load();
	}

	virtual ~CRollbackMod() {}

	virtual void OnModCommand(const CString& sCommand) {
		CString sCmdName = sCommand.Token(0);

		if (sCmdName.Equals("HELP")) {
			Help();
		} else if (sCmdName.Equals("SET")) {
			Set(sCommand.Token(1), sCommand.Token(2, true));
		} else if (sCmdName.Equals("LIST")) {
			List();
		} else if (sCmdName.Equals("TEST")) {
			Test(sCommand.Token(1), sCommand.Token(2, true));
		} else if (sCmdName.Equals("GREP") || sCmdName.Equals("SEARCH")) {
			Search(sCommand.Token(1), sCommand.Token(2, true));
		} else {
			PutModule("Unknown command: [" + sCmdName + "]");
		}
	}

private:
	void Help() {
		CTable Table;

		Table.AddColumn("Command");
		Table.AddColumn("Description");

		Table.AddRow();
		Table.SetCell("Command", "Help");
		Table.SetCell("Description", "This help.");

		Table.AddRow();
		Table.SetCell("Command", "Set <LineCount | PathToLog | Filename> <Value>");
		Table.SetCell("Description", "Set variables.");

		Table.AddRow();
		Table.SetCell("Command", "List");
		Table.SetCell("Description", "Show all variables.");

		Table.AddRow();
		Table.SetCell("Command", "Test <Channel | User> <Path>");
		Table.SetCell("Description", "Display the contents of Path.");

		Table.AddRow();
		Table.SetCell("Command", "Search <Channel | User> <Pattern>");
		Table.SetCell("Description", "Search for Pattern in the logs.");

		PutModule(Table);
	}

	void Set(const CString& sKey, const CString& sValue) {
		if (sKey.Equals("LINECOUNT")) {
			m_uiLineCount = sValue.ToUInt();
			PutModule("Set LineCount to [" + sValue + "]");
		} else if (sKey.Equals("PATHTOLOG")) {
			m_sPathToLog = sValue;
			PutModule("Set PathToLog to [" + sValue + "] [" + ReplaceTags(sValue, "#example") + "]");
		} else if (sKey.Equals("FILENAME")) {
			m_sFilename = sValue;
			PutModule("Set Filename to [" + sValue + "] [" + ReplaceTags(sValue, "#example") + "]");
		} else {
			PutModule("Unknown key: [" + sKey + "]");
		}

		Save();
	}

	void List() {
		CTable Table;

		Table.AddColumn("Key");
		Table.AddColumn("Value");

		Table.AddRow();
		Table.SetCell("Key", "LineCount");
		Table.SetCell("Value", CString(m_uiLineCount));

		Table.AddRow();
		Table.SetCell("Key", "PathToLog");
		Table.SetCell("Value", m_sPathToLog + " [" + ReplaceTags(m_sPathToLog, "#example") + "]");

		Table.AddRow();
		Table.SetCell("Key", "Filename");
		Table.SetCell("Value", m_sFilename + " [" + ReplaceTags(m_sFilename, "#example") + "]");

		PutModule(Table);
	}

	void Test(const CString& sWindow, const CString& sPathToLog) {
		CString sDir = GetSavePath() + "/" + ReplaceTags(sPathToLog, sWindow);

		DIR *dirStream;
		struct dirent *dirEntry;
		if ((dirStream = opendir(sDir.c_str())) == NULL) {
			switch (errno) {
			case ENOENT:
				PutModule("No such file or directory [" + sDir + "]");
				break;
			default:
				PutModule("Error [" + CString(errno) + "] opening [" + sDir + "]");
				break;
			}
			return;
		}

		while ((dirEntry = readdir(dirStream)) != NULL) {
			PutModule(CString(dirEntry->d_type & DT_DIR ? "[D] " : "[F] ") + dirEntry->d_name);
		}
		closedir(dirStream);
	}

	void Search(const CString& sWindow, const CString& sPattern) {
		unsigned int uiLineCount = m_uiLineCount;
		CString sDir = GetSavePath() + "/" + ReplaceTags(m_sPathToLog, sWindow);

		pcrecpp::RE rPattern("(?i)" + sPattern);
		if (rPattern.error().size() != 0) {
			PutModule(rPattern.error());
			return;
		}

		DIR *dirStream;
		if ((dirStream = opendir(sDir.c_str())) == NULL) {
			switch (errno) {
			case ENOENT:
				PutModule("No such file or directory [" + sDir + "]");
				break;
			default:
				PutModule("Error [" + CString(errno) + "] opening [" + sDir + "]");
				break;
			}
			return;
		}

		VCString files;
		struct dirent *dirEntry;
		while ((dirEntry = readdir(dirStream)) != NULL) {
			if (!(dirEntry->d_type & DT_DIR)) {
				CString sName = CString(dirEntry->d_name);
				if (sName.WildCmp("*-*-*.log")) {
					files.push_back(sName);
				}
			}
		}
		closedir(dirStream);

		VCString rollback;
		for (VCString::reverse_iterator itFiles = files.rbegin(); itFiles != files.rend(); ++itFiles) {
			CString sName = *itFiles;
			CString sFullName = sDir + "/" + sName;
			unsigned int uiSize = 0;

			bool bDate = false;
			std::string sYear, sMonth, sDay;
			pcrecpp::RE rDate(ReplaceTags(m_sFilename, sWindow).Replace_n("*", "(\\d{4})-(\\d{2})-(\\d{2})"));
			if (rDate.error().size() == 0 && rDate.FullMatch(sName, &sYear, &sMonth, &sDay)) {
				bDate = true;
			}

			uiSize = rollback.size();
			std::ifstream in(sFullName.c_str());
			if (in.is_open()) {
				VCString lines_in_order;
				std::string line;
				while (std::getline(in, line)) {
					lines_in_order.push_back(line);
				}

				for (VCString::reverse_iterator it = lines_in_order.rbegin(); it != lines_in_order.rend() && uiLineCount > 0; ++it) {
					if (rPattern.PartialMatch(*it)) {
						if (bDate) {
							if (!it->TrimLeft_n("[").Equals(*it)) {
								rollback.push_back("[" + sYear + "-" + sMonth + "-" + sDay + " " + it->TrimLeft_n("["));
							} else {
								rollback.push_back("[" + sYear + "-" + sMonth + "-" + sDay + "] " + *it);
							}
						} else {
							rollback.push_back(*it);
						}
						uiLineCount--;
					}
				}
			}

			if (!bDate && uiSize < rollback.size()) {
				rollback.push_back("========= " + sName + " =========");
			}

			if (uiLineCount == 0) {
				break;
			}
		}

		for (VCString::reverse_iterator it = rollback.rbegin(); it != rollback.rend(); ++it) {
			PutModule(*it);
		}

		if (uiLineCount == m_uiLineCount) {
			PutModule("No results.");
		}
	}

	void Save() {
		ClearNV(false);

		SetNV("LINECOUNT\n" + CString(m_uiLineCount), "", false);
		SetNV("PATHTOLOG\n" + CString(m_sPathToLog), "", false);
		SetNV("FILENAME\n" + CString(m_sFilename), "", false);

		SaveRegistry();
	}

	void Load() {
		bool bWarn = false;

		for (MCString::iterator it = BeginNV(); it != EndNV(); ++it) {
			VCString vList;
			it->first.Split("\n", vList);

			if (vList.size() != 2) {
				bWarn = true;
				continue;
			}

			if (vList[0].Equals("LINECOUNT")) {
				m_uiLineCount = vList[1].ToUInt();
			} else if (vList[0].Equals("PATHTOLOG")) {
				m_sPathToLog = vList[1];
			} else if (vList[0].Equals("FILENAME")) {
				m_sFilename = vList[1];
			}
		}

		if (bWarn)
			PutModule("WARNING: malformed entry found while loading");
	}

	CString ReplaceTags(const CString& sOriginal, const CString& sWindow) {
		CString sStr = sOriginal;
		sStr.Replace("$NETWORK", (m_pNetwork ? m_pNetwork->GetName() : "znc"));
		sStr.Replace("$WINDOW", sWindow.Replace_n("/", "?"));
		sStr.Replace("$USER", (m_pUser ? m_pUser->GetUserName() : "UNKNOWN"));
		return sStr;
	}

	unsigned int  m_uiLineCount;
	CString       m_sPathToLog;
	CString       m_sFilename;
};

template<> void TModInfo<CRollbackMod>(CModInfo& Info) {
	Info.AddType(CModInfo::UserModule);
	Info.AddType(CModInfo::GlobalModule);
}

NETWORKMODULEDEFS(CRollbackMod, "Find things in logs, with regex!")
