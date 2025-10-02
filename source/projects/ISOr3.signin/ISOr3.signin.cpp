/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.


// This Object paints a signin screen for ISO.  If the RememberMe box is checked then UserName, RefreshToken and RememberMeCheck status are stored in a file called ISOdata.dat.
// The file is read and values are restored in the constructor.  If the box is checked, then the files is written after a successful auth.  It is also written in the destructor.


#include "c74_min.h"
#include "../custom_min_operator_ui.h"   //https://cycling74.com/forums/how-does-jboxtextfield-work

#include <string>
#include <thread>
#include <fstream>


#include "json.hpp"
#include "../nakama.h"

using namespace c74::min;
using namespace c74::min::ui;

//Using custom uiFlags to get the behaviour i want.
const long uiflags = 0
| JBOX_DRAWFIRSTIN    // 0
// | JBOX_NODRAWBOX                        // 1
| JBOX_DRAWINLAST                       // 2
//| JBOX_TRANSPARENT		// 3
| JBOX_NOGROW			// 4
// 	| JBOX_GROWY			// 5
// | JBOX_GROWBOTH                         // 6
//	| JBOX_IGNORELOCKCLICK	// 7
//	| JBOX_HILITE			// 8
// | JBOX_BACKGROUND                       // 9
//	| JBOX_NOFLOATINSPECTOR	// 10
//| JBOX_TEXTFIELD		// 11
//   | c74::max::JBOX_MOUSEDRAGDELTA	// 12
//	| JBOX_COLOR			// 13
//	| JBOX_BINBUF			// 14
//	| JBOX_DRAWIOLOCKED		// 15
//	| JBOX_DRAWBACKGROUND	// 16
//	| JBOX_NOINSPECTFIRSTIN	// 17
//	| JBOX_DEFAULTNAMES		// 18
//	| JBOX_FIXWIDTH			// 19
;



class signin : public object<signin>, public custom_ui_operator<465, 365, uiflags> {
private:
    bool	m_bang_on_change{ true };
    number	m_unclipped_value{ 0.0 };
    number  m_anchor{};
    string	m_text;
    bool	m_mouseover{};
    number  m_mouse_position[2]{};
    number	m_range_delta{ 1.0 };
    number  m_fontsize = 14.0;
    symbol  m_fontname = "lato-light";
    
    string errorMessage;

    NakamaSessionManager MySession;
    NakamaSessionManager* theSession = &MySession;
    
    
    c74::max::t_symbol* SessionSym = c74::max::gensym("SessionSym");
    

    //DECLARATIONS FOR THE TEXT BOXES
    #pragma region TextBoxes
    //pointers to store dimensions of the fonts.  Which get updated with each redraw();
    double fontw = 0;
    double fonth = 0;

    double* fontwidth = &fontw;
    double* fontheight = &fonth;

    //The Text Buffer.  Space around the Text
    double textBuffer = (*fontheight * .25);

    //A placeholder string for fourty chacaters
    string placeholderString = "1234567890123456789012345678901234567890123";

    //Some setup to manage cursor blinking
    bool showCursor{ false };
    std::condition_variable cv; // mutex and condition variable to end the sleep early before joining the thread. https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread 
    std::mutex cv_m;
    std::thread blinkCursor;
    void toggleCursor() {
        while ((activeTextBoxPtr)) {
            showCursor = !showCursor;
            redraw();
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait_for(lk, std::chrono::milliseconds(500));
            //Sleep(500);
        }
    }

    void stopCursorThread() {
        showCursor = false;

        if (blinkCursor.joinable()) {
            {
                std::unique_lock <std::mutex> lk(cv_m);  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
                cv.notify_one();  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
            }
            blinkCursor.join();
        }
    }

    //A Struct to Define the TextBox.  This could be a class, and then the cursor could be a member function, but it gets complicated passing functions and values from an enclosing class to a subclass.
    struct textBox {
        int x;  //the x coordinate of the bottom left of the first character
        int y;  //the y coordinate of the bottom left of the first character
        string text;
        bool masked;   //a Bool to determine if text should be masked when painted.
        string labelText;
    };

