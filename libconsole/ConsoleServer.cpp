/*
 * ConsoleServer.cpp
 *
 *  Created on: 2018年8月15日
 *      Author: monan
 */

#include "ConsoleServer.h"
#include <libpbftseal/PBFT.h>
#include <boost/throw_exception.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <libp2p/Common.h>
#include <libp2p/Host.h>
#include <libdevcore/FixedHash.h>
#include <libstorage/StateDB.h>

using namespace dev;
using namespace console;

ConsoleServer::ConsoleServer() {
}

void ConsoleServer::StartListening() {
	try {
		LOG(DEBUG) << "ConsoleServer started";

		std::function<void(dev::channel::ChannelException, dev::channel::ChannelSession::Ptr)> fp = std::bind(&ConsoleServer::onConnect, shared_from_this(), std::placeholders::_1, std::placeholders::_2);
		_server->setConnectionHandler(fp);

		_server->run();

		_running = true;
	}
	catch(std::exception &e) {
		LOG(ERROR) << "StartListening failed! " << boost::diagnostic_information(e);

		BOOST_THROW_EXCEPTION(e);
	}
}

void ConsoleServer::StopListening() {
	if (_running) {
		_server->stop();
	}

	_running = false;
}

void ConsoleServer::onConnect(dev::channel::ChannelException e, dev::channel::ChannelSession::Ptr session) {
	if(e.errorCode() != 0) {
		LOG(ERROR) << "Accept console connection error!" << boost::diagnostic_information(e);

		return;
	}

	std::function<void(dev::channel::ChannelSession::Ptr, dev::channel::ChannelException, dev::channel::Message::Ptr)> fp = std::bind(
			&ConsoleServer::onRequest,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2,
			std::placeholders::_3);
	session->setMessageHandler(fp);

	session->run();
}

void ConsoleServer::onRequest(dev::channel::ChannelSession::Ptr session, dev::channel::ChannelException e, dev::channel::Message::Ptr message) {
	if(e.errorCode() != 0) {
		LOG(ERROR) << "ConsoleServer onRequest error: " << boost::diagnostic_information(e);

		return;
	}

	std::string output;
	try {
		std::string request((const char*)message->data(), message->dataSize());
		LOG(TRACE) << "ConsoleServer onRequest: " << request;

		std::vector<std::string> args;
		boost::split(args, request, boost::is_any_of(" "));

		if(args.empty()) {
			output = "Emput input!";

			throw std::exception();
		}

		std::string func = args[0];
		args.erase(args.begin());

		if(func == "help") {
			output = help(args);
		}
		else if(func == "status") {
			output = status(args);
		}
		else if(func == "p2p.peers")
		{
			output = p2pPeers(args);
		}
		else if(func == "p2p.miners")
		{
			output = p2pMiners(args);
		}
		else if(func == "amdb.select")
		{
			output = amdbSelect(args);
		}
		else {
			output = "Unknown command, enter 'help' for command list";
		}
	}
	catch(std::exception &e) {
		LOG(WARNING) << "Error: " << boost::diagnostic_information(e);
	}

	auto response = session->messageFactory()->buildMessage();
	response->setData((byte*)output.data(), output.size());

	session->asyncSendMessage(response, dev::channel::ChannelSession::CallbackType(), 0);

}

std::string ConsoleServer::help(const std::vector<std::string> args) {
	std::stringstream ss;

	//ss << ""
}

std::string ConsoleServer::status(const std::vector<std::string> args) {
	std::string output;

	try {
		std::stringstream ss;

		ss << "=============Node status=============\n";
		ss << "Status: ";
		if(_interface->wouldSeal()) {
			ss << "sealing";
		}
		else if(_interface->isSyncing() || _interface->isMajorSyncing()){
			ss << "syncing block...";
		}

		ss << std::endl;

		ss << "Block number:" << _interface->number() << " at view:" << dynamic_cast<dev::eth::PBFT*>(_interface->sealEngine())->view();
		ss << std::endl;
		output = ss.str();
	}
	catch(std::exception &e) {
		LOG(ERROR) << "ERROR: " << boost::diagnostic_information(e);

		output = "ERROR while status";
	}

	return output;
}

