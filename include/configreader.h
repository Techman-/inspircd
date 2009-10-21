/*       +------------------------------------+
 *       | Inspire Internet Relay Chat Daemon |
 *       +------------------------------------+
 *
 *  InspIRCd: (C) 2002-2009 InspIRCd Development Team
 * See: http://wiki.inspircd.org/Credits
 *
 * This program is free but copyrighted software; see
 *            the file COPYING for details.
 *
 * ---------------------------------------------------
 */

#ifndef INSPIRCD_CONFIGREADER
#define INSPIRCD_CONFIGREADER

#include <sstream>
#include <string>
#include <vector>
#include <map>
#include "inspircd.h"
#include "modules.h"
#include "socketengine.h"
#include "socket.h"

/* Required forward definitions */
class ServerConfig;
class ServerLimits;
class InspIRCd;
class BufferedSocket;

/** A cached text file stored with its contents as lines
 */
typedef std::vector<std::string> file_cache;

/** A configuration key and value pair
 */
typedef std::pair<std::string, std::string> KeyVal;

struct CoreExport ConfigTag : public refcountbase
{
	const std::string tag;
	const std::string src_name;
	const int src_line;
	std::vector<KeyVal> items;

	ConfigTag(const std::string& Tag, const std::string& file, int line)
		: tag(Tag), src_name(file), src_line(line) {}

	std::string getString(const std::string& key, const std::string& def = "");
	long getInt(const std::string& key, long def = 0);
	double getFloat(const std::string& key, double def = 0);
	bool getBool(const std::string& key, bool def = false);

	bool readString(const std::string& key, std::string& value, bool allow_newline = false);

	std::string getTagLocation();
};

/** An entire config file, built up of KeyValLists
 */
typedef std::multimap<std::string, reference<ConfigTag> > ConfigDataHash;

typedef std::map<std::string, reference<ConfigTag> > TagIndex;

/** Defines the server's length limits on various length-limited
 * items such as topics, nicknames, channel names etc.
 */
class ServerLimits
{
 public:
	/** Maximum nickname length */
	size_t NickMax;
	/** Maximum channel length */
	size_t ChanMax;
	/** Maximum number of modes per line */
	size_t MaxModes;
	/** Maximum length of ident, not including ~ etc */
	size_t IdentMax;
	/** Maximum length of a quit message */
	size_t MaxQuit;
	/** Maximum topic length */
	size_t MaxTopic;
	/** Maximum kick message length */
	size_t MaxKick;
	/** Maximum GECOS (real name) length */
	size_t MaxGecos;
	/** Maximum away message length */
	size_t MaxAway;

	/** Creating the class initialises it to the defaults
	 * as in 1.1's ./configure script. Reading other values
	 * from the config will change these values.
	 */
	ServerLimits() : NickMax(31), ChanMax(64), MaxModes(20), IdentMax(12), MaxQuit(255), MaxTopic(307), MaxKick(255), MaxGecos(128), MaxAway(200)
	{
	}

	/** Finalises the settings by adding one. This allows for them to be used as-is
	 * without a 'value+1' when using the std::string assignment methods etc.
	 */
	void Finalise()
	{
		NickMax++;
		ChanMax++;
		IdentMax++;
		MaxQuit++;
		MaxTopic++;
		MaxKick++;
		MaxGecos++;
		MaxAway++;
	}
};

/** This class holds the bulk of the runtime configuration for the ircd.
 * It allows for reading new config values, accessing configuration files,
 * and storage of the configuration data needed to run the ircd, such as
 * the servername, connect classes, /ADMIN data, MOTDs and filenames etc.
 */
class CoreExport ServerConfig
{
  private:
	void CrossCheckOperClassType();
	void CrossCheckConnectBlocks(ServerConfig* current);

 public:

	/** Get a configuration tag
	 * @param tag The name of the tag to get
	 * @param offset get the Nth occurance of the tag
	 */
	ConfigTag* ConfValue(const std::string& tag, int offset = 0);

	/** Error stream, contains error output from any failed configuration parsing.
	 */
	std::stringstream errstr;

	/** True if this configuration is valid enough to run with */
	bool valid;

	/** Used to indicate who we announce invites to on a channel */
	enum InviteAnnounceState { INVITE_ANNOUNCE_NONE, INVITE_ANNOUNCE_ALL, INVITE_ANNOUNCE_OPS, INVITE_ANNOUNCE_DYNAMIC };

  	/** This holds all the information in the config file,
	 * it's indexed by tag name to a vector of key/values.
	 */
	ConfigDataHash config_data;

	/** Length limits, see definition of ServerLimits class
	 */
	ServerLimits Limits;

	/** Clones CIDR range for ipv4 (0-32)
	 * Defaults to 32 (checks clones on all IPs seperately)
	 */
	int c_ipv4_range;

	/** Clones CIDR range for ipv6 (0-128)
	 * Defaults to 128 (checks on all IPs seperately)
	 */
	int c_ipv6_range;

	/** Max number of WhoWas entries per user.
	 */
	int WhoWasGroupSize;

