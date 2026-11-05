// avp360: std::hash_map, std::hash_set e std::hash como no STLport (o AvP2 foi feito com ele), sobre o GCC.
#ifndef AVP360_HASH_H
#define AVP360_HASH_H

#include <ext/hash_map>
#include <ext/hash_set>
#include <string>

namespace __gnu_cxx {
// O STLport tinha hash<std::string>; o __gnu_cxx so tem para char*.
template <> struct hash<std::string> {
	size_t operator()(const std::string &s) const { return __stl_hash_string(s.c_str()); }
};
}

// std::hash ja existe no C++11 e, para char const*, hasheia o PONTEIRO; no STLport hasheava a string. Por isso o
// hasher padrao destes conteineres e o __gnu_cxx::hash (que hasheia a string), e o butemgr.h usa
// AVP360_STR_HASH no lugar de std::hash<char const*>.
namespace std {
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_multimap;
using __gnu_cxx::hash_set;
using __gnu_cxx::hash_multiset;
}
#define AVP360_STR_HASH __gnu_cxx::hash<char const *>

#endif
