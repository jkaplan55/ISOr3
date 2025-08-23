/// @file
///	@ingroup 	minexamples
///	@copyright	Copyright 2018 The Min-DevKit Authors. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.


// This Object paints a login screen for ISO.  If the RememberMe box is checked then UserName, RefreshToken and RememberMeCheck status are stored in a file called ISOdata.dat.
// The file is read and values are restored in the constructor.  If the box is checked, then the files is written after a successful auth.  It is also written in the destructor.


#include "c74_min.h"
#include "../custom_min_operator_ui.h"   //https://cycling74.com/forums/how-does-jboxtextfield-work
#include <string.h>
#include <thread>
#include <fstream>
#include "json.hpp"

using namespace c74::min;
using namespace c74::min::ui;

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





class login : public object<login>, public custom_ui_operator<465, 365, uiflags> {
private:
    bool	m_bang_on_change{ true };
    number	m_unclipped_value{ 0.0 };
    number  m_anchor{};
    string	m_text;
    bool	m_mouseover{};
    number  m_mouse_position[2]{};
    number	m_range_delta{ 1.0 };


    //DECLARATIONS FOR THE TEXT BOXES
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
        while ((usernameBoxActive || passwordBoxActive)) {
            showCursor = !showCursor;
            redraw();
            std::unique_lock<std::mutex> lk(cv_m);
            cv.wait_for(lk, std::chrono::milliseconds(500));
            //Sleep(500);
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
    textBox usernameBox{ 50, 50, "", 0, "ACCOUNT NAME" };
    textBox passwordBox{ 100, 100, "", 1, "PASSWORD" };

    //Bools to detect activate TextBoxes
    bool usernameBoxActive{ false };
    bool passwordBoxActive{ false };

    //DECLARATIONS FOR MANAGING STATE
   
    //Bool for RemembermeCheck
    bool rememberMeCheck{ false };

    //Bool for Connected
    enum class connectionState {
        disconnected,
        connecting,
        connected
    };

    connectionState connectionStateVar = connectionState::disconnected;

    string errorMessage;

    //DECLARATIONS FOR DATA PERSISTENCE

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
    string refreshToken = "";
      
    //Jsonify and DeJSONify DataScruct
    string jsonifyUserData() {
        nlohmann::json j = nlohmann::json{ {"userName", usernameBox.text}, {"refreshToken", "placeHolderRefreshToken"}, {"rememberMe", rememberMeCheck} };
        return j.dump();
    }
    
    void deJsonifyUserData(string dataString) {
        nlohmann::json j = nlohmann::json::parse(dataString);
        j.at("userName").get_to(usernameBox.text);
        j.at("refreshToken").get_to(refreshToken);
        j.at("rememberMe").get_to(rememberMeCheck);

        return;
    }

    //Read and Write the File
    void readDataFile() {
        std::ifstream file;
        file.open("ISOdata.dat");

        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();        
            file.close();
            string fileString = encrypt_decrypt(buffer.str(), *"Q");
            //cout << fileString.substr(56, fileString.length() - 181) << endl;
            deJsonifyUserData(fileString.substr(56, fileString.length() - 181));
        }

      
        //else {
        //    cout << "Error reading userData file" << endl;
       // }

       
      return;
    }
        
    void writeFile() {
        //string fileString = jsonifyUserData();       
        std::ofstream file;
        file.open("ISOdata.dat");
        if (file.is_open()) {
            file << gen_random(56) << encrypt_decrypt(jsonifyUserData(), *"Q") << gen_random(124) << std::endl;
            file.close();
        }
        else {
            cout << "Error reading userData file" << endl;
        }

        return;
    }



public:
    MIN_DESCRIPTION{ "Display a text label" };
    MIN_TAGS{ "ui" }; // if multitouch tag is defined then multitouch is supported but mousedragdelta is not supported
    MIN_AUTHOR{ "Cycling '74" };
    MIN_RELATED{ "comment, umenu, textbutton" };

    inlet<>  input{ this, "(number) value to set" };
    outlet<> output{ this, "(number) value" };



