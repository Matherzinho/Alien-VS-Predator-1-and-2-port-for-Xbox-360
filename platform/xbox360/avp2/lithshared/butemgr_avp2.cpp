// avp360: CButeMgr para o cabecalho do AvP2 (proj/LT2/lithshared/incs/butemgr.h).
//
// O release do AvP2 so trouxe o .lib x86. O analisador (scanner, tabelas da scangen, gramatica) e o do NOLF
// (nolfsrc-master/src/lithshared/ButeMgr/butemgr.cpp): o CMake extrai dele as linhas do scanner para
// butemgr_nolf_scanner.inc, sem copia-las para ca. O que muda no AvP2 e o armazenamento: hash_map da STL,
// bancos de objetos e um conjunto de strings (AddString). A semantica de cada Get/Set segue a do NOLF:
// m_bSuccess, valores padrao, e onde um valor novo vai parar (m_auxTagTab para uma tag do arquivo,
// m_newTagTab para uma tag nova).
#include "mfcstub.h"
#include "butemgr.h"
#include <float.h>
#include <limits.h>
#include <new>
#include <stdio.h>
#include <stdarg.h>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifndef TRACE
#define TRACE(...) ((void)0)
#endif

#include "butemgr_nolf_scanner.inc"

// ---------------------------------------------------------------------------------------------------------------
// Construcao, estado

CButeMgr::CButeMgr()
	: m_RectBank(32), m_PointBank(32), m_AVectorBank(32), m_ARangeBank(32), m_SymTabItemBank(128)
{
	m_decryptCode = 0;
	m_checksum = 0;
	m_pDisplayFunc = NULL;
	m_lineNumber = 0;
	m_bLineCounterFlag = true;
	m_bPutChar = false;
	m_bCrypt = false;
	m_bSuccess = true;
	m_bErrorFlag = false;
	m_pCurrTabOfItems = NULL;
	m_pData = NULL;
	m_pSaveData = NULL;
	m_szLineBuffer = NULL;
	m_szLineBufferPtr = NULL;
	m_currentChar = 0;
	m_token = m_tokenMinor = 0;
	m_szTokenString[0] = 0;
}

CButeMgr::~CButeMgr()
{
	Term();
}

void CButeMgr::Init() {}

void CButeMgr::Init(void (*pF)(const char *szMsg))
{
	m_pDisplayFunc = pF;
}

bool CButeMgr::TraverseTableOfItemsDeleteSymTab(char const *, CSymTabItem &theSymTab, void *pContext)
{
	CButeMgr *pThis = (CButeMgr *)pContext;
	theSymTab.Term();
	pThis->m_SymTabItemBank.Free(&theSymTab);
	return true;
}

bool CButeMgr::TraverseTableOfTagsDeleteSymTab(char const *, TableOfItems &theTableOfItems, void *pContext)
{
	TraverseTableOfItems(theTableOfItems, TraverseTableOfItemsDeleteSymTab, pContext);
	delete &theTableOfItems;
	return true;
}

void CButeMgr::Term()
{
	TableOfTags *tabs[] = { &m_tagTab, &m_tagTab2, &m_auxTagTab, &m_newTagTab };
	for (size_t i = 0; i < sizeof tabs / sizeof tabs[0]; i++) {
		TraverseTableOfTags(*tabs[i], TraverseTableOfTagsDeleteSymTab, this);
		tabs[i]->clear();
	}
	m_pCurrTabOfItems = NULL;
	m_stringHolder.clear();
}

void CButeMgr::Reset()
{
	m_checksum = 0;
	m_lineNumber = 0;
	m_bLineCounterFlag = true;
	m_bErrorFlag = false;
	m_sTagName = "";
	m_sAttribute = "";
}

void CButeMgr::DisplayMessage(const char *szMsg, ...)
{
	va_list v;
	va_start(v, szMsg);
	CString str;
	str.FormatV(szMsg, v);
	va_end(v);
	m_sErrorString.Format("ButeMgr (%s, %d):  %s", (const char *)m_sAttributeFilename, m_lineNumber, (const char *)str);
	if (m_pDisplayFunc)
		(*m_pDisplayFunc)(m_sErrorString.GetBuffer(0));
}

