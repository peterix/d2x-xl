#ifndef __BANLIST_H
#define __BANLIST_H

#include "strutil.h"

//-----------------------------------------------------------------------------

typedef char tBanListEntry[CALLSIGN_LEN + 1];

class CBanListEntry {
    public:
    tBanListEntry m_entry;
    CBanListEntry() { m_entry[0] = '\0'; }
    inline CBanListEntry &operator=(const char *source) {
        strcpy(m_entry, source);
        return *this;
    }
    inline const bool operator==(const char *other) { return !strnicmp(m_entry, other, sizeof(m_entry)); }
};

class CBanList : public CStack<CBanListEntry> {
    public:
    CBanList() { Create(); }
    bool Add(const char *szPlayer);
    int32_t Find(const char *szPlayer);
    int32_t Load(void);
    int32_t Save(void);
    bool Create(void);
};

extern CBanList banList;

//-----------------------------------------------------------------------------

#endif //__BANLIST_H
