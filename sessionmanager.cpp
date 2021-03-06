#include <thread>
#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <jsoncpp/json.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "udpsocket.h"
#include "tcpsocket.h"
#include "socketexception.h"
#include "sessionmanager.h"
#include "settings.h"
#include "appobject.h"
#include "connector.h"

using namespace std;

SessionManager::SessionManager():
		stopAdvertising(true),
		stopListening(true),
		clientApp(NULL)
{
}

void SessionManager::start()
{
	if (stopAdvertising)
	{
		stopAdvertising = false;
		advertising = new thread(advertisingLauncher, this);
	}

	if (stopListening)
	{
		stopListening = false;
		listener = new thread(listenerLauncher, this);
	}
}

void SessionManager::stop()
{
	list<Connector*>::const_iterator it;

	if (!stopAdvertising)
	{
		stopAdvertising = true;
		advertising->join();
		delete advertising;
	}

	if (!stopListening)
	{
		stopListening = true;
		listener->join();
		delete listener;
	}

	// delete registered clients apps
	for (it=connectorList.begin(); it!=connectorList.end(); ++it) {
		delete *it;
	}
}

void SessionManager::advertisingLauncher(void* instance)
{
	static_cast<SessionManager*>(instance)->advertisingThread();
}

void SessionManager::advertisingThread()
{
	UdpSocket udpSocket;
	Json::Value broadcastMessage;
	chrono::milliseconds TicTac( 1000 );	// 1s
	vector<string> IPAddr;

	broadcastMessage["DeviceName"] = Settings::instance().getDeviceName();
	broadcastMessage["ListenerPort"] = Settings::instance().getListenerPort();
	broadcastMessage["CurrentClientNumber"] = static_cast<int>(connectorList.size());
	udpSocket.getIPs(IPAddr);
	for (int i=0; i<IPAddr.size(); i++) {
		broadcastMessage["ListenerIP"][to_string(i)] = IPAddr[i];		//TODO: figure out what's best: [tostring(i)] vs [i]
	}

	while (!stopAdvertising) {
		try {
			udpSocket << broadcastMessage.toStyledString();
		}
		catch (SocketException& e) {
			stopAdvertising = true;
			cout << e.description();
		}

		this_thread::sleep_for( TicTac );
	}

	cout << "SessionManager advertising thread stopped.\n";
}

void SessionManager::listenerLauncher(void* instance)
{
	static_cast<SessionManager*>(instance)->listenerThread();
}

void SessionManager::listenerThread()
{
	chrono::milliseconds TicTac( 10000 );	// 10s
	TcpSocket serverSocket;
	TcpSocket* clientSocket;
	Connector* connector;

	try {
		serverSocket.create();
	}
	catch (SocketException& e) {
		stopListening = true;
		cout << e.description();
	}

	while (!stopListening) {
		cout << "Wait for a connection from a host.\n";
		clientSocket = new TcpSocket();
		try {
			serverSocket.accept(*clientSocket);
		}
		catch (SocketException& e) {
			cout << e.description();
			break;
		}

		connector = new Connector(clientApp, *clientSocket);
		connectorList.push_back(connector);
	}  /* while (!stopListening) */

	serverSocket.close();
	cout << "SessionManager listener thread stopped.\n";
}

void SessionManager::registerClient(AppObject* object)
{
	if (object != NULL)
		clientApp = object;
}

bool SessionManager::isRunning()
{
	return ~(stopListening || stopAdvertising);
}
