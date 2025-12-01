#pragma once

#include <quickfix/Application.h>
#include <quickfix/MessageCracker.h>
#include <quickfix/Session.h>
#include <quickfix/Message.h>
#include <quickfix/SessionID.h>
#include <quickfix/fix44/Logon.h>
#include <quickfix/SessionSettings.h>
#include <quickfix/FileStore.h>
#include <quickfix/SocketInitiator.h>
#include <quickfix/FileLog.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <quickfix/Fields.h>
#include "LogonRawDataSigner.hpp"

class OrderGateway : public FIX::Application, public FIX::MessageCracker
{
public:
    explicit OrderGateway(std::string api_key, std::string signer);
    ~OrderGateway();

    void start();
    void stop();

    void onCreate(const FIX::SessionID &) override;
    void onLogon(const FIX::SessionID &) override;
    void onLogout(const FIX::SessionID &) override;
    void toAdmin(FIX::Message &, const FIX::SessionID &) override;
    void fromAdmin(const FIX::Message &, const FIX::SessionID &) override;
    void toApp(FIX::Message &, const FIX::SessionID &) override;
    void fromApp(const FIX::Message &, const FIX::SessionID &) override;

private:
    std::unique_ptr<FIX::SessionSettings> settings_;
    std::unique_ptr<FIX::FileStoreFactory> storeFactory_;
    std::unique_ptr<FIX::FileLogFactory> logFactory_;
    std::unique_ptr<FIX::SocketInitiator> initiator_;
    std::string apikey_;
    std::string signer_;
};