// ---------------------------------------------------------------------------------------------------------------
// Armazenamento

CString const &CButeMgr::AddString(char const *pszString)
{
	// Os nos do hash_set nao mudam de endereco: as chaves das tabelas apontam para estas strings.
	return *m_stringHolder.insert(CString(pszString ? pszString : "")).first;
}

CString const &CButeMgr::AddString(CString const &sString)
{
	return AddString((const char *)sString);
}

CButeMgr::TableOfItems *CButeMgr::CreateTableOfItems(TableOfTags &tableOfTags, char const *pszTagName)
{
	TableOfItems *pTab = new TableOfItems;
	tableOfTags[(const char *)AddString(pszTagName)] = pTab;
	return pTab;
}

CButeMgr::TableOfItems *CButeMgr::CreateTableOfItems(TableOfTags &tableOfTags, CString const &sTagName)
{
	return CreateTableOfItems(tableOfTags, (const char *)sTagName);
}

CButeMgr::TableOfItems *CButeMgr::FindTableOfItems(TableOfTags &tableOfTags, char const *pszTagName)
{
	if (!pszTagName) return NULL;
	TableOfTags::iterator it = tableOfTags.find(pszTagName);
	return it == tableOfTags.end() ? NULL : it->second;
}

CButeMgr::TableOfItems *CButeMgr::FindTableOfItems(TableOfTags &tableOfTags, CString const &sTagName)
{
	return FindTableOfItems(tableOfTags, (const char *)sTagName);
}

CButeMgr::CSymTabItem *CButeMgr::CreateSymTabItem(TableOfItems &tableOfItems, char const *pszAttName)
{
	CSymTabItem *pItem = m_SymTabItemBank.Allocate();
	if (!pItem) return NULL;
	new (pItem) CSymTabItem;
	tableOfItems[(const char *)AddString(pszAttName)] = pItem;
	return pItem;
}

CButeMgr::CSymTabItem *CButeMgr::CreateSymTabItem(TableOfItems &tableOfItems, CString const &sAttName)
{
	return CreateSymTabItem(tableOfItems, (const char *)sAttName);
}

CButeMgr::CSymTabItem *CButeMgr::FindSymTabItem(TableOfItems &tableOfItems, char const *pszAttName)
{
	if (!pszAttName) return NULL;
	TableOfItems::iterator it = tableOfItems.find(pszAttName);
	return it == tableOfItems.end() ? NULL : it->second;
}

CButeMgr::CSymTabItem *CButeMgr::FindSymTabItem(TableOfItems &tableOfItems, CString const &sAttName)
{
	return FindSymTabItem(tableOfItems, (const char *)sAttName);
}

// Como o GetItem do NOLF: tabela do arquivo, depois a auxiliar, depois as tags novas.
CButeMgr::CSymTabItem *CButeMgr::FindSymTabItem(char const *pszTagName, char const *pszAttName)
{
	TableOfTags *tabs[] = { &m_tagTab, &m_auxTagTab, &m_newTagTab };
	for (size_t i = 0; i < sizeof tabs / sizeof tabs[0]; i++) {
		TableOfItems *pTab = FindTableOfItems(*tabs[i], pszTagName);
		CSymTabItem *pItem = pTab ? FindSymTabItem(*pTab, pszAttName) : NULL;
		if (pItem) return pItem;
	}
	return NULL;
}

CButeMgr::CSymTabItem *CButeMgr::FindSymTabItem(CString const &sTagName, CString const &sAttName)
{
	return FindSymTabItem((const char *)sTagName, (const char *)sAttName);
}