    //Declare the TextBoxes
    textBox usernameBox{ 0, 0, "", 0, "ACCOUNT NAME" };  //x and y are set at paint time.
    textBox passwordBox{ 0, 0, "", 1, "PASSWORD" };
    textBox newUsernameBox{ 0, 0, "", 0, "ACCOUNT NAME" };
    textBox newEmailBox{ 0, 0, "", 0, "EMAIL" };
    textBox newPasswordBox{ 0, 0, "", 1, "PASSWORD" };
     

    //Points to mange the Active TextBox.
    std::vector<textBox*> signInTextBoxPtrs = { &usernameBox, &passwordBox };
    std::vector<textBox*> newAccountTextBoxPtrs = { &newUsernameBox, &newEmailBox, &newPasswordBox };

    textBox* activeTextBoxPtr;

    textBox* getNextTextBox(textBox* currentTextBox, vector<textBox*> textBoxes) {     
        // Find the element in the std::array
        auto vectorElementPtr = std::find(textBoxes.begin(), textBoxes.end(), currentTextBox);
        int textBoxIndex = std::distance(textBoxes.begin(), vectorElementPtr);
        
        if (textBoxIndex == textBoxes.size() - 1) {  //If you're at the end go back to the beginning
            return textBoxes[0];
        }
        else {   //return the next one
            return textBoxes[textBoxIndex + 1];
        }            
   }
    #pragma endregion
    
    //DECLARATIONS FOR MANAGING STATE
    #pragma region StateManagement 
    //Bool for Checkboxes
    bool rememberMeCheck{ false };
    bool newAccountCheck{ false };

    //Enum for objectState
    enum class objectState {
        disconnected,
        connecting,
        connected,
        newAccount,
        newAccountAttempt
    };

    objectState objectStateVar = objectState::disconnected;
    objectState stateOnPreviousPaint = objectStateVar;
    #pragma endregion
     
    //DECLARATIONS FOR DATA PERSISTENCE
    #pragma region DataPersistence
     //Generate Random String
    std::string gen_random(const int len) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "!@#$%^&*()[]{}:";
        std::string tmp_s;
        tmp_s.reserve(len);

        for (int i = 0; i < len; ++i) {
            tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
        }