    login(const atoms& args = {})
        : custom_ui_operator::custom_ui_operator{ this, args } {

         readDataFile();

         cout << "username = " << usernameBox.text << endl;
         cout << "refreshToken = " << refreshToken << endl;
         cout << "rememberMe = " << rememberMeCheck << endl;
    }

    ~login() {
        usernameBoxActive = false;
        passwordBoxActive = false;
        if (blinkCursor.joinable()) {
            {
                std::unique_lock <std::mutex> lk(cv_m);  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
                cv.notify_one();  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
            }
            blinkCursor.join();
        }

        if (rememberMeCheck) { writeFile(); }  
    }

    // An attribute named "value" is treated as a special property of an object.
// This attribute will be seen by Max's preset, pattr, and parameter systems for saving and recalling object state.
// In a standalone we will need to store pattr settings as a file.  So instead of using pattr, we'll just read and write our own file with the username and refresh token.
    attribute<number>  m_default{ this, "defaultvalue", 0.0 };


    attribute<symbol> m_value{ this, "value", "" };





    //savestate message doesn't work with UI objects.  So we will store RememberMe details as values for pattr.

    attribute<number>  m_fontsize{ this, "fontsize", 14.0 };
    attribute<symbol>  m_fontname{ this, "fontname", "lato-light" };


    message<> mousedown{ this, "mousedown",
        MIN_FUNCTION {
            event   e { args };
            auto    t { e.target() };
            auto    x { e.x() };
            auto    y { e.y() };


           
            //If you clicked in the userName box. Box ranges are determined programatically
            if (x >= usernameBox.x && x <= (usernameBox.x + *fontwidth) && y >= (usernameBox.y - *fontheight) && y <= (usernameBox.y) && connectionStateVar == connectionState::disconnected) {
                cout << "YOU CLICKED IN THE LOGIN BOX!" << endl;
                usernameBoxActive = true;
                if (passwordBoxActive == true) { passwordBoxActive = false; }

                if (!blinkCursor.joinable()) { blinkCursor = std::thread(&Test_MaxUiObject::toggleCursor, this); }
            }
            
            //If you clicked in the password box.  Box ranges are determined programatically
            else if (x >= passwordBox.x && x <= (passwordBox.x + *fontwidth) && y >= (passwordBox.y - *fontheight) && y <= (passwordBox.y) && connectionStateVar == connectionState::disconnected) {
                cout << "YOU CLICKED IN THE PASSWORD BOX!" << endl;
                passwordBoxActive = true;
                if (usernameBoxActive == true) { usernameBoxActive = false; }

                if (!blinkCursor.joinable()) { blinkCursor = std::thread(&Test_MaxUiObject::toggleCursor, this); }
            }
            
            //If you clicked outside either box, ensure both boxes are deactivated.
            else {
                    if (usernameBoxActive == true || passwordBoxActive == true)
                    {
                    showCursor = false;
                    usernameBoxActive = false;
                    passwordBoxActive = false;
                    if (blinkCursor.joinable()) {
                        {
                            std::unique_lock <std::mutex> lk(cv_m);  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
                            cv.notify_one();  //manage the mutex and stop the wait before joining the thread, https://stackoverflow.com/questions/55783451/using-c-how-can-i-stop-a-sleep-thread
                        }
                        blinkCursor.join();
                    }
                    }
                }

            //If you clicked inside the RememberMeBox
            if (x >= (usernameBox.x - 3) && x <= (usernameBox.x - 3 + 15) && y >= 235 && y <= (235 + 15) && connectionStateVar == connectionState::disconnected) {
                    rememberMeCheck = !rememberMeCheck;
                }

            //If you clicked inside the ConnectBox.  Connectbox range is hard coded
            //position{ 158 , 275 },
            //size{ 150.0, 50.0 },
            if (x >= 158 && x <= (158 + 150) && y >= 275 && y <= 275 + 50 && connectionStateVar == connectionState::disconnected) {
                cout << "you clicked the Connect box" << endl;
                connectionStateVar = connectionState::connected;
                if (rememberMeCheck) { writeFile(); };
                errorMessage = ""; //clear the errorMessage when connecting.
            }
            


            cout << "usernameBoxActive = " << usernameBoxActive << endl;
            cout << "passwordBoxActive = " << passwordBoxActive << endl;
            redraw();
            return {};
        }
    };