// Para os Set*: o item existente, ou um novo onde o NOLF o criava.
CButeMgr::CSymTabItem *CButeMgr::GetSymTabItem(char const *pszTagName, char const *pszAttName)
{
	m_bSuccess = true;
	if (FindTableOfItems(m_tagTab, pszTagName)) {
		CSymTabItem *pItem = FindSymTabItem(*FindTableOfItems(m_tagTab, pszTagName), pszAttName);
		if (pItem) return pItem;
		TableOfItems *pAux = FindTableOfItems(m_auxTagTab, pszTagName);
		if (!pAux) pAux = CreateTableOfItems(m_auxTagTab, pszTagName);
		pItem = FindSymTabItem(*pAux, pszAttName);
		return pItem ? pItem : CreateSymTabItem(*pAux, pszAttName);
	}
	TableOfItems *pNew = FindTableOfItems(m_newTagTab, pszTagName);
	if (!pNew) pNew = CreateTableOfItems(m_newTagTab, pszTagName);
	CSymTabItem *pItem = FindSymTabItem(*pNew, pszAttName);
	return pItem ? pItem : CreateSymTabItem(*pNew, pszAttName);
}

CButeMgr::CSymTabItem *CButeMgr::GetSymTabItem(CString const &sTagName, CString const &sAttName)
{
	return GetSymTabItem((const char *)sTagName, (const char *)sAttName);
}

void CButeMgr::TraverseTableOfTags(TableOfTags &theTableOfTags, TraverseTableOfTagsCallback pCb, void *pContext)
{
	// Copia antes: o callback pode apagar a tabela.
	std::vector<std::pair<char const *, TableOfItems *> > all(theTableOfTags.begin(), theTableOfTags.end());
	for (size_t i = 0; i < all.size(); i++)
		if (!pCb(all[i].first, *all[i].second, pContext)) return;
}

void CButeMgr::TraverseTableOfItems(TableOfItems &theTableOfItems, TraverseTableOfItemsCallback pCb, void *pContext)
{
	std::vector<std::pair<char const *, CSymTabItem *> > all(theTableOfItems.begin(), theTableOfItems.end());
	for (size_t i = 0; i < all.size(); i++)
		if (!pCb(all[i].first, *all[i].second, pContext)) return;
}

// ---------------------------------------------------------------------------------------------------------------
// Scanner: palavras reservadas (TRUE/FALSE, as unicas do NOLF) e a gramatica

void CButeMgr::LookupCodes(short State, unsigned char currentChar)
{
	m_token = TransTable[State][CharClass(currentChar)].A;
	m_tokenMinor = TransTable[State][CharClass(currentChar)].B;
	if (m_token == IdTok) {
		if (strcmp(m_szTokenString, "FALSE") == 0) { m_token = BoolTok; m_tokenMinor = 0; }
		else if (strcmp(m_szTokenString, "TRUE") == 0) { m_token = BoolTok; m_tokenMinor = 1; }
	}
}

