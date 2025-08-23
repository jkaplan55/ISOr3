#pragma once


#include <iostream>
#include "nakama.h"



//Functions to Covert Structs to JSON
std::string convertBooltoString(bool b) {
    if (b) {
        return "1";
    }
    else {
        return "0";
    }
}


std::string convertPresenceToJSON(NUserPresence presence) {
    std::string JSON = std::string("{ \"userID\": \"" + presence.userId + "\", \"sessionId\": \"" + presence.sessionId + "\", \"username\": \"" + presence.username + "\", \"persistence\": \"" + convertBooltoString(presence.persistence) + "\", \"status\": \"" + presence.status + "\"}");
    return JSON;
}

std::string convertPresenceListtoJSON(std::vector<NUserPresence> presences) {
    std::string presenceList = "[";
    for (NUserPresence p : presences) {
        presenceList = std::string(presenceList + convertPresenceToJSON(p));
        presenceList = std::string(presenceList + ",");
    };
    if (presences.size() != 0) {
        presenceList.pop_back();
    }
    presenceList = std::string(presenceList + "]");
    return presenceList;
}

std::string convertMatchtoJSON(NMatch match) {
    std::string JSON = std::string("{ \"Match\": { \"matchID\": \"" + match.matchId + "\", \"authoritative\": \"" + convertBooltoString(match.authoritative) + "\", \"label\": \"" + match.label + "\", \"presences\": " + convertPresenceListtoJSON(match.presences) + ", \"self\": " + convertPresenceToJSON(match.self) + "}}");
    return JSON;
}



std::string convertMatchDatatoJSON(NMatchData matchData) {
    std::string JSON = std::string("{ \"MatchData\": { \"matchID\": \"" + matchData.matchId + "\", \"sender\": " + convertPresenceToJSON(matchData.presence) + ", \"opCode\": " + std::to_string(matchData.opCode) + ", \"data\": " + matchData.data + "}}");
    return JSON;
}