    message<> focusgained{ this, "focusgained",
        MIN_FUNCTION {
             cout << "focusgained" << endl;
             return {};
         }

    };

    message<> focuslost{ this, "focuslost",
     MIN_FUNCTION {
         cout << "focuslost" << endl;

    //deactivate both boxes
    usernameBoxActive = false;
    passwordBoxActive = false;
    return {};
}
    };

    message<> key{ this, "key", MIN_FUNCTION {
        char character = int(args[4]);
        if (usernameBoxActive == true) {
            if (character == 8 && !usernameBox.text.empty()) { usernameBox.text.pop_back(); }  //popback on backspace
            else if (character == 9) { // Switch active box on tab
                usernameBoxActive = false;
                passwordBoxActive = true;
                cout << "tab!" << endl;
            }
            else {
                usernameBox.text += int(args[4]);  //add the input to the strong                
            }
        }
        else if (passwordBoxActive == true) {
            if (character == 8 && !passwordBox.text.empty()) { passwordBox.text.pop_back(); } //popback on backspace
            else if (character == 9) { // Switch active box on tab //
                usernameBoxActive = true;
                passwordBoxActive = false;
                cout << "tab!" << endl;
            }
            else { passwordBox.text += int(args[4]); }
        }
        redraw();
        return {1};

    }
    };

    message<> signout{ this, "signout",
     MIN_FUNCTION {

        connectionStateVar = connectionState::disconnected;
        redraw();
        return {};
}
    };

    //Paint Functions
    double centerText(target t, string textToCenter, double fontsize) {
        c74::max::jgraphics_set_font_size(t, fontsize);
        c74::max::jgraphics_text_measure(t, textToCenter.c_str(), fontwidth, fontheight);

        return (t.width() / 2) - (*fontwidth / 2);
    }