bool CButeMgr::Statement()
{
	int left, top, right, bottom, x, y;
	double i, j, k;
	bool bDuplicate = false;

	m_sAttribute = m_szTokenString;
	if (!m_pSaveData && FindSymTabItem(*m_pCurrTabOfItems, m_sAttribute)) {
		DisplayMessage("duplicate symbol encountered - %s", (const char *)m_sAttribute);
		bDuplicate = true;
	}
	if (!Match(EqualTok))
		return false;
	if (m_pSaveData) {
		m_bPutChar = false;
		*m_pSaveData << ' ';
	}
	if (!ScanTok())
		return false;

	// Ao carregar: um item novo na tag corrente (a nao ser que seja repetido).
	CSymTabItem *pNew = (!m_pSaveData && !bDuplicate) ? CreateSymTabItem(*m_pCurrTabOfItems, m_sAttribute) : NULL;
	switch (m_token) {
	case PosIntegerTok:
	case NegIntegerTok:
		if (pNew) pNew->Init(*this, IntType, atoi(m_szTokenString));
		else if (m_pSaveData) *m_pSaveData << GetInt(m_sTagName, m_sAttribute);
		break;
	case CastDwordTok:
		if (!Match(PosIntegerTok)) return false;
		if (pNew) pNew->Init(*this, DwordType, (DWORD)strtoul(m_szTokenString, NULL, 10));
		else if (m_pSaveData) *m_pSaveData << "(DWORD)" << GetDword(m_sTagName, m_sAttribute);
		break;
	case CastByteTok:
		if (!Match(PosIntegerTok)) return false;
		if (pNew) pNew->Init(*this, ByteType, (BYTE)atoi(m_szTokenString));
		else if (m_pSaveData) *m_pSaveData << "(BYTE)" << (DWORD)GetByte(m_sTagName, m_sAttribute);
		break;
	case BoolTok:
		if (pNew) pNew->Init(*this, BoolType, (bool)(m_tokenMinor == 1));
		else if (m_pSaveData) *m_pSaveData << (GetBool(m_sTagName, m_sAttribute) ? "TRUE" : "FALSE");
		break;
	case CastFloatTok:
		if (!Match(DoubleTok)) return false;
		if (pNew) pNew->Init(*this, FloatType, (float)atof(m_szTokenString));
		else if (m_pSaveData) *m_pSaveData << "(FLOAT)" << GetFloat(m_sTagName, m_sAttribute);
		break;
	case FloatTok:
		if (pNew) pNew->Init(*this, FloatType, (float)atof(m_szTokenString));
		else if (m_pSaveData) *m_pSaveData << GetFloat(m_sTagName, m_sAttribute) << "f";
		break;
	case DoubleTok:
		if (pNew) pNew->Init(*this, DoubleType, atof(m_szTokenString));
		else if (m_pSaveData) *m_pSaveData << GetDouble(m_sTagName, m_sAttribute);
		break;
	case RectTok:
		sscanf(m_szTokenString, "(%d, %d, %d, %d)", &left, &top, &right, &bottom);
		if (pNew) pNew->Init(*this, RectType, CRect(left, top, right, bottom));
		else if (m_pSaveData) {
			CRect rc = GetRect(m_sTagName, m_sAttribute);
			*m_pSaveData << "(" << rc.left << ", " << rc.top << ", " << rc.right << ", " << rc.bottom << ")";
		}
		break;
	case PointTok:
		sscanf(m_szTokenString, "(%d, %d)", &x, &y);
		if (pNew) pNew->Init(*this, PointType, CPoint(x, y));
		else if (m_pSaveData) {
			CPoint pt = GetPoint(m_sTagName, m_sAttribute);
			*m_pSaveData << "(" << pt.x << ", " << pt.y << ")";
		}
		break;
	case VectorTok:
		sscanf(m_szTokenString, "<%lf, %lf, %lf>", &i, &j, &k);
		if (pNew) pNew->Init(*this, VectorType, CAVector(i, j, k));
		else if (m_pSaveData) {
			CAVector v = GetVector(m_sTagName, m_sAttribute);
			*m_pSaveData << "<" << v.Geti() << ", " << v.Getj() << ", " << v.Getk() << ">";
		}
		break;
	case RangeTok:
		sscanf(m_szTokenString, "[%lf, %lf]", &i, &j);
		if (pNew) pNew->Init(*this, RangeType, CARange(i, j));
		else if (m_pSaveData) {
			CARange r = GetRange(m_sTagName, m_sAttribute);
			*m_pSaveData << "[" << r.GetMin() << ", " << r.GetMax() << "]";
		}
		break;
	case StringTok:
		if (pNew) pNew->Init(*this, StringType, CString(m_szTokenString));
		else if (m_pSaveData) {
			CString s = GetString(m_sTagName, m_sAttribute);
			s.Replace("\"", "\"\"");
			*m_pSaveData << '"' << (const char *)s << '"';
		}
		break;
	default:
		DisplayMessage("Invalid token encountered.");
		return false;
	}
	if (m_pSaveData)
		m_bPutChar = true;
	return true;
}

bool CButeMgr::StatementList()
{
	for (;;) {
		if (!Statement()) return false;
		if (!ScanTok()) return false;
		if (m_token == TagBegTok || m_token == EofTok) return true;
		if (m_token != IdTok) return false;
	}
}

bool CButeMgr::Tag()
{
	if (!Match(IdTok))
		return false;
	m_sTagName = m_szTokenString;
	if (!m_pSaveData) {
		if (FindTableOfItems(m_tagTab, m_szTokenString)) {
			DisplayMessage("Duplicate tag encountered - %s", m_szTokenString);
			return false;
		}
		m_pCurrTabOfItems = CreateTableOfItems(m_tagTab, m_szTokenString);
	}
	return Match(TagEndTok);
}

