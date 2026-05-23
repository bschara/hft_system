#include "order_gateway/order_gateway.h"
// #include <iostream>

// OrderGateway::OrderGateway(std::string api_key, std::string signer) : apikey_(api_key),
//                                                                       signer_(signer)
// {
//     settings_ = std::make_unique<FIX::SessionSettings>("../src/order_gateway/config/ordergateway.cfg");
//     storeFactory_ = std::make_unique<FIX::FileStoreFactory>(*settings_);
//     logFactory_ = std::make_unique<FIX::FileLogFactory>(*settings_);
//     initiator_ = std::make_unique<FIX::SocketInitiator>(*this, *storeFactory_, *settings_, *logFactory_);
// }

// OrderGateway::~OrderGateway()
// {
//     stop();
// }

// void OrderGateway::start()
// {
//     initiator_->start();
// }

// void OrderGateway::stop()
// {
//     if (initiator_)
//     {
//         initiator_->stop();
//     }
// }

// void OrderGateway::onCreate(const FIX::SessionID &sessionID)
// {
//     std::cout << "Session created: " << sessionID << std::endl;
// }

// void OrderGateway::onLogon(const FIX::SessionID &sessionID)
// {
//     std::cout << "Logon: " << sessionID << std::endl;
// }

// void OrderGateway::onLogout(const FIX::SessionID &sessionID)
// {
//     std::cout << "Logout: " << sessionID << std::endl;
// }

// void OrderGateway::toAdmin(FIX::Message &msg, const FIX::SessionID &sessionID)
// {
//     FIX::MsgType msgType;
//     msg.getHeader().getField(msgType);
//     if (msgType == FIX::MsgType_Logon)
//     {

//         // std::string senderCompID, targetCompID, sendingTime;
//         // int msgSeqNum;

//         FIX::SenderCompID senderCompID;
//         FIX::TargetCompID targetCompID;
//         FIX::MsgSeqNum msgSeqNum;
//         FIX::SendingTime sendingTime;

//         msg.getHeader().getField(senderCompID);
//         msg.getHeader().getField(targetCompID);
//         msg.getHeader().getField(msgSeqNum);
//         msg.getHeader().getField(sendingTime);

//         int seqNum = msgSeqNum.getValue();

//         std::cerr << "SenderCompID: " << senderCompID.getValue() << std::endl;
//         std::cerr << "TargetCompID: " << targetCompID.getValue() << std::endl;
//         std::cerr << "MsgSeqNum: " << seqNum << std::endl;
//         std::cerr << "SendingTime: " << sendingTime.getString() << std::endl;

//         EVP_PKEY *pkey = hash_helpers::loadPrivateKeyFromString(signer_);

//         auto signature = hash_helpers::logonRawData(pkey, senderCompID.getValue(), targetCompID.getValue(), std::to_string(seqNum), sendingTime.getString());
//         std::cerr << "signture " << signature << std::endl;
//         std::cerr << "signture length" << signature.length() << std::endl;

//         msg.setField(FIX::Username(apikey_));
//         msg.setField(FIX::EncryptMethod(0));
//         msg.setField(FIX::HeartBtInt(30)); // Or any value in [5, 60]
//         msg.setField(FIX::ResetSeqNumFlag(true));

//         // msg.setField(FIX::RawData(signature));
//         // msg.setField(FIX::RawDataLength(signature.length()));
//         msg.setField(95, std::to_string(signature.length()));
//         msg.setField(96, signature);
//         msg.setField(25035, "1");

//         std::cout << "🔐 Logon message signed and sent to Binance FIX API" << std::endl;
//         std::cout << "🔐 Logon message " << msg.toString() << std::endl;
//     }
// }

// void OrderGateway::fromAdmin(const FIX::Message &msg, const FIX::SessionID &sessionID)
// {
//     FIX::MsgType msgType;
//     msg.getHeader().getField(msgType);

//     std::cerr << "message from admin  " << msg.toXML() << std::endl;
//     if (msgType == FIX::MsgType_Logout)
//     {
//         std::cerr << "📤 Received Logout message from Binance." << std::endl;

//         if (msg.isSetField(FIX::FIELD::Text))
//         {
//             FIX::Text logoutReason;
//             msg.getField(logoutReason);
//             std::cerr << "🚫 Logout reason: " << logoutReason.getValue() << std::endl;
//         }
//         else
//         {
//             std::cerr << "No logout reason provided." << std::endl;
//         }
//     }
// }

// void OrderGateway::toApp(FIX::Message &msg, const FIX::SessionID &sessionID)
// {
//     std::cerr << "to app from " << msg.toString() << std::endl;
// }

// void OrderGateway::fromApp(const FIX::Message &msg, const FIX::SessionID &sessionID)
// {
//     crack(msg, sessionID);
// }