	/** Max number of cumulative user-entries in WhoWas.
	 *  When max reached and added to, push out oldest entry FIFO style.
	 */
	int WhoWasMaxGroups;

	/** Max seconds a user is kept in WhoWas before being pruned.
	 */
	int WhoWasMaxKeep;

	/** Both for set(g|u)id.
	 */
	std::string SetUser;
	std::string SetGroup;

	/** Holds the server name of the local server
	 * as defined by the administrator.
	 */
	std::string ServerName;

	/** Notice to give to users when they are Xlined
	 */
	std::string MoronBanner;

	/* Holds the network name the local server
	 * belongs to. This is an arbitary field defined
	 * by the administrator.
	 */
	std::string Network;

	/** Holds the description of the local server
	 * as defined by the administrator.
	 */
	std::string ServerDesc;

	/** Holds the admin's name, for output in
	 * the /ADMIN command.
	 */
	std::string AdminName;

	/** Holds the email address of the admin,
	 * for output in the /ADMIN command.
	 */
	std::string AdminEmail;

	/** Holds the admin's nickname, for output
	 * in the /ADMIN command
	 */
	std::string AdminNick;

	/** The admin-configured /DIE password
	 */
	std::string diepass;

	/** The admin-configured /RESTART password
	 */
	std::string restartpass;

	/** The hash method for *BOTH* the die and restart passwords.
	 */
	std::string powerhash;

	/** The pathname and filename of the message of the
	 * day file, as defined by the administrator.
	 */
	std::string motd;

	/** The pathname and filename of the rules file,
	 * as defined by the administrator.
	 */
	std::string rules;

	/** The quit prefix in use, or an empty string
	 */
	std::string PrefixQuit;

	/** The quit suffix in use, or an empty string
	 */
	std::string SuffixQuit;

	/** The fixed quit message in use, or an empty string
	 */
	std::string FixedQuit;

	/** The part prefix in use, or an empty string
	 */
	std::string PrefixPart;

	/** The part suffix in use, or an empty string
	 */
	std::string SuffixPart;

	/** The fixed part message in use, or an empty string
	 */
	std::string FixedPart;

	/** The last string found within a <die> tag, or
	 * an empty string.
	 */
	std::string DieValue;

	/** The DNS server to use for DNS queries
	 */
	std::string DNSServer;

	/** Pretend disabled commands don't exist.
	 */
	bool DisabledDontExist;

	/** This variable contains a space-seperated list
	 * of commands which are disabled by the
	 * administrator of the server for non-opers.
	 */
	std::string DisabledCommands;

	/** This variable identifies which usermodes have been diabled.
	 */

	char DisabledUModes[64];

	/** This variable identifies which chanmodes have been disabled.
	 */
	char DisabledCModes[64];

	/** The full path to the modules directory.
	 * This is either set at compile time, or
	 * overridden in the configuration file via
	 * the <options> tag.
	 */
	std::string ModPath;

	/** The file handle of the logfile. If this
	 * value is NULL, the log file is not open,
	 * probably due to a permissions error on
	 * startup (this should not happen in normal
	 * operation!).
	 */
	FILE *log_file;

	/** If this value is true, the owner of the
	 * server specified -nofork on the command
	 * line, causing the daemon to stay in the
	 * foreground.
	 */
	bool nofork;

	/** If this value if true then all log
	 * messages will be output, regardless of
	 * the level given in the config file.
	 * This is set with the -debug commandline
	 * option.
	 */
	bool forcedebug;

	/** If this is true then log output will be
	 * written to the logfile. This is the default.
	 * If you put -nolog on the commandline then
	 * the logfile will not be written.
	 * This is meant to be used in conjunction with
	 * -debug for debugging without filling up the
	 * hard disk.
	 */
	bool writelog;

	/** If set to true, then all opers on this server are
	 * shown with a generic 'is an IRC operator' line rather
	 * than the oper type. Oper types are still used internally.
	 */
	bool GenericOper;

	/** If this value is true, banned users (+b, not extbans) will not be able to change nick
	 * if banned on any channel, nor to message them.
	 */
	bool RestrictBannedUsers;

	/** If this value is true, halfops have been
	 * enabled in the configuration file.
	 */
	bool AllowHalfop;

	/** If this is set to true, then mode lists (e.g
	 * MODE #chan b) are hidden from unprivileged
	 * users.
	 */
	bool HideModeLists[256];

	/** The number of seconds the DNS subsystem
	 * will wait before timing out any request.
	 */
	int dns_timeout;

	/** The size of the read() buffer in the user
	 * handling code, used to read data into a user's
	 * recvQ.
	 */
	int NetBufferSize;

	/** The value to be used for listen() backlogs
	 * as default.
	 */
	int MaxConn;

	/** The soft limit value assigned to the irc server.
	 * The IRC server will not allow more than this
	 * number of local users.
	 */
	unsigned int SoftLimit;

	/** Maximum number of targets for a multi target command
	 * such as PRIVMSG or KICK
	 */
	unsigned int MaxTargets;