std::string ConsoleServer::p2pPeers(const std::vector<std::string> args) {
	std::string output;

	try {
		std::stringstream ss;

		ss << "=============P2P Peers=============\n";
		ss << "Node number: ";
		std::vector<p2p::PeerSessionInfo> peers = _webThreeDirect->peers();
		ss << peers.size() << std::endl;
		const dev::p2p::Host &host = _webThreeDirect->net();
		for(size_t i = 0; i < peers.size(); i++)
		{
			const p2p::NodeID nodeid = peers[i].id;
			ss << "Nodeid: " << nodeid << std::endl;
			ss << "Ip: " << peers[i].host << std::endl;
			ss << "Port:" << peers[i].port << std::endl;
			ss << "Connected: " << host.isConnected(nodeid) << std::endl;
		}
		ss << std::endl;
		output = ss.str();
	}
	catch(std::exception &e) {
		LOG(ERROR) << "ERROR: " << boost::diagnostic_information(e);

		output = "ERROR while p2p.peers";
	}

	return output;
}

std::string ConsoleServer::p2pMiners(const std::vector<std::string> args) {
	std::string output;

	try {
		std::stringstream ss;

		ss << "=============P2P Miners=============\n";
		ss << "Miners number: ";
		dev::h512s minerNodeList =
				dynamic_cast<dev::eth::PBFT*>(_interface->sealEngine())->getMinerNodeList();
		ss << minerNodeList.size() << std::endl;
		const dev::p2p::Host &host = _webThreeDirect->net();
		for(size_t i = 0; i < minerNodeList.size(); i++)
		{
			const p2p::NodeID nodeid = minerNodeList[i];
			ss << "Nodeid: " << nodeid << std::endl;
//			ss << "ip: " << peers[i].host << std::endl;
//			ss << "port:" << peers[i].port << std::endl;
//			ss << "connected: " << host.isConnected(nodeid) << std::endl;
		}
		ss << std::endl;
		output = ss.str();
	}
	catch(std::exception &e) {
		LOG(ERROR) << "ERROR: " << boost::diagnostic_information(e);

		output = "ERROR while p2p.peers";
	}

	return output;
}

std::string ConsoleServer::amdbSelect(const std::vector<std::string> args) {
	std::string output;

	try {
		std::stringstream ss;
		if(args.size() == 2)
		{
			std::string tableName = args[0];
			std::string key = args[1];
			unsigned int number = _interface->number();
			h256 hash = _interface->hashFromNumber(number);
			ss << "number: " << number << std::endl;
			ss << "hash: " << hash << std::endl;
			dev::storage::Entries entries = _stateStorage->select(
					_interface->hashFromNumber(number), number, tableName, key);
//			storage::Entries::Ptr entries = std::make_shared<storage::Entries>();
//			storage::Entry::Ptr entry = std::make_shared<storage::Entry>();
//			entry->setField("key1", "hello1");
//			entry->setField("key2", "hello2");
//			entry->setField("key3", "hello3");
//			entry->setField("key4", "hello4");
//			entries->addEntry(entry);
      size_t size = entries->size();
      ss << "Number of entries: " << size << std::endl;
      for (size_t i = 0; i < size; ++i)
      {
        storage::Entry::Ptr entry = entries->get(i);
        std::map<std::string, std::string> *fields = entry->fields();
        std::map<std::string, std::string>::iterator it;
        for(it = fields->begin(); it != fields->end(); it++)
        {
          ss << it->first << ": " << it->second << std::endl;
        }
      }
//			ss << "tableName: " << tableName << std::endl;
//			ss << "key:"<< key << std::endl;
			output = ss.str();
		}
		else
		{
			ss << "You must specify table name and table key, for example" << std::endl;
			ss << "amdb.select t_test name" << std::endl;
		}
		ss << std::endl;
		output = ss.str();
	}
	catch(std::exception &e) {
		LOG(ERROR) << "ERROR: " << boost::diagnostic_information(e);

		output = "ERROR while amdb.select";
	}

	return output;
}

