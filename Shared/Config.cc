#include <Shared/Config.hh>

extern const uint64_t VERSION_HASH = 19235684321324ull;

extern const uint32_t SERVER_PORT = 7001;
extern const uint32_t MAX_NAME_LENGTH = 32;
uint32_t const MAX_CHAT_LENGTH = 128;
extern const uint32_t MAX_PASSWORD_LENGTH = 64;
extern const std::string PASSWORD = "a621ab606db2a11f63edc576a729843b8269250dc324206871d90635ac5e531c";

//your ws host url may not follow this format, change it to fit your needs
#ifdef TEST
extern std::string const WS_URL = "ws://localhost:" + std::to_string(SERVER_PORT);
#else
extern std::string const WS_URL = "ws://gardn.camvan.xyz:" + std::to_string(SERVER_PORT);
#endif