bool CButeMgr::AuxTabItemsSave(const char *sz, CSymTabItem &item, void *p)
{
	ostream &out = *(ostream *)p;
	out << "\r\n" << sz << " = ";
	switch (item.SymType) {
	case IntType: out << item.data.i; break;
	case DwordType: out << "(DWORD)" << item.data.dw; break;
	case ByteType: out << "(BYTE)" << (DWORD)item.data.byte; break;
	case BoolType: out << (item.data.b ? "TRUE" : "FALSE"); break;
	case FloatType: out << item.data.f << 'f'; break;
	case DoubleType: out << item.data.d; break;
	case StringType: {
		CString s = *item.data.s;
		s.Replace("\"", "\"\"");
		out << '"' << (const char *)s << '"';
		break;
	}
	case RectType: out << '(' << item.data.r->left << ", " << item.data.r->top << ", " << item.data.r->right << ", " << item.data.r->bottom << ')'; break;
	case PointType: out << "(" << item.data.point->x << ", " << item.data.point->y << ")"; break;
	case VectorType: out << "<" << item.data.v->Geti() << ", " << item.data.v->Getj() << ", " << item.data.v->Getk() << ">"; break;
	case RangeType: out << "[" << item.data.range->GetMin() << ", " << item.data.range->GetMax() << "]"; break;
	default: break;
	}
	return true;
}

bool CButeMgr::NewTabsSave(const char *sz, TableOfItems &tab, void *p)
{
	*(ostream *)p << "\r\n\r\n" << "[" << sz << "]";
	TraverseTableOfItems(tab, AuxTabItemsSave, p);
	return true;
}

bool CButeMgr::TagList()
{
	ConsumeChar();
	if (!ScanTok()) return false;
	if (m_token == EofTok) return true;
	if (m_token != TagBegTok) return false;
	while (m_token != EofTok) {
		if (!Tag()) return false;
		if (m_pSaveData) {
			TableOfItems *pAux = FindTableOfItems(m_auxTagTab, m_sTagName);
			if (pAux) TraverseTableOfItems(*pAux, AuxTabItemsSave, m_pSaveData);
		}
		if (!ScanTok()) return false;
		if (m_token == EofTok) return true;
		if (m_token == TagBegTok) continue;
		if (m_token != IdTok) return false;
		if (!StatementList()) return false;
	}
	return true;
}

