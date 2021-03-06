/*
 * InspIRCd -- Internet Relay Chat Daemon
 *
 *   Copyright (C) 2020 Matt Schatz <genius3000@g3k.solutions>
 *   Copyright (C) 2019 linuxdaemon <linuxdaemon.irc@gmail.com>
 *   Copyright (C) 2013, 2017-2020 Sadie Powell <sadie@witchery.services>
 *   Copyright (C) 2012-2016 Attila Molnar <attilamolnar@hush.com>
 *   Copyright (C) 2012 Robby <robby@chatbelgie.be>
 *   Copyright (C) 2010 Adam <Adam@anope.org>
 *   Copyright (C) 2009-2010 Daniel De Graaf <danieldg@inspircd.org>
 *   Copyright (C) 2008 Robin Burchell <robin+git@viroteck.net>
 *   Copyright (C) 2007 Dennis Friis <peavey@inspircd.org>
 *   Copyright (C) 2006-2007, 2009-2010 Craig Edwards <brain@inspircd.org>
 *
 * This file is part of InspIRCd.  InspIRCd is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "inspircd.h"
#include "modules/ssl.h"
#include "modules/webirc.h"
#include "modules/whois.h"
#include "modules/who.h"

enum
{
	// From oftc-hybrid.
	RPL_WHOISCERTFP = 276,

	// From UnrealIRCd.
	RPL_WHOISSECURE = 671
};

class SSLCertExt : public ExtensionItem
{
 public:
	SSLCertExt(Module* parent)
		: ExtensionItem(parent, "ssl_cert", ExtensionItem::EXT_USER)
	{
	}

	ssl_cert* get(const Extensible* item) const
	{
		return static_cast<ssl_cert*>(GetRaw(item));
	}

	void set(Extensible* item, ssl_cert* value)
	{
		value->refcount_inc();
		ssl_cert* old = static_cast<ssl_cert*>(SetRaw(item, value));
		if (old && old->refcount_dec())
			delete old;
	}

	void unset(Extensible* container)
	{
		Delete(container, UnsetRaw(container));
	}

	std::string ToNetwork(const Extensible* container, void* item) const override
	{
		return static_cast<ssl_cert*>(item)->GetMetaLine();
	}

	void FromNetwork(Extensible* container, const std::string& value) override
	{
		ssl_cert* cert = new ssl_cert;
		set(container, cert);

		std::stringstream s(value);
		std::string v;
		getline(s,v,' ');

		cert->invalid = (v.find('v') != std::string::npos);
		cert->trusted = (v.find('T') != std::string::npos);
		cert->revoked = (v.find('R') != std::string::npos);
		cert->unknownsigner = (v.find('s') != std::string::npos);
		if (v.find('E') != std::string::npos)
		{
			getline(s,cert->error,'\n');
		}
		else
		{
			getline(s,cert->fingerprint,' ');
			getline(s,cert->dn,' ');
			getline(s,cert->issuer,'\n');
		}
	}

	void Delete(Extensible* container, void* item) override
	{
		ssl_cert* old = static_cast<ssl_cert*>(item);
		if (old && old->refcount_dec())
			delete old;
	}
};

class UserCertificateAPIImpl : public UserCertificateAPIBase
{
 public:
	IntExtItem nosslext;
	SSLCertExt sslext;

	UserCertificateAPIImpl(Module* mod)
		: UserCertificateAPIBase(mod)
		, nosslext(mod, "no_ssl_cert", ExtensionItem::EXT_USER)
		, sslext(mod)
	{
	}

	ssl_cert* GetCertificate(User* user) override
	{
		ssl_cert* cert = sslext.get(user);
		if (cert)
			return cert;

		LocalUser* luser = IS_LOCAL(user);
		if (!luser || nosslext.get(luser))
			return NULL;

		cert = SSLClientCert::GetCertificate(&luser->eh);
		if (!cert)
			return NULL;

		SetCertificate(user, cert);
		return cert;
	}

	void SetCertificate(User* user, ssl_cert* cert) override
	{
		ServerInstance->Logs.Log(MODNAME, LOG_DEBUG, "Setting TLS (SSL) client certificate for %s: %s",
			user->GetFullHost().c_str(), cert->GetMetaLine().c_str());
		sslext.set(user, cert);
	}
};

class CommandSSLInfo : public Command
{
 public:
	UserCertificateAPIImpl sslapi;

	CommandSSLInfo(Module* Creator)
		: Command(Creator, "SSLINFO", 1)
		, sslapi(Creator)
	{
		syntax = { "<nick>" };
	}

	CmdResult Handle(User* user, const Params& parameters) override
	{
		User* target = ServerInstance->Users.FindNick(parameters[0]);

		if ((!target) || (target->registered != REG_ALL))
		{
			user->WriteNumeric(Numerics::NoSuchNick(parameters[0]));
			return CmdResult::FAILURE;
		}

		bool operonlyfp = ServerInstance->Config->ConfValue("sslinfo")->getBool("operonly");
		if (operonlyfp && !user->IsOper() && target != user)
		{
			user->WriteNotice("*** You cannot view TLS (SSL) client certificate information for other users");
			return CmdResult::FAILURE;
		}

		ssl_cert* cert = sslapi.GetCertificate(target);
		if (!cert)
		{
			user->WriteNotice(InspIRCd::Format("*** %s is not connected using TLS (SSL).", target->nick.c_str()));
		}
		else if (cert->GetError().length())
		{
			user->WriteNotice(InspIRCd::Format("*** %s is connected using TLS (SSL) but has not specified a valid client certificate (%s).",
				target->nick.c_str(), cert->GetError().c_str()));
		}
		else
		{
			user->WriteNotice("*** Distinguished Name: " + cert->GetDN());
			user->WriteNotice("*** Issuer:             " + cert->GetIssuer());
			user->WriteNotice("*** Key Fingerprint:    " + cert->GetFingerprint());
		}

		return CmdResult::SUCCESS;
	}
};

class ModuleSSLInfo
	: public Module
	, public WebIRC::EventListener
	, public Whois::EventListener
	, public Who::EventListener
{
 private:
	CommandSSLInfo cmd;

	bool MatchFP(ssl_cert* const cert, const std::string& fp) const
	{
		return irc::spacesepstream(fp).Contains(cert->GetFingerprint());
	}

 public:
	ModuleSSLInfo()
		: Module(VF_VENDOR, "Adds user facing TLS (SSL) information, various TLS (SSL) configuration options, and the /SSLINFO command to look up TLS (SSL) certificate information for other users.")
		, WebIRC::EventListener(this)
		, Whois::EventListener(this)
		, Who::EventListener(this)
		, cmd(this)
	{
	}

	void OnWhois(Whois::Context& whois) override
	{
		ssl_cert* cert = cmd.sslapi.GetCertificate(whois.GetTarget());
		if (cert)
		{
			whois.SendLine(RPL_WHOISSECURE, "is using a secure connection");
			bool operonlyfp = ServerInstance->Config->ConfValue("sslinfo")->getBool("operonly");
			if ((!operonlyfp || whois.IsSelfWhois() || whois.GetSource()->IsOper()) && !cert->fingerprint.empty())
				whois.SendLine(RPL_WHOISCERTFP, InspIRCd::Format("has TLS (SSL) client certificate fingerprint %s", cert->fingerprint.c_str()));
		}
	}

	ModResult OnWhoLine(const Who::Request& request, LocalUser* source, User* user, Membership* memb, Numeric::Numeric& numeric) override
	{
		size_t flag_index;
		if (!request.GetFieldIndex('f', flag_index))
			return MOD_RES_PASSTHRU;

		ssl_cert* cert = cmd.sslapi.GetCertificate(user);
		if (cert)
			numeric.GetParams()[flag_index].push_back('s');

		return MOD_RES_PASSTHRU;
	}

	ModResult OnPreCommand(std::string& command, CommandBase::Params& parameters, LocalUser* user, bool validated) override
	{
		if ((command == "OPER") && (validated))
		{
			ServerConfig::OperIndex::const_iterator i = ServerInstance->Config->oper_blocks.find(parameters[0]);
			if (i != ServerInstance->Config->oper_blocks.end())
			{
				std::shared_ptr<OperInfo> ifo = i->second;
				ssl_cert* cert = cmd.sslapi.GetCertificate(user);

				if (ifo->oper_block->getBool("sslonly") && !cert)
				{
					user->WriteNumeric(ERR_NOOPERHOST, "Invalid oper credentials");
					user->CommandFloodPenalty += 10000;
					ServerInstance->SNO.WriteGlobalSno('o', "WARNING! Failed oper attempt by %s using login '%s': a secure connection is required.", user->GetFullRealHost().c_str(), parameters[0].c_str());
					return MOD_RES_DENY;
				}

				std::string fingerprint;
				if (ifo->oper_block->readString("fingerprint", fingerprint) && (!cert || !MatchFP(cert, fingerprint)))
				{
					user->WriteNumeric(ERR_NOOPERHOST, "Invalid oper credentials");
					user->CommandFloodPenalty += 10000;
					ServerInstance->SNO.WriteGlobalSno('o', "WARNING! Failed oper attempt by %s using login '%s': their TLS (SSL) client certificate fingerprint does not match.", user->GetFullRealHost().c_str(), parameters[0].c_str());
					return MOD_RES_DENY;
				}
			}
		}

		// Let core handle it for extra stuff
		return MOD_RES_PASSTHRU;
	}

	void OnPostConnect(User* user) override
	{
		LocalUser* const localuser = IS_LOCAL(user);
		if (!localuser)
			return;

		const SSLIOHook* const ssliohook = SSLIOHook::IsSSL(&localuser->eh);
		if (!ssliohook || cmd.sslapi.nosslext.get(localuser))
			return;

		ssl_cert* const cert = ssliohook->GetCertificate();

		std::string text = "*** You are connected to ";
		if (!ssliohook->GetServerName(text))
			text.append(ServerInstance->Config->GetServerName());
		text.append(" using TLS (SSL) cipher '");
		ssliohook->GetCiphersuite(text);
		text.push_back('\'');
		if (cert && !cert->GetFingerprint().empty())
			text.append(" and your TLS (SSL) client certificate fingerprint is ").append(cert->GetFingerprint());
		user->WriteNotice(text);

		if (!cert)
			return;

		// Find an auto-oper block for this user
		for (auto& [_, ifo] :  ServerInstance->Config->oper_blocks)
		{
			std::string fp = ifo->oper_block->getString("fingerprint");
			if (MatchFP(cert, fp) && ifo->oper_block->getBool("autologin"))
				user->Oper(ifo);
		}
	}

	ModResult OnSetConnectClass(LocalUser* user, std::shared_ptr<ConnectClass> myclass) override
	{
		ssl_cert* cert = cmd.sslapi.GetCertificate(user);
		const char* error = NULL;
		const std::string requiressl = myclass->config->getString("requiressl");
		if (stdalgo::string::equalsci(requiressl, "trusted"))
		{
			if (!cert || !cert->IsCAVerified())
				error = "a trusted TLS (SSL) client certificate";
		}
		else if (myclass->config->getBool("requiressl"))
		{
			if (!cert)
				error = "a TLS (SSL) connection";
		}

		if (error)
		{
			ServerInstance->Logs.Log("CONNECTCLASS", LOG_DEBUG, "The %s connect class is not suitable as it requires %s",
				myclass->GetName().c_str(), error);
			return MOD_RES_DENY;
		}

		return MOD_RES_PASSTHRU;
	}

	void OnWebIRCAuth(LocalUser* user, const WebIRC::FlagMap* flags) override
	{
		// We are only interested in connection flags. If none have been
		// given then we have nothing to do.
		if (!flags)
			return;

		// We only care about the tls connection flag if the connection
		// between the gateway and the server is secure.
		if (!cmd.sslapi.GetCertificate(user))
			return;

		WebIRC::FlagMap::const_iterator iter = flags->find("secure");
		if (iter == flags->end())
		{
			// If this is not set then the connection between the client and
			// the gateway is not secure.
			cmd.sslapi.nosslext.set(user, 1);
			cmd.sslapi.sslext.unset(user);
			return;
		}

		// Create a fake ssl_cert for the user.
		ssl_cert* cert = new ssl_cert;
		cert->error = "WebIRC users can not specify valid certs yet";
		cert->invalid = true;
		cert->revoked = true;
		cert->trusted = false;
		cert->unknownsigner = true;
		cmd.sslapi.SetCertificate(user, cert);
	}
};

MODULE_INIT(ModuleSSLInfo)
