
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

private:                //We protect matters related to the Session because we dont' want other functions interefering with them.
    NClientPtr _client;
    NSessionPtr _session;
    NRtClientPtr _rtClient;
    NRtDefaultClientListener _listener;
    std::thread _theTicker;
    bool _clientActive = false;

 
    //std::string _serverKey = "defaultKey";
    std::string _localHost = "127.0.0.1";
    std::string _serverDomain = "server.interactivescores.online";  //EC2 - 54.245.57.3	  //Joe's IP 73.11.171.178 //LocalHost : 127.0.0.1

    //The litany of subfunctions
    void theTick() {
        std::cout << "ticking" << std::endl;
        while (_clientActive) {
            _client->tick();
            if (_rtClient) { //rtCLient is a pointer.  It will return false if no value is assigned.
                _rtClient->tick();
                //std::cout << "rtclient tick" << std::endl;
            }
            //std::cout << "Tick" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::cout << "stopped ticking" << std::endl;
    }
    void stopTheTicker() {
        _clientActive = false;
        if (_theTicker.joinable()) {
            _theTicker.join();
        }
    }
    void startRtClient() {
        //This implementation follows the pattern in the GitHub Docs.  https://github.com/heroiclabs/nakama-cpp  
        //Other patterns may be possible depending on what we want to listen for.

        bool createStatus = true; // if the server should show the user as online to others.

        _rtClient = _client->createRtClient();
        _rtClient->setListener(&_listener);
        _listener.setConnectCallback([]()
            {
                std::cout << "Socket connected." << std::endl;
            });

        _rtClient->connect(_session, createStatus);
        return;
    };
    


    void updateDisplayName(std::string displayName) {
        std::cout << "called updateDisplayName()" << std::endl;

        auto successCallback = []() {
            std::cout << "Changed Display Name" << std::endl;
            };

        auto errorCallback = [](const NError& error) { std::cout << "Failed to update Display Name" << std::endl; };

        _client->updateAccount(_session,
            opt::nullopt,
            displayName,
            opt::nullopt,
            opt::nullopt,
            opt::nullopt,
            opt::nullopt,
            successCallback,
            errorCallback
        );
    }


    //The litany of public functions, called by Max Objects.
public:
    int latestpacket = 0;
    NMatchData matchdata;

    ~NakamaSessionManager() {
        stopTheTicker();
    }

   
   


    // AUTH RELATED FUNCTIONS
    #pragma region AuthFunctions
    void initializeClient(bool local = 0) {
        NClientParameters parameters;
        //parameters.serverKey = _serverKey;
        if (local) { parameters.host = _localHost; }
        else { parameters.host = _serverDomain; }
        _client = createDefaultClient(parameters);
        _clientActive = true;
        if (!(_theTicker.joinable())) { _theTicker = std::thread{ &NakamaSessionManager::theTick, this }; }  //This ensures the thread only starts on the first attempt to connect. I don't love this solution.  https://forum.heroiclabs.com/t/how-join-tick-thread-on-authentication-error-callback/6390

        return;
    }
    
    void authenticateClientDeviceId(std::string deviceID, std::string userName, std::function<void()> onSuccess = nullptr, std::function<void(std::string)> onError = nullptr) {

        auto successCallback = [this, userName, onSuccess](NSessionPtr session)
            {
                //Authenticate Device impelemnts a Callback That Returns a Session Pointer.
                std::cout << session->getAuthToken() << std::endl;
                std::cout << "GOT THE TOKEN!\n";
                _session = session;
                updateDisplayName(userName);
                startRtClient();
                onSuccess();

                //TODO: save session token in your storage

            };

        auto errorCallback = [this, onError](const NError& error)
            {
                //per "peek definition" NError is a struct.  It contins a Message and a code.  You can print the error.message, or use the toString funct8ion that the API has defined.   
                std::cout << "Something went wrong.  Code = " << int(error.code) << "   " << error.message << std::endl;
                if (int(error.code) == -1) { onError("Unable to reach the server."); }
                else { onError("Something went wrong."); }
                //stopTheTicker();
            };


        _client->authenticateDevice(deviceID, opt::nullopt, opt::nullopt, {}, successCallback, errorCallback);

        return;

    }
    void authenticateClientEmail(const std::string& email, const std::string& password, const std::string& userName, bool create, std::function<void(std::string, std::string)> onSuccess = nullptr, std::function<void(std::string)> onError = nullptr) {

        auto successCallback = [this, userName, onSuccess](NSessionPtr session)
            {
                //Authenticate Device impelements a Callback That Returns a Session Pointer.
                std::cout << session->getAuthToken() << std::endl;

                std::cout << "GOT THE TOKEN!\n";
                _session = session;
                startRtClient();
                onSuccess(session->getAuthToken(), session->getRefreshToken());

            };

        auto errorCallback = [this, onError](const NError& error)
            {
                //per "peek definition" NError is a struct.  It contins a Message and a code.  You can print the error.message, or use the toString funct8ion that the API has defined.   

                std::cout << "Something went wrong.  Code = " << int(error.code) << " \n  " << error.message << std::endl;
                if (int(error.code) == -1) { onError("Unable to reach the server."); }
                else if (int(error.code) == 4 || int(error.code) == 3 || int(error.code) == 2 || int(error.code) == 1) { onError(error.message.substr(9)); }
                else if (int(error.code) == 0) { onError(error.message.substr(9, (error.message.length() - 9 - 7))); }
                else { onError("Something went wrong."); }

                std::cout << "**NERROR Code** " << "\n" << int(error.code) << " \n \n " << "**NERROR Message**" << "\n" << error.message << std::endl;

            };

        NStringMap vars = {
        { "clientVersion", "1.0" }
        };

        _client->authenticateEmail(email, password, userName, create, vars, successCallback, errorCallback);


        return;
    }


    void refreshTheSession(std::string authToken, std::string refreshToken, std::function<void(std::string, std::string)> onSuccess = nullptr, std::function<void(std::string)> onError = nullptr) {
        //if there is a Session token in storage, try to use it.
        if (!authToken.empty())
        {
            // Lets check if we can restore a cached session.
            auto session = restoreSession(authToken, refreshToken);
            _session = session;
            
            auto refreshSuccessCallback = [onSuccess](NSessionPtr session)
                {
                    onSuccess(session->getAuthToken(), session->getRefreshToken());
                };

            auto refreshErrorCallback = [onError](const NError& error)
                {
                    
                    std::cout << "Something went wrong.  Code = " << int(error.code) << "   " << error.message << std::endl;
                    if (int(error.code) == -1) { onError("Unable to reach the server."); }
                    else if (int(error.code) == 4 || int(error.code) == 3 || int(error.code) == 2 || int(error.code) == 1) { onError(error.message.substr(9)); }
                    else { onError("Something went wrong."); }
                    
                };

            // Refresh the existing session
            _client->authenticateRefresh(session, refreshSuccessCallback, refreshErrorCallback);
            std::cout << "tried to refresh" << std::endl;
            
            //if (!session->isExpired())
           // {
                // Session was valid and is restored now.
           //     _session = session;
           //     return;
            //}
        }
    }

    void signOut() {
        _client->disconnect();         
    } //TODO
    #pragma endregion

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