// Regrava o arquivo de atributos com os valores atuais, como o NOLF: le o arquivo de novo e reescreve cada valor.
bool CButeMgr::Save(const char *szNewFileName)
{
	Reset();
	if (m_sAttributeFilename.IsEmpty() && !szNewFileName)
		return false;
	m_bPutChar = true;
	std::ifstream is((const char *)m_sAttributeFilename, std::ios::in | std::ios::binary);
	std::string original;
	if (is.is_open()) {
		char buf[4096];
		while (is.read(buf, sizeof buf) || is.gcount()) original.append(buf, (size_t)is.gcount());
	}
	is.close();
	std::istringstream src(original);
	if (m_bCrypt) {
		std::ostringstream plain;
		m_cryptMgr.Decrypt(src, plain);
		src.str(plain.str());
		src.clear();
	}
	std::stringstream out(std::ios::in | std::ios::out | std::ios::binary);
	m_pSaveData = &out;
	out.flags(out.flags() | std::ios::showpoint | std::ios::fixed);
	m_pData = &src;
	TagList();
	TraverseTableOfTags(m_newTagTab, NewTabsSave, m_pSaveData);
	m_pSaveData = NULL;
	m_pData = NULL;
	m_bPutChar = false;

	const char *path = szNewFileName ? szNewFileName : (const char *)m_sAttributeFilename;
	std::ofstream os(path, std::ios::out | std::ios::binary | std::ios::trunc);
	if (!os.is_open())
		return false;
	if (m_bCrypt) {
		std::istringstream plain(out.str());
		m_cryptMgr.Encrypt(plain, os);
	} else {
		os << out.str();
	}
	return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Consultas

bool CButeMgr::GetTagsTraverseFunc(char const *pszTagName, TableOfItems &, void *pContext)
{
	GetTagsData *p = (GetTagsData *)pContext;
	return p->m_pCallback(pszTagName, p->m_pContext);
}

void CButeMgr::GetTags(GetTagsCallback pCallback, void *pContext)
{
	GetTagsData d(pCallback, pContext);
	TraverseTableOfTags(m_tagTab, GetTagsTraverseFunc, &d);
	TraverseTableOfTags(m_auxTagTab, GetTagsTraverseFunc, &d);
	TraverseTableOfTags(m_newTagTab, GetTagsTraverseFunc, &d);
}

bool CButeMgr::GetKeysTraverseFunc(char const *pszAttName, CSymTabItem &theItem, void *pContext)
{
	GetKeysData *p = (GetKeysData *)pContext;
	return p->m_pCallback(pszAttName, &theItem, p->m_pContext);
}

void CButeMgr::GetKeys(const char *pszTagName, GetKeysCallback pCallback, void *pContext)
{
	GetKeysData d(pCallback, pContext);
	TableOfTags *tabs[] = { &m_tagTab, &m_auxTagTab, &m_newTagTab };
	for (size_t i = 0; i < sizeof tabs / sizeof tabs[0]; i++) {
		TableOfItems *pTab = FindTableOfItems(*tabs[i], pszTagName);
		if (pTab) TraverseTableOfItems(*pTab, GetKeysTraverseFunc, &d);
	}
}

// Leitura comum: com padrao, sem mensagem se faltar; sem padrao, "Symbol not found".
#define BUTE_GET(T, Ret, Field, Fallback)                                                                     \
	Ret CButeMgr::Get##T(const char *szTagName, const char *szAttName, Ret defVal)                          \
	{                                                                                                          \
		m_bSuccess = true;                                                                                     \
		CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);                                             \
		if (pItem) {                                                                                           \
			if (pItem->SymType == T##Type) return Field;                                                    \
			DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);                                   \
		}                                                                                                      \
		m_bSuccess = false;                                                                                    \
		return defVal;                                                                                         \
	}                                                                                                          \
	Ret CButeMgr::Get##T(const char *szTagName, const char *szAttName)                                      \
	{                                                                                                          \
		m_bSuccess = true;                                                                                     \
		CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);                                             \
		if (pItem) {                                                                                           \
			if (pItem->SymType == T##Type) return Field;                                                    \
			DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);                                   \
		} else                                                                                                 \
			DisplayMessage("Symbol not found - [%s]:%s", szTagName, szAttName);                                \
		m_bSuccess = false;                                                                                    \
		return Fallback;                                                                                       \
	}

BUTE_GET(Int, int, pItem->data.i, (int)INT_MIN)
BUTE_GET(Dword, DWORD, pItem->data.dw, (DWORD)0)
BUTE_GET(Byte, BYTE, pItem->data.byte, (BYTE)0)
BUTE_GET(Bool, bool, pItem->data.b, false)
BUTE_GET(Float, float, pItem->data.f, -FLT_MAX)
BUTE_GET(Double, double, pItem->data.d, -DBL_MAX)

#define BUTE_SET(T, Arg)                                                                                     \
	void CButeMgr::Set##T(const char *szTagName, const char *szAttName, Arg val)                            \
	{                                                                                                          \
		CSymTabItem *pItem = GetSymTabItem(szTagName, szAttName);                                              \
		if (pItem) pItem->Init(*this, T##Type, val);                                                        \
	}
BUTE_SET(Int, int)
BUTE_SET(Dword, DWORD)
BUTE_SET(Byte, BYTE)
BUTE_SET(Bool, bool)
BUTE_SET(Float, float)
BUTE_SET(Double, double)
BUTE_SET(String, const CString &)
BUTE_SET(Rect, const CRect &)
BUTE_SET(Point, const CPoint &)
BUTE_SET(Vector, const CAVector &)
BUTE_SET(Range, const CARange &)

CString CButeMgr::GetString(const char *szTagName, const char *szAttName, const CString &defVal)
{
	m_bSuccess = true;
	CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);
	if (pItem) {
		if (pItem->SymType == StringType) return *pItem->data.s;
		DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);
	}
	m_bSuccess = false;
	return defVal;
}