        return tmp_s;
    }


    //Encrypt/Decrypt the String for the File
    std::string encrypt_decrypt(const std::string& text, char key) {
        std::string result = text;
        for (char& c : result) {
            c ^= key;
        }
        return result;
    }

    //Define and Declare User Data Struct
    string authToken = "";
    string refreshToken = "";

    //Jsonify and DeJSONify DataScruct
    string jsonifyUserData() {
        nlohmann::json j = nlohmann::json{ {"userName", usernameBox.text}, {"authToken", authToken}, {"refreshToken", refreshToken}, {"rememberMe", rememberMeCheck} };
        return j.dump();
    }

    void deJsonifyUserData(string dataString) {
        nlohmann::json j = nlohmann::json::parse(dataString);
        j.at("userName").get_to(usernameBox.text);
        j.at("authToken").get_to(authToken);
        j.at("refreshToken").get_to(refreshToken);
        j.at("rememberMe").get_to(rememberMeCheck);

        return;
    }

    //
    //Read and Write the File
    std::string getDataFilePath(){
        std::ofstream file;

#ifdef TARGET_OS_MAC
        auto external_path = path{"ISOr3.signin.mxo", path::filetype::external, 0};  //Get the path to the external
        string dataFilePath = static_cast<std::string>(external_path).substr(0, static_cast<std::string>(external_path).length() - 16) + "ISOdata.dat";
#elif _WIN64
        auto external_path = path{ "ISOr3.signin.mxe64", path::filetype::external, 0 };  //Get the path to the external
        string dataFilePath = static_cast<std::string>(external_path).substr(0, static_cast<std::string>(external_path).length() - 18) + "ISOdata.dat";
#endif
        
        cout << dataFilePath << endl;
        
        return dataFilePath;
    }
    
    void readDataFile() {
        std::ifstream file;
        std::string dataFilePath = getDataFilePath();
        file.open(dataFilePath);
                
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            file.close();
            string jsonString = buffer.str().substr(56, buffer.str().length() - 181);
            cout << encrypt_decrypt(jsonString, *"d") << endl; //the Cypher was failing intermittently with "Q".  I suspect because it was shifting the letters into some character that was causing rdbuf() to end early. Going with a smaller shift.  I will remove this print once I'm confident it is safe.
            deJsonifyUserData(encrypt_decrypt(jsonString, *"d"));
        }

 
        else {
            cerr << "Error reading data file" << endl;
        }


        return;
    }

    void writeFile() {
        std::ofstream file;
        std::string dataFilePath = getDataFilePath();
        file.open(dataFilePath);
        
        if (file.is_open()) {
            //file << gen_random(56) << jsonifyUserData() << gen_random(124) << std::endl;
            file << gen_random(56) << encrypt_decrypt(jsonifyUserData(), *"d") << gen_random(124) << std::endl;
            file.close();
        }
        else {
            cout << "Error writing userData file" << endl;
        }

        return;
    }
    #pragma endregion
   
    //DECLARATIONS FOR OBJECT INITIALIZATION    
    #pragma region Initialization
    void makeSingleton() {
        c74::max::t_symbol* Exists = c74::max::gensym("AuthenticatorInstance");
        if (Exists->s_thing) {
            error("Authentication object already exists.");
            return;
        }
        Exists->s_thing = maxobj();
    };
    bool constructionComplete = false;
    std::thread postConstructionInit;
    void postConstructionInitialization()
    {
        bool waiting = true;
        while (waiting) {
            if (constructionComplete) {
                waiting = false;
                theSession->initializeClient(local);
                if (rememberMeCheck) {
                    authenticateWithRefresh(authToken, refreshToken);
                }
            }
        }

    }
    #pragma endregion

    //FUNCTIONS FOR AUTHENTICATION
    #pragma region AuthFunctions
    void authenticateWithEmail() {
        auto onSuccess = [this](std::string auth, std::string refresh) {
            cout << "connected!" << endl;
            if (objectStateVar == objectState::newAccountAttempt) { usernameBox.text = newUsernameBox.text; }
            authToken = auth;
            refreshToken = refresh;
            newAccountCheck = 0;
            if (rememberMeCheck) { writeFile(); };
            objectStateVar = objectState::connected;            
            redraw();            
            };

        auto onError = [this](std::string error) {
            if (objectStateVar == objectState::newAccountAttempt && error == "Invalid credentials.") { error = "Email already in use."; }
            errorMessage = error;
            cout << error << endl;
            cout << errorMessage << endl;
            if (objectStateVar == objectState::newAccountAttempt) { objectStateVar = objectState::newAccount; }
            else if (objectStateVar == objectState::connecting) { objectStateVar = objectState::disconnected; }
            redraw();
            };


        if (objectStateVar == objectState::disconnected) {
            theSession->authenticateClientEmail("", passwordBox.text, usernameBox.text, 0, onSuccess, onError);
            objectStateVar = objectState::connecting;
            redraw();
        }
        else if (objectStateVar == objectState::newAccount) {
            theSession->authenticateClientEmail(newEmailBox.text, newPasswordBox.text, newUsernameBox.text, 1, onSuccess, onError);
            objectStateVar = objectState::newAccountAttempt;
            redraw();
        }
    }
    void authenticateWithRefresh(std::string authT, std::string refreshT) {
        auto onSuccess = [this](std::string auth, std::string refresh) {
            cout << "connected!" << endl;
            authToken = auth;
            refreshToken = refresh;            
            objectStateVar = objectState::connected;
            writeFile();
            redraw();
            };

        auto onError = [this](std::string error) {
            errorMessage = error;
            objectStateVar = objectState::disconnected;
            redraw();
            };


        objectStateVar = objectState::connecting;

        theSession->refreshTheSession(authT, refreshT, onSuccess, onError);

    }
    #pragma endregion
