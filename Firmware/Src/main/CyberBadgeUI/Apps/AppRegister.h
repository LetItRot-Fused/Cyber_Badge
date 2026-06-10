/**
 * @file AppRegister.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-03-15
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once
#include "AppTypedef.h"


/**
 *  ------------------------------------------- How to Add an App -------------------------------------------
 * 
 *  1. Copy the App_Template folder, rename it like App_MyApp, and paste it next to App_Template.
 *      into App_MyApp folder, rename the cpp and h file, e.g. App_MyApp.cpp
 *      into App_MyApp.cpp and  App_MyApp.h, change all the "Template" to "MyApp"
 * 
 *  2. Include your App's header file in 2)
 *      e.g. #include "App_MyApp/App_MyApp.h"
 * 
 *  3. Log your App into AppRegister in 3)
 *      e.g. App_Login(MyApp),
 * 
 * ----------------------------------------------------------------------------------------------------------
 */


/**
 * @brief 2) Include your App's header file
 * 
 */

#include "App_2048/App_2048.h"
#include "APP_settings/App_Settings.h"
#include "App_ShowGIF/App_ShowGIF.h"
#include "App_Gongde/App_Gongde.h"
#include "App_Transfer/App_Transfer.h"
/* Header files locator */
/* Don't remove this, or python script's auto login will be failed */


namespace App {
    
    static AppRegister_t Register[] = {
        

        /**
         * @brief 3) Log your App into AppRegister here
         * 
         */
        
        App_Login(ShowGIF),
        App_Login(Transfer),
        App_Login(Gongde),
 		App_Login(2048),
        App_Login(Settings),

		/* Login locator */
        /* Don't remove this, or python script's auto login will be failed */


    };
}
