#pragma once

#include "..\const\constant.h"
#include "..\beans\structures.h"
#include "..\io\dirutil.h"


#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>

class CfgLoader{
    public:
        CfgLoader(){
            loadMainConfig();
        }
        ~CfgLoader(){}

		ConfigMain configMain;
        std::vector<ConfigEmu> configEmus;
        
        int getWidth(){
            return configMain.resolution[0] < 0 ? 1280 : configMain.resolution[0];
        }

        int getHeight(){
            return configMain.resolution[1] < 0 ? 720 : configMain.resolution[1];
        }

        void setWidth(int w){
            configMain.resolution[0] = w;
        }

        void setHeight(int h){
            configMain.resolution[1] = h;
        }

        bool isDebug(){
            return configMain.debug;
        }
    private:
        
        static const std::string CONFIGFILE;
        /**
         * 
         */
        void loadMainConfig(){
            dirutil dir;
            std::string filepath = Constant::getAppDir() + std::string(Constant::tempFileSep) + CONFIGFILE;

            if (!dir.fileExists(filepath.c_str())){
                LOG_ERROR("Main config file not found: %s", filepath.c_str());
                std::string upperCfgFile = CONFIGFILE;
                Constant::upperCase(&upperCfgFile);
                filepath = Constant::getAppDir() + std::string(Constant::tempFileSep) + upperCfgFile;
                if (!dir.fileExists(filepath.c_str())){
                    LOG_ERROR("Main config file not found: %s", filepath.c_str());
                }
            }

            fstream filecfg;
            filecfg.open(filepath, ios::in);

            bool fileopened = filecfg.is_open();
            if (fileopened){
                std::string line;
                while(getline(filecfg, line)){
                    line = Constant::Trim(Constant::replaceAll(Constant::replaceAll(line, "\r", ""), "\n", ""));
                    if (line.length() > 1 && line.at(0) != '#' && line.find("=") != std::string::npos){
                        std::vector<std::string> keyvalue = Constant::splitChar(line, '=');
                        if (keyvalue.size() < 2)
                            continue;

                        std::string key = Constant::Trim(keyvalue.at(0));
                        std::string value = Constant::Trim(keyvalue.at(1));

                        if (key.compare("emulators") == 0){
                            configMain.emulators = Constant::splitChar(value, ' '); 
                        } else if (key.compare("debug") == 0){
                            configMain.debug = value.compare("yes") == 0 ? true : false ; 
                        } else if (key.compare("path_prefix") == 0){
                            configMain.path_prefix = value;
                        } else if (key.compare("resolution") == 0){
                            std::vector<std::string> res = Constant::splitChar(value, ' ');
                            configMain.resolution[0] = Constant::strToTipo<int>(res[0]);
                            configMain.resolution[1] = Constant::strToTipo<int>(res[1]);
                        } else if (key.compare("alsa_reset") == 0){
                            configMain.alsaReset = value.compare("yes") == 0 ? true : false ; 
                        } else if (key.compare("background_music") == 0){
                            configMain.background_music = Constant::strToTipo<int>(value);
                        } else if (key.compare("mp3_file") == 0){
                            configMain.mp3_file = value;
                        }
                    }
                }
            }
            filecfg.close();

            if (fileopened){
                if (configMain.debug) LOG_DEBUG("Loading emulators", "");
                for (std::size_t i=0; i < configMain.emulators.size(); i++){
                    loadEmuConfig(configMain.emulators.at(i));
                }
                if (configMain.debug) cout << endl;         
            }

			//Adding always the configuration options
			ConfigEmu salviaConfig;
			salviaConfig.generalConfig = true;
			salviaConfig.name = "Options";
			configEmus.emplace_back(salviaConfig);   
        }

        /**
         * 
         */
        void loadEmuConfig(std::string emuname){
            ConfigEmu cfgEmu;
            dirutil dir;
            std::string strFilepath = Constant::getAppDir() + std::string(Constant::tempFileSep)
                + "config" + std::string(Constant::tempFileSep) + emuname + ".cfg";
            const char *filepath = strFilepath.c_str();

            //cout << " " << emuname << endl;
			LOG_DEBUG("Emulator: %s\n", emuname.c_str());

            bool fileopened = false;
            //cout << "Checking if exists" <<endl;
            if (dir.fileExists(filepath) && !dir.isDir(filepath)){
            //if (dir.fileExists(filepath)){
                fstream fileCfg;
                //cout << "Opening file" <<endl;
                fileCfg.open(filepath, ios::in);

                //cout << "Checking if is open" <<endl;
                fileopened = fileCfg.is_open();
                if (fileopened){
                    std::string line;
                    ConfigEmu cfgEmu;

                    while(getline(fileCfg, line)){
                        //cout << "reading line" <<endl;
                        if (line.length() > 1 && line.at(0) != '#' && line.find("=") != std::string::npos){
                            //cout << "splitting line and trimming" <<endl;
                            std::vector<std::string> keyvalue = Constant::splitChar(line, '=');        
                            std::string key = keyvalue.size() > 0 ? Constant::Trim(keyvalue.at(0)) : "";
                            std::string value = keyvalue.size() > 1 ? Constant::Trim(keyvalue.at(1)) : "";

                            if (keyvalue.size() < 2)
                                continue;

                            //cout << "assigning value for: " << key <<endl;
                            if (key.compare("name") == 0){
                                cfgEmu.name = value;
                            } else if (key.compare("system") == 0){
                                cfgEmu.system = value;
                            } else if (key.compare("description") == 0){
                                cfgEmu.description = value;
                            } else if (key.compare("directory") == 0){
                                cfgEmu.directory = value;
                            } else if (key.compare("executable") == 0){
                                cfgEmu.executable = value;
                            } else if (key.compare("global_options") == 0){
                                cfgEmu.global_options = value;
                            } else if (key.compare("map_file") == 0){
                                cfgEmu.map_file = value;
                            } else if (key.compare("options_before_rom") == 0){
                                cfgEmu.options_before_rom = value.compare("yes") == 0 ? true : false;
                            } else if (key.compare("screen_shot_directory") == 0){
                                cfgEmu.screen_shot_directory = value;
                            } else if (key.compare("assets") == 0){
                                cfgEmu.assets = value;
                            } else if (key.compare("use_rom_file") == 0){
                                cfgEmu.use_rom_file = value.compare("yes") == 0 ? true : false;
                            } else if (key.compare("rom_directory") == 0){
                                cfgEmu.rom_directory = value;
                            } else if (key.compare("rom_extension") == 0){
                                cfgEmu.rom_extension = value;
                            } else if (key.compare("use_extension") == 0){
                                cfgEmu.use_extension = value.compare("yes") == 0 ? true : false;
                            } else if (key.compare("use_rom_directory") == 0){
                                cfgEmu.use_rom_directory = value.compare("yes") == 0 ? true : false;
                            }
                        }
                    }             
                    //cout << "adding emu " << configEmus.size() <<endl;   
                    configEmus.emplace_back(cfgEmu);            
                }
                //cout << "closing file..." <<endl;   
                fileCfg.close();
            }

            if (!fileopened){
                //textout_centre_ex(screen, font, msg.c_str(), screen->w / 2, screen->h / 2, textColor, -1);
                //textout_centre_ex(screen, font, "Press a key to continue", screen->w / 2, screen->h / 2 + (font->height + 3), textColor, -1);
                LOG_ERROR("There is no config file for %s. Exiting...\n", emuname.c_str());
                //readkey();
            }
        }
};