public:
    MIN_DESCRIPTION{ "Display a text label" };
    MIN_TAGS{ "ui" }; // if multitouch tag is defined then multitouch is supported but mousedragdelta is not supported
    MIN_AUTHOR{ "Cycling '74" };
    MIN_RELATED{ "comment, umenu, textbutton" };

    inlet<>  input{ this, "(list) signout" };
    outlet<> output{ this, "(anything) bang on successful auth" };

    attribute<bool>  local{ this, "local", 0 };

    #pragma region signInConstructor
    signin(const atoms& args = {})
        : custom_ui_operator::custom_ui_operator{ this, args } {

               
        //Store the session pointer in the t_symbol
        SessionSym->s_thing = (c74::max::t_object*)theSession;
        
        //make a singleton        
        makeSingleton();       
        
        
        //Cannot access attributes in the constructor, so launch a thread that will run as soon as the constructor finishes to complete remaining initialization.

        if (!dummy()) {
            readDataFile();

            postConstructionInit = std::thread(&signin::postConstructionInitialization, this);
            postConstructionInit.detach();
            constructionComplete = true;
        }
    }
    #pragma endregion
   


    ~signin() {
        activeTextBoxPtr = NULL;
        if (blinkCursor.joinable()) {
            {
                std::unique_lock <std::mutex> lk(cv_m);  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
                cv.notify_one();  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
            }
            blinkCursor.join();
        }                
        
        //if (rememberMeCheck) { writeFile(); }

        //Clear Exists->s_thing when deleting the object or closing the patch.
        c74::max::t_symbol* Exists = c74::max::gensym("AuthenticatorInstance");
        Exists->s_thing = NULL;
    }

    
    
    message<> mousedown{ this, "mousedown",
        MIN_FUNCTION {
            event   e { args };
            auto    t { e.target() };
            auto    x { e.x() };
            auto    y { e.y() };

            //Detect username password box
            if (objectStateVar == objectState::disconnected) {
               
                //USERNAME BOX Box ranges are determined programatically in the Paint Function based on the paint context
                if (x >= usernameBox.x && x <= (usernameBox.x + *fontwidth) && y >= (usernameBox.y - *fontheight) && y <= (usernameBox.y)) {
                    activeTextBoxPtr = &usernameBox;
                    if (!blinkCursor.joinable()) { blinkCursor = std::thread(&signin::toggleCursor, this); }
                    redraw();
                }

                //PASSWORD BOX Box ranges are determined programatically
                else if (x >= passwordBox.x && x <= (passwordBox.x + *fontwidth) && y >= (passwordBox.y - *fontheight) && y <= (passwordBox.y)) {
                    activeTextBoxPtr = &passwordBox;
                    if (!blinkCursor.joinable()) { blinkCursor = std::thread(&signin::toggleCursor, this); }
                    redraw();
                }     

                //REMEMBERME
                else if (x >= (usernameBox.x - 3) && x <= (usernameBox.x - 3 + 15) && y >= 235 && y <= (235 + 15)) {
                    rememberMeCheck = !rememberMeCheck;
                    redraw();
                }
               

            }
            //Detect newUsername, newEmail, newPasswordBox
            else if (objectStateVar == objectState::newAccount) { 
                
                //NEWUSERNAME BOX
                if (x >= newUsernameBox.x && x <= (newUsernameBox.x + *fontwidth) && y >= (newUsernameBox.y - *fontheight) && y <= (newUsernameBox.y)) {
                    
                    activeTextBoxPtr = &newUsernameBox;
                    if (!blinkCursor.joinable()) { blinkCursor = std::thread(&signin::toggleCursor, this); }
                    redraw();
                }

                //NEWEMAIL BOX
                else if (x >= newEmailBox.x && x <= (newEmailBox.x + *fontwidth) && y >= (newEmailBox.y - *fontheight) && y <= (newEmailBox.y)) {
                    

                    activeTextBoxPtr = &newEmailBox;
                    if (!blinkCursor.joinable()) { blinkCursor = std::thread(&signin::toggleCursor, this); }
                    redraw();
                }

                //NEWPASSWORD BOX
                else if (x >= newPasswordBox.x && x <= (newPasswordBox.x + *fontwidth) && y >= (newPasswordBox.y - *fontheight) && y <= (newPasswordBox.y)) {
                    
                    activeTextBoxPtr = &newPasswordBox;
                    if (!blinkCursor.joinable()) { blinkCursor = std::thread(&signin::toggleCursor, this); }
                    redraw();
                }

                //REMEMBERME
                else if (x >= (usernameBox.x - 3) && x <= (usernameBox.x - 3 + 15) && y >= 250 && y <= (250 + 15)) {
                    rememberMeCheck = !rememberMeCheck;
                    redraw();
                }
            }

            //If you clicked outside either box, ensure both boxes are deactivated.
            else {
                    activeTextBoxPtr = NULL;
                    stopCursorThread();
                    redraw();
                }

            //NEW ACCOUNT BOX
            if (x >= (8) && x <= (8 + 15) && y >= t.height() - 22 && y <= (t.height() - 22 + 15) && (objectStateVar == objectState::newAccount || objectStateVar == objectState::disconnected)) {
                newAccountCheck = !newAccountCheck;
                if (newAccountCheck) { objectStateVar = objectState::newAccount; }
                else { objectStateVar = objectState::disconnected; }

                activeTextBoxPtr = NULL;
                stopCursorThread();
                redraw();
            }

            //CONNECT BOX Connectbox range is hard coded //position{ 158 , 275 }, //size{ 150.0, 50.0 }                      
            if (x >= 158 && x <= (158 + 150) && y >= 275 && y <= 275 + 50 && (objectStateVar == objectState::newAccount || objectStateVar == objectState::disconnected)) {
                cout << "you clicked the Connect box" << endl;
                activeTextBoxPtr = NULL;
                stopCursorThread();

                authenticateWithEmail();
             
                errorMessage = ""; //clear the errorMessage when connecting.
            }


            return {};
        }
    };

    message<> focusgained{ this, "focusgained",
        MIN_FUNCTION {
             return {};
         }

    };

    message<> focuslost{ this, "focuslost",
     MIN_FUNCTION {
       
        //clear active textBox.
        activeTextBoxPtr = NULL;
    
    return {};
}
    };

    message<> key{ this, "key", MIN_FUNCTION {
        int character = int(args[4]);
              
        if (activeTextBoxPtr) {
          #ifdef TARGET_OS_MAC
                if (character == 127 && !activeTextBoxPtr->text.empty()) { activeTextBoxPtr->text.pop_back(); } //popback on backspace
                else if (character == 127 && activeTextBoxPtr->text.empty()) { }  //do nothing on backspace if box is already empty
         #elif _WIN64
                if (character == 8 && !activeTextBoxPtr->text.empty()) { activeTextBoxPtr->text.pop_back(); } //popback on backspace
                else if (character == 8 && activeTextBoxPtr->text.empty()) { }  //do nothing on backspace if box is already empty
          #endif
                else if (character == 9) { // Switch active box on tab //
                    if (objectStateVar == objectState::newAccount) {
                        activeTextBoxPtr = getNextTextBox(activeTextBoxPtr, newAccountTextBoxPtrs);
                   }
                    else {
                        activeTextBoxPtr = getNextTextBox(activeTextBoxPtr, signInTextBoxPtrs);
                    }
                }                
                else if (character >= 33 && character <= 126 && activeTextBoxPtr->text.length() < 32) { activeTextBoxPtr->text += character; }  //else if it is an allowed character then add it to the string  (see decimal values for allowed characters: https://web.alfredstate.edu/faculty/weimandn/miscellaneous/ascii/ascii_index.html)
        }
        redraw();
        return {1};

    }
    };

    message<> signout{ this, "signout",
         MIN_FUNCTION {
            

            if (objectStateVar == objectState::connected) { 
                theSession->signOut();
                
                objectStateVar = objectState::disconnected;
                redraw();
            }
            else { cerr << "Cannot sign out if not connected." << endl; }
            
            return {};
         }
    };

    //PAINT FUNCTIONS
    #pragma region PaintFunctions
    void outputObjectStateChange() {
        if (objectStateVar != stateOnPreviousPaint) {
            string stateString;
            switch (objectStateVar) {
                case objectState::disconnected: stateString = "disconnected"; break;
                case objectState::connecting: stateString = "connecting"; break;
                case objectState::connected: stateString = "connected"; break;
                case objectState::newAccount: stateString = "newAccount"; break;
                case objectState::newAccountAttempt : stateString = "newAccountAttempt";  break;
            }
            
            output.send(stateString);
            stateOnPreviousPaint = objectStateVar;

        }
    }

    double centerText(target t, string textToCenter, double fontsize) {
        c74::max::jgraphics_set_font_size(t, fontsize);
        c74::max::jgraphics_text_measure(t, textToCenter.c_str(), fontwidth, fontheight);

        return (t.width() / 2) - (*fontwidth / 2);
    }

    //Paint textboxes that behave according to objectState.
    void paintTextBox(target t, textBox textBox, number m_fontsize, symbol m_fontname) {
        std::string textstring = textBox.text;

        if (textBox.masked) {
            size_t length = textBox.text.length();
            std::string maskedString(length, '*');
            textstring = maskedString;
        }

        if (objectStateVar == objectState::disconnected || objectStateVar == objectState::newAccount) {
            rect<stroke> {
                t,
                    color{ {0.3, 0.3, 0.3, 1.0} },
                    position{ (textBox.x - textBuffer - 5), (textBox.y - m_fontsize - 5) }, //the fontsize adds additional space above the font to give clearance for the line above.  The fontheight coming back from jgraphics_text_measure adds even more.  Through eyeballing and trial and error,  25% of the the fontheight seems to be about the amount that gets added above the top of the character.  We add this to the bottom of the rect to get the text vertically centerred.
                    size{ (*fontwidth + (2 * textBuffer + 10)), (m_fontsize + textBuffer + 10) },
                    line_width{ 3.0 },
                    corner{ 15, 15 }
            };

            text{			// username display
                  t, color {color::predefined::black},
                  position {textBox.x, textBox.y},
                  fontface {m_fontname},
                  fontsize {m_fontsize},
                  content {textstring}
            };
        }

        if (objectStateVar == objectState::connected || objectStateVar == objectState::connecting || objectStateVar == objectState::newAccountAttempt) {

            rect<fill> {
                t,
                    color{ {0.3, 0.3, 0.3, 1.0} },
                    position{ (textBox.x - textBuffer - 5), (textBox.y - m_fontsize - 5) }, //the fontsize adds additional space above the font to give clearance for the line above.  The fontheight coming back from jgraphics_text_measure adds even more.  Through eyeballing and trial and error,  25% of the the fontheight seems to be about the amount that gets added above the top of the character.  We add this to the bottom of the rect to get the text vertically centerred.
                    size{ (*fontwidth + (2 * textBuffer + 10)), (m_fontsize + textBuffer + 10) },
                    line_width{ 3.0 },
                    corner{ 15, 15 }
            };

            if (textBox.masked == false) {  //if text is masked don't paint it when box is unavailable
                text{			// username display
                  t, color {color::predefined::white},
                  position {textBox.x, textBox.y},
                  fontface {m_fontname},
                  fontsize {m_fontsize},
                  content {textstring}
                };
            }
        }

        text{			// labelText
                t, color {color::predefined::black},
                position {textBox.x - textBuffer - 5, textBox.y - m_fontsize - 10},
                fontface {m_fontname},
                fontsize {m_fontsize},
                content {textBox.labelText}
        };


        return;
    }

    void paintRememberMeBox(target t, number m_fontsize) {
    if (objectStateVar == objectState::newAccount || objectStateVar == objectState::newAccountAttempt) {
        rect<> {  //a rect for the password
            t,
                color{ {0.3, 0.3, 0.3, 1.0} },
                position{ (newUsernameBox.x - 3), 250 },
                size{ 15, 15 },
                line_width{ 3.0 }
                // corner{ 15, 15 }
        };

        text{			// labelText
                t, color {color::predefined::black},
                position { newUsernameBox.x - 3 + 19 , 250 + 12},
                fontface {m_fontname},
                fontsize {m_fontsize},
                content {"Remember me"}
        };
    }
    else {
        rect<> {  //a rect for the password
            t,
                color{ {0.3, 0.3, 0.3, 1.0} },
                position{ (usernameBox.x - 3), 235 },
                size{ 15, 15 },
                line_width{ 3.0 }
                // corner{ 15, 15 }
        };

        text{			// labelText
                t, color {color::predefined::black},
                position { usernameBox.x - 3 + 19 , 247},
                fontface {m_fontname},
                fontsize {m_fontsize},
                content {"Remember me"}
        };
    }

    }

    void paintRememberMeCheck(target t) {
        if (objectStateVar == objectState::newAccount || objectStateVar == objectState::newAccountAttempt) {
            line<> {
                t,
                    color{ color::predefined::black },
                    line_width{ 2.0 },
                    origin{ usernameBox.x - 3 + 3, 250 + 7 },   //additional +3 on originx and -3 destinationy to avoid the linewidth of the box
                    destination{ usernameBox.x - 3 + 7, 250 + 15 - 3 }
            };
            line<> {
                t,
                    color{ color::predefined::black },
                    line_width{ 2.0 },
                    origin{ usernameBox.x - 3 + 7, 250 + 15 - 3 },
                    destination{ usernameBox.x - 3 + 15 - 3, 250 + 3 }
            };
        }
        else {
            line<> {
                t,
                    color{ color::predefined::black },
                    line_width{ 2.0 },
                    origin{ usernameBox.x - 3 + 3, 235 + 7 },   //additional +3 on originx and -3 destinationy to avoid the linewidth of the box
                    destination{ usernameBox.x - 3 + 7, 235 + 15 - 3 }
            };
            line<> {
                t,
                    color{ color::predefined::black },
                    line_width{ 2.0 },
                    origin{ usernameBox.x - 3 + 7, 235 + 15 - 3 },
                    destination{ usernameBox.x - 3 + 15 - 3, 235 + 3 }
            };
        }
    }

    void paintNewAccountBox(target t, number m_fontsize) {
        int objectHeight = t.height();
        rect<> {  //a rect for the password
            t,
                color{ {0.3, 0.3, 0.3, 1.0} },
                position{ 8, objectHeight - 22},
                size{ 15, 15 },
                line_width{ 3.0 }
                // corner{ 15, 15 }
        };

        text{			// labelText
                t, color {color::predefined::black},
                position { 8 + 19 , objectHeight - 22 + 12},
                fontface {m_fontname},
                fontsize {m_fontsize},
                content {"New Account"}
        };

    }

    void paintNewAccountBoxCheck(target t) {
        int objectHeight = t.height();

        line<> {
            t,
                color{ color::predefined::black },
                line_width{ 2.0 },
                origin{ 8 + 3, objectHeight - 22 + 7 },   //additional +3 on originx and -3 destinationy to avoid the linewidth of the box
                destination{ 8 + 7, objectHeight - 22 + 15 - 3 }
        };
        line<> {
            t,
                color{ color::predefined::black },
                line_width{ 2.0 },
                origin{ 8 + 7, objectHeight - 22 + 15 - 3 },
                destination{ 8 + 15 - 3, objectHeight - 22 + 3 }
        };
    }

    //Paint connectBoxes that behave according to objectState.
    void paintConnectButton(target t, number m_fontsize) {
        if (objectStateVar == objectState::disconnected || objectStateVar == objectState::newAccount) {

            rect<fill> {  //a rect for the signin
                t,
                    color{ {0.3, 0.3, 0.3, 1.0} },
                    position{ 158 , 268 },
                    size{ 150.0, 50.0 },
                    line_width{ 3.0 },
                    corner{ 15, 15 }
            };

            int connectCenter = centerText(t, "Connect", m_fontsize);
            text{			// ShowTitle
                     t, color {color::predefined::black},
                     position {connectCenter, 268 + 32},
                     fontface {m_fontname},
                     fontsize {m_fontsize},
                     content {"Connect"}
            };
        }

        if (objectStateVar == objectState::connected) {

            rect<stroke> {  //a rect for the signin
                t,
                    color{ {0.3, 0.3, 0.3, 1.0} },
                    position{ 158 , 268 },
                    size{ 150.0, 50.0 },
                    line_width{ 3.0 },
                    corner{ 15, 15 }
            };

            int connectCenter = centerText(t, "Connected", m_fontsize);
            text{			// ShowTitle
                     t, color {color::predefined::black},
                     position {connectCenter, 268 + 32},
                     fontface {m_fontname},
                     fontsize {m_fontsize},
                     content {"Connected"}
            };

            

        }

        if (objectStateVar == objectState::connecting || objectStateVar == objectState::newAccountAttempt) {

            rect<stroke> {  //a rect for the signin
                t,
                    color{ {0.3, 0.3, 0.3, 1.0} },
                    position{ 158 , 268 },
                    size{ 150.0, 50.0 },
                    line_width{ 3.0 },
                    corner{ 15, 15 }
            };

            int connectCenter = centerText(t, "Connecting...", m_fontsize);
            text{			// ShowTitle
                     t, color {color::predefined::black},
                     position {connectCenter, 268 + 32},
                     fontface {m_fontname},
                     fontsize {m_fontsize},
                     content {"Connecting..."}
            };
        }
    }

    void paintErrorMessage(target t, number m_fontsize) {
        if (objectStateVar == objectState::disconnected || objectStateVar == objectState::newAccount) {

            int errorCenter = centerText(t, errorMessage, m_fontsize);
            text{			// ShowTitle
                     t, color { {0.847059, 0., 0., 1.} },
                     position {errorCenter, 268 + 32 + 35},
                     fontface {m_fontname},
                     fontsize {m_fontsize},
                     content {errorMessage}
            };
        }
    }

    void paintCursor(target t) {
        if (activeTextBoxPtr) {
            
            if (activeTextBoxPtr->masked) { 
                size_t length = activeTextBoxPtr->text.length();
                std::string maskedString(length, '*');
                c74::max::jgraphics_text_measure(t, maskedString.c_str(), fontwidth, fontheight); }
            else { c74::max::jgraphics_text_measure(t, activeTextBoxPtr->text.c_str(), fontwidth, fontheight); }
            line<> {
                t,
                    color{ color::predefined::black },
                    line_width{ 1.0 },
                    origin{ number((activeTextBoxPtr->x + *fontwidth + 1)), number(activeTextBoxPtr->y + 2) },   //additional +3 on originx and -3 destinationy to avoid the linewidth of the box
                    destination{ number(activeTextBoxPtr->x + *fontwidth + 1), number(activeTextBoxPtr->y + 2 - *fontheight) }
            };
        }
    };       
    