CString CButeMgr::GetString(const char *szTagName, const char *szAttName)
{
	m_bSuccess = true;
	CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);
	if (pItem) {
		if (pItem->SymType == StringType) return *pItem->data.s;
		DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);
	} else
		DisplayMessage("Symbol not found - [%s]:%s", szTagName, szAttName);
	m_bSuccess = false;
	return CString("");
}

void CButeMgr::GetString(const char *szTagName, const char *szAttName, const char *defVal, char *szResult, DWORD maxLen)
{
	if (!szResult || !maxLen) return;
	m_bSuccess = true;
	const char *sz = defVal;
	CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);
	if (pItem) {
		if (pItem->SymType == StringType) sz = *pItem->data.s;
		else DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);
	} else
		m_bSuccess = false;
	if (!sz) return;
	if (strlen(sz) < maxLen) maxLen = strlen(sz) + 1;
	strncpy(szResult, sz, maxLen);
}

void CButeMgr::GetString(const char *szTagName, const char *szAttName, char *szResult, DWORD maxLen)
{
	if (!szResult || !maxLen) return;
	m_bSuccess = true;
	const char *sz = "";
	CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);
	if (pItem) {
		if (pItem->SymType == StringType) sz = *pItem->data.s;
		else DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);
	} else {
		DisplayMessage("Symbol not found - [%s]:%s", szTagName, szAttName);
		m_bSuccess = false;
	}
	if (strlen(sz) < maxLen) maxLen = strlen(sz) + 1;
	strncpy(szResult, sz, maxLen);
}

// Os que devolvem referencia: o valor guardado, ou o padrao do chamador, ou um vazio estatico (como o NOLF).
#define BUTE_GET_REF(T, Cls, Field)                                                                          \
	Cls &CButeMgr::Get##T(const char *szTagName, const char *szAttName, Cls &defVal)                       \
	{                                                                                                          \
		m_bSuccess = true;                                                                                     \
		CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);                                             \
		if (pItem) {                                                                                           \
			if (pItem->SymType == T##Type) return *pItem->data.Field;                                       \
			DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);                                   \
		}                                                                                                      \
		m_bSuccess = false;                                                                                    \
		return defVal;                                                                                         \
	}                                                                                                          \
	Cls &CButeMgr::Get##T(const char *szTagName, const char *szAttName)                                     \
	{                                                                                                          \
		static Cls empty;                                                                                      \
		m_bSuccess = true;                                                                                     \
		CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);                                             \
		if (pItem) {                                                                                           \
			if (pItem->SymType == T##Type) return *pItem->data.Field;                                       \
			DisplayMessage("Type mismatch - [%s]:%s", szTagName, szAttName);                                   \
		} else                                                                                                 \
			DisplayMessage("Symbol not found - [%s]:%s", szTagName, szAttName);                                \
		m_bSuccess = false;                                                                                    \
		empty = Cls();                                                                                         \
		return empty;                                                                                          \
	}
BUTE_GET_REF(Rect, CRect, r)
BUTE_GET_REF(Point, CPoint, point)
BUTE_GET_REF(Vector, CAVector, v)
BUTE_GET_REF(Range, CARange, range)

bool CButeMgr::AddTag(const char *szTagName)
{
	m_bSuccess = true;
	if (FindTableOfItems(m_tagTab, szTagName) || FindTableOfItems(m_newTagTab, szTagName))
		return false;
	CreateTableOfItems(m_newTagTab, szTagName);
	return true;
}

CButeMgr::SymTypes CButeMgr::GetType(const char *szTagName, const char *szAttName)
{
	m_bSuccess = true;
	CSymTabItem *pItem = FindSymTabItem(szTagName, szAttName);
	if (pItem) return pItem->SymType;
	m_bSuccess = false;
	return NullType;
}

bool CButeMgr::Exist(const char *szTagName, const char *szAttName)
{
	TableOfTags *tabs[] = { &m_auxTagTab, &m_tagTab, &m_newTagTab };
	for (size_t i = 0; i < sizeof tabs / sizeof tabs[0]; i++) {
		TableOfItems *pTab = FindTableOfItems(*tabs[i], szTagName);
		if (pTab && (!szAttName || FindSymTabItem(*pTab, szAttName))) return true;
	}
	return false;
}
