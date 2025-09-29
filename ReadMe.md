# ISOr3
This package was created using the Min-DevKit for Max, an API and supporting tools for writing externals in modern C++.



## Prerequisites

The Package builds on Xcode 9 or Later or Visual Studio 2017 or later:

* On Mac this means **Xcode 9 or later** (you can get from the App Store for free).
* On Mac you will also need to install homebrew, and then use hombrew to install cmake.
* On Mac you may also need to open Xcode.  Go to Xcode->Settings->Locaton.  Find the Command Line tools and specifically set the Xcode version.  This will allow cmake to find Clang for your Xcode Version.
* On Windows this means **Visual Studio 2017** (you can download a free version from Microsoft). The installer for Visual Studio 2017 offers an option to install Git, which you should choose to do.

Building the package requires Cmake:

* On Mac use homebrew to install Cmake.
    * Go to `https://brew.sh` to get homebrew
    * Follow the directions at the end of the install script to add homebrew to your PATH
    * Use `brew install cmake` to install cmake
    * After installing cmake you may need to resent the Xcode configuration with `sudo xcode-select --reset`
Follow the instructions at the end of the homebrew install to add brew to your PATH. 
* on Windows install binaires and add Cmake to path (TODO: Confirm)


## To Build
### Acquire Libraries:
* Clone this the repo to your Max Packages folder or a folder that is attached to the Max Packages folder by symbolic link.  Make sure you clone recursively so that all submodules are properly initiated.  Use your Compiler's clone command or in the terminal, from the Packages folder use `git clone https://github.com/jkaplan55/ISOr3.git --recursive ISOr3`
* Download additional libraries appropriate to your platform from [Google Drive](https://drive.google.com/drive/folders/1IaKlRyWFBS9coNOtA-0CmdqwK-d28z0e?usp=sharing) and extract into the folder where you cloned the repo.
* On Windows: Copy both .dll's in the "shared libs" into your max folder (where max.exe lives).  The release .dll must also be included with any standalones that use the externals.
* In the Terminal or Console app of your choice, change directories (cd) into the folder you cloned the repo, then Download additional submodules: `git submodule update --init --recursive`

### Generate Project Files:
* In folder you cloned the repo, `mkdir build` to create a folder with your various build files
* `cd build` to put yourself into that folder
Build Project Files:
* on Mac: `cmake -G Xcode ..` 
* on Windows: `cmake -G "Visual Studio 17 2022" ..`

### Configure Project Files
#### Xcode [TODO add Nakama Release 2.8.5 to includes package and document]
For each Max Object Target (ignore ALL BUILD, LIB, and RUN_TESTS):  
* Build Settings -> Architectures:  remove `x86_64` 
* Build Settings -> Header Search Paths: add `"$(SRCROOT)/includes/NakamaMac/lib/nakama-sdk.framework`, `$(SRDROOT)/includes/cpr/include`, and `$(SRCROOT)/includes`
* Build Settings -> Runpath Search Paths: add `$(SRCROOT)/includes/NakamaMac/lib`
* Build Settings -> Library Search Paths: add `$(SRCROOT)/includes/NakamaMac/lib`
* General -> Frameworks and Libraries: add `libnakama-sdk.dylib` from `../includes/NakamaMac/lib`
* You may need to update the search paths of some of the Max Libraries do to bad quoting around paths with spaces.
* Set MacOS Deployment Target to 10.15.  Only Seqouia and later is supported.
* Set the Build Target in the bar at the top to All Build
(An xcconfig file can be found int he settings folder off the root.  But it does work correclty in the current version)

#### Visual Studio
View->Other Windows->Property Manager
For each Max Object Target (ignore ALL BUILD, LIB, and RUN_TESTS):
  * Select `Debug | x64` and `RelWithDebInfo | x64` configurations.  Right click and choose "Add Existing Property Sheet".  Navigate to `../project settings`  and select `ISO Project Debugx64`.
  * Select `Release | x64` and `MinSizeRel | x64` configurations.  Right click and choose "Add Existing Property Sheet".  Navigate to `../project settings`  and select `ISO Project Releasex64`.
  * Select all configurations: Right click and choose Properties. Then Linker->Input-> Additional Dependencies-> edit ->check "Inherit from parent or project defaults"-> OK-> Apply
 
 
 OR
 
 Configure Properites directly for each Max Object Target (ignore ALL BUILD, LIB, and RUN_TESTS):
  * C++ -> General -> Additional Include Directories: add `$(SolutionDir)..\includes\NakamaWindows\[Debug or Release]\include`, `$(SolutionDir)..\includes\cpr\include`, and `$(SolutionDir)..\includes`
  * Linker -> General -> add paths:  `$(SolutionDir)..\includes\NakamaWindows\[Debug or Release]\lib` and `$(SolutionDir)..\includes\cpr\lib`
  * Edit Linker-Input-Additional Dependencies:
add `nakama-sdk.lib`, `cpr.lib`, and `libcurl-d_imp.lib`
  * Edit Linker->Input-> Additiona Dependencies: check "Inherit from parent or project defaults"

Setup Debugging
   * Select all configurations: right click and choose properties.  Configuration Properties->Debugging->Command.  Enter the full path for Max.exe or a particular patcher you want to test with.
   * In Solution Explorer, Right click on the object you are testing and select "Set as Startup Project."
   * Now Max will launch when you run your program and you will get debug info from the selected starup project.
### Build
* Select ALL BUILD and build.
* The Max externals will be found in `..\externals`

### If you changed the names of the source files
If you changed the names of the source files then the code for the Max SDK Objects needs to be corrected.  In ext_main for all Max objects is a class_new() function.  The first argument needs to match the name of the file.
  
## Creating a new Repo
If creating a new repo from scratch remember the following:
* `package-info.json.in` is required for Min to build.
* `git init`
* Be sure to add the submodules to your repo even though they are included in .gitsubmodules.
* `git submodule add https://github.com/Cycling74/min-lib.git source/min-lib`
* `git submodule add https://github.com/Cycling74/min-api.git source/min-api`
* `git submodule update --init --recursive`
* `git add --all`
* `git commit -m "First Commit`
*  Test to make sure the solution builds.  It will not build if a commit doesn't exist.
* `git remote add origin [GithubRepoOrigin]`
* `git push -u origin main`




## Contributors / Acknowledgements

ISOr3 is the work of Joseph Kaplan, Attainable Solutions.
Special thanks to Chris Pasillas and Steph Grush for their peerless guidance and advise.


## Support

For support, please contact the developer of this package.