#pragma endregion

    message<> paint{ this, "paint",
        MIN_FUNCTION {
            target t        { args };
            
            //Output Message if Object State has Changed.
            outputObjectStateChange();

    
            // ShowTitle
            text{			
                     t, color {color::predefined::black},
                     position {35, 85},
                     fontface {m_fontname},
                     fontsize {36},
                     content {"Interactive Scores Online"}
            };

            

            //Place the TextBoxes
            usernameBox.x = centerText(t, placeholderString, m_fontsize);
            usernameBox.y = (t.height() / 2) - (*fontheight * .25) - 5 - 10;   //raised in increments relative to the vercial center.  (the fontheight behaves stragely, this formula is mostly trial and error.)

            passwordBox.x = centerText(t, placeholderString, m_fontsize);
            passwordBox.y = (t.height() / 2) + *fontheight + 5 + (*fontheight * .25) + m_fontsize;  //put the box in the vertical center

            newUsernameBox.x = centerText(t, placeholderString, m_fontsize);
            newUsernameBox.y = (t.height() / 2) + *fontheight + 5 + (*fontheight * .25) + m_fontsize + 15 - 50 - 50;    //adjusting in increments from the center according to trial and erryr.

            newEmailBox.x = centerText(t, placeholderString, m_fontsize);
            newEmailBox.y = (t.height() / 2) + *fontheight + 5 + (*fontheight * .25) + m_fontsize + 15 - 50;

            newPasswordBox.x = centerText(t, placeholderString, m_fontsize);
            newPasswordBox.y = (t.height() / 2) + *fontheight + 5 + (*fontheight * .25) + m_fontsize + 15;

            //Paint Elements according to the Object State
            if (objectStateVar == objectState::newAccount || objectStateVar == objectState::newAccountAttempt) {
                paintTextBox(t, newUsernameBox, m_fontsize, m_fontname);
                paintTextBox(t, newEmailBox, m_fontsize, m_fontname);
                paintTextBox(t, newPasswordBox, m_fontsize, m_fontname);
            } 
            else {
                paintTextBox(t, usernameBox, m_fontsize, m_fontname);
                paintTextBox(t, passwordBox, m_fontsize, m_fontname);
            }
            
            paintRememberMeBox(t, m_fontsize);
            if (rememberMeCheck) { paintRememberMeCheck(t); }
            
            if (activeTextBoxPtr && showCursor) { paintCursor(t); }

            paintConnectButton(t, 24);

            if (errorMessage != "") { paintErrorMessage(t, 16); }

            paintNewAccountBox(t, m_fontsize);
            if (newAccountCheck) { paintNewAccountBoxCheck(t); };



//Reset width and height pointers
c74::max::jgraphics_text_measure(t, placeholderString.c_str(), fontwidth, fontheight);


return {};

}

    };

};

MIN_EXTERNAL(signin);