    void paintTextRect(target t, textBox textBox, number m_fontsize, symbol m_fontname) {
        std::string textstring = textBox.text;

        if (textBox.masked) {
            size_t length = textBox.text.length();
            std::string maskedString(length, '*');
            textstring = maskedString;
        }


        

        if (connectionStateVar == connectionState::disconnected) {
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

        if (connectionStateVar == connectionState::connected) {
            
            rect<fill> {
                t,
                    color{ {0.3, 0.3, 0.3, 1.0} },
                    position{ (textBox.x - textBuffer - 5), (textBox.y - m_fontsize - 5) }, //the fontsize adds additional space above the font to give clearance for the line above.  The fontheight coming back from jgraphics_text_measure adds even more.  Through eyeballing and trial and error,  25% of the the fontheight seems to be about the amount that gets added above the top of the character.  We add this to the bottom of the rect to get the text vertically centerred.
                    size{ (*fontwidth + (2 * textBuffer + 10)), (m_fontsize + textBuffer + 10) },
                    line_width{ 3.0 },
                    corner{ 15, 15 }
            };

            if (textBox.masked == false) {
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

    void paintRememberMeCheck(target t) {
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

    void paintConnectButton(target t, number m_fontsize) {
       if (connectionStateVar == connectionState::disconnected) {
        
        rect<fill> {  //a rect for the login
            t,
                color{ {0.3, 0.3, 0.3, 1.0} },
                position{ 158 , 275 },
                size{ 150.0, 50.0 },
                line_width{ 3.0 },
                corner{ 15, 15 }
        };

        int connectCenter = centerText(t, "Connect", m_fontsize);
        text{			// ShowTitle
                 t, color {color::predefined::black},
                 position {connectCenter, 275 + 32},
                 fontface {m_fontname},
                 fontsize {m_fontsize},
                 content {"Connect"}
        };
       }

       if (connectionStateVar == connectionState::connected) {

           rect<stroke> {  //a rect for the login
               t,
                   color{ {0.3, 0.3, 0.3, 1.0} },
                   position{ 158 , 275 },
                   size{ 150.0, 50.0 },
                   line_width{ 3.0 },
                   corner{ 15, 15 }
           };

           int connectCenter = centerText(t, "Connected", m_fontsize);
           text{			// ShowTitle
                    t, color {color::predefined::black},
                    position {connectCenter, 275 + 32},
                    fontface {m_fontname},
                    fontsize {m_fontsize},
                    content {"Connected"}
           };

           if (connectionStateVar == connectionState::connecting) {

               rect<stroke> {  //a rect for the login
                   t,
                       color{ {0.3, 0.3, 0.3, 1.0} },
                       position{ 158 , 275 },
                       size{ 150.0, 50.0 },
                       line_width{ 3.0 },
                       corner{ 15, 15 }
               };

               int connectCenter = centerText(t, "Connecting...", m_fontsize);
               text{			// ShowTitle
                        t, color {color::predefined::black},
                        position {connectCenter, 275 + 32},
                        fontface {m_fontname},
                        fontsize {m_fontsize},
                        content {"Connecting..."}
               };
           }
       }
    }

    void paintErrorMessage(target t, number m_fontsize) {
        if (connectionStateVar == connectionState::disconnected) {

            int errorCenter = centerText(t, errorMessage, m_fontsize);
            text{			// ShowTitle
                     t, color { {0.847059, 0., 0., 1.} },
                     position {errorCenter, 275 + 32 + 43},
                     fontface {m_fontname},
                     fontsize {m_fontsize},
                     content {errorMessage}
            };
        }
    }

    void paintCursor(target t) {
        if (usernameBoxActive) {
            c74::max::jgraphics_text_measure(t, usernameBox.text.c_str(), fontwidth, fontheight);


            line<> {
                t,
                    color{ color::predefined::black },
                    line_width{ 1.0 },
                    origin{ number((usernameBox.x + *fontwidth + 1)), number(usernameBox.y + 2) },   //additional +3 on originx and -3 destinationy to avoid the linewidth of the box
                    destination{ number(usernameBox.x + *fontwidth + 1), number(usernameBox.y + 2 - *fontheight) }
            };
        }
        if (passwordBoxActive) {
            size_t length = passwordBox.text.length();
            std::string maskedString(length, '*');
            c74::max::jgraphics_text_measure(t, maskedString.c_str(), fontwidth, fontheight);


            line<> {
                t,
                    color{ color::predefined::black },
                    line_width{ 1.0 },
                    origin{ number((passwordBox.x + *fontwidth + 1)), number(passwordBox.y + 2) },   //additional +3 on originx and -3 destinationy to avoid the linewidth of the box
                    destination{ number(passwordBox.x + *fontwidth + 1), number(passwordBox.y + 2 - *fontheight) }
            };
        }
    }

    bool firstPaint{ true };  //Load m_value on first paint only

    message<> paint{ this, "paint",
        MIN_FUNCTION {
            target t        { args };

            if (firstPaint) {

                string mystring = usernameBox.text;
                usernameBox.text = mystring;
                firstPaint = false;
            } //Load saved userName on first paint only

            usernameBox.x = centerText(t, placeholderString, m_fontsize);
            usernameBox.y = (t.height() / 2) - (*fontheight * .25) - 5 - 10;

            passwordBox.x = centerText(t, placeholderString, m_fontsize);
            passwordBox.y = (t.height() / 2) + *fontheight + 5 + (*fontheight * .25) + m_fontsize;

            paintTextRect(t, usernameBox, m_fontsize, m_fontname);
            paintTextRect(t, passwordBox, m_fontsize, m_fontname);

            if ((usernameBoxActive || passwordBoxActive) && showCursor) { paintCursor(t); }


            text{			// ShowTitle
                     t, color {color::predefined::black},
                     position {35, 85},
                     fontface {m_fontname},
                     fontsize {36},
                     content {"Interactive Scores Online"}
            };

            paintRememberMeBox(t, m_fontsize);
            if (rememberMeCheck) { paintRememberMeCheck(t); }
            
            paintConnectButton(t, 24);

            if (errorMessage != "") { paintErrorMessage(t, 16); }
            
            
            //Reset width and height pointers
            c74::max::jgraphics_text_measure(t, placeholderString.c_str(), fontwidth, fontheight);


            return {};
        }
    };



};


MIN_EXTERNAL(login);