	/** The maximum number of /WHO results allowed
	 * in any single /WHO command.
	 */
	int MaxWhoResults;

	/** True if the DEBUG loglevel is selected.
	 */
	int debugging;

	/** How many seconds to wait before exiting
	 * the program when /DIE is correctly issued.
	 */
	int DieDelay;

	/** True if we're going to hide netsplits as *.net *.split for non-opers
	 */
	bool HideSplits;

	/** True if we're going to hide ban reasons for non-opers (e.g. G-Lines,
	 * K-Lines, Z-Lines)
	 */
	bool HideBans;

	/** Announce invites to the channel with a server notice
	 */
	InviteAnnounceState AnnounceInvites;

	/** If this is enabled then operators will
	 * see invisible (+i) channels in /whois.
	 */
	bool OperSpyWhois;

	/** Set to a non-empty string to obfuscate the server name of users in WHOIS
	 */
	std::string HideWhoisServer;

	/** Set to a non empty string to obfuscate nicknames prepended to a KILL.
	 */
	std::string HideKillsServer;

	/** The MOTD file, cached in a file_cache type.
	 */
	file_cache MOTD;

	/** The RULES file, cached in a file_cache type.
	 */
	file_cache RULES;

	/** The full pathname and filename of the PID
	 * file as defined in the configuration.
	 */
	std::string PID;

	/** The connect classes in use by the IRC server.
	 */
	ClassVector Classes;

	/** The 005 tokens of this server (ISUPPORT)
	 * populated/repopulated upon loading or unloading
	 * modules.
	 */
	std::string data005;

	/** isupport strings
	 */
	std::vector<std::string> isupport;

	/** STATS characters in this list are available
	 * only to operators.
	 */
	std::string UserStats;

	/** The path and filename of the ircd.log file
	 */
	std::string logpath;

	/** Default channel modes
	 */
	std::string DefaultModes;

	/** Custom version string, which if defined can replace the system info in VERSION.
	 */
	std::string CustomVersion;

	/** List of u-lined servers
	 */
	std::map<irc::string, bool> ulines;

	/** Max banlist sizes for channels (the std::string is a glob)
	 */
	std::map<std::string, int> maxbans;

	/** Directory where the inspircd binary resides
	 */
	std::string MyDir;

	/** If set to true, no user DNS lookups are to be performed
	 */
	bool NoUserDns;

	/** If set to true, provide syntax hints for unknown commands
	 */
	bool SyntaxHints;

	/** If set to true, users appear to quit then rejoin when their hosts change.
	 * This keeps clients synchronized properly.
	 */
	bool CycleHosts;

	/** If set to true, prefixed channel NOTICEs and PRIVMSGs will have the prefix
	 *  added to the outgoing text for undernet style msg prefixing.
	 */
	bool UndernetMsgPrefix;

	/** If set to true, the full nick!user@host will be shown in the TOPIC command
	 * for who set the topic last. If false, only the nick is shown.
	 */
	bool FullHostInTopic;

	/** All oper type definitions from the config file
	 */
	TagIndex opertypes;

	/** All oper class definitions from the config file
	 */
	TagIndex operclass;

	/** Saved argv from startup
	 */
	char** argv;

	/** Saved argc from startup
	 */
	int argc;

	/** Max channels per user
	 */
	unsigned int MaxChans;

	/** Oper max channels per user
	 */
	unsigned int OperMaxChans;

	/** TS6-like server ID.
	 * NOTE: 000...999 are usable for InspIRCd servers. This
	 * makes code simpler. 0AA, 1BB etc with letters are reserved
	 * for services use.
	 */
	std::string sid;

	/** True if we have been told to run the testsuite from the commandline,
	 * rather than entering the mainloop.
	 */
	bool TestSuite;

	/** Construct a new ServerConfig
	 */
	ServerConfig();

	/** Get server ID as string with required leading zeroes
	 */
	std::string GetSID();

	/** Update the 005 vector
	 */
	void Update005();

	/** Send the 005 numerics (ISUPPORT) to a user
	 */
	void Send005(User* user);

	/** Read the entire configuration into memory
	 * and initialize this class. All other methods
	 * should be used only by the core.
	 */
	void Read();

	/** Apply configuration changes from the old configuration.
	 */
	void Apply(ServerConfig* old, const std::string &useruid);
	void ApplyModules(User* user);

	void Fill();

	/** Read a file into a file_cache object
	 */
	bool ReadFile(file_cache &F, const std::string& fname);

	/* Returns true if the given string starts with a windows drive letter
	 */
	bool StartsWithWindowsDriveLetter(const std::string &path);

	bool ApplyDisabledCommands(const std::string& data);

	/** Clean a filename, stripping the directories (and drives) from string.
	 * @param name Directory to tidy
	 * @return The cleaned filename
	 */
	static const char* CleanFilename(const char* name);

	/** Check if a file exists.
	 * @param file The full path to a file
	 * @return True if the file exists and is readable.
	 */
	static bool FileExists(const char* file);

	/** If this value is true, invites will bypass more than just +i
	 */
	bool InvBypassModes;

};
#endif
