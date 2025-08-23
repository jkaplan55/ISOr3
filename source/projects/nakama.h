
#pragma once

#include <thread>
#include <iostream>
#include "nakama-cpp/Nakama.h"
#include <future>
#include "json.hpp"
#include "cpr/cpr.h"



using namespace NAKAMA_NAMESPACE;

class NakamaSessionManager
{
public:
    bool& clientActiveRef = clientActive;
    int latestpacket = 0;
    bool ticking = false;
    NMatchData matchdata;



private:                //We protect matters related to the Session because we dont' want other functions interefering with them.
    NClientPtr _client;
    NSessionPtr _session;
    NRtClientPtr _rtClient;
    NRtDefaultClientListener _listener;
    std::thread theTicker;
    bool clientActive = false;
    
      
    //The litany of subfunctions
    NClientPtr createClient() {
        NClientParameters parameters;
        parameters.host = "127.0.0.1";  //EC2 - 54.245.57.3	  //Joe's IP 73.11.171.178 //LocalHost : 127.0.0.1
        return createDefaultClient(parameters);
    }
    void restoreSession() {     //TODO!
        // to do: read session token from your storage
      // string sessionToken;

       ////if there is a Session token in storage, try to use it.
       //if (!sessionToken.empty())
       //{
       //    // Lets check if we can restore a cached session.
       //    auto session = restoreSession(sessionToken);

       //    if (!session->isExpired())
       //    {
       //        // Session was valid and is restored now.
       //        _session = session;
       //        clientActive = true;
       //        return;
       //    }
       //}
    }
    void authenticateClient(const std::string& deviceId, const std::string& userName, std::promise<bool>&& connectionComplete) {
        
        auto successCallback = [this, &userName, &connectionComplete](NSessionPtr session)
        {
            //Authenticate Device impelemnts a Callback That Returns a Session Pointer.
            std::cout << session->getAuthToken() << std::endl;
            std::cout << "GOT THE TOKEN!\n";
            _session = session;
            updateDisplayName(userName);
            startRtClient(std::move(connectionComplete));
           

            //TODO: save session token in your storage

        };
        
        auto errorCallback = [](const NError& error)
        {
            //per "peek definition" NError is a struct.  It contins a Message and a code.  You can print the error.message, or use the toString funct8ion that the API has defined.   
            std::cout << "Something went wrong:" << error.message << std::endl;
        };
        _client->authenticateDevice(deviceId, opt::nullopt, opt::nullopt, {}, successCallback, errorCallback);
       
        std::cout << "Called authenticateDevice\n";

        return;
        
    }
    void startRtClient(std::promise<bool>&& connectionComplete) {
        int port = 7350; // different port to the main API port
        bool createStatus = true; // if the server should show the user as online to others.
        
        _rtClient = _client->createRtClient(port);
        _rtClient->setListener(&_listener);
        _listener.setConnectCallback([this, &connectionComplete]()  
            {
                std::cout << "Socket connected." << std::endl;
                connectionComplete.set_value(true);    //SetthePromise that signifies the end of the "get connected" chain."

                
                
            });

        _rtClient->connect(_session, createStatus);
        return;
    };
    void updateDisplayName(std::string displayName) {
        auto successCallback = [] () {
            std::cout << "Changed Display Name" << std::endl;
        };
        _client->updateAccount(_session,
            opt::nullopt, 
            displayName, 
            opt::nullopt, 
            opt::nullopt,
            opt::nullopt, 
            opt::nullopt,
            successCallback
        );
    }
    void TheTick() {
        ticking = true;
        std::cout << "ticking" << std::endl;
        while (clientActive) {
            _client->tick();
            if (_rtClient) { //rtCLient is a pointer.  It will return false if no value is assigned.
                _rtClient->tick();
                //std::cout << "rtclient tick" << std::endl;
            }
            //std::cout << "Tick\n";
            //std::cout << "Session Active = " << clientActive << "\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
        ticking = false;
        std::cout << "stopped ticking" << std::endl;
    }
    

    //The litany of public functions, called by Max Objects.
public:

    //I would love to be able to take a callback from the Max Object.  But passing callbacks from another class is proving difficult.  https://youtu.be/Bm2jmUxkUVw?t=931, https://blog.mbedded.ninja/programming/languages/c-plus-plus/callbacks/, https://stackoverflow.com/questions/10537131/how-to-pass-a-method-as-callback-to-another-class
       //For now we will use promises and futures.  https://www.youtube.com/watch?v=XDZkyQVsbDY

    void start(const std::string& deviceId, const std::string& userName)
    {
        std::promise<bool> connectionStatusPromise;
        std::future<bool> connectionStatusFuture = connectionStatusPromise.get_future();




        NLogger::initWithConsoleSink(NLogLevel::Debug);
        _client = createClient();
        clientActive = true;
        theTicker = std::thread{ &NakamaSessionManager::TheTick, this };
        authenticateClient(deviceId, userName, std::move(connectionStatusPromise));
        connectionStatusFuture.get();
 
      
        return;
    }
    
    void stopTicking() {
        theTicker.join();
    }

    NMatch createMatch() {
        setMatchDataCallback();
        std::promise<NMatch> matchPromise;
        std::future<NMatch>  matchFuture = matchPromise.get_future();
        auto successCallback = [&matchPromise](const NMatch& match) {
            matchPromise.set_value(match);
        };

        _rtClient->createMatch(successCallback);            //The CreateMatch function of the rtCLient takes two parameters.  A success callback and an otpional error callback.  In this case we use a lambda for the success callback.  

        return matchFuture.get();
    };
    
    std::string getMatchList(std::string label) {
        std::promise<std::string> matchListPromise;
        std::future<std::string>  matchListFuture = matchListPromise.get_future();
        std::string payload;
        _rtClient->rpc("list_matches", label, [&matchListPromise](const NRpc& matches) {
            std::cout << matches.payload << std::endl;
            matchListPromise.set_value(matches.payload);

            });
        payload = matchListFuture.get();
        payload = std::string("{\"MatchList\":" + payload + "}");
        return payload;
    }

    
    std::string createAuthoritativeMatch(std::string moduleName) {
        std::promise<std::string> createMatchPromise;
        std::future<std::string>  createMatchFuture = createMatchPromise.get_future();
        std::string payload;
        
        auto successCallback = [&createMatchPromise](const NRpc& matchId) {
            createMatchPromise.set_value(matchId.payload);
        };
        //TODO:  Make an error callback for missing module name.  

        _rtClient->rpc("create_match", moduleName, successCallback);
        payload = createMatchFuture.get();
        
        std::cout << "created match.  module: " << moduleName << ". MatchId: " << payload << std::endl;
        return payload;
    }

    NMatch joinMatch(std::string matchId) {
        std::promise<NMatch> matchPromise;
        std::future<NMatch>  matchFuture = matchPromise.get_future();
        setMatchDataCallback();
        _rtClient->joinMatch(matchId, {}, [&matchPromise](const NMatch& match) {
            matchPromise.set_value(match);
            std::cout << "Joined Match!\n\n" << std::endl;

            /* for (auto& presence : match.presences)
            {
                if (presence.userId != match.self.userId)
                {
                    std::cout << "User id " << presence.userId << " username " << presence.username << std::endl;
                }
            }
            */
            });
        return matchFuture.get();

    };
    
    
    void leaveMatch(std::string& matchId) {
        _rtClient->leaveMatch(matchId);
    };
    

    void sendMatchData(std::string matchId, int64_t opCode, std::string sentData) {
        _rtClient->sendMatchData(matchId, opCode, (NBytes)sentData);
        std::cout << "send Match Packet" << std::endl;
    }

    void setMatchDataCallback() {
        //string matchdatastring;
        _listener.setMatchDataCallback([this](const NMatchData& data)
            {
                // string v = data.data;   //QUESTION:  why can't I assign data.data directly to matchdata? [like if I wanted to capture it from the enclosing function.
               // v = matchdatastring;
                matchdata = data;
                latestpacket = latestpacket + 1;

                /* {
                    switch (data.opCode)
                    {
                    case 101:
                        std::cout << "A custom opcode." << std::endl;
                        break;

                    default:
                        std::cout << "User " << data.presence.userId << " sent " << data.data << std::endl;
                        break;
                    }*/
            });
        // return matchdatastring;
    }

   
    void uploadFile(const std::string& fileName) {
        //std::string presignedURL;
        //std::string& presignedURLptr = presignedURL;
        auto successCallback = [fileName](const NRpc& rpc)
            
        {
            std::cout << "Presigned URL = " + rpc.payload + "\n" << std::endl;
           
            nlohmann::json JSON = nlohmann::json::parse(rpc.payload);
            std::cout << JSON.value("URL", "not found") << std::endl;
            std::string URL = JSON.value("URL", "not found");
         
            cpr::Response r = cpr::Put(
            cpr::Url{ URL },
            cpr::File{ "C:/Users/Joe/Documents/Max 8/Packages/ISOr2/" + fileName });
            std::cout << r.status_code + "\n" << std::endl;
            std::cout << r.status_code + "\n" << std::endl;
        };

       
        auto errorCallback = [](const NError& error)
        {
            std::cout << "Error calling RPC " << error.message << std::endl;
        };

        std::string payload = fileName;
        _client->rpc(_session, "GetUploadURL", payload, successCallback, errorCallback);

        
        //return payload;
        
    }
    



};
 


NakamaSessionManager* getSessionPtr() {
    NakamaSessionManager MySession;
    NakamaSessionManager* SessionPtr = &MySession;
    c74::max::t_symbol* SessionSym = c74::max::gensym("SessionSym");
    SessionPtr = (NakamaSessionManager*)SessionSym->s_thing;
    return SessionPtr;
}


